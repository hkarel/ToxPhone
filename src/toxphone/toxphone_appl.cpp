#include "toxphone_appl.h"
#include "tox/tox_net.h"
#include "tox/tox_func.h"
#include "tox/tox_logger.h"
#include "common/functions.h"
#include "audio/audio_dev.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/version/version_number.h"

#include <sodium.h>
#include <string.h>


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

    sodium_memzero(_toxPublicKey, crypto_box_PUBLICKEYBYTES);
    sodium_memzero(_toxSecretKey, crypto_box_SECRETKEYBYTES);
    sodium_memzero(_configPublicKey, crypto_box_PUBLICKEYBYTES);

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
        {
            exit(_exitCode);
            return;
        }
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

void ToxPhoneApplication::socketConnected(SocketDescriptor /*socketDescriptor*/)
{
    sendToxPhoneInfo();
}

void ToxPhoneApplication::socketDisconnected(SocketDescriptor socketDescriptor)
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

void ToxPhoneApplication::sendToxPhoneInfo()
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
        toxPhoneInfo.hostPoint = {intf->ip, port};
        toxPhoneInfo.isPointToPoint = intf->isPointToPoint();
        sendUdpMessageToConfig(intf, port, toxPhoneInfo);
    }
}

void ToxPhoneApplication::command_IncomingConfigConnection(const Message::Ptr& message)
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
    m = createMessage(toxPhoneAbout);
    toxConfig().send(m);
}

void ToxPhoneApplication::command_ToxPhoneInfo(const Message::Ptr& message)
{
    // Обработка сообщения поступившего с TCP сокета
    if (message->socketType() == SocketType::Tcp)
    {
        data::ToxPhoneInfo toxPhoneInfo;
        readFromMessage(message, toxPhoneInfo);
        if (!toxPhoneInfo.info.trimmed().isEmpty())
        {
            config::state().setValue("info_string", toxPhoneInfo.info.trimmed());
            config::state().save();
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

    int port = 33610;
    config::base().getValue("config_connection.port", port);

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    toxPhoneInfo.applId = _applId;
    toxPhoneInfo.configConnectCount = toxConfig().isActive() ? 1: 0;
    for (network::Interface* intf : _netInterfaces)
        if (message->sourcePoint().address().isInSubnet(intf->subnet, intf->subnetPrefixLength))
        {
            toxPhoneInfo.hostPoint = {intf->ip, port};
            toxPhoneInfo.isPointToPoint = intf->isPointToPoint();

            Message::Ptr answer = message->cloneForAnswer();
            writeToMessage(toxPhoneInfo, answer);
            udp::socket().send(answer);
            break;
        }
}

void ToxPhoneApplication::command_ToxCallState(const Message::Ptr& message)
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
        _diverterPhoneNumber.clear();
    }
    updateConfigDiverterInfo();
}

void ToxPhoneApplication::command_DiverterChange(const Message::Ptr& message)
{
    data::DiverterChange diverterChange;
    readFromMessage(message, diverterChange);

    data::MessageError error;
    static const char* failed_save_diverter_state =
        QT_TRANSLATE_NOOP("ToxPhoneApplication", "Failed save a diverter state");

    if (_callState.direction == data::ToxCallState::Direction::Undefined)
    {
        if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::Active)
        {
            config::state().setValue("diverter.active", diverterChange.active);
            if (!config::state().save())
            {
                error.code = 1;
                error.description = tr(failed_save_diverter_state);
                config::state().reRead();
            }
            log_verbose_m << "Diverter use state is changed to "
                          << (diverterChange.active ? "ON" : "OFF");
        }
        else if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
        {
            error.code = 3;
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
                if (!config::state().save())
                {
                    error.code = 1;
                    error.description = tr(failed_save_diverter_state);
                    config::state().reRead();
                }
                log_verbose_m << "Diverter default mode is changed to " << defaultMode;
            }
            else if (diverterChange.changeFlag == data::DiverterChange::ChangeFlag::RingTone)
            {
                config::state().setValue("diverter.ring_tone", diverterChange.ringTone);
                if (!config::state().save())
                {
                    error.code = 1;
                    error.description = tr(failed_save_diverter_state);
                    config::state().reRead();
                }
                log_verbose_m << "Diverter ringtone is changed to " << diverterChange.ringTone;
            }
        }
    }
    else
    {
        error.code = 2;
        error.description = tr("Impossible to change settings of a diverter "
                               "\nduring a call");
    }
    if (error.code != 0)
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

void ToxPhoneApplication::command_DiverterTest(const Message::Ptr& message)
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
                error.code = 1;
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

void ToxPhoneApplication::command_PhoneFriendInfo(const Message::Ptr& message)
{
    data::PhoneFriendInfo phoneFriendInfo;
    readFromMessage(message, phoneFriendInfo);

    if (phoneFriendInfo.phoneNumber != 0)
        _phonesHash[phoneFriendInfo.phoneNumber] = phoneFriendInfo.publicKey;
    else
        _phonesHash.remove(phoneFriendInfo.phoneNumber);
}

void ToxPhoneApplication::command_ConfigAuthorizationRequest(const Message::Ptr& message)
{
    if (toxConfig().isActive())
    {
        data::MessageFailed failed;
        failed.description = tr("Tox a configurator is already connected to this client");

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(failed, answer);
        toxConfig().send(answer);

        data::CloseConnection closeConnection;
        closeConnection.description = failed.description;

        Message::Ptr m = createMessage(closeConnection);
        m->destinationSocketDescriptors().insert(message->socketDescriptor());
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
        m->destinationSocketDescriptors().insert(message->socketDescriptor());
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

void ToxPhoneApplication::command_ConfigAuthorization(const Message::Ptr& message)
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
            const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
                                                "Failed decrypt password");
            error.code = 1;
            error.description = tr(msg);
            closeConnection.description = msg;
            log_error_m << msg;
        }
        else
        {
            QByteArray passw;
            {
                QDataStream s {&passwBuff, QIODevice::ReadOnly};
                s.setVersion(Q_DATA_STREAM_VERSION);
                s >> passw;
            }

            QByteArray hash; hash.resize(crypto_generichash_BYTES);
            res = crypto_generichash((uchar*)hash.constData(), hash.length(),
                                     (uchar*)passw.constData(), passw.length(), 0, 0);
            if (res != 0)
            {
                const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
                                                    "Failed generate password-hash");
                error.code = 2;
                error.description = tr(msg);
                closeConnection.description = msg;
                log_error_m << msg;
            }
            else
            {
                if (password != hash.toHex())
                {
                    const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
                        "Authorization failed. Mismatch of passwords. Code error: %1");

                    error.code = 3;
                    error.description = tr(msg).arg(error.code);
                    closeConnection.description = QString(msg).arg(error.code);
                    log_error_m << QString(msg).arg(error.code);
                }
            }
        }
    }
    else
    {
        const char* msg = QT_TRANSLATE_NOOP("ToxPhoneApplication",
            "Authorization failed. Mismatch of passwords. Code error: %1");

        error.code = 4;
        error.description = tr(msg).arg(error.code);
        closeConnection.description = QString(msg).arg(error.code);
        log_error_m << QString(msg).arg(error.code);
    }
    if (error.code != 0)
    {
        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(error, answer);
        tcp::listener().send(answer);

        Message::Ptr m = createMessage(closeConnection);
        m->destinationSocketDescriptors().insert(message->socketDescriptor());
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
}

void ToxPhoneApplication::command_ConfigSavePassword(const Message::Ptr& message)
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
                s.setVersion(Q_DATA_STREAM_VERSION);
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
        config::state().save();

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(configSavePassword, answer);
        toxConfig().send(answer);
    }
}

void ToxPhoneApplication::command_PlaybackFinish(const Message::Ptr& message)
{
    if (_callState.direction == data::ToxCallState::Direction::Undefined
        && _callState.callState == data::ToxCallState::CallState::Undefined)
    {
        setDiverterToDefaultState();
        _diverterPhoneNumber.clear();
    }
}

void ToxPhoneApplication::fillPhoneDiverter(data::DiverterInfo& diverterInfo)
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

void ToxPhoneApplication::updateConfigDiverterInfo()
{
    if (!toxConfig().isActive())
        return;

    data::DiverterInfo diverterInfo;
    fillPhoneDiverter(diverterInfo);
    Message::Ptr m = createMessage(diverterInfo);
    toxConfig().send(m);
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

void ToxPhoneApplication::setDiverterToDefaultState()
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

void ToxPhoneApplication::phoneDiverterAttached()
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
    { //Block for alog::Line
        alog::Line logLine = log_debug_m << "Phone key is pressed: ";
        if (val == 0x0b)
            logLine << "*";
        else if (val == 0x0c)
            logLine << "#";
        else
            logLine << val;
        logLine << ". Diverter use state: "
                << (diverterIsActive() ? "ON" : "OFF");
    }
    if (!diverterIsActive())
        return;

    bool canToCall = _callState.direction == data::ToxCallState::Direction::Undefined
                     && _callState.callState == data::ToxCallState::CallState::Undefined;
    if (!canToCall)
        return;

    if (val >= 0x00 && val < 0x0a)
    {
        _diverterPhoneNumber.append(QString::number(val));
    }
    else if (val == 0x0b)
    {
        // Нажата '*'
        if (_diverterPhoneNumber.isEmpty())
        {
            if (phoneDiverter().mode() == PhoneDiverter::Mode::Pstn)
            {
                phoneDiverter().switchToUsb();
                updateConfigDiverterInfo();
            }
            if (phoneDiverter().handset() == PhoneDiverter::Handset::On)
                phoneDiverter().startDialTone();
        }
    }
    else if (val == 0x0c)
    {
        // Нажата '#'
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
            _diverterPhoneNumber.clear();
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
            _diverterPhoneNumber.clear();
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
            _diverterPhoneNumber.clear();
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
