#pragma once

#include "common/voice_frame.h"
#include "kernel/communication/commands.h"

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/safe_singleton.h"
#include "shared/qt/thread/qthreadex.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"
#include "toxcore/tox.h"
#include "toxav/toxav.h"

#include <QtCore>
#include <atomic>

#include "tox_net.h"

using namespace std;
using namespace communication;
using namespace communication::transport;

class ToxCall : public QThreadEx
{
public:
    bool init(Tox* tox);

signals:
    void startPlaybackVoice(const VoiceFrameInfo::Ptr&);
    void stopPlaybackVoice();
    // Эмитируется когда получен голосовой фрейм от друга
    //void playbackVoiceFrame(const VoiceFrameInfo::Ptr&);

    void startRecordVoice();
    void stopRecordVoice();

public slots:
    void message(const communication::Message::Ptr&);

    // Вызывается когда нужно отправить голосовой фрейм другу
    //void sendVoiceFrame(const VoiceFrame::Ptr&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(ToxCall)
    ToxCall();

    void run() override;

//    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_ToxCallAction(const Message::Ptr&);

    void iterateVoiceFrame();
    void stopCalling();

private:
    // Tox callback
    static void toxav_call_cb            (ToxAV* av, uint32_t friend_number,
                                          bool audio_enabled, bool video_enabled,
                                          void* user_data);
    static void toxav_call_state         (ToxAV* av, uint32_t friend_number,
                                          uint32_t state, void* user_data);
    static void toxav_audio_bit_rate     (ToxAV* av, uint32_t friend_number,
                                          uint32_t audio_bit_rate, void* tox_call);
    static void toxav_video_bit_rate     (ToxAV* av, uint32_t friend_number,
                                          uint32_t video_bit_rate, void* user_data);
    static void toxav_audio_receive_frame(ToxAV* av, uint32_t friend_number,
                                          const int16_t* pcm, size_t sample_count,
                                          uint8_t channels, uint32_t sampling_rate,
                                          void* user_data);
    static void toxav_video_receive_frame(ToxAV* av, uint32_t friend_number,
                                          uint16_t width, uint16_t height,
                                          const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                          int32_t ystride, int32_t ustride, int32_t vstride,
                                          void* user_data);
private:
    ToxAV* _toxav;
    data::ToxCallState _callState;
    int _skipFirstFrames = {0};
    atomic<quint32> _voiceFriendNumber = {quint32(-1)};

    size_t _recordBytes = {0};
    size_t _playbackBytes = {0};
    //QFile _recordTestFile;

    FunctionInvoker _funcInvoker;

    Message::List _messages;
    mutex _threadLock;
    condition_variable _threadCond;
    atomic_bool _threadSleep = {false};

    template<typename T, int> friend T& ::safe_singleton();
};
ToxCall& toxCall();


