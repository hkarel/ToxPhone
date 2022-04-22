#pragma once

#include "commands/commands.h"
#include "commands/error.h"

#include "pproto/func_invoker.h"
#include "pproto/transport/tcp.h"

#include <QDialog>
#include <functional>

using namespace std;
using namespace pproto;
using namespace pproto::transport;

class ConnectionWidget;

namespace Ui {
class ConnectionWindow;
}

class ConnectionWindow : public QDialog
{
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
    void message(const pproto::Message::Ptr&);
    void socketConnected(pproto::SocketDescriptor);
    void socketDisconnected(pproto::SocketDescriptor);

    void on_btnConnect_clicked(bool checked);

private:
    Q_OBJECT

    //--- Обработчики команд ---
    //void command_CloseConnection(const Message::Ptr&);
    void command_ToxPhoneInfo(const Message::Ptr&);
    void command_ApplShutdown(const Message::Ptr&);
    void command_ConfigAuthorizationRequest(const Message::Ptr&);
    void command_ConfigAuthorization(const Message::Ptr&);

    void closeEvent(QCloseEvent*) override;

private:
    Ui::ConnectionWindow *ui;

    FunctionInvoker _funcInvoker;
    tcp::Socket::Ptr _socket;

    QTimer _requestPhonesTimer;
    QTimer _updatePhonesTimer;
};
