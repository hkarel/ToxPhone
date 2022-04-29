#include "audio_dev.h"
#include "toxphone_appl.h"
#include "common/functions.h"
#include "common/voice_filters.h"

#include "shared/break_point.h"
#include "shared/simple_ptr.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include "pproto/commands/pool.h"
#include "pproto/transport/tcp.h"

#include <string>
#include <string.h>
#include <type_traits>

#define log_error_m   alog::logger().error  (alog_line_location, "AudioDev")
#define log_warn_m    alog::logger().warn   (alog_line_location, "AudioDev")
#define log_info_m    alog::logger().info   (alog_line_location, "AudioDev")
#define log_verbose_m alog::logger().verbose(alog_line_location, "AudioDev")
#define log_debug_m   alog::logger().debug  (alog_line_location, "AudioDev")
#define log_debug2_m  alog::logger().debug2 (alog_line_location, "AudioDev")

struct MainloopLocker
{
    pa_threaded_mainloop* const paMainLoop;
    MainloopLocker(pa_threaded_mainloop* paMainLoop) : paMainLoop(paMainLoop)
    {
        pa_threaded_mainloop_lock(paMainLoop);
    }
    ~MainloopLocker()
    {
        pa_threaded_mainloop_unlock(paMainLoop);
    }
};

static string paStrError(pa_context* context)
{
    return string("; Error: ") + pa_strerror(pa_context_errno(context));
}

static string paStrError(pa_stream* stream)
{
    return string("; Error: ") + pa_strerror(pa_context_errno(pa_stream_get_context(stream)));
}

static void initChannelsVolume(const data::AudioStreamInfo& asi, pa_cvolume& volume)
{
    volume.channels = asi.channels;
    for (quint8 i = 0; i < volume.channels; ++i)
        volume.values[i] = asi.volume;
}

template<typename T> struct pa_operation_deleter
{
    static void destroy(T* x) {if (x) pa_operation_unref(x);}
};
typedef simple_ptr<pa_operation, pa_operation_deleter> pa_operation_ptr;
#define O_PTR(PTR) pa_operation_ptr(PTR)

#define O_PTR_MSG(PTR, MSG, OBJ_ERROR, RETURT_OPERATOR) { \
    pa_operation_ptr o {PTR}; \
    if (!o) { \
        log_error_m << MSG << paStrError(OBJ_ERROR); \
        RETURT_OPERATOR ; \
    } \
}

#define O_PTR_FAIL(PTR, MSG, OBJ_ERROR, RETURT_OPERATOR) { \
    pa_operation_ptr o {PTR}; \
    if (!o) { \
        log_error_m << MSG << paStrError(OBJ_ERROR) \
                    << "; Application will be stopped"; \
        Application::stop(1); \
        RETURT_OPERATOR ; \
    } \
}

//-------------------------------- AudioDev ----------------------------------

AudioDev& audioDev()
{
    return ::safe_singleton<AudioDev>();
}

AudioDev::AudioDev()
{
    chk_connect_q(&tcp::listener(), &tcp::Listener::message,
                  this, &AudioDev::message)

    _palybackAudioStreamInfo.type = data::AudioStreamInfo::Type::Playback;
    _voiceAudioStreamInfo.type    = data::AudioStreamInfo::Type::Voice;
    _recordAudioStreamInfo.type   = data::AudioStreamInfo::Type::Record;

    _playbackTimer.setSingleShot(true);

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &AudioDev::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(AudioDevChange)
    FUNC_REGISTRATION(AudioStreamInfo)
    FUNC_REGISTRATION(AudioTest)
    FUNC_REGISTRATION(ToxCallState)

    #undef FUNC_REGISTRATION
}

bool AudioDev::init()
{
    _paMainLoop = pa_threaded_mainloop_new();
    if (!_paMainLoop)
    {
        log_error_m << "Failed call pa_threaded_mainloop_new()";
        return false;
    }

    _paApi = pa_threaded_mainloop_get_api(_paMainLoop);
    if (!_paApi)
    {
        log_error_m << "Failed call pa_threaded_mainloop_get_api()";
        deinit();
        return false;
    }

    _paContext = pa_context_new(_paApi, TOX_PHONE);
    if (!_paContext)
    {
        log_error_m << "Failed pa_context_new()";
        deinit();
        return false;
    }

    pa_context_set_state_callback(_paContext, context_state, this);
    if (pa_context_connect(_paContext, NULL, PA_CONTEXT_NOFAIL, NULL) < 0)
        if (pa_context_errno(_paContext) != PA_OK)
        {
            log_error_m << "Failed connection to PulseAudio"
                        << paStrError(_paContext);
            deinit();
            return false;
        }

    return true;
}

void AudioDev::deinit()
{
    if (_playbackStream)
    {
        pa_stream_disconnect(_playbackStream);
        pa_stream_unref(_playbackStream);
        _playbackStream = nullptr;
    }
    if (_voiceStream)
    {
        pa_stream_disconnect(_voiceStream);
        pa_stream_unref(_voiceStream);
        _voiceStream = nullptr;
    }
    if (_recordStream)
    {
        pa_stream_disconnect(_recordStream);
        pa_stream_unref(_recordStream);
        _recordStream = nullptr;
    }

    if (_paContext)
    {
        pa_context_disconnect(_paContext);
        pa_context_unref(_paContext);
        _paContext = nullptr;
    }
    if (_paMainLoop)
    {
        pa_threaded_mainloop_free(_paMainLoop);
        _paMainLoop = nullptr;
    }
    _paApi = nullptr;
}

void AudioDev::playRingtone()
{
    QObject::disconnect(&_playbackTimer, nullptr, this, nullptr);
    chk_connect_a(&_playbackTimer, &QTimer::timeout, this, &AudioDev::playRingtoneByTimer);
    _playbackTimer.start(500);
}

void AudioDev::playOutgoing()
{
    QObject::disconnect(&_playbackTimer, nullptr, this, nullptr);
    chk_connect_a(&_playbackTimer, &QTimer::timeout, this, &AudioDev::playOutgoingByTimer);
    _playbackTimer.start(500);
}

void AudioDev::playBusy()
{
    QObject::disconnect(&_playbackTimer, nullptr, this, nullptr);
    chk_connect_a(&_playbackTimer, &QTimer::timeout, this, &AudioDev::playBusyByTimer);
    _playbackTimer.start(500);
}

void AudioDev::playFail()
{
    QObject::disconnect(&_playbackTimer, nullptr, this, nullptr);
    chk_connect_a(&_playbackTimer, &QTimer::timeout, this, &AudioDev::playFailByTimer);
    _playbackTimer.start(500);
}

void AudioDev::playError()
{
    QObject::disconnect(&_playbackTimer, nullptr, this, nullptr);
    chk_connect_a(&_playbackTimer, &QTimer::timeout, this, &AudioDev::playErrorByTimer);
    _playbackTimer.start(500);
}

void AudioDev::playFake()
{
    // 'Проигрываем' фиктивный звук
    data::PlaybackFinish playbackFinish;
    playbackFinish.code = data::PlaybackFinish::Code::Fake;

    Message::Ptr m = createMessage(playbackFinish);
    emit internalMessage(m);
}

void AudioDev::playRingtoneByTimer()
{
    startPlayback("sound/ringtone.wav", std::numeric_limits<int>::max(),
                  data::PlaybackFinish::Code::Ringtone);
}

void AudioDev::playOutgoingByTimer()
{
    startPlayback("sound/outgoing_call.wav", std::numeric_limits<int>::max(),
                  data::PlaybackFinish::Code::Outgoing);
}

void AudioDev::playBusyByTimer()
{
    startPlayback("sound/busy.wav", 8, data::PlaybackFinish::Code::Busy);
}

void AudioDev::playFailByTimer()
{
    startPlayback("sound/fail.wav", 1, data::PlaybackFinish::Code::Fail);
}

void AudioDev::playErrorByTimer()
{
    startPlayback("sound/error.wav", 1, data::PlaybackFinish::Code::Error);
}

void AudioDev::startPlayback(const QString& fileName, int cycleCount,
                             data::PlaybackFinish::Code playbackFinishCode)
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_playbackActive)
        return;

    QString filePath = getFilePath(fileName);
    if (filePath.isEmpty())
    {
        log_error_m << "File " << fileName << " not found";
        return;
    }
    if (_playbackFile.isOpen())
        _playbackFile.close();

    _playbackFile.setFileName(filePath);
    if (!_playbackFile.open())
    {
        log_error_m << "Failed open the playback file " << filePath;
        return;
    }
    if (_playbackFile.header().bitsPerSample != 16)
    {
        log_error_m << "Only 16 bits per sample is supported"
                    << ". FIle: " << _playbackFile.fileName();
        _playbackFile.close();
        return;
    }

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    if (_playbackStream)
    {
        log_debug_m << "Playback stream drop";
        pa_stream_set_write_callback(_playbackStream, 0, 0);
        if (pa_stream_disconnect(_playbackStream) < 0)
        {
            log_error_m << "Failed call pa_stream_disconnect()"
                        << paStrError(_playbackStream);
        }
        pa_stream_unref(_playbackStream);
        log_debug_m << "Playback stream dropped";
    }

    log_debug_m << "Create playback stream";

    pa_sample_spec paSampleSpec;
    paSampleSpec.format = PA_SAMPLE_S16LE;
    paSampleSpec.channels = _playbackFile.header().numChannels;
    paSampleSpec.rate = _playbackFile.header().sampleRate;

    _playbackStream = pa_stream_new(_paContext, "Playback", &paSampleSpec, 0);
    if (!_playbackStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    pa_stream_set_state_callback    (_playbackStream, playback_stream_state, this);
    pa_stream_set_started_callback  (_playbackStream, playback_stream_started, this);
    pa_stream_set_write_callback    (_playbackStream, playback_stream_write, this);
    pa_stream_set_overflow_callback (_playbackStream, playback_stream_overflow, this);
    pa_stream_set_underflow_callback(_playbackStream, playback_stream_underflow, this);
    pa_stream_set_suspended_callback(_playbackStream, playback_stream_suspended, this);
    pa_stream_set_moved_callback    (_playbackStream, playback_stream_moved, this);

    QByteArray devNameBuff;
    const char* devName = currentDeviceName(_sinkDevices, devNameBuff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
    if (pa_stream_connect_playback(_playbackStream, devName, 0, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_playbackStream);
        return;
    }
    _playbackActive = true;
    _playbackCycleCount = cycleCount;
    _playbackFinish.code = playbackFinishCode;
    log_debug_m << "Playback stream start (file: " << _playbackFile.fileName() << ")";
}

void AudioDev::stopPlayback()
{
    _playbackTimer.stop();
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_playbackActive)
        return;

    log_debug_m << "Playback stream stop";

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;
    pa_stream_set_write_callback(_playbackStream, 0, 0);
    playback_stream_drain(_playbackStream, false, this);

    _playbackStream = 0;
    _playbackActive = false;
}

void AudioDev::startVoice(const VoiceFrameInfo::Ptr& voiceFrameInfo)
{
    stopPlayback();

    QMutexLocker locker(&_streamLock); (void) locker;

    if (_voiceActive)
        return;

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    if (_voiceStream)
    {
        log_debug_m << "Voice stream drop";
        pa_stream_set_write_callback(_voiceStream, 0, 0);
        if (pa_stream_disconnect(_voiceStream) < 0)
        {
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_voiceStream);
        }
        pa_stream_unref(_voiceStream);
        log_debug_m << "Voice stream dropped";
    }

    log_debug_m << "Create voice stream";
    _voiceBytes = 0;

    pa_sample_spec paSampleSpec;
    paSampleSpec.format = PA_SAMPLE_S16LE;
    paSampleSpec.channels = voiceFrameInfo->channels;
    paSampleSpec.rate = voiceFrameInfo->samplingRate;

    _voiceStream = pa_stream_new(_paContext, "Voice", &paSampleSpec, 0);
    if (!_voiceStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    getVoiceFrameInfo(voiceFrameInfo.get());
    log_debug_m << "Initialization playback VoiceFrameInfo"
                << "; latency: "       << voiceFrameInfo->latency
                << "; channels: "  << int(voiceFrameInfo->channels)
                << "; sample size: "   << voiceFrameInfo->sampleSize
                << "; sample count: "  << voiceFrameInfo->sampleCount
                << "; sampling rate: " << voiceFrameInfo->samplingRate
                << "; buffer size: "   << voiceFrameInfo->bufferSize;

    pa_stream_set_state_callback    (_voiceStream, voice_stream_state, this);
    pa_stream_set_started_callback  (_voiceStream, voice_stream_started, this);
    pa_stream_set_write_callback    (_voiceStream, voice_stream_write, this);
    pa_stream_set_overflow_callback (_voiceStream, voice_stream_overflow, this);
    pa_stream_set_underflow_callback(_voiceStream, voice_stream_underflow, this);
    pa_stream_set_suspended_callback(_voiceStream, voice_stream_suspended, this);
    pa_stream_set_moved_callback    (_voiceStream, voice_stream_moved, this);

    //size_t latency = 20000;
    size_t latency = voiceFrameInfo->latency;
    pa_buffer_attr paBuffAttr;
    paBuffAttr.maxlength = 3 * pa_usec_to_bytes(latency, &paSampleSpec);
    paBuffAttr.tlength   = pa_usec_to_bytes(latency, &paSampleSpec);
    paBuffAttr.prebuf    = uint32_t(-1);
    paBuffAttr.minreq    = uint32_t(-1);
    paBuffAttr.fragsize  = uint32_t(-1);

    QByteArray devNameBuff;
    const char* devName = currentDeviceName(_sinkDevices, devNameBuff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    if (pa_stream_connect_playback(_voiceStream, devName, &paBuffAttr, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_voiceStream);
        return;
    }
    _voiceActive = true;
    log_debug_m << "Voice stream start";
}

void AudioDev::stopVoice()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_voiceActive)
        return;

    log_debug_m << "Voice stream stop";

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    pa_stream_set_write_callback(_voiceStream, 0, 0);
    if (pa_stream_disconnect(_voiceStream) < 0)
    {
        log_error_m << "Failed call _voiceStream()"
                    << paStrError(_voiceStream);
    }
    pa_stream_unref(_voiceStream);
    _voiceStream = 0;

    log_debug_m << "Voice ring buffer available: " << voiceRBuff().available();

    voiceRBuff().reset();
    getVoiceFrameInfo(0, true);

    log_debug_m << "Voice bytes (processed): " << _voiceBytes;
    log_debug_m << "Voice stream stopped";

    _voiceBytes = 0;
    _voiceActive = false;
}

void AudioDev::startRecord()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_recordActive)
        return;

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    if (_recordStream)
    {
        log_debug_m << "Record stream drop";
        pa_stream_set_read_callback(_recordStream, 0, 0);

        if (pa_stream_disconnect(_recordStream) < 0)
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_recordStream);

        pa_stream_unref(_recordStream);
        log_debug_m << "Record stream dropped";
    }

    log_debug_m << "Create record stream";
    _recordBytes = 0;

    pa_sample_spec paSampleSpec;
    paSampleSpec.format = PA_SAMPLE_S16LE;
    //paSampleSpec.channels = 2;
    paSampleSpec.channels = 1;   // Чтобы фильтр шумоподавления работал корректно
                                 // нужно использовать только один канал.
                                 // Если же требуются два канала, то для каждого
                                 // канала необходимо использовать свой экземпляр
                                 // фильтра.
    paSampleSpec.rate = 48000;

    _recordStream = pa_stream_new(_paContext, "Record", &paSampleSpec, 0);
    if (!_recordStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    quint32 latency = 20000; /* в микросекундах */
    quint32 bufferSize = pa_usec_to_bytes(latency, &paSampleSpec);
    quint32 sampleSize = pa_sample_size(&paSampleSpec);
    quint32 sampleCount = bufferSize / (sampleSize * paSampleSpec.channels);
    VoiceFrameInfo voiceFrameInfo {latency, paSampleSpec.channels, sampleSize,
                                   sampleCount, paSampleSpec.rate, bufferSize};

    getRecordFrameInfo(&voiceFrameInfo);
    log_debug_m << "Initialization record VoiceFrameInfo"
                << "; latency: "       << voiceFrameInfo.latency
                << "; channels: "  << int(voiceFrameInfo.channels)
                << "; sample size: "   << voiceFrameInfo.sampleSize
                << "; sample count: "  << voiceFrameInfo.sampleCount
                << "; sampling rate: " << voiceFrameInfo.samplingRate
                << "; buffer size: "   << voiceFrameInfo.bufferSize;

    pa_stream_set_state_callback    (_recordStream, record_stream_state, this);
    pa_stream_set_started_callback  (_recordStream, record_stream_started, this);
    pa_stream_set_read_callback     (_recordStream, record_stream_read, this);
    pa_stream_set_overflow_callback (_recordStream, record_stream_overflow, this);
    pa_stream_set_underflow_callback(_recordStream, record_stream_underflow, this);
    pa_stream_set_suspended_callback(_recordStream, record_stream_suspended, this);
    pa_stream_set_moved_callback    (_recordStream, record_stream_moved, this);

    pa_buffer_attr paBuffAttr;
    paBuffAttr.maxlength = 3 * pa_usec_to_bytes(latency, &paSampleSpec);
    paBuffAttr.tlength   = uint32_t(-1);
    paBuffAttr.prebuf    = uint32_t(-1);
    paBuffAttr.minreq    = uint32_t(-1);
    paBuffAttr.fragsize  = pa_usec_to_bytes(latency, &paSampleSpec);

    QByteArray devNameBuff;
    const char* devName = currentDeviceName(_sourceDevices, devNameBuff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    if (pa_stream_connect_record(_recordStream, devName, &paBuffAttr, flags) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_record()"
                    << paStrError(_recordStream);
        return;
    }
    voiceFilters().start();
    _recordActive = true;
    log_debug_m << "Record stream start";
}

void AudioDev::stopRecord()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_recordActive)
        return;

    log_debug_m << "Record stream stop";

    if (_recordTest)
    {
        if (toxConfig().isActive())
        {
            data::AudioTest audioTest;
            audioTest.begin = false;
            audioTest.record = true;

            Message::Ptr m = createMessage(audioTest);
            toxConfig().send(m);
        }
        _recordTest = false;
    }

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    pa_stream_set_read_callback(_recordStream, nullptr, nullptr);
    if (pa_stream_disconnect(_recordStream) < 0)
    {
        log_error_m << "Failed call pa_stream_disconnect()"
                    << paStrError(_recordStream);
    }
    pa_stream_unref(_recordStream);
    _recordStream = 0;

    log_debug_m << "Record ring buffer (1) available: " << recordRBuff_1().available();
    log_debug_m << "Record ring buffer (2) available: " << recordRBuff_2().available();

    voiceFilters().stop();
    recordRBuff_1().reset();
    recordRBuff_2().reset();
    getRecordFrameInfo(0, true);

    log_debug_m << "Record bytes (processed): " << _recordBytes;
    log_debug_m << "Record stream stopped";

    _recordBytes = 0;
    _recordActive = false;
}

void AudioDev::stopAudioTests()
{
    if (_playbackTest)
        stopPlayback();

    if (_recordTest)
        stopRecord();
}

void AudioDev::readAudioStreamVolume(data::AudioStreamInfo& streamInfo, const char* confKey)
{
    string key;
    bool personalVolume = false;
    if (_callState.direction != data::ToxCallState::Direction::Undefined
        && !_callState.friendPublicKey.isEmpty())
    {
        key = "phones." + string(_callState.friendPublicKey) + ".audio_streams.active";
        config::state().getValue(key, personalVolume, false);
        if (personalVolume)
        {
            key = "phones." + string(_callState.friendPublicKey) + ".audio_streams." + confKey;
            if (!config::state().getValue(key, streamInfo.volume, false))
                personalVolume = false;
        }
    }
    if (!personalVolume)
    {
        key = string("audio.streams.") + confKey;
        config::state().getValue(key, streamInfo.volume);
    }
}

void AudioDev::saveAudioStreamVolume(data::AudioStreamInfo& streamInfo, const char* confKey)
{
    string key;
    bool personalVolume = false;
    if (_callState.direction != data::ToxCallState::Direction::Undefined
        && !_callState.friendPublicKey.isEmpty())
    {
        key = "phones." + string(_callState.friendPublicKey) + ".audio_streams.active";
        config::state().getValue(key, personalVolume, false);
        if (personalVolume)
        {
            key = "phones." + string(_callState.friendPublicKey) + ".audio_streams." + confKey;
            config::state().setValue(key, streamInfo.volume);
        }
    }
    if (!personalVolume)
    {
        key = string("audio.streams.") + confKey;
        config::state().setValue(key, streamInfo.volume);
    }
    config::state().saveFile();
}

bool AudioDev::start()
{
    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;
    if (pa_threaded_mainloop_start(_paMainLoop) < 0)
    {
        log_error_m << "Failed call pa_threaded_mainloop_start()";
        return false;
    }
    return true;
}

bool AudioDev::stop(unsigned long /*time*/)
{
    //stopRingtone();
    stopPlayback();
    stopVoice();
    stopRecord();

    if (_paMainLoop)
        pa_threaded_mainloop_stop(_paMainLoop);

    deinit();
    return true;
}

void AudioDev::message(const pproto::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (lst::FindResult fr = _funcInvoker.findCommand(message->command()))
    {
        if (!command::pool().commandIsMultiproc(message->command()))
            message->markAsProcessed();
        _funcInvoker.call(message, fr);
    }
}

void AudioDev::command_IncomingConfigConnection(const Message::Ptr& /*message*/)
{
    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    Message::Ptr m;
    for (int i = 0; i < _sinkDevices.count(); ++i)
    {
        m = createMessage(_sinkDevices[i]);
        toxConfig().send(m);
    }
    for (int i = 0; i < _sourceDevices.count(); ++i)
    {
        m = createMessage(_sourceDevices[i]);
        toxConfig().send(m);
    }

    // Отправляем фиктивные сообщения о создании потоков, чтобы правильно
    // проинициализировать настройки конфигуратора
    data::AudioStreamInfo audioStreamInfo;
    if (_palybackAudioStreamInfo.state == data::AudioStreamInfo::State::Changed)
    {
        audioStreamInfo = _palybackAudioStreamInfo;
        audioStreamInfo.state = data::AudioStreamInfo::State::Created;

        m = createMessage(audioStreamInfo);
        toxConfig().send(m);
    }
    if (_voiceAudioStreamInfo.state == data::AudioStreamInfo::State::Changed)
    {
        audioStreamInfo = _voiceAudioStreamInfo;
        audioStreamInfo.state = data::AudioStreamInfo::State::Created;

        m = createMessage(audioStreamInfo);
        toxConfig().send(m);
    }
    if (_recordAudioStreamInfo.state == data::AudioStreamInfo::State::Changed)
    {
        audioStreamInfo = _recordAudioStreamInfo;
        audioStreamInfo.state = data::AudioStreamInfo::State::Created;

        m = createMessage(audioStreamInfo);
        toxConfig().send(m);
    }

    m = createMessage(_palybackAudioStreamInfo);
    toxConfig().send(m);

    m = createMessage(_voiceAudioStreamInfo);
    toxConfig().send(m);

    m = createMessage(_recordAudioStreamInfo);
    toxConfig().send(m);
}

void AudioDev::command_AudioDevChange(const Message::Ptr& message)
{
    data::AudioDevChange audioDevChange;
    readFromMessage(message, audioDevChange);

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;
    data::AudioDevInfo::List* devices = getDevices(audioDevChange.type);

    // ChangeFlag::Volume
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Volume)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.failed())
        {
            log_error_m << "Failed change a device volume level. "
                        << (audioDevChange.type == data::AudioDevType::Sink ? "Sink " : "Source ")
                        << "device not found by index: " << audioDevChange.index;
            return;
        }
        data::AudioDevInfo* audioDevInfo = devices->item(fr.index());
        audioDevInfo->volume = audioDevChange.value;

        pa_cvolume volume;
        volume.channels = audioDevInfo->channels;
        for (quint8 i = 0; i < volume.channels; ++i)
            volume.values[i] = audioDevInfo->volume;

        if (audioDevInfo->type == data::AudioDevType::Sink)
        {
            O_PTR_MSG(pa_context_set_sink_volume_by_index(
                      _paContext, audioDevInfo->index, &volume, nullptr, nullptr),
                      "Failed call pa_context_set_sink_volume_by_index()", _paContext, {})
        }
        else
        {
            O_PTR_MSG(pa_context_set_source_volume_by_index(
                      _paContext, audioDevInfo->index, &volume, nullptr, nullptr),
                      "Failed call pa_context_set_source_volume_by_index()", _paContext, {})
        }
    }

    // ChangeFlag::Current
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Current)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.failed())
        {
            log_error_m << "Failed set device as current. "
                        << (audioDevChange.type == data::AudioDevType::Sink ? "Sink " : "Source ")
                        << "device not found by index: " << audioDevChange.index;
            return;
        }

        for (int i = 0; i < devices->count(); ++i)
            devices->item(i)->isCurrent = false;

        data::AudioDevInfo* audioDevInfo = devices->item(fr.index());
        audioDevInfo->isCurrent = true;

        alog::Line logLine = log_verbose_m
            << ((audioDevInfo->type == data::AudioDevType::Sink)
                ? "Sound sink "
                : "Sound source ");
        logLine << "set as current"
                << "; index: "  << audioDevInfo->index
                << "; (card: "  << audioDevInfo->cardIndex << ")"
                << "; volume: " << audioDevInfo->volume
                << "; name: "   << audioDevInfo->name;


        if (toxConfig().isActive())
        {
            data::AudioDevChange audioDevChange {*audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

            Message::Ptr m = createMessage(audioDevChange);
            toxConfig().send(m);
        }
    }

    // ChangeFlag::Default
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Default)
    {
        lst::FindResult fr = devices->findRef(audioDevChange.index);
        if (fr.failed())
        {
            log_error_m << "Failed set device as default. "
                        << (audioDevChange.type == data::AudioDevType::Sink ? "Sink " : "Source ")
                        << "device not found by index: " << audioDevChange.index;
            return;
        }

        for (int i = 0; i < devices->count(); ++i)
        {
            devices->item(i)->isCurrent = false;
            devices->item(i)->isDefault = false;
        }

        data::AudioDevInfo* audioDevInfo = devices->item(fr.index());
        audioDevInfo->isCurrent = true;
        audioDevInfo->isDefault = true;

        const char* confName =
            (audioDevInfo->type == data::AudioDevType::Sink)
            ? "audio.devices.playback_default"
            : "audio.devices.record_default";
        config::state().setValue(confName, audioDevInfo->name.toStdString() /*.constData()*/);
        config::state().saveFile();

        alog::Line logLine = log_verbose_m
            << ((audioDevInfo->type == data::AudioDevType::Sink)
                ? "Sound sink "
                : "Sound source ");
        logLine << "set as default"
                << "; index: "  << audioDevInfo->index
                << "; (card: "  << audioDevInfo->cardIndex << ")"
                << "; volume: " << audioDevInfo->volume
                << "; name: "   << audioDevInfo->name;

        if (toxConfig().isActive())
        {
            data::AudioDevChange audioDevChange {*audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Default;

            Message::Ptr m = createMessage(audioDevChange);
            toxConfig().send(m);
        }
    }
}

void AudioDev::command_AudioStreamInfo(const Message::Ptr& message)
{
    data::AudioStreamInfo audioStreamInfo;
    readFromMessage(message, audioStreamInfo);

    MainloopLocker mainloopLocker(_paMainLoop); (void) mainloopLocker;

    if (audioStreamInfo.state == data::AudioStreamInfo::State::Created
        || audioStreamInfo.state == data::AudioStreamInfo::State::Changed)
    {
        pa_cvolume volume;
        if (audioStreamInfo.type == data::AudioStreamInfo::Type::Playback)
        {
            _palybackAudioStreamInfo.volume = audioStreamInfo.volume;
            log_debug2_m << "Set playback stream volume: " << _palybackAudioStreamInfo.volume
                         << "; index: " << _palybackAudioStreamInfo.index;

            initChannelsVolume(_palybackAudioStreamInfo, volume);
            if (O_PTR(pa_context_set_sink_input_volume(
                      _paContext, _palybackAudioStreamInfo.index, &volume, nullptr, nullptr)))
            {
                if (audioStreamInfo.state == data::AudioStreamInfo::State::Changed)
                {
                    /* Для потока воспроизведения звуков нет возможности сохранить
                       персональный уровень громкости, да это и не нужно делать.
                       saveAudioStreamVolume(_palybackAudioStreamInfo, "playback_volume");
                    */
                    config::state().setValue("audio.streams.playback_volume",
                                             _palybackAudioStreamInfo.volume);
                    config::state().saveFile();
                }
            }
            else
                log_error_m << "Failed call pa_context_set_sink_input_volume()"
                            << paStrError(_paContext);
        }
        else if (audioStreamInfo.type == data::AudioStreamInfo::Type::Voice)
        {
            _voiceAudioStreamInfo.volume = audioStreamInfo.volume;
            log_debug2_m << "Set voice stream volume: " << _voiceAudioStreamInfo.volume
                         << "; index: " << _voiceAudioStreamInfo.index;

            initChannelsVolume(_voiceAudioStreamInfo, volume);
            if (O_PTR(pa_context_set_sink_input_volume(
                      _paContext, _voiceAudioStreamInfo.index, &volume, nullptr, nullptr)))
            {
                if (audioStreamInfo.state == data::AudioStreamInfo::State::Changed)
                    saveAudioStreamVolume(_voiceAudioStreamInfo, "voice_volume");
            }
            else
                log_error_m << "Failed call pa_context_set_sink_input_volume()"
                            << paStrError(_paContext);
        }
        else if (audioStreamInfo.type == data::AudioStreamInfo::Type::Record)
        {
            _recordAudioStreamInfo.volume = audioStreamInfo.volume;
            log_debug2_m << "Set record stream volume: " << _recordAudioStreamInfo.volume
                         << "; index: " << _recordAudioStreamInfo.index;

            initChannelsVolume(_recordAudioStreamInfo, volume);
            if (O_PTR(pa_context_set_source_output_volume(
                      _paContext, _recordAudioStreamInfo.index, &volume, nullptr, nullptr)))
            {
                if (audioStreamInfo.state == data::AudioStreamInfo::State::Changed)
                    saveAudioStreamVolume(_recordAudioStreamInfo, "record_volume");
            }
            else
                log_error_m << "Failed call pa_context_set_source_output_volume()"
                            << paStrError(_paContext);
        }
    }
}

void AudioDev::command_AudioTest(const Message::Ptr& message)
{
    data::AudioTest audioTest;
    readFromMessage(message, audioTest);

    if (_callState.direction != data::ToxCallState::Direction::Undefined)
    {
        Message::Ptr answer = message->cloneForAnswer();

        data::MessageFailed failed;
        failed.description = tr("A call is in progress, the test cannot be started");
        writeToMessage(failed, answer);

        tcp::listener().send(answer);
        return;
    }

    if (audioTest.playback)
    {
        if (audioTest.begin)
        {
            stopPlayback();
            startPlayback("sound/test.wav", 1, data::PlaybackFinish::Code::Test);
            _playbackTest = true;
        }
        else
            stopPlayback();
    }
    if (audioTest.record)
    {
        if (audioTest.begin)
        {
            startRecord();
            _recordTest = true;

            /*** Отладка эхоподавления ***/
            //VoiceFrameInfo::Ptr voiceFrameInfo = getRecordFrameInfo();
            //startVoice(voiceFrameInfo);
            /***/

        }
        else
        {
            stopRecord();

            /*** Отладка эхоподавления ***/
            //stopVoice();
            /***/
        }
    }
}

void AudioDev::command_ToxCallState(const Message::Ptr& message)
{
    readFromMessage(message, _callState);

    if (_callState.direction != data::ToxCallState::Direction::Undefined)
    {
        stopAudioTests();
    }

    if (_callState.direction == data::ToxCallState::Direction::Incoming
        && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        if (!diverterIsActive())
            playRingtone();
    }
    else if (_callState.direction == data::ToxCallState::Direction::Incoming
             && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        stopPlayback();
        startRecord();
    }
    else if (_callState.direction == data::ToxCallState::Direction::Outgoing
             && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
    {
        playOutgoing();
    }
    else if (_callState.direction == data::ToxCallState::Direction::Outgoing
             && _callState.callState == data::ToxCallState::CallState::InProgress)
    {
        stopPlayback();
        startRecord();
    }
    else if (_callState.direction == data::ToxCallState::Direction::Undefined
             && _callState.callState == data::ToxCallState::CallState::IsComplete)
    {
        stopVoice();
        stopRecord();

        if (_callState.callEnd == data::ToxCallState::CallEnd::Undefined
            || _callState.callEnd == data::ToxCallState::CallEnd::SelfEnd
            || _callState.callEnd == data::ToxCallState::CallEnd::SelfReject
            || _callState.callEnd == data::ToxCallState::CallEnd::FriendEnd)
        {
            if (_playbackActive)
            {
                _emitPlaybackFinish = false;
                stopPlayback();
            }
            playFake();
        }
        else if (_callState.callEnd == data::ToxCallState::CallEnd::FriendBusy
                 || _callState.callEnd == data::ToxCallState::CallEnd::FriendReject)
        {
            if (_playbackActive)
            {
                _emitPlaybackFinish = false;
                stopPlayback();
            }
            playBusy();
        }
        else if (_callState.callEnd == data::ToxCallState::CallEnd::NotConnected)
        {
            if (_playbackActive)
            {
                _emitPlaybackFinish = false;
                stopPlayback();
            }
            playFail();
        }
        else if (_callState.callEnd == data::ToxCallState::CallEnd::Error)
        {
            if (_playbackActive)
            {
                _emitPlaybackFinish = false;
                stopPlayback();
            }
            playError();
        }
        else
        {
            stopPlayback();
        }
    }
    else  if (_callState.direction == data::ToxCallState::Direction:: Undefined
              && _callState.callState == data::ToxCallState::CallState::Undefined)
    {
        stopVoice();
        stopRecord();
        stopPlayback();
    }
}

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
void AudioDev::updateAudioDevInfo(const InfoType* info, data::AudioDevInfo::List& devices)
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

        enum DeviceDefault {Yes = true, No = false};
        auto setCurrentDevice = [&audioDevInfo, &devices](DeviceDefault devDefault, int  line)
        {
            for (int i = 0; i < devices.count(); ++i)
            {
                devices[i].isCurrent = false;
                devices[i].isDefault = false;
            }
            audioDevInfo.isCurrent = true;
            audioDevInfo.isDefault = bool(devDefault);

            alog::Line logLine =
                alog::logger().verbose(__FILE__, __func__, line, "AudioDev")
                << ((audioDevInfo.type == data::AudioDevType::Sink)
                    ? "Sound sink"
                    : "Sound source");
            logLine << " current: "  << audioDevInfo.isCurrent
                    << "; default: " << audioDevInfo.isDefault
                    << "; index: "   << audioDevInfo.index
                    << "; (card: "   << audioDevInfo.cardIndex << ")"
                    << "; volume: "  << audioDevInfo.volume
                    << "; name: "    << audioDevInfo.name;
        };

        if (devices.empty())
            setCurrentDevice(DeviceDefault::No, __LINE__);

        // Инициализация isDefault
        const char* confName =
            (audioDevInfo.type == data::AudioDevType::Sink)
            ? "audio.devices.playback_default"
            : "audio.devices.record_default";
        string devName;
        config::state().getValue(confName, devName);
        if (devName == audioDevInfo.name.constData())
        {
            setCurrentDevice(DeviceDefault::Yes, __LINE__);
        }
        else if (devName.empty())
        {
            bool defaultDeviceAssigned = false;
            for (int i = 0; i < devices.count(); ++i)
                if (devices[i].isDefault)
                {
                    defaultDeviceAssigned = true;
                    break;
                }

            if (!defaultDeviceAssigned)
            {
                static QRegExp reg {R"(.*Yealink.*VOIP_USB_Phone.*)"};
                if (reg.exactMatch(audioDevInfo.name))
                    setCurrentDevice(DeviceDefault::No, __LINE__);
            }
        }
        audioDevInfoPtr = devices.addCopy(audioDevInfo);
        updateStartVolume(audioDevInfo);
    }

    if (toxConfig().isActive())
    {
        Message::Ptr m = createMessage(*audioDevInfoPtr);
        toxConfig().send(m);
    }
}

void AudioDev::updateStartVolume(const data::AudioDevInfo& audioDevInfo)
{
    QVector<QString> knownDevices;
    config::state().getValue("audio.known_devices", knownDevices);
    if (!knownDevices.contains(audioDevInfo.name))
    {
        knownDevices.append(audioDevInfo.name);
        config::state().setValue("audio.known_devices", knownDevices);
        config::state().setNodeStyle("audio.known_devices", YAML::EmitterStyle::Block);
        config::state().saveFile();

        data::AudioDevChange audioDevChange;
        audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Volume;
        audioDevChange.cardIndex  = audioDevInfo.cardIndex;
        audioDevChange.type       = audioDevInfo.type;
        audioDevChange.index      = audioDevInfo.index;
        audioDevChange.value      = audioDevInfo.volumeSteps * 0.85;

        Message::Ptr m = createMessage(audioDevChange);
        QMetaObject::invokeMethod(this, "message", Qt::QueuedConnection,
                                  Q_ARG(pproto::Message::Ptr, m));
    }
}

void AudioDev::fillAudioStreamInfo(const pa_sink_input_info* info,
                                   data::AudioStreamInfo& audioStreamInfo)
{
    //audioStreamInfo.index = info->index;
    audioStreamInfo.devIndex = info->sink;
    audioStreamInfo.name = QString::fromUtf8(info->name);
    audioStreamInfo.hasVolume = info->has_volume;
    audioStreamInfo.volumeWritable = info->volume_writable;
    audioStreamInfo.channels = info->channel_map.channels;
    audioStreamInfo.volume = info->volume.values[0];

    lst::FindResult fr = _sinkDevices.findRef(info->sink);
    audioStreamInfo.volumeSteps =
        (fr.success()) ? _sinkDevices[fr.index()].volumeSteps : 0;
}

void AudioDev::fillAudioStreamInfo(const pa_source_output_info* info,
                                   data::AudioStreamInfo& audioStreamInfo)
{
    //audioStreamInfo.index = info->index;
    audioStreamInfo.devIndex = info->source;
    audioStreamInfo.name = QString::fromUtf8(info->name);
    audioStreamInfo.hasVolume = info->has_volume;
    audioStreamInfo.volumeWritable = info->volume_writable;
    audioStreamInfo.channels = info->channel_map.channels;
    audioStreamInfo.volume = info->volume.values[0];

    lst::FindResult fr = _sourceDevices.findRef(info->source);
    audioStreamInfo.volumeSteps =
        (fr.success()) ? _sourceDevices[fr.index()].volumeSteps : 0;
}

data::AudioDevInfo::List* AudioDev::getDevices(data::AudioDevType type)
{
    return (type == data::AudioDevType::Sink) ? &_sinkDevices : &_sourceDevices;
}

data::AudioDevInfo* AudioDev::currentDevice(const data::AudioDevInfo::List& devices)
{
    for (int i = 0; i < devices.count(); ++i)
        if (devices.item(i)->isCurrent)
            return devices.item(i);

    return 0;
}

const char* AudioDev::currentDeviceName(const data::AudioDevInfo::List& devices,
                                        QByteArray& buff)
{
    if (data::AudioDevInfo* audioDevInfo = currentDevice(devices))
        buff = audioDevInfo->name;

    return (!buff.isEmpty()) ? buff.constData() : 0;
}

bool AudioDev::removeDevice(quint32 index, data::AudioDevType type, bool byCardIndex)
{
    bool isRemoved = false;
    bool isCurrent = false;
    data::AudioDevInfo::List* devices = getDevices(type);
    for (int i = 0; i < devices->count(); ++i)
    {
        quint32 devIndex = (byCardIndex) ? devices->at(i).cardIndex
                                         : devices->at(i).index;
        if (devIndex == index)
        {
            if (toxConfig().isActive())
            {
                data::AudioDevChange audioDevChange {devices->at(i)};
                audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Remove;

                Message::Ptr m = createMessage(audioDevChange);
                toxConfig().send(m);
            }
            isCurrent |= devices->at(i).isCurrent;
            devices->remove(i--);
            isRemoved = true;
        }
    }

    if (isRemoved && isCurrent && !devices->empty())
    {
        // Сбрасываем флаг текущего устройства
        for (int i = 0; i < devices->count(); ++i)
            (*devices)[i].isCurrent = false;

        // Выбираем новое текущее устройство
        isCurrent = false;
        for (int i = 0; i < devices->count(); ++i)
            if (devices->at(i).isDefault)
            {
                (*devices)[i].isCurrent = true;

                if (toxConfig().isActive())
                {
                    Message::Ptr m = createMessage(devices->at(i));
                    toxConfig().send(m);

                    data::AudioDevChange audioDevChange {devices->at(i)};
                    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

                    m = createMessage(audioDevChange);
                    toxConfig().send(m);
                }
                isCurrent = true;
                break;
            }

        if (!isCurrent)
        {
            // Если новое текущее устройство не выбрали, то делаем текущим
            // первое устройство.
            (*devices)[0].isCurrent = true;

            if (toxConfig().isActive())
            {
                Message::Ptr m = createMessage(devices->at(0));
                toxConfig().send(m);

                data::AudioDevChange audioDevChange {devices->at(0)};
                audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

                m = createMessage(audioDevChange);
                toxConfig().send(m);
            }
        }
    }
    return isRemoved;
}

//-------------------------- PulseAudio callback -----------------------------

void AudioDev::context_state(pa_context* context, void* userdata)
{
    log_debug2_m << "context_state()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);

    switch (pa_context_get_state(context))
    {
        case PA_CONTEXT_UNCONNECTED:
            log_debug2_m << "Context event: PA_CONTEXT_UNCONNECTED";
            break;

        case PA_CONTEXT_CONNECTING:
            log_debug2_m << "Context event: PA_CONTEXT_CONNECTING";
            break;

        case PA_CONTEXT_AUTHORIZING:
            log_debug2_m << "Context event: PA_CONTEXT_AUTHORIZING";
            break;

        case PA_CONTEXT_SETTING_NAME:
            log_debug2_m << "Context event: PA_CONTEXT_SETTING_NAME";
            break;

        case PA_CONTEXT_READY:
            log_debug2_m << "Context event: PA_CONTEXT_READY";

            pa_context_set_subscribe_callback(context, context_subscribe, ad);
            O_PTR_FAIL(pa_context_subscribe(context,
                                            pa_subscription_mask_t(
                                            PA_SUBSCRIPTION_MASK_SINK
                                            |PA_SUBSCRIPTION_MASK_SOURCE
                                            |PA_SUBSCRIPTION_MASK_SINK_INPUT
                                            |PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT
                                            //|PA_SUBSCRIPTION_MASK_CLIENT
                                            //|PA_SUBSCRIPTION_MASK_SERVER
                                            |PA_SUBSCRIPTION_MASK_CARD),
                                            0, 0),
                       "Failed call pa_context_subscribe()", context,
                       break)

            O_PTR_FAIL(pa_context_get_card_info_list(context, card_info, ad),
                       "Failed call pa_context_get_card_info_list()", context, {})
            break;

        case PA_CONTEXT_FAILED:
            break_point

            pa_context_unref(ad->_paContext);
            ad->_paContext = 0;

            log_error_m << "Sink event: PA_CONTEXT_FAILED"
                        << paStrError(ad->_paContext);
            Application::stop(1);
            break;

        case PA_CONTEXT_TERMINATED:
            log_debug2_m << "Context event: PA_CONTEXT_TERMINATED";

        default:
            break;
    }
}

void AudioDev::context_subscribe(pa_context* context, pa_subscription_event_type_t type,
                                 uint32_t index, void* userdata)
{
    log_debug2_m << "context_subscribe()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);

    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
    {
        case PA_SUBSCRIPTION_EVENT_CARD:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_verbose_m << "Sound card detached; index: " << index;

                ad->removeDevice(index, data::AudioDevType::Sink, true);
                ad->removeDevice(index, data::AudioDevType::Source, true);
            }
            else
            {
                O_PTR_MSG(pa_context_get_card_info_by_index(context, index, card_info, ad),
                          "Failed call pa_context_get_card_info_by_index()", context, {})
            }
            break;

        // Вывод информации по устройству воспроизведения
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_verbose_m << "Sound sink detached; index: " << index;
                ad->removeDevice(index, data::AudioDevType::Sink, false);
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                O_PTR_MSG(pa_context_get_sink_info_by_index(context, index, sink_change, ad),
                          "Failed call pa_context_get_sink_info_by_index()", context, {})
            }
            break;

        /** Вывод информации по потоку воспроизведения **/
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT CHANGE; index: " << index;
                O_PTR_MSG(pa_context_get_sink_input_info(context, index, sink_stream_info, ad),
                          "Failed call pa_context_get_sink_info_by_index()", context, {})
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT REMOVE; index: " << index;
            }
            break;

        // Вывод информации по устройству записи
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_verbose_m << "Sound source detached; index: " << index;
                ad->removeDevice(index, data::AudioDevType::Source, false);
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                O_PTR_MSG(pa_context_get_source_info_by_index(context, index, source_change, ad),
                          "Failed call pa_context_get_source_info_by_index()", context, {})
            }
            break;

        // Вывод информации по потоку записи
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT CHANGE; index: " << index;
                O_PTR_MSG(pa_context_get_source_output_info(context, index, source_stream_info, ad),
                          "Failed call pa_context_get_source_output_info()", context, {})
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT REMOVE; index: " << index;
            }
            break;
    }
}

void AudioDev::card_info(pa_context* context, const pa_card_info* card_info,
                         int eol, void* userdata)
{
    log_debug2_m << "card_info(); eol: " << eol;

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_verbose_m << "Sound card detected"
                  << "; index: " << card_info->index
                  << "; name: " << card_info->name;

    /** Расширенная информация по звуковой карте **
    pa_card_profile_info* profile = card_info->profiles;
    for (size_t i = 0; i < card_info->n_profiles; ++i)
    {
        log_debug2_m << "Sound card " << card_info->index
                     << "; Profile: " << profile->name
                     << "; description: " << profile->description;
        ++profile;
    }
    if (pa_card_profile_info* activeProfile = card_info->active_profile)
    {
        log_debug2_m << "Sound card " << card_info->index
                     << "; Active profile: " << activeProfile->name
                     << "; description: " << activeProfile->description;
    }
    else
    {
        log_debug2_m << "Sound card " << card_info->index
                     << "; Active profile is undefined";
    }
    */

    O_PTR_MSG(pa_context_get_sink_info_list(context, sink_info, ad),
              "Failed call pa_context_get_sink_info_list()", context, {})

    O_PTR_MSG(pa_context_get_source_info_list(context, source_info, ad),
              "Failed call pa_context_get_source_info_list()", context, {})
}

void AudioDev::sink_info(pa_context* context, const pa_sink_info* info,
                         int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    /* Даем возможность выбирать виртуальные звуковые устройства. Это позволит
       на уровне PulseAudio выбирать устройства с функцией эхоподавления
    */
    //if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
    //    return;

    lst::FindResult fr = ad->_sinkDevices.find(info->name);
    if (fr.failed())
        log_verbose_m << "Sound sink detected"
                      << "; index: " << info->index
                      << "; (card: " << info->card << ")"
                      << "; volume: " << info->volume.values[0]
                      << "; name: " << info->name;

    ad->updateAudioDevInfo(info, ad->_sinkDevices);
}

void AudioDev::sink_change(pa_context* context, const pa_sink_info* info,
                           int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    /* См. комментарий в функции AudioDev::sink_info() */
    //if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
    //    return;

    lst::FindResult fr = ad->_sinkDevices.findRef(quint32(info->index));
    if (fr.failed())
        return;

    log_debug_m << "Sound sink changed"
                << "; index: " << info->index
                << "; (card: " << info->card << ")"
                << "; volume: " << info->volume.values[0]
                << "; name: " << info->name;

    ad->updateAudioDevInfo(info, ad->_sinkDevices);
}

void AudioDev::source_info(pa_context* context, const pa_source_info* info,
                           int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    /* См. комментарий в функции AudioDev::sink_info() */
    //if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
    //    return;

    lst::FindResult fr = ad->_sourceDevices.find(info->name);
    if (fr.failed())
        log_verbose_m << "Sound source detected"
                      << "; index: " << info->index
                      << "; (card: " << info->card << ")"
                      << "; volume: " << info->volume.values[0]
                      << "; name: " << info->name;

    ad->updateAudioDevInfo(info, ad->_sourceDevices);
}

void AudioDev::source_change(pa_context* context, const pa_source_info* info,
                             int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    /* См. комментарий в функции AudioDev::sink_info() */
    //if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
    //    return;

    lst::FindResult fr = ad->_sourceDevices.findRef(quint32(info->index));
    if (fr.failed())
        return;

    log_debug_m << "Sound source changed"
                << "; index: " << info->index
                << "; (card: " << info->card << ")"
                << "; volume: " << info->volume.values[0]
                << "; name: " << info->name;

    ad->updateAudioDevInfo(info, ad->_sourceDevices);
}

void AudioDev::playback_stream_create(pa_context* context, const pa_sink_input_info* info,
                                      int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_debug_m << "Playback stream info"
                << "; index: " << info->index
                << "; name: " << info->name
                << "; (sink: " << info->sink << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];

    ad->_palybackAudioStreamInfo.type = data::AudioStreamInfo::Type::Playback;
    ad->_palybackAudioStreamInfo.state = data::AudioStreamInfo::State::Created;
    ad->_palybackAudioStreamInfo.index = info->index;
    ad->fillAudioStreamInfo(info, ad->_palybackAudioStreamInfo);

    // Восстанавливаем уровень громкости
    /* Для потока воспроизведения звуков нет возможности использовать
       персональный уровень громкости, да это и не нужно делать.
       ad->readAudioStreamVolume(ad->_palybackAudioStreamInfo, "playback_volume");
    */
    config::state().getValue("audio.streams.playback_volume", ad->_palybackAudioStreamInfo.volume);

    Message::Ptr m = createMessage(ad->_palybackAudioStreamInfo);
    toxConfig().send(m);

    // Обновляем уровень громкости
    QMetaObject::invokeMethod(ad, "message", Qt::QueuedConnection,
                              Q_ARG(pproto::Message::Ptr, m));
}

void AudioDev::voice_stream_create(pa_context* context, const pa_sink_input_info* info,
                                   int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_debug_m << "Voice stream info"
                << "; index: " << info->index
                << "; name: " << info->name
                << "; (sink: " << info->sink << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];

    ad->_voiceAudioStreamInfo.type = data::AudioStreamInfo::Type::Voice;
    ad->_voiceAudioStreamInfo.state = data::AudioStreamInfo::State::Created;
    ad->_voiceAudioStreamInfo.index = info->index;
    ad->fillAudioStreamInfo(info, ad->_voiceAudioStreamInfo);

    // Восстанавливаем уровень громкости
    ad->readAudioStreamVolume(ad->_voiceAudioStreamInfo, "voice_volume");

    Message::Ptr m = createMessage(ad->_voiceAudioStreamInfo);
    toxConfig().send(m);

    // Обновляем уровень громкости
    QMetaObject::invokeMethod(ad, "message", Qt::QueuedConnection,
                              Q_ARG(pproto::Message::Ptr, m));
}

void AudioDev::record_stream_create(pa_context* context, const pa_source_output_info* info,
                                    int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_debug_m << "Record stream info"
                << "; index: " << info->index
                << "; name: " << info->name
                << "; (source: " << info->source << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];

    ad->_recordAudioStreamInfo.type = data::AudioStreamInfo::Type::Record;
    ad->_recordAudioStreamInfo.state = data::AudioStreamInfo::State::Created;
    ad->_recordAudioStreamInfo.index = info->index;
    ad->fillAudioStreamInfo(info, ad->_recordAudioStreamInfo);

    // Восстанавливаем уровень громкости
    ad->readAudioStreamVolume(ad->_recordAudioStreamInfo, "record_volume");

    Message::Ptr m = createMessage(ad->_recordAudioStreamInfo);
    toxConfig().send(m);

    // Обновляем уровень громкости
    QMetaObject::invokeMethod(ad, "message", Qt::QueuedConnection,
                              Q_ARG(pproto::Message::Ptr, m));
}

void AudioDev::sink_stream_info(pa_context* context, const pa_sink_input_info* info,
                                int eol, void* userdata)
{
    log_debug2_m << "sink_stream_info(); eol: " << eol;

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_debug_m << "Sink stream changed"
                << "; index: " << info->index
                << "; name: " << info->name
                << "; (sink: " << info->sink << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];

    data::AudioStreamInfo* audioStreamInfo = 0;
    if (ad->_palybackAudioStreamInfo.state != data::AudioStreamInfo::State::Terminated
        && info->index == ad->_palybackAudioStreamInfo.index)
    {
        audioStreamInfo = &ad->_palybackAudioStreamInfo;
    }
    else if (ad->_voiceAudioStreamInfo.state != data::AudioStreamInfo::State::Terminated
             && info->index == ad->_voiceAudioStreamInfo.index)
    {
        audioStreamInfo = &ad->_voiceAudioStreamInfo;
    }
    if (audioStreamInfo)
    {
        audioStreamInfo->state = data::AudioStreamInfo::State::Changed;
        ad->fillAudioStreamInfo(info, *audioStreamInfo);

        if (toxConfig().isActive())
        {
            Message::Ptr m = createMessage(*audioStreamInfo);
            toxConfig().send(m);
        }
    }
}

void AudioDev::source_stream_info(pa_context* context, const pa_source_output_info* info,
                                    int eol, void* userdata)
{
    log_debug2_m << "source_stream_info(); eol: " << eol;

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    if (eol != 0)
        return;

    log_debug_m << "Source stream changed"
                << "; index: " << info->index
                << "; name: " << info->name
                << "; (source: " << info->source << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];

    data::AudioStreamInfo* audioStreamInfo = 0;
    if (ad->_recordAudioStreamInfo.state != data::AudioStreamInfo::State::Terminated
        && info->index == ad->_recordAudioStreamInfo.index)
    {
        audioStreamInfo = &ad->_recordAudioStreamInfo;
    }
    if (audioStreamInfo)
    {
        audioStreamInfo->state = data::AudioStreamInfo::State::Changed;
        ad->fillAudioStreamInfo(info, *audioStreamInfo);

        if (toxConfig().isActive())
        {
            Message::Ptr m = createMessage(*audioStreamInfo);
            toxConfig().send(m);
        }
    }
}

void AudioDev::playback_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_state()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    pa_context* context;
    uint32_t index;

    switch (pa_stream_get_state(stream))
    {
        case PA_STREAM_CREATING:
            log_debug2_m << "Playback stream event: PA_STREAM_CREATING";
            break;

        case PA_STREAM_TERMINATED:
            log_debug2_m << "Playback stream event: PA_STREAM_TERMINATED";

            ad->_palybackAudioStreamInfo.state = data::AudioStreamInfo::State::Terminated;
            ad->_palybackAudioStreamInfo.index = -1;

            if (toxConfig().isActive())
            {
                Message::Ptr m = createMessage(ad->_palybackAudioStreamInfo);
                toxConfig().send(m);
            }
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Playback stream event: PA_STREAM_READY";
            log_debug_m  << "Playback stream started";

            context = pa_stream_get_context(stream);
            index = pa_stream_get_index(stream);
            O_PTR_MSG(pa_context_get_sink_input_info(context, index, playback_stream_create, ad),
                      "Failed call pa_context_get_sink_info_by_index()", context, {})
            break;

        case PA_STREAM_FAILED:
            log_error_m << "Playback stream event: PA_STREAM_FAILED"
                        << paStrError(stream);
        default:
            break;
    }
}

void AudioDev::playback_stream_started(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_started()";
}

void AudioDev::playback_stream_write(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "playback_stream_write()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);

    void* data;
    if (pa_stream_begin_write(stream, &data, &nbytes) < 0)
    {
        log_error_m << "Failed call pa_stream_begin_write()" << paStrError(stream);
        return;
    }

    qint64 len = ad->_playbackFile.read((char*)data, nbytes);
    if (len == qint64(nbytes))
    {
        if (pa_stream_write(stream, data, len, 0, 0LL, PA_SEEK_RELATIVE) < 0)
            log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
    }
    else
    {
        log_debug2_m << "Playback cycle";
        bool success = false;
        if (--ad->_playbackCycleCount > 0)
        {
            if (ad->_playbackFile.seek(ad->_playbackFile.dataChunkPos()))
            {
                len = ad->_playbackFile.read((char*)data, nbytes);
                if (len > 0)
                {
                    if (pa_stream_write(stream, data, len, 0, 0LL, PA_SEEK_RELATIVE) < 0)
                        log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
                    else
                        success = true;
                }
            }
        }
        if (!success)
        {
            log_debug_m << "Playback data empty";
            pa_stream_cancel_write(stream);
            pa_stream_set_write_callback(stream, 0, 0);
            O_PTR_MSG(pa_stream_drain(stream, playback_stream_drain, ad),
                      "Failed call pa_stream_drain()", stream, {})
        }
    }
}

void AudioDev::playback_stream_overflow(pa_stream*, void* userdata)
{
    log_debug2_m << "playback_stream_overflow()";
}

void AudioDev::playback_stream_underflow(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_underflow()";
}

void AudioDev::playback_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "playback_stream_suspended()";
}

void AudioDev::playback_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "playback_stream_moved()";
}

void AudioDev::playback_stream_drain(pa_stream* stream, int success, void *userdata)
{
    log_debug2_m << "playback_stream_drain()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    ad->_playbackFile.close();

    if (ad->_playbackTest)
    {
        if (toxConfig().isActive())
        {
            data::AudioTest audioTest;
            audioTest.begin = false;
            audioTest.playback = true;

            Message::Ptr m = createMessage(audioTest);
            toxConfig().send(m);
        }
        ad->_playbackTest = false;
    }
    if (pa_stream_disconnect(stream) < 0)
        log_error_m << "Failed call pa_stream_disconnect()" << paStrError(stream);

    if (ad->_emitPlaybackFinish)
    {
        Message::Ptr m = createMessage(ad->_playbackFinish);
        emit ad->internalMessage(m);
    }
    ad->_playbackFinish.code = data::PlaybackFinish::Code::Undefined;
    ad->_emitPlaybackFinish = true;

    pa_stream_unref(stream);
    ad->_playbackStream = 0;
    ad->_playbackActive = false;

    log_debug_m << "Playback stream stopped (file: " << ad->_playbackFile.fileName() << ")";
}

void AudioDev::voice_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "voice_stream_state()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);

    pa_context* context;
    uint32_t index;
    const pa_buffer_attr* attr = 0;
    char cmt[PA_CHANNEL_MAP_SNPRINT_MAX];
    char sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

    switch (pa_stream_get_state(stream))
    {
        case PA_STREAM_CREATING:
            log_debug2_m << "Voice stream event: PA_STREAM_CREATING";
            break;

        case PA_STREAM_TERMINATED:
            log_debug2_m << "Voice stream event: PA_STREAM_TERMINATED";

            ad->_voiceAudioStreamInfo.state = data::AudioStreamInfo::State::Terminated;
            ad->_voiceAudioStreamInfo.index = -1;

            if (toxConfig().isActive())
            {
                Message::Ptr m = createMessage(ad->_voiceAudioStreamInfo);
                toxConfig().send(m);
            }
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Voice stream event: PA_STREAM_READY";
            log_debug_m  << "Voice stream started";

            context = pa_stream_get_context(stream);
            index = pa_stream_get_index (stream);
            O_PTR_MSG(pa_context_get_sink_input_info(context, index, voice_stream_create, ad),
                      "Failed call pa_context_get_sink_info_by_index()", context, {})

            if (VoiceFrameInfo::Ptr voiceFrameInfo = getVoiceFrameInfo())
            {
                voiceRBuff().init(8 * voiceFrameInfo->bufferSize);
                log_debug_m  << "Initialization a voice ring buffer"
                             << "; size: " << voiceRBuff().size();
            }
            else
                log_error_m << "Failed get VoiceFrameInfo for voice";

            if (alog::logger().level() >= alog::Level::Debug)
            {
                if (!!(attr = pa_stream_get_buffer_attr(stream)))
                {
                    log_debug_m << "PA voice buffer metrics"
                                << "; maxlength: " << attr->maxlength
                                << ", fragsize: " << attr->fragsize;

                    log_debug_m << "PA voice using sample spec "
                                << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(stream))
                                << ", channel map "
                                << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(stream));

                    log_debug_m << "PA voice connected to device "
                                << pa_stream_get_device_name(stream)
                                << " (" << pa_stream_get_device_index(stream)
                                << ", "
                                << (pa_stream_is_suspended(stream) ? "" : "not ")
                                << " suspended)";
                }
                else
                    log_error_m << "Failed call pa_stream_get_buffer_attr()"
                                << paStrError(stream);
            }
            break;

        case PA_STREAM_FAILED:
            log_error_m << "Voice stream event: PA_STREAM_FAILED"
                        << paStrError(stream);
        default:
            break;
    }
}

void AudioDev::voice_stream_started(pa_stream* stream, void* userdata)
{
    log_debug2_m << "voice_stream_started()";
}

void AudioDev::voice_stream_write(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "voice_stream_write() nbytes: " << nbytes;

    AudioDev* ad = static_cast<AudioDev*>(userdata); (void) ad;

    void* data;
    if (pa_stream_begin_write(stream, &data, &nbytes) < 0)
    {
        log_error_m << "Failed call pa_stream_begin_write()" << paStrError(stream);
        return;
    }

    if (voiceRBuff().read((char*)data, nbytes))
        ad->_voiceBytes += nbytes;
    else
        memset(data, 0, nbytes);

    if (pa_stream_write(stream, data, nbytes, 0, 0LL, PA_SEEK_RELATIVE) < 0)
        log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
}

void AudioDev::voice_stream_overflow(pa_stream*, void* userdata)
{
    log_debug2_m << "voice_stream_overflow()";
}

void AudioDev::voice_stream_underflow(pa_stream* stream, void* userdata)
{
    log_debug2_m << "voice_stream_underflow()";
}

void AudioDev::voice_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "voice_stream_suspended()";
}

void AudioDev::voice_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "voice_stream_moved()";
}

void AudioDev::record_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "record_stream_state()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);

    pa_context* context;
    uint32_t index;
    const pa_buffer_attr* attr = 0;
    char cmt[PA_CHANNEL_MAP_SNPRINT_MAX];
    char sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

    switch (pa_stream_get_state(stream))
    {
        case PA_STREAM_CREATING:
            log_debug2_m << "Record stream event: PA_STREAM_CREATING";
            break;

        case PA_STREAM_TERMINATED:
            log_debug2_m << "Record stream event: PA_STREAM_TERMINATED";

            ad->_recordAudioStreamInfo.state = data::AudioStreamInfo::State::Terminated;
            ad->_recordAudioStreamInfo.index = -1;

            if (toxConfig().isActive())
            {
                Message::Ptr m = createMessage(ad->_recordAudioStreamInfo);
                toxConfig().send(m);
            }
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Record stream event: PA_STREAM_READY";
            log_debug_m  << "Record stream started";

            context = pa_stream_get_context(stream);
            index = pa_stream_get_index (stream);
            O_PTR_MSG(pa_context_get_source_output_info(context, index, record_stream_create, ad),
                      "Failed call pa_context_get_source_output_info()", context, {})

            if (VoiceFrameInfo::Ptr voiceFrameInfo = getRecordFrameInfo())
            {
                recordRBuff_1().init(5 * voiceFrameInfo->bufferSize);
                log_debug_m  << "Initialization a record ring buffer (1)"
                             << "; size: " << recordRBuff_1().size();

                recordRBuff_2().init(5 * voiceFrameInfo->bufferSize);
                log_debug_m  << "Initialization a record ring buffer (2)"
                             << "; size: " << recordRBuff_2().size();
            }
            else
                log_error_m << "Failed get VoiceFrameInfo for record";

            if (alog::logger().level() >= alog::Level::Debug)
            {
                if (!!(attr = pa_stream_get_buffer_attr(stream)))
                {
                    log_debug_m << "PA record buffer metrics"
                                << "; maxlength: " << attr->maxlength
                                << ", fragsize: " << attr->fragsize;

                    log_debug_m << "PA record using sample spec "
                                << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(stream))
                                << ", channel map "
                                << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(stream));

                    log_debug_m << "PA record connected to device "
                                << pa_stream_get_device_name(stream)
                                << " (" << pa_stream_get_device_index(stream)
                                << ", "
                                << (pa_stream_is_suspended(stream) ? "" : "not ")
                                << " suspended)";
                }
                else
                    log_error_m << "Failed call pa_stream_get_buffer_attr()"
                                << paStrError(stream);
            }
            break;

        case PA_STREAM_FAILED:
            log_error_m << "Record stream event: PA_STREAM_FAILED"
                        << paStrError(stream);
        default:
            break;
    }
}

void AudioDev::record_stream_started(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_started()";

}

void AudioDev::record_stream_read(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "record_stream_read()";

    AudioDev* ad = static_cast<AudioDev*>(userdata); (void) ad;

    const void* data;
    while (pa_stream_readable_size(stream) > 0)
    {
        if (pa_stream_peek(stream, &data, &nbytes) < 0)
        {
            log_error_m << "Failed call pa_stream_peek()" << paStrError(stream);
            continue;
        }

        if (data && nbytes)
        {
            if (data)
            {
                ad->_recordBytes += nbytes;

                if (recordRBuff_1().write((char*)data, nbytes))
                    voiceFilters().wake();
                else
                    log_error_m << "Failed write data to recordRBuff_1"
                                << ". Data size: " << nbytes;
            }
        }
        if (nbytes)
            pa_stream_drop(stream);
     }
}

void AudioDev::record_stream_overflow(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_overflow()";
}

void AudioDev::record_stream_underflow(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_underflow()";
}

void AudioDev::record_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_suspended()";
}

void AudioDev::record_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_moved()";
}
