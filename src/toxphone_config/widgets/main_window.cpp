#include "main_window.h"
#include "ui_main_window.h"
#include "widgets/connection.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/transport/udp.h"
#include "shared/qt/config/config.h"
#include "kernel/communication/commands.h"
#include "kernel/network/interfaces.h"

#include <QMessageBox>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <unistd.h>

#define PHONES_LIST_TIMEUPDATE 5

using namespace std;
using namespace communication;
using namespace communication::transport;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(qApp->applicationName());

    chk_connect_q(&_socket, SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))
    chk_connect_q(&_socket, SIGNAL(connected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(&_socket, SIGNAL(disconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    chk_connect_q(&udp::socket(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    chk_connect_q(&_requestPhonesTimer, SIGNAL(timeout()),
                  this, SLOT(requestPhonesList()))
    _requestPhonesTimer.start(PHONES_LIST_TIMEUPDATE * 2 * 1000);

    chk_connect_q(&_updatePhonesTimer, SIGNAL(timeout()),
                  this, SLOT(updatePhonesList()))
    _updatePhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    ui->btnConnect->setEnabled(false);

    _labelConnectStatus = new QLabel(tr("Disconnected"), this);
    ui->statusBar->addWidget(_labelConnectStatus);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &MainWindow::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ApplShutdown)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::init()
{
    int port = 3609;
    if (!config::state().getValue("connection.port", port))
        config::state().setValue("connection.port", port);

    if (!udp::socket().init({QHostAddress::Any, port - 1}))
        return false;

    udp::socket().start();
    udp::socket().waitBinding(3);
    if (!udp::socket().isBound())
        return false;


    return true;
}

void MainWindow::deinit()
{
    udp::socket().stop();
    _socket.disconnect();

}

void MainWindow::saveGeometry()
{
    QPoint p = pos();
    std::vector<int> v {p.x(), p.y(), width(), height()};
    config::state().setValue("windows.main_window.geometry", v);

//    QList<int> splitterSizes = ui->splitter->sizes();
//    config::state().setValue("windows.main_window.splitter_sizes", splitterSizes.toVector());
}

void MainWindow::loadGeometry()
{
    std::vector<int> v {0, 0, 800, 600};
    config::state().getValue("windows.main_window.geometry", v);
    move(v[0], v[1]);
    resize(v[2], v[3]);

//    QVector<int> splitterSizes;
//    if (config::state().getValue("windows.main_window.splitter_sizes", splitterSizes))
//        ui->splitter->setSizes(splitterSizes.toList());
}

void MainWindow::saveSettings()
{
}

void MainWindow::loadSettings()
{
}

void MainWindow::message(const communication::Message::Ptr& message)
{
    if (_funcInvoker.containsCommand(message->command()))
        _funcInvoker.call(message);
}

void MainWindow::socketConnected(communication::SocketDescriptor)
{
    ui->btnConnect->setText(tr("Disconnect"));
    QString msg = tr("Connected to %1:%2");
    _labelConnectStatus->setText(msg.arg(_socket.peerPoint().address().toString())
                                    .arg(_socket.peerPoint().port()));

    connectionWidgetIterator([this](ConnectionWidget* cw) {
        bool res = (cw->hostPoint() == this->_socket.peerPoint());
        if (res)
            cw->setConnectStatus(true);
        return res;
    });
}

void MainWindow::socketDisconnected(communication::SocketDescriptor)
{
    ui->btnConnect->setText(tr("Connect"));
    ui->btnConnect->setChecked(false);
    _labelConnectStatus->setText(tr("Disconnected"));

    connectionWidgetIterator([this](ConnectionWidget* cw) {
        bool res = (cw->hostPoint() == this->_socket.peerPoint());
        if (res)
            cw->setConnectStatus(false);
        return res;
    });
}

void MainWindow::on_btnConnect_clicked(bool checked)
{
    if (!checked)
    {
        _socket.disconnect();
        return;
    }

    QString msg;
    if (!ui->phonesWidget->count())
    {
        msg = tr("Connection list is empty");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        ui->btnConnect->setChecked(false);
        return;
    }
    if (!ui->phonesWidget->currentItem())
    {
        msg = tr("No connection selected");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        ui->btnConnect->setChecked(false);
        return;
    }

    QListWidgetItem* lwi = ui->phonesWidget->currentItem();
    ConnectionWidget* cw =
        qobject_cast<ConnectionWidget*>(ui->phonesWidget->itemWidget(lwi));

    if (!_socket.init(cw->hostPoint()))
    {
        msg = tr("Failed initialize a communication system.\n"
                 "See details in the log file.");
        QMessageBox::critical(this, qApp->applicationName(), msg);
        ui->btnConnect->setChecked(false);
        return;
    }

    setCursor(Qt::WaitCursor);
    msg = tr("Connection to %1:%2");
    _labelConnectStatus->setText(msg.arg(_socket.peerPoint().address().toString())
                                    .arg(_socket.peerPoint().port()));
    for (int i = 0; i < 3; ++i)
    {
        usleep(100*1000);
        qApp->processEvents();
    }

    _socket.connect();

    int sleepCount = 0;
    while (sleepCount++ < 12)
    {
        usleep(500*1000);
        qApp->processEvents();
        if (_socket.isConnected())
            break;
    }
    if (!_socket.isConnected())
    {
        _socket.stop();

        msg = tr("Failed connect to host %1:%2");
        msg = msg.arg(_socket.peerPoint().address().toString())
                 .arg(_socket.peerPoint().port());
        QMessageBox::critical(this, qApp->applicationName(), msg);

        _labelConnectStatus->setText(tr("Disconnected"));
        ui->btnConnect->setChecked(false);
    }
    setCursor(Qt::ArrowCursor);
}

void MainWindow::requestPhonesList()
{
    int port = 3609;
    config::state().getValue("connection.port", port);

    Message::Ptr message = createMessage(command::ToxPhoneInfo);
    network::Interface::List netInterfaces = network::getInterfaces();
    for (network::Interface* intf : netInterfaces)
        message->destinationPoints().insert({intf->broadcast, port});

    udp::socket().send(message);
}

void MainWindow::updatePhonesList()
{
    for (int i = 0; i < ui->phonesWidget->count(); ++i)
    {
        QListWidgetItem* lwi = ui->phonesWidget->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->phonesWidget->itemWidget(lwi));
        if (cw->lifeTimeExpired())
        {
            --i;
            ui->phonesWidget->removeItemWidget(lwi);
            delete lwi;
        }
    }
    ui->btnConnect->setEnabled(ui->phonesWidget->count());
}

void MainWindow::command_ToxPhoneInfo(const Message::Ptr& message)
{
    data::ToxPhoneInfo toxPhoneInfo;
    readFromMessage(message, toxPhoneInfo);

    bool found =
    connectionWidgetIterator([&](ConnectionWidget* cw) {
        bool res = (cw->applId() == toxPhoneInfo.applId);
        if (res)
        {
            cw->resetLifeTimer();
            if (cw->isPointToPoint() && !toxPhoneInfo.isPointToPoint)
            {
                cw->setHostPoint(message->sourcePoint());
                cw->setPointToPoint(false);
            }
        }
        return res;
    });

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
        ui->phonesWidget->addItem(lwi);
        ui->phonesWidget->setItemWidget(lwi, cw);
    }

    if (ui->phonesWidget->count())
    {
        ui->btnConnect->setEnabled(true);
        if (!ui->phonesWidget->currentItem())
            ui->phonesWidget->setCurrentRow(0);
    }
}

void MainWindow::command_ApplShutdown(const Message::Ptr& message)
{
    data::ApplShutdown applShutdown;
    readFromMessage(message, applShutdown);

    for (int i = 0; i < ui->phonesWidget->count(); ++i)
    {
        QListWidgetItem* lwi = ui->phonesWidget->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->phonesWidget->itemWidget(lwi));
        if (cw->applId() == applShutdown.applId)
        {
            ui->phonesWidget->removeItemWidget(lwi);
            delete lwi;
            break;
        }
    }
    ui->btnConnect->setEnabled(ui->phonesWidget->count());
}

bool MainWindow::connectionWidgetIterator(function<bool (ConnectionWidget*)> func)
{
    for (int i = 0; i < ui->phonesWidget->count(); ++i)
    {
        QListWidgetItem* lwi = ui->phonesWidget->item(i);
        ConnectionWidget* cw =
            qobject_cast<ConnectionWidget*>(ui->phonesWidget->itemWidget(lwi));
        if (func(cw))
            return true;
    }
    return false;
}
