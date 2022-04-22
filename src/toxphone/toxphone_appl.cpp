#include "toxphone_appl.h"
#include "tox/tox_net.h"
#include "tox/tox_func.h"
#include "tox/tox_logger.h"
#include "common/functions.h"
#include "audio/audio_dev.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/version_number.h"

#include "pproto/commands/pool.h"
#include "pproto/transport/tcp.h"

#include <sodium.h>
#include <string.h>

#define log_error_m   alog::logger().error  (alog_line_location, "Application")
#define log_warn_m    alog::logger().warn   (alog_line_location, "Application")
#define log_info_m    alog::logger().info   (alog_line_location, "Application")
#define log_verbose_m alog::logger().verbose(alog_line_location, "Application")
#define log_debug_m   alog::logger().debug  (alog_line_location, "Application")
#define log_debug2_m  alog::logger().debug2 (alog_line_location, "Application")

volatile bool Application::_stop = false;
std::atomic_int Application::_exitCode = {0};
QUuidEx Application::_applId = QUuidEx::createUuid();

Application::Application(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    _stopTimerId = startTimer(1000);

    sodium_memzero(_toxPublicKey, crypto_box_PUBLICKEYBYTES);
    sodium_memzero(_toxSecretKey, crypto_box_SECRETKEYBYTES);
    sodium_memzero(_configPublicKey, crypto_box_PUBLICKEYBYTES);

    chk_connect_q(&tcp::listener(), &tcp::Listener::message,
                  this, &Application::message)

    chk_connect_q(&tcp::listener(), &tcp::Listener::socketConnected,
                  this, &Application::socketConnected)

    chk_connect_q(&tcp::listener(), &tcp::Listener::socketDisconnected,
                  this, &Application::socketDisconnected)

    chk_connect_q(&udp::socket(), &udp::Socket::message,
                  this, &Application::message)

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &Application::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ToxCallState)
    FUNC_REGISTRATION(DiverterChange)
    FUNC_REGISTRATION(DiverterTest)
    FUNC_REGISTRATION(PhoneFriendInfo)
    FUNC_REGISTRATION(ConfigAuthorizationRequest)
    FUNC_REGISTRATION(ConfigAuthorization)
    FUNC_REGISTRATION(ConfigSavePassword)
    FUNC_REGISTRATION(PlaybackFinish)

    #undef FUNC_REGISTRATION
}

Application::~Application()
{
    // Реализация деструктора нужна для подавления inline warning-ов
}

void Application::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == _stopTimerId)
    {
        if (_stop)
        {
            exit(_exitCode);
            return;
        }
    }
}

void Application::stop(int exitCode)
{
    _exitCode = exitCode;
    stop();
}

void Application::message(const Message::Ptr& message)
{
    if (message->processed())
        return;

    if (lst::FindResult fr = _funcInvoker.findCommand(message->command()))
    {
        if (!command::pool().commandIsMultiproc(message->command()))
            message->markAsProcessed();
        _funcInvoker.call(message, fr);
    }
}

void Application::socketConnected(SocketDescriptor /*socketDescriptor*/)
{
    sendToxPhoneInfo();
}

void Application::socketDisconnected(SocketDescriptor socketDescriptor)
{
    if (toxConfig().isActive()
        && toxConfig().socketDescriptor == socketDescriptor)
    {
        toxConfig().reset();
        sodium_memzero(_toxPublicKey, crypto_box_PUBLICKEYBYTES);
        sodium_memzero(_toxSecretKey, crypto_box_SECRETKEYBYTES);
        sodium_memzero(_configPublicKey, crypto_box_PUBLICKEYBYTES);
    }

    data::AudioTest audioTest;
    audioTest.begin = false;
    audioTest.playback = true;
    audioTest.record = true;

    Message::Ptr m = createMessage(audioTest);
    emit internalMessage(m);

    sendToxPhoneInfo();
}

void Application::sendToxPhoneInfo()
{
    int port = 33601;
    config::base().getValue("config_connection.port", port);

    QString info = "Tox Phone Info";
    config::state().getValue("info_string", info, false);

    network::Interface::List nl = network::getInterfaces();
    _netInterfaces.swap(nl);
    _netInterfacesTimer.reset();

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    toxPhoneInfo.applId = _applId;
    toxPhoneInfo.configConnectCount = toxConfig().isActive() ? 1: 0;
    for (network::Interface* intf : _netInterfaces)
    {
        toxPhoneInfo.hostPoint = {intf->ip(), port};
        toxPhoneInfo.isPointToPoint = intf->isPointToPoint();
        sendUdpMessageToConfig(intf, port, toxPhoneInfo);
    }
}

void Application::command_IncomingConfigConnection(const Message::Ptr& message)
{
    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);
    Message::Ptr m = createMessage(diverterInfo);
    toxConfig().send(m);

    QString info = "Tox Phone Info";
    config::state().getValue("info_string", info, false);

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    m = createMessage(toxPhoneInfo);
    toxConfig().send(m);

    data::ToxPhoneAbout toxPhoneAbout;
    toxPhoneAbout.version = productVersion().vers;
    toxPhoneAbout.toxcore = VersionNumber(tox_version_major(),
                                          tox_version_minor(),
                                          tox_version_patch()).vers;
    toxPhoneAbout.gitrev = GIT_REVISION;
    toxPhoneAbout.qtvers = QT_VERSION_STR;
    toxPhoneAbout.sodium = sodium_version_string();
    m = createMessage(toxPhoneAbout);
    toxConfig().send(m);
}

void Application::command_ToxPhoneInfo(const Message::Ptr& message)
{
    // Обработка сообщения поступившего с TCP сокета
    if (message->socketType() == SocketType::Tcp)
    {
        data::ToxPhoneInfo toxPhoneInfo;
        readFromMessage(message, toxPhoneInfo);
        if (!toxPhoneInfo.info.trimmed().isEmpty())
        {
            config::state().setValue("info_string", toxPhoneInfo.info.trimmed());
            config::state().saveFile();
        }
        Message::Ptr m = createMessage(toxPhoneInfo);
        toxConfig().send(m);
        sendToxPhoneInfo();
        return;
    }

    // Обработка сообщения поступившего с UDP сокета
    if (_netInterfacesTimer.elapsed<chrono::seconds>() > 15)
    {
        network::Interface::List nl = network::getInterfaces();
        _netInterfaces.swap(nl);
        _netInterfacesTimer.reset();
    }

    QString info = "Tox Phone Info";
    config::state().getValue("info_string", info, false);

    int port = 33601;
    config::base().getValue("config_connection.port", port);

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    toxPhoneInfo.applId = _applId;
    toxPhoneInfo.configConnectCount = toxConfig().isActive() ? 1: 0;
    for (network::Interface* intf : _netInterfaces)
        if (message->sourcePoint().address().isInSubnet(intf->subnet(), intf->subnetPrefixLength()))
        {
            toxPhoneInfo.hostPoint = {intf->ip(), port};
            toxPhoneInfo.isPointToPoint = intf->isPointToPoint();

            Message::Ptr answer = message->cloneForAnswer();
            writeToMessage(toxPhoneInfo, answer);
            udp::socket().send(answer);
            break;
        }
}

void Application::command_ToxCallState(const Message::Ptr& message)
{
    readFromMessage(message, _callState);

    QFile pipe {"/tmp/toxphone/cpufreq"};
    if (pipe.exists())
    {
        // Изменяем частоту работы процессора
        if (pipe.open(QIODevice::WriteOnly))
        {
            const char* command = "cpufreq-max\n";
            if (_callState.direction == data::ToxCallState::Direction::Undefined
                && _callState.callState == data::ToxCallState::CallState::Undefined)
            {
                command = "cpufreq-default\n";
            }
            if (pipe.write(command) == -1)
                log_error_m << "Failed write command " << command
                            << " to pipe /tmp/toxphone/cpufreq";
            pipe.flush();
            pipe.close();
        }
        else
            log_error_m << "Failed open pipe /tmp/toxphone/cpufreq at write-mode";
    }

    if (!diverterIsActive())
        return;

    // Ожидание ответа на входящий вызов
    if (_callState.direction == data::ToxCallState::Direction::Incoming
        && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        if (phoneDiverter().handset() == PhoneDiverter::Handset::Off)
        {
            if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
                phoneDiverter().switchToUsb();

            if (!phoneDiverter().isRinging())
                phoneDiverter().startRing();

            phoneDiverter().stopDialTone();
        }
        else // PhoneDiverter::Handset::On
        {
            // Отклонить входящий вызов
            data::ToxCallAction toxCallAction;
            toxCallAction.action = data::ToxCallAction::Action::HandsetOn;
            toxCallAction.friendNumber = _callState.friendNumber;

            Message::Ptr m = createMessage(toxCallAction);
            emit internalMessage(m);
        }
    }

    // Входящий вызов принят
    else if (_callState.direction == data::ToxCallState::Direction::Incoming
             && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        if (phoneDiverter().isRinging())
            phoneDiverter().stopRing();

        phoneDiverter().stopDialTone();
    }

    // Ожидание ответа на исходящий вызов
    else if (_callState.direction == data::ToxCallState::Direction::Outgoing
             && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            phoneDiverter().switchToUsb();

        phoneDiverter().stopDialTone();
    }

    // Исходящий вызов принят
    else if (_callState.direction == data::ToxCallState::Direction::Outgoing
             && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        phoneDiverter().stopDialTone();
    }

    // Вызов завершен
    else if (_callState.direction == data::ToxCallState::Direction::Undefined
             && _callState.callState == data::ToxCallState::CallState::IsComplete)
    {
        if (phoneDiverter().isRinging())
            phoneDiverter().stopRing();

        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            phoneDiverter().switchToUsb();

        phoneDiverter().stopDialTone();
    }

    // Механизм вызовов переведен в неопределенное состояние
    else if (_callState.direction == data::ToxCallState::Direction::Undefined
             && _callState.callState == data::ToxCallState::CallState::Undefined)
    {
        setDiverterToDefaultState();
        resetDiverterPhoneNumber();
    }
    updateConfigDiverterInfo();
}

void Application::command_DiverterChange(const Message::Ptr& message)
{
    data::DiverterChange diverterChange;
    readFromMessage(message, diverterChange);

    data::MessageError error;
    QUuidEx failed_save_diverter_state__code {"8d1b2f14-4330-4ed3-8ac4-303e689db6df"};
    static const char* failed_save_diverter_state =
        QT_TRANSLATE_NOOP("ToxPhoneApplication", "Failed save a diverter state");

    if (_callState.direction == data::ToxCallState::Direction::Undefined)
    {
        if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::Active)
        {
            config::state().setValue("diverter.active", diverterChange.active);
            if (!config::state().saveFile())
            {
                error.code = failed_save_diverter_state__code;
                error.description = tr(failed_save_diverter_state);
                config::state().rereadFile();
            }
            log_verbose_m << "Diverter use state is changed to "
                          << (diverterChange.active ? "ON" : "OFF");
        }
        else if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
        {
            error.code = QUuidEx("bb76ae22-d123-465d-bcd5-b85f47ec98df");
            error.description = tr("Impossible to change settings of a diverter "
                                   "\nwhen handset is on");
        }
        else
        {
            if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::DefaultMode)
            {
                QString defaultMode =
                        (diverterChange.defaultMode == data::DiverterDefaultMode::Pstn)
                        ? "PSTN" : "USB";
                config::state().setValue("diverter.default_mode", defaultMode);
                if (!config::state().saveFile())
                {
                    error.code = failed_save_diverter_state__code;
                    error.description = tr(failed_save_diverter_state);
                    config::state().rereadFile();
                }
                log_verbose_m << "Diverter default mode is changed to " << defaultMode;
            }
            else if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::RingTone)
            {
                config::state().setValue("diverter.ring_tone", diverterChange.ringTone);
                if (!config::state().saveFile())
                {
                    error.code = failed_save_diverter_state__code;
                    error.description = tr(failed_save_diverter_state);
                    config::state().rereadFile();
                }
                log_verbose_m << "Diverter ringtone is changed to " << diverterChange.ringTone;
            }
        }
    }
    else
    {
        error.code = QUuidEx("f392cd66-1c5c-48ff-b58f-192a10276fd7");
        error.description = tr("Impossible to change settings of a diverter "
                               "\nduring a call");
    }
    if (!error.code.isNull())
    {
        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(error, answer);
        tcp::listener().send(answer);

        data::DiverterInfo diverterInfo;
        fillPhoneDiverter(diverterInfo);
        Message::Ptr m = createMessage(diverterInfo);
        toxConfig().send(m);
        return;
    }
    initPhoneDiverter();
}

void Application::command_DiverterTest(const Message::Ptr& message)
{
    data::DiverterTest diverterTest;
    readFromMessage(message, diverterTest);

    if (diverterTest.ringTone)
    {
        if (diverterTest.begin)
        {
            if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
            {
                data::MessageError error;
                error.code = QUuidEx("ba71e5e2-578c-4cb1-b06e-695b445165d3");
                error.description = tr("Ringtone test is impossible when handset is on");

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                tcp::listener().send(answer);
                return;
            }
            phoneDiverter().startRing();
        }
        else
            phoneDiverter().stopRing();
    }
}

void Application::command_PhoneFriendInfo(const Message::Ptr& message)
{
    data::PhoneFriendInfo phoneFriendInfo;
    readFromMessage(message, phoneFriendInfo);

    if (phoneFriendInfo.phoneNumber != 0)
        _phonesHash[phoneFriendInfo.phoneNumber] = phoneFriendInfo.publicKey;
    else
        _phonesHash.remove(phoneFriendInfo.phoneNumber);
}

void Application::command_ConfigAuthorizationRequest(const Message::Ptr& message)
{
    if (toxConfig().isActive())
    {
        data::MessageFailed failed;
        failed.description = tr("Tox a configurator is already connected to this client");

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(failed, answer);
        tcp::listener().send(answer);

        data::CloseConnection closeConnection;
        closeConnection.description = failed.description;

        Message::Ptr m = createMessage(closeConnection);
        m->appendDestinationSocket(message->socketDescriptor());
        tcp::listener().send(m);
        return;
    }

    data::ConfigAuthorizationRequest configAuthorizationRequest;
    readFromMessage(message, configAuthorizationRequest);

    if (configAuthorizationRequest.publicKey.length() != crypto_box_PUBLICKEYBYTES)
    {
        const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
            "Authorization request error. Invalid public key length: required %1, received %2");
        log_error_m << QString(msg).arg(crypto_box_PUBLICKEYBYTES)
                                   .arg(configAuthorizationRequest.publicKey.length());

        data::MessageError error;
        error.description = tr(msg).arg(crypto_box_PUBLICKEYBYTES)
                                   .arg(configAuthorizationRequest.publicKey.length());

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(error, answer);
        toxConfig().send(answer);

        data::CloseConnection closeConnection;
        closeConnection.description = QString(msg).arg(crypto_box_PUBLICKEYBYTES)
                                                  .arg(configAuthorizationRequest.publicKey.length());

        Message::Ptr m = createMessage(closeConnection);
        m->appendDestinationSocket(message->socketDescriptor());
        tcp::listener().send(m);
        return;
    }

    crypto_box_keypair(_toxPublicKey, _toxSecretKey);
    memcpy(_configPublicKey, configAuthorizationRequest.publicKey.constData(), crypto_box_PUBLICKEYBYTES);

    QString password;
    config::state().getValue("config_authorization.password", password, false);

    configAuthorizationRequest.publicKey = QByteArray((char*)_toxPublicKey, crypto_box_PUBLICKEYBYTES);
    configAuthorizationRequest.needPassword = !password.isEmpty();

    Message::Ptr answer = message->cloneForAnswer();
    writeToMessage(configAuthorizationRequest, answer);
    tcp::listener().send(answer);
}

void Application::command_ConfigAuthorization(const Message::Ptr& message)
{
    data::ConfigAuthorization configAuthorization;
    readFromMessage(message, configAuthorization);

    QString password;
    config::state().getValue("config_authorization.password", password, false);

    data::MessageError error;
    data::CloseConnection closeConnection;

    if (password.isEmpty() && configAuthorization.password.isEmpty())
    {
        // Авторизация без пароля
    }
    else if (!password.isEmpty() && !configAuthorization.password.isEmpty())
    {
        QByteArray passwBuff; passwBuff.resize(512);
        int plen = configAuthorization.password.length();
        int res = crypto_box_open_easy((uchar*)passwBuff.constData(),
                                       (uchar*)configAuthorization.password.constData(), plen,
                                       (uchar*)configAuthorization.nonce.constData(),
                                       _configPublicKey, _toxSecretKey);
        if (res != 0)
        {
            const char* msg =
                QT_TRANSLATE_NOOP("ToxPhoneApplication", "Failed decrypt password");
            error.code = QUuidEx("17ebf8eb-d7cc-4960-9161-48b15521aed0");
            error.description = tr(msg);
            closeConnection.description = msg;
            log_error_m << closeConnection.description;
        }
        else
        {
            QByteArray passw;
            {
                QDataStream s {&passwBuff, QIODevice::ReadOnly};
                STREAM_INIT(s)
                s >> passw;
            }

            QByteArray hash; hash.resize(crypto_generichash_BYTES);
            res = crypto_generichash((uchar*)hash.constData(), hash.length(),
                                     (uchar*)passw.constData(), passw.length(), 0, 0);
            if (res != 0)
            {
                const char* msg =
                    QT_TRANSLATE_NOOP("ToxPhoneApplication", "Failed generate password-hash");

                error.code = QUuidEx("15fb33fd-539f-4de0-96c2-35068eba8179");
                error.description = tr(msg);
                closeConnection.description = msg;
                log_error_m << closeConnection.description;
            }
            else
            {
                if (password != hash.toHex())
                {
                    const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
                        "Authorization failed. Mismatch of passwords. Code error: %1");

                    error.code = QUuidEx("006c8bf5-540f-4c82-b38e-91d54fe525bc");
                    error.description = tr(msg).arg(error.code.toString());
                    closeConnection.description = QString(msg).arg(error.code.toString());
                    log_error_m << closeConnection.description;
                }
            }
        }
    }
    else
    {
        const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
            "Authorization failed. Mismatch of passwords. Code error: %1");

        error.code = QUuidEx("dfb6dcba-e4fe-4dfe-aeb0-7ba0142cfa64");
        error.description = tr(msg).arg(error.code.toString());
        closeConnection.description = QString(msg).arg(error.code.toString());
        log_error_m << closeConnection.description;
    }
    if (!error.code.isNull())
    {
        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(error, answer);
        tcp::listener().send(answer);

        Message::Ptr m = createMessage(closeConnection);
        m->appendDestinationSocket(message->socketDescriptor());
        tcp::listener().send(m);
        return;
    }

    toxConfig().socketDescriptor = message->socketDescriptor();
    toxConfig().socket = tcp::listener().socketByDescriptor(message->socketDescriptor());

    Message::Ptr answer = message->cloneForAnswer();
    toxConfig().send(answer);

    Message::Ptr m = createMessage(command::IncomingConfigConnection);
    m->setTag(quint64(message->socketDescriptor()));
    emit internalMessage(m);

    sendToxPhoneInfo();
}

void Application::command_ConfigSavePassword(const Message::Ptr& message)
{
    // Сохранение пароля
    if (message->type() == Message::Type::Command)
    {
        if (!toxConfig().isActive())
            return;

        if (toxConfig().socketDescriptor != message->socketDescriptor())
            return;

        data::ConfigSavePassword configSavePassword;
        readFromMessage(message, configSavePassword);

        if (!configSavePassword.password.isEmpty())
        {
            QByteArray passwBuff; passwBuff.resize(512);
            int plen = configSavePassword.password.length();
            int res = crypto_box_open_easy((uchar*)passwBuff.constData(),
                                           (uchar*)configSavePassword.password.constData(), plen,
                                           (uchar*)configSavePassword.nonce.constData(),
                                           _configPublicKey, _toxSecretKey);
            if (res != 0)
            {
                data::MessageError error;
                error.description = tr("Failed decrypt password");

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                toxConfig().send(answer);
                return;
            }

            QByteArray passw;
            {
                QDataStream s {&passwBuff, QIODevice::ReadOnly};
                STREAM_INIT(s)
                s >> passw;
            }

            QByteArray hash; hash.resize(crypto_generichash_BYTES);
            res = crypto_generichash((uchar*)hash.constData(), hash.length(),
                                     (uchar*)passw.constData(), passw.length(), 0, 0);
            if (res != 0)
            {
                data::MessageError error;
                error.description = tr("Failed generate password-hash");

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                toxConfig().send(answer);
                return;
            }
            configSavePassword.password = hash.toHex();
            config::state().setValue("config_authorization.password",
                                     string(configSavePassword.password));
        }
        else
        {
            config::state().remove("config_authorization.password", false);
        }
        config::state().saveFile();

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(configSavePassword, answer);
        toxConfig().send(answer);
    }
}

void Application::command_PlaybackFinish(const Message::Ptr& message)
{
    if (_callState.direction == data::ToxCallState::Direction::Undefined
        && _callState.callState == data::ToxCallState::CallState::Undefined)
    {
        setDiverterToDefaultState();
        resetDiverterPhoneNumber();
    }
}

void Application::fillPhoneDiverter(data::DiverterInfo& diverterInfo)
{
    YamlConfig::Func loadFunc =
        [&diverterInfo](YamlConfig* conf, YAML::Node& node, bool logWarn)
    {
        if (!node.IsMap())
            return false;

        diverterInfo.active = true;
        conf->getValue(node, "active", diverterInfo.active, logWarn);

        QString defaultMode = "PSTN";
        conf->getValue(node, "default_mode", defaultMode, logWarn);
        diverterInfo.defaultMode = (defaultMode == "PSTN")
                                   ? data::DiverterDefaultMode::Pstn
                                   : data::DiverterDefaultMode::Usb;

        diverterInfo.ringTone = "DbDt";
        conf->getValue(node, "ring_tone", diverterInfo.ringTone, logWarn);
        return true;
    };
    config::state().getValue("diverter", loadFunc);

    diverterInfo.currentMode = "Undefined";
    diverterInfo.attached = phoneDiverter().isAttached();
    if (phoneDiverter().isAttached())
    {
        phoneDiverter().getInfo(diverterInfo.deviceUsbBus,
                                diverterInfo.deviceName,
                                diverterInfo.deviceVersion,
                                diverterInfo.deviceSerial);
        diverterInfo.currentMode =
            (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            ? "PSTN" : "USB";
        diverterInfo.ringTone = phoneDiverter().ringTone();
    }
}

void Application::updateConfigDiverterInfo()
{
    if (!toxConfig().isActive())
        return;

    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);
    Message::Ptr m = createMessage(diverterInfo);
    toxConfig().send(m);
}

void Application::initPhoneDiverter()
{
    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);

    bool active = diverterInfo.active && phoneDiverter().isAttached();
    diverterIsActive(&active);

    if (phoneDiverter().isAttached())
    {
        if (diverterInfo.defaultMode == data::DiverterDefaultMode::Pstn)
        {
            _diverterDefaultMode = PhoneDiverter::Mode::Pstn;
            phoneDiverter().switchToPstn();
        }
        else
        {
            _diverterDefaultMode = PhoneDiverter::Mode::Usb;
            phoneDiverter().switchToUsb();
        }
        diverterInfo.currentMode =
            (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            ? "PSTN" : "USB";
        phoneDiverter().setRingTone(diverterInfo.ringTone);
    }

    if (toxConfig().isActive())
    {
        Message::Ptr m = createMessage(diverterInfo);
        toxConfig().send(m);
    }
}

void Application::setDiverterToDefaultState()
{
    if (!diverterIsActive())
        return;

    if (phoneDiverter().isRinging())
        phoneDiverter().stopRing();

    if (phoneDiverter().mode() != _diverterDefaultMode)
        phoneDiverter().setMode(_diverterDefaultMode);

    if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
    {
        phoneDiverter().startDialTone();
    }
    else // PhoneDiverter::Handset::Off
    {
        if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
            phoneDiverter().stopDialTone();
    }
}

void Application::resetDiverterPhoneNumber()
{
    _asteriskPressed = false;
    _diverterPhoneNumber.clear();
}

void Application::phoneDiverterAttached()
{
    YamlConfig::Func loadFunc = [this](YamlConfig* conf, YAML::Node& phones, bool)
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

            quint32 phoneNumber = 0;
            if (conf->getValue(it->second, "phone_number", phoneNumber, false))
                _phonesHash[phoneNumber] = publicKey;
        }
        return true;
    };
    config::state().rereadFile();
    config::state().getValue("phones", loadFunc);

    initPhoneDiverter();
}

void Application::phoneDiverterDetached()
{
    initPhoneDiverter();
}

void Application::phoneDiverterPstnRing()
{
    break_point
}

void Application::phoneDiverterKey(int val)
{
    { //Block for alog::Line
        alog::Line logLine = log_debug_m << "Phone key is pressed: ";
        if (val == 0x0b)
            logLine << "*";
        else if (val == 0x0c)
            logLine << "#";
        else
            logLine << val;
        logLine << "  Diverter use state: "
                << (diverterIsActive() ? "ON" : "OFF");
    }
    if (!diverterIsActive())
        return;

    bool canToCall = _callState.direction == data::ToxCallState::Direction::Undefined
                     && _callState.callState == data::ToxCallState::CallState::Undefined;
    if (!canToCall)
        return;

    if (val >= 0x00 && val < 0x0a) // Нажата цифра
    {
        if (_asteriskPressed)
            _diverterPhoneNumber.append(QString::number(val));
    }
    else if (val == 0x0b) // Нажата '*'
    {
        if (phoneDiverter().handset() == PhoneDiverter::Handset::On
            && _diverterHandsetTimer.elapsed() > 5*1000 /*5 сек*/ )
        {
            // Обработка случайного нажатия '*' во время телефонного разговора
            log_debug_m << "Expired timeout for press '*' (5 sec)"
                        << "; expired: " << _diverterHandsetTimer.elapsed() << " msec";
            return;
        }

        _asteriskPressed = true;
        _diverterPhoneNumber.clear();
        if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
        {
            phoneDiverter().switchToUsb();
            updateConfigDiverterInfo();
        }
        if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
            phoneDiverter().startDialTone();
    }
    else if (val == 0x0c) // Нажата '#'
    {
        if (!_asteriskPressed)
        {
            resetDiverterPhoneNumber();
            return;
        }
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

            if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
            {
                phoneDiverter().stopDialTone();
                audioDev().playError();
            }
            resetDiverterPhoneNumber();
            return;
        }

        QByteArray friendPubKey = _phonesHash[phoneNum];
        if (friendPubKey.isEmpty())
        {
            log_error_m << "Failed find friend public key in phones-hash list "
                        << "for phone number " << phoneNum;

            if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
            {
                phoneDiverter().stopDialTone();
                audioDev().playError();
            }
            resetDiverterPhoneNumber();
            return;
        }

        QByteArray friendPk = QByteArray::fromHex(friendPubKey);
        uint32_t friendNumber = getToxFriendNum(toxNet().tox(), friendPk);
        if (friendNumber == UINT32_MAX)
        {
            log_error_m << "Incorrect friend number for friend public key "
                        << friendPubKey;

            if (phoneDiverter().mode() == PhoneDiverter::Mode::Usb)
            {
                phoneDiverter().stopDialTone();
                audioDev().playError();
            }
            resetDiverterPhoneNumber();
            return;
        }
        log_debug_m << "Call phone number: *" << phoneNum
                    << "#  " << ToxFriendLog(toxNet().tox(), friendNumber);

        // Новый вызов
        data::ToxCallAction toxCallAction;
        toxCallAction.action = data::ToxCallAction::Action::Call;
        toxCallAction.friendNumber = friendNumber;

        Message::Ptr m = createMessage(toxCallAction);
        emit internalMessage(m);
    }
}

void Application::phoneDiverterHandset(PhoneDiverter::Handset handset)
{
    resetDiverterPhoneNumber();
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
            return;
        }

        if (_callState.direction == data::ToxCallState::Direction::Undefined
            && _callState.callState == data::ToxCallState::CallState::IsComplete)
        {
            data::DiverterHandset diverterHandset;
            diverterHandset.on = false;

            Message::Ptr m = createMessage(diverterHandset);
            emit internalMessage(m);
        }
        else if (_callState.direction == data::ToxCallState::Direction::Undefined
                 && _callState.callState == data::ToxCallState::CallState::Undefined)
        {
            setDiverterToDefaultState();
        }
    }
    else // PhoneDiverter::Handset::On
    {
        _diverterHandsetTimer.restart();

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
