/*****************************************************************************
  Модуль предоставляет информацию по аудио-устройствам, и так же отслеживает
  изменение их состояния.
*****************************************************************************/

#pragma once

#include "wav_file.h"
#include "common/functions.h"
#include "common/voice_filters.h"
#include "common/voice_frame.h"
#include "kernel/communication/commands.h"

#include "shared/_list.h"
#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/func_invoker.h"
#include "shared/qt/communication/transport/tcp.h"

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
class AudioDev : public QObject
{
public:
    bool init();
    void start();
    bool stop(unsigned long time = ULONG_MAX);
    void terminate() {}

public slots:
    void message(const communication::Message::Ptr&);

    // Старт/стоп проигрывания звука звонка
    void startRingtone();
    void stopRingtone();

    // Старт/стоп воспроизведения голоса и звуков
    void startPlayback(const QString& fileName);
    void stopPlayback();

    void startPlaybackVoice(const VoiceFrameInfo::Ptr&);
    void stopPlaybackVoice();

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

    //--- Обработчики команд ---
    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_AudioDevChange(const Message::Ptr&);
    void command_AudioTest(const Message::Ptr&);
    void command_ToxCallState(const Message::Ptr&);

    template<typename InfoType>
    void fillAudioDevInfo(const InfoType*, data::AudioDevInfo&);

    template<typename InfoType>
    void fillAudioDevVolume(const InfoType*, data::AudioDevChange&);

    template<typename InfoType>
    data::AudioDevInfo* updateAudioDevInfo(const InfoType*, data::AudioDevInfo::List& devices);

    data::AudioDevInfo::List* getDevices(data::AudioDevType);
    data::AudioDevInfo* currentDevice(const data::AudioDevInfo::List&);
    const char* currentDeviceName(const data::AudioDevInfo::List&, QByteArray& buff);
    bool removeDevice(quint32 index, data::AudioDevType, bool byCardIndex);

    // Останавливает выполнение всех аудио-тестов
    void stopAudioTests();

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
    static void sink_input_info   (pa_context* context, const pa_sink_input_info* info,
                                   int eol, void* userdata);
    static void source_info       (pa_context* context, const pa_source_info* info,
                                   int eol, void* userdata);
    static void source_change     (pa_context* context, const pa_source_info* info,
                                   int eol, void* userdata);
    static void source_output_info(pa_context* context, const pa_source_output_info* info,
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
    pa_threaded_mainloop* _paMainLoop = {0};
    pa_mainloop_api*      _paApi = {0};
    pa_context*           _paContext = {0};

    data::AudioDevInfo::List _sinkDevices;
    data::AudioDevInfo::List _sourceDevices;
    mutable QMutex _devicesLock;

    pa_stream* _ringtoneStream = {0}; // Поток для воспроизведения звука звонка
    pa_stream* _playbackStream = {0}; // Поток для воспроизведения голоса и звуков
    pa_stream* _recordStream = {0};   // Поток для записи голоса
    mutable QMutex _streamLock;

    atomic_bool _ringtoneActive = {false};
    atomic_bool _recordActive = {false};
    atomic_bool _playbackActive = {false};
    atomic_bool _playbackVoice = {false};

    VoiceFilters _voiceFilters;

    //QByteArray _voiceData;
    //mutable std::atomic_flag _voiceDataLock = ATOMIC_FLAG_INIT;
    //SpinLocker locker(_socketLock); (void) locker;

    size_t _playbackBytes = {0};
    size_t _recordBytes = {0};

    atomic_bool _playbackTest = {false};
    atomic_bool _recordTest = {false};

    WavFile _ringtoneFile;
    WavFile _playbackFile;

    // Индикатор состояния звонка, нужен здесь для прерывания тестов
    data::ToxCallState _callState;

    // Время необходимое на воспроизведение _recordBytes
    //quint32 _recordTime = {0};

    //QQueue<QByteArray> _recordBuff;
    //mutable QMutex _recordBuffLock;

    //QFile _sourceTestFile;
    //QFile _recordTestFile;

    FunctionInvoker _funcInvoker;

    template<typename T, int> friend T& ::safe_singleton();

};
AudioDev& audioDev();

//----------------------------- Implementation -------------------------------

template<typename InfoType>
void AudioDev::fillAudioDevInfo(const InfoType* info, data::AudioDevInfo& audioDevInfo)
{
    data::AudioDevType type = data::AudioDevType::Sink;
    if (std::is_same<InfoType, pa_source_info>::value)
        type = data::AudioDevType::Source;

    audioDevInfo.cardIndex = info->card;
    audioDevInfo.type = type;
    audioDevInfo.index = info->index;
    audioDevInfo.name = info->name;
    audioDevInfo.description = QString::fromUtf8(info->description);
    audioDevInfo.channels = info->volume.channels;
    audioDevInfo.baseVolume = info->base_volume;
    audioDevInfo.volume = info->volume.values[0];
    audioDevInfo.volumeSteps = info->n_volume_steps;
}

template<typename InfoType>
void AudioDev::fillAudioDevVolume(const InfoType* info, data::AudioDevChange& audioDevChange)
{
    data::AudioDevType type = data::AudioDevType::Sink;
    if (std::is_same<InfoType, pa_source_info>::value)
        type = data::AudioDevType::Source;

    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Volume;
    audioDevChange.cardIndex = info->card;
    audioDevChange.type = type;
    audioDevChange.index = info->index;
    audioDevChange.value = info->volume.values[0];
}

template<typename InfoType>
data::AudioDevInfo* AudioDev::updateAudioDevInfo(const InfoType* info, data::AudioDevInfo::List& devices)
{
    data::AudioDevInfo* audioDevInfoPtr = 0;
    if (lst::FindResult fr = devices.find(info->name))
    {
        // Если устройство найдено, то обновляем по нему информацию
        fillAudioDevInfo(info, devices[fr.index()]);
        audioDevInfoPtr = &devices[fr.index()];
    }
    else
    {
        data::AudioDevInfo audioDevInfo;
        fillAudioDevInfo(info, audioDevInfo);

        if (devices.empty())
        {
            audioDevInfo.isCurrent = true;

            alog::Line logLine =
                alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
                << ((audioDevInfo.type == data::AudioDevType::Sink)
                    ? "Sound sink "
                    : "Sound source ");
            logLine << "is current"
                    << "; index: "  << audioDevInfo.index
                    << "; (card: "  << audioDevInfo.cardIndex << ")"
                    << "; volume: " << audioDevInfo.volume
                    << "; name: "   << audioDevInfo.name;
        }

        // Инициализация isDefault
        const char* confName =
            (audioDevInfo.type == data::AudioDevType::Sink)
            ? "audio.device.playback_default"
            : "audio.device.record_default";
        string devName;
        config::state().getValue(confName, devName);
        if (devName == audioDevInfo.name.constData())
        {
            for (int i = 0; i < devices.count(); ++i)
            {
                devices[i].isCurrent = false;
                devices[i].isDefault = false;
            }
            audioDevInfo.isCurrent = true;
            audioDevInfo.isDefault = true;

            alog::Line logLine =
                alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
                << ((audioDevInfo.type == data::AudioDevType::Sink)
                    ? "Sound sink "
                    : "Sound source ");
            logLine << "is default/current"
                    << "; index: "  << audioDevInfo.index
                    << "; (card: "  << audioDevInfo.cardIndex << ")"
                    << "; volume: " << audioDevInfo.volume
                    << "; name: "   << audioDevInfo.name;
        }

        audioDevInfoPtr = devices.addCopy(audioDevInfo);
    }

    if (configConnected())
    {
        Message::Ptr m = createMessage(*audioDevInfoPtr, Message::Type::Event);
        tcp::listener().send(m);
    }
    return audioDevInfoPtr;
}


