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

VoiceFilters& voiceFilters()
{
    return ::safe_singleton<VoiceFilters, 0>();
}

void VoiceFilters::bufferUpdated()
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

    auto getDataSize = [](VoiceFrameInfo::Ptr vfi) -> quint32
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
    };

    VoiceFrameInfo::Ptr recordFrameInfo = getRecordFrameInfo();
    if (recordFrameInfo.empty())
    {
        log_error_m << "Failed get VoiceFrameInfo for record";
        ToxPhoneApplication::stop(1);
        return;
    }
    quint32 recordDataSize = getDataSize(recordFrameInfo);

    Filter_Audio* noiseFilter = new_filter_audio(recordFrameInfo->samplingRate);
    if (!noiseFilter)
    {
        log_error_m << "Failed call new_filter_audio() for noise filter";
        ToxPhoneApplication::stop(1);
        return;
    }
    enable_disable_filters(noiseFilter, 0, 1, 0, 0);

    char data[4000];
    VoiceFrameInfo::Ptr voiceFrameInfo;
    quint32 voiceDataSize;
    Filter_Audio* echoFilter = {0};

    while (true)
    {
        if (threadStop())
            break;

        if (voiceFrameInfo)
        {
            if (firstVoiceRB().read(data, voiceDataSize))
            {
//                int16_t* pcm = (int16_t*)data;
//                if (filter_audio(noiseFilter, pcm, voiceDataSize / sizeof(int16_t)) < 0)
//                {
//                    log_error_m << "Failed call filter_audio() for noise filter";
//                }
//                if (echoFilter)
//                    if (pass_audio_output(echoFilter, pcm, voiceDataSize / sizeof(int16_t)) < 0)
//                    {
//                        log_error_m << "Failed call pass_audio_output() for echo filter";
//                    }

                if (!secondVoiceRB().write((char*)data, voiceDataSize))
                {
                    log_error_m << "Failed write data to secondVoiceRB"
                                << ". Data size: " << voiceDataSize;
                }
            }
        }
        else
        {
            if (voiceFrameInfo = getVoiceFrameInfo())
            {
                voiceDataSize = getDataSize(voiceFrameInfo);

                echoFilter = new_filter_audio(recordFrameInfo->samplingRate);
                if (!echoFilter)
                {
                    log_error_m << "Failed call new_filter_audio() for echo filter";
                    ToxPhoneApplication::stop(1);
                    return;
                }
                enable_disable_filters(echoFilter, 1, 0, 0, 0);
                //static const int kMaxTrustedDelayMs = 120;

                double msInSndCardBuf =
                    double(voiceFrameInfo->sampleCount) / voiceFrameInfo->samplingRate;
                msInSndCardBuf *= 1000;
                //msInSndCardBuf *= voiceFrameInfo->channels;
                msInSndCardBuf += voiceFrameInfo->latency / 1000;
                msInSndCardBuf = 20;
                set_echo_delay_ms(echoFilter, int(msInSndCardBuf));
            }
        }

        if (firstRecordRB().read(data, recordDataSize))
        {
            int16_t* pcm = (int16_t*)data;

            /*** Отладка эхоподавления ***/
            //secondVoiceRB().write((char*)data, recordDataSize);
            /***/

//            if (echoFilter)
//                if (filter_audio(echoFilter, pcm, recordDataSize / sizeof(int16_t)) < 0)
//                {
//                    log_error_m << "Failed filter_audio() for echo filter";
//                }

            if (!secondRecordRB().write((char*)data, recordDataSize))
            {
                log_error_m << "Failed write data to secondRecordRB"
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
            && firstVoiceRB().available() < voiceDataSize
            && firstRecordRB().available() < recordDataSize)
        {
            QMutexLocker locker(&_threadLock); (void) locker;
            _threadCond.wait(&_threadLock, 10);
        }
    }
    kill_filter_audio(noiseFilter);
    if (echoFilter)
        kill_filter_audio(echoFilter);

    _recordLevetMax = 0;
    sendRecordLevet(_recordLevetMax, 200);

    log_info_m << "Stopped";
}
//---------------------------------------------

//NoiseFilter& noiseFilter()
//{
//    return ::safe_singleton<NoiseFilter, 0>();
//}

//void NoiseFilter::bufferUpdated()
//{
//    QMutexLocker locker(&_threadLock); (void) locker;
//    _threadCond.wakeAll();
//}

//void NoiseFilter::run()
//{
//    log_info_m << "Started";

//    VoiceFrameInfo::Ptr voiceFrameInfo = getVoiceFrameInfo();
//    if (voiceFrameInfo.empty())
//    {
//        log_error_m << "Failed get VoiceFrameInfo for record";
//        ToxPhoneApplication::stop(1);
//        return;
//    }

//    Filter_Audio* filters = new_filter_audio(voiceFrameInfo->samplingRate);
//    if (!filters)
//    {
//        log_error_m << "Failed call new_filter_audio()";
//        ToxPhoneApplication::stop(1);
//        return;
//    }
//    enable_disable_filters(filters, 0, 1, 0, 0);

//    char data[4000];
//    quint32 dataSize;
//    switch (voiceFrameInfo->latency)
//    {
//        case 40000:
//            dataSize = voiceFrameInfo->bufferSize / 4;
//            break;

//        case 20000:
//            dataSize = voiceFrameInfo->bufferSize / 2;
//            break;

//        case 10000:
//            dataSize = voiceFrameInfo->bufferSize;
//            break;

//        case 5000:
//            dataSize = voiceFrameInfo->bufferSize * 2;
//            break;

//        default:
//            dataSize = voiceFrameInfo->bufferSize;
//    }

//    while (true)
//    {
//        if (threadStop())
//            break;

//        while (noiseRingBuff().read(data, dataSize))
//        {
//            if (threadStop())
//                break;

//            int16_t* pcm = (int16_t*)data;
//            if (filter_audio(filters, pcm, dataSize / sizeof(int16_t)) < 0)
//            {
//                log_error_m << "Failed filter_audio()";
//            }

//            if (!voiceRingBuff().write((char*)data, dataSize))
//            {
//                log_error_m << "Failed write data to voiceRingBuff"
//                            << ". Data size: " << dataSize;
//            }
//        }
//        if (!threadStop())
//        {
//            QMutexLocker locker(&_threadLock); (void) locker;
//            _threadCond.wait(&_threadLock, 10);
//        }
//    }
//    kill_filter_audio(filters);

//    log_info_m << "Stopped";
//}

//EchoFilter& echoFilter()
//{
//    return ::safe_singleton<EchoFilter, 0>();
//}

//void EchoFilter::bufferUpdated()
//{
//    QMutexLocker locker(&_threadLock); (void) locker;
//    _threadCond.wakeAll();
//}

//void EchoFilter::sendRecordLevet(quint32 maxLevel, quint32 time)
//{
//    if (toxConfig().isActive())
//    {
//        data::AudioRecordLevel audioRecordLevel;
//        audioRecordLevel.max = maxLevel;
//        audioRecordLevel.time = time;

//        Message::Ptr m = createMessage(audioRecordLevel);
//        m->setPriority(Message::Priority::High);
//        toxConfig().send(m);
//    }
//}

//void EchoFilter::run()
//{
//    log_info_m << "Started";

//    VoiceFrameInfo::Ptr voiceFrameInfo = getRecordFrameInfo();
//    if (voiceFrameInfo.empty())
//    {
//        log_error_m << "Failed get VoiceFrameInfo for record";
//        ToxPhoneApplication::stop(1);
//        return;
//    }

////    Filter_Audio* filters = new_filter_audio(voiceFrameInfo->samplingRate);
////    if (!filters)
////    {
////        log_error_m << "Failed call new_filter_audio()";
////        ToxPhoneApplication::stop(1);
////        return;
////    }
////    enable_disable_filters(filters, 0, 1, 0, 0);

//    char data[4000];
//    quint32 dataSize;
//    switch (voiceFrameInfo->latency)
//    {
//        case 40000:
//            dataSize = voiceFrameInfo->bufferSize / 4;
//            break;

//        case 20000:
//            dataSize = voiceFrameInfo->bufferSize / 2;
//            break;

//        case 10000:
//            dataSize = voiceFrameInfo->bufferSize;
//            break;

//        case 5000:
//            dataSize = voiceFrameInfo->bufferSize * 2;
//            break;

//        default:
//            dataSize = voiceFrameInfo->bufferSize;
//    }

//    while (true)
//    {
//        if (threadStop())
//            break;

//        while (recordRingBuff().read(data, dataSize))
//        {
//            if (threadStop())
//                break;

//            int16_t* pcm = (int16_t*)data;
////            if (filter_audio(filters, pcm, dataSize / sizeof(int16_t)) < 0)
////            {
////                log_error_m << "Failed filter_audio()";
////            }

//            if (!echoRingBuff().write((char*)data, dataSize))
//            {
//                log_error_m << "Failed write data to voiceRingBuff"
//                            << ". Data size: " << dataSize;
//            }

//            // Уровень сигнала для микрофона отправляем именно из этой точки,
//            // т.к. это позволит учитывать уровень усиления сигнала полученный
//            // в функции filter_audio() при активном флаге gain.
//            if (toxConfig().isActive())
//            {
//                pcm = (int16_t*)data;
//                for (size_t i = 0; i < (dataSize / sizeof(int16_t)); ++i)
//                {
//                    if (*pcm > 0)
//                        if (_recordLevetMax < quint32(*pcm))
//                            _recordLevetMax = *pcm;
//                    ++pcm;
//                }
//                if (_recordLevetTimer.elapsed() > 200)
//                {
//                    sendRecordLevet(_recordLevetMax, 200);
//                    _recordLevetMax = 0;
//                    _recordLevetTimer.reset();
//                }
//            }
//        }
//        if (!threadStop())
//        {
//            QMutexLocker locker(&_threadLock); (void) locker;
//            _threadCond.wait(&_threadLock, 10);
//        }
//    }
//    //kill_filter_audio(filters);

//    _recordLevetMax = 0;
//    sendRecordLevet(_recordLevetMax, 200);

//    log_info_m << "Stopped";
//}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
