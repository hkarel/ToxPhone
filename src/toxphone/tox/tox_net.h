/*****************************************************************************
  Модуль для работы tox-ядра

*****************************************************************************/

#pragma once

#include "commands/commands.h"
#include "commands/error.h"

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/safe_singleton.h"
#include "shared/qt/qthreadex.h"

#include "pproto/func_invoker.h"
#include "toxcore/tox.h"

#include <QtCore>
#include <atomic>

using namespace std;
using namespace pproto;
using namespace pproto::transport;

class ToxNet : public QThreadEx
{
public:
    bool init();
    void deinit();
    Tox* tox() const {return _tox;}

signals:
    // Используется для отправки сообщения в пределах программы
    void internalMessage(const pproto::Message::Ptr&);

public slots:
    void message(const pproto::Message::Ptr&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(ToxNet)
    ToxNet();

    void run() override;
    void updateBootstrap();
    bool saveState();
    bool saveAvatar(const QByteArray& avatar, const QString& avatarFile);

    QByteArray avatarHash(const QString& avatarFile);
    void sendAvatar(uint32_t friendNumber);
    void stopSendAvatars();
    void broadcastSendAvatars();

    bool setUserProfile(const QString& name, const QString& status);
    void setDhtConnectStatus(bool val) {_dhtConnected = val;}

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_ToxProfile(const Message::Ptr&);
    void command_RequestFriendship(const Message::Ptr&);
    void command_FriendRequest(const Message::Ptr&);
    void command_RemoveFriend(const Message::Ptr&);
    void command_PhoneFriendInfo(const Message::Ptr&);
    void command_FriendAudioChange(const Message::Ptr&);
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
    static void tox_file_recv_control       (Tox* tox, uint32_t friend_number, uint32_t file_number,
                                             TOX_FILE_CONTROL control, void* user_data);
    static void tox_file_recv               (Tox* tox, uint32_t friend_number, uint32_t file_number,
                                             uint32_t kind, uint64_t file_size, const uint8_t* filename,
                                             size_t filename_length, void* user_data);
    static void tox_file_recv_chunk         (Tox* tox, uint32_t friend_number, uint32_t file_number,
                                             uint64_t position, const uint8_t* data, size_t length,
                                             void* user_data);
    static void tox_file_chunk_request      (Tox* tox, uint32_t friend_number, uint32_t file_number,
                                             uint64_t position, size_t length, void* user_data);
    static void tox_friend_lossless_packet  (Tox* tox, uint32_t friend_number,
                                             const uint8_t* data, size_t length,
                                             void* user_data);


private:
    struct BootstrapNode
    {
        QString address;
        quint16 port;
        QString publicKey;
        QString name; // Вспомогательный информационный параметр
        ~BootstrapNode();
    };
    QVector<BootstrapNode> _bootstrapNodes;

    Tox* _tox = {0};
    Tox_Options _toxOptions;
    QByteArray _savedState;
    QString _configPath;
    QString _configFile;
    bool _dhtConnected = {false};
    int _updateBootstrapCounter = 30;

    QString _avatarPath;
    QByteArray _avatar;
    int _avatarNeedUpdate = {0};

    struct TransferData
    {
        TransferData() = default;

        uint32_t friendNumber = {0};
        uint32_t fileNumber = {0};
        uint64_t size = {0}; // Размер передаваемых данных
        QByteArray data;

        // Вспомогательный конструктор, используется функцией поиска
        TransferData(uint32_t friendNumber, uint32_t fileNumber)
            : friendNumber(friendNumber), fileNumber(fileNumber)
        {}
        struct Compare
        {
            int operator() (const TransferData* item1, const TransferData* item2) const
            {
                LIST_COMPARE_MULTI_ITEM(item1->friendNumber, item2->friendNumber)
                LIST_COMPARE_MULTI_ITEM(item1->fileNumber,   item2->fileNumber)
                return 0;
            }
        };
        typedef lst::List<TransferData, Compare> List;
    };
    TransferData::List _sendAvatars;
    TransferData::List _recvAvatars;

    // Параметр используется для отслеживания смены статуса подключения друзей.
    // Не используем здесь QSet, т.к. QSet некорректно работает с типом uint32_t
    //std::set<uint32_t> _connectionStatusSet;
    QSet<uint32_t> _connectionStatusSet;

    FunctionInvoker _funcInvoker;

    Message::List _messages;
    QMutex _threadLock;
    QWaitCondition _threadCond;

    template<typename T, int> friend T& ::safe_singleton();
};

ToxNet& toxNet();
