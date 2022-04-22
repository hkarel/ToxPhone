#pragma once

#include "shared/defmac.h"
#include "shared/steady_timer.h"
#include "shared/safe_singleton.h"
#include "shared/qt/qthreadex.h"

#include "pproto/func_invoker.h"

#include "commands/commands.h"
#include "commands/error.h"

#include <QtCore>
#include <atomic>

using namespace pproto;
using namespace pproto::transport;

class VoiceFilters : public QThreadEx
{
public:
    ~VoiceFilters() = default;
    void wake();
    void sendRecordLevet(quint32 maxLevel, quint32 time);

public slots:
    void message(const pproto::Message::Ptr&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(VoiceFilters)
    VoiceFilters();
    void run() override;

    void command_IncomingConfigConnection(const Message::Ptr&);
    void command_AudioNoise(const Message::Ptr&);

    data::AudioNoise::FilterType rereadFilterType();

private:
    // Параметр используется для подготовки данных об индикации уровня сигнала
    // микрофона в конфигураторе
    quint32 _recordLevetMax = {0};
    steady_timer _recordLevetTimer;

    QMutex _threadLock;
    QWaitCondition _threadCond;

    volatile bool _filterChanged;
    FunctionInvoker _funcInvoker;

    template<typename T, int> friend T& ::safe_singleton();
};
VoiceFilters& voiceFilters();
