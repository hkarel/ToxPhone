#pragma once

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/qt/thread/qthreadex.h"

#include <QtCore>
//#include <mutex>
//#include <condition_variable>

class VoiceFilters : public QThreadEx
{
public:
    VoiceFilters() = default;
    ~VoiceFilters() = default;

    void bufferUpdated();
    void sendRecordLevet(quint32 maxLevel, quint32 time);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(VoiceFilters)
    void run() override;

    // Параметр используется для подготовки данных об индикации уровня сигнала
    // микрофона в конфигураторе
    quint32 _recordLevetMax = {0};
    steady_timer _recordLevetTimer;

    //std::mutex _threadLock;
    //std::condition_variable _threadCond;
    QMutex _threadLock;
    QWaitCondition _threadCond;
};
