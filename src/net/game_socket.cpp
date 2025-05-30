#include "game_socket.hpp"

#include <data/bytebuffer.hpp>
#include <data/packets/match.hpp>
#include <util/debug.hpp>
#include <util/net.hpp>
#include <util/format.hpp>
#include <util/crypto.hpp>

#ifdef GEODE_IS_WINDOWS
# include <WinSock2.h>
#else
# include <sys/socket.h>
# include <poll.h>
#endif

constexpr size_t DATA_BUF_SIZE = 2 << 18;

using namespace util::data;
using namespace util::debug;
using namespace asp::time;
using PollResult = GameSocket::PollResult;
using Protocol = GameSocket::Protocol;
using ReceivedPacket = GameSocket::ReceivedPacket;

GameSocket::GameSocket() {
    dataBuffer = new byte[DATA_BUF_SIZE];
}

GameSocket::~GameSocket() {
    this->disconnect();
    delete[] dataBuffer;
}

Result<> GameSocket::connect(const NetworkAddress& address, bool isRecovering) {
#ifdef GLOBED_DEBUG
    auto r = address.resolveToString();
    std::string resolved = "<unresolved>";
    if (r) {
        resolved = r.unwrap();
    } else {
        resolved = fmt::format("<unresolved>: {}", r.unwrapErr());
    }

    log::debug("Connecting to {} (resolved to {})", address.toString(), resolved);
#endif

    GLOBED_UNWRAP(tcpSocket.connect(address))
    GLOBED_UNWRAP(udpSocket.connect(address))

    // send a magic byte telling the server whether we are recovering or not
    uint8_t byte = isRecovering ? MARKER_CONN_RECOVERY : MARKER_CONN_INITIAL;
    GLOBED_UNWRAP(tcpSocket.send(reinterpret_cast<const char*>(&byte), 1));

    return Ok();
}

void GameSocket::disconnect() {
    tcpSocket.disconnect();
    udpSocket.disconnect();
    udpBuffer.clear();
}

bool GameSocket::isConnected() {
    return tcpSocket.connected;
}

Result<std::shared_ptr<Packet>> GameSocket::recvPacketTCP() {
    ByteBuffer bb;
    bb.grow(4);

    // receive the packet length
    GLOBED_UNWRAP(tcpSocket.recvExact(reinterpret_cast<char*>(bb.data().data()), 4));

    auto packetSize = bb.readU32().unwrapOr(0); // must always be 4 bytes so cant error
    GLOBED_REQUIRE_SAFE(packetSize < DATA_BUF_SIZE, "packet is too big, rejecting")

    GLOBED_UNWRAP(tcpSocket.recvExact(reinterpret_cast<char*>(dataBuffer), packetSize));

    ByteBuffer buf(dataBuffer, packetSize);

    auto retval = this->decodePacket(buf);
    if (retval) {
        auto& pkt = retval.unwrap();
        PacketLogger::get().record(pkt->getPacketId(), pkt->getEncrypted(), false, buf.size());
    }

    return retval;
}

Result<std::optional<ReceivedPacket>> GameSocket::recvPacketUDP(bool skipMarker) {
    auto recvResult = udpSocket.receive(reinterpret_cast<char*>(dataBuffer), DATA_BUF_SIZE);

    ReceivedPacket out;
    out.fromConnected = recvResult.fromServer;

    if (recvResult.result < 0) {
        return Err(fmt::format("udp recv failed ({}): {}", recvResult.result, util::net::lastErrorString()));
    }

    ByteBuffer buf(dataBuffer, (size_t)recvResult.result);

    // if not from active server, dont't read the marker
    if (!out.fromConnected || skipMarker) {
        GLOBED_UNWRAP_INTO(this->decodePacket(buf), out.packet);

        if (out.packet) {
            PacketLogger::get().record(out.packet->getPacketId(), out.packet->getEncrypted(), false, buf.size());
        }

        return Ok(std::move(out));
    }

    // check if it is a full packet or a frame,
    auto marker = buf.readU8();
    if (marker.isErr()) {
        return Err(fmt::to_string(marker.unwrapErr()));
    }

    if (*marker == MARKER_UDP_PACKET) {
        GLOBED_UNWRAP_INTO(this->decodePacket(buf), out.packet);
    } else if (*marker == MARKER_UDP_FRAME) {
        GLOBED_UNWRAP_INTO(udpBuffer.pushFrameFromBuffer(buf), auto maybeBuf);
        if (!maybeBuf.empty()) {
            ByteBuffer toDecode(std::move(maybeBuf));
            GLOBED_UNWRAP_INTO(this->decodePacket(toDecode), out.packet);
        } else {
            return Ok(std::nullopt);
        }
    } else {
        return Err("invalid marker at the start of a udp packet");
    }

    if (out.packet) {
        PacketLogger::get().record(out.packet->getPacketId(), out.packet->getEncrypted(), false, buf.size());
    }

    return Ok(std::move(out));
}

Result<ReceivedPacket> GameSocket::recvPacket(int timeoutMs) {
    // negative value means poll indefinitely until either tcp or udp receives data
    GLOBED_UNWRAP_INTO(this->poll(timeoutMs), auto pollResult);

    if (pollResult == PollResult::None) {
        return Err("timed out");
    }

    // prioritize TCP, if the result is Tcp or Both, we care about TCP.
    if (pollResult != PollResult::Udp) {
        auto res = this->recvPacketTCP();

        if (res) {
            auto pkt = std::move(res).unwrap();
            return Ok(ReceivedPacket {
                .packet = std::move(pkt),
                .fromConnected = true
            });
        } else {
            return Err(fmt::format("recvPacketTCP failed: {}", res.unwrapErr()));
        }
    }

    // else it's a udp packet
    auto udpres = this->recvPacketUDP();

    if (udpres && udpres.unwrap().has_value()) {
        return Ok(std::move(**udpres));
    } else if (!udpres) {
        return Err(fmt::format("recvPacketUDP failed: {}", std::move(std::move(udpres).unwrapErr())));
    }

    // if it was a frame keep trying
    for (;;) {
        GLOBED_UNWRAP_INTO(udpSocket.poll(25), auto pollres);
        if (!pollres) {
            return Err("timed out");
        }

        auto udpres = this->recvPacketUDP();
        if (udpres && udpres.unwrap().has_value()) {
            return Ok(std::move(**udpres));
        } else if (!udpres) {
            return Err(fmt::format("recvPacketUDP failed: {}", std::move(std::move(udpres).unwrapErr())));
        }
    }
}

Result<ReceivedPacket> GameSocket::recvPacket() {
    return this->recvPacket(-1);
}

Result<> GameSocket::sendPacket(std::shared_ptr<Packet> packet, Protocol protocol) {
    GLOBED_REQUIRE_SAFE(this->isConnected(), "attempting to send a packet while disconnected")

    bool useTcp = false;
    switch (protocol) {
        case Protocol::Tcp: useTcp = true; break;
        case Protocol::Udp: useTcp = false; break;
        default: useTcp = packet->getUseTcp(); break;
    }

    ByteBuffer buf;
    GLOBED_UNWRAP(this->encodePacket(*packet, buf, useTcp))

    if (dumpPackets) {
        this->dumpPacket(packet->getPacketId(), buf, true);
    }

#ifdef GLOBED_DEBUG_PACKETS
    PacketLogger::get().record(packet->getPacketId(), packet->getEncrypted(), true, buf.size());
#endif

    if (useTcp) {
        GLOBED_UNWRAP(tcpSocket.sendAll(reinterpret_cast<const char*>(buf.data().data()), buf.size()));
    } else {
        GLOBED_UNWRAP(udpSocket.send(reinterpret_cast<const char*>(buf.data().data()), buf.size()));
    }

    return Ok();
}

Result<> GameSocket::sendPacketTCP(std::shared_ptr<Packet> packet) {
    return this->sendPacket(packet, Protocol::Tcp);
}

Result<> GameSocket::sendPacketUDP(std::shared_ptr<Packet> packet) {
    return this->sendPacket(packet, Protocol::Udp);
}

Result<> GameSocket::sendPacketTo(std::shared_ptr<Packet> packet, const NetworkAddress& address) {
    GLOBED_REQUIRE_SAFE(!packet->getUseTcp(), "cannot send a TCP packet to a UDP connection")

    ByteBuffer buf;
    GLOBED_UNWRAP(this->encodePacket(*packet, buf, false))

    if (dumpPackets) {
        this->dumpPacket(packet->getPacketId(), buf, true);
    }

#ifdef GLOBED_DEBUG_PACKETS
    PacketLogger::get().record(packet->getPacketId(), packet->getEncrypted(), true, buf.size());
#endif

    GLOBED_UNWRAP_INTO(udpSocket.sendTo(reinterpret_cast<const char*>(buf.data().data()), buf.size(), address), auto res)

    GLOBED_REQUIRE_SAFE(
        res == buf.size(),
        "failed to send the entire buffer"
    )

    return Ok();
}

Result<> GameSocket::sendRecoveryData(int accountId, uint32_t secretKey) {
    ByteBuffer bb;
    bb.writeI32(accountId);
    bb.writeU32(secretKey);

    return tcpSocket.sendAll(reinterpret_cast<const char*>(bb.data().data()), bb.size());
}

void GameSocket::cleanupBox() {
    cryptoBox = std::unique_ptr<CryptoBox>(nullptr);
}

void GameSocket::createBox() {
    cryptoBox = std::make_unique<CryptoBox>();
}

void GameSocket::togglePacketLogging(bool state) {
    dumpPackets = state;
}

Result<PollResult> GameSocket::poll(int timeoutMs) {
    if (!tcpSocket.connected) {
        GLOBED_UNWRAP_INTO(udpSocket.poll(timeoutMs), auto res);
        return Ok(res ? PollResult::Udp : PollResult::None);
    }

    GLOBED_SOCKET_POLLFD fds[2];

    fds[0].fd = tcpSocket.socket_;
    fds[0].events = POLLIN;
    fds[1].fd = udpSocket.socket_;
    fds[1].events = POLLIN;

    int result = GLOBED_SOCKET_POLL(fds, 2, timeoutMs);

    if (result == -1) {
        return Err(util::net::lastErrorString());
    }

    bool tcp = fds[0].revents & POLLIN;
    bool udp = fds[1].revents & POLLIN;

    if (tcp && udp) {
        return Ok(PollResult::Both);
    } else if (tcp) {
        return Ok(PollResult::Tcp);
    } else if (udp) {
        return Ok(PollResult::Udp);
    } else {
        return Ok(PollResult::None);
    }
}

Result<bool> GameSocket::poll(Protocol proto, int timeoutMs) {
    GLOBED_REQUIRE_SAFE(proto != Protocol::Unspecified, "invalid protocol");

    GLOBED_SOCKET_POLLFD fd;
    fd.events = POLLIN;

    if (proto == Protocol::Tcp) {
        if (!tcpSocket.connected) {
            return Err("TCP socket is not connected");
        }

        fd.fd = tcpSocket.socket_;
    } else {
        fd.fd = udpSocket.socket_;
    }

    int result = GLOBED_SOCKET_POLL(&fd, 1, timeoutMs);
    if (result == -1) {
        return Err(util::net::lastErrorString());
    }

    return Ok((bool) (fd.revents & POLLIN));

}

Result<> GameSocket::encodePacket(Packet& packet, ByteBuffer& buffer, bool tcp) {
    PacketHeader header = {
        .id = packet.getPacketId(),
        .encrypted = packet.getEncrypted(),
    };

    // reserve space for packet length when using TCP
    size_t startPos = buffer.getPosition();

    if (tcp) {
        buffer.writeU32(0);
    }

    buffer.writeValue<PacketHeader>(header);
    packet.encode(buffer);

    if (packet.getEncrypted()) {
        GLOBED_REQUIRE_SAFE(cryptoBox.get() != nullptr, "attempted to encrypt a packet when no cryptobox is initialized")

        // grow the vector by CryptoBox::PREFIX_LEN extra bytes to do in-place encryption
        buffer.grow(CryptoBox::PREFIX_LEN);
        uint32_t headerSize = PacketHeader::SIZE;
        if (tcp) {
            headerSize += sizeof(uint32_t);
        }

        auto rawSize = buffer.size() - headerSize - startPos - CryptoBox::PREFIX_LEN;
        cryptoBox->encryptInPlace(buffer.data().data() + startPos + headerSize, rawSize);
    }

    // write length
    if (tcp) {
        size_t lastPos = buffer.getPosition();
        size_t packetSize = buffer.size() - sizeof(uint32_t) - startPos;

        buffer.setPosition(startPos);
        buffer.writeU32(packetSize);
        buffer.setPosition(lastPos);
    }

    return Ok();
}

Result<std::shared_ptr<Packet>> GameSocket::decodePacket(ByteBuffer& buffer) {
    // read header
    auto header = buffer.readValue<PacketHeader>().unwrap(); // we know that the header must be present by now.

    // packet size without the header
    size_t messageStart = buffer.getPosition();
    size_t messageLength = buffer.size() - messageStart;

    auto packet = matchPacket(header.id);

    GLOBED_REQUIRE_SAFE(packet.get() != nullptr, std::string("invalid server-side packet: ") + std::to_string(header.id))

    if (packet->getEncrypted() && !header.encrypted) {
        GLOBED_REQUIRE_SAFE(false, fmt::format("server sent a cleartext packet when expected an encrypted one ({})", header.id))
    }

    if (header.encrypted) {
        GLOBED_REQUIRE_SAFE(cryptoBox.get() != nullptr, "attempted to decrypt a packet when no cryptobox is initialized")
        bytevector& bufvec = buffer.data();

        GLOBED_UNWRAP_INTO(cryptoBox->decryptInPlace(bufvec.data() + messageStart, messageLength), messageLength);
        buffer.resize(messageStart + messageLength);
    }

    if (dumpPackets) {
        this->dumpPacket(header.id, buffer, false);
    }

    auto result = packet->decode(buffer);
    if (result.isErr()) {
        return Err(fmt::format("Decoding packet ID {} failed: {}", header.id, ByteBuffer::strerror(result.unwrapErr())));
    }

    return Ok(std::move(packet));
}

void GameSocket::dumpPacket(packetid_t id, ByteBuffer& buffer, bool sending) {
    log::debug("{} packet {}", sending ? "Sending" : "Receiving", id);

    auto folder = Mod::get()->getSaveDir() / "packets";
    (void) geode::utils::file::createDirectoryAll(folder);
    util::misc::callOnce("networkmanager-log-to-file", [&] {
        log::debug("Packet log folder: {}", folder);
    });

    auto datetime = util::format::formatDateTime(SystemTime::now());
    auto filepath = folder / fmt::format("{}-{}.bin", id, datetime);

    std::ofstream fs(filepath, std::ios::binary);

    const auto& vec = buffer.data();
    fs.write(reinterpret_cast<const char*>(vec.data()), vec.size());
}