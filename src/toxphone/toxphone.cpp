#include "toxphone_appl.h"
#include "shared/defmac.h"
#include "shared/utils.h"
#include "shared/logger/logger.h"
#include "shared/logger/config.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/quuidex.h"
#include "shared/qt/communication/commands_base.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/communication/transport/udp.h"
#include "shared/qt/compression/qlzma.h"
#include "shared/qt/compression/qppmd.h"
#include "shared/qt/config/config.h"
#include "shared/qt/version/version_number.h"
#include "shared/thread/thread_pool.h"
#include "kernel/communication/commands.h"

#if defined(__MINGW32__)
#include <windows.h>
#else
#include <signal.h>
#endif

#if defined(__SSE__) && defined(USE_SIMD)
//#include <immintrin.h>
#include <xmmintrin.h>
#endif

#include <unistd.h>

using namespace std;
using namespace communication;
using namespace communication::transport;

/**
  Используется для уведомления основного потока о завершении работы программы.
*/
#if defined(__MINGW32__)
BOOL WINAPI stopProgramHandler(DWORD sig)
{
    if ((sig == CTRL_C_EVENT) || (sig == CTRL_BREAK_EVENT))
    {
        const char* sigName = (sig == CTRL_C_EVENT) ? "CTRL_C_EVENT" : "CTRL_BREAK_EVENT";
        log_verbose << "Signal " << sigName << " is received. Program will be stopped";
        QCoreApplicationM::stop();
    }
    else
        log_verbose << "Signal " << sig << " is received";
    return TRUE;
}
#else
void stopProgramHandler(int sig)
{
    if ((sig == SIGTERM) || (sig == SIGINT))
    {
        const char* sigName = (sig == SIGTERM) ? "SIGTERM" : "SIGINT";
        log_verbose << "Signal " << sigName << " is received. Program will be stopped";
        ToxPhoneApplication::stop();
    }
    else
        log_verbose << "Signal " << sig << " is received";
}
#endif // #if defined(__MINGW32__)

void stopProgram()
{
    #define STOP_THREAD(THREAD_FUNC, NAME, TIMEOUT) \
        if (!THREAD_FUNC.stop(TIMEOUT * 1000)) { \
            log_info << "Thread '" NAME "': Timeout expired, thread will be terminated."; \
            THREAD_FUNC.terminate(); \
        }

    STOP_THREAD(udp::socket(), "TransportUDP",  15)
    tcp::listener().close();

    log_info << "ToxPhone service is stopped";
    alog::logger().stop();

    trd::threadPool().stop();

    #undef STOP_THREAD
}

void helpInfo(/*const char * binary*/)
{
    alog::logger().clearSavers();
    alog::logger().addSaverStdOut(alog::Level::Info, true);

    log_info << "ToxPhone service"
             << " (version: " << productVersion().toString()
             << "; binary protocol version: "
             << BPROTOCOL_VERSION_LOW << "-" << BPROTOCOL_VERSION_HIGH
             << "; gitrev: " << GIT_REVISION << ")";
    log_info << "Usage: ToxPhone [nh]";
    log_info << "  -n do not daemonize";
    log_info << "  -h this help";
    alog::logger().flush();
}

int main(int argc, char *argv[])
{
    // Устанавливаем в качестве разделителя целой и дробной части символ '.',
    // если этого не сделать - функции преобразования строк в числа (std::atof)
    // буду неправильно работать.
    qputenv("LC_NUMERIC", "C");

    // Пул потоков запустим после кода демонизации
    trd::threadPool().stop();

    int ret = 0;
    try
    {
        alog::logger().start();

#ifdef NDEBUG
        alog::logger().addSaverStdOut(alog::Level::Info, true);
#else
        alog::logger().addSaverStdOut(alog::Level::Debug);
#endif

#if defined(__MINGW32__)
        if (!SetConsoleCtrlHandler(stopProgramHandler, TRUE))
        {
            log_error << "Could not set control handler";
            alog::logger().flush();
            alog::logger().stop();
            return 1;
        }
#else
        signal(SIGTERM, &stopProgramHandler);
        signal(SIGINT,  &stopProgramHandler);
#endif

        QDir homeDir = QDir::home();
        if (!homeDir.exists())
        {
            log_error << "Home dir " << homeDir.path() << " not exists";
            alog::logger().stop();
            return 1;
        }

        bool isDaemon = true;
        int c;
        while ((c = getopt(argc, argv, "nhc:l:")) != EOF)
        {
            switch(c)
            {
                case 'h':
                    helpInfo();
                    alog::logger().stop();
                    exit(0);
                case 'n':
                    isDaemon = false;
                    break;
              case '?':
                    log_error << "Invalid option";
                    alog::logger().stop();
                    return 1;
            }
        }

        QString configFile = "/etc/ToxPhone/toxphone.conf";
        if (!QFile::exists(configFile))
        {
            log_error << "Config file " << configFile << " not exists";
            alog::logger().stop();
            return 1;
        }

        config::base().setReadOnly(true);
        config::base().setSaveDisabled(true);
        if (!config::base().read(configFile.toStdString()))
        {
            alog::logger().stop();
            return 1;
        }

        QString configFileS = "/var/opt/ToxPhone/state/toxphone.state";
        config::state().read(configFileS.toStdString());

        QString logFile;
#if defined(__MINGW32__)
            config::base().getValue("logger.file_win", logFile);
#else
            config::base().getValue("logger.file", logFile);
#endif
        config::homeDirExpansion(logFile);

        QFileInfo logFileInfo {logFile};
        QString logFileDir = logFileInfo.absolutePath();
        if (!QDir(logFileDir).exists())
            if (!QDir().mkpath(logFileDir))
            {
                log_error << "Failed create log directory: " << logFileDir;
                alog::logger().stop();
                return 1;
            }

        // Создаем дефолтный сэйвер для логгера
        {
            std::string logLevelStr = "info";
            config::base().getValue("logger.level", logLevelStr);

            bool logContinue = true;
            config::base().getValue("logger.continue", logContinue);

            alog::Level logLevel = alog::levelFromString(logLevelStr);
            alog::SaverPtr saver {new alog::SaverFile("default",
                                                      logFile.toStdString(),
                                                      logLevel,
                                                      logContinue)};
            alog::logger().addSaver(saver);
        }
        log_info << "ToxPhone service is running"
                 << " (version " << productVersion().toString() << ")";
        alog::logger().flush();

        if (isDaemon)
        {
#if !(defined(_MSC_VER) || defined(__MINGW32__))
            alog::logger().stop();
            alog::logger().removeSaverStdOut();
            alog::logger().removeSaverStdErr();

            if (daemon(1, 0) != 0)
                return 0;

            alog::logger().start();
            log_verbose << "Demonization success";
#endif
        }
        alog::logger().removeSaverStdOut();
        alog::logger().removeSaverStdErr();

        // Создаем дополнительные сэйверы для логгера
        QString logConf;
#if defined(__MINGW32__)
            config::base().getValue("logger.conf_win", logConf);
#else
            config::base().getValue("logger.conf", logConf);
#endif
        if (!logConf.isEmpty())
        {
            config::homeDirExpansion(logConf);
            if (QFile::exists(logConf))
                alog::loadSavers(logConf.toStdString());
            else
                log_error << "Logger config file not exists: " << logConf;
        }
        alog::printSaversInfo();

        if (!communication::commandsPool().checkUnique())
        {
            stopProgram();
            return 1;
        }

        // Пул потоков нужно активировать после кода демонизации
        trd::threadPool().start();

        ToxPhoneApplication appl {argc, argv};

        // Устанавливаем текущую директорию. Эта конструкция работает только
        // когда создан экземпляр QCoreApplication.
        if (QDir::setCurrent(QCoreApplication::applicationDirPath()))
        {
            log_debug << "Set work directory: " << QCoreApplication::applicationDirPath();
        }
        else
        {
            log_error << "Failed set work directory";
            stopProgram();
            return 1;
        }

        // Инициализация communication::listener::tcp
        QHostAddress hostAddress = QHostAddress::Any;
        QString hostAddressStr;
        if (config::base().getValue("connection.address", hostAddressStr))
        {
            if (hostAddressStr.toLower().trimmed() == "localhost")
                hostAddress = QHostAddress::LocalHost;
            else if (hostAddressStr.toLower().trimmed() == "any")
                hostAddress = QHostAddress::Any;
            else
                hostAddress = QHostAddress(hostAddressStr);
        }

        int port = 3609;
        config::base().getValue("connection.port", port);
        if (!tcp::listener().init({hostAddress, port}))
        {
            stopProgram();
            return 1;
        }

        if (!udp::socket().init({QHostAddress::Any, port}))
        {
            stopProgram();
            return 1;
        }
        udp::socket().start();
        udp::socket().waitBinding(3);
        if (!udp::socket().isBound())
        {
            stopProgram();
            return 1;
        }

        if (ToxPhoneApplication::isStopped())
        {
            stopProgram();
            return 0;
        }

        QMetaObject::invokeMethod(&appl, "sendInfo", Qt::QueuedConnection);
        ret = appl.exec();

        communication::data::ApplShutdown applShutdown;
        applShutdown.applId = ToxPhoneApplication::applId();

        Message::Ptr message = createMessage(applShutdown);
        network::Interface::List netInterfaces = network::getInterfaces();
        for (network::Interface* intf : netInterfaces)
            message->destinationPoints().insert({intf->broadcast, port - 1});

        udp::socket().send(message);
        usleep(200*1000);
    }
    catch (std::exception& e)
    {
        log_error << "Failed initialization. Detail: " << e.what();
        ret = 1;
    }
    catch (...)
    {
        log_error << "Failed initialization. Unknown error";
        ret = 1;
    }
    stopProgram();
    return ret;
}
