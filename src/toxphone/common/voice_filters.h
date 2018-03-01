#pragma once

#include "shared/defmac.h"
#include "shared/qt/thread/qthreadex.h"

class VoiceFilters : public QThreadEx
{
public:
    VoiceFilters() = default;
    ~VoiceFilters() = default;

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(VoiceFilters)
    void run() override;
};
