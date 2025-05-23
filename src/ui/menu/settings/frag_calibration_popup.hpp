#pragma once
#include <defs/all.hpp>

#include <data/packets/server/connection.hpp>

#include <asp/time/SystemTime.hpp>

class FragmentationCalibartionPopup : public geode::Popup<> {
public:
    static constexpr float POPUP_WIDTH = 180.f;
    static constexpr float POPUP_HEIGHT = 80.f;
    static FragmentationCalibartionPopup* create();
    // mac has a packet limit of <10kb idk why, on the 10kb test it says "message too long"
#ifdef GEODE_IS_MACOS
    static constexpr size_t TEST_PACKET_SIZES[] = {
        1000, 1300, 1400, 1450, 2000, 3000, 4000, 5000, 6000, 7000, 0
    };
#else
    static constexpr size_t TEST_PACKET_SIZES[] = {
        1000, 1300, 1400, 1450, 2000, 4000, 7000, 10000, 13000, 18000, 25000, 30000, 40000, 50000, 60000, 65000, 0
    };
#endif

private:
    cocos2d::CCLabelBMFont* statusLabel = nullptr;
    size_t currentSize = 0;
    size_t currentAttempt = 0;
    size_t currentSizeIdx = 0;
    int uid;
    int failedAttempts = 0;
    asp::time::SystemTime lastPacket;

    bool setup() override;
    void onClose(cocos2d::CCObject*) override;
    void checkForUpdates(float);

    void nextStep();
    void closeDelayed();
};
