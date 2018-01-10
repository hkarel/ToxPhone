#include "shared/break_point.h"
#include "shared/config/yaml_config.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/version/version_number.h"
#include "shared/qt/config/config.h"
#include "widgets/main_window.h"

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

using namespace std;

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

        //QString configFile = configPath("27fretail.conf");
        QString configFile = "~/.config/ToxPhone/toxphone_config.conf";
        config::homeDirExpansion(configFile);
        config::state().read(configFile.toStdString());

        QString logFile;
        if (!config::state().getValue("logger.file", logFile))
        {
            //logFile = configPath("log/27fretail.log");
            logFile = "~/.config/ToxPhone/toxphone_config.log";
            config::state().setValue("logger.file", logFile);
            config::state().save();
        }
        config::homeDirExpansion(logFile);

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

        if (!communication::commandsPool().checkUnique())
        {
            stopProgram();
            return 1;
        }

        QApplication appl {argc, argv};
        appl.setApplicationName("ToxPhoneConfig " + productVersion().toString());

        MainWindow w;
        if (!w.init())
        {
            QMessageBox::critical(0, qApp->applicationName(),
                "During the initialization of the program, errors occurred. "
                "The execution of the program will be stopped. For information "
                "about the error, see the log file " + logFile);

            stopProgram();
            return 1;
        }
        w.loadGeometry();
        w.loadSettings();
        w.show();

        alog::logger().removeSaverStdOut();
        alog::logger().removeSaverStdErr();

        QMetaObject::invokeMethod(&w, "requestPhonesList", Qt::QueuedConnection);
        ret = appl.exec();

        w.saveGeometry();
        w.saveSettings();
        w.deinit();

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
