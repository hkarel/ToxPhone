#pragma once

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/safe_singleton.h"
#include "shared/qt/thread/qthreadex.h"

#include <QtCore>

class VoiceFilters : public QThreadEx
{
public:
    ~VoiceFilters() = default;
    void wake();
    void sendRecordLevet(quint32 maxLevel, quint32 time);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(VoiceFilters)
    VoiceFilters() = default;
    void run() override final;

    // Параметр используется для подготовки данных об индикации уровня сигнала
    // микрофона в конфигураторе
    quint32 _recordLevetMax = {0};
    steady_timer _recordLevetTimer;

    QMutex _threadLock;
    QWaitCondition _threadCond;

    template<typename T, int> friend T& ::safe_singleton();
};
VoiceFilters& voiceFilters();
