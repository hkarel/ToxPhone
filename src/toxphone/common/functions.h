#pragma once

#include "shared/safe_singleton.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/transport/base.h"

#include <QtCore>

struct ToxConfig
{
    bool isActive() const {return !socket.empty();}
    void send(const communication::Message::Ptr& message) const;
    void reset();

    communication::SocketDescriptor socketDescriptor = {-1};
    communication::transport::base::Socket::Ptr socket;

    template<typename T, int> friend T& ::safe_singleton();
};
ToxConfig& toxConfig();

// Возвращает TRUE когда дивертер активирован
bool diverterIsActive(bool* = 0);

// Строит путь до различных ресурсов
QString getFilePath(const QString& fileName);
