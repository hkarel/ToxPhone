#pragma once

#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"
#include "shared/qt/thread/qthreadex.h"

#include <QLabel>
#include <QMainWindow>
#include <functional>

class ConnectionWidget;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool init();
    void deinit();

    void saveGeometry();
    void loadGeometry();

    void saveSettings();
    void loadSettings();

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
    void command_ToxPhoneInfo(const communication::Message::Ptr&);
    void command_ApplShutdown(const communication::Message::Ptr&);

    bool connectionWidgetIterator(std::function<bool (ConnectionWidget*)>);

private:
    Ui::MainWindow *ui;
    QLabel* _labelConnectStatus;

    communication::FunctionInvoker _funcInvoker;
    communication::transport::tcp::Socket _socket;

    QTimer _requestPhonesTimer;
    QTimer _updatePhonesTimer;
};
