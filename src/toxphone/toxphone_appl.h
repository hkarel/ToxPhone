#pragma once

#include "kernel/network/interfaces.h"
#include "kernel/communication/commands.h"
#include "diverter/phone_diverter.h"

#include "shared/steady_timer.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/udp.h"

#include <sodium/crypto_box.h>
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
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_ToxPhoneInfo(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);
    void command_DiverterChange(const Message::Ptr&);
    void command_DiverterTest(const Message::Ptr&);
    void command_PhoneFriendInfo(const Message::Ptr&);
    void command_ConfigAuthorizationRequest(const Message::Ptr&);
    void command_ConfigAuthorization(const Message::Ptr&);
    void command_ConfigSavePassword(const Message::Ptr&);
    void command_PlaybackFinish(const Message::Ptr&);

    void fillPhoneDiverter(data::DiverterInfo&);

    // Обновляет информацию по дивертеру в конфигураторе
    void updateConfigDiverterInfo();

    // Возвращает дивертер в состояние согласно базовым установкам
    void setDiverterToDefaultState();

    void resetDiverterPhoneNumber();

private:
    int _stopTimerId = {-1};
    static volatile bool _stop;
    static std::atomic_int _exitCode;

    // Идентификатор сокета конфигуратора, используется для предотвращения
    // подключения более чем одного конфигуратора
    //int _configConnectCount = {0};
    //SocketDescriptorSet _closeSocketDescriptors;
    //SocketDescriptor _configSocketDescriptor = {-1};

    // Сессионные ключи для авторизации Tox-конфигуратора
    uchar _toxPublicKey[crypto_box_PUBLICKEYBYTES];
    uchar _toxSecretKey[crypto_box_SECRETKEYBYTES];

    // Сессионный публичный ключ Tox-конфигуратора
    uchar _configPublicKey[crypto_box_PUBLICKEYBYTES];

    // Индикатор состояния звонка
    data::ToxCallState _callState;

    //data::PhoneDiverter _phoneDiverter;
    QHash<quint32/*PhoneNumber*/, QByteArray/*FriendKey*/> _phonesHash;

    bool _asteriskPressed = {false};
    QString _diverterPhoneNumber;
    PhoneDiverter::Mode _diverterDefaultMode = {PhoneDiverter::Mode::Pstn};

    FunctionInvoker _funcInvoker;

    network::Interface::List _netInterfaces;
    steady_timer _netInterfacesTimer;

    // Идентификатор приложения времени исполнения.
    static QUuidEx _applId;
};

template<typename MessageData>
void sendUdpMessageToConfig(const network::Interface* interface, int basePort,
                            const MessageData& messageData)
{
    if (interface->canBroadcast() && !interface->isPointToPoint())
    {
        Message::Ptr message = createMessage(messageData);
        for (int i = 1; i <= 5; ++i)
            message->destinationPoints().insert({interface->broadcast, basePort + i});
        udp::socket().send(message);
    }
    else if (interface->isPointToPoint() && (interface->subnetPrefixLength == 24))
    {
        Message::Ptr message = createMessage(messageData);
        for (int i = 1; i <= 5; ++i)
        {
            union {
                quint8  ip4[4];
                quint32 ip4_val;
            };
            ip4_val = interface->subnet.toIPv4Address();
            for (quint8 i = 1; i < 255; ++i)
            {
                ip4[0] = i;
                QHostAddress addr {ip4_val};
                message->destinationPoints().insert({addr, basePort + i});
                if (message->destinationPoints().count() > 20)
                {
                    udp::socket().send(message);
                    message = createMessage(messageData);
                    usleep(10);
                }
            }
        }
        if (!message->destinationPoints().isEmpty())
            udp::socket().send(message);
    }
}

