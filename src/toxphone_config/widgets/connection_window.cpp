#include "connection_window.h"
#include "ui_connection_window.h"
#include "widgets/connection.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/transport/udp.h"
#include "shared/qt/config/config.h"
#include "kernel/network/interfaces.h"

#include <sodium.h>
#include <QCloseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <unistd.h>

// Ключи для авторизации конфигуратора
extern uchar configPublicKey[crypto_box_PUBLICKEYBYTES];
extern uchar configSecretKey[crypto_box_SECRETKEYBYTES];

// Сессионный публичный ключ Tox-клиента
extern uchar toxPublicKey[crypto_box_PUBLICKEYBYTES];

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
    _requestPhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    chk_connect_q(&_updatePhonesTimer, SIGNAL(timeout()),
                  this, SLOT(updatePhonesList()))
    _updatePhonesTimer.start(PHONES_LIST_TIMEUPDATE * 1000);

    ui->btnConnect->setEnabled(false);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ConnectionWindow::command_##COMMAND, this);

    //FUNC_REGISTRATION(CloseConnection)
    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ApplShutdown)
    FUNC_REGISTRATION(ConfigAuthorizationRequest)
    FUNC_REGISTRATION(ConfigAuthorization)
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

    chk_connect_q(_socket.get(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))
    chk_connect_q(_socket.get(), SIGNAL(connected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(_socket.get(), SIGNAL(disconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    int port = 33601;
    if (!config::state().getValue("connection.port", port))
        config::state().setValue("connection.port", port);

    int port_counter = 1;
    while (port_counter <= 5)
    {
        if (!udp::socket().init({QHostAddress::Any, port + port_counter}))
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
        log_error << "The number of attempts of initialization of UDP is exhausted";

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
    std::vector<int> v {p.x(), p.y()};
    config::state().setValue("windows.connection_window.geometry", v);
}

void ConnectionWindow::loadGeometry()
{
    std::vector<int> v;
    if (config::state().getValue("windows.connection_window.geometry", v))
        move(v[0], v[1]);
}

void ConnectionWindow::message(const communication::Message::Ptr& message)
{
    if (_funcInvoker.containsCommand(message->command()))
        _funcInvoker.call(message);
}

void ConnectionWindow::socketConnected(communication::SocketDescriptor)
{
    //hide();
    //saveGeometry();
}

void ConnectionWindow::socketDisconnected(communication::SocketDescriptor)
{
    sodium_memzero(configPublicKey, crypto_box_PUBLICKEYBYTES);
    sodium_memzero(configSecretKey, crypto_box_SECRETKEYBYTES);
    sodium_memzero(toxPublicKey, crypto_box_PUBLICKEYBYTES);

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

    if (cw->configConnectCount() != 0)
    {
        msg = tr("Tox a configurator is already connected to this client");
        QMessageBox::information(this, qApp->applicationName(), msg);
        return;
    }

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
    while (sleepCount++ < 1200)
    {
        usleep(5*1000);
        qApp->processEvents();
        if (_socket->isConnected())
            break;
    }

    if (_socket->isConnected())
    {
        crypto_box_keypair(configPublicKey, configSecretKey);

        data::ConfigAuthorizationRequest configAuthorizationRequest;
        configAuthorizationRequest.publicKey =
            QByteArray((char*)configPublicKey, crypto_box_PUBLICKEYBYTES);

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
            message->destinationPoints().insert({intf->broadcast, port});
            udp::socket().send(message);
        }
        else if (intf->isPointToPoint() && (intf->subnetPrefixLength == 24))
        {
            Message::Ptr message = createMessage(command::ToxPhoneInfo);
            union {
                quint8  ip4[4];
                quint32 ip4_val;
            };
            ip4_val = intf->subnet.toIPv4Address();
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
        if (cw->lifeTimeExpired())
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
        if (cw->applId() == toxPhoneInfo.applId)
        {
            cw->resetLifeTimer();
            cw->setInfo(toxPhoneInfo.info);
            cw->setConfigConnectCount(toxPhoneInfo.configConnectCount);
            if (cw->isPointToPoint() && !toxPhoneInfo.isPointToPoint)
            {
                cw->setHostPoint(toxPhoneInfo.hostPoint);
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
        cw->setApplId(toxPhoneInfo.applId);
        cw->setHostPoint(toxPhoneInfo.hostPoint);
        cw->setPointToPoint(toxPhoneInfo.isPointToPoint);
        cw->setConfigConnectCount(toxPhoneInfo.configConnectCount);
        cw->setLifeTimeInterval(PHONES_LIST_TIMEUPDATE * 3 + 2);
        cw->resetLifeTimer();
        //QSize sz = cw->sizeHint();
        QListWidgetItem* lwi = new QListWidgetItem();
        lwi->setSizeHint(cw->minimumSize());
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

void ConnectionWindow::command_ConfigAuthorizationRequest(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            data::ConfigAuthorizationRequest configAuthorizationRequest;
            readFromMessage(message, configAuthorizationRequest);

            memcpy(toxPublicKey, configAuthorizationRequest.publicKey.constData(),
                   crypto_box_PUBLICKEYBYTES);

            data::ConfigAuthorization configAuthorization;
            if (configAuthorizationRequest.needPassword)
            {
                // Отправляем пароль
                bool ok;
                QByteArray passw = QInputDialog::getText(this, qApp->applicationName(),
                                                         tr("Password:"), QLineEdit::Password,
                                                         "", &ok).toUtf8().trimmed();
                if (!ok)
                {
                    _socket->stop();
                    return;
                }

                QByteArray passwBuff;
                {
                    QDataStream s {&passwBuff, QIODevice::WriteOnly};
                    s.setVersion(Q_DATA_STREAM_VERSION);

                    QByteArray garbage; garbage.resize(512 - passw.length() - 2 * sizeof(int));
                    randombytes((uchar*)garbage.constData(), garbage.length());

                    s << passw;
                    s << garbage;
                }
                QByteArray nonce; nonce.resize(crypto_box_NONCEBYTES);
                randombytes((uchar*)nonce.constData(), crypto_box_NONCEBYTES);

                QByteArray ciphertext;
                ciphertext.resize(passwBuff.length() + crypto_box_MACBYTES);

                int res = crypto_box_easy((uchar*)ciphertext.constData(),
                                          (uchar*)passwBuff.constData(), passwBuff.length(),
                                          (uchar*)nonce.constData(), toxPublicKey, configSecretKey);
                if (res != 0)
                {
                    _socket->stop();
                    QString msg = tr("Failed encript password");
                    QMessageBox::critical(this, qApp->applicationName(), msg);
                    return;
                }
                configAuthorization.nonce = nonce;
                configAuthorization.password = ciphertext;
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
