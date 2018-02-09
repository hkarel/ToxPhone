#include "toxphone_appl.h"
#include "tox/tox_net.h"
#include "tox/tox_call.h"
#include "audio/audio_dev.h"
#include "common/functions.h"
#include "kernel/communication/commands.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/base.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/communication/transport/udp.h"

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")

volatile bool ToxPhoneApplication::_stop = false;
std::atomic_int ToxPhoneApplication::_exitCode = {0};
QUuidEx ToxPhoneApplication::_applId = QUuidEx::createUuid();

ToxPhoneApplication::ToxPhoneApplication(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    _stopTimerId = startTimer(1000);

    chk_connect_q(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))
    chk_connect_q(&tcp::listener(), SIGNAL(socketConnected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(&tcp::listener(), SIGNAL(socketDisconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    chk_connect_q(&udp::socket(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    chk_connect_q(&audioDev(), SIGNAL(sourceLevel(quint32, quint32)),
                  this, SLOT(audioSourceLevel(quint32, quint32)))

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ToxPhoneApplication::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxPhoneInfo)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

void ToxPhoneApplication::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == _stopTimerId)
    {
        if (_stop)
            exit(_exitCode);
    }
}

void ToxPhoneApplication::stop(int exitCode)
{
    _exitCode = exitCode;
    stop();
}

void ToxPhoneApplication::message(const Message::Ptr& message)
{
    if (message->processed())
        return;

    if (_funcInvoker.containsCommand(message->command()))
    {
        message->markAsProcessed();
        _funcInvoker.call(message);
    }
}

void ToxPhoneApplication::socketConnected(SocketDescriptor socketDescriptor)
{
    bool connected = true;
    configConnected(&connected);

    Message::Ptr message = createMessage(command::IncomingConfigConnection);
    message->setTag(quint64(socketDescriptor));
    toxNet().message(message);
    toxCall().message(message);

    QMetaObject::invokeMethod(&audioDev(), "message", Qt::QueuedConnection,
                              Q_ARG(communication::Message::Ptr, message));
}

void ToxPhoneApplication::socketDisconnected(SocketDescriptor /*socketDescriptor*/)
{
    bool connected = false;
    configConnected(&connected);
}

void ToxPhoneApplication::sendToxPhoneInfo()
{
    int port = 3609;
    config::base().getValue("config_connection.port", port);

    QString info;
    config::base().getValue("config_connection.info", info);

    network::Interface::List nl = network::getInterfaces();
    _netInterfaces.swap(nl);
    _netInterfacesTimer.reset();

    for (network::Interface* intf : _netInterfaces)
    {
        data::ToxPhoneInfo toxPhoneInfo;
        toxPhoneInfo.info = info;
        toxPhoneInfo.applId = _applId;
        toxPhoneInfo.isPointToPoint = intf->isPointToPoint();

        Message::Ptr message = createMessage(toxPhoneInfo);
        message->destinationPoints().insert({intf->broadcast, port - 1});
        udp::socket().send(message);
    }
}

void ToxPhoneApplication::audioSourceLevel(quint32 averageLevel, quint32 time)
{
    if (!configConnected())
        return;

    data::AudioSourceLevel audioSourceLevel;
    audioSourceLevel.average = averageLevel;
    audioSourceLevel.time = time;

    Message::Ptr m = createMessage(audioSourceLevel, Message::Type::Event);
    m->setPriority(Message::Priority::High);
    tcp::listener().send(m);
}

void ToxPhoneApplication::command_ToxPhoneInfo(const Message::Ptr& message)
{
    if (_netInterfacesTimer.elapsed<chrono::seconds>() > 15)
    {
        network::Interface::List nl = network::getInterfaces();
        _netInterfaces.swap(nl);
        _netInterfacesTimer.reset();
    }

    QString info;
    config::base().getValue("config_connection.info", info);

    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = info;
    toxPhoneInfo.applId = _applId;
    for (network::Interface* intf : _netInterfaces)
        if (message->sourcePoint().address().isInSubnet(intf->subnet, intf->subnetPrefixLength))
        {
            toxPhoneInfo.isPointToPoint = intf->isPointToPoint();
            break;
        }

    Message::Ptr answer = message->cloneForAnswer();
    writeToMessage(toxPhoneInfo, answer);
    udp::socket().send(answer);
}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
