/**
 *  Copyright (C) 2017 Pavel Karelin <hkarel@yandex.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#pragma once

#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/qt/thread/qthreadex.h"
#include <atomic>

class PhoneDiverter;

class PhoneRing : public QThreadEx
{
public:
    ~PhoneRing();
    bool mode() const {return _mode;}

    QString tone() const;
    void setTone(const QString&);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(PhoneRing)
    PhoneRing();

    void run() override;

private:
    volatile bool _mode = {false};
    QString _tone = {"DbDt"};
    mutable std::atomic_flag _toneLock = ATOMIC_FLAG_INIT;

    friend class PhoneDiverter;
};
