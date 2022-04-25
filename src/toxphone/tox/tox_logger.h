#pragma once

#include "shared/logger/logger.h"
#include "toxcore/tox.h"

/**
  Вспомогательная структура, используется для отправки в лог идентификатора
  имени tox-пользователя
*/
struct ToxFriendLog
{
    const Tox* tox;
    const uint32_t friendNumber;
    bool withoutKey;
    ToxFriendLog(Tox* tox, uint32_t friendNumber, bool withoutKey = true)
        : tox(tox),
          friendNumber(friendNumber),
          withoutKey(withoutKey)
    {}
};

namespace alog {

Line& operator<< (Line& line, const ToxFriendLog&);

} // namespace alog

