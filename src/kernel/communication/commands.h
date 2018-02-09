/*****************************************************************************
  В модуле представлен список идентификаторов команд для коммуникации между
  клиентской и серверной частями приложения.
  В данном модуле представлен список команд персональный для этого приложения.

  Требование надежности коммуникаций: однажды назначенный идентификатор коман-
  ды не должен более меняться.

*****************************************************************************/

#pragma once

//#include "shared/_list.h"
//#include "shared/clife_base.h"
//#include "shared/clife_ptr.h"
#include "shared/container_ptr.h"
#include "shared/qt/quuidex.h"
#include "shared/qt/communication/commands_base.h"
#include <sys/time.h>

namespace communication {
namespace command {

//----------------------------- Список команд --------------------------------

/**
  Команда используется для поиска экземпляров ToxPhone, отправляется как широко-
  вещательное сообщение, так же она используется в качетсве ответа.
*/
extern const QUuidEx ToxPhoneInfo;

/**
  Команда завершения работы приложения
*/
extern const QUuidEx ApplShutdown;

/**
  Внутренняя команда, используется для уведомления потока ToxKernel о том,
  что выполнено подключение конфигуратора.
*/
extern const QUuidEx IncomingConfigConnection;

/**
  Передает данные о tox-профиле
*/
extern const QUuidEx ToxProfile;

/**
  Запрос дружбы
*/
extern const QUuidEx RequestFriendship;

/**
  Запрос от другого пользователя на добавление в друзья
*/
extern const QUuidEx FriendRequest;

/**
  Список запросов от других пользователей
*/
extern const QUuidEx FriendRequests;

/**
  Информация по другу
*/
extern const QUuidEx FriendItem;

/**
  Список друзей
*/
extern const QUuidEx FriendList;

/**
  Удалить друга
*/
extern const QUuidEx RemoveFriend;

/**
  Статус подключения к DHT сети
*/
extern const QUuidEx DhtConnectStatus;

/**
  Информация по аудио-устройству
*/
extern const QUuidEx AudioDev;

/**
  Список аудио-устройств
*/
extern const QUuidEx AudioDevList;

/**
  Тест вывода звука
*/
extern const QUuidEx AudioSinkTest;

/**
  Отображение уровня сигнала микрофона в конфигураторе
*/
extern const QUuidEx AudioSourceLevel;

/**
  Команда для управления tox-звоноком
*/
extern const QUuidEx ToxCallAction;

/**
  Событие определяет состояние tox-звонка
*/
extern const QUuidEx ToxCallState;

/**
  Вспомогательная команда, используется для отправки tox-сообщения
*/
extern const QUuidEx ToxMessage;



} // namespace command

//---------------- Структуры данных используемые в сообщениях ----------------

namespace data {

struct ToxPhoneInfo : Data<&command::ToxPhoneInfo,
                            Message::Type::Command,
                            Message::Type::Answer>
{
    // Краткая информация об экземпляре ToxPhone
    QString info;

    // Идентификатор приложения времени исполнения.
    QUuidEx applId;

    // Признак что интерфейс с которого пришло сообщение является poin-to-point.
    // Используется для отсечения poin-to-point интерфейсов в конфигураторе.
    bool isPointToPoint = {false};

    DECLARE_B_SERIALIZE_FUNC
};

struct ApplShutdown : Data<&command::ApplShutdown,
                            Message::Type::Command>
{
    // Идентификатор приложения времени исполнения.
    QUuidEx applId;
    DECLARE_B_SERIALIZE_FUNC
};

struct ToxProfile : Data<&command::ToxProfile,
                          Message::Type::Command,
                          Message::Type::Answer>
{
    QByteArray toxId;
    QString    name;
    QString    status;

    DECLARE_B_SERIALIZE_FUNC
};

struct RequestFriendship : Data<&command::RequestFriendship,
                                 Message::Type::Command,
                                 Message::Type::Answer>
{
    QByteArray toxId;
    QString    message;

    DECLARE_B_SERIALIZE_FUNC
};

struct FriendRequest : Data<&command::FriendRequest,
                             Message::Type::Command,
                             Message::Type::Answer>
{
    QByteArray publicKey;
    QString    message;
    QString    dateTime;
    qint8      accept = {-1};  // 0 - запрос на дружбу отвергнут
                               // 1 - запрос на дружбу принят
    DECLARE_B_SERIALIZE_FUNC
};

struct FriendRequests : Data<&command::FriendRequests,
                              Message::Type::Event>
{
    QVector<FriendRequest> list;
    DECLARE_B_SERIALIZE_FUNC
};

struct FriendItem : Data<&command::FriendItem,
                          Message::Type::Command,
                          Message::Type::Answer,
                          Message::Type::Event>
{
    enum class ChangeFlag : quint32
    {
        None          = 0x00,
        Name          = 0x01, // Изменено имя
        StatusMessage = 0x02, // Изменен статус-сообщение
        IsConnecnted  = 0x04  // Изменен признак подключения к сети
    };

    ChangeFlag changeFlag = {ChangeFlag::None};
    QByteArray publicKey;     // Идентификатор друга
    quint32    number;        // Числовой идентификатор друга
    QString    name;          // Имя друга
    QString    statusMessage; // Статус-сообщение
    bool       isConnecnted;  // Признак подключения к сети

    DECLARE_B_SERIALIZE_FUNC
};

struct FriendList : Data<&command::FriendList,
                          Message::Type::Event>
{
    QVector<FriendItem> list;
    DECLARE_B_SERIALIZE_FUNC
};

struct RemoveFriend: Data<&command::RemoveFriend,
                           Message::Type::Command,
                           Message::Type::Answer>
{
    QByteArray publicKey;
    QString    name;

    DECLARE_B_SERIALIZE_FUNC
};

struct DhtConnectStatus : Data<&command::DhtConnectStatus,
                                Message::Type::Event>
{
    bool active = {false};
    DECLARE_B_SERIALIZE_FUNC
};


/**
  Тип аудио-устройства
*/
enum AudioDevType : quint32
{
    Sink   = 0, // Устройство воспроизведения
    Source = 1  // Микрофон
};

struct AudioDev : Data<&command::AudioDev,
                        Message::Type::Command,
                        Message::Type::Answer,
                        Message::Type::Event>
{
    //typedef container_ptr<AudioDev> Ptr;
    //typedef lst::List<AudioDev, lst::CompareItemDummy> List;

    enum class ChangeFlag : quint32
    {
        None       = 0,
        Volume     = 1, // Изменен уровень громкости
        IsCurrent  = 2, // Изменен признак устройства по умолчанию
    };

    ChangeFlag   changeFlag = {ChangeFlag::None};
    quint16      cardIndex;     // Индекс звуковой карты
    AudioDevType type;          // Тип устройства
    quint16      index;         // Индекс устройства
    QByteArray   name;          // Наименование устройства
    QString      description;   // Описание устройства
    quint8       channels;      // Количество каналов
    quint32      baseVolume;    // Базовый уровень громкости
    quint32      currentVolume; // Текущий уровень громкости (для первого канала)
    quint32      volumeSteps;   // Количество шагов уровня громкости
    bool         isCurrent;     // Признак текущего устройства (для ToxPhone)

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioDevList : Data<&command::AudioDevList,
                            Message::Type::Event>
{
    AudioDevType type;
    QVector<AudioDev> list;
    DECLARE_B_SERIALIZE_FUNC
};

struct AudioSourceLevel : Data<&command::AudioSourceLevel,
                                Message::Type::Event>
{
    quint32 average = {0}; // Усредненное значение уровня звукового потока
    quint32 time    = {0}; // Время обновления average (в микросекундах)

    DECLARE_B_SERIALIZE_FUNC
};

struct ToxCallAction : Data<&command::ToxCallAction,
                             Message::Type::Command,
                             Message::Type::Answer>
{
    // Выполняемые действия
    enum class Action : quint32
    {
        None   = 0,
        Accept = 1, // Принять входящий вызов
        Reject = 2, // Отклонить входящий вызов
        Call   = 3, // Сделать вызов
        End    = 4  // Завершить звонок
    };

    Action  action       = {Action::None};
    quint32 friendNumber = quint32(-1); // Числовой идентификатор друга

    DECLARE_B_SERIALIZE_FUNC
};

struct ToxCallState : Data<&command::ToxCallState,
                            Message::Type::Event>
{
    // Направление звонка: входящий/исходящий
    enum class Direction : quint32
    {
        Undefined = 0,
        Incoming  = 1,
        Outgoing  = 2
    };

    // Состояние соединения
    enum class State : quint32
    {
        Undefined     = 0,
        WaitingAnswer = 1, // Процесс установки соединения
        InProgress    = 2, // Соединение установлено
    };

    Direction direction    = {Direction::Undefined};
    State     state        = {State::Undefined};
    quint32   friendNumber = quint32(-1);  // Числовой идентификатор друга

    DECLARE_B_SERIALIZE_FUNC
};


struct ToxMessage : Data<&command::ToxMessage,
                          Message::Type::Command>
{
    quint32 friendNumber; // Числовой идентификатор друга
    QByteArray text;

    DECLARE_B_SERIALIZE_FUNC
};


} // namespace data
} // namespace communication
