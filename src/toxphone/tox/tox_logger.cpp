#include "tox_logger.h"
#include "tox_func.h"

#include "shared/qt/logger_operators.h"
#include "toxcore/logger.h"
#include <stdarg.h>

extern bool enable_toxcore_log;

/**
  Небольшой хак: переопределяем реализацию функций из ядра toxcore
*/
struct Logger
{
    logger_cb* callback = {nullptr};
    void* context  = {nullptr};
    void* userdata = {nullptr};
};

// Заперт на mangling для имен функций
Logger* logger_new(void) asm ("logger_new");
void logger_kill(Logger* log) asm ("logger_kill");

void logger_callback_log(Logger* log, logger_cb* function, void* context, void* userdata)
    asm ("logger_callback_log");

void logger_write(
    const Logger* log, Logger_Level level, const char* file, int line, const char* func,
    const char* format, ...)
    asm ("logger_write");

Logger* logger_new(void)
{
    return (Logger *)calloc(1, sizeof(Logger));
}

void logger_kill(Logger *log)
{
    free(log);
}

void logger_callback_log(Logger* log, logger_cb* function, void* context, void* userdata)
{
    (void) log;
    (void) function;
    (void) context;
    (void) userdata;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

void logger_write(
    const Logger* log, Logger_Level level, const char* file, int line, const char* func,
    const char* format, ...)
{
    (void) log;
    if (!enable_toxcore_log)
        return;

    va_list args;
    va_start(args, format);

    char buff[1024] = {0};
    switch (level)
    {
        case LOGGER_LEVEL_TRACE:
            if (alog::logger().level() == alog::Level::Debug2)
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().debug2(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOGGER_LEVEL_DEBUG:
            if (alog::logger().level() >= alog::Level::Debug)
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().debug(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOGGER_LEVEL_WARNING:
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().warn(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOGGER_LEVEL_ERROR:
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().error(file, func, line, "ToxCore") << buff;
                alog::logger().flush();
                alog::logger().waitingFlush();
            }
            break;

        default:
            {
                // LOGGER_LEVEL_INFO
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().info(file, func, line, "ToxCore") << buff;
            }
    }
    va_end(args);
}

#pragma GCC diagnostic pop

//----------------------------- ToxFriendLog ---------------------------------

namespace alog {

Line& operator<< (Line& line, const ToxFriendLog& tfl)
{
    if (line.toLogger())
    {
        if (tfl.friendNumber == std::numeric_limits<uint32_t>::max())
            return line;

        QByteArray name;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            size_t size = tox_friend_get_name_size(tfl.tox, tfl.friendNumber, 0);
            name.resize(size);
            tox_friend_get_name(tfl.tox, tfl.friendNumber, (uint8_t*)name.constData(), 0);
        }

        if (!tfl.withoutKey)
        {
            QByteArray friendPk;
            friendPk.resize(TOX_PUBLIC_KEY_SIZE);

            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            if (!tox_friend_get_public_key(tfl.tox, tfl.friendNumber, (uint8_t*)friendPk.constData(), 0))
                friendPk.clear();

            line << log_format("Friend name/number/key: %?/%?/%?", name, tfl.friendNumber, friendPk);
        }
        else
            line << log_format("Friend name/number: %?/%?", name, tfl.friendNumber);
    }
    return line;
}

} // namespace alog
