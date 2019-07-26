#include "tox_func.h"
#include "tox_error.h"
#include "tox_logger.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")

namespace {
const quint64 toxPhoneMessageSignature = *((quint64*)TOX_MESSAGE_SIGNATURE);
}

QMutex ToxGlobalLock::mutex {QMutex::Recursive};

ToxGlobalLock::ToxGlobalLock()
{
    mutex.lock();
}

ToxGlobalLock::~ToxGlobalLock()
{
    mutex.unlock();
}

QString getToxFriendName(Tox* tox, const QByteArray& publicKey)
{
    if (publicKey.size() != TOX_PUBLIC_KEY_SIZE)
        return "";

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    uint32_t friendNumber =
        tox_friend_by_public_key(tox, (uint8_t*)publicKey.constData(), 0);

    if (friendNumber == UINT32_MAX)
        return "";

    QByteArray data;
    size_t size = tox_friend_get_name_size(tox, friendNumber, 0);
    data.resize(size);
    tox_friend_get_name(tox, friendNumber, (uint8_t*)data.constData(), 0);
    return QString::fromUtf8(data);
}

QString getToxFriendName(Tox* tox, uint32_t friendNumber)
{
    if (friendNumber == std::numeric_limits<uint32_t>::max())
        return QString();

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    QByteArray data;
    size_t size = tox_friend_get_name_size(tox, friendNumber, 0);
    data.resize(size);
    tox_friend_get_name(tox, friendNumber, (uint8_t*)data.constData(), 0);
    return QString::fromUtf8(data);
}

QString getToxFriendStatusMsg(Tox* tox, uint32_t friendNumber)
{
    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    QByteArray data;
    size_t size = tox_friend_get_status_message_size(tox, friendNumber, 0);
    data.resize(size);
    tox_friend_get_status_message(tox, friendNumber, (uint8_t*)data.constData(), 0);
    return QString::fromUtf8(data);
}

QByteArray getToxFriendKey(Tox* tox, uint32_t friendNumber)
{
    QByteArray friendPk;
    friendPk.resize(TOX_PUBLIC_KEY_SIZE);

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    if (!tox_friend_get_public_key(tox, friendNumber, (uint8_t*)friendPk.constData(), 0))
    {
        log_error_m << "Failed get friend key by friend number: " << friendNumber;
        friendPk.clear();
    }
    return friendPk;
}

uint32_t getToxFriendNum(Tox* tox, const QByteArray& publicKey)
{

    if (publicKey.size() != TOX_PUBLIC_KEY_SIZE)
        return UINT32_MAX;

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
    return tox_friend_by_public_key(tox, (uint8_t*)publicKey.constData(), 0);
}


QByteArray getToxSelfPublicKey(Tox* tox)
{
    QByteArray selfPk;
    selfPk.resize(TOX_PUBLIC_KEY_SIZE);

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    tox_self_get_public_key(tox, (uint8_t*)selfPk.constData());
    return selfPk;
}

bool sendToxLosslessMessage(Tox* tox, uint32_t friendNumber,
                            const communication::Message::Ptr& message)
{
    if ((message->size() + sizeof(toxPhoneMessageSignature) + 1) > TOX_MAX_CUSTOM_PACKET_SIZE)
    {
        message->compress();
        if ((message->size() + sizeof(toxPhoneMessageSignature) + 1) > TOX_MAX_CUSTOM_PACKET_SIZE)
        {
            log_error_m << toxError(TOX_ERR_FRIEND_CUSTOM_PACKET_TOO_LONG).description;
            return false;
        }
    }

    QByteArray buff;
    buff.reserve(message->size() + sizeof(toxPhoneMessageSignature) + 1);
    {
        QDataStream stream {&buff, QIODevice::WriteOnly};
        STREAM_INIT(stream)
        quint8 toxCommand = 160; // Идентификатор пользовательской tox-команды
        stream << toxCommand;
        stream << toxPhoneMessageSignature;
        message->toDataStream(stream);
    }

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    TOX_ERR_FRIEND_CUSTOM_PACKET err;
    tox_friend_send_lossless_packet(tox, friendNumber, (uint8_t*)buff.constData(),
                                    buff.length(), &err);
    if (err != TOX_ERR_FRIEND_CUSTOM_PACKET_OK)
    {
        log_error_m << toxError(err).description;
        return false;
    }
    return true;
}

const communication::Message::Ptr readToxMessage(Tox* tox, uint32_t friendNumber,
                                                 const uint8_t* data, size_t length)
{
    QByteArray buff {QByteArray::fromRawData((char*)data, length)};
    QDataStream stream {&buff, QIODevice::ReadOnly | QIODevice::Unbuffered};
    STREAM_INIT(stream)
    quint8 toxCommand; (void) toxCommand;
    quint64 toxPhoneSign;
    stream >> toxCommand;
    stream >> toxPhoneSign;
    if (toxPhoneSign != toxPhoneMessageSignature)
    {
        if (alog::logger().level() == alog::Level::Debug2)
        {
            log_debug2_m << "Raw message incompatible signature, discarded"
                         << ". Message from " << ToxFriendLog(tox, friendNumber);
        }
        return communication::Message::Ptr();
    }
    communication::Message::Ptr message {communication::Message::fromDataStream(stream)};
    message->setAuxiliary(friendNumber);
    return std::move(message);
}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
