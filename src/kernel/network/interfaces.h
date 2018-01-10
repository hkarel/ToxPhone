#pragma once

#include "shared/_list.h"
#include "shared/defmac.h"
#include "shared/clife_base.h"
#include "shared/clife_alloc.h"
#include "shared/clife_ptr.h"

#include <QtCore>
#include <QHostAddress>
#include <QNetworkReply>
#include <QNetworkAccessManager>

namespace network {

/**
  Структура содержит совокупные характеристики сетевого интерфейса
*/
struct Interface : public clife_base
{
    typedef clife_ptr<Interface> Ptr;

    Interface();

    // Адрес сетевого интерфейса
    QHostAddress ip;

    // Адрес сегмента сети
    QHostAddress subnet;

    // Широковещательный адрес сетевого интерфейса
    QHostAddress broadcast;

    // Длина маски подсети
    int	subnetPrefixLength = {0};

    // Наименование сетевого интерфейса
    QString name;

    // Флаги ассоциированные с сетевым интерфейсом
    quint32 flags = {0};

    // Интерфейс может работать в broadcast режиме
    bool canBroadcast() const;

    // Интерфейс работает в режиме point-to-point
    bool isPointToPoint() const;

    typedef lst::List<Interface, lst::CompareItemDummy, clife_alloc<Interface>> List;
};

// Возвращает список сетевых интерфейсов
Interface::List getInterfaces();

} // namespace network
