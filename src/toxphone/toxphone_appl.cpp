#include "toxphone_appl.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/compression/qlzma.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/base.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/communication/transport/udp.h"
#include "kernel/communication/commands.h"

#include <QNetworkAddressEntry>
#include <QNetworkInterface>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxPhoneAppl")

using namespace std;
using namespace communication;
using namespace communication::transport;

volatile bool ToxPhoneApplication::_stop = false;
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

void ToxPhoneApplication::stop2(int exitCode)
{
    _exitCode = exitCode;
    stop();
}

void ToxPhoneApplication::message(const Message::Ptr& message)
{
    if (_funcInvoker.containsCommand(message->command()))
        _funcInvoker.call(message);
}

void ToxPhoneApplication::socketConnected(SocketDescriptor socketDescriptor)
{
//    data::StatusVideoSave statusVideoSave;
//    statusVideoSave.active = videoSaver().isRunning();
//    Message::Ptr m = createMessage(statusVideoSave);
//    m->destinationSocketDescriptors().insert(socketDescriptor);
//    listenerSend(m);
}

void ToxPhoneApplication::socketDisconnected(SocketDescriptor socketDescriptor)
{
//    if (_videoSaverSocketDescriptor == socketDescriptor)
//        _videoSaverSocketDescriptor = 0;
}

void ToxPhoneApplication::sendInfo()
{
    int port = 3609;
    config::base().getValue("connection.port", port);

    QString info;
    config::base().getValue("connection.info", info);

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

void ToxPhoneApplication::command_ToxPhoneInfo(const Message::Ptr& message)
{
    if (_netInterfacesTimer.elapsed<chrono::seconds>() > 15)
    {
        network::Interface::List nl = network::getInterfaces();
        _netInterfaces.swap(nl);
        _netInterfacesTimer.reset();
    }

    QString info;
    config::base().getValue("connection.info", info);

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

//void QCoreApplicationM::command_GetWebcamParams(const Message::Ptr& message)
//{
//    // Запрос на получение параметров web-камеры
//    if (message->type() == Message::Type::Command)
//    {
//        Message::Ptr answer = message->cloneForAnswer();
//        if (videoCapture().source() == VideoSource::Undefined
//            || videoCapture().source() != VideoSource::Camera)
//        {
//            data::MessageError error;
//            error.description =
//                (videoCapture().source() == VideoSource::Undefined)
//                ? "Undefined current video device"
//                : "Current device is not a web-camera";
//            writeToMessage(error, answer);
//            listenerSend(answer);
//        }
//        else
//        {
//            answer->setPriority(Message::Priority::High);
//            listenerSend(answer);

//            if (WebcamParams::Ptr params = videoCapture().webcamParams())
//            {
//                data::WebcamParams webcamParams;
//                webcamParams.params = params;
//                Message::Ptr m = createMessage(webcamParams);
//                m->setPriority(Message::Priority::High);
//                m->destinationSocketDescriptors().insert(message->socketDescriptor());
//                listenerSend(m);
//            }
//        }
//    }
//}

//void QCoreApplicationM::command_ShowPolylineIntersect(const Message::Ptr& message)
//{
//    data::ShowPolylineIntersect showPolylineIntersect;
//    communication::readFromMessage(message, showPolylineIntersect);
//    if (showPolylineIntersect.isValid)
//    {
//        detectParams().setShowPolylineIntersect(showPolylineIntersect.value);

//        Message::Ptr answer = message->cloneForAnswer();
//        if (!detectParams().save())
//        {
//            data::MessageError error;
//            error.description = "An error occurred when saving a parameters";
//            writeToMessage(error, answer);
//        }
//        listenerSend(answer);

//        if (answer->execStatus() == Message::ExecStatus::Success)
//        {
//            data::ShowPolylineIntersectEvent showPolylineIntersectEvent;
//            showPolylineIntersectEvent.value = detectParams().showPolylineIntersect();
//            Message::Ptr event = createMessage(showPolylineIntersectEvent);
//            listenerSend(event, message->socketDescriptor());
//        }
//    }
//}

//void QCoreApplicationM::command_TokenKey(const Message::Ptr& message)
//{
//    if (message->type() == Message::Type::Command)
//    {
//        data::TokenKey tokenKey;
//        communication::readFromMessage(message, tokenKey);
//        if (tokenKey.isValid)
//        {
//            config::state().remove("statistics");
//            if (!tokenKey.value.trimmed().isEmpty())
//                config::state().setValue("statistics.token", tokenKey.value.trimmed());

//            Message::Ptr answer = message->cloneForAnswer();
//            if (!config::state().save())
//            {
//                data::MessageError error;
//                error.description = "An error occurred when saving a token key";
//                writeToMessage(error, answer);
//            }
//            listenerSend(answer);

//            if (answer->execStatus() == Message::ExecStatus::Success)
//            {
//                Message::Ptr event = createMessage(tokenKey, Message::Type::Event);
//                listenerSend(event, message->socketDescriptor());
//            }
//        }
//    }
//}



#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
