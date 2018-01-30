#include "tox_net.h"
#include "toxphone_defs.h"

#include "shared/break_point.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/base.h"
#include "shared/qt/communication/transport/tcp.h"
#include "kernel/communication/commands.h"

#include <chrono>
#include <string>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")

const char* error_save_tox_state =
    QT_TRANSLATE_NOOP("ToxNet", "An error occurred when saved tox state");

const char* tox_chat_responce_message =
    QT_TRANSLATE_NOOP("ToxNet", "Hi, it ToxPhone client. The ToxPhone client not support a chat function.");

void tox_friend_request(Tox* /*tox*/, const uint8_t* pub_key,
                        const uint8_t* msg, size_t length, void* tox_net)
{
    QByteArray pk = QByteArray::fromRawData((char*)pub_key, TOX_PUBLIC_KEY_SIZE);
    string public_key {pk.toHex().toUpper().constData()};
    string message {(char*)msg, length};
    string date_time = QDateTime::currentDateTime().toString("yyyy.MM.dd hh.mm.ss").toStdString();

    YamlConfig::Func saveFunc = [&](YAML::Node& node, bool)
    {
        node["message"] = message;
        node["date_time"] = date_time;
        return true;
    };
    config::state().setValue("friend_requests." + public_key, saveFunc);
    YAML::Node node = config::state().getNode("friend_requests");
    node.SetStyle(YAML::EmitterStyle::Block);
    config::state().save();

    ToxNet* tn = (ToxNet*)tox_net;
    tn->updateFriendRequests();
}

void tox_friend_name(Tox* tox, uint32_t friend_number,
                     const uint8_t* name, size_t length, void* /*tox_net*/)
{
    QByteArray friendPk;
    friendPk.resize(TOX_PUBLIC_KEY_SIZE);
    if (!tox_friend_get_public_key(tox, friend_number, (uint8_t*)friendPk.constData(), 0))
    {
        log_error_m << "Failed get friend key by friend number: " << friend_number;
        return;
    }

    data::FriendItem friendItem;
    friendItem.changeFlaf = data::FriendItem::ChangeFlaf::Name;
    friendItem.name = QString::fromUtf8(QByteArray::fromRawData((char*)name, length));
    friendItem.publicKey = friendPk.toHex().toUpper();

    log_debug_m << "ToxEvent: friend name changed"
                << ". Friend public key/name: "
                << friendItem.publicKey
                << "/" << friendItem.name;

    Message::Ptr m = createMessage(friendItem, Message::Type::Event);
    tcp::listener().send(m);
}

void tox_friend_status_message(Tox* tox, uint32_t friend_number,
                               const uint8_t* message, size_t length, void* tox_net)
{
    QByteArray friendPk;
    friendPk.resize(TOX_PUBLIC_KEY_SIZE);
    if (!tox_friend_get_public_key(tox, friend_number, (uint8_t*)friendPk.constData(), 0))
    {
        log_error_m << "Failed get friend key by friend number: " << friend_number;
        return;
    }

    data::FriendItem friendItem;
    friendItem.changeFlaf = data::FriendItem::ChangeFlaf::StatusMessage;
    friendItem.statusMessage = QString::fromUtf8(QByteArray::fromRawData((char*)message, length));
    friendItem.publicKey = friendPk.toHex().toUpper();

    if (alog::logger().level() >= alog::Level::Debug)
    {
        ToxNet* tn = (ToxNet*)tox_net;
        log_debug_m << "ToxEvent: friend status message changed"
                    << ". Friend public key/name: "
                    << friendItem.publicKey
                    << "/" << tn->getToxFriendName(friend_number);
    }
    Message::Ptr m = createMessage(friendItem, Message::Type::Event);
    tcp::listener().send(m);
}

void tox_friend_message(Tox* tox, uint32_t friend_number, TOX_MESSAGE_TYPE type,
                        const uint8_t* /*message*/, size_t /*length*/, void* /*user_data*/)
{
    if (type == TOX_MESSAGE_TYPE_NORMAL)
    {
        QString msg = QObject::tr(tox_chat_responce_message);
        QByteArray message = msg.toUtf8();

        TOX_ERR_FRIEND_SEND_MESSAGE error;
        tox_friend_send_message(tox, friend_number, TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t*)message.constData(), message.length(), &error);
        if (error != TOX_ERR_FRIEND_SEND_MESSAGE_OK)
            log_error_m << "Failed send info message: '" << tox_chat_responce_message << "'";
    }
}

void tox_self_connection_status(Tox* /*tox*/, TOX_CONNECTION connection_status, void* tox_net)
{
    if (connection_status != TOX_CONNECTION_NONE)
        log_verbose_m << "Connected to the DHT";
    else
        log_verbose_m << "Disconnected from the DHT";

    ToxNet* tn = (ToxNet*)tox_net;
    tn->setDhtConnectStatus(connection_status != TOX_CONNECTION_NONE);
    tn->updateDhtStatus();
}

void tox_friend_connection_status(Tox* tox, uint32_t friend_number,
                                  TOX_CONNECTION connection_status, void* tox_net)
{
    QByteArray friendPk;
    friendPk.resize(TOX_PUBLIC_KEY_SIZE);
    if (!tox_friend_get_public_key(tox, friend_number, (uint8_t*)friendPk.constData(), 0))
    {
        log_error_m << "Failed get friend key by friend number: " << friend_number;
        return;
    }

    data::FriendItem friendItem;
    friendItem.changeFlaf = data::FriendItem::ChangeFlaf::IsConnecnted;
    friendItem.isConnecnted = (connection_status != TOX_CONNECTION_NONE);
    friendItem.publicKey = friendPk.toHex().toUpper();

    if (alog::logger().level() >= alog::Level::Debug)
    {
        ToxNet* tn = (ToxNet*)tox_net;
        const char* stat = friendItem.isConnecnted ? "connected" : "disonnected";
        log_debug_m << "ToxEvent: friend " << stat
                    << ". Friend public key/name: "
                    << friendItem.publicKey
                    << "/" << tn->getToxFriendName(friend_number);
    }
    Message::Ptr m = createMessage(friendItem, Message::Type::Event);
    tcp::listener().send(m);


}


//--------------------------------- ToxNet -----------------------------------

ToxNet& toxNet()
{
    return ::safe_singleton<ToxNet>();
}

ToxNet::ToxNet() : QThreadEx(0)
{
    _configPath = "/var/opt/ToxPhone/state";
    _configFile = _configPath + "/toxphone.tox";

    chk_connect_d(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ToxNet::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(ToxProfile)
    FUNC_REGISTRATION(RequestFriendship)
    FUNC_REGISTRATION(FriendRequest)
    FUNC_REGISTRATION(RemoveFriend)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

bool ToxNet::init()
{
    _runInit = -1;

    QString bootstrapFile;
    config::base().getValue("tox_core.file_bootstrap_nodes", bootstrapFile);
    if (!QFile::exists(bootstrapFile))
    {
        log_error_m << "Bootstrap nodes file " << bootstrapFile << " not exists";
        return false;
    }

    _bootstrapNodes.clear();

    YamlConfig::Func bootstrapFunc = [this](YAML::Node& nodes, bool)
    {
        for (const YAML::Node& node : nodes)
        {
            BootstrapNode bn;
            bn.address = QString::fromStdString(node["address"].as<string>());

            int port = node["port"].as<int>(0);
            if (port < 1 || port > 65535)
            {
                log_error_m << "Bootstrap nodes load"
                            << ": a port must be in interval 1 - 65535"
                            << ". Assigned value: " << port
                            << ". Node " << bn.address << " will be skipped";
                continue;
            }
            bn.port = port;

            bn.publicKey = QString::fromStdString(node["public_key"].as<string>());
            if (bn.publicKey.length() < (TOX_PUBLIC_KEY_SIZE * 2))
            {
                log_error_m << "Bootstrap nodes load"
                            << ": a publickey must be not less than " << (TOX_PUBLIC_KEY_SIZE * 2)
                            << ". Node " << bn.address << " will be skipped";
                continue;
            }
            bn.name = QString::fromStdString(node["name"].as<string>());

            this->_bootstrapNodes.append(bn);
        }
        return true;
    };
    YamlConfig bootstrapConfig;
    bootstrapConfig.read(bootstrapFile.toStdString());
    bootstrapConfig.getValue("bootstrap_nodes", bootstrapFunc);

    if (_bootstrapNodes.count() == 0)
    {
        log_error_m << "Bootstrap nodes list is empty";
        return false;
    }

    tox_options_default(&_toxOptions);

    config::base().getValue("tox_core.options.ipv6_enabled", _toxOptions.ipv6_enabled);
    if (_toxOptions.ipv6_enabled)
        log_verbose_m << "Core starting with IPv6 enabled";
    else
        log_warn_m << "Core starting with IPv6 disabled. LAN discovery may not work properly.";

    config::base().getValue("tox_core.options.udp_enabled", _toxOptions.udp_enabled);
    log_verbose_m << "Core starting with UDP "
                  << (_toxOptions.udp_enabled ? "enabled" : "disabled");

    config::base().getValue("tox_core.options.local_discovery_enabled",
                            _toxOptions.local_discovery_enabled);
    log_verbose_m << "Core starting with local discovery "
                  << (_toxOptions.local_discovery_enabled ? "enabled" : "disabled");

    int start_port = 0;
    config::base().getValue("tox_core.options.start_port", start_port);
    if (start_port < 0 || start_port > 65535)
    {
        log_error_m << "Tox core options"
                    << ": a start_port must be in interval 0 - 65535"
                    << ". Assigned value: " << start_port;
        return false;
    }
    _toxOptions.start_port = start_port;


    int end_port = 0;
    config::base().getValue("tox_core.options.end_port", end_port);
    if (end_port < 0 || end_port > 65535)
    {
        log_error_m << "Tox core options"
                    << ": a end_port must be in interval 0 - 65535"
                    << ". Assigned value: " << end_port;
        return false;
    }
    _toxOptions.end_port = end_port;

    int tcp_port = 0;
    config::base().getValue("tox_core.options.tcp_port", tcp_port);
    if (end_port < 0 || end_port > 65535)
    {
        log_error_m << "Tox core options"
                    << ": a tcp_port must be in interval 0 - 65535"
                    << ". Assigned value: " << tcp_port;
        return false;
    }
    _toxOptions.tcp_port = tcp_port;

    _savedState.clear();
    if (QFile::exists(_configFile))
    {
        QFile file {_configFile};
        if (!file.open(QIODevice::ReadOnly))
        {
            log_error_m << "Failed open a tox state file " << _configFile;
            return false;
        }
        _savedState = file.readAll();
        file.close();
    }
    _toxOptions.savedata_type = (_savedState.isEmpty())
                                ? TOX_SAVEDATA_TYPE_NONE
                                : TOX_SAVEDATA_TYPE_TOX_SAVE;
    _toxOptions.savedata_data = (const uint8_t*)_savedState.constData();
    _toxOptions.savedata_length = _savedState.length();

    return true;
}

bool ToxNet::createToxInstance()
{
    TOX_ERR_NEW tox_err;
    _tox = tox_new(&_toxOptions, &tox_err);

    _savedState.clear();

    switch (tox_err) {
        case TOX_ERR_NEW_OK:
            break;

        case TOX_ERR_NEW_LOAD_BAD_FORMAT:
            log_error_m << "Tox core creation: failed to parse Tox save data";
            return false;

        case TOX_ERR_NEW_PORT_ALLOC:
            if (_toxOptions.ipv6_enabled)
            {
                _toxOptions.ipv6_enabled = false;
                _tox = tox_new(&_toxOptions, &tox_err);
                if (tox_err == TOX_ERR_NEW_OK)
                {
                    log_warn_m << "Tox core creation: failed to start with IPv6, falling back to IPv4"
                               << ". LAN discovery may not work properly.";
                    break;
                }
            }
            log_error_m << "Tox core creation: can't to bind the port; errno: " << errno;
            return false;

        case TOX_ERR_NEW_PROXY_BAD_HOST:
        case TOX_ERR_NEW_PROXY_BAD_PORT:
        case TOX_ERR_NEW_PROXY_BAD_TYPE:
            log_error_m << "Tox core creation: bad proxy; error code: " << tox_err;
            return false;

        case TOX_ERR_NEW_PROXY_NOT_FOUND:
            log_error_m << "Tox core creation: proxy not found";
            return false;

        case TOX_ERR_NEW_LOAD_ENCRYPTED:
            log_error_m << "Tox core creation: attempted to load encrypted Tox save data";
            return false;

        case TOX_ERR_NEW_MALLOC:
            log_error_m << "Tox core creation: memory allocation failed";
            return false;

        case TOX_ERR_NEW_NULL:
            log_error_m << "Tox core creation: a parameter was null";
            return false;

        default:
            log_error_m << "Tox core creation. Unknown error code: " << tox_err;
            return false;
    }
    return true;

}

void ToxNet::assignToxCallback()
{
    tox_callback_friend_request(_tox, tox_friend_request);
    tox_callback_friend_name(_tox, tox_friend_name);
    tox_callback_friend_status_message(_tox, tox_friend_status_message);
    tox_callback_friend_message(_tox, tox_friend_message);
    tox_callback_self_connection_status(_tox, tox_self_connection_status);
    tox_callback_friend_connection_status(_tox, tox_friend_connection_status);


}

void ToxNet::updateBootstrap()
{
    int qsrandStep = 500;
    for (int i = 0; i < 2; ++i)
    {
        qsrand(std::time(0) + qsrandStep * i);
        int j = qrand() % _bootstrapNodes.count();
        const BootstrapNode& bn = _bootstrapNodes[j];
        QByteArray addr = bn.address.toUtf8();
        QByteArray pubKey = QByteArray::fromHex(bn.publicKey.toLatin1());
        log_verbose_m << "Connecting to bootstrap node " << bn.address << " : " << bn.port
                      << "; name: " << bn.name;

        bool res = tox_bootstrap(_tox, addr.constData(), bn.port,
                                 (const uint8_t*)pubKey.constData(), 0);
        if (!res)
            log_verbose_m << "Failed bootstrapping from " << bn.address << " : " << bn.port
                          << "; name: " << bn.name;

        res = tox_add_tcp_relay(_tox, addr.constData(), bn.port,
                                (const uint8_t*)pubKey.constData(), 0);
        if (!res)
            log_verbose_m << "Failed adding TCP relay from " << bn.address << " : " << bn.port
                          << "; name: " << bn.name;
    }
}

bool ToxNet::saveState()
{
    QString configTmp = _configPath + "/.tox";

    size_t size = tox_get_savedata_size(_tox);
    QByteArray data;
    data.resize(size);
    tox_get_savedata(_tox, (uint8_t*)data.constData());

    qsrand(std::time(0));
    configTmp += QString::number(qrand());

    QFile file {configTmp};
    if (!file.open(QIODevice::WriteOnly))
    {
        log_error_m << "Failed open a temporary file " << configTmp << " to save tox state";
        file.remove();
        return false;
    }
    int writeRes = file.write(data);
    file.close();
    if (writeRes == -1)
    {
        log_error_m << "Failed write tox state to temporary file " << configTmp;
        file.remove();
        return false;
    }
    if (QFile::exists(_configFile))
        if (!QFile::remove(_configFile))
        {
            log_error_m << "Failed remove current tox state file " << _configFile;
            file.remove();
            return false;
        }

    if (!file.rename(_configFile))
    {
        log_error_m << "Failed rename temporary tox state file " << configTmp
                    << " to " << _configFile;
        return false;
    }
    return true;
}

bool ToxNet::setUserProfile(const QString& name, const QString& status)
{
    QByteArray userName = name.toUtf8();
    QByteArray userStatus = status.toUtf8();

    if (!tox_self_set_name(_tox, (const uint8_t*)userName.constData(), userName.length(), 0))
    {
        log_error_m << "Failed assign the default user name: " << name;
        return false;
    }
    if (!tox_self_set_status_message(_tox, (const uint8_t*)userStatus.constData(), userStatus.length(), 0))
    {
        log_error_m << "Failed assign the default user status: " << status;
        return false;
    }
    return true;
}

void ToxNet::run()
{
    _runInit = -1;
    log_info_m << "Started";

    int updateBootstrapCounter = 0;

    if (!createToxInstance())
    {
        _runInit = 0;
        goto thread_end;
    }
    assignToxCallback();

    if (_toxOptions.savedata_type == TOX_SAVEDATA_TYPE_NONE)
        if (!setUserProfile("toxphone_user", "Toxing on ToxPhone"))
        {
            _runInit = 0;
            goto thread_end;
        }

    _runInit = 1;
    while (true)
    {
        CHECK_THREAD_STOP

        if (tox_self_get_connection_status(_tox) == TOX_CONNECTION_NONE)
            ++updateBootstrapCounter;

        if (updateBootstrapCounter > 30)
        {
            //this_thread::sleep_for(chrono::milliseconds(500));
            updateBootstrap();
            updateBootstrapCounter = 0;
        }

        tox_iterate(_tox, this);
        int sleepTime = int(tox_iteration_interval(_tox));

        Message::List messages;
        { //Block for SpinLocker
            SpinLocker locker(_messagesLock); (void) locker;
            messages.swap(_messages);
        }
        if (!messages.empty())
        {
            _iterationTimer.reset();
            // Обрабатываем полученные сообщения
            for (Message* m : messages)
                _funcInvoker.call(Message::Ptr{m});
            sleepTime -= _iterationTimer.elapsed();
        }
        if (sleepTime > 0)
            this_thread::sleep_for(chrono::milliseconds(sleepTime));



    } // while (true)

    saveState();

thread_end:

    if (_tox)
        tox_kill(_tox);

    log_info_m << "Stopped";
}

void ToxNet::message(const communication::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (_funcInvoker.containsCommand(message->command()))
    {
        message->add_ref();
        message->markAsProcessed();

        SpinLocker locker(_messagesLock); (void) locker;
        _messages.add(message.get());
    }
}

void ToxNet::command_IncomingConfigConnection(const Message::Ptr& message)
{
    SocketDescriptor socketDescr = SocketDescriptor(message->tag());
    {
        QByteArray data;
        data::ToxProfile toxProfile;

        size_t size = tox_self_get_name_size(_tox);
        data.resize(size);
        tox_self_get_name(_tox, (uint8_t*)data.constData());
        toxProfile.name = QString::fromUtf8(data);

        size = tox_self_get_status_message_size(_tox);
        data.resize(size);
        tox_self_get_status_message(_tox, (uint8_t*)data.constData());
        toxProfile.status = QString::fromUtf8(data);

        data.resize(TOX_ADDRESS_SIZE);
        tox_self_get_address(_tox, (uint8_t*)data.constData());
        toxProfile.toxId = data.toHex().toUpper();

        Message::Ptr m = createMessage(toxProfile);
        m->destinationSocketDescriptors().insert(socketDescr);
        tcp::listener().send(m);
    }
    updateFriendList();
    updateFriendRequests();
    updateDhtStatus();
}

void ToxNet::command_ToxProfile(const Message::Ptr& message)
{
    data::ToxProfile toxProfile;
    readFromMessage(message, toxProfile);

    Message::Ptr answer = message->cloneForAnswer();
    if (setUserProfile(toxProfile.name, toxProfile.status))
    {
        if (!saveState())
        {
            data::MessageError error;
            error.description = tr(error_save_tox_state);
            writeToMessage(error, answer);
        }
    }
    else
    {
        data::MessageError error;
        error.description = tr("An error occurred when saved profile");
        writeToMessage(error, answer);
    }
    tcp::listener().send(answer);
}

void ToxNet::command_RequestFriendship(const Message::Ptr& message)
{
    data::RequestFriendship reqFriendship;
    readFromMessage(message, reqFriendship);

    Message::Ptr answer = message->cloneForAnswer();
    QString friendId = reqFriendship.toxId.trimmed();
    QString friendMessage = reqFriendship.message.trimmed();

    const char* err;
    data::MessageError error;
    QByteArray friendIdBin;

    auto checkError = [&]()
    {
        if (friendId.length() != TOX_ADDRESS_SIZE*2)
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID length, should be %1 symbols");
            log_error_m << QString(err).arg(TOX_ADDRESS_SIZE*2);
            error.description = tr(err).arg(TOX_ADDRESS_SIZE*2);
            error.code = 1;
            return;
        }

        static QRegExp regex {QString("^[A-Fa-f0-9]{%1}$").arg(TOX_ADDRESS_SIZE*2)};
        if (!friendId.contains(regex))
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID format");
            log_error_m << err;
            error.description = tr(err);
            error.code = 1;
            return;
        }

        friendIdBin = QByteArray::fromHex(friendId.toLatin1());
        const int size = TOX_PUBLIC_KEY_SIZE + TOX_NOSPAM_SIZE;
        const int CHECKSUM_BYTES = sizeof(uint16_t);

        QByteArray data = friendIdBin.left(size);
        QByteArray checksum = friendIdBin.right(CHECKSUM_BYTES);
        QByteArray calculated(CHECKSUM_BYTES, 0);

        for (int i = 0; i < size; ++i)
            calculated[i % 2] = calculated[i % 2] ^ data[i];

        if (calculated != checksum)
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Invalid Tox ID checksumm");
            log_error_m << err;
            error.description = tr(err);
            error.code = 1;
            return;
        }
        if (friendMessage.isEmpty())
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "You need to write a message with your request");
            log_error_m << err;
            error.description = tr(err);
            error.code = 1;
            return;
        }
        if (friendMessage.length() > int(tox_max_friend_request_length()))
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Your message is too long! Maximum length %1 symbols");
            log_error_m << QString(err).arg(tox_max_friend_request_length());
            error.description = tr(err).arg(tox_max_friend_request_length());
            error.code = 1;
            return;
        }

        QByteArray toxPk = friendIdBin.left(TOX_PUBLIC_KEY_SIZE);
        uint32_t friendNum = tox_friend_by_public_key(_tox, (uint8_t*) toxPk.constData(), 0);
        if (friendNum != std::numeric_limits<uint32_t>::max())
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Friend is already added");
            log_error_m << err;
            error.description = tr(err);
            error.code = 1;
            return;
        }
    };
    checkError();

    if (error.code == 0)
    {
        QByteArray msg = friendMessage.toUtf8();
        uint32_t friendNum =
            tox_friend_add(_tox, (uint8_t*)friendIdBin.constData(),
                           (uint8_t*)msg.constData(), msg.length(), 0);
        if (friendNum == std::numeric_limits<uint32_t>::max())
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Failed to request friendship. Friend id: %1");
            log_error_m << QString(err).arg(friendId);
            error.description = tr(err).arg(friendId);
            error.code = 1;
        }
        else
        {
            log_verbose_m << "Requested friendship with " << friendId;
            if (!saveState())
            {
                error.description = tr(error_save_tox_state);
                error.code = 2;
            }
        }
    }
    if (error.code != 0)
        writeToMessage(error, answer);

    tcp::listener().send(answer);
}

void ToxNet::command_FriendRequest(const Message::Ptr& message)
{
    data::FriendRequest friendRequest;
    readFromMessage(message, friendRequest);

    const char* err;
    data::MessageError error;
    QByteArray publicKey = friendRequest.publicKey;

    if (friendRequest.accept == 1)
    {
        QByteArray pubKey = QByteArray::fromHex(publicKey);
        uint32_t friendNum = tox_friend_add_norequest(_tox, (uint8_t*)pubKey.constData(), 0);
        if (friendNum != std::numeric_limits<uint32_t>::max())
        {
            if (saveState())
            {
                log_verbose_m << "Friend was successfully added to friend list"
                              << ". Friend public key: " << publicKey;
            }
            else
            {
                error.description = tr(error_save_tox_state);
                error.code = 1;
            }
        }
        else
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Failed add friend");
            log_error_m << err;
            error.description = tr(err);
            error.code = 1;
        }
    }
    config::state().reRead();
    config::state().remove("friend_requests." + string(publicKey));

    // Костыль: если нода friend_requests не содержит элементов, то удаляем
    // саму ноду friend_requests. Если этого не делать, то при добавлении
    // новых элементов будет некорректное форматирование.
    //if (!config::state().getValue("friend_requests", [](YAML::Node& node, bool) {return node.size();}))
    //    config::state().remove("friend_requests");

    config::state().save();

    Message::Ptr answer = message->cloneForAnswer();
    if (error.code != 0)
        writeToMessage(error, answer);
    else
        writeToMessage(friendRequest, answer);

    tcp::listener().send(answer);
    updateFriendRequests();
    updateFriendList();
}

void ToxNet::command_RemoveFriend(const Message::Ptr& message)
{
    data::RemoveFriend removeFriend;
    readFromMessage(message, removeFriend);

    const char* err;
    data::MessageError error;
    QByteArray publicKey = QByteArray::fromHex(removeFriend.publicKey);

    uint32_t friendNum =
        tox_friend_by_public_key(_tox, (uint8_t*)publicKey.constData(), 0);
    if (friendNum != std::numeric_limits<uint32_t>::max())
    {
        if (tox_friend_delete(_tox, friendNum, 0))
        {
            if (saveState())
            {
                log_verbose_m << "Friend was successfully removed"
                              << ". Friend public key/name: "
                              << removeFriend.publicKey
                              << "/" << getToxFriendName(friendNum);
            }
            else
            {
                error.description = tr(error_save_tox_state);
                error.code = 1;
            }
        }
        else
        {
            err = QT_TRANSLATE_NOOP("ToxNet", "Failed remove friend %1");
            log_error_m << QString(err).arg(removeFriend.name);
            error.description = tr(err).arg(removeFriend.name);
            error.code = 1;
        }
    }
    else
    {
        err = QT_TRANSLATE_NOOP("ToxNet", "Friend %1 not found");
        log_error_m << QString(err).arg(removeFriend.name);
        error.description = tr(err).arg(removeFriend.name);
        error.code = 1;
    }

    Message::Ptr answer = message->cloneForAnswer();
    if (error.code != 0)
        writeToMessage(error, answer);

    tcp::listener().send(answer);
    updateFriendList();
}

void ToxNet::updateFriendList()
{
    data::FriendList friendList;
    if (uint32_t friendCount = tox_self_get_friend_list_size(_tox))
    {
        QVector<uint32_t> ids;
        ids.resize(friendCount);
        tox_self_get_friend_list(_tox, ids.data());
        QByteArray friendPk;
        friendPk.resize(TOX_PUBLIC_KEY_SIZE);

        for (uint32_t i = 0; i < friendCount; ++i)
        {
            if (!tox_friend_get_public_key(_tox, ids[i], (uint8_t*)friendPk.constData(), 0))
                continue;

            data::FriendItem friendItem;
            friendItem.publicKey = friendPk.toHex().toUpper();

            QByteArray data;
            size_t size = tox_friend_get_name_size(_tox, ids[i], 0);
            data.resize(size);
            tox_friend_get_name(_tox, ids[i], (uint8_t*)data.constData(), 0);
            friendItem.name = QString::fromUtf8(data);
            if (friendItem.name.isEmpty())
                friendItem.name = friendItem.publicKey;

            size = tox_friend_get_status_message_size(_tox, ids[i], 0);
            data.resize(size);
            tox_friend_get_status_message(_tox, ids[i], (uint8_t*)data.constData(), 0);
            friendItem.statusMessage = QString::fromUtf8(data);

            TOX_CONNECTION connection_status =
                tox_friend_get_connection_status(_tox, ids[i], 0);
            friendItem.isConnecnted = (connection_status != TOX_CONNECTION_NONE);

            friendList.list.append(friendItem);
        }
    }
    Message::Ptr m = createMessage(friendList, Message::Type::Event);
    tcp::listener().send(m);
}

void ToxNet::updateFriendRequests()
{
    data::FriendRequests friendRequests;
    YamlConfig::Func loadFunc = [&friendRequests](YAML::Node& friend_requests, bool)
    {
        for(auto  it = friend_requests.begin(); it != friend_requests.end(); ++it)
        {
            QString publicKey = QString::fromStdString(it->first.as<string>(""));
            if (publicKey.isEmpty())
                continue;

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

            YAML::Node val = it->second;
            data::FriendRequest friendRequest;
            friendRequest.publicKey = publicKey.toLatin1();
            friendRequest.message   = QString::fromUtf8(val["message"].as<string>("").c_str());
            friendRequest.dateTime  = QString::fromUtf8(val["date_time"].as<string>("").c_str());

            friendRequests.list.append(friendRequest);
        }
        return true;
    };
    config::state().reRead();
    config::state().getValue("friend_requests", loadFunc);

    Message::Ptr m = createMessage(friendRequests, Message::Type::Event);
    tcp::listener().send(m);
}

void ToxNet::updateDhtStatus()
{
    data::DhtConnectStatus dhtConnectStatus;
    dhtConnectStatus.active = _dhtConnected;

    Message::Ptr m = createMessage(dhtConnectStatus, Message::Type::Event);
    tcp::listener().send(m);
}

QString ToxNet::getToxFriendName(const QByteArray& publicKey) const
{
    uint32_t friendNum =
        tox_friend_by_public_key(_tox, (uint8_t*)publicKey.constData(), 0);
    return getToxFriendName(friendNum);
}

QString ToxNet::getToxFriendName(uint32_t friendNumber) const
{
    if (friendNumber == std::numeric_limits<uint32_t>::max())
        return QString();

    QByteArray data;
    size_t size = tox_friend_get_name_size(_tox, friendNumber, 0);
    data.resize(size);
    tox_friend_get_name(_tox, friendNumber, (uint8_t*)data.constData(), 0);
    return QString::fromUtf8(data);
}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
