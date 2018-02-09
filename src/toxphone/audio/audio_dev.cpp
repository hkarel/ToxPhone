#include "audio_dev.h"
#include "toxphone_appl.h"
#include "common/functions.h"

#include "shared/break_point.h"
#include "shared/simple_ptr.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/base.h"
#include "shared/qt/communication/transport/tcp.h"

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
    FUNC_REGISTRATION(AudioDev)
    FUNC_REGISTRATION(AudioSinkTest)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

bool AudioDev::init()
{
//    if (_init)
//    {
//        log_error_m << "AudioDev module already initialized";
//        return false;
//    }


    //_paMainLoop = pa_mainloop_new();
    _paMainLoop = pa_threaded_mainloop_new();
    if (!_paMainLoop)
    {
        log_error_m << "Failed call pa_threaded_mainloop_new()";
        return false;
    }

    //_paApi = pa_mainloop_get_api(_paMainLoop);
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

    //QTimer::singleShot(2*1000, this, SLOT(audioSinkTest()));

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

void AudioDev::audioSinkTest()
{
    if (ringtoneActive())
        stopRingtone();
    else
        startRingtone();

//    if (_sinkStreamPlaying)
//    {
//        stopSinkStream();
//        return;
//    }

//    //createDefaultSinkStream();
//    //return;

//    //QString fileName = getFilePath("sound/sound_test.wav");
//    QString fileName = getFilePath("sound/ring.wav");
//    //QString fileName = "/tmp/2.wav";
//    if (fileName.isEmpty())
//    {
//        log_error_m << "File sound_test.wav not found";
//        return;
//    }

//    if (_sinkTestFile.isOpen())
//        _sinkTestFile.close();

//    _sinkTestFile.setFileName(fileName);
//    if (!_sinkTestFile.open())
//    {
//        log_error_m << "Failed open the sound file " << fileName;
//        return;
//    }
//    if (_sinkTestFile.header().bitsPerSample != 16)
//    {
//        log_error_m << "Only 16 bits per sample is supported"
//                    << ". FIle: " << _sinkTestFile.fileName();
//        _sinkTestFile.close();
//        return;
//    }

//    pa_sample_spec paSampleSpec;
//    paSampleSpec.format = PA_SAMPLE_S16LE;
//    paSampleSpec.channels = _sinkTestFile.header().numChannels;
//    paSampleSpec.rate = _sinkTestFile.header().sampleRate;

//    playSinkStream(&paSampleSpec);
}

void AudioDev::startRingtone()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_ringtoneActive)
        return;

    QString fileName = getFilePath("sound/ringtone.wav");
    if (fileName.isEmpty())
    {
        log_error_m << "File ringtone.wav not found";
        return;
    }
    if (_ringtoneFile.isOpen())
        _ringtoneFile.close();

    _ringtoneFile.setFileName(fileName);
    if (!_ringtoneFile.open())
    {
        log_error_m << "Failed open the ringtone file " << fileName;
        return;
    }
    if (_ringtoneFile.header().bitsPerSample != 16)
    {
        log_error_m << "Only 16 bits per sample is supported"
                    << ". FIle: " << _ringtoneFile.fileName();
        _sinkTestFile.close();
        return;
    }

    if (_ringtoneStream)
    {
        log_debug_m << "Ringtone stream drop";
        pa_stream_set_write_callback(_ringtoneStream, 0, 0);

        if (pa_stream_disconnect(_ringtoneStream) < 0)
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_ringtoneStream);

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

    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
    if (pa_stream_connect_playback(_ringtoneStream, 0, 0, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_ringtoneStream);
        ToxPhoneApplication::stop(1);
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
        this_thread::sleep_for(chrono::milliseconds(10));

    if (pa_stream_disconnect(_ringtoneStream) < 0)
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_ringtoneStream);

    pa_stream_unref(_ringtoneStream);
    _ringtoneActive = false;
    _ringtoneStream = 0;
}

void AudioDev::startPlayback()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_playbackActive)
        return;

//    QString fileName = getFilePath("sound/ringtone.wav");
//    if (fileName.isEmpty())
//    {
//        log_error_m << "File ringtone.wav not found";
//        return;
//    }
//    if (_ringtoneFile.isOpen())
//        _ringtoneFile.close();

//    _ringtoneFile.setFileName(fileName);
//    if (!_ringtoneFile.open())
//    {
//        log_error_m << "Failed open the ringtone file " << fileName;
//        return;
//    }
//    if (_ringtoneFile.header().bitsPerSample != 16)
//    {
//        log_error_m << "Only 16 bits per sample is supported"
//                    << ". FIle: " << _ringtoneFile.fileName();
//        _sinkTestFile.close();
//        return;
//    }

    if (_playbackStream)
    {
        log_debug_m << "Playback stream drop";
        pa_stream_set_write_callback(_playbackStream, 0, 0);

        if (pa_stream_disconnect(_playbackStream) < 0)
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_playbackStream);

        pa_stream_unref(_playbackStream);
        log_debug_m << "Playback stream dropped";
    }

    log_debug_m << "Create playback stream";

    pa_sample_spec paSampleSpec;
//    paSampleSpec.format = PA_SAMPLE_S16LE;
//    paSampleSpec.channels = _ringtoneFile.header().numChannels;
//    paSampleSpec.rate = _ringtoneFile.header().sampleRate;

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

    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
    if (pa_stream_connect_playback(_playbackStream, 0, 0, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_playbackStream);
        ToxPhoneApplication::stop(1);
        return;
    }

    _playbackActive = true;
    log_debug_m << "Playback stream start";
}

void AudioDev::startPlaybackVoice(const VoiceFrameInfo::Ptr& voiceFrameInfo)
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (_playbackActive)
        return;

    if (_playbackStream)
    {
        log_debug_m << "Playback stream drop";
        pa_stream_set_write_callback(_playbackStream, 0, 0);

        if (pa_stream_disconnect(_playbackStream) < 0)
            log_error_m << "Failed call pa_stream_connect_playback()"
                        << paStrError(_playbackStream);

        pa_stream_unref(_playbackStream);
        log_debug_m << "Playback stream dropped";
    }

    _playbackBytes = 0;
    log_debug_m << "Create voice stream";

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
                << "; latency: "      << voiceFrameInfo->latency
                << "; channels: " << int(voiceFrameInfo->channels)
                << "; sampleSize: "   << voiceFrameInfo->sampleSize
                << "; sampleCount: "  << voiceFrameInfo->sampleCount
                << "; samplingRate: " << voiceFrameInfo->samplingRate
                << "; bufferSize: "   << voiceFrameInfo->bufferSize;

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

    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    if (pa_stream_connect_playback(_playbackStream, 0, &paBuffAttr, flags, 0, 0) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_playbackStream);
        ToxPhoneApplication::stop(1);
        return;
    }

//    _sourceTestFile.setFileName("/tmp/222.wav");
//    if (!_sourceTestFile.open(QIODevice::WriteOnly))
//    {
//        log_error_m << "Failed open /tmp/222.wav";
//        ToxPhoneApplication::stop(1);
//        return;
//    }

    _playbackActive = true;
    _playbackVoice = true;
    log_debug_m << "Playback stream start";
}

void AudioDev::stopPlayback()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_playbackActive)
        return;

    log_debug_m << "Playback stream stop";
    log_debug_m << "Playback ring buffer; available: "
                << playbackVoiceRBuff().available();

    playbackVoiceRBuff().reset();
    playbackVoiceFrameInfo(0, true);

    pa_stream_set_write_callback(_playbackStream, 0, 0);
    if (!_playbackVoice)
    {
        O_PTR_MSG(pa_stream_drain(_playbackStream, playback_stream_drain, this),
                  "Failed call pa_stream_drain()", _ringtoneStream, {})

        while (_playbackActive)
            this_thread::sleep_for(chrono::milliseconds(10));
    }
    if (pa_stream_disconnect(_playbackStream) < 0)
    {
        log_error_m << "Failed call pa_stream_disconnect()"
                    << paStrError(_playbackStream);
    }
    pa_stream_unref(_playbackStream);
    _playbackActive = false;
    _playbackVoice = false;
    _playbackStream = 0;

    log_debug_m << "Playback bytes: " << _playbackBytes;
    log_debug_m << "Playback stream stopped";
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

    _recordBytes = 0;
    log_debug_m << "Create record stream";

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
    quint32 bufferSize = pa_usec_to_bytes(latency, &paSampleSpec);
    quint32 sampleSize = pa_sample_size(&paSampleSpec);
    quint32 sampleCount = bufferSize / (sampleSize * paSampleSpec.channels);
    VoiceFrameInfo voiceFrameInfo {latency, paSampleSpec.channels, sampleSize,
                                   sampleCount, paSampleSpec.rate, bufferSize};

    recordVoiceFrameInfo(&voiceFrameInfo);
    log_debug_m << "Initialization record VoiceFrameInfo"
                << "; latency: "      << voiceFrameInfo.latency
                << "; channels: " << int(voiceFrameInfo.channels)
                << "; sampleSize: "   << voiceFrameInfo.sampleSize
                << "; sampleCount: "  << voiceFrameInfo.sampleCount
                << "; samplingRate: " << voiceFrameInfo.samplingRate
                << "; bufferSize: "   << voiceFrameInfo.bufferSize;

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

    //const char* dev = "alsa_input.pci-0000_03_01.0.analog-stereo";

    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
                                                |PA_STREAM_ADJUST_LATENCY
                                                |PA_STREAM_AUTO_TIMING_UPDATE);

    if (pa_stream_connect_record(_recordStream, 0, &paBuffAttr, flags) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_record()"
                    << paStrError(_recordStream);
        return;
    }

    _recordActive = true;
    log_debug_m << "Record stream start";
}

void AudioDev::stopRecord()
{
    QMutexLocker locker(&_streamLock); (void) locker;

    if (!_recordActive)
        return;

    log_debug_m << "Record stream stop";
    log_debug_m << "Record ring buffer; available: "
                << recordVoiceRBuff().available();

    recordVoiceRBuff().reset();
    recordVoiceFrameInfo(0, true);

    pa_stream_set_read_callback(_recordStream, 0, 0);
    if (pa_stream_disconnect(_recordStream) < 0)
    {
        log_error_m << "Failed call pa_stream_connect_playback()"
                    << paStrError(_recordStream);
    }
    pa_stream_unref(_recordStream);
    _recordActive = false;
    _recordStream = 0;

    log_debug_m << "Record bytes: " << _recordBytes;
    log_debug_m << "Record stream stopped";
}

void AudioDev::start()
{
    pa_threaded_mainloop_start(_paMainLoop);
}

bool AudioDev::stop(unsigned long time)
{
//    if (_paMainLoop)
//        pa_mainloop_quit(_paMainLoop, 0);
//    return QThreadEx::stop(time);

    if (_paMainLoop)
        pa_threaded_mainloop_stop(_paMainLoop);

    deinit();
    return true;
}

//void AudioDev::run()
//{
//    log_info_m << "Started";

//    int retval;
//    int res = pa_mainloop_run(_paMainLoop, &retval);
//    (void) res;

//    deinit();

//    _sourceTestFile.close();

//    log_info_m << "Stopped";
//}

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
    updateDevicesInConfig(data::AudioDevType::Sink);
    updateDevicesInConfig(data::AudioDevType::Source);
}

void AudioDev::command_AudioDev(const Message::Ptr& message)
{
    data::AudioDev audioDev;
    readFromMessage(message, audioDev);

    pa_cvolume volume;
    volume.channels = audioDev.channels;
    for (quint8 i = 0; i < volume.channels; ++i)
        volume.values[i] = audioDev.currentVolume; // * 1.2;

    if (audioDev.type == data::AudioDevType::Sink)
    {
        O_PTR_MSG(pa_context_set_sink_volume_by_index(_paContext, audioDev.index, &volume, 0, 0),
                  "Failed call pa_context_set_sink_volume_by_index()", _paContext, {})
    }
    else
    {
        O_PTR_MSG(pa_context_set_source_volume_by_index(_paContext, audioDev.index, &volume, 0, 0),
                  "Failed call pa_context_set_source_volume_by_index()", _paContext, {})
    }
}

void AudioDev::command_AudioSinkTest(const Message::Ptr& message)
{
    audioSinkTest();

}

int AudioDev::findDevByName(const QVector<data::AudioDev>& devices, const QByteArray& name)
{
    int index = -1;
    for (int i = 0; i < devices.count(); ++i)
        if (devices[i].name == name)
        {
            index = i;
            break;
        }

    return index;
}

void AudioDev::updateDevices(const data::AudioDev& audioDev)
{
    QMutexLocker locker(&_devicesLock); (void) locker;
    QVector<data::AudioDev>* devices;
    if (audioDev.type == data::AudioDevType::Sink)
        devices = &_sinkDevices;
    else
        devices = &_sourceDevices;

    int index = findDevByName(*devices, audioDev.name);
    if (index != -1)
        (*devices)[index] = audioDev;
    else
        devices->append(audioDev);
}

void AudioDev::updateDevicesInConfig(data::AudioDevType type)
{
    if (!configConnected())
        return;

    QMutexLocker locker(&_devicesLock); (void) locker;
    data::AudioDevList audioDevList;
    if (type == data::AudioDevType::Sink)
    {
        audioDevList.type = data::AudioDevType::Sink;
        audioDevList.list = _sinkDevices;
    }
    else
    {
        audioDevList.type = data::AudioDevType::Source;
        audioDevList.list = _sourceDevices;
    }

    Message::Ptr m = createMessage(audioDevList, Message::Type::Event);
    tcp::listener().send(m);
}

bool AudioDev::removeDeviceByCardIndex(int index, data::AudioDevType type)
{
    QMutexLocker locker(&_devicesLock); (void) locker;
    QVector<data::AudioDev>* devices;
    if (type == data::AudioDevType::Sink)
        devices = &_sinkDevices;
    else
        devices = &_sourceDevices;

    bool ret = false;
    for (int i = 0; i < devices->count(); ++i)
        if (devices->at(i).cardIndex == index)
        {
            devices->remove(i--);
            ret = true;
        }
    return ret;
}

bool AudioDev::removeDeviceByIndex(int index, data::AudioDevType type)
{
    QMutexLocker locker(&_devicesLock); (void) locker;
    QVector<data::AudioDev>* devices;
    if (type == data::AudioDevType::Sink)
        devices = &_sinkDevices;
    else
        devices = &_sourceDevices;

    bool ret = false;
    for (int i = 0; i < devices->count(); ++i)
        if (devices->at(i).index == index)
        {
            devices->remove(i--);
            ret = true;
        }
    return ret;
}

//void AudioDev::createDefaultSinkStream()
//{
////    if (_sinkStream)
////        return;

////    log_debug_m << "Create sink stream (default)";

////    pa_sample_spec paSampleSpec;
////    paSampleSpec.format = PA_SAMPLE_S16LE;
////    //paSampleSpec.channels = 2;
////    //paSampleSpec.rate = 48000;
////    paSampleSpec.channels = 1;
////    paSampleSpec.rate = 44100;

////    // Создаем Sink Stream
////    _sinkStream = pa_stream_new(_paContext, "Playback", &paSampleSpec, 0);
////    if (!_sinkStream)
////    {
////        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
////        ToxPhoneApplication::stop(1);
////        return;
////    }

////    pa_stream_set_state_callback    (_sinkStream, sink_stream_state_cb, this);
////    pa_stream_set_started_callback  (_sinkStream, sink_stream_started_cb, this);
////    pa_stream_set_write_callback    (_sinkStream, sink_stream_write_cb, this);
////    pa_stream_set_overflow_callback (_sinkStream, sink_stream_overflow_cb, this);
////    pa_stream_set_underflow_callback(_sinkStream, sink_stream_underflow_cb, this);
////    pa_stream_set_suspended_callback(_sinkStream, sink_stream_suspended_cb, this);
////    pa_stream_set_moved_callback    (_sinkStream, sink_stream_moved_cb, this);

//////    size_t latency = 20000;
//////    pa_buffer_attr paBuffAttr; (void) paBuffAttr;
//////    paBuffAttr.fragsize  = (uint32_t)-1;
//////    paBuffAttr.maxlength = pa_usec_to_bytes(latency, &paSampleSpec);
//////    paBuffAttr.minreq    = (uint32_t)-1;
//////    paBuffAttr.prebuf    = (uint32_t)-1;
//////    paBuffAttr.tlength   = (uint32_t)-1;

//////    pa_buffer_attr paBuffAttr; (void) paBuffAttr;
//////    paBuffAttr.maxlength = pa_usec_to_bytes(latency, &paSampleSpec);
//////    paBuffAttr.tlength   = uint32_t(-1);
//////    paBuffAttr.prebuf    = uint32_t(-1);
//////    paBuffAttr.minreq    = uint32_t(-1);
//////    paBuffAttr.fragsize  = uint32_t(-1);

////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);

////    if (pa_stream_connect_playback(_sinkStream, 0, 0, flags, 0, 0) < 0)
////    {
////        log_error_m << "Failed call pa_stream_connect_playback()"
////                    << paStrError(_sinkStream);
////        ToxPhoneApplication::stop(1);
////    }
//}

//void AudioDev::playSinkStream(const pa_sample_spec* sampleSpec)
//{
////    if (pa_sample_spec_valid(sampleSpec) == 0)
////    {
////        log_error_m << "Invalid sample specification" << paStrError(_paContext);
////        return;
////    }

////    if (_sinkStream)
////    {
////        pa_stream_set_write_callback(_sinkStream, 0, 0);
////        if (pa_stream_disconnect(_sinkStream) < 0)
////        {
////            log_error_m << "Failed call pa_stream_connect_playback()"
////                        << paStrError(_sinkStream);
////            return;
////        }
////        pa_stream_unref(_sinkStream);
////    }

////    log_debug_m << "Create sink stream";

////    _sinkStream = pa_stream_new(_paContext, "Playback", sampleSpec, 0);
////    if (!_sinkStream)
////    {
////        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
////        ToxPhoneApplication::stop(1);
////        return;
////    }

////    pa_stream_set_state_callback    (_sinkStream, sink_stream_state_cb, this);
////    pa_stream_set_started_callback  (_sinkStream, sink_stream_started_cb, this);
////    pa_stream_set_write_callback    (_sinkStream, sink_stream_write_cb, this);
////    pa_stream_set_overflow_callback (_sinkStream, sink_stream_overflow_cb, this);
////    pa_stream_set_underflow_callback(_sinkStream, sink_stream_underflow_cb, this);
////    pa_stream_set_suspended_callback(_sinkStream, sink_stream_suspended_cb, this);
////    pa_stream_set_moved_callback    (_sinkStream, sink_stream_moved_cb, this);

////    size_t latency = 20000;
//////    pa_buffer_attr paBuffAttr;
//////    paBuffAttr.fragsize  = (uint32_t)-1;
//////    paBuffAttr.maxlength = pa_usec_to_bytes(latency, sampleSpec);
//////    paBuffAttr.minreq    = (uint32_t)-1;
//////    paBuffAttr.prebuf    = (uint32_t)-1;
//////    paBuffAttr.tlength   = (uint32_t)-1;

////    pa_buffer_attr paBuffAttr; (void) paBuffAttr;
////    paBuffAttr.maxlength = pa_usec_to_bytes(latency, sampleSpec);
////    paBuffAttr.tlength   = uint32_t(-1);
////    paBuffAttr.prebuf    = uint32_t(-1);
////    paBuffAttr.minreq    = uint32_t(-1);
////    paBuffAttr.fragsize  = uint32_t(-1);

////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
////                                                |PA_STREAM_ADJUST_LATENCY
////                                                |PA_STREAM_AUTO_TIMING_UPDATE);
//////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_ADJUST_LATENCY);

////    //if (pa_stream_connect_playback(_sinkStream, 0, &paBuffAttr, flags, 0, 0) < 0)
////    if (pa_stream_connect_playback(_sinkStream, 0, 0, flags, 0, 0) < 0)
////    {
////        log_error_m << "Failed call pa_stream_connect_playback()"
////                    << paStrError(_sinkStream);
////        ToxPhoneApplication::stop(1);
////        return;
////    }

////    _sinkStreamPlaying = true;
////    log_debug_m << "Playback start";
//}

//void AudioDev::stopSinkStream()
//{
////    if (_sinkStream == 0)
////        return;

////    log_debug_m << "Playback stop";
////    pa_stream_set_write_callback(_sinkStream, 0, 0);

////    O_PTR_MSG(pa_stream_drain(_sinkStream, sink_stream_drain_complete, this),
////              "Failed call pa_stream_drain()", _sinkStream, {})
//}

//void AudioDev::createSourceStream()
//{
////    if (_sourceStream)
////        return;

////    log_debug_m << "Create source stream";

////    _sourceTestFile.setFileName("/tmp/222.wav");
////    if (!_sourceTestFile.open(QIODevice::WriteOnly))
////    {
////        log_error_m << "Failed open /tmp/222.wav";
////        ToxPhoneApplication::stop(1);
////        return;
////    }

////    pa_sample_spec paSampleSpec;
////    paSampleSpec.format = PA_SAMPLE_S16LE;
////    //paSampleSpec.format = PA_SAMPLE_U8;
////    paSampleSpec.channels = 2;
////    paSampleSpec.rate = 44100;
////    paSampleSpec.rate = 48000;

////    _sourceStream = pa_stream_new(_paContext, "Recording", &paSampleSpec, 0);
////    if (!_sourceStream)
////    {
////        log_error_m << "Failed call pa_stream_new()" << paStrError(_paContext);
////        ToxPhoneApplication::stop(1);
////        return;
////    }

////    pa_stream_set_state_callback    (_sourceStream, source_stream_state_cb, this);
////    pa_stream_set_started_callback  (_sourceStream, source_stream_started_cb, this);
////    pa_stream_set_write_callback    (_sourceStream, source_stream_write_cb, this);
////    pa_stream_set_read_callback     (_sourceStream, source_stream_read_cb, this);
////    pa_stream_set_overflow_callback (_sourceStream, source_stream_overflow_cb, this);
////    pa_stream_set_underflow_callback(_sourceStream, source_stream_underflow_cb, this);
////    pa_stream_set_suspended_callback(_sourceStream, source_stream_suspended_cb, this);
////    pa_stream_set_moved_callback    (_sourceStream, source_stream_moved_cb, this);

////    size_t latency = 20000;
////    pa_buffer_attr paBuffAttr; (void) paBuffAttr;
////    paBuffAttr.maxlength = 3 * pa_usec_to_bytes(latency, &paSampleSpec);
////    paBuffAttr.tlength   = uint32_t(-1);
////    paBuffAttr.prebuf    = uint32_t(-1);
////    paBuffAttr.minreq    = uint32_t(-1);
////    paBuffAttr.fragsize  = pa_usec_to_bytes(latency, &paSampleSpec);

////    //const char* dev = "alsa_input.pci-0000_03_01.0.analog-stereo";

////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING
////                                                |PA_STREAM_ADJUST_LATENCY
////                                                |PA_STREAM_AUTO_TIMING_UPDATE);
//////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_NOFLAGS);
//////    pa_stream_flags_t flags = pa_stream_flags_t(PA_STREAM_ADJUST_LATENCY);

////    int res = pa_stream_connect_record(_sourceStream, 0, &paBuffAttr, flags);

////    if (res < 0)
////    {
////        log_error_m << "Failed call pa_stream_connect_record()"
////                    << paStrError(_sinkStream);
////        ToxPhoneApplication::stop(1);
////    }
//}

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
                       "Failed call pa_context_get_card_info_list()", context,
                       break)

            //ad->createDefaultSinkStream();
            //ad->createSourceStream();
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

                if (ad->removeDeviceByCardIndex(index, data::AudioDevType::Sink))
                    ad->updateDevicesInConfig(data::AudioDevType::Sink);

                if (ad->removeDeviceByCardIndex(index, data::AudioDevType::Source))
                    ad->updateDevicesInConfig(data::AudioDevType::Source);
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
                if (ad->removeDeviceByIndex(index, data::AudioDevType::Sink))
                    ad->updateDevicesInConfig(data::AudioDevType::Sink);
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                O_PTR_MSG(pa_context_get_sink_info_by_index(context, index, sink_volume, ad),
                          "Failed call pa_context_get_sink_info_by_index()", context, {})
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            {
                log_verbose_m << "Sound source detached; index: " << index;
                if (ad->removeDeviceByIndex(index, data::AudioDevType::Source))
                    ad->updateDevicesInConfig(data::AudioDevType::Source);
            }
            else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE)
            {
                O_PTR_MSG(pa_context_get_source_info_by_index(context, index, source_volume, ad),
                          "Failed call pa_context_get_source_info_by_index()", context, {})
            }
            break;
    }
}

void AudioDev::card_info(pa_context* context, const pa_card_info* info,
                         int eol, void* userdata)
{
    if (eol != 0)
        return;

    log_verbose_m << "Sound card detected"
                  << "; index: " << info->index
                  << "; name: " << info->name;

    /*
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
    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    log_verbose_m << "Sound sink detected"
                  << "; index: " << info->index
                  << "; (card: " << info->card << ")"
                  << "; volume: " << info->volume.values[0]
                  << "; name: " << info->name;
                  //<< "; description: " << sinkDescr

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    data::AudioDev audioDev;
    ad->fillAudioDev(info, audioDev);
    ad->updateDevices(audioDev);
    ad->updateDevicesInConfig(data::AudioDevType::Sink);
}

void AudioDev::sink_volume(pa_context* context, const pa_sink_info* info,
                           int eol, void* userdata)
{
    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    log_debug_m << "Sound sink changed"
                << "; index: " << info->index
                << "; (card: " << info->card << ")"
                << "; volume: " << info->volume.values[0]
                << "; name: " << info->name;

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    data::AudioDev audioDev;
    ad->fillAudioDev(info, audioDev);
    ad->updateDevices(audioDev);

    if (configConnected())
    {
        audioDev.changeFlag = data::AudioDev::ChangeFlag::Volume;
        Message::Ptr m = createMessage(audioDev, Message::Type::Event);
        tcp::listener().send(m);
    }
}

void AudioDev::source_info(pa_context* context, const pa_source_info* info,
                           int eol, void* userdata)
{
    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    log_verbose_m << "Sound source detected"
                  << "; index: " << info->index
                  << "; (card: " << info->card << ")"
                  << "; volume: " << info->volume.values[0]
                  << "; name: " << info->name;
                  //<< "; description: " << sourceDescr

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    data::AudioDev audioDev;
    ad->fillAudioDev(info,  audioDev);
    ad->updateDevices(audioDev);
    ad->updateDevicesInConfig(data::AudioDevType::Source);
}

void AudioDev::source_volume(pa_context* context, const pa_source_info* info,
                             int eol, void* userdata)
{
    if (eol != 0)
        return;

    if ((info->flags & PA_SOURCE_HARDWARE) != PA_SOURCE_HARDWARE)
        return;

    log_debug_m << "Sound source changed"
                << "; index: " << info->index
                << "; (card: " << info->card << ")"
                << "; volume: " << info->volume.values[0]
                << "; name: " << info->name;

    AudioDev* ad = static_cast<AudioDev*>(userdata);
    data::AudioDev audioDev;
    ad->fillAudioDev(info, audioDev);
    ad->updateDevices(audioDev);

    if (configConnected())
    {
        audioDev.changeFlag = data::AudioDev::ChangeFlag::Volume;
        Message::Ptr m = createMessage(audioDev, Message::Type::Event);
        tcp::listener().send(m);
    }
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
    //if (len > 0)
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
            log_debug_m  << "Playback stream started";

            if (ad->_playbackVoice)
            {
                if (VoiceFrameInfo::Ptr voiceFrameInfo = playbackVoiceFrameInfo())
                {
                    size_t rbuffSize = 8 * voiceFrameInfo->bufferSize;
                    playbackVoiceRBuff().init(rbuffSize);
                    log_debug_m  << "Initialization a playback ring buffer"
                                 << "; size: " << rbuffSize;
                }
                else
                    log_error_m << "Failed get VoiceFrameInfo for playback";

                if (!(attr = pa_stream_get_buffer_attr(stream)))
                {
                    log_error_m << "Failed call pa_stream_get_buffer_attr()"
                                << paStrError(stream);
                }
                else
                {
                    log_debug2_m << "PA playback buffer metrics"
                                 << "; maxlength: " << attr->maxlength
                                 << ", fragsize: " << attr->fragsize;

                    log_debug2_m << "PA playback using sample spec "
                                 << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(stream))
                                 << ", channel map "
                                 << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(stream));

                    log_debug2_m << "PA playback connected to device "
                                 << pa_stream_get_device_name(stream)
                                 << " (" << pa_stream_get_device_index(stream)
                                 << ", "
                                 << (pa_stream_is_suspended(stream) ? "" : "not ")
                                 << " suspended)";
                }
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

    //AudioDev* ad = static_cast<AudioDev*>(userdata);
    //ad->_sinkStreamPlaying = true;
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

    if (playbackVoiceRBuff().read((char*)data, nbytes))
        ad->_playbackBytes += nbytes;
    else
        memset(data, 0, nbytes);

        float gainFactor = qPow(10.0, (10 / 20.0));

        int16_t* pcm = (int16_t*) data;
        for (quint32 i = 0; i < nbytes / sizeof(int16_t); i += 2)
        {
            // gain amplification with clipping to 16-bit boundaries
            //int ampPCM = qBound<int>(std::numeric_limits<int16_t>::min(),
            //                         qRound(*pcm * 1.0f),
            //                         std::numeric_limits<int16_t>::max());
            int ampPCM = qRound(*pcm * gainFactor);
            *pcm = static_cast<int16_t>(ampPCM);
        }

    if (pa_stream_write(stream, data, nbytes, 0, 0LL, PA_SEEK_RELATIVE) < 0)
        log_error_m << "Failed call pa_stream_write()" << paStrError(stream);
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
    ad->_playbackActive = false;
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
                size_t rbuffSize = 5 * voiceFrameInfo->bufferSize;
                recordVoiceRBuff().init(rbuffSize);
                log_debug_m  << "Initialization a record ring buffer"
                             << "; size: " << rbuffSize;
            }
            else
                log_error_m << "Failed get VoiceFrameInfo for record";

            if (!(attr = pa_stream_get_buffer_attr(stream)))
            {
                log_error_m << "Failed call pa_stream_get_buffer_attr()"
                            << paStrError(stream);
            }
            else
            {
                log_debug2_m << "PA record buffer metrics"
                             << "; maxlength: " << attr->maxlength
                             << ", fragsize: " << attr->fragsize;

                log_debug2_m << "PA record using sample spec "
                             << pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(stream))
                             << ", channel map "
                             << pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(stream));

                log_debug2_m << "PA record connected to device "
                             << pa_stream_get_device_name(stream)
                             << " (" << pa_stream_get_device_index(stream)
                             << ", "
                             << (pa_stream_is_suspended(stream) ? "" : "not ")
                             << " suspended)";
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

                if (!recordVoiceRBuff().write((char*)data, nbytes))
                    log_error_m << "Failed write data to voice ring buffer"
                                << ". Data size: " << nbytes;

                //                if (configConnected())
                //                {
                //                    ad->_sourceBytes += buff;
                //                    ad->_sourceTime += bytes_to_usec;
                //                    if (ad->_sourceTime > 200000 /*Интервал отправки данных 200 мс*/)
                //                    {
                //                        quint32 average = 0;
                //                        quint16* sample = (quint16*)ad->_sourceBytes.constData();
                //                        for (int i = 0; i < ad->_sourceBytes.count(); i += 2)
                //                            average += *sample++;
                //                        average /= ad->_sourceBytes.count();
                //                        average = average << 1;

                //                        log_debug2_m << "sourceLevel.average: " << average;
                //                        log_debug2_m << "sourceLevel.time: " << ad->_sourceTime;

                //                        emit ad->sourceLevel(average, ad->_sourceTime);

                //                        ad->_sourceBytes.clear();
                //                        ad->_sourceTime = 0;
                //                    }
                //                }
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
