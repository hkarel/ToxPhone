#include "voice_frame.h"

#include "shared/safe_singleton.h"
#include "shared/spin_locker.h"
#include <atomic>

VoiceFrameInfo::VoiceFrameInfo(quint32 latency,
                               quint8  channels,
                               quint32 sampleSize,
                               quint32 sampleCount,
                               quint32 samplingRate,
                               quint32 bufferSize)
    : latency(latency),
      channels(channels),
      sampleSize(sampleSize),
      sampleCount(sampleCount),
      samplingRate(samplingRate),
      bufferSize(bufferSize)
{}


VoiceFrameInfo::Ptr getRecordFrameInfo(const VoiceFrameInfo* vfi, bool reset)
{
    static VoiceFrameInfo::Ptr voiceFrameInfo;
    static std::atomic_flag voiceFrameInfoLock {ATOMIC_FLAG_INIT};

    SpinLocker locker(voiceFrameInfoLock); (void) locker;
    if (reset)
        voiceFrameInfo = VoiceFrameInfo::Ptr();
    else if (vfi)
        voiceFrameInfo = VoiceFrameInfo::Ptr::create_ptr(*vfi);

    return voiceFrameInfo;
}

RingBuffer& recordRingBuff()
{
    return ::safe_singleton<RingBuffer, 0>();
}

RingBuffer& filterRingBuff()
{
    return ::safe_singleton<RingBuffer, 1>();
}

VoiceFrameInfo::Ptr getVoiceFrameInfo(const VoiceFrameInfo* vfi, bool reset)
{
    static VoiceFrameInfo::Ptr voiceFrameInfo;
    static std::atomic_flag voiceFrameInfoLock {ATOMIC_FLAG_INIT};

    SpinLocker locker(voiceFrameInfoLock); (void) locker;
    if (reset)
        voiceFrameInfo = VoiceFrameInfo::Ptr();
    else if (vfi)
        voiceFrameInfo = VoiceFrameInfo::Ptr::create_ptr(*vfi);

    return voiceFrameInfo;
}

RingBuffer& voiceRingBuff()
{
    return ::safe_singleton<RingBuffer, 2>();
}
