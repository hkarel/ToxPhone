#pragma once

#include "pproto/message.h"
#include "pproto/transport/base.h"
#include <QtCore>

struct ToxConfig
{
    bool isActive() const {return !socket.empty();}
    void send(const pproto::Message::Ptr& message) const;
    void reset();

    pproto::SocketDescriptor socketDescriptor = {-1};
    pproto::transport::base::Socket::Ptr socket;
};
ToxConfig& toxConfig();

// Возвращает TRUE когда дивертер активирован
bool diverterIsActive(bool* = 0);

// Строит путь до различных ресурсов
QString getFilePath(const QString& fileName);
