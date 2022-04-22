/**
 *  Copyright (C) 2017 Pavel Karelin <hkarel@yandex.ru>
 *  Copyright (C) 2007 Marcos Diez <marcos AT unitron.com.br>
 *  Copyright (C) 2005 PGT-Linux.org http://www.pgt-linux.org
 *  Author: vandorpe Olivier <vandorpeo@pgt-linux.org>
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

#include "phone_ring.h"
#include "shared/defmac.h"
#include "shared/safe_singleton.h"
#include "shared/qt/qthreadex.h"
#include <usb.h>

#define SERIAL_NUMBER_SIZE 11

class PhoneDiverter : public QThreadEx
{
public:
    enum class Mode {Usb, Pstn};
    enum class Handset {On, Off};

    bool init();

    Mode mode() const {return _mode;}
    bool setMode(Mode);

    Handset handset() const {return _handset;}
    bool isAttached() const {return _deviceInitialized;}
    bool isRinging() const;

    bool dialTone() const {return _dialTone;}
    void setDialTone(int val) {_dialTone = val;}

    void startDialTone() {setDialTone(true);}
    void stopDialTone()  {setDialTone(false);}


    QString ringTone() const;
    void setRingTone(const QString&);

    void startRing() {setRing(true);}
    void stopRing()  {setRing(false);}

    void getInfo(QString& usbBus,
                 QString& deviceName,
                 QString& deviceVersion,
                 QString& deviceSerial);
    bool switchToUsb();
    bool switchToPstn();
    bool pickupPstn();
    bool hangupPstn();
    bool joinUsbAndPstn();
    bool detachUsbAndPstn();

signals:
    void attached();
    void detached();
    void pstnRing();
    void key(int);
    void handset(PhoneDiverter::Handset);

private:
    Q_OBJECT
    DISABLE_DEFAULT_COPY(PhoneDiverter)
    PhoneDiverter();

    void run() override;

    void setRing(bool);
    bool claimDevice();
    bool initDevice();
    void releaseDevice();

private:
    volatile Mode _mode = {Mode::Pstn};

    //--- Old YealinkDevice structure ---
    int _usbBusNumber = {0};
    int _usbDeviceNumber = {0};

    volatile bool   _deviceInitialized = {false};
    usb_dev_handle* _deviceHandle = {0};  // for libUSB
    int             _deviceVersion = {0};
    unsigned char   _deviceSerialNumber[SERIAL_NUMBER_SIZE + 1];
    //---

    volatile Handset _handset = {Handset::Off};
    volatile bool _dialTone = {false};

    PhoneRing _phoneRing;

    int (*_check_handset_keypress_pstnring)(usb_dev_handle *dev_h,
                                            bool* handset,
                                            bool* keypress,
                                            int*  pstn_ring);

    template<typename T, int> friend T& ::safe_singleton();
};

PhoneDiverter& phoneDiverter();
