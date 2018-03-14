/*****************************************************************************
  В модуле представлен список идентификаторов команд для коммуникации между
  клиентской и серверной частями приложения.
  В данном модуле представлен список команд персональный для этого приложения.

  Требование надежности коммуникаций: однажды назначенный идентификатор коман-
  ды не должен более меняться.

*****************************************************************************/

#pragma once

#include "shared/_list.h"
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
extern const QUuidEx AudioDevInfo;

/**
  Информация по изменению состояния аудио-устройства
*/
extern const QUuidEx AudioDevChange;

/**
  Команда для запуска/остановки аудио-тестов
*/
extern const QUuidEx AudioTest;

/**
  Отображение уровня сигнала микрофона в конфигураторе
*/
extern const QUuidEx AudioRecordLevel;

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

/**
  Информация по телефонному дивертеру
*/
extern const QUuidEx DiverterInfo;

/**
  Информация по изменению состояния телефонного дивертера
*/
extern const QUuidEx DiverterChange;

/**
  Команда для запуска/остановки тестов для дивертера
*/
extern const QUuidEx DiverterTest;


/**
  Расширенная информация по другу. Используется при сохранении настроек
  из конфигуратора и связывает идентификатор друга и номер телефона
*/
extern const QUuidEx PhoneFriendInfo;


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
    QByteArray publicKey;      // Идентификатор друга
    quint32    number;         // Числовой идентификатор друга
    QString    name;           // Имя друга
    QString    statusMessage;  // Статус-сообщение
    bool       isConnecnted;   // Признак подключения к сети

    /** Дополнительные параметры для ToxPhone **/
    QString nameAlias;         // Альтернативное имя друга
    quint32 phoneNumber = {0}; // Число (0-99) для вызова друга с телефона

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

struct AudioDevInfo : Data<&command::AudioDevInfo,
                            Message::Type::Command,
                            Message::Type::Answer,
                            Message::Type::Event>
{
    quint32      cardIndex;   // Индекс звуковой карты
    AudioDevType type;        // Тип устройства
    quint32      index;       // Индекс устройства
    QByteArray   name;        // Наименование устройства
    QString      description; // Описание устройства
    quint8       channels;    // Количество каналов
    qint32       baseVolume;  // Базовый уровень громкости
    qint32       volume;      // Текущий уровень громкости (для первого канала)
    qint32       volumeSteps; // Количество шагов уровня громкости
    bool         isCurrent  = {false}; // Признак текущего устройства (для ToxPhone)
    bool         isDefault  = {false}; // Признак устройства по умолчанию (для ToxPhone)

    // Признаки для обозначения устройства для проигрывания звонка вызова.
    // Пока эти параметры ремим, попробуем на их основе реализовать версию
    // структуры №2 для AudioDevInfo
    //bool isRingtoneCurrent = {false};
    //bool isRingtoneDefault = {false};

    struct Find
    {
        int operator() (const char*       devName,  const AudioDevInfo* item2, void*) const;
        int operator() (const QByteArray* devName,  const AudioDevInfo* item2, void*) const;
        int operator() (const quint32*    devIndex, const AudioDevInfo* item2, void*) const;
    };
    typedef lst::List<AudioDevInfo, Find> List;

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioDevChange: Data<&command::AudioDevChange,
                             Message::Type::Command,
                             Message::Type::Answer,
                             Message::Type::Event>
{
    AudioDevChange() = default;
    AudioDevChange(const AudioDevInfo&);

    enum class ChangeFlag : quint32
    {
        None    = 0,
        Volume  = 1, // Изменен уровень громкости
        Current = 2, // Изменен признак текущего устройства
        Default = 3, // Изменен признак устройства по умолчанию
        Remove  = 4, // Устройство удалено
    };
    ChangeFlag   changeFlag = {ChangeFlag::None};
    quint32      cardIndex; // Индекс звуковой карты
    AudioDevType type;      // Тип устройства
    quint32      index;     // Индекс устройства
    qint64       value;     // Зачение изменяемого параметра

    // Признак, что измененный параметр относится к устройству для проигрывания
    // звонка вызова.
    // Пока эти параметры ремим, см. описаний для параметров isRingtoneCurrent,
    // isRingtoneDefault в AudioDevInfo.
    //bool isRingtone = {false};

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioTest : Data<&command::AudioTest,
                         Message::Type::Command,
                         Message::Type::Answer,
                         Message::Type::Event>
{
    bool begin = {false};
    bool end() const {return !begin;}

    bool playback = {false}; // Тест проигрывания звука
    bool record   = {false}; // Тест микрофона

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioRecordLevel : Data<&command::AudioRecordLevel,
                                Message::Type::Event>
{
    quint32 max  = {0}; // Максимальное значение уровня звукового сигнала
    quint32 time = {0}; // Время обновления max (в миллисекундах)

    DECLARE_B_SERIALIZE_FUNC
};

struct ToxCallAction : Data<&command::ToxCallAction,
                             Message::Type::Command,
                             Message::Type::Answer>
{
    // Выполняемые действия
    enum class Action : quint32
    {
        None      = 0,
        Accept    = 1, // Принять входящий вызов
        Reject    = 2, // Отклонить входящий вызов
        HandsetOn = 3, // Отклонить входящий вызов по причине того, что
                       // телефонная трубка поднята/не положена
        Call      = 4, // Сделать вызов
        End       = 5  // Завершить звонок
    };

    Action  action = {Action::None};
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
    enum class CallState : quint32
    {
        Undefined     = 0,
        WaitingAnswer = 1, // Процесс установки соединения
        InProgress    = 2, // Соединение установлено
    };

    // Состояние завершения соединения. Сейчас предполагается, что состояние
    // завершения соединения будет использоваться для проигрывания различных
    // звуков при завершении или прерывании звонка, что позволит пользователю
    // понимать причину неудачи.
    enum class CallEnd : quint32
    {
        Undefined    = 0,
        SelfEnd      = 1,  // Звонок удачно завершен пользователем
        FriendEnd    = 2,  // Звонок удачно завершен другом
        NotConnected = 3,  // Друг не подключен
        FriendInCall = 4,  // Друг находиться в состоянии звонка, то есть линия
                           // занята.
        Reject       = 5,  // Пользователь отклонил входящий вызов
        Error        = 10  // В процессе звонка произошла какая-то ошибка
    };

    Direction direction    = {Direction::Undefined};
    CallState callState    = {CallState::Undefined};
    CallEnd   callEnd      = {CallEnd::Undefined};
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

// Режим дивертера по умолчанию
enum class DiverterDefaultMode : quint32
{
    Pstn = 0,
    Usb  = 1
};

struct DiverterInfo : Data<&command::DiverterInfo,
                            Message::Type::Command,
                            Message::Type::Answer,
                            Message::Type::Event>
{
    // Признак активности механизма дивертера
    bool active = {true};

    // Признак подключенного устройства
    bool attached = {false};

    // Режим дивертера по умолчанию
    DiverterDefaultMode defaultMode = {DiverterDefaultMode::Pstn};

    // Тональность звонка для телефона
    QString ringTone;

    // Информация по устройству
    QString deviceUsbBus  = {"Undefined"};
    QString deviceName    = {"Undefined"};
    QString deviceVersion = {"Undefined"};
    QString deviceSerial  = {"Undefined"};

    DECLARE_B_SERIALIZE_FUNC
};

struct DiverterChange : Data<&command::DiverterChange,
                              Message::Type::Command,
                              Message::Type::Answer>
{
    enum class ChangeFlag : quint32
    {
        None        = 0,
        Active      = 1, // Изменен признак активности дивертера
        DefaultMode = 2, // Изменен признак режима по умолчанию для дивертера
        RingTone    = 3, // Изменено значение для RingTone
    };

    ChangeFlag  changeFlag = {ChangeFlag::None};

    // Признак активности механизма дивертера
    bool active = {true};

    // Режим дивертера по умолчанию
    DiverterDefaultMode defaultMode = {DiverterDefaultMode::Pstn};

    // Тональность звонка для телефона
    QString ringTone;

    DECLARE_B_SERIALIZE_FUNC
};


struct DiverterTest : Data<&command::DiverterTest,
                            Message::Type::Command,
                            Message::Type::Answer,
                            Message::Type::Event>
{
    bool begin = {false};
    bool end() const {return !begin;}

    // Тест тональности звонка для телефона
    bool ringTone = {false};

    DECLARE_B_SERIALIZE_FUNC
};


struct PhoneFriendInfo : Data<&command::PhoneFriendInfo,
                               Message::Type::Command,
                               Message::Type::Answer>
{
    QByteArray publicKey;  // Tox- Идентификатор друга
    quint32    number;     // Tox- Числовой идентификатор друга
    QString    name;       // Tox- Имя друга
    QString    nameAlias;  // Альтернативное имя друга
    quint32    phoneNumber = {0}; // Число (0-99) для вызова друга с телефона

    DECLARE_B_SERIALIZE_FUNC
};


} // namespace data
} // namespace communication
