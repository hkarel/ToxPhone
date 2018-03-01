#include "voice_filters.h"
#include "voice_frame.h"
#include "toxphone_appl.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"

extern "C" {
#include "filter_audio.h"
}

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "VoiceFilters")

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

    Filter_Audio* filters = new_filter_audio(voiceFrameInfo->samplingRate);
    if (!filters)
    {
        log_error_m << "Failed call new_filter_audio()";
        ToxPhoneApplication::stop(1);
        return;
    }
    enable_disable_filters(filters, 0, 1, 0, 0);

    char data[4000];
    quint32 dataSize = voiceFrameInfo->bufferSize / 2;

    while (true)
    {
        if (threadStop())
            break;

        while (filterVoiceRBuff().read(data, dataSize))
        {
            if (threadStop())
                break;

            int16_t* pcm = (int16_t*)data;
            if (filter_audio(filters, pcm, dataSize / sizeof(int16_t)) < 0)
            {
                log_error_m << "Failed filter_audio()";
            }

            if (!recordVoiceRBuff().write((char*)data, dataSize))
            {
                log_error_m << "Failed write data to recordVoiceRBuff"
                            << ". Data size: " << dataSize;
            }
        }
        if (!threadStop())
            msleep(3);
    }
    kill_filter_audio(filters);

    log_info_m << "Stopped";
}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
