#include "voice_filters.h"
#include "voice_frame.h"
#include "toxphone_appl.h"
#include "common/functions.h"
#include "kernel/communication/commands.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/communication/message.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"

extern "C" {
#include "filter_audio.h"
#include "rnnoise.h"
}

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilter")

#define RNNOISE_FRAME_SIZE 480

void rnNoiseFilter(DenoiseState* ds, int16_t* data)
{
    float channel[RNNOISE_FRAME_SIZE];

    float* c = channel;
    int16_t* d = data;
    for (int i = 0; i < RNNOISE_FRAME_SIZE; ++i)
        *c++ = *d++;

    rnnoise_process_frame(ds, channel, channel);

    c = channel;
    d = data;
    for (int i = 0; i < RNNOISE_FRAME_SIZE; ++i)
        *d++ = *c++;
}

static quint32 getDataSize(const VoiceFrameInfo::Ptr& vfi)
{
    switch (vfi->latency)
    {
        case 40000:
            return vfi->bufferSize / 4;

        case 20000:
            return vfi->bufferSize / 2;

        case 10000:
            return vfi->bufferSize;

        case 5000:
            return vfi->bufferSize * 2;

        default:
            return vfi->bufferSize;
    }
}

VoiceFilters& voiceFilters()
{
    return ::safe_singleton<VoiceFilters, 0>();
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
        audioRecordLevel.max = maxLevel;
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
        ToxPhoneApplication::stop(1);
        return;
    }
    quint32 recordDataSize = getDataSize(recordFrameInfo);

    // Для двух каналов
    recordDataSize = RNNOISE_FRAME_SIZE * recordFrameInfo->sampleSize; // * 2;

    Filter_Audio* noiseFilter = new_filter_audio(recordFrameInfo->samplingRate);
    if (!noiseFilter)
    {
        log_error_m << "Failed call new_filter_audio() for noise filter";
        ToxPhoneApplication::stop(1);
        return;
    }
    enable_disable_filters(noiseFilter, 0, 1, 0, 0);

    char data[4000];
    //VoiceFrameInfo::Ptr voiceFrameInfo;
    //quint32 voiceDataSize;
    DenoiseState* denoiseState = rnnoise_create();

    while (true)
    {
        if (threadStop())
            break;

        if (recordRBuff_1().read(data, recordDataSize))
        {
            int16_t* pcm = (int16_t*)data;
//            rnNoiseFilter(denoiseState, pcm);

            if (filter_audio(noiseFilter, pcm, recordDataSize / sizeof(int16_t)) < 0)
            {
                log_error_m << "Failed call filter_audio() for noise filter";
            }

            /*** Отладка эхоподавления ***/
            //secondVoiceRB().write((char*)data, recordDataSize);
            /***/

//            if (echoFilter)
//                if (filter_audio(echoFilter, pcm, recordDataSize / sizeof(int16_t)) < 0)
//                {
//                    log_error_m << "Failed filter_audio() for echo filter";
//                }

            if (!recordRBuff_2().write((char*)data, recordDataSize))
            {
                log_error_m << "Failed write data to recordRBuff_2"
                            << ". Data size: " << recordDataSize;
            }

            // Уровень сигнала для микрофона отправляем именно из этой точки,
            // т.к. это позволит учитывать уровень усиления сигнала полученный
            // в функции filter_audio() при активном флаге gain.
            if (toxConfig().isActive())
            {
                pcm = (int16_t*)data;
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
    kill_filter_audio(noiseFilter);

//    if (echoFilter)
//        kill_filter_audio(echoFilter);

    rnnoise_destroy(denoiseState);

    _recordLevetMax = 0;
    sendRecordLevet(_recordLevetMax, 200);

    log_info_m << "Stopped";
}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
