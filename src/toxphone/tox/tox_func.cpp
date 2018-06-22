#include "tox_func.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxFunc")

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


QByteArray getToxPublicKey(Tox* tox)
{
    QByteArray selfPk;
    selfPk.resize(TOX_PUBLIC_KEY_SIZE);

    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;

    tox_self_get_public_key(tox, (uint8_t*)selfPk.constData());
    return selfPk;
}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
