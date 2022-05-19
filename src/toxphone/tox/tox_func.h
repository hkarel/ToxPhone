#pragma once

#include "pproto/message.h"
#include "toxcore/tox.h"
#include <QtCore>

// Отправляет сообщение Message через tox-механизм пользовательских сообщений
bool sendToxLosslessMessage(Tox* tox, uint32_t friendNumber,
                            const pproto::Message::Ptr&);

// Читает сообщение Message из tox-механизма пользовательских сообщений
const pproto::Message::Ptr readToxMessage(Tox* tox, uint32_t friendNumber,
                                          const uint8_t* data, size_t length);
