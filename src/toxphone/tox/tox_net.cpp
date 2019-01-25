#include "tox_net.h"
#include "tox_func.h"
#include "tox_logger.h"
#include "tox_error.h"

#include "common/defines.h"
#include "common/functions.h"
#include "shared/break_point.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"

#include <chrono>
#include <string>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxNet")

static const char* error_save_tox_state =
    QT_TRANSLATE_NOOP("ToxNet", "An error occurred when saved tox state");

static const char* tox_chat_responce_message =
    QT_TRANSLATE_NOOP("ToxNet", "Hi, it ToxPhone client. The ToxPhone client not support a chat function.");

static const char* tox_transfer_data_message =
    QT_TRANSLATE_NOOP("ToxNet", "Hi, it ToxPhone client. The ToxPhone client not support a transfer of data.");

//--------------------------------- ToxNet -----------------------------------

ToxNet& toxNet()
{
    return ::safe_singleton<ToxNet>();
}

ToxNet::ToxNet() : QThreadEx(0)
{
    _avatarPath = "/var/opt/toxphone/avatars/";
    _configPath = "/var/opt/toxphone/state/";
    _configFile = _configPath + "toxphone.tox";

    chk_connect_d(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ToxNet::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(ToxProfile)
    FUNC_REGISTRATION(RequestFriendship)
    FUNC_REGISTRATION(FriendRequest)
    FUNC_REGISTRATION(RemoveFriend)
    FUNC_REGISTRATION(PhoneFriendInfo)
    FUNC_REGISTRATION(FriendAudioChange)
    FUNC_REGISTRATION(ToxMessage)

    #undef FUNC_REGISTRATION
}

bool ToxNet::init()
{
    QString bootstrapFile;
    config::base().getValue("tox_core.file_bootstrap_nodes", bootstrapFile);
    if (!QFile::exists(bootstrapFile))
    {
        log_error_m << "Bootstrap nodes file " << bootstrapFile << " not exists";
        return false;
    }

    _bootstrapNodes.clear();

    YamlConfig::Func bootstrapFunc = [this](YamlConfig* conf,
                                            YAML::Node& nodes, bool logWarn)
    {
        for (const YAML::Node& node : nodes)
        {
            BootstrapNode bn;
            conf->getValue(node, "name", bn.name, false);
            conf->getValue(node, "address", bn.address, logWarn);
            if (bn.address.isEmpty())
            {
                log_error_m << "Bootstrap nodes load: Empty address";
                continue;
            }

            int port = 0;
            conf->getValue(node, "port", port, logWarn);
            if (port < 1 || port > 65535)
            {
                log_error_m << "Bootstrap nodes load"
                            << ": a port must be in interval 1 - 65535"
                            << ". Assigned value: " << port
                            << ". Node " << bn.address << " will be skipped";
                continue;
            }
            bn.port = port;

            conf->getValue(node, "public_key", bn.publicKey, logWarn);
            if (bn.publicKey.length() < (TOX_PUBLIC_KEY_SIZE * 2))
            {
                log_error_m << "Bootstrap nodes load"
                            << ": a publickey must be not less than " << (TOX_PUBLIC_KEY_SIZE * 2)
                            << ". Node " << bn.address << " will be skipped";
                continue;
            }
            _bootstrapNodes.append(bn);
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
    if (!_savedState.isEmpty())
    {
        _toxOptions.savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
        _updateBootstrapCounter = -200;
    }
    else
        _toxOptions.savedata_type = TOX_SAVEDATA_TYPE_NONE;

    _toxOptions.savedata_data = (const uint8_t*)_savedState.constData();
    _toxOptions.savedata_length = _savedState.length();

    TOX_ERR_NEW tox_err;
    _tox = tox_new(&_toxOptions, &tox_err);

    if (tox_err == TOX_ERR_NEW_PORT_ALLOC)
    {
        if (_toxOptions.ipv6_enabled)
        {
            _toxOptions.ipv6_enabled = false;
            _tox = tox_new(&_toxOptions, &tox_err);
            if (tox_err == TOX_ERR_NEW_OK)
            {
                 log_warn_m << "Tox core creation: failed to start with IPv6, falling back to IPv4"
                            << ". LAN discovery may not work properly.";
            }
        }
        //log_error_m << "Tox core creation: can't to bind the port; errno: " << errno;
        //return false;
    }
    if (tox_err != TOX_ERR_NEW_OK)
    {
         log_error_m << "Failed tox core creation: " << toxError(tox_err);
         return false;
    }
    _savedState.clear();

    if (_toxOptions.savedata_type == TOX_SAVEDATA_TYPE_NONE)
        if (!setUserProfile("toxphone_user", "Toxing on ToxPhone"))
            return false;

    QByteArray selfPubKey = getToxSelfPublicKey(_tox);
    QString avatarFile = _avatarPath + selfPubKey.toHex().toUpper();
    if (QFile::exists(avatarFile))
    {
        QFile file {avatarFile};
        if (file.open(QIODevice::ReadOnly))
        {
            _avatar = file.readAll();
            file.close();
        }
        else
            log_error_m << "Failed open a tox avatar file " << avatarFile;
    }

    // Assign Tox callback
    tox_callback_friend_request          (_tox, tox_friend_request);
    tox_callback_friend_name             (_tox, tox_friend_name);
    tox_callback_friend_status_message   (_tox, tox_friend_status_message);
    tox_callback_friend_message          (_tox, tox_friend_message);
    tox_callback_self_connection_status  (_tox, tox_self_connection_status);
    tox_callback_friend_connection_status(_tox, tox_friend_connection_status);
    tox_callback_file_recv_control       (_tox, tox_file_recv_control);
    tox_callback_file_recv               (_tox, tox_file_recv);
    tox_callback_file_recv_chunk         (_tox, tox_file_recv_chunk);
    tox_callback_file_chunk_request      (_tox, tox_file_chunk_request);
    tox_callback_friend_lossless_packet  (_tox, tox_friend_lossless_packet);

    return true;
}

void ToxNet::deinit()
{
    if (_tox)
    {
        tox_kill(_tox);
        _tox = 0;
    }
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
    QString configTmp = _configPath + ".tox";

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

bool ToxNet::saveAvatar(const QByteArray& avatar, const QString& avatarFile)
{
    if (avatar.isEmpty())
    {
        if (QFile::exists(avatarFile))
            if (!QFile::remove(avatarFile))
            {
                log_error_m << "Failed remove tox avatar file " << avatarFile;
                return false;
            }
        return true;
    }

    qsrand(std::time(0));
    QString avatarFileTmp = avatarFile + ".tmp" + QString::number(qrand());

    QFile file {avatarFileTmp};
    if (!file.open(QIODevice::WriteOnly))
    {
        log_error_m << "Failed open a temporary file " << avatarFileTmp << " to save tox avatar";
        file.remove();
        return false;
    }
    int writeRes = file.write(avatar);
    file.close();
    if (writeRes == -1)
    {
        log_error_m << "Failed write tox avatar to temporary file " << avatarFileTmp;
        file.remove();
        return false;
    }
    if (QFile::exists(avatarFile))
        if (!QFile::remove(avatarFile))
        {
            log_error_m << "Failed remove tox avatar file " << avatarFile;
            file.remove();
            return false;
        }

    if (!file.rename(avatarFile))
    {
        log_error_m << "Failed rename temporary tox avatar file " << avatarFileTmp
                    << " to " << avatarFile;
        return false;
    }
    return true;
}

QByteArray ToxNet::avatarHash(const QString& avatarFile)
{
    QFile file {avatarFile};
    if (!file.exists())
        return QByteArray();

    if (!file.open(QIODevice::ReadOnly))
    {
        log_error_m << "Failed open a tox avatar file " << avatarFile;
        return QByteArray();
    }
    QByteArray data = file.readAll();
    file.close();

    uint8_t avatarHash[TOX_HASH_LENGTH];
    tox_hash(avatarHash, (uint8_t*)data.data(), data.size());

    return QByteArray((char*)avatarHash, TOX_HASH_LENGTH);
}

void ToxNet::sendAvatar(uint32_t friendNumber)
{
    static_assert(TOX_HASH_LENGTH <= TOX_FILE_ID_LENGTH,
                  "TOX_HASH_LENGTH > TOX_FILE_ID_LENGTH!");

    if (_avatar.isEmpty())
    {
        tox_file_send(_tox, friendNumber, TOX_FILE_KIND_AVATAR, 0, 0, 0, 0, 0);
        return;
    }

    uint8_t avatarHash[TOX_HASH_LENGTH];
    tox_hash(avatarHash, (uint8_t*)_avatar.data(), _avatar.size());

    TOX_ERR_FILE_SEND err;
    uint32_t fileNumber = tox_file_send(_tox, friendNumber, TOX_FILE_KIND_AVATAR,
                                        _avatar.size(), avatarHash, 0, 0, &err);
    if (err != TOX_ERR_FILE_SEND_OK)
    {
        log_error_m << "Failed tox_file_send: " << toxError(err);
        return;
    }

    TransferData* td = _sendAvatars.add();
    td->friendNumber = friendNumber;
    td->fileNumber = fileNumber;
    td->size = _avatar.size();
    td->data = _avatar;
    _sendAvatars.sort();
}

void ToxNet::stopSendAvatars()
{
    for(TransferData* fd : _sendAvatars)
        tox_file_control(_tox, fd->friendNumber, fd->fileNumber, TOX_FILE_CONTROL_CANCEL, 0);

    _sendAvatars.clear();
}

void ToxNet::broadcastSendAvatars()
{
    if (uint32_t friendCount = tox_self_get_friend_list_size(_tox))
    {
        QVector<uint32_t> ids;
        ids.resize(friendCount);
        tox_self_get_friend_list(_tox, ids.data());

        for (uint32_t i = 0; i < friendCount; ++i)
            sendAvatar(ids[i]);
    }
}

bool ToxNet::setUserProfile(const QString& name, const QString& status)
{
    QByteArray userName = name.toUtf8();
    QByteArray userStatus = status.toUtf8();

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

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
    log_info_m << "Started";

    Message::List messages;
    steady_timer iterationTimer;
    int iterationSleepTime;
    int updateBootstrapAttempt = 0;

    while (true)
    {
        CHECK_THREAD_STOP

        if (tox_self_get_connection_status(_tox) == TOX_CONNECTION_NONE)
        {
            ++_updateBootstrapCounter;
            if (updateBootstrapAttempt > 10)
            {
                updateBootstrapAttempt = 0;
                _updateBootstrapCounter = -1200;
            }
        }
        if (_updateBootstrapCounter > 30)
        {
            updateBootstrap();
            _updateBootstrapCounter = 0;
            ++updateBootstrapAttempt;
        }

        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            tox_iterate(_tox, this);
        }

        if (tox_self_get_connection_status(_tox) != TOX_CONNECTION_NONE)
        {
            _updateBootstrapCounter = 0;
            updateBootstrapAttempt = 0;
        }

        // Параметр iterationSleepTime вычисляется с учетом времени потраченного
        // на выполнение tox_iterate()
        iterationSleepTime = int(tox_iteration_interval(_tox));
        iterationTimer.reset();

        if (_avatarNeedUpdate == 1)
        {
            stopSendAvatars();
        }
        else if (_avatarNeedUpdate == 5)
        {
            broadcastSendAvatars();
            _avatarNeedUpdate = 0;
        }
        if (_avatarNeedUpdate > 0)
            ++_avatarNeedUpdate;

        { //Block for QMutexLocker
            QMutexLocker locker(&_threadLock); (void) locker;
            if (!_messages.empty())
            {
                for (int i = 0; i < _messages.count(); ++i)
                    messages.add(_messages.release(i, lst::CompressList::No));
                _messages.clear();
            }
        }
        while (!messages.empty())
        {
            Message::Ptr m {messages.release(0), false};
            _funcInvoker.call(m);
            if (iterationTimer.elapsed() > iterationSleepTime)
                break;
        }
        if (!messages.empty())
            continue;

        iterationSleepTime -= iterationTimer.elapsed();
        if (iterationSleepTime > 0)
        {
            QMutexLocker locker(&_threadLock); (void) locker;
            if (!_messages.empty())
                continue;
            _threadCond.wait(&_threadLock, iterationSleepTime);
        }
    } // while (true)

    saveState();
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
        if (!commandsPool().commandIsMultiproc(message->command()))
            message->markAsProcessed();

        message->add_ref();
        QMutexLocker locker(&_threadLock); (void) locker;
        _messages.add(message.get());
        _threadCond.wakeAll();
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

        toxProfile.avatar = _avatar;

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

    data::MessageError error;
    if (!setUserProfile(toxProfile.name, toxProfile.status))
        error.code = 2;

    if (error.code == 0)
        if (!saveState())
        {
            error.code = 1;
            error.description = tr(error_save_tox_state);
        }

    if (error.code == 0)
        if (_avatar.toHex().toUpper() != toxProfile.avatar.toHex().toUpper())
        {
            QByteArray selfPubKey = getToxSelfPublicKey(_tox);
            QString avatarFile = _avatarPath + selfPubKey.toHex().toUpper();
            if (saveAvatar(toxProfile.avatar, avatarFile))
            {
                _avatar = toxProfile.avatar;
                _avatarNeedUpdate = 1;
                log_debug_m << "Own avatar updated";
            }
            else
                error.code = 3;
        }

    if (error.code > 1)
    {
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
        QByteArray calculated {CHECKSUM_BYTES, 0};

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
        //uint32_t friendNum = tox_friend_by_public_key(_tox, (uint8_t*) toxPk.constData(), 0);
        uint32_t friendNum = getToxFriendNum(_tox, toxPk);
        if (friendNum != UINT32_MAX)
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
        uint32_t friendNum;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            friendNum = tox_friend_add(_tox, (uint8_t*)friendIdBin.constData(),
                                       (uint8_t*)msg.constData(), msg.length(), 0);
        }
        if (friendNum == UINT32_MAX)
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
        uint32_t friendNum;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            friendNum = tox_friend_add_norequest(_tox, (uint8_t*)pubKey.constData(), 0);
        }
        if (friendNum != UINT32_MAX)
        {
            if (saveState())
            {
                log_verbose_m << "Friend was successfully added to friend list"
                              << ". Friend key: " << publicKey;
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
    config::state().remove("friend_requests." + string(publicKey));
    config::state().save();

    Message::Ptr answer = message->cloneForAnswer();
    if (error.code != 0)
        writeToMessage(error, answer);
    else
        writeToMessage(friendRequest, answer);

    tcp::listener().send(answer);
    updateFriendRequests();
    //updateFriendList();
}

void ToxNet::command_RemoveFriend(const Message::Ptr& message)
{
    data::RemoveFriend removeFriend;
    readFromMessage(message, removeFriend);

    const char* err;
    data::MessageError error;
    QByteArray publicKey = QByteArray::fromHex(removeFriend.publicKey);
    uint32_t friendNum = getToxFriendNum(_tox, publicKey);
    if (friendNum != UINT32_MAX)
    {
        QString fiendName = getToxFriendName(_tox, friendNum);
        bool result;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            result = tox_friend_delete(_tox, friendNum, 0);
        }
        if (result)
        {
            if (saveState())
            {
                config::state().remove("phones." + string(removeFriend.publicKey));
                config::state().save();

                log_verbose_m << "Friend was successfully removed"
                              << ". Friend name/number/key: "
                              << fiendName << "/" << friendNum << "/"
                              << removeFriend.publicKey;
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

void ToxNet::command_PhoneFriendInfo(const Message::Ptr& message)
{
    data::PhoneFriendInfo phoneFriendInfo;
    readFromMessage(message, phoneFriendInfo);

    QByteArray removePubKey;
    quint32 phoneNumber = quint32(-1);

    YamlConfig::Func getPhones = [&](YamlConfig* conf, YAML::Node& phones, bool)
    {
        for(auto  it = phones.begin(); it != phones.end(); ++it)
        {
            QByteArray publicKey = it->first.as<string>("").c_str();
            if (publicKey.isEmpty())
                continue;

            phoneNumber = quint32(-1);
            conf->getValue(it->second, "phone_number", phoneNumber, false);

            if (phoneNumber == phoneFriendInfo.phoneNumber
                && publicKey != phoneFriendInfo.publicKey)
            {
                removePubKey = publicKey;
                break;
            }
        }
        return true;
    };
    config::state().getValue("phones", getPhones);

    if (!removePubKey.isEmpty())
    {
        config::state().remove("phones." + string(removePubKey) + ".phone_number");
        config::state().save();
        QByteArray removePk = QByteArray::fromHex(removePubKey);
        quint32 removeFriendNum = getToxFriendNum(_tox, removePk);

        log_verbose_m << "Remove duplicate phone number " << phoneNumber
                      << " for "<< ToxFriendLog(_tox, removeFriendNum);

        data::FriendItem friendItem;
        if (fillFriendItem(friendItem, removeFriendNum))
        {
            Message::Ptr m = createMessage(friendItem);
            toxConfig().send(m);
        }
    }

    YamlConfig::Func saveFunc = [&phoneFriendInfo](YamlConfig*, YAML::Node& node, bool)
    {
        if (node.IsDefined() && !node.IsMap())
            node = YAML::Node();

        node["friend_name"] = phoneFriendInfo.name.toUtf8().constData();
        if (!phoneFriendInfo.nameAlias.trimmed().isEmpty())
            node["friend_alias"] = phoneFriendInfo.nameAlias.trimmed().toUtf8().constData();
        else
            node.remove("friend_alias");

        if (phoneFriendInfo.phoneNumber != 0)
            node["phone_number"] = phoneFriendInfo.phoneNumber;
        else
            node.remove("phone_number");

        return true;
    };
    config::state().setValue("phones." + string(phoneFriendInfo.publicKey), saveFunc);
    config::state().save();

    log_verbose_m << "Phone number " << phoneFriendInfo.phoneNumber
                  << " assigned to " << ToxFriendLog(_tox, phoneFriendInfo.number);

    Message::Ptr answer = message->cloneForAnswer();
    writeToMessage(phoneFriendInfo, answer);
    tcp::listener().send(answer);

    data::FriendItem friendItem;
    if (!fillFriendItem(friendItem, phoneFriendInfo.number))
        return;

    Message::Ptr m = createMessage(friendItem);
    toxConfig().send(m);
}

void ToxNet::command_FriendAudioChange(const Message::Ptr& message)
{
    data::FriendAudioChange friendAudioChange;
    readFromMessage(message, friendAudioChange);

    if (friendAudioChange.changeFlag == data::FriendAudioChange::ChangeFlag::PersVolumes)
    {
        string confKey = "phones." + string(friendAudioChange.publicKey);
        config::state().setValue(confKey + ".audio_streams.active", bool(friendAudioChange.value));
        config::state().save();

        log_debug_m << "Personal audio stream volumes flag is assigned to "
                    << ((friendAudioChange.value) ? "TRUE" : "FALSE")
                    << " for " << ToxFriendLog(_tox, friendAudioChange.number);

        Message::Ptr answer = message->cloneForAnswer();
        writeToMessage(friendAudioChange, answer);
        tcp::listener().send(answer);

        data::FriendItem friendItem;
        if (!fillFriendItem(friendItem, friendAudioChange.number))
            return;

        Message::Ptr m = createMessage(friendItem);
        toxConfig().send(m);
    }
}

void ToxNet::command_ToxMessage(const Message::Ptr& message)
{
    data::ToxMessage toxMessage;
    readFromMessage(message, toxMessage);

    QString msg = QObject::tr(toxMessage.text);
    QByteArray ba = msg.toUtf8();

    TOX_ERR_FRIEND_SEND_MESSAGE error;
    tox_friend_send_message(_tox, toxMessage.friendNumber, TOX_MESSAGE_TYPE_NORMAL,
                            (uint8_t*)ba.constData(), ba.length(), &error);
    if (error != TOX_ERR_FRIEND_SEND_MESSAGE_OK)
        log_error_m << "Failed send info message: '" << toxMessage.text << "'";
}

void ToxNet::updateFriendList()
{
    if (!toxConfig().isActive())
        return;

    data::FriendList friendList;
    if (uint32_t friendCount = tox_self_get_friend_list_size(_tox))
    {
        QVector<uint32_t> ids;
        ids.resize(friendCount);
        tox_self_get_friend_list(_tox, ids.data());

        for (uint32_t i = 0; i < friendCount; ++i)
        {
            data::FriendItem friendItem;
            if (!fillFriendItem(friendItem, ids[i]))
                continue;

            friendList.list.append(friendItem);
        }
    }
    Message::Ptr m = createMessage(friendList);
    toxConfig().send(m);
}

void ToxNet::updateFriendRequests()
{
    if (!toxConfig().isActive())
        return;

    data::FriendRequests friendRequests;
    YamlConfig::Func loadFunc =
        [&friendRequests](YamlConfig* conf, YAML::Node& friend_requests, bool logWarn)
    {
        if (!friend_requests.IsMap())
            return false;

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

            data::FriendRequest friendRequest;
            friendRequest.publicKey = publicKey.toLatin1();
            conf->getValue(it->second, "message",   friendRequest.message,  logWarn);
            conf->getValue(it->second, "date_time", friendRequest.dateTime, logWarn);

            friendRequests.list.append(friendRequest);
        }
        return true;
    };
    config::state().reRead();
    config::state().getValue("friend_requests", loadFunc);

    Message::Ptr m = createMessage(friendRequests);
    toxConfig().send(m);
}

void ToxNet::updateDhtStatus()
{
    if (toxConfig().isActive())
    {
        data::DhtConnectStatus dhtConnectStatus;
        dhtConnectStatus.active = _dhtConnected;

        Message::Ptr m = createMessage(dhtConnectStatus);
        toxConfig().send(m);
    }
}

bool ToxNet::fillFriendItem(data::FriendItem& item, uint32_t friendNumber)
{
    QByteArray friendPk = getToxFriendKey(_tox, friendNumber);
    if (friendPk.isEmpty())
        return false;

    item.publicKey = friendPk.toHex().toUpper();
    item.number = friendNumber;

    item.name = getToxFriendName(_tox, friendNumber);
    if (item.name.isEmpty())
        item.name = item.publicKey;

    item.statusMessage = getToxFriendStatusMsg(_tox, friendNumber);

    { //Block for ToxGlobalLock
        ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
        TOX_CONNECTION connection_status =
            tox_friend_get_connection_status(_tox, friendNumber, 0);
        item.isConnecnted = (connection_status != TOX_CONNECTION_NONE);
    }

    QString avatarFile = _avatarPath + item.publicKey;
    QFile file {avatarFile};
    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            item.avatar = file.readAll();
            file.close();
        }
        else
            log_error_m << "Failed open a avatar file " << avatarFile;
    }

    YamlConfig::Func loadFunc = [&item](YamlConfig* conf, YAML::Node& node, bool)
    {
        conf->getValue(node, "friend_alias", item.nameAlias, false);
        conf->getValue(node, "phone_number", item.phoneNumber, false);
        conf->getValue(node, "audio_streams.active", item.personalVolumes, false);
        conf->getValue(node, "echo_cancel", item.echoCancel, false);
        return true;
    };
    string confKey = "phones." + string(item.publicKey);
    config::state().getValue(confKey, loadFunc, false);
    return true;
}

ToxNet::BootstrapNode::~BootstrapNode()
{
    // Реализация деструктора нужна для подавления inline warning-ов
}

//------------------------------ Tox callback --------------------------------

void ToxNet::tox_friend_request(Tox* tox, const uint8_t* pub_key,
                                const uint8_t* msg, size_t length, void* user_data)
{
    QByteArray ba = QByteArray::fromRawData((char*)pub_key, TOX_PUBLIC_KEY_SIZE);
    string public_key {ba.toHex().toUpper().constData()};
    string message {(char*)msg, length};
    string date_time = QDateTime::currentDateTime().toString("yyyy.MM.dd hh.mm.ss").toStdString();

    YamlConfig::Func saveFunc = [&](YamlConfig*, YAML::Node& node, bool)
    {
        if (node.IsDefined() && !node.IsMap())
            node = YAML::Node();

        node["message"] = message;
        node["date_time"] = date_time;
        return true;
    };
    config::state().setValue("friend_requests." + public_key, saveFunc);
    { //Block for YamlConfigLock
        YamlConfigLock lock(config::state()); (void) lock;
        YAML::Node node = config::state().getNode("friend_requests");
        node.SetStyle(YAML::EmitterStyle::Block);
    }
    config::state().save();

    ToxNet* tn = static_cast<ToxNet*>(user_data);
    tn->updateFriendRequests();
}

void ToxNet::tox_friend_name(Tox* tox, uint32_t friend_number,
                             const uint8_t* name, size_t length, void* user_data)
{
    log_debug_m << "ToxEvent: friend name changed. "
                << ToxFriendLog(tox, friend_number);

    if (toxConfig().isActive())
    {
        ToxNet* tn = static_cast<ToxNet*>(user_data);
        data::FriendItem friendItem;
        if (!tn->fillFriendItem(friendItem, friend_number))
            return;

        friendItem.changeFlag = data::FriendItem::ChangeFlag::Name;
        friendItem.name = QString::fromUtf8(QByteArray::fromRawData((char*)name, length));

        Message::Ptr m = createMessage(friendItem);
        toxConfig().send(m);
    }
}

void ToxNet::tox_friend_status_message(Tox* tox, uint32_t friend_number,
                                       const uint8_t* message, size_t length, void* user_data)
{
    log_debug_m << "ToxEvent: friend status message changed. "
                << ToxFriendLog(tox, friend_number);

    if (toxConfig().isActive())
    {
        ToxNet* tn = static_cast<ToxNet*>(user_data);
        data::FriendItem friendItem;
        if (!tn->fillFriendItem(friendItem, friend_number))
            return;

        friendItem.changeFlag = data::FriendItem::ChangeFlag::StatusMessage;
        friendItem.statusMessage = QString::fromUtf8((char*)message, length);

        Message::Ptr m = createMessage(friendItem);
        toxConfig().send(m);
    }
}

void ToxNet::tox_friend_message(Tox* tox, uint32_t friend_number, TOX_MESSAGE_TYPE type,
                                const uint8_t* message, size_t length, void* user_data)
{
    if (type == TOX_MESSAGE_TYPE_NORMAL)
    {
        data::ToxMessage toxMessage;
        toxMessage.friendNumber = friend_number;
        toxMessage.text = QByteArray(tox_chat_responce_message);

        ToxNet* tn = static_cast<ToxNet*>(user_data);
        Message::Ptr m = createMessage(toxMessage);
        tn->message(m);
    }
}

void ToxNet::tox_self_connection_status(Tox* tox, TOX_CONNECTION connection_status,
                                        void* user_data)
{
    if (connection_status == TOX_CONNECTION_TCP)
        log_verbose_m << "Connected to DHT through TCP";
    else if (connection_status == TOX_CONNECTION_UDP)
        log_verbose_m << "Connected to DHT through UDP";
    else
        log_verbose_m << "Disconnected from DHT";

    ToxNet* tn = static_cast<ToxNet*>(user_data);
    if (connection_status != TOX_CONNECTION_NONE)
        log_debug_m << "Update bootstrap counter: " << tn->_updateBootstrapCounter;

    tn->setDhtConnectStatus(connection_status != TOX_CONNECTION_NONE);
    tn->updateDhtStatus();
}

void ToxNet::tox_friend_connection_status(Tox* tox, uint32_t friend_number,
                                          TOX_CONNECTION connection_status, void* user_data)
{
    ToxNet* tn = static_cast<ToxNet*>(user_data);

    const char* stat;
    switch (connection_status)
    {
        case TOX_CONNECTION_TCP:
            stat = "TCP connection. ";
            break;

        case TOX_CONNECTION_UDP:
            stat = "UDP connection. ";
            break;

        default:
            stat = "disonnected. ";
    }
    log_debug_m << "ToxEvent: friend " << stat
                << ToxFriendLog(tox, friend_number);

    if (connection_status != TOX_CONNECTION_NONE
        && tn->_connectionStatusSet.find(friend_number) == tn->_connectionStatusSet.end())
    {
        tn->_connectionStatusSet.insert(friend_number);
        tn->sendAvatar(friend_number);
    }
    if (connection_status == TOX_CONNECTION_NONE)
    {
        tn->_connectionStatusSet.erase(friend_number);
        auto remove = [friend_number](TransferData* td) -> bool
        {
            return (td->friendNumber == friend_number);
        };
        tn->_sendAvatars.removeCond(remove);
        tn->_recvAvatars.removeCond(remove);
    }

    if (toxConfig().isActive())
    {
        data::FriendItem friendItem;
        if (!tn->fillFriendItem(friendItem, friend_number))
            return;

        friendItem.changeFlag = data::FriendItem::ChangeFlag::IsConnecnted;
        friendItem.isConnecnted = (connection_status != TOX_CONNECTION_NONE);

        Message::Ptr m = createMessage(friendItem);
        toxConfig().send(m);
    }
}

void ToxNet::tox_file_recv_control(Tox* tox, uint32_t friend_number, uint32_t file_number,
                                   TOX_FILE_CONTROL control, void* user_data)
{
    if (control == TOX_FILE_CONTROL_CANCEL)
    {
        ToxNet* tn = static_cast<ToxNet*>(user_data);
        if (lst::FindResult fr = tn->_recvAvatars.findRef(TransferData{friend_number, file_number}))
            tn->_recvAvatars.remove(fr.index());
    }
    else if (control == TOX_FILE_CONTROL_RESUME)
    {
        //break_point
    }
}

void ToxNet::tox_file_recv(Tox *tox, uint32_t friend_number, uint32_t file_number,
                           uint32_t kind, uint64_t file_size, const uint8_t *filename,
                           size_t filename_length, void* user_data)
{
    ToxNet* tn = static_cast<ToxNet*>(user_data);
    if (kind == TOX_FILE_KIND_AVATAR)
    {
        QByteArray friendPk = getToxFriendKey(tox, friend_number).toHex().toUpper();
        QString avatarFile = tn->_avatarPath + friendPk;

        // Обновляем аватар
        if ((file_size != 0) && QFile::exists(avatarFile))
        {
            uint8_t fileId[TOX_HASH_LENGTH];
            tox_file_get_file_id(tox, friend_number, file_number, fileId, 0);

            QByteArray hash1 = tn->avatarHash(avatarFile).toHex().toUpper();
            QByteArray hash2 = QByteArray::fromRawData((char*)fileId, TOX_HASH_LENGTH)
                                                       .toHex().toUpper();
            if (!hash1.isEmpty() && (hash1 == hash2))
            {
                // Хеши аватаров совпадают, ничего не делаем
                log_debug_m << "Hashes of avatar is equal. "
                            << ToxFriendLog(tox, friend_number);
            }
            else
            {
                // Закачиваем аватар
                TransferData* td = tn->_recvAvatars.add();
                td->friendNumber = friend_number;
                td->fileNumber = file_number;
                td->size = file_size;
                tn->_recvAvatars.sort();

                tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_RESUME, 0);
                return;
            }
        }

        // Закачиваем аватар
        else if ((file_size != 0) && !QFile::exists(avatarFile))
        {
            TransferData* td = tn->_recvAvatars.add();
            td->friendNumber = friend_number;
            td->fileNumber = file_number;
            td->size = file_size;
            tn->_recvAvatars.sort();

            tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_RESUME, 0);
            return;
        }

        // Удаляем аватар
        else if ((file_size == 0) && QFile::exists(avatarFile))
        {
            if (tn->saveAvatar(QByteArray(), avatarFile))
                log_debug_m << "Avatar deleted. " << ToxFriendLog(tox, friend_number);

            if (toxConfig().isActive())
            {
                data::FriendItem friendItem;
                if (tn->fillFriendItem(friendItem, friend_number))
                {
                    Message::Ptr m = createMessage(friendItem);
                    toxConfig().send(m);
                }
            }
        }
    }
    else
    {
        data::ToxMessage toxMessage;
        toxMessage.friendNumber = friend_number;
        toxMessage.text = QByteArray(tox_transfer_data_message);

        Message::Ptr m = createMessage(toxMessage);
        tn->message(m);
    }

    // По умолчанию отменяем передачу
    tox_file_control(tox, friend_number, file_number, TOX_FILE_CONTROL_CANCEL, 0);
}

void ToxNet::tox_file_recv_chunk(Tox* tox, uint32_t friend_number, uint32_t file_number,
                                 uint64_t position, const uint8_t* data, size_t length,
                                 void* user_data)
{
    ToxNet* tn = static_cast<ToxNet*>(user_data);
    if (lst::FindResult fr = tn->_recvAvatars.findRef(TransferData{friend_number, file_number}))
    {
        if (length == 0)
        {
            tn->_recvAvatars.remove(fr.index());
            return;
        }

        TransferData* td = tn->_recvAvatars.item(fr.index());
        td->data.append(QByteArray::fromRawData((char*)data, length));
        if (td->data.size() == int(td->size))
        {
            QByteArray friendPk = getToxFriendKey(tox, friend_number).toHex().toUpper();
            QString avatarFile = tn->_avatarPath + friendPk;

            if (tn->saveAvatar(td->data, avatarFile))
                log_debug_m << "Avatar updated. " << ToxFriendLog(tox, friend_number);

            tn->_recvAvatars.remove(fr.index());

            if (toxConfig().isActive())
            {
                data::FriendItem friendItem;
                if (tn->fillFriendItem(friendItem, friend_number))
                {
                    Message::Ptr m = createMessage(friendItem);
                    toxConfig().send(m);
                }
            }
        }
    }
}

void ToxNet::tox_file_chunk_request(Tox* tox, uint32_t friend_number, uint32_t file_number,
                                    uint64_t position, size_t length, void* user_data)
{
    ToxNet* tn = static_cast<ToxNet*>(user_data);
    if (lst::FindResult fr = tn->_sendAvatars.findRef(TransferData{friend_number, file_number}))
    {
        if (length == 0)
        {
            tn->_sendAvatars.remove(fr.index());
            return;
        }

        TOX_ERR_FILE_SEND_CHUNK err;
        if (int(position + length) > tn->_avatar.size())
        {
            tox_file_send_chunk(tox, friend_number, file_number, position, 0, 0, &err);
            if (err != TOX_ERR_FILE_SEND_CHUNK_OK)
                log_error_m << "Failed tox_file_send_chunk: " << toxError(err);
            return;
        }

        TransferData* td = tn->_sendAvatars.item(fr.index());
        const char* data = td->data.constData() + position;

        tox_file_send_chunk(tox, friend_number, file_number,
                            position, (uint8_t*)data, length, &err);
        if (err != TOX_ERR_FILE_SEND_CHUNK_OK)
            log_error_m << "Failed tox_file_send_chunk: " << toxError(err);
    }
}

void ToxNet::tox_friend_lossless_packet(Tox* tox, uint32_t friend_number,
                                        const uint8_t* data, size_t length, void* user_data)
{
    ToxNet* tn = static_cast<ToxNet*>(user_data);
    communication::Message::Ptr message = readToxMessage(tox, friend_number, data, length);
    if (message)
        emit tn->internalMessage(message);
}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
