/*****************************************************************************
  В модуле представлен список идентификаторов команд для коммуникации между
  клиентской и серверной частями приложения.
  В данном модуле представлен список команд персональный для этого приложения.

  Требование надежности коммуникаций: однажды назначенный идентификатор коман-
  ды не должен более меняться.

*****************************************************************************/

#pragma once

#include "shared/_list.h"
#include "shared/clife_base.h"
#include "shared/clife_ptr.h"
#include "shared/qt/quuidex.h"
#include "shared/qt/communication/commands_base.h"
#include "shared/container_ptr.h"
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
    enum ChangeFlaf : quint32
    {
        None          = 0x00,
        Name          = 0x01,
        StatusMessage = 0x02,
        IsConnecnted  = 0x04
    };

    QByteArray publicKey;
    ChangeFlaf changeFlaf = {None}; // Флаг определяет какое поле нужно обновить
    QString    name;                // Имя друга
    QString    statusMessage;       // Статус-сообщение
    bool       isConnecnted;        // Признак подключения к сети

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




} // namespace data
} // namespace communication


