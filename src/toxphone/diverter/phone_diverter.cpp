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


#include "phone_diverter.h"
#include "yealink_protocol.h"

#include "common/defines.h"
#include "shared/break_point.h"
#include "shared/utils.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"

#include <string>

#define telbox_idVendor  0x6993
#define telbox_idProduct 0xb001

//Interface
#define AUDIO_IN     0x01
#define AUDIO_OUT    0x02
#define CONTROL      0x03

#define SEARCH_IN_EVERY_BUS -1
#define PICK_ANY_DEVICE -1
#define MAX_CONTINUOUS_USB_ERROS 50

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "PhoneDiverter")

extern std::atomic_int usbContinuousBusErrorCounter;
extern std::atomic_bool pstn_and_usb_joined;
extern std::recursive_mutex usb_talk_lock;

PhoneDiverter& phoneDiverter()
{
    return ::safe_singleton<PhoneDiverter>();
}

PhoneDiverter::PhoneDiverter() : QThreadEx(0)
{
    static bool first {true};
    if (first)
    {
        qRegisterMetaType<PhoneDiverter::Handset>("PhoneDiverter::Handset");
        first = false;
    }
}

bool PhoneDiverter::init()
{
    usb_init();
    return true;
}

void PhoneDiverter::run()
{
    log_info_m << "Started";

    bool deviceDetachedEmitted = false;
    while (true)
    {
        CHECK_THREAD_STOP

        _deviceInitialized = false;
        if (claimDevice() && initDevice())
            _deviceInitialized = true;

        if (!_deviceInitialized)
        {
            releaseDevice();
            if (!deviceDetachedEmitted)
            {
                log_error_m << "Yealink device detached";
                deviceDetachedEmitted = true;
                emit detached();
            }
            int attempts = 0;
            while (attempts++ < 10)
            {
                sleep(1);
                if (threadStop())
                    break;
            }
            continue;
        }

        { //---
            QString usbBus, deviceName, deviceVersion, deviceSerial;
            getInfo(usbBus, deviceName, deviceVersion, deviceSerial);

            log_info_m << "Yealink device initialized"
                       << ": " << deviceName << " ["  << deviceVersion << "]"
                       << "; Serial: "  << deviceSerial;

            emit attached();
        }
        deviceDetachedEmitted = false;

        _handset = Handset::Off;
        Handset handset = Handset::Off;

        _dialTone = false;
        bool dialtone = false;

        int keyPress = 0;
        int lastKeyPress = 0;

        bool pstnRinging = false;
        int lastPstnRinging = false;

        bool handsetVal;
        _check_handset_keypress_pstnring(_deviceHandle,
                                         &handsetVal,
                                         &pstnRinging,
                                         &keyPress);
        _handset = (handsetVal) ? Handset::On : Handset::Off;
        lastKeyPress = keyPress;

        bool ringingMode = false;

        while (true)
        {
            CHECK_THREAD_STOP

            int counter = usbContinuousBusErrorCounter;
            if (counter > MAX_CONTINUOUS_USB_ERROS)
            {
                //usb_loop = OFF;
                //cmd_loop = OFF;
                //fifoWrite(yealinkDevice, "%s", FIFO_SAY_DEVICE_REMOVED);
                //continue;
                log_error_m << "Yealink device detached";
                deviceDetachedEmitted = true;
                emit detached();
                break;
            }

            _check_handset_keypress_pstnring(_deviceHandle,
                                             &handsetVal,
                                             &pstnRinging,
                                             &keyPress);
            _handset = (handsetVal) ? Handset::On : Handset::Off;

            /* check HANDSET and notify client */

            if (pstnRinging != lastPstnRinging)
            {
                lastPstnRinging = pstnRinging;
                if (pstnRinging)
                {
                    break_point
                    //fifoWrite(yealinkDevice, "%s", FIFO_SAY_PSTN_RING);
                    emit pstnRing();
                }
            }

            if (handset != _handset)
            {
                //last_handset_state_flag=handset_state_flag;
                handset = _handset;

                //if (_handsetState == ON)
                //    fifoWrite(yealinkDevice, "%s", FIFO_SAY_HANDSET_ON);
                //else
                //    fifoWrite(yealinkDevice, "%s", FIFO_SAY_HANDSET_OFF);

                if (_handset == Handset::On)
                    log_debug_m << "Change handset to ON";
                else
                    log_debug_m << "Change handset to OFF";

                emit this->handset(_handset);

                //if (ringing_state == ON)
                if (_phoneRing.isRunning())
                {
                    //ringing_state = OFF;
                    //ringing_mode = OFF;
                    _phoneRing.stop();
                    //_ringing.setMode(OFF);
                    ringingMode = false;

                    if (USB_ERR == usbb2k_ring(_deviceHandle, USB_OFF))
                        log_error_m << "Failed OFF ringing";
                }
            }

            if (_handset == Handset::Off)
            {
                /* set RING */
                //if (ringing_state == ON || ringing_mode_flag == ON)
                if (_phoneRing.isRunning() || ringingMode)
                {
                    //if (ringing_mode_flag != ringing_mode)
                    if (ringingMode != _phoneRing.mode())
                    {
                        //ringing_mode_flag = ringing_mode;
                        ringingMode = _phoneRing.mode();
                        if (ringingMode)
                            usbb2k_ring(_deviceHandle, USB_ON);
                        else
                            usbb2k_ring(_deviceHandle, USB_OFF);
                    }
                }
            }

            //if (handset_state == ON || pstn_and_usb_joined == ON)
            if (_handset == Handset::On || pstn_and_usb_joined == USB_ON)
            {
                /* check KEYPRESS */
                if (keyPress != USB_UNDEF)
                {
                    if (lastKeyPress != keyPress)
                    {
                        lastKeyPress = keyPress;
                        int keyCode = usbb2k_get_key(_deviceHandle, keyPress);
                        if (keyCode != USB_ERR)
                        {
                            //fifoWrite(yealinkDevice, FIFO_SAY_KEY_NUM, keyCode);
                            emit key(keyCode);
                        }
                    }
                }
            }

            //added by simon dible 08-01-07
            //dial tone functions
            // modified by dmitry romashko 2007-09-06
            /* set DIALTONE */
            if (_dialTone != dialtone)
            {
                if (_dialTone)
                {
                    if (USB_ON == usbb2k_tone(_deviceHandle, USB_ON))
                        dialtone = true;
                }
                else
                {
                    if (USB_OFF == usbb2k_tone(_deviceHandle, USB_OFF))
                        dialtone = false;
                }
            }
            usleep(25000);

        } // while (true)

        /* cleanup */
        hangup_pstn(_deviceHandle);
        usbb2k_switch_mode(_deviceHandle, PSTN_MODE);

        /* that does not work and lock the daemon */
        /* by the time we get here, the "client" socket is already closed */
        /*   sprintf(msg,"USB DISCONNECT\n"); */
        /*   syslog(LOG_INFO,"MMMM 111"); */
        /*   if (write(client,msg,strlen(msg)) == -1){ */
        /*     syslog(LOG_ERR,"error writing socket %s", msg); */
        /*   } */

        /* Stop Ringing LOOP */
        //ringing_state = OFF;
        _phoneRing.stop();
        /* Stop ring */

        usbb2k_ring(_deviceHandle, USB_OFF);

//        //syslog(LOG_INFO,"USB server shutdown");
//        log_info << "USB server shutdown";
//        //this function is not mandatory to be called and valgrind says it leaks. so we don't call it
//        // pthread_exit(NULL);
//        return 0;

        releaseDevice();

    } // while (true)

    log_info_m << "Stopped";
}

bool PhoneDiverter::claimDevice()
{
    struct usb_bus *bus;
    struct usb_device *device;

    usb_find_busses();
    usb_find_devices();

    for (bus = usb_busses; bus; bus = bus->next)
    {
        CHECK_THREAD_STOP

        for (device = bus->devices; device; device = device->next)
        {
            CHECK_THREAD_STOP

            if (device->descriptor.idVendor == telbox_idVendor
                && device->descriptor.idProduct == telbox_idProduct)
            {
                sscanf(bus->dirname, "%d", &(_usbBusNumber));
                _usbDeviceNumber = device->devnum;

                log_info_m << "Yealink device found on bus "
                           << utl::formatMessage("%03d/%03d", _usbBusNumber, _usbDeviceNumber);

                int numTries = 3;
                while (numTries-- > 0)
                {
                    CHECK_THREAD_STOP

                    _deviceHandle = usb_open(device);
                    if (!_deviceHandle)
                    {
                        log_error_m << "USB interface not opened";
                        sleep(3);
                        continue;
                    }

                    /**
                      Чтобы получить возможность работать с функцией usb_claim_interface()
                      в режиме обычного пользователя необходимо выполнить
                      следующие шаги:
                      1) Создать файл /lib/udev/rules.d/99-skypemate-b2k-b3g.rules
                      2) Записать в файл 99-yealink.rules следующую строку:
                           SUBSYSTEMS=="usb", ATTRS{idVendor}=="6993", ATTRS{idProduct}=="b001", ACTION=="add", GROUP="yealink", MODE="0664"

                         Здесь yealink - это группа в которую входит текущий
                         пользователь. Так же можно указать дефолтную группу
                         текущего пользователя.
                         Значения idVendor и idProduct можно узнать командой:
                           sudo lsusb -v

                      3) Перечитать файлы конфигурации udev:
                           sudo udevadm control --reload-rules
                      4) Переподключить устройство.
                    */
                    int err = usb_claim_interface(_deviceHandle, CONTROL);
                    if (err != 0)
                    {
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
                        usb_detach_kernel_driver_np(_deviceHandle, CONTROL);
#endif
                        err = usb_claim_interface(_deviceHandle, CONTROL);
                        if (err != 0)
                        {
                            log_error_m << "USB claim interface failed. Need create UDEV rule to access this device";
                            usb_close(_deviceHandle);
                            _deviceHandle = 0;
                            sleep(3);
                            continue;
                        }
                    }
                    return true;

                } // while (numTries-- > 0)

                log_error_m << "The number of claim attempts of the Yealink device is exceeded";
                if (_deviceHandle)
                {
                    usb_close(_deviceHandle);
                    _deviceHandle = 0;
                }
                return false;
            }
        }
    }

    log_debug2_m << "Device not found";
    _deviceHandle = 0;
    return false;
}

bool PhoneDiverter::initDevice()
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

    char data[URB_LENGTH];

    /* USB B3G USES ALL THIS STEPS, SO WHY WONT I ? */
    if (usb_talk(_deviceHandle, urb_cmd(URB_DEVICE_MODEL), data))
    {
        usleep(25000);
        if (usb_talk(_deviceHandle, urb_cmd(URB_DEVICE_MODEL), data))
        {
            usleep(25000);
            if (usb_talk(_deviceHandle, urb_cmd(URB_DEVICE_MODEL), data))
            {
                usleep(25000);
                if (usb_talk(_deviceHandle, urb_cmd(URB_DEVICE_MODEL), data))
                {
                    usleep(25000);
                    log_error_m << "Yealink device not initialized";
                    return false;
                }
            }
        }
    }

    _deviceVersion = (data[4] << 8) + data[5];

    const char *deviceModelName;
    YealinkModel deviceModel = yld_decode_model(_deviceVersion, &deviceModelName);
    switch (deviceModel)
    {
        case YealinkModel::b3g:
            _check_handset_keypress_pstnring = usbb3g_check_handset_keypress_pstnring;
            usb_talk(_deviceHandle, urb_cmd(URB_DETATCH_USB_AND_PSTN), data);
            break;

        case YealinkModel::b2k:
            _check_handset_keypress_pstnring = usbb2k_check_handset_keypress_pstnring;
            break;

        default:
            log_error << "Unsuported Yealink device. I only support B2K and B3G";
            return false;
    }

    usb_talk(_deviceHandle, urb_cmd(URB_TEST), data);

    if (usb_talk(_deviceHandle, urb_cmd(URB_DEVICE_SERIAL_NUMBER), data))
    {
        log_error_m << "Yealink device not initialized";
        return false;
    }

    memset(_deviceSerialNumber, 0, SERIAL_NUMBER_SIZE + 1);
    memcpy(_deviceSerialNumber, data + 4, SERIAL_NUMBER_SIZE);

    usb_talk(_deviceHandle, urb_cmd(URB_HANGUP_PSTN_LINE), data);
    usb_talk(_deviceHandle, urb_cmd(URB_RING_OFF), data);
    usbb2k_tone(_deviceHandle, USB_OFF);
    usb_talk(_deviceHandle, urb_cmd(URB_TEST3), data);

    if (!switchToPstn())
        return false;

    return true;
}

void PhoneDiverter::releaseDevice()
{
    if (_deviceHandle)
    {
        usb_release_interface(_deviceHandle, CONTROL);
        // usb_reset(client_arg.dev_h); // if I reset I don't have to close
        usb_close(_deviceHandle);
        _deviceHandle = 0;
        _deviceInitialized = false;
    }
}

void PhoneDiverter::getInfo(QString& usbBus,
                            QString& deviceName,
                            QString& deviceVersion,
                            QString& deviceSerial)
{
    std::string s = utl::formatMessage("%03d/%03d", _usbBusNumber, _usbDeviceNumber);
    usbBus = QString::fromStdString(s);

    const char *name;
    yld_decode_model(_deviceVersion, &name);
    deviceName = name;

    s = utl::formatMessage("0x%x", _deviceVersion);
    deviceVersion = QString::fromStdString(s);

    s = utl::formatMessage("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                           _deviceSerialNumber[0], _deviceSerialNumber[1],
                           _deviceSerialNumber[2], _deviceSerialNumber[3],
                           _deviceSerialNumber[4], _deviceSerialNumber[5],
                           _deviceSerialNumber[6], _deviceSerialNumber[7],
                           _deviceSerialNumber[8], _deviceSerialNumber[9],
                           _deviceSerialNumber[10]);
    deviceSerial = QString::fromStdString(s);
}

bool PhoneDiverter::setMode(Mode mode)
{
    return (mode == Mode::Usb)
           ? switchToUsb()
           : switchToPstn();
}

bool PhoneDiverter::switchToUsb()
{
    int res = usbb2k_switch_mode(_deviceHandle, USB_MODE);
    if (USB_MODE != res)
    {
        log_error_m << "Failed switch to USB mode";
        return false;
    }
    log_debug2_m << "Switch to USB mode";
    _mode = Mode::Usb;
    return true;
}

bool PhoneDiverter::switchToPstn()
{
    int res = usbb2k_switch_mode(_deviceHandle, PSTN_MODE);
    if (PSTN_MODE != res)
    {
        log_error_m << "Failed switch to PSTN mode";
        return false;
    }
    log_debug2_m << "Switch to PSTN mode";
    _mode = Mode::Pstn;
    return true;
}

bool PhoneDiverter::isRinging() const
{
    return _phoneRing.isRunning();
}

void PhoneDiverter::setRing(bool val)
{
    if (val
        && !_phoneRing.isRunning()
        && _handset == Handset::Off)
    {
        _phoneRing.start();
    }
    else
    {
        _phoneRing.stop();
        //_ringing.setMode(OFF);
    }
}

QString PhoneDiverter::ringTone() const
{
    return _phoneRing.tone();
}

void PhoneDiverter::setRingTone(const QString& val)
{
    _phoneRing.setTone(val);
}

bool PhoneDiverter::pickupPstn()
{
    int res = pickup_pstn(_deviceHandle);
    if (USB_TALK_OK != res)
    {
        log_error_m << "Failed pickup PSTN";
        return false;
    }
    log_debug2_m << "Pickup PSTN";
    return true;
}

bool PhoneDiverter::hangupPstn()
{
    int res = hangup_pstn(_deviceHandle);
    if (USB_TALK_OK != res)
    {
        log_error_m << "Failed hangup PSTN";
        return false;
    }
    log_debug2_m << "Hangup PSTN";
    return true;
}

bool PhoneDiverter::joinUsbAndPstn()
{
    int res = b3g_join_usb_and_pstn(_deviceHandle);
    if (USB_TALK_OK != res)
    {
        log_error_m << "Failed join USB and PSTN";
        return false;
    }
    log_debug2_m << "Join USB and PSTN";
    return true;
}

bool PhoneDiverter::detachUsbAndPstn()
{
    int res = b3g_detach_usb_and_pstn(_deviceHandle);
    if (USB_TALK_OK != res)
    {
        log_error_m << "Failed detach USB and PSTN";
        return false;
    }
    log_debug2_m << "Detach USB and PSTN";
    return true;
}

#undef COMMAND_ANSWER

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
