#include "ui.hpp"

#include <hooks/game_manager.hpp>
#include <data/types/admin.hpp>
#include <managers/settings.hpp>
#include <managers/role.hpp>
#include <managers/popup.hpp>
#include <util/cocos.hpp>
#include <util/format.hpp>
#include <util/misc.hpp>
#include <util/time.hpp>
#include <util/math.hpp>
#include <util/lowlevel.hpp>

using namespace geode::prelude;

namespace util::ui {
    void switchToScene(CCScene* scene) {
        CCDirector::get()->pushScene(CCTransitionFade::create(.5f, scene));
    }

    void switchToScene(CCLayer* layer) {
        switchToScene(Build<CCScene>::create().child(layer).collect());
    }

    void replaceScene(CCScene* layer) {
        CCDirector::get()->replaceScene(CCTransitionFade::create(.5f, layer));
    }

    void replaceScene(CCLayer* layer) {
        replaceScene(Build<CCScene>::create().child(layer).collect());
    }

    void prepareLayer(CCLayer* layer, bool bg, cocos2d::ccColor3B color) {
        if (bg) {
            util::ui::addBackground(layer, color);
        }

        auto menu = CCMenu::create();
        menu->setID("back-menu");
        layer->addChild(menu);

        util::ui::addBackButton(menu, util::ui::navigateBack);

        layer->setKeyboardEnabled(true);
        layer->setKeypadEnabled(true);
    }

    void addBackground(CCNode* layer, cocos2d::ccColor3B color) {
        auto windowSize = CCDirector::get()->getWinSize();

        auto bg = CCSprite::create("GJ_gradientBG.png");
        auto bgSize = bg->getTextureRect().size;

        Build<CCSprite>(bg)
            .anchorPoint({0.f, 0.f})
            .scaleX((windowSize.width + 10.f) / bgSize.width)
            .scaleY((windowSize.height + 10.f) / bgSize.height)
            .pos({-5.f, -5.f})
            .color(color)
            .zOrder(-1)
            .id("background")
            .parent(layer);
    }

    void addBackButton(CCMenu* menu, std::function<void()> callback) {
        auto windowSize = CCDirector::get()->getWinSize();
        Build<CCSprite>::createSpriteName("GJ_arrow_01_001.png")
            .intoMenuItem([callback](CCObject*) {
                callback();
            })
            .id("back-button")
            .pos(-windowSize.width / 2 + 25.0f, windowSize.height / 2 - 25.0f)
            .parent(menu);
    }

    void navigateBack() {
        auto director = CCDirector::get();
        director->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
    }

    void rescaleToMatch(cocos2d::CCNode* node, cocos2d::CCNode* target, bool stretch) {
        auto targetSize = target->getScaledContentSize();
        rescaleToMatch(node, targetSize, stretch);
    }

    void rescaleToMatch(cocos2d::CCNode *node, cocos2d::CCSize targetSize, bool stretch) {
        auto nodeSize = node->getContentSize();

        if (!stretch) {
            float scale = targetSize.width / nodeSize.width;
            node->setScale(scale);
        } else {
            node->setScaleX(targetSize.width / nodeSize.width);
            node->setScaleY(targetSize.height / nodeSize.height);
        }
    }

    void rescaleToMatch(cocos2d::CCNode* node, float targetSize, bool stretch) {
        rescaleToMatch(node, {targetSize, targetSize}, stretch);
    }

    void rescaleToMatchX(cocos2d::CCNode* node, float targetWidth) {
        node->setScaleX(targetWidth / node->getContentWidth());
    }

    float getScrollPos(BoomListView* listView) {
        auto* cl = listView->m_tableView->m_contentLayer;
        if (cl->getPositionY() > 0.f) return 99999.f;
        return cl->getScaledContentSize().height + cl->getPositionY();
    }

    void setScrollPos(BoomListView* listView, float pos) {
        if (util::math::equal(pos, 99999.f)) return;

        auto* cl = listView->m_tableView->m_contentLayer;
        float actualPos = pos - cl->getScaledContentSize().height;

        cl->setPositionY(std::min(actualPos, 0.f));
    }

    static void scrollToBottom(CCContentLayer* layer) {
        layer->setPositionY(0.f);
    }

    static void scrollToTop(CCContentLayer* layer) {
        layer->setPositionY(
            layer->getParent()->getScaledContentSize().height - layer->getScaledContentSize().height
        );
    }

    void scrollToBottom(BoomListView* listView) {
        scrollToBottom(listView->m_tableView->m_contentLayer);
    }

    void scrollToBottom(ScrollLayer* listView) {
        scrollToBottom(listView->m_contentLayer);
    }

    void scrollToTop(BoomListView* listView) {
        scrollToTop(listView->m_tableView->m_contentLayer);
    }

    void scrollToTop(ScrollLayer* listView) {
        scrollToTop(listView->m_contentLayer);
    }

    CCPoint PopupLayout::fromTop(float y) {
        return fromTop({0.f, y});
    }

    CCPoint PopupLayout::fromTop(CCSize off) {
        return this->centerTop - off;
    }

    CCPoint PopupLayout::fromBottom(float y) {
        return fromBottom({0.f, y});
    }

    CCPoint PopupLayout::fromBottom(CCSize off) {
        return this->centerBottom + off;
    }

    CCPoint PopupLayout::fromLeft(float x) {
        return fromLeft({x, 0.f});
    }

    CCPoint PopupLayout::fromLeft(CCSize off) {
        return this->centerLeft + off;
    }

    CCPoint PopupLayout::fromRight(float x) {
        return fromRight({x, 0.f});
    }

    CCPoint PopupLayout::fromRight(CCSize off) {
        return this->centerRight + CCSize{-off.width, off.height};
    }

    CCPoint PopupLayout::fromCenter(CCSize off) {
        return this->center + off;
    }

    CCPoint PopupLayout::fromCenter(float x, float y) {
        return this->fromCenter(CCSize{x, y});
    }

    CCPoint PopupLayout::fromBottomRight(CCSize off) {
        return {bottomRight.width - off.width, bottomRight.height + off.height};
    }

    CCPoint PopupLayout::fromBottomRight(float x, float y) {
        return fromBottomRight({x, y});
    }

    CCPoint PopupLayout::fromTopRight(CCSize off) {
        return this->topRight - off;
    }

    CCPoint PopupLayout::fromTopRight(float x, float y) {
        return fromTopRight({x, y});
    }

    CCPoint PopupLayout::fromBottomLeft(CCSize off) {
        return this->bottomLeft + off;
    }

    CCPoint PopupLayout::fromBottomLeft(float x, float y) {
        return fromBottomLeft({x, y});
    }

    CCPoint PopupLayout::fromTopLeft(CCSize off) {
        return {topLeft.width + off.width, topLeft.height - off.height};
    }

    CCPoint PopupLayout::fromTopLeft(float x, float y) {
        return fromTopLeft({x, y});
    }

    static PopupLayout popupLayoutWith(const CCSize& popupSize, bool useWinSize) {
        PopupLayout layout;

        layout.winSize = CCDirector::get()->getWinSize();
        layout.popupSize = popupSize;

        if (useWinSize) {
            layout.center = CCSize{layout.winSize.width / 2, layout.winSize.height / 2};
        } else {
            layout.center = CCSize{layout.popupSize.width / 2, layout.popupSize.height / 2};
        }

        layout.left = layout.center.width - popupSize.width / 2;
        layout.bottom = layout.center.height - popupSize.height / 2;
        layout.right = layout.center.width + popupSize.width / 2;
        layout.top = layout.center.height + popupSize.height / 2;

        layout.centerLeft = CCSize{layout.left, layout.center.height};
        layout.centerRight = CCSize{layout.right, layout.center.height};
        layout.centerTop = CCSize{layout.center.width, layout.top};
        layout.centerBottom = CCSize{layout.center.width, layout.bottom};

        layout.bottomLeft = CCSize{layout.left, layout.bottom};
        layout.topLeft = CCSize{layout.left, layout.top};
        layout.bottomRight = CCSize{layout.right, layout.bottom};
        layout.topRight = CCSize{layout.right, layout.top};

        return layout;
    }

    PopupLayout getNodeLayout(const CCSize& nodeSize) {
        return popupLayoutWith(nodeSize, false);
    }

    PopupLayout getNodeLayout(float width, float height) {
        return getNodeLayout(CCSize{width, height});
    }

    PopupLayout getPopupLayout(const CCSize& popupSize) {
        return popupLayoutWith(popupSize, true);
    }

    PopupLayout getPopupLayoutAnchored(const CCSize& popupSize) {
        return popupLayoutWith(popupSize, false);
    }

    CCNode* findChildByMenuSelectorRecursive(CCNode* node, uintptr_t function) {
        if (auto button = typeinfo_cast<CCMenuItem*>(node)) {
            uintptr_t btnAddr = *(uintptr_t*)&button->m_pfnSelector;
            if (function == btnAddr) {
                return node;
            }
        }

        if (!node->getChildren() || node->getChildrenCount() == 0) return nullptr;

        for (CCNode* child : CCArrayExt<CCNode*>(node->getChildren())) {
            auto result = findChildByMenuSelectorRecursive(child, function);
            if (result) {
                return result;
            }
        }

        return nullptr;
    }

    CCSprite* createBadge(const std::string& sprite) {
        // have multiple fallback sprites in case it's invalid
        CCSprite* spr1 = nullptr;

        if (!sprite.empty()) spr1 = CCSprite::createWithSpriteFrameName(util::cocos::spr(sprite).c_str());
        if (!util::cocos::isValidSprite(spr1) && !sprite.empty()) spr1 = CCSprite::createWithSpriteFrameName(sprite.c_str());
        if (!util::cocos::isValidSprite(spr1)) spr1 = CCSprite::createWithSpriteFrameName("button-secret.png"_spr);

        return spr1;
    }

    void addBadgesToMenu(const std::vector<std::string>& roleVector, CCNode* menu, int z, CCSize size) {
        if (roleVector.empty()) return;

        auto layout = menu->getLayout();
        if (layout) {
            static_cast<AxisLayout*>(layout)->setAutoScale(false);
        }

        for (const std::string& spr : roleVector) {
            auto badge = util::ui::createBadge(spr);
            if (badge) {
                util::ui::rescaleToMatch(badge, size);
                badge->setZOrder(z);
                menu->addChild(badge);
            }
        }

        menu->updateLayout();
    }

    static ComputedRole compute(const SpecialUserData& data) {
        return RoleManager::get().compute(data.roles.value());
    }

    static ComputedRole compute(const UserEntry& data) {
        return RoleManager::get().compute(data.userRoles);
    }

    CCSprite* createBadgeIfSpecial(const SpecialUserData& data) {
        if (!data.roles) return nullptr;

        return createBadge(compute(data).badgeIcon);
    }

    CCSprite* createBadgeIfSpecial(const UserEntry& data) {
        if (data.userRoles.empty()) return nullptr;

        return createBadge(compute(data).badgeIcon);
    }

    // CCMenu* createBadgeMenuIfSpecial(const SpecialUserData& data) {
    //     if (!data.roles) return nullptr;

    //     return createBadgeMenu(RoleManager::get().createRoleArray(data.roles.value()));
    // }

    // CCMenu* createBadgeMenuIfSpecial(const UserEntry& data) {
    //     if (data.userRoles.empty()) return nullptr;

    //     return createBadgeMenu(RoleManager::get().createRoleArray(data.userRoles));
    // }

    ccColor3B getNameColor(const SpecialUserData& data) {
        if (!data.roles) return ccc3(255, 255, 255);

        auto computed = compute(data);
        if (computed.nameColor) return computed.nameColor->getAnyColor();

        return ccc3(255, 255, 255);
    }

    RichColor getNameRichColor(const SpecialUserData& data) {
        if (!data.roles) return RichColor(ccc3(255, 255, 255));

        auto computed = compute(data);
        if (computed.nameColor) return computed.nameColor.value();

        return RichColor(ccc3(255, 255, 255));
    }

    void makeListGray(GJListLayer* listLayer) {
        auto top = static_cast<CCSprite*>(geode::cocos::getChildBySpriteFrameName(listLayer, "GJ_table_top_001.png"));
        auto bottom = static_cast<CCSprite*>(geode::cocos::getChildBySpriteFrameName(listLayer, "GJ_table_bottom_001.png"));

        CCSprite *side1 = nullptr, *side2 = nullptr;

        for (auto* child : CCArrayExt<CCNode*>(listLayer->getChildren())) {
            if (geode::cocos::isSpriteFrameName(child, "GJ_table_side_001.png")) {
                if (!side1) {
                    side1 = static_cast<CCSprite*>(child);
                } else if (child != side1) {
                    side2 = static_cast<CCSprite*>(child);
                    break;
                }
            }
        }

        if (top && bottom && side1 && side2) {
            auto top2 = CCSprite::createWithSpriteFrameName("list-border-top.png"_spr);
            auto bottom2 = CCSprite::createWithSpriteFrameName("list-border-bottom.png"_spr);

            auto cloneThing = [](CCSprite* from, CCSprite* to) {
                to->setPosition(from->getPosition());
                to->setAnchorPoint(from->getAnchorPoint());
                to->setScaleX(from->getScaleX());
                to->setScaleY(from->getScaleY());
                to->setID(from->getID());
                to->setZOrder(from->getZOrder());
                to->setOpacity(from->getOpacity());

                from->getParent()->addChild(to);
                from->removeFromParent();
            };

            cloneThing(bottom, bottom2);
            cloneThing(top, top2);

            for (auto* side : {side1, side2}) {
                auto id = side->getID();
                auto pos = side->getPosition();
                auto scaleY = side->getScaleY();

                side->removeFromParent();

                auto* spr = Build<CCSprite>::createSpriteName("list-border-side.png"_spr)
                    .zOrder(9)
                    .id(id)
                    .anchorPoint({0.f, 0.f})
                    .pos(pos)
                    .scaleY(scaleY)
                    .parent(listLayer)
                    .collect();

                if (side == side2) {
                    spr->setScaleX(-1.f);
                }
            }
        }

        listLayer->setColor({55, 55, 55});
    }

    void setCellColors(CCArray* cells, ccColor3B color) {
        for (auto cell : CCArrayExt<CCNode*>(cells)) {
            while (cell && !typeinfo_cast<GenericListCell*>(cell)) {
                cell = cell->getParent();
            }

            if (!cell) continue;

            static_cast<GenericListCell*>(cell)->m_backgroundLayer->setColor(color);
        }
    }

    struct RepeatingBackgroundParams : public CCObject {
    public:
        static RepeatingBackgroundParams* create(float scale, float speed, CCSize textureSize, RepeatMode repeat, CCSize visibleSize) {
            auto ret = new RepeatingBackgroundParams;
            ret->scale = scale;
            ret->speed = speed;
            ret->textureSize = textureSize;
            ret->visibleSize = visibleSize;
            ret->repeatMode = repeat;
            ret->autorelease();
            return ret;
        }

        float scale;
        float speed;
        CCSize textureSize;
        CCSize visibleSize;
        RepeatMode repeatMode;
    };

    void RepeatingBackground::update(float dt) {
        auto* params = static_cast<RepeatingBackgroundParams*>(this->getUserObject("repeat-bg-params"));
        if (!params) return;

        float moveByX = dt * params->speed * params->textureSize.width / 12.f;
        float moveByY = dt * params->speed * params->textureSize.height / 15.f;

        bool repeatX = params->repeatMode == RepeatMode::X || params->repeatMode == RepeatMode::Both;
        bool repeatY = params->repeatMode == RepeatMode::Y || params->repeatMode == RepeatMode::Both;

        float loopAfterXMin = params->visibleSize.width;
        float loopAfterX = 0.f;

        while (loopAfterX < loopAfterXMin) {
            loopAfterX += params->textureSize.width * params->scale;
        }

        this->setPositionX(this->getPositionX() - moveByX);
        if (std::fabs(this->getPositionX()) > loopAfterX) {
            // log::debug("Looping, visSize: {}, loopAfter: {}", loopAfterX);
            this->setPositionX(this->getPositionX() + params->textureSize.width * params->scale);
        }
    }

    RepeatingBackground* RepeatingBackground::create(const char* name) {
        auto tex = util::cocos::textureFromSpriteName(name);
        auto bg = CCSprite::createWithTexture(tex);

        if (util::cocos::isValidSprite(bg)) {
            // replace the vtable so we can get our update() called
            // is this UB?
            // Yes.
            return util::lowlevel::swapVtable<RepeatingBackground>(bg);
        }

        return nullptr;
    }

    CCSprite* makeRepeatingBackground(const char* texture, ccColor3B color, float speed, float scale, RepeatMode mode, const cocos2d::CCSize& _vsize) {
        auto bg = RepeatingBackground::create(texture);
        if (!bg) return nullptr;

        auto visibleSize = _vsize;
        if (visibleSize == CCSize{0.f, 0.f}) {
            visibleSize = CCDirector::get()->getWinSize();
        }

        auto winSize = CCDirector::get()->getWinSize();

        auto bgrect = bg->getTextureRect();
        auto rawTextureSize = bg->getContentSize();

        bool repeatX = mode == RepeatMode::X || mode == RepeatMode::Both;
        bool repeatY = mode == RepeatMode::Y || mode == RepeatMode::Both;

        if (repeatX) {
            float minX = visibleSize.width * 3.f / scale;
            float w = 0.f;
            while (w < minX) {
                w += rawTextureSize.width;
            }

            bgrect.size.width = w;
        }

        if (repeatY) {
            float minY = visibleSize.width * 2.f / scale;
            float h = 0.f;
            while (h < minY) {
                h += rawTextureSize.height;
            }

            bgrect.size.height = h;
        }

        ccTexParams tp = {
            GL_LINEAR, GL_LINEAR,
            static_cast<GLuint>(repeatX ? GL_REPEAT : GL_CLAMP_TO_EDGE),
            static_cast<GLuint>(repeatY ? GL_REPEAT : GL_CLAMP_TO_EDGE)
        };

        bg->getTexture()->setTexParameters(&tp);

        auto rbg = Build(bg)
            .contentSize(winSize.width, winSize.height)
            .textureRect(bgrect)
            .scale(scale)
            .zOrder(-1)
            .anchorPoint(0.f, 0.f)
            .color(color)
            .collect();

        rbg->setUserObject("repeat-bg-params", RepeatingBackgroundParams::create(scale, speed, rawTextureSize, mode, visibleSize));
        rbg->scheduleUpdate();

        return bg;
    }

    CCScale9Sprite* attachBackground(
        CCNode* node,
        const BackgroundOptions& options
    ) {
        if (!node) return nullptr;

        CCSize rawSize = node->getScaledContentSize();
        rawSize.width += options.sidePadding * 2;
        rawSize.height += options.verticalPadding * 2;

        // now, we have some annoying math to figure out.
        // at 1x scale, if `rawSize` is small enough, the parts of the ccscale9sprite will overlap with each other, which is very unwanted.
        // on top of that, the `cornerRoundness` parameter will also impact the scale of the sprite,
        // as scaling the texture down and blowing up the size makes the corners a lot more rough, and vice versa.

        // here goes nothing

        float scaleX = 1.f, scaleY = 1.f;
        float roundness = options.cornerRoundness;

        auto spr = CCScale9Sprite::create(options.texture);
        auto bgSize = spr->getContentSize();

        if (rawSize.width < bgSize.width) {
            scaleX = rawSize.width / bgSize.width;
        }

        if (rawSize.height < bgSize.height) {
            scaleY = rawSize.height / bgSize.height;
        }

        if (options.scaleMustMatch) {
            scaleX = scaleY = util::math::min(scaleX, scaleY);
        }

        return Build(spr)
            .pos(node->getScaledContentSize() / 2.f)
            .contentSize(rawSize.width / scaleX, rawSize.height / scaleY)
            .scaleX(scaleX)
            .scaleY(scaleY)
            .opacity(options.opacity)
            .zOrder(options.zOrder)
            .parent(node)
            .id(options.id)

            ;
    }

    float capPopupWidth(float in) {
        auto winSize = CCDirector::get()->getWinSize();
        return util::math::min(in, winSize.width * 0.75f);
    }

    void fixListBorders(GJCommentListLayer* list) {
        float w = list->getScaledContentWidth() * 1.045f;

        // find the borders
        CCNode* border1 = nullptr, *border2 = nullptr;

        for (auto* child : CCArrayExt<CCNode*>(list->getChildren())) {
            auto sprite = typeinfo_cast<CCSprite*>(child);
            if (!sprite) continue;

            if (geode::cocos::isSpriteFrameName(sprite, "GJ_commentTop2_001.png")) {
                border1 ? (border2 = sprite) : (border1 = sprite);
            }

            if (border2) break;
        }

        // rescale them
        if (border1) {
            rescaleToMatchX(border1, w);
        }

        if (border2) {
            rescaleToMatchX(border2, w);
        }
    }

    CCSize fixTextAreaContentSize(TextArea* textarea) {
        // TODO: this doesn't really work the best rn
        CCSize out{};

        for (auto child : CCArrayExt<CCLabelBMFont>(textarea->m_label->getChildren())) {
            out.width = std::max<float>(out.width, child->getContentWidth());
            out.height += child->getContentHeight();
        }

        textarea->setContentSize(out);
        textarea->m_label->setContentSize(out);
        textarea->m_label->setAnchorPoint({-0.5f, -0.5f});
        textarea->m_label->setPosition({0.f, 0.f});

        return out;
    }

    float getAspectRatio() {
        auto winSize = CCDirector::get()->getWinSize();

        return winSize.width / winSize.height;
    }

    void showMotd(const std::string& text) {
        std::string title, body;
        auto newline = text.find("\n");

        if (text.starts_with("#") && newline != std::string::npos) {
            title = util::format::trim(text.substr(1, newline)); // from hashtag and until newline
            body = util::format::trim(text.substr(newline + 1));
        } else {
            title = "Globed Message";
            body = text;
        }

        auto popup = MDPopup::create(title, body, "Ok");
        auto pref = PopupManager::get().manage(popup);
        pref.setPersistent();
        pref.showQueue();
    }
}