/*****************************************************************************
  Модуль предоставляет информацию по аудио-устройствам, и так же отслеживает
  изменение их состояния.
*****************************************************************************/

#pragma once

#include "wav_file.h"
#include "common/voice_frame.h"
#include "kernel/communication/commands.h"

#include "shared/_list.h"
#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/qt/thread/qthreadex.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/func_invoker.h"

#include <pulse/pulseaudio.h>
#include <QtCore>
#include <atomic>
#include <type_traits>

using namespace std;
using namespace communication;
using namespace communication::transport;

/**
  Класс для работы с аудио-устройствами через PulseAudio
*/
class AudioDev : public QObject  //public QThreadEx
{
public:
    bool init();
    void start();
    bool stop(unsigned long time = ULONG_MAX);
    void terminate() {}

signals:
    // Сигнал эмитируется при поступлении данных с микрофона.
    // averageLevel - средний уровень чувствительности за время time
    void sourceLevel(quint32 averageLevel, quint32 time);

public slots:
    void message(const communication::Message::Ptr&);

    void audioSinkTest();

    // Старт/стоп проигрывания звука звонка
    void startRingtone();
    void stopRingtone();


    // Старт/стоп воспроизведения голоса и звуков
    void startPlayback();
    void startPlaybackVoice(const VoiceFrameInfo::Ptr&);
    void stopPlayback();

    // Старт/стоп записи голоса
    void startRecord();
    void stopRecord();

    // Признак активности звука звонка
    bool ringtoneActive() const {return _ringtoneActive;}

    // Признак активности воспроизведения голосового потока
    bool voiceActive() const {return _playbackActive;}

    // Признак активности записи голосового потока
    bool recordActive() const {return _recordActive;}

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(AudioDev)
    AudioDev();

    void deinit();
    //void run() override;

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_AudioDev(const Message::Ptr&);
    void command_AudioSinkTest(const Message::Ptr&);

    template<typename info_type>
    void fillAudioDev(const info_type*, data::AudioDev&);

    int findDevByName(const QVector<data::AudioDev>& devices, const QByteArray& name);
    void updateDevices(const data::AudioDev& audioDev);
    bool removeDeviceByCardIndex(int index, data::AudioDevType);
    bool removeDeviceByIndex(int index, data::AudioDevType);

    // Функции обновляют состояние конфигуратора
    void updateDevicesInConfig(data::AudioDevType type);

    void createDefaultSinkStream();
    //void playSinkStream(const pa_sample_spec*);
    //void stopSinkStream();

    //void createSourceStream();


private:
    // PulseAudio callback
    static void context_state    (pa_context* context, void* userdata);
    static void context_subscribe(pa_context* context, pa_subscription_event_type_t type,
                                  uint32_t index, void* userdata);
    static void card_info        (pa_context* context, const pa_card_info* info,
                                  int eol, void* userdata);
    static void sink_info        (pa_context* context, const pa_sink_info* info,
                                  int eol, void* userdata);
    static void sink_volume      (pa_context* context, const pa_sink_info* info,
                                  int eol, void* userdata);
    static void source_info      (pa_context* context, const pa_source_info* info,
                                  int eol, void* userdata);
    static void source_volume    (pa_context* context, const pa_source_info* info,
                                  int eol, void* userdata);

    static void ringtone_stream_state    (pa_stream*, void* userdata);
    static void ringtone_stream_started  (pa_stream*, void* userdata);
    static void ringtone_stream_write    (pa_stream*, size_t nbytes, void* userdata);
    static void ringtone_stream_overflow (pa_stream*, void* userdata);
    static void ringtone_stream_underflow(pa_stream*, void* userdata);
    static void ringtone_stream_suspended(pa_stream*, void* userdata);
    static void ringtone_stream_moved    (pa_stream*, void* userdata);
    static void ringtone_stream_drain    (pa_stream*, int success, void *userdata);

    static void playback_stream_state    (pa_stream*, void* userdata);
    static void playback_stream_started  (pa_stream*, void* userdata);
    static void playback_stream_write    (pa_stream*, size_t nbytes, void* userdata);
    static void playback_stream_overflow (pa_stream*, void* userdata);
    static void playback_stream_underflow(pa_stream*, void* userdata);
    static void playback_stream_suspended(pa_stream*, void* userdata);
    static void playback_stream_moved    (pa_stream*, void* userdata);
    static void playback_stream_drain    (pa_stream*, int success, void *userdata);

    static void record_stream_state      (pa_stream*, void* userdata);
    static void record_stream_started    (pa_stream*, void* userdata);
    static void record_stream_read       (pa_stream*, size_t nbytes, void* userdata);
    static void record_stream_overflow   (pa_stream*, void* userdata);
    static void record_stream_underflow  (pa_stream*, void* userdata);
    static void record_stream_suspended  (pa_stream*, void* userdata);
    static void record_stream_moved      (pa_stream*, void* userdata);


private:
    //pa_mainloop*     _paMainLoop = {0};
    pa_threaded_mainloop* _paMainLoop = {0};
    pa_mainloop_api*      _paApi = {0};
    pa_context*           _paContext = {0};

    QVector<data::AudioDev> _sinkDevices;
    QVector<data::AudioDev> _sourceDevices;
    mutable QMutex _devicesLock;

    pa_stream* _ringtoneStream = {0}; // Поток для воспроизведения звука звонка
    pa_stream* _playbackStream = {0}; // Поток для воспроизведения голоса и звуков
    pa_stream* _recordStream = {0};   // Поток для записи голоса
    mutable QMutex _streamLock;
    //mutable QMutex _streamLock2;

    atomic_bool _ringtoneActive = {false};
    atomic_bool _recordActive = {false};
    atomic_bool _playbackActive = {false};
    atomic_bool _playbackVoice = {false};

    //QByteArray _voiceData;
    //mutable std::atomic_flag _voiceDataLock = ATOMIC_FLAG_INIT;
    //SpinLocker locker(_socketLock); (void) locker;

    size_t _recordBytes = {0};
    size_t _playbackBytes = {0};

    WavFile _sinkTestFile;
    WavFile _ringtoneFile;

    // Параметр используется для подготовки данных об индикации уровня сигнала
    // микрофона в конфигураторе (Байты звукового потока, размер фрейма
    // потока quint16)
    //QByteArray _recordBytes;

    // Время необходимое на воспроизведение _recordBytes
    quint32 _recordTime = {0};

    QQueue<QByteArray> _recordBuff;
    mutable QMutex _recordBuffLock;

    QFile _sourceTestFile;
    //QFile _recordTestFile;

    FunctionInvoker _funcInvoker;

    template<typename T, int> friend T& ::safe_singleton();

};
AudioDev& audioDev();

//----------------------------- Implementation -------------------------------

template<typename info_type>
void AudioDev::fillAudioDev(const info_type* info, data::AudioDev& audioDev)
{
    data::AudioDevType type = data::AudioDevType::Sink;
    if (std::is_same<info_type, pa_source_info>::value)
        type = data::AudioDevType::Source;

    audioDev.cardIndex = info->card;
    audioDev.type = type;
    audioDev.index = info->index;
    audioDev.name = info->name;
    audioDev.description = QString::fromUtf8(info->description);
    audioDev.channels = info->volume.channels;
    audioDev.baseVolume = info->base_volume;
    audioDev.currentVolume = info->volume.values[0];
    audioDev.volumeSteps = info->n_volume_steps;
}

