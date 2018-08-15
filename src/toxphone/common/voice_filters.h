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

    void bufferUpdated();
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


///**
//  Фильтр для подавления шумов
//*/
//class NoiseFilter : public QThreadEx
//{
//public:
//    ~NoiseFilter() = default;

//    void bufferUpdated();

//private:
//    Q_OBJECT
//    DISABLE_DEFAULT_COPY(NoiseFilter)
//    NoiseFilter() = default;
//    void run() override final;

//    QMutex _threadLock;
//    QWaitCondition _threadCond;

//    template<typename T, int> friend T& ::safe_singleton();
//};
//NoiseFilter& noiseFilter();

///**
//  Фильтр для подавления эха
//*/
//class EchoFilter : public QThreadEx
//{
//public:
//    ~EchoFilter() = default;

//    void bufferUpdated();
//    void sendRecordLevet(quint32 maxLevel, quint32 time);

//private:
//    Q_OBJECT
//    DISABLE_DEFAULT_COPY(EchoFilter)
//    EchoFilter() = default;
//    void run() override final;

//    // Параметр используется для подготовки данных об индикации уровня сигнала
//    // микрофона в конфигураторе
//    quint32 _recordLevetMax = {0};
//    steady_timer _recordLevetTimer;

//    QMutex _threadLock;
//    QWaitCondition _threadCond;

//    template<typename T, int> friend T& ::safe_singleton();
//};
//EchoFilter& echoFilter();
