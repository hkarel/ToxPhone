/*****************************************************************************
  Модуль для работы tox-ядра

*****************************************************************************/

#pragma once

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/safe_singleton.h"
#include "shared/qt/thread/qthreadex.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "toxcore/tox.h"

#include <QtCore>
#include <atomic>

using namespace std;
using namespace communication;
using namespace communication::transport;

class ToxNet : public QThreadEx
{
public:
    bool init();
    int runInit() const {return _runInit;}

    void updateFriendList();
    void updateFriendRequests();
    void updateDhtStatus();

    QString getToxFriendName(const QByteArray& publicKey) const;
    QString getToxFriendName(uint32_t friendNumber) const;

    void setDhtConnectStatus(bool val) {_dhtConnected = val;}

public slots:
    void message(const communication::Message::Ptr&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(ToxNet)
    ToxNet();

    void run() override;
    bool createToxInstance();
    void assignToxCallback();
    void updateBootstrap();
    bool saveState();

    bool setUserProfile(const QString& name, const QString& status);

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_ToxProfile(const Message::Ptr&);
    void command_RequestFriendship(const Message::Ptr&);
    void command_FriendRequest(const Message::Ptr&);
    void command_RemoveFriend(const Message::Ptr&);


private:
    struct BootstrapNode
    {
        QString address;
        quint16 port;
        QString publicKey;
        QString name;      // Информационный параметр
    };
    QVector<BootstrapNode> _bootstrapNodes;

    Tox* _tox;
    Tox_Options _toxOptions;
    QByteArray  _savedState;
    volatile int _runInit = {-1};
    steady_timer _iterationTimer;
    QString _configPath;
    QString _configFile;
    bool _dhtConnected = {false};

    Message::List _messages;
    mutable atomic_flag _messagesLock = ATOMIC_FLAG_INIT;

    FunctionInvoker _funcInvoker;

    template<typename T, int> friend T& ::safe_singleton();
};
ToxNet& toxNet();
