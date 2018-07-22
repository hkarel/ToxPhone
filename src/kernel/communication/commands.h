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
#include "shared/qt/communication/host_point.h"
#include <sys/time.h>

namespace communication {
namespace command {

//----------------------------- Список команд --------------------------------

/**
  Команда используется для поиска экземпляров ToxPhone. Команда отправляется
  как широковещательное сообщение, так же она используется в качестве ответа.
*/
extern const QUuidEx ToxPhoneInfo;

/**
  Информация о приложении
*/
extern const QUuidEx ToxPhoneAbout;

/**
  Команда завершения работы приложения
*/
extern const QUuidEx ApplShutdown;

/**
  Внутренняя команда, используется для уведомления модулей ToxPhone о том,
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
  Информация по изменению персональных аудио-установок друга
*/
extern const QUuidEx FriendAudioChange;

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
  Информация по изменению состояния потока воспроизведения
*/
extern const QUuidEx AudioStreamInfo;

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
  Команда возвращает причину по которой друг завершил звонок
*/
extern const QUuidEx FriendCallEndCause;

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

/**
  Запрос процесса авторизации конфигуратора
*/
extern const QUuidEx ConfigAuthorizationRequest;

/**
  Команда авторизации конфигуратора. Так же используется для сохранения
  состояния активности и пароля для авторизации.
*/
extern const QUuidEx ConfigAuthorization;

/**
  Сохранение пароля
*/
extern const QUuidEx ConfigSavePassword;

/**
  Информирует модули программы о том, что воспроизведение звука завершено
*/
extern const QUuidEx PlaybackFinish;

/**
  Информирует модули программы о том, телефонная трубка поднята или опущена
*/
extern const QUuidEx DiverterHandset;

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

    // Адрес ToxPhone-клиента в локальной сети.
    HostPoint hostPoint;

    // Признак что интерфейс с которого пришло сообщение является poin-to-point.
    // Используется для отсечения poin-to-point интерфейсов в конфигураторе.
    bool isPointToPoint = {false};

    // Количество подключенных конфигураторов
    quint16 configConnectCount = {0};

    DECLARE_B_SERIALIZE_FUNC
};

struct ToxPhoneAbout : Data<&command::ToxPhoneAbout,
                             Message::Type::Command,
                             Message::Type::Answer>
{
    quint32 version; // Версия ToxPhone клиента
    quint32 toxcore; // Версия ядра Tox-библиотеки
    QString gitrev;  // Хэш git-коммита
    QString qtvers;  // Версия Qt

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
    QByteArray avatar;
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
                              Message::Type::Command>
{
    QVector<FriendRequest> list;
    DECLARE_B_SERIALIZE_FUNC
};

struct FriendItem : Data<&command::FriendItem,
                          Message::Type::Command,
                          Message::Type::Answer>
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
    QByteArray avatar;

    /** Дополнительные параметры для ToxPhone **/
    QString nameAlias;         // Альтернативное имя друга
    quint32 phoneNumber = {0}; // Число (0-99) для вызова друга с телефона

    /** Персональные аудио-установки **/
    bool personalVolumes = {false}; // Признак использования персональных
                                    // установок для звуковых потоков
    bool echoMute = {false};        // Признак использования системы эхоподавления

    DECLARE_B_SERIALIZE_FUNC
};

struct FriendAudioChange : Data<&command::FriendAudioChange,
                                 Message::Type::Command,
                                 Message::Type::Answer>
{
    enum class ChangeFlag : quint32
    {
        None        = 0,
        PersVolumes = 1, // Изменен признак использования персональных установок
                         // для звуковых потоков.
        EchoMute    = 2, // Изменен признак использования системы эхоподавления
    };
    ChangeFlag changeFlag = {ChangeFlag::None};
    QByteArray publicKey; // Tox- Идентификатор друга
    quint32    number;    // Tox- Числовой идентификатор друга
    qint64     value;     // Зачение изменяемого параметра

    DECLARE_B_SERIALIZE_FUNC
};

struct FriendList : Data<&command::FriendList,
                          Message::Type::Command>
{
    QVector<FriendItem> list;
    DECLARE_B_SERIALIZE_FUNC
};

struct RemoveFriend : Data<&command::RemoveFriend,
                            Message::Type::Command,
                            Message::Type::Answer>
{
    QByteArray publicKey;
    QString    name;

    DECLARE_B_SERIALIZE_FUNC
};

struct DhtConnectStatus : Data<&command::DhtConnectStatus,
                                Message::Type::Command>
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
                            Message::Type::Answer>
{
    quint32      cardIndex;   // Индекс звуковой карты
    AudioDevType type;        // Тип устройства
    quint32      index;       // Индекс устройства
    QByteArray   name;        // Наименование устройства
    QString      description; // Описание устройства
    quint8       channels;    // Количество каналов
    quint32      baseVolume;  // Базовый уровень громкости
    quint32      volume;      // Текущий уровень громкости (для первого канала)
    quint32      volumeSteps; // Количество шагов уровня громкости
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

struct AudioDevChange : Data<&command::AudioDevChange,
                              Message::Type::Command,
                              Message::Type::Answer>
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

struct AudioStreamInfo : Data<&command::AudioStreamInfo,
                               Message::Type::Command,
                               Message::Type::Answer>
{
    // Типы потоков
    enum class Type : quint32
    {
        Undefined  = 0,
        Playback   = 1,
        Voice      = 2,
        Record     = 3
    };

    // Состояния потоков
    enum class State : quint32
    {
        Created    = 0,  // Поток был создан
        Changed    = 1,  // Параметры потока изменились
        Terminated = 2   // Поток был уничтожен
    };

    Type    type  = {Type::Undefined};
    State   state = {State::Terminated};
    quint32 devIndex = (-1); // Индекс устройства
    quint32 index = (-1);    // Индекс потока
    QString name;            // Имя потока
    bool    hasVolume = {true}; // Признак, что поток имеет уровень громкости
    bool    volumeWritable = {true}; // Признак, что у потока можно изменять
                                     // уровень громкости
    quint8  channels = {2};    // Количество каналов
    quint32 volume = {0};      // Текущий уровень громкости (для первого канала)
    quint32 volumeSteps = {0}; // Количество шагов уровня громкости

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioTest : Data<&command::AudioTest,
                         Message::Type::Command,
                         Message::Type::Answer>
{
    bool begin = {false};
    bool end() const {return !begin;}

    bool playback = {false}; // Тест проигрывания звука
    bool record   = {false}; // Тест микрофона

    DECLARE_B_SERIALIZE_FUNC
};

struct AudioRecordLevel : Data<&command::AudioRecordLevel,
                                Message::Type::Command>
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
    quint32 friendNumber = quint32(-1); // Tox- Числовой идентификатор друга

    DECLARE_B_SERIALIZE_FUNC
};

struct ToxCallState : Data<&command::ToxCallState,
                            Message::Type::Command>
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
        IsComplete    = 3  // Используется для совершения дополнительных
                           // действий после того как звонок завершен,
                           // например для проигрывания звуков.
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
        FriendBusy   = 4,  // Друг находиться в состоянии звонка, то есть линия
                           // занята.
        SelfReject   = 5,  // Пользователь отклонил входящий вызов
        FriendReject = 6,  // Друг отклонил входящий вызов
        Error        = 10  // В процессе звонка произошла какая-то ошибка
    };

    Direction  direction = {Direction::Undefined};
    CallState  callState = {CallState::Undefined};
    CallEnd    callEnd   = {CallEnd::Undefined};

    QByteArray friendPublicKey;            // Tox- Идентификатор друга
    quint32    friendNumber = quint32(-1); // Tox- Числовой идентификатор друга

    DECLARE_B_SERIALIZE_FUNC
};

struct FriendCallEndCause : Data<&command::FriendCallEndCause,
                                  Message::Type::Command>
{
    enum class CallEnd : quint32
    {
        Undefined    = 0,
        FriendEnd    = 1,  // Звонок удачно завершен другом
        FriendBusy   = 2,  // Друг находиться в состоянии звонка, то есть линия
                           // занята.
        FriendReject = 3,  // Друг отклонил входящий вызов
        Error        = 10  // В процессе звонка произошла какая-то ошибка
    };
    CallEnd callEnd = {CallEnd::Undefined};

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
                            Message::Type::Answer>
{
    // Признак активности механизма дивертера
    bool active = {true};

    // Признак подключенного устройства
    bool attached = {false};

    // Режим дивертера по умолчанию
    DiverterDefaultMode defaultMode = {DiverterDefaultMode::Pstn};

    // Текущий режим дивертера
    QString currentMode = {"Undefined"};

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
                            Message::Type::Answer>
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

struct ConfigAuthorizationRequest : Data<&command::ConfigAuthorizationRequest,
                                          Message::Type::Command,
                                          Message::Type::Answer>
{
    // Сессионный ключ
    QByteArray publicKey;

    // Требование пароля для авторизации, флаг используется в ответе
    bool needPassword = {false};

    DECLARE_B_SERIALIZE_FUNC
};

struct ConfigAuthorization : Data<&command::ConfigAuthorization,
                                   Message::Type::Command,
                                   Message::Type::Answer>
{
    QByteArray nonce;    // Разовый nonce
    QByteArray password; // Зашифрованный пароль

    DECLARE_B_SERIALIZE_FUNC
};

struct ConfigSavePassword : Data<&command::ConfigSavePassword,
                                  Message::Type::Command,
                                  Message::Type::Answer>
{
    QByteArray nonce;    // Разовый nonce
    QByteArray password; // Зашифрованный пароль

    DECLARE_B_SERIALIZE_FUNC
};

struct PlaybackFinish : Data<&command::PlaybackFinish,
                              Message::Type::Command>
{
    // Код звука, который был воспроизведен через playback-механизм
    enum class Code : quint32
    {
        Undefined = 0,
        Ringtone  = 1,
        Outgoing  = 2,
        Busy      = 3,
        Fail      = 4,
        Error     = 5,
        Test      = 6,
        Fake      = 10 // Используется в том случае, когда никакой звук
                       // не проигрывается, но сообщение PlaybackFinish
                       // все равно нужно отправить.
    };
    Code code = {Code::Undefined};

    DECLARE_B_SERIALIZE_FUNC
};

struct DiverterHandset : Data<&command::DiverterHandset,
                               Message::Type::Command>
{
    bool on = {false};
    DECLARE_B_SERIALIZE_FUNC
};


} // namespace data
} // namespace communication
