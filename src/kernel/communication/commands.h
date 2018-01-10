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


} // namespace data
} // namespace communication


