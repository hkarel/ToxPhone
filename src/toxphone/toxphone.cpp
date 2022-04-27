#include "toxphone_appl.h"
#include "tox/tox_net.h"
#include "tox/tox_call.h"
#include "audio/audio_dev.h"
#include "common/voice_frame.h"
#include "common/voice_filters.h"
#include "diverter/phone_diverter.h"

#include "commands/commands.h"
#include "commands/error.h"

#include "shared/defmac.h"
#include "shared/utils.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/logger/config.h"
#include "shared/config/appl_conf.h"
#include "shared/config/logger_conf.h"
//#include "shared/qt/compression/qlzma.h"
//#include "shared/qt/compression/qppmd.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/version_number.h"
#include "shared/thread/thread_pool.h"

#include "pproto/commands/base.h"
#include "pproto/commands/pool.h"
#include "pproto/transport/tcp.h"
#include "pproto/transport/udp.h"

#if defined(__MINGW32__)
#include <windows.h>
#else
#include <signal.h>
#endif

#if defined(__SSE__) && defined(USE_SIMD)
//#include <immintrin.h>
#include <xmmintrin.h>
#endif

#include <sodium.h>
#include <unistd.h>

using namespace std;
using namespace pproto;
using namespace pproto::transport;

bool enable_toxcore_log = false;

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
        Application::stop();
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

    tcp::listener().close();
    STOP_THREAD(udp::socket(),   "TransportUDP",  15)
    STOP_THREAD(phoneDiverter(), "PhoneDiverter", 15)
    STOP_THREAD(audioDev(),      "AudioDev",      15)
    STOP_THREAD(toxCall(),       "ToxCall",       15)
    STOP_THREAD(toxNet(),        "ToxNet",        15)

    #undef STOP_THREAD

    log_info << "ToxPhone is stopped";
    alog::stop();

    trd::threadPool().stop();
}

void helpInfo(/*const char * binary*/)
{
    alog::logger().clearSavers();
    alog::logger().addSaverStdOut(alog::Level::Info, true);

    log_info << log_format(
        "ToxPhone (version: %?; protocol version: %?-%?; gitrev: %?)",
        productVersion().toString(),
        PPROTO_VERSION_LOW, PPROTO_VERSION_HIGH, GIT_REVISION);
    log_info << "Copyright (c) 2018 Pavel Karelin <hkarel@yandex.ru>";
    log_info << "ToxPhone is used and distributed under the terms of"
             << " the GNU General Public License Version 3"
             << " (https://www.gnu.org/licenses/gpl-3.0.html)";
    log_info << "Usage: ToxPhone [sh]";
    log_info << "  -s do sleep when start program (in seconds)";
    log_info << "  -h this help";
}

//std::vector<int> testNRVO(int value, size_t size, const std::vector<int> **localVec)
//{
//    std::vector<int> vec(size, value);
//    (void) vec;
//    *localVec = &vec;

//    return vec;
//}

int main(int argc, char *argv[])
{
//    const std::vector<int> *localVec = nullptr;
//    std::vector<int> vec = testNRVO(0, 10, &localVec);

//    if (&vec == localVec)
//        std::cout << "NRVO was applied" << std::endl;
//    else
//        std::cout << "NRVO was not applied" << std::endl;

//    return 0;


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
            stopLog();
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
            alog::stop();
            return 1;
        }

        QString startSleep;

        int c;
        while ((c = getopt(argc, argv, "s:h:")) != EOF)
        {
            switch(c)
            {
                case 'h':
                    helpInfo();
                    alog::stop();
                    exit(0);
                case 's':
                    startSleep = optarg;
                    break;
                case '?':
                    log_error << "Invalid option";
                    alog::stop();
                    return 1;
            }
        }

        if (!startSleep.isEmpty())
        {
            bool ok;
            int s = startSleep.toInt(&ok);
            if (!ok)
            {
                log_error << log_format(
                    "Failed convert the value (%?) for -s parameter to integer",
                    startSleep);
                alog::stop();
                return 1;
            }
            if (s > 0)
                sleep(s);
        }

        // Путь к основному конфиг-файлу
        QString configFile = config::qdir() + "/toxphone.conf";
        if (!QFile::exists(configFile))
        {
            log_error << "Config file " << configFile << " not exists";
            alog::stop();
            return 1;
        }

        config::base().setReadOnly(true);
        config::base().setSaveDisabled(true);
        if (!config::base().readFile(configFile.toStdString()))
        {
            alog::stop();
            return 1;
        }

        // Путь к конфиг-файлу текущих настроек
        QString configFileS;
        config::base().getValue("state.file", configFileS);

        config::dirExpansion(configFileS);
        config::state().readFile(configFileS.toStdString());

        // Создаем дефолтный сэйвер для логгера
        if (!alog::configDefaultSaver())
        {
            alog::stop();
            return 1;
        }

        log_info << log_format(
            "ToxPhone is running (version: %?; protocol version: %?-%?; gitrev: %?)",
            productVersion().toString(),
            PPROTO_VERSION_LOW, PPROTO_VERSION_HIGH, GIT_REVISION);
        alog::logger().flush();

        alog::logger().removeSaverStdOut();
        alog::logger().removeSaverStdErr();

        // Создаем дополнительные сэйверы для логгера
        alog::configExtendedSavers();
        alog::printSaversInfo();

        // Флаг логирование tox-ядра
        config::base().getValue("logger.enable_toxcore_log", enable_toxcore_log);

#ifdef NDEBUG
        // Понижаем уровень логера для консоли что бы видеть только сообщения
        // о возможных неисправностях
        alog::logger().addSaverStdOut(alog::Level::Warning);
#endif

        // Test
        //const QUuidEx cmd = CommandsPool::Registry{"917b1e18-4a5e-4432-b6d6-457a666ef2b1", "IncomingConfigConnection1", true};

        if (!pproto::command::pool().checkUnique())
        {
            stopProgram();
            return 1;
        }

        if (!pproto::error::checkUnique())
        {
            stopProgram();
            return 1;
        }

        if (sodium_init() < 0)
        {
            log_error  << "Can't init libsodium";
            stopProgram();
            return 1;
        }

        // Пул потоков нужно активировать после кода демонизации
        trd::threadPool().start();

        Application appl {argc, argv};

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

        // Инициализация pproto::listener::tcp
        QHostAddress hostAddress = QHostAddress::Any;
        config::readHostAddress("config_connection.address", hostAddress);

        int port = 33601;
        config::base().getValue("config_connection.port", port);

        if (!tcp::listener().init({hostAddress, port}))
        {
            stopProgram();
            return 1;
        }

        if (!udp::socket().init({QHostAddress::AnyIPv4, port}))
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

        if (!toxNet().init())
        {
            toxNet().deinit();
            stopProgram();
            return 1;
        }
        toxNet().start();

        qRegisterMetaType<VoiceFrameInfo::Ptr>("VoiceFrameInfo::Ptr");

        chk_connect_d(&toxNet(),        &ToxNet::internalMessage,
                      &toxCall(),       &ToxCall::message)

        chk_connect_q(&toxCall(),       &ToxCall::startVoice,
                      &audioDev(),      &AudioDev::startVoice)

        chk_connect_q(&toxCall(),       &ToxCall::internalMessage,
                      &audioDev(),      &AudioDev::message)

        chk_connect_q(&toxCall(),       &ToxCall::internalMessage,
                      &appl,            &Application::message)

        chk_connect_q(&audioDev(),      &AudioDev::internalMessage,
                      &toxCall(),       &ToxCall::message)

        chk_connect_q(&audioDev(),      &AudioDev::internalMessage,
                      &appl,            &Application::message)

        chk_connect_d(&appl,            &Application::internalMessage,
                      &toxNet(),        &ToxNet::message)
        chk_connect_d(&appl,            &Application::internalMessage,
                      &toxCall(),       &ToxCall::message)
        chk_connect_q(&appl,            &Application::internalMessage,
                      &audioDev(),      &AudioDev::message)
        chk_connect_q(&appl,            &Application::internalMessage,
                      &voiceFilters(),  &VoiceFilters::message)
        chk_connect_q(&appl,            &Application::internalMessage,
                      &appl,            &Application::message)

        chk_connect_q(&phoneDiverter(), &PhoneDiverter::attached,
                      &appl,            &Application::phoneDiverterAttached)
        chk_connect_q(&phoneDiverter(), &PhoneDiverter::detached,
                      &appl,            &Application::phoneDiverterDetached)
        chk_connect_q(&phoneDiverter(), &PhoneDiverter::pstnRing,
                      &appl,            &Application::phoneDiverterPstnRing)
        chk_connect_q(&phoneDiverter(), &PhoneDiverter::key,
                      &appl,            &Application::phoneDiverterKey)
        chk_connect_q(&phoneDiverter(), qOverload<PhoneDiverter::Handset>(&PhoneDiverter::handset),
                      &appl,            &Application::phoneDiverterHandset)

        if (!toxCall().init(toxNet().tox()))
        {
            stopProgram();
            return 1;
        }
        toxCall().start();

        if (!audioDev().init()
            || !audioDev().start())
        {
            stopProgram();
            return 1;
        }

        if (!phoneDiverter().init())
        {
            stopProgram();
            return 1;
        }
        phoneDiverter().start();

        if (Application::isStopped())
        {
            stopProgram();
            return 0;
        }

        alog::logger().removeSaverStdOut();
        alog::logger().removeSaverStdErr();

        usleep(200*1000);
        appl.initPhoneDiverter();

        QMetaObject::invokeMethod(&appl, "sendToxPhoneInfo", Qt::QueuedConnection);

        ret = appl.exec();

        pproto::data::ApplShutdown applShutdown;
        applShutdown.applId = Application::applId();

        network::Interface::List netInterfaces = network::getInterfaces();
        for (network::Interface* intf : netInterfaces)
            sendUdpMessageToConfig(intf, port, applShutdown);

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
