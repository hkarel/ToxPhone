#pragma once

#include "shared/steady_timer.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "kernel/network/interfaces.h"

#include <QtCore>
#include <QCoreApplication>

using namespace std;
using namespace communication;
using namespace communication::transport;

class ToxPhoneApplication : public QCoreApplication
{
public:
    ToxPhoneApplication(int &argc, char **argv);

    static void stop() {_stop = true;}
    static bool isStopped() {return _stop;}
    static const QUuidEx& applId() {return _applId;}

public slots:
    void stop2(int exitCode);
    void sendInfo();

private slots:
    void message(const communication::Message::Ptr&);
    void socketConnected(communication::SocketDescriptor);
    void socketDisconnected(communication::SocketDescriptor);

private:
    Q_OBJECT
    void timerEvent(QTimerEvent* event) override;
    void updateNetInterfaces();

    //--- Обработчики команд ---
    void command_ToxPhoneInfo(const Message::Ptr&);

private:
    int _exitCode = {0};
    int _stopTimerId = {-1};
    static volatile bool _stop;

    FunctionInvoker _funcInvoker;

    network::Interface::List _netInterfaces;
    steady_timer _netInterfacesTimer;

    // Идентификатор приложения времени исполнения.
    static QUuidEx _applId;
};

