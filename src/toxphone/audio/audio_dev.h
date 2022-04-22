/*****************************************************************************
  Модуль предоставляет информацию по аудио-устройствам, и так же отслеживает
  изменение их состояния.
*****************************************************************************/

#pragma once

#include "audio/wav_file.h"
#include "common/voice_frame.h"
#include "diverter/phone_diverter.h"

#include "shared/list.h"
#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "pproto/func_invoker.h"

#include "commands/commands.h"
#include "commands/error.h"

#include <pulse/pulseaudio.h>
#include <QtCore>
#include <atomic>

using namespace std;
using namespace pproto;
using namespace pproto::transport;

/**
  Класс для работы с аудио-устройствами через PulseAudio
*/
class AudioDev : public QObject
{
public:
    bool init();
    bool start();
    bool stop(unsigned long time = ULONG_MAX);
    void terminate() {}

signals:
    // Используется для отправки сообщения в пределах программы
    void internalMessage(const pproto::Message::Ptr&);

public slots:
    void message(const pproto::Message::Ptr&);

    // Проигрывания звука звонка
    void playRingtone();

    // Проигрывания звука исходящего вызова
    void playOutgoing();

    // Проигрывания звука "занято"
    void playBusy();

    // Проигрывания звука "неудача"
    void playFail();

    // Проигрывания звука "ошибка"
    void playError();

    // Используется для эмитирования сообщения завершения проигрывания звука
    void playFake();

    // Старт/стоп воспроизведения звуков
    void startPlayback(const QString& fileName, int cycleCount = 1,
                       data::PlaybackFinish::Code playbackFinishCode = data::PlaybackFinish::Code::Undefined);
    void stopPlayback();

    // Старт/стоп воспроизведения голоса
    void startVoice(const VoiceFrameInfo::Ptr&);
    void stopVoice();

    // Старт/стоп записи голоса
    void startRecord();
    void stopRecord();

    // Признак активности воспроизведения голосового потока
    bool voiceActive() const {return _voiceActive;}

    // Признак активности записи голосового потока
    bool recordActive() const {return _recordActive;}

private slots:
    void playRingtoneByTimer();
    void playOutgoingByTimer();
    void playBusyByTimer();
    void playFailByTimer();
    void playErrorByTimer();

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(AudioDev)
    AudioDev();
    void deinit();

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_AudioDevChange(const Message::Ptr&);
    void command_AudioStreamInfo(const Message::Ptr&);
    void command_AudioTest(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);

    template<typename InfoType>
    void fillAudioDevInfo(const InfoType*, data::AudioDevInfo&);

    template<typename InfoType>
    void updateAudioDevInfo(const InfoType*, data::AudioDevInfo::List& devices);
    void updateStartVolume(const data::AudioDevInfo&);

    void fillAudioStreamInfo(const pa_sink_input_info*, data::AudioStreamInfo&);
    void fillAudioStreamInfo(const pa_source_output_info*, data::AudioStreamInfo&);

    data::AudioDevInfo::List* getDevices(data::AudioDevType);
    data::AudioDevInfo* currentDevice(const data::AudioDevInfo::List&);
    const char* currentDeviceName(const data::AudioDevInfo::List&, QByteArray& buff);
    bool removeDevice(quint32 index, data::AudioDevType, bool byCardIndex);

    // Останавливает выполнение всех аудио-тестов
    void stopAudioTests();

    void readAudioStreamVolume (data::AudioStreamInfo&, const char* confKey);
    void saveAudioStreamVolume(data::AudioStreamInfo&, const char* confKey);

private:
    // PulseAudio callback
    static void context_state     (pa_context* context, void* userdata);
    static void context_subscribe (pa_context* context, pa_subscription_event_type_t type,
                                   uint32_t index, void* userdata);
    static void card_info         (pa_context* context, const pa_card_info* info,
                                   int eol, void* userdata);
    static void sink_info         (pa_context* context, const pa_sink_info* info,
                                   int eol, void* userdata);
    static void sink_change       (pa_context* context, const pa_sink_info* info,
                                   int eol, void* userdata);
    static void source_info       (pa_context* context, const pa_source_info* info,
                                   int eol, void* userdata);
    static void source_change     (pa_context* context, const pa_source_info* info,
                                   int eol, void* userdata);

    static void playback_stream_create(pa_context* context, const pa_sink_input_info* info,
                                       int eol, void* userdata);
    static void voice_stream_create   (pa_context* context, const pa_sink_input_info* info,
                                       int eol, void* userdata);
    static void record_stream_create  (pa_context* context, const pa_source_output_info* info,
                                       int eol, void* userdata);
    static void sink_stream_info      (pa_context* context, const pa_sink_input_info* info,
                                       int eol, void* userdata);
    static void source_stream_info    (pa_context* context, const pa_source_output_info* info,
                                       int eol, void* userdata);


    static void playback_stream_state    (pa_stream*, void* userdata);
    static void playback_stream_started  (pa_stream*, void* userdata);
    static void playback_stream_write    (pa_stream*, size_t nbytes, void* userdata);
    static void playback_stream_overflow (pa_stream*, void* userdata);
    static void playback_stream_underflow(pa_stream*, void* userdata);
    static void playback_stream_suspended(pa_stream*, void* userdata);
    static void playback_stream_moved    (pa_stream*, void* userdata);
    static void playback_stream_drain    (pa_stream*, int success, void *userdata);

    static void voice_stream_state       (pa_stream*, void* userdata);
    static void voice_stream_started     (pa_stream*, void* userdata);
    static void voice_stream_write       (pa_stream*, size_t nbytes, void* userdata);
    static void voice_stream_overflow    (pa_stream*, void* userdata);
    static void voice_stream_underflow   (pa_stream*, void* userdata);
    static void voice_stream_suspended   (pa_stream*, void* userdata);
    static void voice_stream_moved       (pa_stream*, void* userdata);

    static void record_stream_state      (pa_stream*, void* userdata);
    static void record_stream_started    (pa_stream*, void* userdata);
    static void record_stream_read       (pa_stream*, size_t nbytes, void* userdata);
    static void record_stream_overflow   (pa_stream*, void* userdata);
    static void record_stream_underflow  (pa_stream*, void* userdata);
    static void record_stream_suspended  (pa_stream*, void* userdata);
    static void record_stream_moved      (pa_stream*, void* userdata);


private:
    pa_threaded_mainloop* _paMainLoop = {0};
    pa_mainloop_api*      _paApi = {0};
    pa_context*           _paContext = {0};

    data::AudioDevInfo::List _sinkDevices;
    data::AudioDevInfo::List _sourceDevices;

    pa_stream* _playbackStream = {0}; // Поток для воспроизведения звуков
    pa_stream* _voiceStream = {0};    // Поток для воспроизведения голоса
    pa_stream* _recordStream = {0};   // Поток для записи голоса
    QMutex _streamLock;

    data::AudioStreamInfo _palybackAudioStreamInfo;
    data::AudioStreamInfo _voiceAudioStreamInfo;
    data::AudioStreamInfo _recordAudioStreamInfo;

    atomic_bool _playbackActive = {false};
    atomic_bool _voiceActive = {false};
    atomic_bool _recordActive = {false};

    size_t _voiceBytes = {0};
    size_t _recordBytes = {0};

    atomic_bool _playbackTest = {false};
    atomic_bool _recordTest = {false};

    atomic_int _playbackCycleCount = {1};
    WavFile _playbackFile;
    QTimer _playbackTimer;

    data::PlaybackFinish _playbackFinish;
    atomic_bool _emitPlaybackFinish = {true};

    // Индикатор состояния звонка
    data::ToxCallState _callState;

    FunctionInvoker _funcInvoker;

    template<typename T, int> friend T& ::safe_singleton();

};

AudioDev& audioDev();
