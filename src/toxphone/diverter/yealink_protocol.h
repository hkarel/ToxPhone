/**
 *  Copyright (C) 2017 Pavel Karelin <hkarel@yandex.ru>
 *  Copyright (C) 2007 Marcos Diez <marcos AT unitron.com.br>
 *  Copyright (C) 2007 PGT-Linux.org http://www.pgt-linux.org
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

#include <usb.h>
#include <atomic>

//#define USBB2K_API_VERSION 2.09
//#define USBB2K_API_VERSION VersionNumber(3, 0, 0)

#define USB_MODE  1
#define PSTN_MODE 2

#define KEY_0    0x00
#define KEY_1    0x01
#define KEY_2    0x02
#define KEY_3    0x03
#define KEY_4    0x04
#define KEY_5    0x05
#define KEY_6    0x06
#define KEY_7    0x07
#define KEY_8    0x08
#define KEY_9    0x09
#define KEY_STAR 0x0b
#define KEY_POUN 0x0c

#define TEL_ERR ((char) 0xfd) /* vendor code for busy, stalled, or error? */

#define USB_TALK_OK 0
#define USB_TALK_INVALID_MSG  -1
#define USB_TALK_INVALID_READ -2

#define MSG_MAXL 1000
#define URB_LENGTH	16

#define USB_ON	   1
#define USB_OFF	   0
#define USB_UNDEF -1
#define USB_ERR   -1


//Commande URB
enum URB
{
    URB_RING_ON, URB_RING_OFF,
    URB_USB_ON, URB_USB_OFF,
    URB_LEDS_BOTH_OFF, URB_LEDS_ONLY_USB_ON, URB_LEDS_ONLY_PSTN_ON, URB_LEDS_BOTH_ON,
    URB_TONE_ON, URB_TONE_OFF,
    URB_HANDSET,
    URB_KEYPRESS,
    URB_KEYCODE,
    URB_HOOKPRESS,
    URB_DEVICE_MODEL,
    URB_DEVICE_MODEL_ALTERNATIVE,
    URB_DEVICE_SERIAL_NUMBER,

    URB_PICKUP_PSTN_LINE,
    URB_HANGUP_PSTN_LINE,
    URB_JOIN_USB_AND_PSTN,
    URB_DETATCH_USB_AND_PSTN,

    URB_TEST,
    // I found out what test2 is, so it's not here anymore
    URB_TEST3,
    URB_TEST4,
    URB_TEST6,
    URB_TEST7,
    URB_TEST8,
    URB_TEST9,
    URB_TEST10,

    MAX_URB_VALUE
};
unsigned char* urb_cmd(URB urb);


int usb_talk(usb_dev_handle *dev_h, unsigned char *inputData, char *outputData);

/** List of the supported Yealink models */
enum YealinkModel {unknown, p1k, p2k, p3k, p4k, b1k, b2k, b3g, t1k, v1k, p1kh};

//int printInfo(char *msg, int msgSize, YealinkDevice *yealinkDevice);
YealinkModel yld_decode_model(unsigned int deviceVersion, const char **name);
//void createYealinkDeviceName(YealinkDevice *yealinkDevice);
//int yealinkInit(YealinkDevice *yealinkDevice);

void bigtest(usb_dev_handle *dev_h);
int b3g_join_usb_and_pstn(usb_dev_handle *dev_h);
int b3g_detach_usb_and_pstn(usb_dev_handle *dev_h);
int hangup_pstn(usb_dev_handle *dev_h);
int pickup_pstn(usb_dev_handle *dev_h);
int usbb3g_check_handset_keypress_pstnring(usb_dev_handle *dev_h,
                                           bool *handset,
                                           bool *pstn_ring,
                                           int  *keypress);
int usbb2k_check_handset_keypress_pstnring(usb_dev_handle *dev_h,
                                           bool *handset,
                                           bool *pstn_ring,
                                           int  *keypress);
int usbb2k_ring(usb_dev_handle *dev_h, int status);
int usbb2k_tone(usb_dev_handle *dev_h, int status);
int usbb2k_switch_mode(usb_dev_handle *dev_h, int mode);
int usbb2k_get_key(usb_dev_handle *dev_h, int keynum);
void api_debug(usb_dev_handle *dev_h, int urbID);


