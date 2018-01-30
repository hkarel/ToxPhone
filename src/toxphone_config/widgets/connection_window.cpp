#include "connection_window.h"
#include "ui_connection_window.h"
#include "widgets/connection.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/transport/udp.h"
#include "shared/qt/config/config.h"
#include "kernel/communication/commands.h"
#include "kernel/network/interfaces.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <unistd.h>

#define PHONES_LIST_TIMEUPDATE 5

ConnectionWindow::ConnectionWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectionWindow)
{
    ui->setupUi(this);
    setWindowTitle(qApp->applicationName());
    setFixedSize(size());
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    //setWindowState(windowState() & ~(Qt::WindowMaximized | Qt::WindowFullScreen));
    //setWindowFlags(Qt::Dialog|Qt::WindowCloseButtonHint|Qt::MSWindowsFixedSizeDialogHint);

    chk_connect_q(&udp::socket(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    chk_connect_q(&_requestPhonesTimer, SIGNAL(timeout()),
                  this, SLOT(requestPhonesList()))
    _requestPhonesTimer.start(PHONES_LIST_TIMEUPDATE * 2 * 1000);

    chk_connect_q(&_updatePhonesTimer, SIGNAL(timeout()),
                  this, SLOT(updatePhonesList()))
    _updatePhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    ui->btnConnect->setEnabled(false);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ConnectionWindow::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ApplShutdown)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION

}

ConnectionWindow::~ConnectionWindow()
{
    delete ui;
}

bool ConnectionWindow::init(const tcp::Socket::Ptr& socket)
{
    _socket = socket;

    int port = 3609;
    if (!config::state().getValue("connection.port", port))
        config::state().setValue("connection.port", port);

    if (!udp::socket().init({QHostAddress::Any, port - 1}))
        return false;

    udp::socket().start();
    udp::socket().waitBinding(3);
    if (!udp::socket().isBound())
        return false;

    chk_connect_q(_socket.get(), SIGNAL(connected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(_socket.get(), SIGNAL(disconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    return (_init = true);
}

void ConnectionWindow::deinit()
{
    _init = false;
    udp::socket().stop();
    //_socket.disconnect();
}

void ConnectionWindow::saveGeometry()
{
//    QPoint p = pos();
//    std::vector<int> v {p.x(), p.y(), width(), height()};
//    config::state().setValue("windows.main_window.geometry", v);
}

void ConnectionWindow::loadGeometry()
{
//    std::vector<int> v {0, 0, 800, 600};
//    config::state().getValue("windows.main_window.geometry", v);
//    move(v[0], v[1]);
//    resize(v[2], v[3]);
}

void ConnectionWindow::message(const communication::Message::Ptr& message)
{
    if (_funcInvoker.containsCommand(message->command()))
        _funcInvoker.call(message);
}

void ConnectionWindow::socketConnected(communication::SocketDescriptor)
{
    hide();
}

void ConnectionWindow::socketDisconnected(communication::SocketDescriptor)
{
    if (_init)
        show();
}

void ConnectionWindow::on_btnConnect_clicked(bool checked)
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

    if (!_socket->init(cw->hostPoint()))
    {
        msg = tr("Failed initialize a communication system.\n"
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
    while (sleepCount++ < 12)
    {
        usleep(500*1000);
        qApp->processEvents();
        if (_socket->isConnected())
            break;
    }
    if (!_socket->isConnected())
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
    int port = 3609;
    config::state().getValue("connection.port", port);

    Message::Ptr message = createMessage(command::ToxPhoneInfo);
    network::Interface::List netInterfaces = network::getInterfaces();
    for (network::Interface* intf : netInterfaces)
        message->destinationPoints().insert({intf->broadcast, port});

    udp::socket().send(message);
}

void ConnectionWindow::updatePhonesList()
{
    for (int i = 0; i < ui->listPhones->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listPhones->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));
        if (cw->lifeTimeExpired())
        {
            --i;
            ui->listPhones->removeItemWidget(lwi);
            delete lwi;
        }
    }
    ui->btnConnect->setEnabled(ui->listPhones->count());
}

void ConnectionWindow::command_ToxPhoneInfo(const Message::Ptr& message)
{
    data::ToxPhoneInfo toxPhoneInfo;
    readFromMessage(message, toxPhoneInfo);

    bool found = false;
    for (int i = 0; i < ui->listPhones->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listPhones->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->listPhones->itemWidget(lwi));
        if (cw->applId() == toxPhoneInfo.applId)
        {
            cw->resetLifeTimer();
            if (cw->isPointToPoint() && !toxPhoneInfo.isPointToPoint)
            {
                cw->setHostPoint(message->sourcePoint());
                cw->setPointToPoint(false);
            }
            found = true;
            break;
        }
    }

    if (!found)
    {
        ConnectionWidget* cw = new ConnectionWidget();
        cw->setInfo(toxPhoneInfo.info);
        cw->setPointToPoint(toxPhoneInfo.isPointToPoint);
        cw->setApplId(toxPhoneInfo.applId);
        cw->setHostPoint(message->sourcePoint());
        cw->setLifeTimeInterval(PHONES_LIST_TIMEUPDATE * 2 + 5);
        cw->resetLifeTimer();

        QListWidgetItem* lwi = new QListWidgetItem();
        lwi->setSizeHint(cw->sizeHint());
        ui->listPhones->addItem(lwi);
        ui->listPhones->setItemWidget(lwi, cw);
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
        if (cw->applId() == applShutdown.applId)
        {
            ui->listPhones->removeItemWidget(lwi);
            delete lwi;
            break;
        }
    }
    ui->btnConnect->setEnabled(ui->listPhones->count());
}

void ConnectionWindow::closeEvent(QCloseEvent* event)
{
    qApp->exit();
    event->accept();
}
