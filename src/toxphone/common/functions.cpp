#include "functions.h"
#include "shared/safe_singleton.h"
#include <atomic>

ToxConfig& toxConfig()
{
    return safe::singleton<ToxConfig>();
}

void ToxConfig::send(const pproto::Message::Ptr& message) const
{
    if (socket)
        socket->send(message);
}
void ToxConfig::reset()
{
    if (socket)
        socket->stop();
    socket.reset();
    socketDescriptor = -1;
}

bool diverterIsActive(bool* val)
{
    static std::atomic_bool active {false};
    if (val)
        active = *val;
    return active;
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
