#include "interfaces.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"

#include <QNetworkAddressEntry>
#include <QNetworkInterface>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "NetIntf")

namespace network {

Interface::Interface()
{}

bool Interface::canBroadcast() const
{
    return (flags & QNetworkInterface::CanBroadcast);
}

bool Interface::isPointToPoint() const
{
    return (flags & QNetworkInterface::IsPointToPoint);
}

Interface::List getInterfaces()
{
    Interface::List interfaces;
    for (const QNetworkInterface& netIntf : QNetworkInterface::allInterfaces())
    {
        if (netIntf.flags() & QNetworkInterface::IsLoopBack)
            continue;

        if (netIntf.flags() & QNetworkInterface::IsRunning)
            for (const QNetworkAddressEntry& entry : netIntf.addressEntries())
            {
                if (entry.broadcast() == QHostAddress::Null)
                    continue;

                Interface* intf = interfaces.add();
                intf->ip = entry.ip();
                intf->broadcast = entry.broadcast();
                intf->name = netIntf.name();
                intf->flags = netIntf.flags();

                // Дебильный способ получения адреса сети, но к сожалению
                // Qt API другой возможности не предоставляет.
                QString subnetStr = entry.ip().toString()
                                    + QString("/%1").arg(entry.prefixLength());
                QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(subnetStr);
                intf->subnet = subnet.first;
                intf->subnetPrefixLength = entry.prefixLength();

                if (alog::logger().level() == alog::Level::Debug2)
                {
                    log_debug2_m << "Found interface: " << intf->name
                                 << "; ip: " << intf->ip
                                 << "; subnet: " << intf->subnet
                                 << "; broadcast: " << intf->broadcast
                                 << "; prefix length: " << intf->subnetPrefixLength
                                 << "; flags: " << intf->flags
                                 << "; canBroadcast: " << intf->canBroadcast()
                                 << "; isPointToPoint: " << intf->isPointToPoint();
                }
            }
    }
    return std::move(interfaces);
}

} // namespace network

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
