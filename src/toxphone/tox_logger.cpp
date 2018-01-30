/*
 * The hack: substitution of the original implementation of the logger
 */

#include "../3rdparty/toxcore/toxcore/logger.h"
#include "shared/logger/logger.h"
#include <stdarg.h>

extern bool enable_toxcore_log;

struct Logger {
    logger_cb *callback;
    void *context;
    void *userdata;
};

// Disable mangling the function names
Logger *logger_new(void) asm ("logger_new");
void logger_kill(Logger *log) asm ("logger_kill");
void logger_callback_log(Logger *log, logger_cb *function, void *context, void *userdata)  asm ("logger_callback_log");
void logger_write(Logger* log, LOGGER_LEVEL level,
                  const char* file, int line, const char* func, const char* format, ...) asm ("logger_write");

Logger *logger_new(void)
{
    return 0;
}

void logger_kill(Logger *log)
{
    (void) log;
}

void logger_callback_log(Logger *log, logger_cb *function, void *context, void *userdata)
{
    (void) log;
    (void) function;
    (void) context;
    (void) userdata;
}

void logger_write(Logger* log, LOGGER_LEVEL level,
                  const char* file, int line, const char* func, const char* format, ...)
{
    (void) log;
    if (!enable_toxcore_log)
        return;

    va_list args;
    va_start(args, format);

    char buff[1024] = {0};
    switch (level)
    {
        case LOG_TRACE:
            if (alog::logger().level() == alog::Level::Debug2)
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().debug2_f(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOG_DEBUG:
            if (alog::logger().level() >= alog::Level::Debug)
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().debug_f(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOG_WARNING:
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().warn_f(file, func, line, "ToxCore") << buff;
            }
            break;

        case LOG_ERROR:
            {
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().error_f(file, func, line, "ToxCore") << buff;
            }
            break;

        default:
            {
                // LOG_INFO
                vsnprintf(buff, sizeof(buff) - 1, format, args);
                alog::logger().info_f(file, func, line, "ToxCore") << buff;
            }
    }
    va_end(args);
}


//void tox_logger_write(Logger* /*log*/, LOGGER_LEVEL level,
//                      const char* file, int line, const char* func,
//                      const char* format, va_list args)
//{
//    //break_point

//    char buff[1024] = {0};
//    switch (level)
//    {
//        case LOG_TRACE:
//            if (alog::logger().level() == alog::Level::Debug2)
//            {
//                vsnprintf(buff, sizeof(buff) - 1, format, args);
//                alog::logger().debug2_f(file, func, line, "ToxCore") << buff;
//            }
//            break;

//        case LOG_DEBUG:
//            if (alog::logger().level() >= alog::Level::Debug)
//            {
//                vsnprintf(buff, sizeof(buff) - 1, format, args);
//                alog::logger().debug_f(file, func, line, "ToxCore") << buff;
//            }
//            break;

//        case LOG_WARNING:
//            {
//                vsnprintf(buff, sizeof(buff) - 1, format, args);
//                alog::logger().warn_f(file, func, line, "ToxCore") << buff;
//            }
//            break;

//        case LOG_ERROR:
//            {
//                vsnprintf(buff, sizeof(buff) - 1, format, args);
//                alog::logger().error_f(file, func, line, "ToxCore") << buff;
//            }
//            break;

//        default:
//            {
//                // LOG_INFO
//                vsnprintf(buff, sizeof(buff) - 1, format, args);
//                alog::logger().info_f(file, func, line, "ToxCore") << buff;
//            }
//    }
//}
