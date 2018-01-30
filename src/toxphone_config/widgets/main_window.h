#pragma once

#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"

#include <QLabel>
#include <QMainWindow>

using namespace std;
using namespace communication;
using namespace communication::transport;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


    bool init(const tcp::Socket::Ptr&);
    void deinit();

    void saveGeometry();
    void loadGeometry();

    void saveSettings();
    void loadSettings();

private slots:
    void message(const communication::Message::Ptr&);
    void socketConnected(communication::SocketDescriptor);
    void socketDisconnected(communication::SocketDescriptor);

    void on_btnDisconnect_clicked(bool);
    void on_btnSaveProfile_clicked(bool);
    void on_btnRequestFriendship_clicked(bool);
    void on_btnFriendAccept_clicked(bool);
    void on_btnFriendReject_clicked(bool);
    void on_btnRemoveFriend_clicked(bool);

private:
    void closeEvent(QCloseEvent*) override;

    //--- Обработчики команд ---
    void command_ToxProfile(const Message::Ptr&);
    void command_RequestFriendship(const Message::Ptr&);
    void command_FriendRequest(const Message::Ptr&);
    void command_FriendRequests(const Message::Ptr&);
    void command_FriendItem(const Message::Ptr&);
    void command_FriendList(const Message::Ptr&);
    void command_DhtConnectStatus(const Message::Ptr&);

    void friendRequestAccept(bool accept);


private:
    Ui::MainWindow *ui;
    //QLabel* _labelConnectStatus;

    FunctionInvoker _funcInvoker;
    tcp::Socket::Ptr _socket;

    int _tabRrequestsIndex = {0};
};
