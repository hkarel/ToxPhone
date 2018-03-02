#include "functions.h"
#include <atomic>

bool configConnected(bool* val)
{
    static std::atomic_bool connected {false};
    if (val)
        connected = *val;
    return connected;
}


QString getFilePath(const QString& fileName)
{
    QStringList paths;
    paths << "./" << "../" << "../../" << "../../../";
    for (int i = 0; i < paths.count(); ++i)
    {
        QString filePath = paths[i] + fileName;
        if (QFile::exists(filePath))
            return QFileInfo(filePath).absoluteFilePath();
    }
    return "";
}
