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
#include "kernel/communication/commands.h"
#include "toxcore/tox.h"

#include <QtCore>
#include <atomic>
#include <mutex>
#include <condition_variable>

using namespace std;
using namespace communication;
using namespace communication::transport;

class ToxNet : public QThreadEx
{
public:
    bool init();
    void deinit();
    Tox* tox() const {return _tox;}

public slots:
    void message(const communication::Message::Ptr&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(ToxNet)
    ToxNet();

    void run() override;
    void updateBootstrap();
    bool saveState();

    bool setUserProfile(const QString& name, const QString& status);
    void setDhtConnectStatus(bool val) {_dhtConnected = val;}

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_ToxProfile(const Message::Ptr&);
    void command_RequestFriendship(const Message::Ptr&);
    void command_FriendRequest(const Message::Ptr&);
    void command_RemoveFriend(const Message::Ptr&);
    void command_ToxMessage(const Message::Ptr&);

    // Функции обновляют состояние конфигуратора
    void updateFriendList();
    void updateFriendRequests();
    void updateDhtStatus();

    bool fillFriendItem(data::FriendItem&, uint32_t friendNumber);

private:
    // Tox callback
    static void tox_friend_request          (Tox* tox, const uint8_t* pub_key,
                                             const uint8_t* msg, size_t length,
                                             void* user_data);
    static void tox_friend_name             (Tox* tox, uint32_t friend_number,
                                             const uint8_t* name, size_t length,
                                             void* user_data);
    static void tox_friend_status_message   (Tox* tox, uint32_t friend_number,
                                             const uint8_t* message, size_t length,
                                             void* user_data);
    static void tox_friend_message          (Tox* tox, uint32_t friend_number,
                                             TOX_MESSAGE_TYPE type, const uint8_t* message,
                                             size_t length, void* user_data);
    static void tox_self_connection_status  (Tox* tox, TOX_CONNECTION connection_status,
                                             void* user_data);
    static void tox_friend_connection_status(Tox* tox, uint32_t friend_number,
                                             TOX_CONNECTION connection_status,
                                             void* user_data);
private:
    struct BootstrapNode
    {
        QString address;
        quint16 port;
        QString publicKey;
        QString name;      // Информационный параметр
        ~BootstrapNode();
    };
    QVector<BootstrapNode> _bootstrapNodes;

    Tox* _tox = {0};
    Tox_Options _toxOptions;
    QByteArray  _savedState;
    QString _configPath;
    QString _configFile;
    bool _dhtConnected = {false};

    FunctionInvoker _funcInvoker;

    Message::List _messages;
    mutex _threadLock;
    condition_variable _threadCond;
    atomic_bool _threadSleep = {false};

    template<typename T, int> friend T& ::safe_singleton();
};
ToxNet& toxNet();
