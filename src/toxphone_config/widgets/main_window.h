#pragma once

#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"
#include "kernel/communication/commands.h"

#include <QLabel>
#include <QSlider>
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
    void on_btnAudioTest_clicked(bool);
    void on_btnCall_clicked(bool);
    void on_btnEndCall_clicked(bool);

    void on_cboxAudioSink_currentIndexChanged(int index);
    void on_cboxAudioSource_currentIndexChanged(int index);
    //void on_cboxAudioSink_activated(int index);
    //void on_cboxAudioSource_activated(int index);

    void on_sliderAudioSink_sliderReleased();
    void on_sliderAudioSource_sliderReleased();

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
    void command_AudioDev(const Message::Ptr&);
    void command_AudioDevList(const Message::Ptr&);
    void command_AudioSourceLevel(const Message::Ptr&);
    void command_ToxCallAction(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);

    void friendRequestAccept(bool accept);
    void setSliderLevel(QSlider* slider, int base, int current, int max);

private:
    Ui::MainWindow *ui;
    //QLabel* _labelConnectStatus;

    FunctionInvoker _funcInvoker;
    tcp::Socket::Ptr _socket;

    data::ToxCallState _callState;

//    // Вспомогательная структура, используется для приема информации
//    // об уровне сигнала микрофона в конфигуратор
//    union  {
//        quint64 _audioSourceTag;
//        struct {
//            quint32 time;      // Время необходимое на воспроизведение
//            quint16 average;   // Усредненное значение уровня звукового потока
//            quint16 reserved;
//        } _audioSourceLevel;
//    } ;

    int _tabRrequestsIndex = {0};
};
