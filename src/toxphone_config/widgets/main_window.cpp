#include "main_window.h"
#include "ui_main_window.h"
#include "widgets/friend.h"
#include "widgets/friend_request.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "kernel/communication/commands.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->labelConnectStatus->clear();
    setWindowTitle(qApp->applicationName());

    for (int i = 0; i < ui->tabWidget->count(); ++i)
        if (ui->tabWidget->tabText(i).startsWith("Requests"))
        {
            _tabRrequestsIndex = i;
            break;
        }

    //_labelConnectStatus = new QLabel(tr("Disconnected"), this);
    //ui->statusBar->addWidget(_labelConnectStatus);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &MainWindow::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxProfile)
    FUNC_REGISTRATION(RequestFriendship)
    FUNC_REGISTRATION(FriendRequest)
    FUNC_REGISTRATION(FriendRequests)
    FUNC_REGISTRATION(FriendItem)
    FUNC_REGISTRATION(FriendList)
    FUNC_REGISTRATION(DhtConnectStatus)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION

}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::init(const tcp::Socket::Ptr& socket)
{
    _socket = socket;

    chk_connect_q(_socket.get(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))
    chk_connect_q(_socket.get(), SIGNAL(connected(communication::SocketDescriptor)),
                  this, SLOT(socketConnected(communication::SocketDescriptor)))
    chk_connect_q(_socket.get(), SIGNAL(disconnected(communication::SocketDescriptor)),
                  this, SLOT(socketDisconnected(communication::SocketDescriptor)))

    return true;
}

void MainWindow::deinit()
{
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
    QString msg = tr("Connected to %1 : %2");
    ui->labelConnectStatus->setText(msg.arg(_socket->peerPoint().address().toString())
                                       .arg(_socket->peerPoint().port()));
    show();
}

void MainWindow::socketDisconnected(communication::SocketDescriptor)
{
    hide();
    //_labelConnectStatus->setText(tr("Disconnected"));
    ui->labelConnectStatus->clear();
}

void MainWindow::command_ToxProfile(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Command)
    {
        data::ToxProfile toxProfile;
        readFromMessage(message, toxProfile);

        ui->lineSelfToxName->setText(toxProfile.name);
        ui->lineSelfToxStatus->setText(toxProfile.status);
        ui->lineSelfToxId->setText(toxProfile.toxId);
    }

    // Если Answer - значит мы отправляли команду на сервер с новыми значениями
    // имени и статуса пользователя, и получили ответ о выполнении этой операции.
    // См. функцию on_btnSaveProfile_clicked()
    else if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            QString msg = tr("Profile was successfully saved");
            QMessageBox::information(this, qApp->applicationName(), msg);
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void MainWindow::command_RequestFriendship(const Message::Ptr& message)
{
    // См. функцию on_btnRequestFriendship_clicked()
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            ui->lineRequestToxId->clear();
            ui->txtRequestMessage->clear();

            QString msg = tr("Friendship request has been sent successfully");
            QMessageBox::information(this, qApp->applicationName(), msg);
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void MainWindow::command_FriendRequest(const Message::Ptr& message)
{
    // См. функцию on_btnFriendAccept_clicked()
    // См. функцию on_btnFriendRejectp_clicked()
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            data::FriendRequest friendRequest;
            readFromMessage(message, friendRequest);

            if (friendRequest.accept == 1)
            {
                QString msg = tr("Friend was successfully added to friend list");
                QMessageBox::information(this, qApp->applicationName(), msg);
            }
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void MainWindow::command_FriendRequests(const Message::Ptr& message)
{
    data::FriendRequests friendRequests;
    readFromMessage(message, friendRequests);

    while (friendRequests.list.count() > ui->listFriendRequests->count())
    {
        FriendRequestWidget* frw = new FriendRequestWidget();
        QListWidgetItem* lwi = new QListWidgetItem();
        lwi->setSizeHint(frw->sizeHint());
        ui->listFriendRequests->addItem(lwi);
        ui->listFriendRequests->setItemWidget(lwi, frw);
    }
    while (friendRequests.list.count() < ui->listFriendRequests->count())
    {
        QListWidgetItem* lwi = ui->listFriendRequests->item(0);
        ui->listFriendRequests->removeItemWidget(lwi);
        delete lwi;
    }
    for (int i = 0; i < friendRequests.list.count(); ++i)
    {
        const data::FriendRequest& fr = friendRequests.list[i];
        QListWidgetItem* lwi = ui->listFriendRequests->item(i);
        FriendRequestWidget* frw =
            qobject_cast<FriendRequestWidget*>(ui->listFriendRequests->itemWidget(lwi));
        frw->setPublicKey(fr.publicKey);
        frw->setMessage(fr.message);
    }
    QString tabRrequestsText = tr("Requests (%1)");
    ui->tabWidget->setTabText(_tabRrequestsIndex, tabRrequestsText.arg(friendRequests.list.count()));
}

void MainWindow::command_FriendItem(const Message::Ptr& message)
{
    data::FriendItem friendItem;
    readFromMessage(message, friendItem);

    for (int i = 0; i < ui->listFriends->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listFriends->item(i);
        FriendWidget* fw = qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));
        if (fw->publicKey() == friendItem.publicKey)
        {
            switch (friendItem.changeFlaf)
            {
                case data::FriendItem::ChangeFlaf::Name:
                    fw->setName(friendItem.name);
                    break;
                case data::FriendItem::ChangeFlaf::StatusMessage:
                    fw->setStatusMessage(friendItem.statusMessage);
                    break;
                case data::FriendItem::ChangeFlaf::IsConnecnted:
                    fw->setConnectStatus(friendItem.isConnecnted);
                    break;
                default:
                    break;
            }
            fw->update();
            break;
        }
    }

}

void MainWindow::command_FriendList(const Message::Ptr& message)
{
    data::FriendList friendList;
    readFromMessage(message, friendList);

    while (friendList.list.count() > ui->listFriends->count())
    {
        FriendWidget* fw = new FriendWidget();
        QListWidgetItem* lwi = new QListWidgetItem();

        lwi->setSizeHint(fw->sizeHint());
        ui->listFriends->addItem(lwi);
        ui->listFriends->setItemWidget(lwi, fw);
    }
    while (friendList.list.count() < ui->listFriends->count())
    {
        QListWidgetItem* lwi = ui->listFriends->item(0);
        ui->listFriends->removeItemWidget(lwi);
        delete lwi;
    }
    for (int i = 0; i < friendList.list.count(); ++i)
    {
        const data::FriendItem& fi = friendList.list[i];
        QListWidgetItem* lwi = ui->listFriends->item(i);
        FriendWidget* fw = qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));
        fw->setName(fi.name);
        fw->setStatusMessage(fi.statusMessage);
        fw->setPublicKey(fi.publicKey);
        fw->setConnectStatus(fi.isConnecnted);
    }
}

void MainWindow::command_DhtConnectStatus(const Message::Ptr& message)
{
    data::DhtConnectStatus dhtConnectStatus;
    readFromMessage(message, dhtConnectStatus);

    if (dhtConnectStatus.active)
        ui->labelDhtConnecntStatus->setText(tr("Active"));
    else
        ui->labelDhtConnecntStatus->setText(tr("Inactive"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qApp->exit();
    event->accept();
}

void MainWindow::on_btnDisconnect_clicked(bool)
{
    _socket->disconnect();

}

void MainWindow::on_btnSaveProfile_clicked(bool)
{
    data::ToxProfile toxProfile;
    toxProfile.name = ui->lineSelfToxName->text();
    toxProfile.status = ui->lineSelfToxStatus->text();

    Message::Ptr message = createMessage(toxProfile);
    _socket->send(message);
}

void MainWindow::on_btnRequestFriendship_clicked(bool)
{
    data::RequestFriendship reqFriendship;
    reqFriendship.toxId = ui->lineRequestToxId->text().trimmed().toLatin1();
    reqFriendship.message = ui->txtRequestMessage->toPlainText();

    Message::Ptr message = createMessage(reqFriendship);
    _socket->send(message);

}

void MainWindow::friendRequestAccept(bool accept)
{
    if (!ui->listFriendRequests->count())
        return;

    if (!ui->listFriendRequests->currentItem())
        return;

    QListWidgetItem* lwi = ui->listFriendRequests->currentItem();
    FriendRequestWidget* frw =
        qobject_cast<FriendRequestWidget*>(ui->listFriendRequests->itemWidget(lwi));

    data::FriendRequest friendRequest;
    friendRequest.publicKey = frw->publicKey();
    friendRequest.accept = (accept) ? 1 : 0;

    Message::Ptr message = createMessage(friendRequest);
    _socket->send(message);
}

void MainWindow::on_btnFriendAccept_clicked(bool)
{
    friendRequestAccept(true);
}

void MainWindow::on_btnFriendReject_clicked(bool)
{
    friendRequestAccept(false);
}

void MainWindow::on_btnRemoveFriend_clicked(bool)
{
    if (!ui->listFriends->count())
        return;

    if (!ui->listFriends->currentItem())
        return;

    QListWidgetItem* lwi = ui->listFriends->currentItem();
    FriendWidget* fw =
        qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));

    data::RemoveFriend removeFriend;
    removeFriend.name = fw->name();
    removeFriend.publicKey = fw->publicKey();

    Message::Ptr message = createMessage(removeFriend);
    _socket->send(message);
}
