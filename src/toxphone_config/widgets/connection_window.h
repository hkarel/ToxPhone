#pragma once

#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"

#include <QDialog>
#include <functional>

using namespace std;
using namespace communication;
using namespace communication::transport;

class ConnectionWidget;

namespace Ui {
class ConnectionWindow;
}

class ConnectionWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionWindow(QWidget *parent = 0);
    ~ConnectionWindow();

    bool init(const tcp::Socket::Ptr&);
    void deinit();

    void saveGeometry();
    void loadGeometry();

public slots:
    void requestPhonesList();
    void updatePhonesList();

private slots:
    void message(const communication::Message::Ptr&);
    void socketConnected(communication::SocketDescriptor);
    void socketDisconnected(communication::SocketDescriptor);

    void on_btnConnect_clicked(bool checked);

private:
    //--- Обработчики команд ---
    void command_ToxPhoneInfo(const Message::Ptr&);
    void command_ApplShutdown(const Message::Ptr&);

    void closeEvent(QCloseEvent*) override;

private:
    Ui::ConnectionWindow *ui;

    bool _init = {false};
    FunctionInvoker _funcInvoker;
    tcp::Socket::Ptr _socket;

    QTimer _requestPhonesTimer;
    QTimer _updatePhonesTimer;
};
