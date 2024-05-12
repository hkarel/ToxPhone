#include "voice_filters.h"
#include "voice_frame.h"
#include "toxphone_appl.h"
#include "common/functions.h"

#include "shared/logger/logger.h"
#include "shared/logger/format.h"
#include "shared/config/appl_conf.h"
#include "shared/qt/logger_operators.h"

#include "pproto/commands/pool.h"
#include "pproto/transport/tcp.h"

#include <string>

using namespace std;

extern "C" {
#include "filter_audio.h"
#include "rnnoise.h"
}

#define log_error_m   alog::logger().error  (alog_line_location, "VoiceFilter")
#define log_warn_m    alog::logger().warn   (alog_line_location, "VoiceFilter")
#define log_info_m    alog::logger().info   (alog_line_location, "VoiceFilter")
#define log_verbose_m alog::logger().verbose(alog_line_location, "VoiceFilter")
#define log_debug_m   alog::logger().debug  (alog_line_location, "VoiceFilter")
#define log_debug2_m  alog::logger().debug2 (alog_line_location, "VoiceFilter")

#define RNNOISE_FRAME_SIZE 480

VoiceFilters& voiceFilters()
{
    return safe::singleton<VoiceFilters, 0>();
}

VoiceFilters::VoiceFilters()
{
    chk_connect_q(&tcp::listener(), &tcp::Listener::message,
                  this, &VoiceFilters::message)

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &VoiceFilters::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(AudioNoise)

    #undef FUNC_REGISTRATION
}

void VoiceFilters::wake()
{
    QMutexLocker locker(&_threadLock); (void) locker;
    _threadCond.wakeAll();
}

void VoiceFilters::sendRecordLevet(quint32 maxLevel, quint32 time)
{
    if (toxConfig().isActive())
    {
        data::AudioRecordLevel audioRecordLevel;
        audioRecordLevel.max = maxLevel * 5;
        audioRecordLevel.time = time;

        Message::Ptr m = createMessage(audioRecordLevel);
        m->setPriority(Message::Priority::High);
        toxConfig().send(m);
    }
}

void VoiceFilters::run()
{
    log_info_m << "Started";

    VoiceFrameInfo::Ptr recordFrameInfo = getRecordFrameInfo();
    if (recordFrameInfo.empty())
    {
        log_error_m << "Failed get VoiceFrameInfo for record";
        Application::stop(1);
        return;
    }

    Filter_Audio* webrtcFilter = new_filter_audio(recordFrameInfo->samplingRate);
    if (!webrtcFilter)
    {
        log_error_m << "Failed call new_filter_audio() for noise filter";
        Application::stop(1);
        return;
    }
    enable_disable_filters(webrtcFilter, 0, 1, 0, 0);

    DenoiseState* rnnoiseFilter = rnnoise_create(0);
    data::AudioNoise::FilterType filterType = data::AudioNoise::FilterType::WebRtc;
    quint32 recordDataSize = 0;
    char recordDataBuff[4000];

    _filterChanged = true;

    while (true)
    {
        if (threadStop())
            break;

        if (_filterChanged)
        {
            filterType = rereadFilterType();
            if (filterType == data::AudioNoise::FilterType::WebRtc)
            {
                log_verbose_m << "Using WebRtc noise suppression";
                switch (recordFrameInfo->latency)
                {
                    case 40000:
                        recordDataSize = recordFrameInfo->bufferSize / 4;
                        break;

                    case 20000:
                        recordDataSize = recordFrameInfo->bufferSize / 2;
                        break;

                    case 10000:
                        recordDataSize = recordFrameInfo->bufferSize;
                        break;

                    case 5000:
                        recordDataSize = recordFrameInfo->bufferSize * 2;
                        break;

                    default:
                        recordDataSize = recordFrameInfo->bufferSize;
                }
            }
            else if (filterType == data::AudioNoise::FilterType::RNNoise)
            {
                log_verbose_m << "Using RNNoise noise suppression";
                recordDataSize = RNNOISE_FRAME_SIZE * recordFrameInfo->sampleSize;
            }
            else
            {
                log_verbose_m << "Not used noise suppression";
                recordDataSize = recordFrameInfo->bufferSize;
            }
            _filterChanged = false;
        }

        if (recordRBuff_1().read(recordDataBuff, recordDataSize))
        {
            int16_t* pcm = (int16_t*)recordDataBuff;
            if (filterType == data::AudioNoise::FilterType::WebRtc)
            {
                if (filter_audio(webrtcFilter, pcm, recordDataSize / sizeof(int16_t)) < 0)
                    log_error_m << "Failed call filter_audio() for noise filter";
            }
            else if (filterType == data::AudioNoise::FilterType::RNNoise)
            {
                float channel[RNNOISE_FRAME_SIZE];

                float* c = channel;
                int16_t* p = pcm;
                for (int i = 0; i < RNNOISE_FRAME_SIZE; ++i)
                    *c++ = *p++;

                rnnoise_process_frame(rnnoiseFilter, channel, channel);

                c = channel;
                p = pcm;
                for (int i = 0; i < RNNOISE_FRAME_SIZE; ++i)
                    *p++ = *c++;
            }

            /*** Отладка эхоподавления ***/
            //secondVoiceRB().write((char*)data, recordDataSize);
            /***/

            if (!recordRBuff_2().write((char*)recordDataBuff, recordDataSize))
            {
                log_error_m << "Failed write data to recordRBuff_2"
                            << ". Data size: " << recordDataSize;
            }

            // Уровень сигнала для микрофона отправляем именно из этой точки,
            // т.к. это позволит учитывать уровень усиления сигнала полученный
            // в функции filter_audio() при активном флаге gain.
            if (toxConfig().isActive())
            {
                pcm = (int16_t*)recordDataBuff;
                for (size_t i = 0; i < (recordDataSize / sizeof(int16_t)); ++i)
                {
                    if (*pcm > 0)
                        if (_recordLevetMax < quint32(*pcm))
                            _recordLevetMax = *pcm;
                    ++pcm;
                }
                if (_recordLevetTimer.elapsed() > 200)
                {
                    sendRecordLevet(_recordLevetMax, 200);
                    _recordLevetMax = 0;
                    _recordLevetTimer.reset();
                }
            }
        }

        if (!threadStop()
            && recordRBuff_1().available() < recordDataSize)
        {
            QMutexLocker locker(&_threadLock); (void) locker;
            _threadCond.wait(&_threadLock, 10);
        }
    }
    kill_filter_audio(webrtcFilter);
    rnnoise_destroy(rnnoiseFilter);

    _recordLevetMax = 0;
    sendRecordLevet(_recordLevetMax, 200);

    log_info_m << "Stopped";
}

void VoiceFilters::message(const pproto::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (lst::FindResult fr = _funcInvoker.findCommand(message->command()))
    {
        if (command::pool().commandIsSinglproc(message->command()))
            message->markAsProcessed();
        _funcInvoker.call(message, fr);
    }
}

void VoiceFilters::command_IncomingConfigConnection(const Message::Ptr& /*message*/)
{
    data::AudioNoise audioNoise;
    audioNoise.filterType = rereadFilterType();

    Message::Ptr m = createMessage(audioNoise);
    toxConfig().send(m);
}

void VoiceFilters::command_AudioNoise(const Message::Ptr& message)
{
    data::AudioNoise audioNoise;
    readFromMessage(message, audioNoise);

    string value = "webrtc";
    if (audioNoise.filterType == data::AudioNoise::FilterType::None)
        value = "none";
    else if (audioNoise.filterType == data::AudioNoise::FilterType::RNNoise)
        value = "rnnoise";

    config::state().setValue("audio.streams.noise_filter", value);
    config::state().saveFile();

    _filterChanged = true;
}

data::AudioNoise::FilterType VoiceFilters::rereadFilterType()
{
    data::AudioNoise::FilterType filterType = data::AudioNoise::FilterType::WebRtc;

    string value = "webrtc";
    config::state().getValue("audio.streams.noise_filter", value);
    if (value == "none")
        filterType = data::AudioNoise::FilterType::None;
    else if (value == "rnnoise")
        filterType = data::AudioNoise::FilterType::RNNoise;

    return filterType;
}
