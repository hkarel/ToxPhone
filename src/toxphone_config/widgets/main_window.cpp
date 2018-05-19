#include "main_window.h"
#include "ui_main_window.h"
#include "widgets/friend.h"
#include "widgets/friend_request.h"

#include "shared/defmac.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/version/version_number.h"
#include "shared/qt/config/config.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <limits>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->labelConnectStatus->clear();
    setWindowTitle(qApp->applicationName());

    ui->tabWidget->setCurrentIndex(0);

    for (int i = 0; i < ui->tabWidget->count(); ++i)
        if (ui->tabWidget->tabText(i).startsWith("Requests"))
        {
            _tabRrequestsIndex = i;
            break;
        }

    ui->labelCallState->clear();
    ui->widgetFriends->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    ui->pbarAudioRecord->setMinimum(0);
    ui->pbarAudioRecord->setMaximum(std::numeric_limits<quint16>::max() / 2);
    ui->pbarAudioRecord->setValue(0);

    aboutClear();
    ui->labelToxPhoneConfVers->setText(productVersion().toString());
    ui->labelGitrevConf->setText(GIT_REVISION);
    ui->labelBprotocolConfVers->setText(QString("%1-%2").arg(BPROTOCOL_VERSION_LOW)
                                                        .arg(BPROTOCOL_VERSION_HIGH));
    ui->labelQtVersionConf->setText(QT_VERSION_STR);
    ui->labelCopyright->setTextInteractionFlags(Qt::TextBrowserInteraction);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &MainWindow::command_##COMMAND, this);

    FUNC_REGISTRATION(ToxPhoneInfo)
    FUNC_REGISTRATION(ToxPhoneAbout)
    FUNC_REGISTRATION(ToxProfile)
    FUNC_REGISTRATION(RequestFriendship)
    FUNC_REGISTRATION(FriendRequest)
    FUNC_REGISTRATION(FriendRequests)
    FUNC_REGISTRATION(FriendItem)
    FUNC_REGISTRATION(FriendList)
    FUNC_REGISTRATION(RemoveFriend)
    FUNC_REGISTRATION(PhoneFriendInfo)
    FUNC_REGISTRATION(DhtConnectStatus)
    FUNC_REGISTRATION(AudioDevInfo)
    FUNC_REGISTRATION(AudioDevChange)
    FUNC_REGISTRATION(AudioStreamInfo)
    FUNC_REGISTRATION(AudioTest)
    FUNC_REGISTRATION(AudioRecordLevel)
    FUNC_REGISTRATION(ToxCallAction)
    FUNC_REGISTRATION(ToxCallState)
    FUNC_REGISTRATION(DiverterInfo)
    FUNC_REGISTRATION(DiverterChange)
    FUNC_REGISTRATION(DiverterTest)
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
}

void MainWindow::loadGeometry()
{
    std::vector<int> v {0, 0, 800, 600};
    config::state().getValue("windows.main_window.geometry", v);
    move(v[0], v[1]);
    resize(v[2], v[3]);

    QVector<int> qv;
    config::state().getValue("windows.main_window.friends_splitter", qv);
    if (qv.isEmpty())
    {
        QList<int> spl; spl << (width() * 1./3) << (width() * 2./3);
        ui->splitterFriends->setSizes(spl);
    }
    else
        ui->splitterFriends->setSizes(qv.toList());
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
    ui->labelConnectStatus->clear();

    ui->lineSelfToxName->clear();
    ui->lineSelfToxStatus->clear();
    ui->lineSelfToxId->clear();
    ui->labelDhtConnecntStatus->setText("Undefined");

    for (int i = 0; i < ui->listFriends->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listFriends->item(i);
        ui->listFriends->removeItemWidget(lwi);
        delete lwi;
    }
    ui->listFriends->clear();
    ui->lineToxName->clear();
    ui->lineToxStatus->clear();
    ui->linefToxId->clear();
    ui->lineToxNameAlias->clear();
    ui->linePhoneNumber->clear();

    ui->cboxAudioPlayback->clear();
    ui->cboxAudioRecord->clear();
    _sinkDevices.clear();
    _sourceDevices.clear();

    ui->cboxAudioPlayback->setEnabled(true);
    ui->cboxAudioRecord->setEnabled(true);

    ui->btnPlaybackTest->setChecked(false);
    ui->btnRecordTest->setChecked(false);

    ui->pbarAudioRecord->setValue(0);
    ui->labelDeviceCurentMode->setText("Undefined");

    aboutClear();
}

void MainWindow::command_ToxPhoneInfo(const Message::Ptr& message)
{
    if (message->socketType() != SocketType::Tcp)
        return;

    data::ToxPhoneInfo toxPhoneInfo;
    readFromMessage(message, toxPhoneInfo);

    if (!toxPhoneInfo.info.isEmpty())
    {
        setWindowTitle(QString("%1 (%2)").arg(qApp->applicationName()).arg(toxPhoneInfo.info));
        ui->lineToxInfoString->setText(toxPhoneInfo.info);
    }
}

void MainWindow::command_ToxPhoneAbout(const Message::Ptr& message)
{
    data::ToxPhoneAbout toxPhoneAbout;
    readFromMessage(message, toxPhoneAbout);

    ui->labelToxPhoneVers->setText(VersionNumber(toxPhoneAbout.version).toString());
    ui->labelToxcoreVers->setText(VersionNumber(toxPhoneAbout.toxcore).toString());
    ui->labelGitrev->setText(toxPhoneAbout.gitrev);
    ui->labelQtVersion->setText(toxPhoneAbout.qtvers);
    ui->labelBprotocolVers->setText(QString("%1-%2").arg(message->protocolVersionLow())
                                                    .arg(message->protocolVersionHigh()));
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

    bool found = false;
    for (int i = 0; i < ui->listFriends->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listFriends->item(i);
        FriendWidget* fw = qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));
        if (fw->properties().publicKey == friendItem.publicKey)
        {
            found = true;
            fw->setProperties(friendItem);
            break;
        }
    }
    if (!found)
    {
        FriendWidget* fw = new FriendWidget();
        QListWidgetItem* lwi = new QListWidgetItem();
        lwi->setSizeHint(fw->sizeHint());
        ui->listFriends->addItem(lwi);
        ui->listFriends->setItemWidget(lwi, fw);

        // Значения присваиваем после создания элемента в списке,
        // иначе будет неверная геометрия
        fw->setProperties(friendItem);
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
        fw->setProperties(fi);
    }
}

void MainWindow::command_RemoveFriend(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            QString msg = tr("Friend was successfully removed");
            QMessageBox::information(this, qApp->applicationName(), msg);
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
    }
}

void MainWindow::command_PhoneFriendInfo(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer)
    {
        if (message->execStatus() == Message::ExecStatus::Success)
        {
            data::PhoneFriendInfo phoneFriendInfo;
            readFromMessage(message, phoneFriendInfo);

            QString msg = tr(
                "ToxPhone parameters for friend %1 was successfully saved");
            QMessageBox::information(this, qApp->applicationName(),
                                     msg.arg(phoneFriendInfo.name));
        }
        else
        {
            QString msg = errorDescription(message);
            QMessageBox::critical(this, qApp->applicationName(), msg);
        }
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

void MainWindow::command_AudioDevInfo(const Message::Ptr& message)
{
    data::AudioDevInfo audioDevInfo;
    readFromMessage(message, audioDevInfo);

    QComboBox* cbox;
    QSlider* slider;
    data::AudioDevInfo::List* devices;
    if (audioDevInfo.type == data::AudioDevType::Sink)
    {
        cbox = ui->cboxAudioPlayback;
        slider = ui->sliderAudioPlayback;
        devices = &_sinkDevices;
    }
    else
    {
        cbox = ui->cboxAudioRecord;
        slider = ui->sliderAudioRecord;
        devices = &_sourceDevices;
    }

    lst::FindResult fr = devices->findRef(audioDevInfo.name);
    if (fr.success())
    {
        (*devices)[fr.index()] = audioDevInfo;
        if (cbox->currentIndex() == fr.index())
        {
            if (slider->value() != int(audioDevInfo.volume))
                slider->setValue(audioDevInfo.volume);
        }
    }
    else
    {
        cbox->blockSignals(true);
        cbox->addItem(audioDevInfo.description);
        devices->addCopy(audioDevInfo);
        cbox->blockSignals(false);

        if (audioDevInfo.isCurrent)
        {
            data::AudioDevChange audioDevChange {audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

            Message::Ptr m = createMessage(audioDevChange, Message::Type::Event);
            command_AudioDevChange(m);
        }

        if (audioDevInfo.isDefault)
        {
            data::AudioDevChange audioDevChange {audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Default;

            Message::Ptr m = createMessage(audioDevChange, Message::Type::Event);
            command_AudioDevChange(m);
        }
    }
}

void MainWindow::command_AudioDevChange(const Message::Ptr& message)
{
    data::AudioDevChange audioDevChange;
    readFromMessage(message, audioDevChange);

    QComboBox* cbox;
    QSlider* slider;
    QPushButton* btdDefault;
    data::AudioDevInfo::List* devices;
    if (audioDevChange.type == data::AudioDevType::Sink)
    {
        cbox = ui->cboxAudioPlayback;
        slider = ui->sliderAudioPlayback;
        btdDefault = ui->btnPlaybackAsDefault;
        devices = &_sinkDevices;
    }
    else
    {
        cbox = ui->cboxAudioRecord;
        slider = ui->sliderAudioRecord;
        btdDefault = ui->btnRecordAsDefault;
        devices = &_sourceDevices;
    }

    // ChangeFlag::Remove
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Remove)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.success())
        {
            cbox->removeItem(fr.index());
            devices->remove(fr.index());
        }
    }

    // ChangeFlag::Current
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Current)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.success())
        {
            for (int i = 0; i < devices->count(); ++i)
                devices->item(i)->isCurrent = false;

            data::AudioDevInfo* audioDevInfo = devices->item(fr.index());
            audioDevInfo->isCurrent = true;

            cbox->blockSignals(true);
            cbox->setCurrentIndex(fr.index());
            cbox->blockSignals(false);

            //slider->setValue(devices->item(fr.index())->volume);
            setSliderLevel(slider, 0, audioDevInfo->volume, audioDevInfo->volumeSteps);
            btdDefault->setDisabled(devices->item(fr.index())->isDefault);
        }
    }

    // ChangeFlag::Default
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Default)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.success())
        {
            for (int i = 0; i < devices->count(); ++i)
                devices->item(i)->isDefault = false;

            devices->item(fr.index())->isDefault = true;

            if (cbox->currentIndex() == fr.index())
                btdDefault->setDisabled(true);
        }
    }
}

void MainWindow::command_AudioStreamInfo(const Message::Ptr& message)
{
    data::AudioStreamInfo audioStreamInfo;
    readFromMessage(message, audioStreamInfo);

    QLabel* label = 0;
    QSlider* slider = 0;
    if (audioStreamInfo.type == data::AudioStreamInfo::Type::Playback)
    {
        label = ui->labelStreamPlayback;
        slider = ui->sliderStreamPlayback;
    }
    else if (audioStreamInfo.type == data::AudioStreamInfo::Type::Voice)
    {
        label = ui->labelStreamVoice;
        slider = ui->sliderStreamVoice;
    }
    else if (audioStreamInfo.type == data::AudioStreamInfo::Type::Record)
    {
        label = ui->labelStreamRecord;
        slider = ui->sliderStreamRecord;
    }
    if (!slider)
        return;

    if (audioStreamInfo.state == data::AudioStreamInfo::State::Created)
    {
        if (!audioStreamInfo.hasVolume)
        {
            label->setDisabled(true);
            slider->setDisabled(true);
            slider->setValue(0);
            return;
        }
        if (!audioStreamInfo.volumeWritable)
        {
            label->setDisabled(true);
            slider->setDisabled(true);
            slider->setValue(0);
            return;
        }
        if (audioStreamInfo.volumeSteps == 0)
        {
            label->setDisabled(true);
            slider->setDisabled(true);
            slider->setValue(0);
            return;
        }
        label->setEnabled(true);
        slider->setEnabled(true);
        setSliderLevel(slider, 0, audioStreamInfo.volume, audioStreamInfo.volumeSteps);
    }
    else if (audioStreamInfo.state == data::AudioStreamInfo::State::Changed)
    {
        if (audioStreamInfo.volumeSteps == 0)
            return;

        setSliderLevel(slider, 0, audioStreamInfo.volume, audioStreamInfo.volumeSteps);
    }
    else if (audioStreamInfo.state == data::AudioStreamInfo::State::Terminated)
    {
        label->setDisabled(true);
        slider->setDisabled(true);
        slider->setValue(0);
    }
}

void MainWindow::command_AudioTest(const Message::Ptr& message)
{

    if (message->type() == Message::Type::Answer
        && message->execStatus() != Message::ExecStatus::Success)
    {
        ui->btnPlaybackTest->setChecked(false);
        ui->btnRecordTest->setChecked(false);

        QString msg = errorDescription(message);
        QMessageBox::information(this, qApp->applicationName(), msg);
        return;
    }

    data::AudioTest audioTest;
    readFromMessage(message, audioTest);

    if (audioTest.playback)
    {
        ui->btnPlaybackTest->setChecked(false);
        ui->cboxAudioPlayback->setEnabled(true);
    }

    if (audioTest.record)
    {
        ui->btnRecordTest->setChecked(false);
        ui->cboxAudioRecord->setEnabled(true);
    }
}

void MainWindow::command_AudioRecordLevel(const Message::Ptr& message)
{
    data::AudioRecordLevel audioRecordLevel;
    readFromMessage(message, audioRecordLevel);

    ui->pbarAudioRecord->setValue(audioRecordLevel.max);
}

void MainWindow::command_ToxCallAction(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer
        && message->execStatus() != Message::ExecStatus::Success)
    {
        QString msg = errorDescription(message);
        QMessageBox::critical(this, qApp->applicationName(), msg);
        return;
    }
}

void MainWindow::command_ToxCallState(const Message::Ptr& message)
{
    const char* CALL    = QT_TRANSLATE_NOOP("MainWindow", "Call");
    const char* ENDCALL = QT_TRANSLATE_NOOP("MainWindow", "End call");
    const char* ACCEPT  = QT_TRANSLATE_NOOP("MainWindow", "Accept");
    const char* REJECT  = QT_TRANSLATE_NOOP("MainWindow", "Reject");

    readFromMessage(message, _callState);

    if (_callState.direction == data::ToxCallState::Direction::Incoming)
    {
        if (_callState.callState == data::ToxCallState::CallState::WaitingAnswer)
        {
            log_debug << "Call command_ToxCallState() "
                      << "Direction: Incoming; CallState: WaitingAnswer";

            ui->btnCall->setText(tr(ACCEPT));
            ui->btnCall->setEnabled(true);

            ui->btnEndCall->setText(tr(REJECT));
            ui->btnEndCall->setEnabled(true);
        }
        else
        {
            log_debug << "Call command_ToxCallState() "
                      << "Direction: Incoming; CallState: Undefined/InProgress";

            ui->btnCall->setText(tr(CALL));
            ui->btnCall->setEnabled(false);

            ui->btnEndCall->setText(tr(ENDCALL));
            ui->btnEndCall->setEnabled(true);
        }

        QString msg = "Incoming call: " + friendCalling(_callState.friendNumber);
        ui->labelCallState->setText(msg);
    }
    else if (_callState.direction == data::ToxCallState::Direction::Outgoing)
    {
        if (_callState.callState == data::ToxCallState::CallState::WaitingAnswer)
            log_debug << "Call command_ToxCallState() "
                      << "Direction: Outgoing; CallState: WaitingAnswer";
        else
            log_debug << "Call command_ToxCallState() "
                     <<  "Direction: Outgoing; CallState: Undefined/InProgress";

        ui->btnCall->setText(tr(CALL));
        ui->btnCall->setEnabled(false);

        ui->btnEndCall->setText(tr(ENDCALL));
        ui->btnEndCall->setEnabled(true);

        QString msg = "Outgoing call: " + friendCalling(_callState.friendNumber);
        ui->labelCallState->setText(msg);
    }
    else // data::ToxCall::Direction::Undefined
    {
        log_debug << "Call command_ToxCallState() Direction: Undefined";

        ui->btnCall->setText(tr(CALL));
        ui->btnCall->setEnabled(true);

        ui->btnEndCall->setText(tr(ENDCALL));
        ui->btnEndCall->setEnabled(false);

        ui->labelCallState->clear();
    }

    if (_callState.direction == data::ToxCallState::Direction::Undefined)
    {
        ui->btnPlaybackTest->setEnabled(true);
        ui->btnRecordTest->setEnabled(true);
    }
    else
    {
        ui->btnPlaybackTest->setEnabled(false);
        ui->btnRecordTest->setEnabled(false);
    }
}

void MainWindow::command_DiverterInfo(const Message::Ptr& message)
{
    data::DiverterInfo diverterInfo;
    readFromMessage(message, diverterInfo);

    ui->cboxUseDiverter->blockSignals(true);
    ui->cboxUseDiverter->setChecked(diverterInfo.active);
    ui->cboxUseDiverter->blockSignals(false);

    if (diverterInfo.defaultMode == data::DiverterDefaultMode::Pstn)
        ui->rbtnDiverterPSTN->setChecked(true);
    else
        ui->rbtnDiverterUSB->setChecked(true);

    ui->labelDeviceCurentMode->setText(diverterInfo.currentMode);
    ui->linePhoneRingtone->setText(diverterInfo.ringTone.trimmed());

    ui->labelDiverterDevUsbBus ->setText(diverterInfo.deviceUsbBus);
    ui->labelDiverterDevName   ->setText(diverterInfo.deviceName);
    ui->labelDiverterDevVersion->setText(diverterInfo.deviceVersion);
    ui->labelDiverterDevSerial ->setText(diverterInfo.deviceSerial);

    ui->btnTestPhoneRingtone->setEnabled(diverterInfo.attached);
}

void MainWindow::command_DiverterChange(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer
        && message->execStatus() != Message::ExecStatus::Success)
    {
        QString msg = errorDescription(message);
        QMessageBox::critical(this, qApp->applicationName(), msg);
    }
}

void MainWindow::command_DiverterTest(const Message::Ptr& message)
{
    if (message->type() == Message::Type::Answer
        && message->execStatus() != Message::ExecStatus::Success)
    {
        ui->btnTestPhoneRingtone->setChecked(false);
        QString msg = errorDescription(message);
        QMessageBox::critical(this, qApp->applicationName(), msg);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->accept();
    qApp->exit();
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

    Message::Ptr m = createMessage(toxProfile);
    _socket->send(m);
}

void MainWindow::on_btnRequestFriendship_clicked(bool)
{
    data::RequestFriendship reqFriendship;
    reqFriendship.toxId = ui->lineRequestToxId->text().trimmed().toLatin1();
    reqFriendship.message = ui->txtRequestMessage->toPlainText();

    Message::Ptr m = createMessage(reqFriendship);
    _socket->send(m);
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

    Message::Ptr m = createMessage(friendRequest);
    _socket->send(m);
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

    QString msg = tr("You are sure that you want to remove the friend of %1?");
    QMessageBox::StandardButton mboxResult =
        QMessageBox::question(this, qApp->applicationName(),
                              msg.arg(fw->properties().name),
                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (mboxResult == QMessageBox::No)
        return;

    data::RemoveFriend removeFriend;
    removeFriend.publicKey = fw->properties().publicKey;
    removeFriend.name = fw->properties().name;

    Message::Ptr m = createMessage(removeFriend);
    _socket->send(m);
}

void MainWindow::on_btnSaveFiendPhone_clicked(bool)
{
    if (!ui->listFriends->count())
        return;

    if (!ui->listFriends->currentItem())
        return;

    QListWidgetItem* lwi = ui->listFriends->currentItem();
    FriendWidget* fw =
        qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));

    data::PhoneFriendInfo phoneFriendInfo;
    phoneFriendInfo.publicKey = fw->properties().publicKey;
    phoneFriendInfo.number = fw->properties().number;
    phoneFriendInfo.name = fw->properties().name;
    phoneFriendInfo.nameAlias = ui->lineToxNameAlias->text();

    bool toint;
    phoneFriendInfo.phoneNumber = ui->linePhoneNumber->text().toInt(&toint);
    if (!toint)
        phoneFriendInfo.phoneNumber = 0;

    bool duplicateFound = false;
    if (phoneFriendInfo.phoneNumber != 0)
        for (int i = 0; i < ui->listFriends->count(); ++i)
        {
            lwi = ui->listFriends->item(i);
            fw = qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));
            if (fw->properties().phoneNumber == phoneFriendInfo.phoneNumber
                && fw->properties().number != phoneFriendInfo.number)
            {
                duplicateFound = true;
                break;
            }
        }

    if (duplicateFound)
    {
        QString msg = tr("The phone number %1 already assigned to friend %2."
                         "\nYou are sure that you want to rewrite phone number?");
        QMessageBox::StandardButton mboxResult =
                QMessageBox::question(this, qApp->applicationName(),
                                      msg.arg(fw->properties().phoneNumber)
                                      .arg(fw->properties().name),
                                      QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
        if (mboxResult == QMessageBox::No)
            return;
    }
    Message::Ptr m = createMessage(phoneFriendInfo);
    _socket->send(m);
}

void MainWindow::on_btnPlaybackTest_clicked(bool)
{
    ui->cboxAudioPlayback->setDisabled(true);

    data::AudioTest audioTest;
    audioTest.begin = ui->btnPlaybackTest->isChecked();
    audioTest.playback = true;

    Message::Ptr m = createMessage(audioTest);
    _socket->send(m);
}

void MainWindow::on_btnRecordTest_clicked(bool)
{
    ui->cboxAudioRecord->setDisabled(true);

    data::AudioTest audioTest;
    audioTest.begin = ui->btnRecordTest->isChecked();
    audioTest.record = true;

    Message::Ptr m = createMessage(audioTest);
    _socket->send(m);
}

void MainWindow::on_btnCall_clicked(bool)
{
    data::ToxCallAction toxCallAction;

    // Новый вызов
    if (_callState.direction == data::ToxCallState::Direction::Undefined)
    {
        if (!ui->listFriends->count())
            return;

        if (!ui->listFriends->currentItem())
            return;

        QListWidgetItem* lwi = ui->listFriends->currentItem();
        FriendWidget* fw =
            qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));

        toxCallAction.action = data::ToxCallAction::Action::Call;
        toxCallAction.friendNumber = fw->properties().number;
    }

    // Принять входящий вызов
    else if (_callState.direction == data::ToxCallState::Direction::Incoming
             && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        toxCallAction.action = data::ToxCallAction::Action::Accept;
        toxCallAction.friendNumber = _callState.friendNumber;
    }

    Message::Ptr m = createMessage(toxCallAction);
    _socket->send(m);
}

void MainWindow::on_btnEndCall_clicked(bool)
{
    if (_callState.direction == data::ToxCallState::Direction::Undefined)
        return;

    data::ToxCallAction toxCallAction;

    // Завершить исходящий вызов
    if (_callState.direction == data::ToxCallState::Direction::Outgoing)
    {
        toxCallAction.action = data::ToxCallAction::Action::End;
        toxCallAction.friendNumber = _callState.friendNumber;
    }

    // Завершить входящий вызов
    else if (_callState.direction == data::ToxCallState::Direction::Incoming
             && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        toxCallAction.action = data::ToxCallAction::Action::End;
        toxCallAction.friendNumber = _callState.friendNumber;
    }

    // Отклонить входящий вызов
    else if (_callState.direction == data::ToxCallState::Direction::Incoming
             && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        toxCallAction.action = data::ToxCallAction::Action::Reject;
        toxCallAction.friendNumber = _callState.friendNumber;
    }

    Message::Ptr m = createMessage(toxCallAction);
    _socket->send(m);
}

void MainWindow::on_splitterFriends_splitterMoved(int pos, int index)
{
    // Сохранение данного параметра в функции saveGeometry() выполняется
    // некорректно, поэтому сохраняем его здесь.
    QVector<int> qv = ui->splitterFriends->sizes().toVector();
    config::state().setValue("windows.main_window.friends_splitter", qv);
}

void MainWindow::on_listFriends_itemClicked(QListWidgetItem* item)
{
    FriendWidget* fw =
        qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(item));

    ui->lineToxName->setText(fw->properties().name);

    ui->lineToxStatus->clear(); // Предотвращает эффект перемотки текста вперед
                                // когда вставляется длинная строка
    ui->lineToxStatus->setText(fw->properties().statusMessage);
    ui->lineToxStatus->home(false);

    ui->linefToxId->clear();
    ui->linefToxId->setText(fw->properties().publicKey);
    ui->linefToxId->home(false);

    ui->lineToxNameAlias->setText(fw->properties().nameAlias);
    if (fw->properties().phoneNumber != 0)
        ui->linePhoneNumber->setText(QString::number(fw->properties().phoneNumber));
    else
        ui->linePhoneNumber->clear();
}

void MainWindow::on_cboxAudioPlayback_currentIndexChanged(int index)
{
    if (!lst::checkBounds(index, _sinkDevices))
        return;

    data::AudioDevInfo* audioDevInfo = _sinkDevices.item(index);
    setSliderLevel(ui->sliderAudioPlayback, 0, audioDevInfo->volume, audioDevInfo->volumeSteps);
    ui->btnPlaybackAsDefault->setDisabled(audioDevInfo->isDefault);

    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_cboxAudioRecord_currentIndexChanged(int index)
{
    if (!lst::checkBounds(index, _sourceDevices))
        return;

    data::AudioDevInfo* audioDevInfo = _sourceDevices.item(index);
    setSliderLevel(ui->sliderAudioRecord, 0, audioDevInfo->volume, audioDevInfo->volumeSteps);
    ui->pbarAudioRecord->setMaximum(audioDevInfo->volumeSteps / 2);
    ui->btnRecordAsDefault->setDisabled(audioDevInfo->isDefault);

    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_btnPlaybackAsDefault_clicked(bool)
{
    int index = ui->cboxAudioPlayback->currentIndex();
    if (index < 0)
        return;

    data::AudioDevInfo* audioDevInfo = _sinkDevices.item(index);
    audioDevInfo->isDefault = true;

    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Default;

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_btnRecordAsDefault_clicked(bool)
{
    int index = ui->cboxAudioRecord->currentIndex();
    if (index < 0)
        return;

    data::AudioDevInfo* audioDevInfo = _sourceDevices.item(index);
    audioDevInfo->isDefault = true;

    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Default;

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_sliderAudioPlayback_sliderReleased()
{
    int index = ui->cboxAudioPlayback->currentIndex();
    if (index < 0)
        return;

    data::AudioDevInfo* audioDevInfo = _sinkDevices.item(index);
    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Volume;
    audioDevChange.value = ui->sliderAudioPlayback->value();

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_sliderAudioRecord_sliderReleased()
{
    int index = ui->cboxAudioRecord->currentIndex();
    if (index < 0)
        return;

    data::AudioDevInfo* audioDevInfo = _sourceDevices.item(index);
    data::AudioDevChange audioDevChange {*audioDevInfo};
    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Volume;
    audioDevChange.value = ui->sliderAudioRecord->value();

    Message::Ptr m = createMessage(audioDevChange);
    _socket->send(m);
}

void MainWindow::on_sliderStreamPlayback_sliderReleased()
{
    data::AudioStreamInfo audioStreamInfo;
    audioStreamInfo.type = data::AudioStreamInfo::Type::Playback;
    audioStreamInfo.state = data::AudioStreamInfo::State::Changed;
    audioStreamInfo.volume = ui->sliderStreamPlayback->value();

    Message::Ptr m = createMessage(audioStreamInfo);
    _socket->send(m);
}

void MainWindow::on_sliderStreamVoice_sliderReleased()
{
    data::AudioStreamInfo audioStreamInfo;
    audioStreamInfo.type = data::AudioStreamInfo::Type::Voice;
    audioStreamInfo.state = data::AudioStreamInfo::State::Changed;
    audioStreamInfo.volume = ui->sliderStreamVoice->value();

    Message::Ptr m = createMessage(audioStreamInfo);
    _socket->send(m);
}

void MainWindow::on_sliderStreamRecord_sliderReleased()
{
    data::AudioStreamInfo audioStreamInfo;
    audioStreamInfo.type = data::AudioStreamInfo::Type::Record;
    audioStreamInfo.state = data::AudioStreamInfo::State::Changed;
    audioStreamInfo.volume = ui->sliderStreamRecord->value();

    Message::Ptr m = createMessage(audioStreamInfo);
    _socket->send(m);
}

void MainWindow::setSliderLevel(QSlider* slider, int base, int current, int max)
{
    slider->setMinimum(base);
    slider->setMaximum(max);
    slider->setValue(current);
}

void MainWindow::on_cboxUseDiverter_stateChanged(int state)
{
    data::DiverterChange diverterChange;
    diverterChange.active = (state != Qt::Unchecked);
    diverterChange.changeFlag = data::DiverterChange::ChangeFlag::Active;

    Message::Ptr m = createMessage(diverterChange);
    _socket->send(m);
}

void MainWindow::on_rbtnDiverterPSTN_clicked(bool)
{
    data::DiverterChange diverterChange;
    diverterChange.defaultMode = data::DiverterDefaultMode::Pstn;
    diverterChange.changeFlag = data::DiverterChange::ChangeFlag::DefaultMode;

    Message::Ptr m = createMessage(diverterChange);
    _socket->send(m);
}

void MainWindow::on_rbtnDiverterUSB_clicked(bool)
{
    data::DiverterChange diverterChange;
    diverterChange.defaultMode = data::DiverterDefaultMode::Usb;
    diverterChange.changeFlag = data::DiverterChange::ChangeFlag::DefaultMode;

    Message::Ptr m = createMessage(diverterChange);
    _socket->send(m);
}

void MainWindow::on_btnSavePhoneRingtone_clicked(bool)
{
    data::DiverterChange diverterChange;
    diverterChange.ringTone = ui->linePhoneRingtone->text().trimmed();
    diverterChange.changeFlag = data::DiverterChange::ChangeFlag::RingTone;

    Message::Ptr m = createMessage(diverterChange);
    _socket->send(m);
}

void MainWindow::on_btnTestPhoneRingtone_clicked(bool)
{
    data::DiverterTest diverterTest;
    diverterTest.begin = ui->btnTestPhoneRingtone->isChecked();
    diverterTest.ringTone = true;

    Message::Ptr m = createMessage(diverterTest);
    _socket->send(m);
}

void MainWindow::on_btnSaveToxInfo_clicked(bool)
{
    data::ToxPhoneInfo toxPhoneInfo;
    toxPhoneInfo.info = ui->lineToxInfoString->text().trimmed();

    Message::Ptr m = createMessage(toxPhoneInfo);
    _socket->send(m);
}

QString MainWindow::friendCalling(quint32 friendNumber)
{
    QString result;
    for (int i = 0; i < ui->listFriends->count(); ++i)
    {
        QListWidgetItem* lwi = ui->listFriends->item(i);
        FriendWidget* fw = qobject_cast<FriendWidget*>(ui->listFriends->itemWidget(lwi));
        if (fw->properties().number == friendNumber)
        {
            result = (fw->properties().nameAlias.isEmpty())
                     ? fw->properties().name
                     : QString("%1 (%2)").arg(fw->properties().nameAlias)
                                         .arg(fw->properties().name);
            break;
        }
    }
    return result;
}

void MainWindow::aboutClear()
{
    ui->labelToxPhoneVers->clear();
    ui->labelToxcoreVers->clear();
    ui->labelGitrev->clear();
    ui->labelBprotocolVers->clear();
    ui->labelQtVersion->clear();
}

void MainWindow::on_labelCopyright_linkActivated(const QString& link)
{
     QDesktopServices::openUrl(QUrl(link));
}
