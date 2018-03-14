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

#include "phone_ring.h"

#include "common/defines.h"
#include "shared/break_point.h"
#include "shared/spin_locker.h"
#include "shared/logger/logger.h"

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneRing")

PhoneRing::PhoneRing() : QThreadEx(0)
{
}

QString PhoneRing::tone() const
{
    SpinLocker locker(_toneLock); (void) locker;
    return _tone;
}

void PhoneRing::setTone(const QString& val)
{
    SpinLocker locker(_toneLock); (void) locker;
    _tone = val;
}

void PhoneRing::run()
{
    log_debug_m << "Started";

    QString tone;
    { //Block for SpinLocker
        SpinLocker locker(_toneLock); (void) locker;
        tone = _tone;
    }

    int timeout = 30;
    while (timeout-- > 0)
    {
        CHECK_THREAD_STOP

        // --- added by ant@loadtrax.com 30/10/2005 ---
        // ringing_tone[] contains, eg, DbDt,  where letter
        // A/a=0.1s, Y/y=2.5s, UPPER=ring, lower=no ring
        // Z/z is 'stop here', ie, only do one ring loop
        // Try AaAaAx :-)
        //int ringcyclelength = strlen(ringing_tone);
        //if (ringcyclelength < 1) {
        //    // the user didn't set a valid tone, so apply a default. DbDt is
        //    // the UK ringing tone
        //    sprintf(ringing_tone, "DbDt");
        //    ringcyclelength = strlen(ringing_tone);
        //}
        if (tone.length() < 1)
        {
            // the user didn't set a valid tone, so apply a default. DbDt is
            // the UK ringing tone
            tone = "DbDt";
        }
        for (int i = 0; i < tone.length(); ++i)
        {
            if (threadStop())
                break;

            char thissegment = tone[i].toLatin1();
            _mode = (thissegment & 0x20) ? false : true;
            if ((thissegment & 0x1f) == 26)
            {
                // user specified 'z' or 'Z' so drop out here
                timeout = 0;
                //_mode = false;
                break;
            }
            else
                usleep((thissegment & 0x1f) * 100000);
        }
    }
    _mode = false;
    log_debug_m << "Stopped";
}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
