#include "shared/break_point.h"
#include "shared/config/yaml_config.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/version/version_number.h"
#include "shared/qt/config/config.h"
#include "widgets/connection_window.h"
#include "widgets/main_window.h"

#include <sodium.h>
#include <QtCore>
#include <QApplication>
#include <QMessageBox>

namespace {

void stopProgram()
{
    log_info << "ToxPhoneConfig is stopped";
    alog::logger().stop();
}

} // namespace

// Ключи для авторизации конфигуратора
uchar configPublicKey[crypto_box_PUBLICKEYBYTES];
uchar configSecretKey[crypto_box_SECRETKEYBYTES];

// Сессионный публичный ключ Tox-клиента
uchar toxPublicKey[crypto_box_PUBLICKEYBYTES];

using namespace std;
using namespace communication;
using namespace communication::transport;

int main(int argc, char *argv[])
{
    int ret = 0;
    try
    {
        alog::logger().start();

#ifdef NDEBUG
        alog::logger().addSaverStdOut(alog::Level::Info, true);
#else
        alog::logger().addSaverStdOut(alog::Level::Debug);
#endif

        QDir homeDir = QDir::home();
        if (!homeDir.exists())
        {
            log_error << "Home dir " << homeDir.path() << " not exists";
            return 1;
        }

        QString configFile = "~/.config/toxphone/toxphone_config.conf";
        config::dirExpansion(configFile);
        config::state().readFile(configFile.toStdString());

        QString logFile;
        if (!config::state().getValue("logger.file", logFile))
        {
            logFile = "~/.config/toxphone/toxphone_config.log";
            config::state().setValue("logger.file", logFile);
            config::state().save();
        }
        config::dirExpansion(logFile);

        QFileInfo logFileInfo {logFile};
        QString logFileDir = logFileInfo.absolutePath();
        if (!QDir(logFileDir).exists())
            if (!QDir().mkpath(logFileDir))
            {
                log_error << "Failed create log directory: " << logFileDir;
                return 1;
            }

        // Создаем дефолтный сэйвер для логгера
        {
            std::string logLevelStr = "info";
            if (!config::state().getValue("logger.level", logLevelStr))
                config::state().setValue("logger.level", logLevelStr);

            bool logContinue = false;
            if (!config::state().getValue("logger.continue", logContinue))
                config::state().setValue("logger.continue", false);

            alog::Level logLevel = alog::levelFromString(logLevelStr);
            alog::SaverPtr saver {new alog::SaverFile("default",
                                                      logFile.toStdString(),
                                                      logLevel,
                                                      logContinue)};
            alog::logger().addSaver(saver);
        }
        log_info << "ToxPhoneConfig is running"
                 << " (version " << productVersion().toString() << ")";
        alog::logger().flush();

        if (!communication::command::pool().checkUnique())
        {
            stopProgram();
            return 1;
        }

        if (!communication::error::checkUnique())
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

        QApplication appl {argc, argv};
        QApplication::setApplicationName("ToxPhoneConfig " + productVersion().toString());
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setWindowIcon(QIcon("://resources/toxphone.png"));

        tcp::Socket::Ptr socket {new tcp::Socket};

        QString errMessage = QObject::tr(
            "During the initialization of the program, errors occurred.\n"
            "The execution of the program will be stopped.\n"
            "For information about the error, see the log file: %1");
        errMessage = errMessage.arg(logFile);

        ConnectionWindow cw;
        if (!cw.init(socket))
        {
            QMessageBox::critical(0, qApp->applicationName(), errMessage);
            stopProgram();
            return 1;
        }
        cw.loadGeometry();
        cw.show();

        MainWindow mw;
        if (!mw.init(socket))
        {
            QMessageBox::critical(0, qApp->applicationName(), errMessage);
            stopProgram();
            return 1;
        }
        mw.loadGeometry();
        mw.loadSettings();
        //mw.show();

        alog::logger().removeSaverStdOut();
        alog::logger().removeSaverStdErr();

        QMetaObject::invokeMethod(&cw, "requestPhonesList", Qt::QueuedConnection);

        ret = appl.exec();

        mw.saveGeometry();
        mw.saveSettings();
        mw.deinit();

        cw.saveGeometry();
        cw.deinit();

        socket->disconnect();

        config::state().save();
    }
    catch (std::exception& e)
    {
        log_error << "Detail: " << e.what();
        ret = 1;
    }
    catch (...)
    {
        log_error << "Unknown error";
        ret = 1;
    }

#ifdef NDEBUG
    alog::logger().addSaverStdOut(alog::Level::Info, true);
#else
    alog::logger().addSaverStdOut(alog::Level::Debug);
#endif

    stopProgram();
    return ret;
}
