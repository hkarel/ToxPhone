#include "audio_dev.h"
#include "toxphone_appl.h"

#include "shared/break_point.h"
#include "shared/simple_ptr.h"
#include "shared/spin_locker.h"

//#include <chrono>
#include <string>
#include <string.h>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "AudioDev")

static string paStrError(pa_context* context)
{
    return string("; Error: ") +  pa_strerror(pa_context_errno(context));
}

static string paStrError(pa_stream* stream)
{
    return string("; Error: ") + pa_strerror(pa_context_errno(pa_stream_get_context(stream)));
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
        ToxPhoneApplication::stop(1); \
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
    chk_connect_q(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &AudioDev::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    //FUNC_REGISTRATION(AudioDev)
    FUNC_REGISTRATION(AudioDevChange)
    FUNC_REGISTRATION(AudioTest)
    FUNC_REGISTRATION(ToxCallState)
    _funcInvoker.sort();

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
    if (_ringtoneStream)
    {
        pa_stream_disconnect(_ringtoneStream);
        pa_stream_unref(_ringtoneStream);
        _ringtoneStream = 0;
    }
    if (_playbackStream)
    {
        pa_stream_disconnect(_playbackStream);
        pa_stream_unref(_playbackStream);
        _playbackStream = 0;
    }
    if (_recordStream)
    {
        pa_stream_disconnect(_recordStream);
        pa_stream_unref(_recordStream);
        _recordStream = 0;
    }

    if (_paContext)
    {
        pa_context_disconnect(_paContext);
        pa_context_unref(_paContext);
        _paContext = 0;
    }
    if (_paMainLoop)
    {
        pa_threaded_mainloop_free(_paMainLoop);
        _paMainLoop = 0;
    }
    _paApi = 0;
}

void AudioDev::startRingtone()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_ringtoneActive)
        return;

    QString filePath = getFilePath("sound/ringtone.wav");
    if (filePath.isEmpty())
    {
        log_error_m << "File " << filePath << " not found";
        return;
    }
    if (_ringtoneFile.isOpen())
        _ringtoneFile.close();

    _ringtoneFile.setFileName(filePath);
    if (!_ringtoneFile.open())
    {
        log_error_m << "Failed open the ringtone file " << filePath;
        return;
    }
    if (_ringtoneFile.header().bitsPerSample != 16)
    {
        log_error_m << "Only 16 bits per sample is supported"
                    << ". FIle: " << _ringtoneFile.fileName();
        _ringtoneFile.close();
        return;
    }

    if (_ringtoneStream)
    {
        log_debug_m << "Ringtone stream drop";
        pa_stream_set_write_callback(_ringtoneStream, 0, 0);
        if (pa_stream_disconnect(_ringtoneStream) < 0)
        {
            log_error_m << "Failed call pa_stream_disconnect()"
                        << paStrError(_ringtoneStream);
        }
        pa_stream_unref(_ringtoneStream);
        log_debug_m << "Ringtone stream dropped";
    }

    log_debug_m << "Create ringtone stream";

    pa_sample_spec paSampleSpec;
    paSampleSpec.format = PA_SAMPLE_S16LE;
    paSampleSpec.channels = _ringtoneFile.header().numChannels;
    paSampleSpec.rate = _ringtoneFile.header().sampleRate;

    _ringtoneStream = pa_stream_new(_paContext, "Playback", &paSampleSpec, 0);
    if (!_ringtoneStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    pa_stream_set_state_callback    (_ringtoneStream, ringtone_stream_state, this);
    pa_stream_set_started_callback  (_ringtoneStream, ringtone_stream_started, this);
    pa_stream_set_write_callback    (_ringtoneStream, ringtone_stream_write, this);
    pa_stream_set_overflow_callback (_ringtoneStream, ringtone_stream_overflow, this);
    pa_stream_set_underflow_callback(_ringtoneStream, ringtone_stream_underflow, this);
    pa_stream_set_suspended_callback(_ringtoneStream, ringtone_stream_suspended, this);
    pa_stream_set_moved_callback    (_ringtoneStream, ringtone_stream_moved, this);

    QByteArray devNameByff;
    const char* devName = currentDeviceName(_sinkDevices, devNameByff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
    if (pa_stream_connect_playback(_ringtoneStream, devName, 0, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_ringtoneStream);
        return;
    }

    _ringtoneActive = true;
    log_debug_m << "Ringtone stream start";
}

void AudioDev::stopRingtone()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_ringtoneActive)
        return;

    log_debug_m << "Ringtone stream stop";

    pa_stream_set_write_callback(_ringtoneStream, 0, 0);
    O_PTR_MSG(pa_stream_drain(_ringtoneStream, ringtone_stream_drain, this),
              "Failed call pa_stream_drain()", _ringtoneStream, {})

    while (_ringtoneActive)
        this_thread::sleep_for(chrono::milliseconds(5));

    if (_ringtoneStream)
    {
        if (pa_stream_disconnect(_ringtoneStream) < 0)
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_ringtoneStream);
        pa_stream_unref(_ringtoneStream);
    }
    _ringtoneStream = 0;
    _ringtoneActive = false;
}

void AudioDev::startPlayback(const QString& fileName)
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_playbackActive)
        return;

    QString filePath = getFilePath(fileName);
    if (filePath.isEmpty())
    {
        log_error_m << "File " << filePath << " not found";
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

    QByteArray devNameByff;
    const char* devName = currentDeviceName(_sinkDevices, devNameByff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
    if (pa_stream_connect_playback(_playbackStream, devName, 0, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_playbackStream);
        return;
    }

    _playbackActive = true;
    log_debug_m << "Playback stream start (file: " << _playbackFile.fileName() << ")";
}

void AudioDev::stopPlayback()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_playbackActive)
        return;

    log_debug_m << "Playback stream stop";

    pa_stream_set_write_callback(_playbackStream, 0, 0);
    O_PTR_MSG(pa_stream_drain(_playbackStream, playback_stream_drain, this),
              "Failed call pa_stream_drain()", _ringtoneStream, {})

    while (_playbackActive)
        this_thread::sleep_for(chrono::milliseconds(5));

    if (_playbackStream)
    {
        if (pa_stream_disconnect(_playbackStream) < 0)
            log_error_m << "Failed call pa_stream_disconnect()"
                        << paStrError(_playbackStream);
        pa_stream_unref(_playbackStream);
    }
    _playbackStream = 0;
    _playbackActive = false;
}

void AudioDev::startPlaybackVoice(const VoiceFrameInfo::Ptr& voiceFrameInfo)
{
    stopRingtone();
    stopPlayback();

    QMutexLocker locker(&_streamLock); (void) locker;

    if (_playbackActive)
        return;

    if (_playbackStream)
    {
        log_debug_m << "Playback stream drop";
        pa_stream_set_write_callback(_playbackStream, 0, 0);
        if (pa_stream_disconnect(_playbackStream) < 0)
        {
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_playbackStream);
        }
        pa_stream_unref(_playbackStream);
        log_debug_m << "Playback stream dropped";
    }

    log_debug_m << "Create voice stream";
    _playbackBytes = 0;

    pa_sample_spec paSampleSpec;
    paSampleSpec.format = PA_SAMPLE_S16LE;
    paSampleSpec.channels = voiceFrameInfo->channels;
    paSampleSpec.rate = voiceFrameInfo->samplingRate;

    _playbackStream = pa_stream_new(_paContext, "Playback", &paSampleSpec, 0);
    if (!_playbackStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    playbackVoiceFrameInfo(voiceFrameInfo.get());
    log_debug_m << "Initialization playback VoiceFrameInfo"
                << "; latency: "       << voiceFrameInfo->latency
                << "; channels: "  << int(voiceFrameInfo->channels)
                << "; sample size: "   << voiceFrameInfo->sampleSize
                << "; sample count: "  << voiceFrameInfo->sampleCount
                << "; sampling rate: " << voiceFrameInfo->samplingRate
                << "; buffer size: "   << voiceFrameInfo->bufferSize;

    pa_stream_set_state_callback    (_playbackStream, playback_stream_state, this);
    pa_stream_set_started_callback  (_playbackStream, playback_stream_started, this);
    pa_stream_set_write_callback    (_playbackStream, playback_stream_write, this);
    pa_stream_set_overflow_callback (_playbackStream, playback_stream_overflow, this);
    pa_stream_set_underflow_callback(_playbackStream, playback_stream_underflow, this);
    pa_stream_set_suspended_callback(_playbackStream, playback_stream_suspended, this);
    pa_stream_set_moved_callback    (_playbackStream, playback_stream_moved, this);

    //size_t latency = 20000;
    size_t latency = voiceFrameInfo->latency;
    pa_buffer_attr paBuffAttr;
    paBuffAttr.maxlength = 3 * pa_usec_to_bytes(latency, &paSampleSpec);
    paBuffAttr.tlength   = pa_usec_to_bytes(latency, &paSampleSpec);
    paBuffAttr.prebuf    = uint32_t(-1);
    paBuffAttr.minreq    = uint32_t(-1);
    paBuffAttr.fragsize  = uint32_t(-1);

    QByteArray devNameByff;
    const char* devName = currentDeviceName(_sinkDevices, devNameByff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    // Параметр _playbackVoice должен быть установлен в TRUE до вызова функции
    // pa_stream_connect_playback(). Таким образом мы гарантированно корректно
    // обработаем событие PA_STREAM_READY в функции playback_stream_state().
    _playbackVoice = true;

    if (pa_stream_connect_playback(_playbackStream, devName, &paBuffAttr, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_playbackStream);
        _playbackVoice = false;
        return;
    }

    _playbackActive = true;
    log_debug_m << "Voice playback stream start";
}

void AudioDev::stopPlaybackVoice()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_playbackActive)
        return;

    log_debug_m << "Voice playback stream stop";
    log_debug_m << "Voice playback ring buffer available: "
                << playbackVoiceRBuff().available();

    playbackVoiceRBuff().reset();
    playbackVoiceFrameInfo(0, true);

    pa_stream_set_write_callback(_playbackStream, 0, 0);
    if (pa_stream_disconnect(_playbackStream) < 0)
    {
        log_error_m << "Failed call pa_stream_disconnect()"
                    << paStrError(_playbackStream);
    }
    pa_stream_unref(_playbackStream);
    _playbackStream = 0;
    _playbackActive = false;
    _playbackVoice = false;

    log_debug_m << "Voice playback bytes: " << _playbackBytes;
    log_debug_m << "Voice playback stream stopped";
}

void AudioDev::startRecord()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_recordActive)
        return;

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
    paSampleSpec.channels = 2;
    //paSampleSpec.channels = 1;
    paSampleSpec.rate = 48000;

    _recordStream = pa_stream_new(_paContext, "Recording", &paSampleSpec, 0);
    if (!_recordStream)
    {
        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
        return;
    }

    quint32 latency = 20000;
    //latency = 10000;

    quint32 bufferSize = pa_usec_to_bytes(latency, &paSampleSpec);
    quint32 sampleSize = pa_sample_size(&paSampleSpec);
    quint32 sampleCount = bufferSize / (sampleSize * paSampleSpec.channels);
    VoiceFrameInfo voiceFrameInfo {latency, paSampleSpec.channels, sampleSize,
                                   sampleCount, paSampleSpec.rate, bufferSize};

    recordVoiceFrameInfo(&voiceFrameInfo);
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

    QByteArray devNameByff;
    const char* devName = currentDeviceName(_sourceDevices, devNameByff);
    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    if (pa_stream_connect_record(_recordStream, devName, &paBuffAttr, flags) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_record()"
                    << paStrError(_recordStream);
        return;
    }

    _voiceFilters.start();
    _recordActive = true;
    log_debug_m << "Record stream start";
}

void AudioDev::stopRecord()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_recordActive)
        return;

    log_debug_m << "Record stream stop";
    log_debug_m << "Record ring buffer available: "
                << recordVoiceRBuff().available();

    _voiceFilters.stop();
    recordVoiceRBuff().reset();
    recordVoiceFrameInfo(0, true);

    if (_recordTest)
    {
        if (configConnected())
        {
            data::AudioTest audioTest;
            audioTest.begin = false;
            audioTest.record = true;

            Message::Ptr m = createMessage(audioTest, Message::Type::Event);
            tcp::listener().send(m);
        }
        _recordTest = false;
    }

    pa_stream_set_read_callback(_recordStream, 0, 0);
    if (pa_stream_disconnect(_recordStream) < 0)
    {
        log_error_m << "Failed call pa_stream_disconnect()"
                    << paStrError(_recordStream);
    }
    pa_stream_unref(_recordStream);
    _recordStream = 0;
    _recordActive = false;

    log_debug_m << "Record bytes: " << _recordBytes;
    log_debug_m << "Record stream stopped";
}

void AudioDev::stopAudioTests()
{
    if (_playbackTest)
        stopPlayback();

    if (_recordTest)
        stopRecord();
}

void AudioDev::start()
{
    pa_threaded_mainloop_start(_paMainLoop);
}

bool AudioDev::stop(unsigned long /*time*/)
{
    stopRingtone();
    stopPlayback();
    stopPlaybackVoice();
    stopRecord();

    if (_paMainLoop)
        pa_threaded_mainloop_stop(_paMainLoop);

    deinit();
    return true;
}

void AudioDev::message(const communication::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (_funcInvoker.containsCommand(message->command()))
    {
        if (message->command() != command::IncomingConfigConnection)
            message->markAsProcessed();

        _funcInvoker.call(message);
    }
}

void AudioDev::command_IncomingConfigConnection(const Message::Ptr& message)
{
    QMutexLocker locker(&_devicesLock); (void) locker;

    for (int i = 0; i < _sinkDevices.count(); ++i)
    {
        Message::Ptr m = createMessage(_sinkDevices[i], Message::Type::Event);
        tcp::listener().send(m);
    }
    for (int i = 0; i < _sourceDevices.count(); ++i)
    {
        Message::Ptr m = createMessage(_sourceDevices[i], Message::Type::Event);
        tcp::listener().send(m);
    }
}

void AudioDev::command_AudioDevChange(const Message::Ptr& message)
{
    QMutexLocker locker(&_devicesLock); (void) locker;

    data::AudioDevChange audioDevChange;
    readFromMessage(message, audioDevChange);

    // ChangeFlag::Volume
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Volume)
    {
        data::AudioDevInfo::List* devices = getDevices(audioDevChange.type);
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
            O_PTR_MSG(pa_context_set_sink_volume_by_index(_paContext, audioDevInfo->index, &volume, 0, 0),
                      "Failed call pa_context_set_sink_volume_by_index()", _paContext, {})
        }
        else
        {
            O_PTR_MSG(pa_context_set_source_volume_by_index(_paContext, audioDevInfo->index, &volume, 0, 0),
                      "Failed call pa_context_set_source_volume_by_index()", _paContext, {})
        }
    }

    // ChangeFlag::Current
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Current)
    {
        data::AudioDevInfo::List* devices = getDevices(audioDevChange.type);
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


        if (configConnected())
        {
            data::AudioDevChange audioDevChange {*audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

            Message::Ptr m = createMessage(audioDevChange, Message::Type::Event);
            tcp::listener().send(m);
        }
    }

    // ChangeFlag::Default
    if (audioDevChange.changeFlag == data::AudioDevChange::ChangeFlag::Default)
    {
        data::AudioDevInfo::List* devices = getDevices(audioDevChange.type);
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
            ? "audio.device.playback_default"
            : "audio.device.record_default";
        config::state().setValue(confName, audioDevInfo->name.constData());
        config::state().save();

        alog::Line logLine = log_verbose_m
            << ((audioDevInfo->type == data::AudioDevType::Sink)
                ? "Sound sink "
                : "Sound source ");
        logLine << "set as default"
                << "; index: "  << audioDevInfo->index
                << "; (card: "  << audioDevInfo->cardIndex << ")"
                << "; volume: " << audioDevInfo->volume
                << "; name: "   << audioDevInfo->name;

        if (configConnected())
        {
            data::AudioDevChange audioDevChange {*audioDevInfo};
            audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Default;

            Message::Ptr m = createMessage(audioDevChange, Message::Type::Event);
            tcp::listener().send(m);
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
            startPlayback("sound/test.wav");
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
        }
        else
            stopRecord();
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
        && _callState.state == data::ToxCallState::State::WaitingAnswer)
    {
        startRingtone();
    }
    if (_callState.direction == data::ToxCallState::Direction::Incoming
        && _callState.state == data::ToxCallState::State::InProgress)
    {
        stopRingtone();
        startRecord();
    }
    if (_callState.direction == data::ToxCallState::Direction::Outgoing
        && _callState.state == data::ToxCallState::State::InProgress)
    {
        //stopRingtone();
        startRecord();
    }
    if (_callState.direction == data::ToxCallState::Direction:: Undefined
        && _callState.state == data::ToxCallState::State::Undefined)
    {
        stopRingtone();
        stopPlaybackVoice();
        stopRecord();
    }

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
    QMutexLocker locker(&_devicesLock); (void) locker;
    if (data::AudioDevInfo* audioDevInfo = currentDevice(devices))
        buff = audioDevInfo->name;

    return (!buff.isEmpty()) ? buff.constData() : 0;
}

bool AudioDev::removeDevice(quint32 index, data::AudioDevType type, bool byCardIndex)
{
    QMutexLocker locker(&_devicesLock); (void) locker;

    bool isRemoved = false;
    bool isCurrent = false;
    data::AudioDevInfo::List* devices = getDevices(type);
    for (int i = 0; i < devices->count(); ++i)
    {
        quint32 devIndex = (byCardIndex) ? devices->at(i).cardIndex
                                         : devices->at(i).index;
        if (devIndex == index)
        {
            if (configConnected())
            {
                data::AudioDevChange audioDevChange {devices->at(i)};
                audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Remove;

                Message::Ptr m = createMessage(audioDevChange, Message::Type::Event);
                tcp::listener().send(m);
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

                if (configConnected())
                {
                    Message::Ptr m = createMessage(devices->at(i), Message::Type::Event);
                    tcp::listener().send(m);

                    data::AudioDevChange audioDevChange {devices->at(i)};
                    audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

                    m = createMessage(audioDevChange, Message::Type::Event);
                    tcp::listener().send(m);
                }
                isCurrent = true;
                break;
            }

        if (!isCurrent)
        {
            // Если новое текущее устройство не выбрали, то делаем текущим
            // первое устройство.
            (*devices)[0].isCurrent = true;

            if (configConnected())
            {
                Message::Ptr m = createMessage(devices->at(0), Message::Type::Event);
                tcp::listener().send(m);

                data::AudioDevChange audioDevChange {devices->at(0)};
                audioDevChange.changeFlag = data::AudioDevChange::ChangeFlag::Current;

                m = createMessage(audioDevChange, Message::Type::Event);
                tcp::listener().send(m);
            }
        }
    }
    return isRemoved;
}

//-------------------------- PulseAudio callback -----------------------------

void AudioDev::context_state(pa_context* context, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);
    //pa_operation_ptr o;

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
            ToxPhoneApplication::stop(1);
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

        /** Вывод информации по потоку воспроизведения **
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            //log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT";
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT new; index: " << index;

                O_PTR_MSG(pa_context_get_sink_input_info(context, index, sink_input_info, ad),
                          "Failed call pa_context_get_sink_info_by_index()", context, {})
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT remove; index: " << index;
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                log_debug_m << "PA_SUBSCRIPTION_EVENT_SINK_INPUT change; index: " << index;

                O_PTR_MSG(pa_context_get_sink_input_info(context, index, sink_input_info, ad),
                          "Failed call pa_context_get_sink_info_by_index()", context, {})
            }
            break;
        */

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

        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            // Вывод информации по потоку записи
            //pa_context_get_source_output_info
            break;
    }
}

void AudioDev::card_info(pa_context* context, const pa_card_info* card_info,
                         int eol, void* userdata)
{
    if (eol != 0)
        return;

    log_verbose_m << "Sound card detected"
                  << "; index: " << card_info->index
                  << "; name: " << card_info->name;

    /** Расширенная информация по звуковой карте **
    pa_card_profile_info* profile = info->profiles;
    for (size_t i = 0; i < info->n_profiles; ++i)
    {
        log_debug2_m << "Sound card " << info->index
                     << "; Profile: " << profile->name
                     << "; description: " << profile->description;
        ++profile;
    }
    if (pa_card_profile_info* activeProfile = info->active_profile)
    {
        log_debug2_m << "Sound card " << info->index
                     << "; Active profile: " << activeProfile->name
                     << "; description: " << activeProfile->description;
    }
    else
    {
        log_debug2_m << "Sound card " << info->index
                     << "; Active profile is undefined";
    }
    */


    AudioDev* ad = static_cast<AudioDev*>(userdata);

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

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    QMutexLocker locker(&ad->_devicesLock); (void) locker;
    lst::FindResult fr = ad->_sinkDevices.find(info->name);
    if (fr.failed())
        log_verbose_m << "Sound sink detected"
                      << "; index: " << info->index
                      << "; (card: " << info->card << ")"
                      << "; volume: " << info->volume.values[0]
                      << "; name: " << info->name;
                      //<< "; description: " << sinkDescr

    ad->updateAudioDevInfo(info, ad->_sinkDevices);
}

void AudioDev::sink_change(pa_context* context, const pa_sink_info* info,
                           int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);

    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    QMutexLocker locker(&ad->_devicesLock); (void) locker;
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

void AudioDev::sink_input_info(pa_context* context, const pa_sink_input_info* info,
                               int eol, void* userdata)
{
    if (eol != 0)
        return;

    log_debug_m << "Sound sink input changed"
                << "; index: " << info->index
                << "; name: " << info->name
                //<< "; (card: " << info->card << ")"
                << "; has_volume: " << info->has_volume
                << "; volume_writable: " << info->volume_writable
                << "; volume: " << info->volume.values[0];
}

void AudioDev::source_info(pa_context* context, const pa_source_info* info,
                           int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);

    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    QMutexLocker locker(&ad->_devicesLock); (void) locker;
    lst::FindResult fr = ad->_sourceDevices.find(info->name);
    if (fr.failed())
        log_verbose_m << "Sound source detected"
                      << "; index: " << info->index
                      << "; (card: " << info->card << ")"
                      << "; volume: " << info->volume.values[0]
                      << "; name: " << info->name;
                      //<< "; description: " << sourceDescr

    ad->updateAudioDevInfo(info, ad->_sourceDevices);
}

void AudioDev::source_change(pa_context* context, const pa_source_info* info,
                             int eol, void* userdata)
{
    AudioDev* ad = static_cast<AudioDev*>(userdata);

    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    QMutexLocker locker(&ad->_devicesLock); (void) locker;
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

void AudioDev::ringtone_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "ringtone_stream_state_cb()";

    switch (pa_stream_get_state(stream))
    {
        case PA_STREAM_CREATING:
            log_debug2_m << "Ringtone stream event: PA_STREAM_CREATING";
            break;

        case PA_STREAM_TERMINATED:
            log_debug2_m << "Ringtone stream event: PA_STREAM_TERMINATED";
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Ringtone stream event: PA_STREAM_READY";
            log_debug_m  << "Ringtone stream started";
            break;

        case PA_STREAM_FAILED:
            log_error_m << "Ringtone stream event: PA_STREAM_FAILED"
                        << paStrError(stream);
        default:
            break;
    }
}

void AudioDev::ringtone_stream_started(pa_stream* stream, void* userdata)
{
    log_debug2_m << "ringtone_stream_started_cb()";

    //AudioDev* ad = static_cast<AudioDev*>(userdata);
    //ad->_sinkStreamPlaying = true;

}

void AudioDev::ringtone_stream_write(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "ringtone_stream_write_cb()";
    AudioDev* ad = static_cast<AudioDev*>(userdata);

    void* data;
    if (pa_stream_begin_write(stream, &data, &nbytes) < 0)
    {
        log_error_m << "Failed call pa_stream_begin_write()" << paStrError(stream);
        return;
    }

    qint64 len = ad->_ringtoneFile.read((char*)data, nbytes);
    if (len == qint64(nbytes))
    {
        if (pa_stream_write(stream, data, len, 0, 0LL, PA_SEEK_RELATIVE) < 0)
            log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
    }
    else
    {
        log_debug2_m << "Ringtone cycle";
        bool failRead = true;
        if (ad->_ringtoneFile.seek(ad->_ringtoneFile.dataChunkPos()))
        {
            len = ad->_ringtoneFile.read((char*)data, nbytes);
            if (len > 0)
            {
                if (pa_stream_write(stream, data, len, 0, 0LL, PA_SEEK_RELATIVE) < 0)
                    log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
                else
                    failRead = false;
            }
        }
        if (failRead)
        {
            log_debug_m << "Ringtone data empty";
            pa_stream_cancel_write(stream);
            pa_stream_set_write_callback(stream, 0, 0);
            O_PTR_MSG(pa_stream_drain(stream, ringtone_stream_drain, ad),
                      "Failed call pa_stream_drain()", stream, {})
        }
    }
}

void AudioDev::ringtone_stream_overflow(pa_stream*, void* userdata)
{
    log_debug2_m << "ringtone_stream_overflow_cb()";
}

void AudioDev::ringtone_stream_underflow(pa_stream* stream, void* userdata)
{
    log_debug2_m << "ringtone_stream_underflow_cb()";
}

void AudioDev::ringtone_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "ringtone_stream_suspended_cb()";
}

void AudioDev::ringtone_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "ringtone_stream_moved_cb()";
}

void AudioDev::ringtone_stream_drain(pa_stream* stream, int success, void *userdata)
{
    log_debug2_m << "ringtone_stream_drain_cb()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    ad->_ringtoneFile.close();

    if (pa_stream_disconnect(stream) < 0)
        log_error_m << "Failed call pa_stream_disconnect()" << paStrError(stream);

    pa_stream_unref(stream);
    ad->_ringtoneStream = 0;
    ad->_ringtoneActive = false;

    log_debug_m << "Ringtone stream stopped";
}

void AudioDev::playback_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_state_cb()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    const pa_buffer_attr* attr = 0;
    char cmt[PA_CHANNEL_MAP_SNPRINT_MAX];
    char sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

    switch (pa_stream_get_state(stream))
    {
        case PA_STREAM_CREATING:
            log_debug2_m << "Playback stream event: PA_STREAM_CREATING";
            break;

        case PA_STREAM_TERMINATED:
            log_debug2_m << "Playback stream event: PA_STREAM_TERMINATED";
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Playback stream event: PA_STREAM_READY";

            if (ad->_playbackVoice)
                log_debug_m  << "Voice playback stream started";
            else
                log_debug_m  << "Playback stream started";

            if (ad->_playbackVoice)
            {

                if (VoiceFrameInfo::Ptr voiceFrameInfo = playbackVoiceFrameInfo())
                {
                    size_t rbuffSize = 8 * voiceFrameInfo->bufferSize;
                    playbackVoiceRBuff().init(rbuffSize);
                    log_debug_m  << "Initialization a voice playback ring buffer"
                                 << "; size: " << rbuffSize;
                }
                else
                    log_error_m << "Failed get VoiceFrameInfo for playback";
            }

            if (alog::logger().level() >= alog::Level::Debug)
            {
                if (!!(attr = pa_stream_get_buffer_attr(stream)))
                {
                    log_debug_m << "PA playback buffer metrics"
                                << "; maxlength: " << attr->maxlength
                                << ", fragsize: " << attr->fragsize;

                    log_debug_m << "PA playback using sample spec "
                                << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(stream))
                                << ", channel map "
                                << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(stream));

                    log_debug_m << "PA playback connected to device "
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
            log_error_m << "Playback stream event: PA_STREAM_FAILED"
                        << paStrError(stream);
        default:
            break;
    }
}

void AudioDev::playback_stream_started(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_started_cb()";
}

void AudioDev::playback_stream_write(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "playback_stream_write_cb() nbytes: " << nbytes;

    AudioDev* ad = static_cast<AudioDev*>(userdata); (void) ad;

    void* data;
    if (pa_stream_begin_write(stream, &data, &nbytes) < 0)
    {
        log_error_m << "Failed call pa_stream_begin_write()" << paStrError(stream);
        return;
    }

    if (ad->_playbackVoice)
    {
        if (playbackVoiceRBuff().read((char*)data, nbytes))
            ad->_playbackBytes += nbytes;
        else
            memset(data, 0, nbytes);

        if (pa_stream_write(stream, data, nbytes, 0, 0LL, PA_SEEK_RELATIVE) < 0)
            log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
    }
    else
    {
        qint64 len = ad->_playbackFile.read((char*)data, nbytes);
        if (len > 0)
        {
            if (pa_stream_write(stream, data, len, 0, 0LL, PA_SEEK_RELATIVE) < 0)
                log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
        }
        else
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
    log_debug2_m << "playback_stream_overflow_cb()";
}

void AudioDev::playback_stream_underflow(pa_stream* stream, void* userdata)
{
    log_debug2_m << "playback_stream_underflow_cb()";
}

void AudioDev::playback_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "playback_stream_suspended_cb()";
}

void AudioDev::playback_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "playback_stream_moved_cb()";
}

void AudioDev::playback_stream_drain(pa_stream* stream, int success, void *userdata)
{
    log_debug2_m << "playback_stream_drain_cb()";

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    ad->_playbackFile.close();

    if (ad->_playbackTest)
    {
        if (configConnected())
        {
            data::AudioTest audioTest;
            audioTest.begin = false;
            audioTest.playback = true;

            Message::Ptr m = createMessage(audioTest, Message::Type::Event);
            tcp::listener().send(m);
        }
        ad->_playbackTest = false;
    }

    if (pa_stream_disconnect(stream) < 0)
        log_error_m << "Failed call pa_stream_disconnect()" << paStrError(stream);

    pa_stream_unref(stream);
    ad->_playbackStream = 0;
    ad->_playbackActive = false;

    log_debug_m << "Playback stream stopped (file: " << ad->_playbackFile.fileName() << ")";
}

void AudioDev::record_stream_state(pa_stream* stream, void* userdata)
{
    log_debug2_m << "record_stream_state_cb()";

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
            break;

        case PA_STREAM_READY:
            log_debug2_m << "Record stream event: PA_STREAM_READY";
            log_debug_m  << "Record stream started";

            if (VoiceFrameInfo::Ptr voiceFrameInfo = recordVoiceFrameInfo())
            {
                recordVoiceRBuff().init(5 * voiceFrameInfo->bufferSize);
                log_debug_m  << "Initialization a record ring buffer"
                             << "; size: " << recordVoiceRBuff().size();
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
    log_debug2_m << "record_stream_started_cb()";

}

void AudioDev::record_stream_read(pa_stream* stream, size_t nbytes, void* userdata)
{
    //log_debug2_m << "record_stream_read_cb()";

    AudioDev* ad = static_cast<AudioDev*>(userdata); (void) ad;
    //size_t sampleSize = pa_sample_size(sampleSpec);

    //return;

    //size_t frame_size = pa_frame_size(sampleSpec);
    //size_t sample_size = pa_sample_size(sampleSpec);
    //log_debug2_m << "frame_size: " << frame_size;
    //log_debug2_m << "sample_size: " << sample_size;

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

                if (recordVoiceRBuff().write((char*)data, nbytes))
                    ad->_voiceFilters.bufferUpdated();
                else
                    log_error_m << "Failed write data to recordVoiceRBuff"
                                << ". Data size: " << nbytes;
            }
        }
        if (nbytes)
            pa_stream_drop(stream);
     }
}

void AudioDev::record_stream_overflow(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_overflow_cb()";
}

void AudioDev::record_stream_underflow(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_underflow_cb()";
}

void AudioDev::record_stream_suspended(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_suspended_cb()";
}

void AudioDev::record_stream_moved(pa_stream*, void* userdata)
{
    log_debug2_m << "record_stream_moved_cb()";
}



#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
