#include "connection_window.h"
#include "ui_connection_window.h"
#include "widgets/connection.h"
#include "widgets/list_widget_item.h"
#include "widgets/stub_widget.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"
#include "shared/qt/network/interfaces.h"

#include "pproto/commands/pool.h"
#include "pproto/transport/udp.h"

#include <sodium.h>
#include <QCloseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <unistd.h>

#define PHONES_LIST_TIMEUPDATE 5

ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);
    setWindowTitle(qApp->applicationName());
    //setFixedSize(size());
    //setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    //setWindowState(windowState() & ~(Qt::WindowMaximized | Qt::WindowFullScreen));
    //setWindowFlags(Qt::Dialog|Qt::WindowCloseButtonHint|Qt::MSWindowsFixedSizeDialogHint);

    chk_connect_q(&udp::socket(), &udp::Socket::message,
                  this, &ConnectionWindow::message)

    chk_connect_q(&_requestPhonesTimer, &QTimer::timeout,
                  this, &ConnectionWindow::requestPhonesList)
    _requestPhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    chk_connect_q(&_updatePhonesTimer, &QTimer::timeout,
                  this, &ConnectionWindow::updatePhonesList)
    _updatePhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    ui->btnConnect->setEnabled(false);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ConnectionWindow::command_##COMMAND, this);

    //FUNC_REGISTRATION(CloseConnection)
    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ApplShutdown)
    FUNC_REGISTRATION(ConfigAuthorizationRequest)
    FUNC_REGISTRATION(ConfigAuthorization)

    #undef FUNC_REGISTRATION

}

ConnectionWindow::~ConnectionWindow()
{
    delete ui;
}

bool ConnectionWindow::init(const tcp::Socket::Ptr& socket)
{
    _socket = socket;

    chk_connect_q(_socket.get(), &tcp::Socket::message,
                  this, &ConnectionWindow::message)

    chk_connect_q(_socket.get(), &tcp::Socket::connected,
                  this, &ConnectionWindow::socketConnected)

    chk_connect_q(_socket.get(), &tcp::Socket::disconnected,
                  this, &ConnectionWindow::socketDisconnected)

    StubWidget* sw = new StubWidget();
    ListWidgetItem* lwi = new ListWidgetItem(sw);
    lwi->setSizeHint(sw->minimumSize());
    ui->listPhones->addItem(lwi);
    ui->listPhones->setItemWidget(lwi, sw);
    //ui->listPhones->sortItems();

    int port = 33601;
    if (!config::state().getValue("connection.port", port))
        config::state().setValue("connection.port", port);

    int port_counter = 1;
    while (port_counter <= 5)
    {
        if (!udp::socket().init({QHostAddress::AnyIPv4, port + port_counter}))
            return false;

        udp::socket().start();
        //udp::socket().waitBinding(1);

        int attempts = 3;
        while (attempts--)
        {
            usleep(50*1000);
            if (udp::socket().isBound())
                break;
        }
        if (udp::socket().isBound())
            break;

        ++port_counter;
        udp::socket().stop();
    }
    if (port_counter > 5)
        log_error << "The number attempts of initialization UDP is exhausted";

    return true;
}

void ConnectionWindow::deinit()
{
    udp::socket().stop();
    //_socket.disconnect();
}

void ConnectionWindow::saveGeometry()
{
    QPoint p = pos();
    QVector<int> v {p.x(), p.y(), width(), height()};
    config::state().setValue("windows.connection_window.geometry", v);
}

void ConnectionWindow::loadGeometry()
{
    QVector<int> v {0, 0, 280, 350};
    config::state().getValue("windows.connection_window.geometry", v);

    move(v[0], v[1]);
    resize(v[2], v[3]);
}

void ConnectionWindow::message(const pproto::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (lst::FindResult fr = _funcInvoker.findCommand(message->command()))
    {
        if (command::pool().commandIsSinglproc(message->command()))
            message->markAsProcessed();
        _funcInvoker.call(message, fr);
    }
}

void ConnectionWindow::socketConnected(pproto::SocketDescriptor)
{
    //hide();
    //saveGeometry();
}

void ConnectionWindow::socketDisconnected(pproto::SocketDescriptor)
{
    show();
    loadGeometry();
}

void ConnectionWindow::on_btnConnect_clicked(bool /*checked*/)
{
    QString msg;
    if (!ui->listPhones->count())
    {
        msg = tr("Connection list is empty");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        return;
    }
    if (!ui->listPhones->currentItem())
    {
        msg = tr("No connection selected");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        return;
    }

    QListWidgetItem* lwi = ui->listPhones->currentItem();
    ConnectionWidget* cw =
        qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));

    if (!cw)
        return;

    if (cw->configConnectCount() != 0)
    {
        msg = tr("Tox a configurator is already connected to this client");
        QMessageBox::information(this, qApp->applicationName(), msg);
        return;
    }

    if (!_socket->init(cw->hostPoint()))
    {
        msg = tr("Failed initialize a pproto system.\n"
                 "See details in the log file.");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        return;
    }

    ui->btnConnect->setEnabled(false);
    setCursor(Qt::WaitCursor);
    for (int i = 0; i < 3; ++i)
    {
        usleep(100*1000);
        qApp->processEvents();
    }

    _socket->connect();

    int sleepCount = 0;
    while (sleepCount++ < 1200)
    {
        usleep(5*1000);
        qApp->processEvents();
        if (_socket->isConnected())
            break;
    }

    if (_socket->isConnected())
    {
        data::ConfigAuthorizationRequest configAuthorizationRequest;
        Message::Ptr m = createMessage(configAuthorizationRequest);
        _socket->send(m);
    }
    else
    {
        _socket->stop();
        msg = tr("Failed connect to host %1 : %2");
        msg = msg.arg(_socket->peerPoint().address().toString())
                 .arg(_socket->peerPoint().port());
        QMessageBox::critical(this, qApp->applicationName(), msg);
    }
    ui->btnConnect->setEnabled(true);
    setCursor(Qt::ArrowCursor);
}

void ConnectionWindow::requestPhonesList()
{
    int port = 33601;
    config::state().getValue("connection.port", port);

    network::Interface::List netInterfaces = network::getInterfaces();
    for (network::Interface* intf : netInterfaces)
    {
        if (intf->canBroadcast() && !intf->isPointToPoint())
        {
            Message::Ptr message = createMessage(command::ToxPhoneInfo);
            message->destinationPoints().insert({intf->broadcast(), port});
            udp::socket().send(message);
        }
        else if (intf->isPointToPoint() && (intf->subnetPrefixLength() == 24))
        {
            Message::Ptr message = createMessage(command::ToxPhoneInfo);
            union {
                quint8  ip4[4];
                quint32 ip4_val;
            };
            ip4_val = intf->subnet().toIPv4Address();
            for (quint8 i = 1; i < 255; ++i)
            {
                ip4[0] = i;
                QHostAddress addr {ip4_val};
                message->destinationPoints().insert({addr, port});
                if (message->destinationPoints().count() > 20)
                {
                    udp::socket().send(message);
                    message = createMessage(command::ToxPhoneInfo);
                    usleep(10);
                }
            }
            if (!message->destinationPoints().isEmpty())
                udp::socket().send(message);
        }
    }
}

void ConnectionWindow::updatePhonesList()
{
    for (int i = 0; i < ui->listPhones->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listPhones->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));
        if (cw && cw->lifeTimeExpired())
        {
            --i;
            ui->listPhones->removeItemWidget(lwi);
            delete lwi;
        }
    }
    ui->btnConnect->setEnabled(ui->listPhones->count());
}

//void ConnectionWindow::command_CloseConnection(const Message::Ptr& message)
//{
//    if (message->type() == Message::Type::Command)
//    {
//        data::CloseConnection closeConnection;
//        readFromMessage(message, closeConnection);

//        QString msg = tr("Connection is closed at the request of the client's Tox"
//                         ". Remote detail: ") + closeConnection.description;
//        QMessageBox::information(this, qApp->applicationName(), msg);
//    }
//}

void ConnectionWindow::command_ToxPhoneInfo(const Message::Ptr& message)
{
    if (message->socketType() != SocketType::Udp)
        return;

    data::ToxPhoneInfo toxPhoneInfo;
    readFromMessage(message, toxPhoneInfo);

    bool found = false;
    for (int i = 0; i < ui->listPhones->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listPhones->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));
        if (cw && (cw->applId() == toxPhoneInfo.applId))
        {
            cw->resetLifeTimer();
            cw->setInfo(toxPhoneInfo.info);
            cw->setConfigConnectCount(toxPhoneInfo.configConnectCount);
            if (cw->isPointToPoint() && !toxPhoneInfo.isPointToPoint)
            {
                cw->setHostPoint(toxPhoneInfo.hostPoint);
                cw->setPointToPoint(false);
            }
            ui->listPhones->sortItems();
            found = true;
            break;
        }
    }

    if (!found)
    {
        ConnectionWidget* cw = new ConnectionWidget();
        cw->setInfo(toxPhoneInfo.info);
        cw->setApplId(toxPhoneInfo.applId);
        cw->setHostPoint(toxPhoneInfo.hostPoint);
        cw->setPointToPoint(toxPhoneInfo.isPointToPoint);
        cw->setConfigConnectCount(toxPhoneInfo.configConnectCount);
        cw->setLifeTimeInterval(PHONES_LIST_TIMEUPDATE * 3 + 2);
        cw->resetLifeTimer();
        ListWidgetItem* lwi = new ListWidgetItem(cw);
        //QSize sz2 = cw->minimumSize();
        //lwi->setSizeHint(cw->minimumSize());
        //QSize sz = cw->sizeHint();
        lwi->setSizeHint(cw->sizeHint());
        ui->listPhones->addItem(lwi);
        ui->listPhones->setItemWidget(lwi, cw);
        ui->listPhones->sortItems();
    }

    if (ui->listPhones->count())
    {
        ui->btnConnect->setEnabled(true);
        if (!ui->listPhones->currentItem())
            ui->listPhones->setCurrentRow(0);
    }
}

void ConnectionWindow::command_ApplShutdown(const Message::Ptr& message)
{
    data::ApplShutdown applShutdown;
    readFromMessage(message, applShutdown);

    for (int i = 0; i < ui->listPhones->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listPhones->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));
        if (cw && (cw->applId() == applShutdown.applId))
        {
            ui->listPhones->removeItemWidget(lwi);
            delete lwi;
            break;
        }
    }
    ui->btnConnect->setEnabled(ui->listPhones->count());
}

void ConnectionWindow::command_ConfigAuthorizationRequest(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            data::ConfigAuthorizationRequest configAuthorizationRequest;
            readFromMessage(message, configAuthorizationRequest);

            data::ConfigAuthorization configAuthorization;
            if (configAuthorizationRequest.needPassword)
            {
                // Отправляем пароль
                bool ok;
                QString passw = QInputDialog::getText(this, qApp->applicationName(),
                                                      tr("Password:"), QLineEdit::Password,
                                                      "", &ok).toUtf8().trimmed();
                if (!ok)
                {
                    _socket->stop();
                    return;
                }
                configAuthorization.password = passw;
            }
            Message::Ptr m = createMessage(configAuthorization);
            _socket->send(m);
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void ConnectionWindow::command_ConfigAuthorization(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            saveGeometry();
            hide();
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void ConnectionWindow::closeEvent(QCloseEvent* event)
{
    qApp->exit();
    event->accept();
}
