#pragma once

#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"
#include "kernel/communication/commands.h"

#include <QLabel>
#include <QSlider>
#include <QListWidget>
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
    void on_btnSaveFiendPhone_clicked(bool);
    void on_btnCall_clicked(bool);
    void on_btnEndCall_clicked(bool);

    void on_splitterFriends_splitterMoved(int pos, int index);
    void on_listFriends_itemClicked(QListWidgetItem* item);

    void on_btnPlaybackTest_clicked(bool);
    void on_btnRecordTest_clicked(bool);

    void on_cboxAudioPlayback_currentIndexChanged(int index);
    void on_cboxAudioRecord_currentIndexChanged(int index);

    void on_btnPlaybackAsDefault_clicked(bool);
    void on_btnRecordAsDefault_clicked(bool);

    void on_sliderAudioPlayback_sliderReleased();
    void on_sliderAudioRecord_sliderReleased();

    void on_sliderStreamPlayback_sliderReleased();
    void on_sliderStreamVoice_sliderReleased();
    void on_sliderStreamRecord_sliderReleased();

    void on_cboxUseDiverter_stateChanged(int state);

    void on_rbtnDiverterPSTN_clicked(bool);
    void on_rbtnDiverterUSB_clicked(bool);
    void on_btnSavePhoneRingtone_clicked(bool);
    void on_btnTestPhoneRingtone_clicked(bool);

    void on_btnSaveToxInfo_clicked(bool);

    void on_labelCopyright_linkActivated(const QString& link);

private:
    void closeEvent(QCloseEvent*) override;

    //--- Обработчики команд ---
    void command_ToxPhoneInfo(const Message::Ptr&);
    void command_ToxPhoneAbout(const Message::Ptr&);
    void command_ToxProfile(const Message::Ptr&);
    void command_RequestFriendship(const Message::Ptr&);
    void command_FriendRequest(const Message::Ptr&);
    void command_FriendRequests(const Message::Ptr&);
    void command_FriendItem(const Message::Ptr&);
    void command_FriendList(const Message::Ptr&);
    void command_RemoveFriend(const Message::Ptr&);
    void command_PhoneFriendInfo(const Message::Ptr&);
    void command_DhtConnectStatus(const Message::Ptr&);
    void command_AudioDevInfo(const Message::Ptr&);
    void command_AudioDevChange(const Message::Ptr&);
    void command_AudioStreamInfo(const Message::Ptr&);
    void command_AudioTest(const Message::Ptr&);
    void command_AudioRecordLevel(const Message::Ptr&);
    void command_ToxCallAction(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);
    void command_DiverterInfo(const Message::Ptr&);
    void command_DiverterChange(const Message::Ptr&);
    void command_DiverterTest(const Message::Ptr&);

    void friendRequestAccept(bool accept);
    void setSliderLevel(QSlider* slider, int base, int current, int max);
    QString friendCalling(quint32 friendNumber);

    void aboutClear();

private:
    Ui::MainWindow *ui;
    //QLabel* _labelConnectStatus;

    FunctionInvoker _funcInvoker;
    tcp::Socket::Ptr _socket;

    data::ToxCallState _callState;

    data::AudioDevInfo::List _sinkDevices;
    data::AudioDevInfo::List _sourceDevices;


    int _tabRrequestsIndex = {0};
};
