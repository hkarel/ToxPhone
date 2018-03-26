#include "toxphone_appl.h"
#include "common/functions.h"
#include "tox/tox_net.h"
#include "tox/tox_func.h"
#include "tox/tox_logger.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/communication/transport/udp.h"

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")

volatile bool ToxPhoneApplication::_stop = false;
std::atomic_int ToxPhoneApplication::_exitCode = {0};
QUuidEx ToxPhoneApplication::_applId = QUuidEx::createUuid();

ToxPhoneApplication::ToxPhoneApplication(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    _stopTimerId = startTimer(1000);

    chk_connect_q(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))
    chk_connect_q(&tcp::listener(), SIGNAL(socketConnected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(&tcp::listener(), SIGNAL(socketDisconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    chk_connect_q(&udp::socket(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))


    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ToxPhoneApplication::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ToxCallState)
    FUNC_REGISTRATION(DiverterChange)
    FUNC_REGISTRATION(DiverterTest)
    FUNC_REGISTRATION(PhoneFriendInfo)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

ToxPhoneApplication::~ToxPhoneApplication()
{
    // Реализация деструктора нужна для подавления inline warning-ов
}

void ToxPhoneApplication::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == _stopTimerId)
    {
        if (_stop)
            exit(_exitCode);
    }
}

void ToxPhoneApplication::stop(int exitCode)
{
    _exitCode = exitCode;
    stop();
}

void ToxPhoneApplication::message(const Message::Ptr& message)
{
    if (message->processed())
        return;

    if (_funcInvoker.containsCommand(message->command()))
    {
        if (!commandsPool().commandIsMultiproc(message->command()))
            message->markAsProcessed();
        _funcInvoker.call(message);
    }
}

void ToxPhoneApplication::socketConnected(SocketDescriptor socketDescriptor)
{
    bool connected = true;
    configConnected(&connected);

    Message::Ptr m = createMessage(command::IncomingConfigConnection);
    m->setTag(quint64(socketDescriptor));
    emit internalMessage(m);

    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);
    if (phoneDiverter().isAttached())
    {
        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            diverterInfo.defaultMode = data::DiverterDefaultMode::Pstn;
        else
            diverterInfo.defaultMode = data::DiverterDefaultMode::Usb;

        diverterInfo.ringTone = phoneDiverter().ringTone();
    }
    m = createMessage(diverterInfo, Message::Type::Event);
    tcp::listener().send(m);
}

void ToxPhoneApplication::socketDisconnected(SocketDescriptor /*socketDescriptor*/)
{
    bool connected = false;
    configConnected(&connected);

    data::AudioTest audioTest;
    audioTest.begin = false;
    audioTest.playback = true;
    audioTest.record = true;

    Message::Ptr m = createMessage(audioTest);
    emit internalMessage(m);
}

void ToxPhoneApplication::sendToxPhoneInfo()
{
    int port = 3609;
    config::base().getValue("config_connection.port", port);

    QString info;
    config::base().getValue("config_connection.info", info);

    network::Interface::List nl = network::getInterfaces();
    _netInterfaces.swap(nl);
    _netInterfacesTimer.reset();

    for (network::Interface* intf : _netInterfaces)
    {
        data::ToxPhoneInfo toxPhoneInfo;
        toxPhoneInfo.info = info;
        toxPhoneInfo.applId = _applId;
        toxPhoneInfo.isPointToPoint = intf->isPointToPoint();

        Message::Ptr m = createMessage(toxPhoneInfo);
        m->destinationPoints().insert({intf->broadcast, port - 1});
        udp::socket().send(m);
    }
}

void ToxPhoneApplication::command_ToxPhoneInfo(const Message::Ptr& message)
{
    if (_netInterfacesTimer.elapsed<chrono::seconds>() > 15)
    {
        network::Interface::List nl = network::getInterfaces();
        _netInterfaces.swap(nl);
        _netInterfacesTimer.reset();
    }

    QString info;
    config::base().getValue("config_connection.info", info);

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    toxPhoneInfo.applId = _applId;
    for (network::Interface* intf : _netInterfaces)
        if (message->sourcePoint().address().isInSubnet(intf->subnet, intf->subnetPrefixLength))
        {
            toxPhoneInfo.isPointToPoint = intf->isPointToPoint();
            break;
        }

    Message::Ptr answer = message->cloneForAnswer();
    writeToMessage(toxPhoneInfo, answer);
    udp::socket().send(answer);
}

void ToxPhoneApplication::command_ToxCallState(const Message::Ptr& message)
{
    readFromMessage(message, _callState);

    if (!diverterIsActive())
        return;

    if (_callState.direction == data::ToxCallState::Direction::Incoming
        && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        if (phoneDiverter().handset() == PhoneDiverter::Handset::Off)
        {
            if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
                phoneDiverter().switchToUsb();

            if (!phoneDiverter().isRinging())
                phoneDiverter().startRing();

            if (phoneDiverter().dialTone())
                phoneDiverter().stopDialTone();
        }
        else //handset() == PhoneDiverter::Handset::On
        {
            // Отклонить входящий вызов
            data::ToxCallAction toxCallAction;
            toxCallAction.action = data::ToxCallAction::Action::Reject;
            toxCallAction.friendNumber = _callState.friendNumber;

            Message::Ptr m = createMessage(toxCallAction);
            emit internalMessage(m);
        }
    }
    if (_callState.direction == data::ToxCallState::Direction::Incoming
        && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        if (phoneDiverter().isRinging())
            phoneDiverter().stopRing();

        phoneDiverter().stopDialTone();
    }
    if (_callState.direction == data::ToxCallState::Direction::Outgoing
        && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            phoneDiverter().switchToUsb();

        phoneDiverter().stopDialTone();
    }
    if (_callState.direction == data::ToxCallState::Direction::Outgoing
        && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        phoneDiverter().stopDialTone();
    }
    if (_callState.direction == data::ToxCallState::Direction:: Undefined
        && _callState.callState == data::ToxCallState::CallState::Undefined)
    {
        if (phoneDiverter().isRinging())
            phoneDiverter().stopRing();

        phoneDiverter().setMode(_diverterDefaultMode);

        if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
        {
            phoneDiverter().startDialTone();
        }
        else // handset() == PhoneDiverter::Handset::Off
        {
            if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
                phoneDiverter().stopDialTone();
        }
    }
}

void ToxPhoneApplication::command_DiverterChange(const Message::Ptr& message)
{
    data::DiverterChange diverterChange;
    readFromMessage(message, diverterChange);

    data::MessageError error;
    if (_callState.direction == data::ToxCallState::Direction::Undefined)
    {
        if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::Active)
        {
            bool active = diverterChange.active && phoneDiverter().isAttached();
            diverterIsActive(&active);
            config::state().setValue("diverter.active", diverterChange.active);
        }
        else if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::DefaultMode)
        {
            if (diverterChange.defaultMode == data::DiverterDefaultMode::Pstn)
            {
                if (phoneDiverter().switchToPstn())
                {
                    config::state().setValue("diverter.default_mode", string("PSTN"));
                }
                else
                {
                    error.code = 1;
                    error.description = tr("Failed switch a diverter to PSTN default mode.");
                    if (!phoneDiverter().isAttached())
                        error.description += tr("\nDevice not attached");
                }
            }
            else
            {
                if (phoneDiverter().switchToUsb())
                {
                    config::state().setValue("diverter.default_mode", string("USB"));
                }
                else
                {
                    error.code = 1;
                    error.description = tr("Failed switch a diverter to USB default mode.");
                    if (!phoneDiverter().isAttached())
                        error.description += tr("\nDevice not attached");
                }
            }
        }
        else if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::RingTone)
        {
            phoneDiverter().setRingTone(diverterChange.ringTone);
            config::state().setValue("diverter.ring_tone", diverterChange.ringTone);
        }

        if (error.code == 0
            && !config::state().save())
        {
            error.code = 1;
            error.description = tr("Failed save a diverter state");
        }
    }
    else
    {
        error.code = 2;
        error.description =
            tr("It is impossible to change settings of a diverter during a call");
    }
    if (error.code != 0)
    {
        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(error, answer);
        tcp::listener().send(answer);
    }
    if (error.code == 2)
    {
        data::DiverterInfo diverterInfo;
        fillPhoneDiverter(diverterInfo);

        Message::Ptr m = createMessage(diverterInfo, Message::Type::Event);
        tcp::listener().send(m);
        return;
    }

    initPhoneDiverter();
}

void ToxPhoneApplication::command_DiverterTest(const Message::Ptr& message)
{
    data::DiverterTest diverterTest;
    readFromMessage(message, diverterTest);

    if (diverterTest.ringTone)
    {
        if (diverterTest.begin)
            phoneDiverter().startRing();
        else
            phoneDiverter().stopRing();
    }
}

void ToxPhoneApplication::command_PhoneFriendInfo(const Message::Ptr& message)
{
    data::PhoneFriendInfo phoneFriendInfo;
    readFromMessage(message, phoneFriendInfo);

    if (phoneFriendInfo.phoneNumber != 0)
        _phonesHash[phoneFriendInfo.phoneNumber] = phoneFriendInfo.publicKey;
    else
        _phonesHash.remove(phoneFriendInfo.phoneNumber);
}

void ToxPhoneApplication::fillPhoneDiverter(data::DiverterInfo& diverterInfo)
{
    YamlConfig::Func loadFunc = [&diverterInfo](YAML::Node& node, bool)
    {
        diverterInfo.active = node["active"].as<bool>(true);
        string str = node["default_mode"].as<string>("PSTN");
        utl::trim(str);
        diverterInfo.defaultMode = (str == "PSTN")
                                   ? data::DiverterDefaultMode::Pstn
                                   : data::DiverterDefaultMode::Usb;

        str = node["ring_tone"].as<string>("DbDt");
        diverterInfo.ringTone = QString::fromUtf8(str.c_str()).trimmed();
        return true;
    };
    config::state().getValue("diverter", loadFunc);

    diverterInfo.attached = phoneDiverter().isAttached();
    if (phoneDiverter().isAttached())
    {
        phoneDiverter().getInfo(diverterInfo.deviceUsbBus,
                                diverterInfo.deviceName,
                                diverterInfo.deviceVersion,
                                diverterInfo.deviceSerial);
    }
}

void ToxPhoneApplication::initPhoneDiverter()
{
    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);

    bool active = diverterInfo.active && phoneDiverter().isAttached();
    diverterIsActive(&active);

    if (phoneDiverter().isAttached())
    {
        if (diverterInfo.defaultMode == data::DiverterDefaultMode::Pstn)
            phoneDiverter().switchToPstn();
        else
            phoneDiverter().switchToUsb();

        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
        {
            diverterInfo.defaultMode = data::DiverterDefaultMode::Pstn;
            _diverterDefaultMode = PhoneDiverter::Mode::Pstn;
        }
        else
        {
            diverterInfo.defaultMode = data::DiverterDefaultMode::Usb;
            _diverterDefaultMode = PhoneDiverter::Mode::Usb;
        }
        phoneDiverter().setRingTone(diverterInfo.ringTone);
    }

    if (configConnected())
    {
        Message::Ptr m = createMessage(diverterInfo, Message::Type::Event);
        tcp::listener().send(m);
    }
}

void ToxPhoneApplication::phoneDiverterAttached()
{
    YamlConfig::Func loadFunc = [this](YAML::Node& phones, bool)
    {
        for(auto  it = phones.begin(); it != phones.end(); ++it)
        {
            QByteArray publicKey = it->first.as<string>("").c_str();
            if (publicKey.isEmpty())
                continue;

            /** Возможно потребуется **
            if (publicKey.length() != TOX_PUBLIC_KEY_SIZE*2)
            {
                log_verbose_m << "Invalid Tox public key length; Public key: " << publicKey;
                continue;
            }

            static QRegExp regex {QString("^[A-Fa-f0-9]{%1}$").arg(TOX_PUBLIC_KEY_SIZE*2)};
            if (!regex.exactMatch(publicKey))
            {
                log_verbose_m << "Invalid public key format";
                continue;
            }
            */

            YAML::Node val = it->second;
            quint32 phoneNumber = val["phone_number"].as<int>(0);
            _phonesHash[phoneNumber] = publicKey;
        }
        return true;
    };
    config::state().reRead();
    config::state().getValue("phones", loadFunc);

    initPhoneDiverter();
}

void ToxPhoneApplication::phoneDiverterDetached()
{
    initPhoneDiverter();
}

void ToxPhoneApplication::phoneDiverterPstnRing()
{
    break_point
}

void ToxPhoneApplication::phoneDiverterKey(int val)
{
    if (!diverterIsActive())
        return;

    if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
        if (_diverterPhoneNumber.isEmpty())
            phoneDiverter().stopDialTone();

    if (val >= 0x00 && val < 0x0a)
    {
        _diverterPhoneNumber.append(QString::number(val));
    }
    else if (val == 0x0b)
    {
        // Нажата '*'
        if (_diverterPhoneNumber.isEmpty())
        {
            phoneDiverter().switchToUsb();
            phoneDiverter().startDialTone();
        }
    }
    else if (val == 0x0c)
    {
        phoneDiverter().stopDialTone();

        // Удаляем ведущий символ '*'
        QString phoneNumber = _diverterPhoneNumber;
        while (!phoneNumber.isEmpty() && phoneNumber[0] == QChar('*'))
            phoneNumber.remove(0, 1);

        bool ok;
        quint32 phoneNum = phoneNumber.toUInt(&ok);
        if (!ok)
        {
            log_error_m << "Failed convert phone number "
                        << phoneNumber << " to integer";
            return;
        }

        QByteArray friendPubKey = _phonesHash[phoneNum];
        if (friendPubKey.isEmpty())
        {
            log_error_m << "Failed find friend public key in phones-hash list "
                        << "for phone number " << phoneNum;
            return;
        }

        QByteArray friendPk = QByteArray::fromHex(friendPubKey);
        uint32_t friendNumber = getToxFriendNum(toxNet().tox(), friendPk);
        if (friendNumber == UINT32_MAX)
        {
            log_error_m << "Incorrect friend number for friend public key "
                        << friendPubKey;
            return;
        }
        log_debug_m << "Call phone number: *" << phoneNum
                    << "#. " << ToxFriendLog(toxNet().tox(), friendNumber);

        // Новый вызов
        data::ToxCallAction toxCallAction;
        toxCallAction.action = data::ToxCallAction::Action::Call;
        toxCallAction.friendNumber = friendNumber;

        Message::Ptr m = createMessage(toxCallAction);
        emit internalMessage(m);
    }
}

void ToxPhoneApplication::phoneDiverterHandset(PhoneDiverter::Handset handset)
{
    _diverterPhoneNumber.clear();
    if (!diverterIsActive())
        return;

    if (handset == PhoneDiverter::Handset::Off)
    {
        data::ToxCallAction toxCallAction;

        // Завершить исходящий вызов
        if (_callState.direction == data::ToxCallState::Direction::Outgoing)
        {
            toxCallAction.action = data::ToxCallAction::Action::End;
            toxCallAction.friendNumber = _callState.friendNumber;
        }

        // Завершить входящий вызов
        else if (_callState.direction == data::ToxCallState::Direction::Incoming
                 && _callState.callState == data::ToxCallState::CallState::InProgress)
        {
            toxCallAction.action = data::ToxCallAction::Action::End;
            toxCallAction.friendNumber = _callState.friendNumber;
        }

        // Отклонить входящий вызов
        else if (_callState.direction == data::ToxCallState::Direction::Incoming
                 && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
        {
            toxCallAction.action = data::ToxCallAction::Action::Reject;
            toxCallAction.friendNumber = _callState.friendNumber;
        }

        if (toxCallAction.action != data::ToxCallAction::Action::None)
        {
            Message::Ptr m = createMessage(toxCallAction);
            emit internalMessage(m);
        }

        if (_diverterDefaultMode == PhoneDiverter::Mode::Usb)
            phoneDiverter().stopDialTone();
    }

    else if (handset == PhoneDiverter::Handset::On)
    {
        // Принять входящий вызов
        if (_callState.direction == data::ToxCallState::Direction::Incoming
                 && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
        {
            data::ToxCallAction toxCallAction;
            toxCallAction.action = data::ToxCallAction::Action::Accept;
            toxCallAction.friendNumber = _callState.friendNumber;

            Message::Ptr m = createMessage(toxCallAction);
            emit internalMessage(m);
        }
    }
}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
