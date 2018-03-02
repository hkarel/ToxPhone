#pragma once

#include "kernel/network/interfaces.h"
#include "shared/steady_timer.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"

#include <QtCore>
#include <QCoreApplication>
#include <atomic>

using namespace std;
using namespace communication;
using namespace communication::transport;

class ToxPhoneApplication : public QCoreApplication
{
public:
    ToxPhoneApplication(int &argc, char **argv);
    ~ToxPhoneApplication();

    static void stop() {_stop = true;}
    static bool isStopped() {return _stop;}
    static const QUuidEx& applId() {return _applId;}

public slots:
    static void stop(int exitCode);
    void sendToxPhoneInfo();

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
    int _stopTimerId = {-1};
    static volatile bool _stop;
    static std::atomic_int _exitCode;

    FunctionInvoker _funcInvoker;

    network::Interface::List _netInterfaces;
    steady_timer _netInterfacesTimer;

    // Идентификатор приложения времени исполнения.
    static QUuidEx _applId;
};

