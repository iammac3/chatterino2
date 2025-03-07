#include "widgets/Window.hpp"

#include "Application.hpp"
#include "common/Credentials.hpp"
#include "common/Modes.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/Updates.hpp"
#include "singletons/WindowManager.hpp"
#include "util/InitUpdateButton.hpp"
#include "widgets/AccountSwitchPopup.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/dialogs/UpdateDialog.hpp"
#include "widgets/dialogs/WelcomeDialog.hpp"
#include "widgets/dialogs/switcher/QuickSwitcherPopup.hpp"
#include "widgets/helper/EffectLabel.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/helper/TitlebarButton.hpp"
#include "widgets/splits/ClosedSplits.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"

#ifndef NDEBUG
#    include <rapidjson/document.h>
#    include "providers/twitch/PubSubManager.hpp"
#    include "providers/twitch/PubSubMessages.hpp"
#    include "util/SampleCheerMessages.hpp"
#    include "util/SampleLinks.hpp"
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QHeaderView>
#include <QMenuBar>
#include <QPalette>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace chatterino {

Window::Window(WindowType type)
    : BaseWindow(BaseWindow::EnableCustomFrame)
    , type_(type)
    , notebook_(new SplitNotebook(this))
{
    this->addCustomTitlebarButtons();
    this->addShortcuts();
    this->addLayout();

#ifdef Q_OS_MACOS
    this->addMenuBar();
#endif

    this->bSignals_.emplace_back(
        getApp()->accounts->twitch.currentUserChanged.connect([this] {
            this->onAccountSelected();
        }));
    this->onAccountSelected();

    if (type == WindowType::Main)
    {
        this->resize(int(600 * this->scale()), int(500 * this->scale()));
    }
    else
    {
        this->resize(int(300 * this->scale()), int(500 * this->scale()));
    }

    this->signalHolder_.managedConnect(getApp()->hotkeys->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
    if (type == WindowType::Main || type == WindowType::Popup)
    {
        getSettings()->tabDirection.connect([this](int val) {
            this->notebook_->setTabDirection(NotebookTabDirection(val));
        });
    }
}

WindowType Window::getType()
{
    return this->type_;
}

SplitNotebook &Window::getNotebook()
{
    return *this->notebook_;
}

bool Window::event(QEvent *event)
{
    switch (event->type())
    {
        case QEvent::WindowActivate:
            break;

        case QEvent::WindowDeactivate: {
            auto page = this->notebook_->getOrAddSelectedPage();

            if (page != nullptr)
            {
                std::vector<Split *> splits = page->getSplits();

                for (Split *split : splits)
                {
                    split->updateLastReadMessage();
                }
            }

            if (SplitContainer *container =
                    dynamic_cast<SplitContainer *>(page))
            {
                container->hideResizeHandles();
            }
        }
        break;

        default:;
    }

    return BaseWindow::event(event);
}

void Window::closeEvent(QCloseEvent *)
{
    if (this->type_ == WindowType::Main)
    {
        auto app = getApp();
        app->windows->save();
        app->windows->closeAll();
    }

    this->closed.invoke();

    if (this->type_ == WindowType::Main)
    {
        QApplication::exit();
    }
}

void Window::addLayout()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(this->notebook_);
    this->getLayoutContainer()->setLayout(layout);

    // set margin
    layout->setMargin(0);

    this->notebook_->setAllowUserTabManagement(true);
    this->notebook_->setShowAddButton(true);
}

void Window::addCustomTitlebarButtons()
{
    if (!this->hasCustomWindowFrame())
        return;
    if (this->type_ != WindowType::Main)
        return;

    // settings
    this->addTitleBarButton(TitleBarButtonStyle::Settings, [this] {
        getApp()->windows->showSettingsDialog(this);
    });

    // updates
    auto update = this->addTitleBarButton(TitleBarButtonStyle::None, [] {});

    initUpdateButton(*update, this->signalHolder_);

    // account
    this->userLabel_ = this->addTitleBarLabel([this] {
        getApp()->windows->showAccountSelectPopup(this->userLabel_->mapToGlobal(
            this->userLabel_->rect().bottomLeft()));
    });
    this->userLabel_->setMinimumWidth(20 * scale());
}

void Window::addDebugStuff(HotkeyController::HotkeyMap &actions)
{
#ifndef NDEBUG
    std::vector<QString> cheerMessages, subMessages, miscMessages, linkMessages,
        emoteTestMessages;

    cheerMessages = getSampleCheerMessage();
    auto validLinks = getValidLinks();
    auto invalidLinks = getInvalidLinks();
    // clang-format off

    subMessages.emplace_back(R"(@badges=staff/1,broadcaster/1,turbo/1;color=#008000;display-name=ronni;emotes=;id=db25007f-7a18-43eb-9379-80131e44d633;login=ronni;mod=0;msg-id=resub;msg-param-months=6;msg-param-sub-plan=Prime;msg-param-sub-plan-name=Prime;room-id=1337;subscriber=1;system-msg=ronni\shas\ssubscribed\sfor\s6\smonths!;tmi-sent-ts=1507246572675;turbo=1;user-id=1337;user-type=staff :tmi.twitch.tv USERNOTICE #pajlada :Great stream -- keep it up!)");
    subMessages.emplace_back(R"(@badges=staff/1,premium/1;color=#0000FF;display-name=TWW2;emotes=;id=e9176cd8-5e22-4684-ad40-ce53c2561c5e;login=tww2;mod=0;msg-id=subgift;msg-param-months=1;msg-param-recipient-display-name=Mr_Woodchuck;msg-param-recipient-id=89614178;msg-param-recipient-name=mr_woodchuck;msg-param-sub-plan-name=House\sof\sNyoro~n;msg-param-sub-plan=1000;room-id=19571752;subscriber=0;system-msg=TWW2\sgifted\sa\sTier\s1\ssub\sto\sMr_Woodchuck!;tmi-sent-ts=1521159445153;turbo=0;user-id=13405587;user-type=staff :tmi.twitch.tv USERNOTICE #pajlada)");

    // hyperbolicxd gifted a sub to quote_if_nam
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=#00FF7F;display-name=hyperbolicxd;emotes=;id=b20ef4fe-cba8-41d0-a371-6327651dc9cc;login=hyperbolicxd;mod=0;msg-id=subgift;msg-param-months=1;msg-param-recipient-display-name=quote_if_nam;msg-param-recipient-id=217259245;msg-param-recipient-user-name=quote_if_nam;msg-param-sender-count=1;msg-param-sub-plan-name=Channel\sSubscription\s(nymn_hs);msg-param-sub-plan=1000;room-id=62300805;subscriber=1;system-msg=hyperbolicxd\sgifted\sa\sTier\s1\ssub\sto\squote_if_nam!\sThis\sis\stheir\sfirst\sGift\sSub\sin\sthe\schannel!;tmi-sent-ts=1528190938558;turbo=0;user-id=111534250;user-type= :tmi.twitch.tv USERNOTICE #pajlada)");

    // first time sub
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=#0000FF;display-name=byebyeheart;emotes=;id=fe390424-ab89-4c33-bb5a-53c6e5214b9f;login=byebyeheart;mod=0;msg-id=sub;msg-param-months=0;msg-param-sub-plan-name=Dakotaz;msg-param-sub-plan=Prime;room-id=39298218;subscriber=0;system-msg=byebyeheart\sjust\ssubscribed\swith\sTwitch\sPrime!;tmi-sent-ts=1528190963670;turbo=0;user-id=131956000;user-type= :tmi.twitch.tv USERNOTICE #pajlada)");

    // first time sub
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=;display-name=vJoeyzz;emotes=;id=b2476df5-fffe-4338-837b-380c5dd90051;login=vjoeyzz;mod=0;msg-id=sub;msg-param-months=0;msg-param-sub-plan-name=Dakotaz;msg-param-sub-plan=Prime;room-id=39298218;subscriber=0;system-msg=vJoeyzz\sjust\ssubscribed\swith\sTwitch\sPrime!;tmi-sent-ts=1528190995089;turbo=0;user-id=78945903;user-type= :tmi.twitch.tv USERNOTICE #pajlada)");

    // first time sub
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=;display-name=Lennydog3;emotes=;id=44feb1eb-df60-45f6-904b-7bf0d5375a41;login=lennydog3;mod=0;msg-id=sub;msg-param-months=0;msg-param-sub-plan-name=Dakotaz;msg-param-sub-plan=Prime;room-id=39298218;subscriber=0;system-msg=Lennydog3\sjust\ssubscribed\swith\sTwitch\sPrime!;tmi-sent-ts=1528191098733;turbo=0;user-id=175759335;user-type= :tmi.twitch.tv USERNOTICE #pajlada)");

    // resub with message
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=#1E90FF;display-name=OscarLord;emotes=;id=376529fd-31a8-4da9-9c0d-92a9470da2cd;login=oscarlord;mod=0;msg-id=resub;msg-param-months=2;msg-param-sub-plan-name=Dakotaz;msg-param-sub-plan=1000;room-id=39298218;subscriber=1;system-msg=OscarLord\sjust\ssubscribed\swith\sa\sTier\s1\ssub.\sOscarLord\ssubscribed\sfor\s2\smonths\sin\sa\srow!;tmi-sent-ts=1528191154801;turbo=0;user-id=162607810;user-type= :tmi.twitch.tv USERNOTICE #pajlada :Hey dk love to watch your streams keep up the good work)");

    // resub with message
    subMessages.emplace_back(R"(@badges=subscriber/0,premium/1;color=;display-name=samewl;emotes=9:22-23;id=599fda87-ca1e-41f2-9af7-6a28208daf1c;login=samewl;mod=0;msg-id=resub;msg-param-months=5;msg-param-sub-plan-name=Channel\sSubscription\s(forsenlol);msg-param-sub-plan=Prime;room-id=22484632;subscriber=1;system-msg=samewl\sjust\ssubscribed\swith\sTwitch\sPrime.\ssamewl\ssubscribed\sfor\s5\smonths\sin\sa\srow!;tmi-sent-ts=1528191317948;turbo=0;user-id=70273207;user-type= :tmi.twitch.tv USERNOTICE #pajlada :lot of love sebastian <3)");

    // resub without message
    subMessages.emplace_back(R"(@badges=subscriber/12;color=#CC00C2;display-name=cspice;emotes=;id=6fc4c3e0-ca61-454a-84b8-5669dee69fc9;login=cspice;mod=0;msg-id=resub;msg-param-months=12;msg-param-sub-plan-name=Channel\sSubscription\s(forsenlol):\s$9.99\sSub;msg-param-sub-plan=2000;room-id=22484632;subscriber=1;system-msg=cspice\sjust\ssubscribed\swith\sa\sTier\s2\ssub.\scspice\ssubscribed\sfor\s12\smonths\sin\sa\srow!;tmi-sent-ts=1528192510808;turbo=0;user-id=47894662;user-type= :tmi.twitch.tv USERNOTICE #pajlada)");

    // display name renders strangely
    miscMessages.emplace_back(R"(@badges=;color=#00AD2B;display-name=Iamme420\s;emotes=;id=d47a1e4b-a3c6-4b9e-9bf1-51b8f3dbc76e;mod=0;room-id=11148817;subscriber=0;tmi-sent-ts=1529670347537;turbo=0;user-id=56422869;user-type= :iamme420!iamme420@iamme420.tmi.twitch.tv PRIVMSG #pajlada :offline chat gachiBASS)");
    miscMessages.emplace_back(R"(@badge-info=founder/47;badges=moderator/1,founder/0,premium/1;color=#00FF80;display-name=gempir;emotes=;flags=;id=d4514490-202e-43cb-b429-ef01a9d9c2fe;mod=1;room-id=11148817;subscriber=0;tmi-sent-ts=1575198233854;turbo=0;user-id=77829817;user-type=mod :gempir!gempir@gempir.tmi.twitch.tv PRIVMSG #pajlada :offline chat gachiBASS)");

    // "first time chat" message
    miscMessages.emplace_back(R"(@badge-info=;badges=glhf-pledge/1;client-nonce=5d2627b0cbe56fa05faf5420def4807d;color=#1E90FF;display-name=oldcoeur;emote-only=1;emotes=84608:0-7;first-msg=1;flags=;id=7412fea4-8683-4cc9-a506-4228127a5c2d;mod=0;room-id=11148817;subscriber=0;tmi-sent-ts=1623429859222;turbo=0;user-id=139147886;user-type= :oldcoeur!oldcoeur@oldcoeur.tmi.twitch.tv PRIVMSG #pajlada :cmonBruh)");

    // Message with founder badge
    miscMessages.emplace_back(R"(@badge-info=founder/72;badges=founder/0,bits/5000;color=#FF0000;display-name=TranRed;emotes=;first-msg=0;flags=;id=7482163f-493d-41d9-b36f-fba50e0701b7;mod=0;room-id=11148817;subscriber=0;tmi-sent-ts=1641123773885;turbo=0;user-id=57019243;user-type= :tranred!tranred@tranred.tmi.twitch.tv PRIVMSG #pajlada :GFMP pajaE)");

    // mod announcement
    miscMessages.emplace_back(R"(@badge-info=subscriber/47;badges=broadcaster/1,subscriber/3012,twitchconAmsterdam2020/1;color=#FF0000;display-name=Supinic;emotes=;flags=;id=8c26e1ab-b50c-4d9d-bc11-3fd57a941d90;login=supinic;mod=0;msg-id=announcement;msg-param-color=PRIMARY;room-id=31400525;subscriber=1;system-msg=;tmi-sent-ts=1648762219962;user-id=31400525;user-type= :tmi.twitch.tv USERNOTICE #supinic :mm test lol)");

    // various link tests
    linkMessages.emplace_back(R"(@badge-info=subscriber/48;badges=broadcaster/1,subscriber/36,partner/1;color=#CC44FF;display-name=pajlada;emotes=;flags=;id=3c23cf3c-0864-4699-a76b-089350141147;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1577628844607;turbo=0;user-id=11148817;user-type= :pajlada!pajlada@pajlada.tmi.twitch.tv PRIVMSG #pajlada : Links that should pass: )" + getValidLinks().join(' '));
    linkMessages.emplace_back(R"(@badge-info=subscriber/48;badges=broadcaster/1,subscriber/36,partner/1;color=#CC44FF;display-name=pajlada;emotes=;flags=;id=3c23cf3c-0864-4699-a76b-089350141147;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1577628844607;turbo=0;user-id=11148817;user-type= :pajlada!pajlada@pajlada.tmi.twitch.tv PRIVMSG #pajlada : Links that should NOT pass: )" + getInvalidLinks().join(' '));
    linkMessages.emplace_back(R"(@badge-info=subscriber/48;badges=broadcaster/1,subscriber/36,partner/1;color=#CC44FF;display-name=pajlada;emotes=;flags=;id=3c23cf3c-0864-4699-a76b-089350141147;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1577628844607;turbo=0;user-id=11148817;user-type= :pajlada!pajlada@pajlada.tmi.twitch.tv PRIVMSG #pajlada : Links that should technically pass but we choose not to parse them: )" + getValidButIgnoredLinks().join(' '));

    // channel point reward test
    const char *channelRewardMessage = "{ \"type\": \"MESSAGE\", \"data\": { \"topic\": \"community-points-channel-v1.11148817\", \"message\": { \"type\": \"reward-redeemed\", \"data\": { \"timestamp\": \"2020-07-13T20:19:31.430785354Z\", \"redemption\": { \"id\": \"b9628798-1b4e-4122-b2a6-031658df6755\", \"user\": { \"id\": \"91800084\", \"login\": \"cranken1337\", \"display_name\": \"cranken1337\" }, \"channel_id\": \"11148817\", \"redeemed_at\": \"2020-07-13T20:19:31.345237005Z\", \"reward\": { \"id\": \"313969fe-cc9f-4a0a-83c6-172acbd96957\", \"channel_id\": \"11148817\", \"title\": \"annoying reward pogchamp\", \"prompt\": \"\", \"cost\": 3000, \"is_user_input_required\": true, \"is_sub_only\": false, \"image\": null, \"default_image\": { \"url_1x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-1.png\", \"url_2x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-2.png\", \"url_4x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-4.png\" }, \"background_color\": \"#52ACEC\", \"is_enabled\": true, \"is_paused\": false, \"is_in_stock\": true, \"max_per_stream\": { \"is_enabled\": false, \"max_per_stream\": 0 }, \"should_redemptions_skip_request_queue\": false, \"template_id\": null, \"updated_for_indicator_at\": \"2020-01-20T04:33:33.624956679Z\" }, \"user_input\": \"wow, amazing reward\", \"status\": \"UNFULFILLED\", \"cursor\": \"Yjk2Mjg3OTgtMWI0ZS00MTIyLWIyYTYtMDMxNjU4ZGY2NzU1X18yMDIwLTA3LTEzVDIwOjE5OjMxLjM0NTIzNzAwNVo=\" } } } } }";
    const char *channelRewardMessage2 = "{ \"type\": \"MESSAGE\", \"data\": { \"topic\": \"community-points-channel-v1.11148817\", \"message\": { \"type\": \"reward-redeemed\", \"data\": { \"timestamp\": \"2020-07-13T20:19:31.430785354Z\", \"redemption\": { \"id\": \"b9628798-1b4e-4122-b2a6-031658df6755\", \"user\": { \"id\": \"91800084\", \"login\": \"cranken1337\", \"display_name\": \"cranken1337\" }, \"channel_id\": \"11148817\", \"redeemed_at\": \"2020-07-13T20:19:31.345237005Z\", \"reward\": { \"id\": \"313969fe-cc9f-4a0a-83c6-172acbd96957\", \"channel_id\": \"11148817\", \"title\": \"annoying reward pogchamp\", \"prompt\": \"\", \"cost\": 3000, \"is_user_input_required\": false, \"is_sub_only\": false, \"image\": null, \"default_image\": { \"url_1x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-1.png\", \"url_2x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-2.png\", \"url_4x\": \"https://static-cdn.jtvnw.net/custom-reward-images/default-4.png\" }, \"background_color\": \"#52ACEC\", \"is_enabled\": true, \"is_paused\": false, \"is_in_stock\": true, \"max_per_stream\": { \"is_enabled\": false, \"max_per_stream\": 0 }, \"should_redemptions_skip_request_queue\": false, \"template_id\": null, \"updated_for_indicator_at\": \"2020-01-20T04:33:33.624956679Z\" }, \"status\": \"UNFULFILLED\", \"cursor\": \"Yjk2Mjg3OTgtMWI0ZS00MTIyLWIyYTYtMDMxNjU4ZGY2NzU1X18yMDIwLTA3LTEzVDIwOjE5OjMxLjM0NTIzNzAwNVo=\" } } } } }";
    const char *channelRewardIRCMessage(R"(@badge-info=subscriber/43;badges=subscriber/42;color=#1E90FF;custom-reward-id=313969fe-cc9f-4a0a-83c6-172acbd96957;display-name=Cranken1337;emotes=;flags=;id=3cee3f27-a1d0-44d1-a606-722cebdad08b;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1594756484132;turbo=0;user-id=91800084;user-type= :cranken1337!cranken1337@cranken1337.tmi.twitch.tv PRIVMSG #pajlada :wow, amazing reward)");

    emoteTestMessages.emplace_back(R"(@badge-info=subscriber/3;badges=subscriber/3;color=#0000FF;display-name=Linkoping;emotes=25:40-44;flags=17-26:S.6;id=744f9c58-b180-4f46-bd9e-b515b5ef75c1;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1566335866017;turbo=0;user-id=91673457;user-type= :linkoping!linkoping@linkoping.tmi.twitch.tv PRIVMSG #pajlada :Då kan du begära skadestånd och förtal Kappa)");
    emoteTestMessages.emplace_back(R"(@badge-info=subscriber/1;badges=subscriber/0;color=;display-name=jhoelsc;emotes=301683486:46-58,60-72,74-86/301683544:88-100;flags=0-4:S.6;id=1f1afcdd-d94c-4699-b35f-d214deb1e11a;mod=0;room-id=11148817;subscriber=1;tmi-sent-ts=1588640587462;turbo=0;user-id=505763008;user-type= :jhoelsc!jhoelsc@jhoelsc.tmi.twitch.tv PRIVMSG #pajlada :pensé que no habría directo que bueno que si staryuukiLove staryuukiLove staryuukiLove staryuukiBits)");
    emoteTestMessages.emplace_back(R"(@badge-info=subscriber/34;badges=moderator/1,subscriber/24;color=#FF0000;display-name=테스트계정420;emotes=41:6-13,16-23;flags=;id=97c28382-e8d2-45a0-bb5d-2305fc4ef139;mod=1;room-id=11148817;subscriber=1;tmi-sent-ts=1590922036771;turbo=0;user-id=117166826;user-type=mod :testaccount_420!testaccount_420@testaccount_420.tmi.twitch.tv PRIVMSG #pajlada :-tags Kreygasm, Kreygasm)");
    emoteTestMessages.emplace_back(R"(@badge-info=subscriber/34;badges=moderator/1,subscriber/24;color=#FF0000;display-name=테스트계정420;emotes=25:24-28/41:6-13,15-22;flags=;id=5a36536b-a952-43f7-9c41-88c829371b7a;mod=1;room-id=11148817;subscriber=1;tmi-sent-ts=1590922039721;turbo=0;user-id=117166826;user-type=mod :testaccount_420!testaccount_420@testaccount_420.tmi.twitch.tv PRIVMSG #pajlada :-tags Kreygasm,Kreygasm Kappa (no space then space))");
    emoteTestMessages.emplace_back(R"(@badge-info=subscriber/34;badges=moderator/1,subscriber/24;color=#FF0000;display-name=테스트계정420;emotes=25:6-10/1902:12-16/88:18-25;flags=;id=aed9e67e-f8cd-493e-aa6b-da054edd7292;mod=1;room-id=11148817;subscriber=1;tmi-sent-ts=1590922042881;turbo=0;user-id=117166826;user-type=mod :testaccount_420!testaccount_420@testaccount_420.tmi.twitch.tv PRIVMSG #pajlada :-tags Kappa.Keepo.PogChamp.extratext (3 emotes with extra text))");
	emoteTestMessages.emplace_back(R"(@badge-info=;badges=moderator/1,partner/1;color=#5B99FF;display-name=StreamElements;emotes=86:30-39/822112:73-79;flags=22-27:S.5;id=03c3eec9-afd1-4858-a2e0-fccbf6ad8d1a;mod=1;room-id=11148817;subscriber=0;tmi-sent-ts=1588638345928;turbo=0;user-id=100135110;user-type=mod :streamelements!streamelements@streamelements.tmi.twitch.tv PRIVMSG #pajlada :╔ACTION A LOJA AINDA NÃO ESTÁ PRONTA BibleThump , AGUARDE... NOVIDADES EM BREVE FortOne╔)");
	emoteTestMessages.emplace_back(R"(@badge-info=subscriber/20;badges=moderator/1,subscriber/12;color=#19E6E6;display-name=randers;emotes=25:39-43;flags=;id=3ea97f01-abb2-4acf-bdb8-f52e79cd0324;mod=1;room-id=11148817;subscriber=1;tmi-sent-ts=1588837097115;turbo=0;user-id=40286300;user-type=mod :randers!randers@randers.tmi.twitch.tv PRIVMSG #pajlada :Då kan du begära skadestånd och förtal Kappa)");
	emoteTestMessages.emplace_back(R"(@badge-info=subscriber/34;badges=moderator/1,subscriber/24;color=#FF0000;display-name=테스트계정420;emotes=41:6-13,15-22;flags=;id=a3196c7e-be4c-4b49-9c5a-8b8302b50c2a;mod=1;room-id=11148817;subscriber=1;tmi-sent-ts=1590922213730;turbo=0;user-id=117166826;user-type=mod :testaccount_420!testaccount_420@testaccount_420.tmi.twitch.tv PRIVMSG #pajlada :-tags Kreygasm,Kreygasm (no space))");
    // clang-format on

    actions.emplace("addMiscMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = miscMessages;
        static int index = 0;
        auto app = getApp();
        const auto &msg = messages[index++ % messages.size()];
        app->twitch->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addCheerMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = cheerMessages;
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->twitch->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addLinkMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = linkMessages;
        static int index = 0;
        auto app = getApp();
        const auto &msg = messages[index++ % messages.size()];
        app->twitch->addFakeMessage(msg);
        return "";
    });

    actions.emplace("addRewardMessage", [=](std::vector<QString>) -> QString {
        rapidjson::Document doc;
        auto app = getApp();
        static bool alt = true;
        if (alt)
        {
            auto oMessage = parsePubSubBaseMessage(channelRewardMessage);
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();

            app->twitch->addFakeMessage(channelRewardIRCMessage);
            app->twitch->pubsub->signals_.pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        else
        {
            auto oMessage = parsePubSubBaseMessage(channelRewardMessage2);
            auto oInnerMessage =
                oMessage->toInner<PubSubMessageMessage>()
                    ->toInner<PubSubCommunityPointsChannelV1Message>();
            app->twitch->pubsub->signals_.pointReward.redeemed.invoke(
                oInnerMessage->data.value("redemption").toObject());
            alt = !alt;
        }
        return "";
    });

    actions.emplace("addEmoteMessage", [=](std::vector<QString>) -> QString {
        const auto &messages = emoteTestMessages;
        static int index = 0;
        const auto &msg = messages[index++ % messages.size()];
        getApp()->twitch->addFakeMessage(msg);
        return "";
    });
#endif
}

void Window::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"openSettings",  // Open settings
         [this](std::vector<QString>) -> QString {
             SettingsDialog::showDialog(this);
             return "";
         }},
        {"newSplit",  // Create a new split
         [this](std::vector<QString>) -> QString {
             this->notebook_->getOrAddSelectedPage()->appendNewSplit(true);
             return "";
         }},
        {"openTab",  // CTRL + 1-8 to open corresponding tab.
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
                 return "openTab shortcut called without arguments. "
                        "Takes only "
                        "one argument: tab specifier";
             }
             auto target = arguments.at(0);
             if (target == "last")
             {
                 this->notebook_->selectLastTab();
             }
             else if (target == "next")
             {
                 this->notebook_->selectNextTab();
             }
             else if (target == "previous")
             {
                 this->notebook_->selectPreviousTab();
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (ok)
                 {
                     this->notebook_->selectIndex(result);
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for openTab shortcut";
                     return QString("Invalid argument for openTab "
                                    "shortcut: \"%1\". Use \"last\", "
                                    "\"next\", \"previous\" or an integer.")
                         .arg(target);
                 }
             }
             return "";
         }},
        {"popup",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 return "popup action called without arguments. Takes only "
                        "one: \"split\" or \"window\".";
             }
             if (arguments.at(0) == "split")
             {
                 if (auto page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     if (auto split = page->getSelectedSplit())
                     {
                         split->popup();
                     }
                 }
             }
             else if (arguments.at(0) == "window")
             {
                 if (auto page = dynamic_cast<SplitContainer *>(
                         this->notebook_->getSelectedPage()))
                 {
                     page->popup();
                 }
             }
             else
             {
                 return "Invalid popup target. Use \"split\" or \"window\".";
             }
             return "";
         }},
        {"zoom",
         [](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
                 return "zoom shortcut called without arguments. Takes "
                        "only "
                        "one argument: \"in\", \"out\", or \"reset\"";
             }
             auto change = 0.0f;
             auto direction = arguments.at(0);
             if (direction == "reset")
             {
                 getSettings()->uiScale.setValue(1);
                 return "";
             }

             if (direction == "in")
             {
                 change = 0.1f;
             }
             else if (direction == "out")
             {
                 change = -0.1f;
             }
             else
             {
                 qCWarning(chatterinoHotkeys)
                     << "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
                 return "Invalid zoom direction, use \"in\", \"out\", or "
                        "\"reset\"";
             }
             getSettings()->setClampedUiScale(
                 getSettings()->getClampedUiScale() + change);
             return "";
         }},
        {"newTab",
         [this](std::vector<QString>) -> QString {
             this->notebook_->addPage(true);
             return "";
         }},
        {"removeTab",
         [this](std::vector<QString>) -> QString {
             this->notebook_->removeCurrentPage();
             return "";
         }},
        {"reopenSplit",
         [this](std::vector<QString>) -> QString {
             if (ClosedSplits::empty())
             {
                 return "";
             }
             ClosedSplits::SplitInfo si = ClosedSplits::pop();
             SplitContainer *splitContainer{nullptr};
             if (si.tab)
             {
                 splitContainer = dynamic_cast<SplitContainer *>(si.tab->page);
             }
             if (!splitContainer)
             {
                 splitContainer = this->notebook_->getOrAddSelectedPage();
             }
             this->notebook_->select(splitContainer);
             Split *split = new Split(splitContainer);
             split->setChannel(
                 getApp()->twitch->getOrAddChannel(si.channelName));
             split->setFilters(si.filters);
             splitContainer->appendSplit(split);
             return "";
         }},
        {"toggleLocalR9K",
         [](std::vector<QString>) -> QString {
             getSettings()->hideSimilar.setValue(!getSettings()->hideSimilar);
             getApp()->windows->forceLayoutChannelViews();
             return "";
         }},
        {"openQuickSwitcher",
         [](std::vector<QString>) -> QString {
             auto quickSwitcher =
                 new QuickSwitcherPopup(&getApp()->windows->getMainWindow());
             quickSwitcher->show();
             return "";
         }},
        {"quit",
         [](std::vector<QString>) -> QString {
             QApplication::exit();
             return "";
         }},
        {"moveTab",
         [this](std::vector<QString> arguments) -> QString {
             if (arguments.size() == 0)
             {
                 qCWarning(chatterinoHotkeys)
                     << "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
                 return "moveTab shortcut called without arguments. "
                        "Takes only one argument: new index (number, "
                        "\"next\" "
                        "or \"previous\")";
             }
             int newIndex = -1;
             bool indexIsGenerated =
                 false;  // indicates if `newIndex` was generated using target="next" or target="previous"

             auto target = arguments.at(0);
             qCDebug(chatterinoHotkeys) << target;
             if (target == "next")
             {
                 newIndex = this->notebook_->getSelectedIndex() + 1;
                 indexIsGenerated = true;
             }
             else if (target == "previous")
             {
                 newIndex = this->notebook_->getSelectedIndex() - 1;
                 indexIsGenerated = true;
             }
             else
             {
                 bool ok;
                 int result = target.toInt(&ok);
                 if (!ok)
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for moveTab shortcut";
                     return QString("Invalid argument for moveTab shortcut: "
                                    "%1. Use \"next\" or \"previous\" or an "
                                    "integer.")
                         .arg(target);
                 }
                 newIndex = result;
             }
             if (newIndex >= this->notebook_->getPageCount() || 0 > newIndex)
             {
                 if (indexIsGenerated)
                 {
                     return "";  // don't error out on generated indexes, ie move tab right
                 }
                 qCWarning(chatterinoHotkeys)
                     << "Invalid index for moveTab shortcut:" << newIndex;
                 return QString("Invalid index for moveTab shortcut: %1.")
                     .arg(newIndex);
             }
             this->notebook_->rearrangePage(this->notebook_->getSelectedPage(),
                                            newIndex);
             return "";
         }},
        {"setStreamerMode",
         [](std::vector<QString> arguments) -> QString {
             auto mode = 2;
             if (arguments.size() != 0)
             {
                 auto arg = arguments.at(0);
                 if (arg == "off")
                 {
                     mode = 0;
                 }
                 else if (arg == "on")
                 {
                     mode = 1;
                 }
                 else if (arg == "toggle")
                 {
                     mode = 2;
                 }
                 else if (arg == "auto")
                 {
                     mode = 3;
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for setStreamerMode hotkey: "
                         << arg;
                     return QString("Invalid argument for setStreamerMode "
                                    "hotkey: %1. Use \"on\", \"off\", "
                                    "\"toggle\" or \"auto\".")
                         .arg(arg);
                 }
             }

             if (mode == 0)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Disabled);
             }
             else if (mode == 1)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::Enabled);
             }
             else if (mode == 2)
             {
                 if (isInStreamerMode())
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Disabled);
                 }
                 else
                 {
                     getSettings()->enableStreamerMode.setValue(
                         StreamerModeSetting::Enabled);
                 }
             }
             else if (mode == 3)
             {
                 getSettings()->enableStreamerMode.setValue(
                     StreamerModeSetting::DetectObs);
             }
             return "";
         }},
        {"setTabVisibility",
         [this](std::vector<QString> arguments) -> QString {
             auto mode = 2;
             if (arguments.size() != 0)
             {
                 auto arg = arguments.at(0);
                 if (arg == "off")
                 {
                     mode = 0;
                 }
                 else if (arg == "on")
                 {
                     mode = 1;
                 }
                 else if (arg == "toggle")
                 {
                     mode = 2;
                 }
                 else
                 {
                     qCWarning(chatterinoHotkeys)
                         << "Invalid argument for setStreamerMode hotkey: "
                         << arg;
                     return QString("Invalid argument for setTabVisibility "
                                    "hotkey: %1. Use \"on\", \"off\" or "
                                    "\"toggle\".")
                         .arg(arg);
                 }
             }

             if (mode == 0)
             {
                 this->notebook_->setShowTabs(false);
             }
             else if (mode == 1)
             {
                 this->notebook_->setShowTabs(true);
             }
             else if (mode == 2)
             {
                 this->notebook_->setShowTabs(!this->notebook_->getShowTabs());
             }
             return "";
         }},
    };

    this->addDebugStuff(actions);

    this->shortcuts_ = getApp()->hotkeys->shortcutsForCategory(
        HotkeyCategory::Window, actions, this);
}

void Window::addMenuBar()
{
    QMenuBar *mainMenu = new QMenuBar();
    mainMenu->setNativeMenuBar(true);

    // First menu.
    QMenu *menu = mainMenu->addMenu(QString());
    QAction *prefs = menu->addAction(QString());
    prefs->setMenuRole(QAction::PreferencesRole);
    connect(prefs, &QAction::triggered, this, [this] {
        SettingsDialog::showDialog(this);
    });

    // Window menu.
    QMenu *windowMenu = mainMenu->addMenu(QString("Window"));

    QAction *nextTab = windowMenu->addAction(QString("Select next tab"));
    nextTab->setShortcuts({QKeySequence("Meta+Tab")});
    connect(nextTab, &QAction::triggered, this, [=] {
        this->notebook_->selectNextTab();
    });

    QAction *prevTab = windowMenu->addAction(QString("Select previous tab"));
    prevTab->setShortcuts({QKeySequence("Meta+Shift+Tab")});
    connect(prevTab, &QAction::triggered, this, [=] {
        this->notebook_->selectPreviousTab();
    });
}

void Window::onAccountSelected()
{
    auto user = getApp()->accounts->twitch.getCurrent();

    // update title (also append username on Linux and MacOS)
    QString windowTitle = Version::instance().fullVersion();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    if (user->isAnon())
    {
        windowTitle += " - not logged in";
    }
    else
    {
        windowTitle += " - " + user->getUserName();
    }
#endif

    this->setWindowTitle(windowTitle);

    // update user
    if (this->userLabel_)
    {
        if (user->isAnon())
        {
            this->userLabel_->getLabel().setText("anonymous");
        }
        else
        {
            this->userLabel_->getLabel().setText(user->getUserName());
        }
    }
}

}  // namespace chatterino
