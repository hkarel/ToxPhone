#pragma once

#include "kernel/network/interfaces.h"
#include "kernel/communication/commands.h"
#include "diverter/phone_diverter.h"

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

    void initPhoneDiverter();

    static void stop() {_stop = true;}
    static bool isStopped() {return _stop;}
    static const QUuidEx& applId() {return _applId;}

signals:
    // Используется для отправки сообщения в пределах программы
    void internalMessage(const communication::Message::Ptr&);

public slots:
    static void stop(int exitCode);
    void sendToxPhoneInfo();

private slots:
    void message(const communication::Message::Ptr&);
    void socketConnected(communication::SocketDescriptor);
    void socketDisconnected(communication::SocketDescriptor);

    void phoneDiverterAttached();
    void phoneDiverterDetached();
    void phoneDiverterPstnRing();
    void phoneDiverterKey(int);
    void phoneDiverterHandset(PhoneDiverter::Handset);

private:
    Q_OBJECT
    void timerEvent(QTimerEvent* event) override;
    void updateNetInterfaces();

    //--- Обработчики команд ---
    void command_ToxPhoneInfo(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);
    void command_DiverterChange(const Message::Ptr&);
    void command_DiverterTest(const Message::Ptr&);
    void command_PhoneFriendInfo(const Message::Ptr&);

    void fillPhoneDiverter(data::DiverterInfo&);

private:
    int _stopTimerId = {-1};
    static volatile bool _stop;
    static std::atomic_int _exitCode;

    // Индикатор состояния звонка
    data::ToxCallState _callState;

    //data::PhoneDiverter _phoneDiverter;
    QHash<quint32/*PhoneNumber*/, QByteArray/*FriendKey*/> _phonesHash;

    QString _diverterPhoneNumber;
    PhoneDiverter::Mode _diverterDefaultMode = {PhoneDiverter::Mode::Pstn};


    FunctionInvoker _funcInvoker;

    network::Interface::List _netInterfaces;
    steady_timer _netInterfacesTimer;

    // Идентификатор приложения времени исполнения.
    static QUuidEx _applId;
};

