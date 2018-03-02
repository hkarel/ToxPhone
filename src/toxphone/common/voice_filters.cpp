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
}

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")

void VoiceFilters::bufferUpdated()
{
    //std::lock_guard<mutex> locker(_threadLock); (void) locker;
    //_threadCond.notify_all();

    QMutexLocker locker(&_threadLock); (void) locker;
    _threadCond.wakeAll();
}

void VoiceFilters::sendRecordLevet(quint32 maxLevel, quint32 time)
{
    data::AudioRecordLevel audioRecordLevel;
    audioRecordLevel.max = maxLevel;
    audioRecordLevel.time = time;

    Message::Ptr m = createMessage(audioRecordLevel, Message::Type::Event);
    m->setPriority(Message::Priority::High);
    tcp::listener().send(m);
}

void VoiceFilters::run()
{
    log_info_m << "Started";

    VoiceFrameInfo::Ptr voiceFrameInfo = recordVoiceFrameInfo();
    if (voiceFrameInfo.empty())
    {
        log_error_m << "Failed get VoiceFrameInfo for record";
        ToxPhoneApplication::stop(1);
        return;
    }

    filterVoiceRBuff().init(5 * voiceFrameInfo->bufferSize);
    log_debug_m  << "Initialization a filter ring buffer"
                 << "; size: " << filterVoiceRBuff().size();

    Filter_Audio* filters = new_filter_audio(voiceFrameInfo->samplingRate);
    if (!filters)
    {
        log_error_m << "Failed call new_filter_audio()";
        ToxPhoneApplication::stop(1);
        return;
    }
    enable_disable_filters(filters, 0, 1, 0, 0);

    char data[4000];
    quint32 dataSize;
    switch (voiceFrameInfo->latency)
    {
        case 40000:
            dataSize = voiceFrameInfo->bufferSize / 4;
            break;

        case 20000:
            dataSize = voiceFrameInfo->bufferSize / 2;
            break;

        case 10000:
            dataSize = voiceFrameInfo->bufferSize;
            break;

        case 5000:
            dataSize = voiceFrameInfo->bufferSize * 2;
            break;

        default:
            dataSize = voiceFrameInfo->bufferSize;
    }

    while (true)
    {
        if (threadStop())
            break;

        while (recordVoiceRBuff().read(data, dataSize))
        {
            if (threadStop())
                break;

            int16_t* pcm = (int16_t*)data;
            if (filter_audio(filters, pcm, dataSize / sizeof(int16_t)) < 0)
            {
                log_error_m << "Failed filter_audio()";
            }

            if (!filterVoiceRBuff().write((char*)data, dataSize))
            {
                log_error_m << "Failed write data to filterVoiceRBuff"
                            << ". Data size: " << dataSize;
            }

            // Уровень сигнала для микрофона отправляем именно из этой точки,
            // т.к. это позволит учитывать уровень усиления сигнала полученный
            // в функции filter_audio() при активном флаге gain.
            if (configConnected())
            {
                pcm = (int16_t*)data;
                for (size_t i = 0; i < (dataSize / sizeof(int16_t)); ++i)
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
        if (!threadStop())
        {
            //std::unique_lock<mutex> locker(_threadLock); (void) locker;
            //_threadCond.wait_for(locker, std::chrono::milliseconds(10));

            QMutexLocker locker(&_threadLock); (void) locker;
            _threadCond.wait(&_threadLock, 10);
        }
    }
    kill_filter_audio(filters);

    log_debug_m << "Filter ring buffer available: "
                << filterVoiceRBuff().available();
    filterVoiceRBuff().reset();

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
