/**
 *  Copyright (C) 2017 Pavel Karelin <hkarel@yandex.ru>
 *  Copyright (C) 2007 Marcos Diez marcos AT unitron.com.br
 *  Copyright (C) 2007 Thomas Reitmayr <treitmayr@yahoo.com> (from libYealink)
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

#include "yealink_protocol.h"
#include "shared/logger/logger.h"

#include <atomic>
#include <mutex>

#define USB_MSG_IN  USB_TYPE_CLASS
#define USB_MSG_OUT USB_TYPE_CLASS | 0x01
#define TIME_OUT    1600

#define log_error_m   alog::logger().error_f  (alog_line_location, "YealinkProto")
#define log_warn_m    alog::logger().warn_f   (alog_line_location, "YealinkProto")
#define log_info_m    alog::logger().info_f   (alog_line_location, "YealinkProto")
#define log_verbose_m alog::logger().verbose_f(alog_line_location, "YealinkProto")
#define log_debug_m   alog::logger().debug_f  (alog_line_location, "YealinkProto")
#define log_debug2_m  alog::logger().debug2_f (alog_line_location, "YealinkProto")

std::atomic_int usbContinuousBusErrorCounter {0};
std::atomic_bool pstn_and_usb_joined {false}; // b3g only
std::recursive_mutex usb_talk_lock;

int usb_talk(usb_dev_handle *dev_h, unsigned char *inputData, char *outputData)
{
    if ( !dev_h)
        return USB_TALK_INVALID_MSG;

    // usleep(25000);

    int err = usb_control_msg(dev_h, USB_MSG_OUT, 0x9, 0x200, 0x03,
                              (char *)inputData, URB_LENGTH, TIME_OUT);
    if (err < 0) {
        ++usbContinuousBusErrorCounter;
    } else {
        usbContinuousBusErrorCounter = 0;
    }
    if (err != URB_LENGTH)
        return USB_TALK_INVALID_MSG;

    if (outputData == NULL)
        return USB_TALK_OK;

    err = usb_interrupt_read(dev_h, 0x81, (char *) outputData, URB_LENGTH, TIME_OUT);
    if (err < 0) {
        ++usbContinuousBusErrorCounter;
    } else {
        usbContinuousBusErrorCounter = 0;
    }
    if (err != URB_LENGTH)
        return USB_TALK_INVALID_READ;

    return USB_TALK_OK;
}

YealinkModel yld_decode_model(unsigned int deviceVersion, const char **name)
{
    YealinkModel model = YealinkModel::unknown;
	*name = "UNKNOWN";
	//  if (hid_msg_size == sizeof(struct yld_packet_g2))
	//  {
	//    model = yld_model_p1kh;
	//  }
	//  else
	//	if (hid_msg_size == sizeof(struct yld_packet_g1)) {
	if (deviceVersion >= 0x0100) {
		if (deviceVersion < 0x0200) {
            model = YealinkModel::p1k;
			*name = "P1K";
		} else if (deviceVersion < 0x0230) {
            model = YealinkModel::p3k;
			*name = "P3K";
		} else if (deviceVersion < 0x0300) {
            model = YealinkModel::p4k;
			*name = "P4K";
		} else if (deviceVersion < 0x0500) {
            model = YealinkModel::v1k;
			*name = "V1K";
		} else if (deviceVersion < 0x0520) {
            model = YealinkModel::b1k;
			*name = "B1K";
		} else if (deviceVersion < 0x0540) {
            model = YealinkModel::b2k;
			*name = "B2Kv1"; // without caller ID
		} else if (deviceVersion < 0x0570) {
            model = YealinkModel::b3g;
			*name = "B3G";
		} else if (deviceVersion < 0x0590) {
            model = YealinkModel::b2k;
			*name = "B2Kv2";
		} else if (deviceVersion < 0x0600) {
            model = YealinkModel::unknown;
		} else if (deviceVersion < 0x0700) {
            model = YealinkModel::t1k;
			*name = "T1K";
		}
		//	}
	}
	return model;
}

void bigtest(usb_dev_handle *dev_h)
{
	return;
	/*   char data[URB_LENGTH]; */
	/* 	char key, key2; */

	/* 	globalExclusive=1; */
	/* 	usleep(250000); */
    /* 	Usb_talk( dev_h , urb_cmd(URB_TEST7 ] , data ); */
	/* 	usleep(250000); // 0x81 */
    /* 	Usb_talk( dev_h , urb_cmd(URB_HANDSET ] , data ); */
	/* 	usleep(250000); // 0x81 */
    /* 	Usb_talk( dev_h , urb_cmd(URB_KEYPRESS ] , data ); */
	/* 	usleep(250000); */
    /* 	Usb_talk( dev_h , urb_cmd(URB_PICKUP_PSTN_PART_1 ] , data ); */
	/* 	usleep(250000); */
    /* 	Usb_talk( dev_h , urb_cmd(URB_PICKUP_PSTN_PART_2 ] , data ); */
	/* 	usleep(250000); */
	/* 	globalExclusive=0; */
	/* 	return; */

	/* 	usleep(250000); */

    /* 	Usb_talk( dev_h , urb_cmd(URB_HANGUP_PSTN_PART_1 ] , data ); */
	/* 	usleep(250000); */
    /* 	Usb_talk( dev_h , urb_cmd(URB_TEST4 ] , data ); */
	/* 	usleep(250000); */

    /* 	Usb_talk( dev_h , urb_cmd(URB_PICKUP_PSTN_PART_1 ] , data ); */
	/* 	usleep(250000); */
	/* 	key=data[4]; */

    /* 	Usb_talk( dev_h , urb_cmd(URB_PICKUP_PSTN_PART_2 ] , data ); */
	/* 	usleep(250000); */
	/* 	key2=data[4]; */

	/* 	// now we dial */
	/* 	// * it does not work... damn..... B3G does not DIAL, it just receive dialed keys... *\/*/
/* 	memcpy(data, urb_cmd(URB_B3G_KEY_WAS_DIALED], URB_LENGTH); */
/* 	data[4] = key; // globalKeyPress; */
/* 	data[5] = 0x03; /\* I wanna dial 3 *\/*/
/* 	data[15]-= (data[4] + data[5] ); */
/* 	Usb_talk( dev_h , (unsigned char *) data , data ); */
/* 	usleep(250000); */

/* 	// now we get off hook */
/* 	Usb_talk( dev_h , urb_cmd(URB_HANGUP_PSTN_PART_1 ] , data ); */
/* 	usleep(250000); */

/* 	Usb_talk( dev_h , urb_cmd(URB_HANGUP_PSTN_PART_2 ] , data ); */
/* 	usleep(250000); */

/* 	globalExclusive=0; */
}

int b3g_join_usb_and_pstn(usb_dev_handle *dev_h)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    pstn_and_usb_joined = true;
    int res = 0;
    res |= usb_talk(dev_h, urb_cmd(URB_JOIN_USB_AND_PSTN), data);
    res |= usb_talk(dev_h, urb_cmd(URB_LEDS_BOTH_ON), data);
    return res;
}

int b3g_detach_usb_and_pstn(usb_dev_handle *dev_h)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    pstn_and_usb_joined = false;
    int res = 0;
    res |= usb_talk(dev_h, urb_cmd(URB_DETATCH_USB_AND_PSTN), data);
    res |= usb_talk(dev_h, urb_cmd(URB_LEDS_ONLY_USB_ON), data);
    return res;
}

int hangup_pstn(usb_dev_handle *dev_h)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    pstn_and_usb_joined = false;
    int res = 0;
    res |= usb_talk(dev_h, urb_cmd(URB_DETATCH_USB_AND_PSTN), data);
    res |= usb_talk(dev_h, urb_cmd(URB_HANGUP_PSTN_LINE), data);
    return res;
}

int pickup_pstn(usb_dev_handle *dev_h)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    return usb_talk(dev_h, urb_cmd(URB_PICKUP_PSTN_LINE), data);
}

int usbb3g_check_handset_keypress_pstnring(usb_dev_handle *dev_h,
                                           bool *handset,
                                           bool *pstn_ring,
                                           int  *keypress)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	// marcos 2007-10-20
	// USBB3G works different than USB-B2K.
	// Nevertheless, this function also works with both of my B2K 
	// since I do not own the other yealink devices, I will just leave it as is
	// for compatibility reasons

	int err;
	char data[URB_LENGTH];

    err = usb_talk(dev_h, urb_cmd(URB_KEYPRESS), data); // 0x80
	if (err != USB_TALK_OK || data[0] == TEL_ERR)
        return USB_ERR;

    *handset = bool(data[6] == 0x01);
    *pstn_ring = bool(data[5] == 0x01);
    *keypress = data[4];

	// printf("%s: handSet=%d keyPress=%d pstn_ring=%d\n" , 	__FUNCTION__ , *handset , *keypress , *pstn_ring );
	return 0;
}

int usbb2k_check_handset_keypress_pstnring(usb_dev_handle *dev_h,
                                           bool* handset,
                                           bool *pstn_ring,
                                           int  *keypress)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	int err;
	char data[URB_LENGTH];
	// printf("INIT%s: handSet=%d keyPress=%d pstn_ring=%d\n" ,		__FUNCTION__ , *handset , *keypress , *pstn_ring );
	/*
	 data[4] from URB_HANDSET
	 0x00 OFF
	 0x01 OFF_WITH_PSTN_RING
	 0x02 ON
	 0x03 ON_WITH_PSTN_RING
	 since USB-B3G does NOT send ON_WITH_PSTN_RING, we will ignore it here.
	 */
    err = usb_talk(dev_h, urb_cmd(URB_HANDSET), data); // 0x81
	if (err != USB_TALK_OK || data[0] == TEL_ERR)
        return USB_ERR;
    *handset = bool(data[4] & 0x02);
    *pstn_ring = bool(data[4] == 0x01);

	//char blah=data[4];
    err = usb_talk(dev_h, urb_cmd(URB_KEYPRESS), data);
	if (err != USB_TALK_OK || data[0] == TEL_ERR)
        return USB_ERR;

    *keypress = data[4];

	// printf("%s: handSet=%d keyPress=%d pstn_ring=%d blah=%d\n" , 				__FUNCTION__ , *handset , *keypress , *pstn_ring , blah );
	return 0;
}

int usbb2k_ring(usb_dev_handle *dev_h, int status)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    if (status == USB_ON) {
        if (usb_talk(dev_h, urb_cmd(URB_RING_ON), data))
            return USB_ERR;
		if (data[0] != TEL_ERR)
            return USB_ON;
	} else {
        if (usb_talk(dev_h, urb_cmd(URB_RING_OFF), data))
            return USB_ERR;
		if (data[0] != TEL_ERR)
            return USB_OFF;
	}
    return USB_ERR;
}

int usbb2k_tone(usb_dev_handle *dev_h, int status)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
    if (status == USB_ON) {
        if (usb_talk(dev_h, urb_cmd(URB_TONE_ON), data))
            return USB_ERR;
		if (data[0] != TEL_ERR) {
            return USB_ON;
		}
	} else {
        if (usb_talk(dev_h, urb_cmd(URB_TONE_OFF), data))
            return USB_ERR;
		if (data[0] != TEL_ERR) {
            return USB_OFF;
		}
	}
    return USB_ERR;
}

int usbb2k_switch_mode(usb_dev_handle *dev_h, int mode)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

    int res = 0;
	char data[URB_LENGTH];
    if (mode == USB_MODE) {
        res |= usb_talk(dev_h, urb_cmd(URB_LEDS_BOTH_OFF), data);
        res |= usb_talk(dev_h, urb_cmd(URB_LEDS_ONLY_USB_ON), data);
        res |= usb_talk(dev_h, urb_cmd(URB_USB_ON), data);

        return (res == 0) ? USB_MODE : USB_ERR;
	}
    if (mode == PSTN_MODE) {
        res |= usb_talk(dev_h, urb_cmd(URB_LEDS_BOTH_OFF), data);
        res |= usb_talk(dev_h, urb_cmd(URB_LEDS_ONLY_PSTN_ON), data);
        res |= usb_talk(dev_h, urb_cmd(URB_USB_OFF), data);

        return (res == 0) ? PSTN_MODE : USB_ERR;
	}
    return USB_ERR;
}

int usbb2k_get_key(usb_dev_handle *dev_h, int keynum)
{
    std::lock_guard<std::recursive_mutex> locker(usb_talk_lock); (void) locker;

	char data[URB_LENGTH];
	keynum--;
    if (keynum < 0)
        keynum = 0x1F;
    memcpy(data, urb_cmd(URB_KEYCODE), URB_LENGTH);
    data[3] = keynum;
    data[15] = 0x7e - keynum;

    if (usb_talk(dev_h, (unsigned char *) data, data) != USB_TALK_OK)
        return USB_ERR;

    //if (data[0]!=urb_cmd(URB_KEYCODE][0]) return -2;
	//Frank Bennett
	//syslog(LOG_INFO,"get_key: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x",
	//	   data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15]);
	// syslog(LOG_INFO,"key event number/key %.2x/%.2x",data[3],data[4]);
	// dmitry romashko - check response before returning result
	if (data[0] != TEL_ERR)
		return data[4];

    return USB_ERR;
}

//void api_debug(usb_dev_handle *dev_h, int urbID)
//{
//    char data[URB_LENGTH];

//    if (urbID < MAX_URB_VALUE) {
//        USB_TALK(dev_h, urb_cmd(urbID), data);

//        /* int err=  	  Usb_talk(dev_h, urb_cmd(urbID], data); */
//        /*     syslog(LOG_INFO, */
//        /* 	   "apiDebug(%2d/0x%.2x)=%d: %.2x %.2x %.2x %.2x [ %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x ] %.2x", */
//        /* 	   urbID , urb_cmd(urbID][0]  , err , */
//        /* 	   outputData[0] , */
//        /* 	   outputData[1],outputData[2],outputData[3],outputData[4],outputData[5], */
//        /* 	   outputData[6],outputData[7],outputData[8],outputData[9],outputData[10], */
//        /* 	   outputData[11],outputData[12],outputData[13],outputData[14],outputData[15] */

//        /* 	   ); */
//    } else {
//        //syslog(LOG_INFO, "apiDebug(%2d): Error... URB_ID > %d" , urbID , MAX_URB_VALUE);
//        log_error << "apiDebug(" << urbID << "): Error... URB_ID > " << MAX_URB_VALUE;
//    }
//}

unsigned char* urb_cmd(URB urb)
{
    static unsigned char URB_CMD[][URB_LENGTH]= {

            // RING ON
            { 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xFd },
            // RING OFF
            { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xFe },

            // USB / PSTN SWITCH CMD
            // URB_USB_ON 	    // PSTN OFF
            { 0x0E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xF1 },
            // URB_USB_OFF      // PSTN on
            { 0x0E, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xf0 },

            // URB_LEDS_BOTH_OFF
            { 0x05, 0x02, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xFb },
            // URB_LEDS_ONLY_USB_ON
            { 0x05, 0x02, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xFb },
            // URB_LEDS_ONLY_PSTN_ON
            { 0x05, 0x02, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xFb },
            // URB_LEDS_BOTH_ON
            { 0x05, 0x02, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xfb },

            // TONE / RING CMD
            // TONE_ON
            { 0x09, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xF5 },
            // TONE_OFF
            { 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xF6 },

            // URB_HANDSET   // b2k ONLY
            // used to be HANDSET QUERY [CMD_DEVICE_IDENT]
            // data[0] == 0x02 -> B2K or B3G
            // data[0] == 0x0c -> P1K ??

            // marcos 2007-10-20
            // it does apply for B2K but does NOT apply to B3G
            // On B3G and B2K, one needs simply to send KEY_PRESS.
            // data[4] == 0x0 --> phone on hook
            // data[4] == 0x1 --> phone on hook with an incoming PSTN call)
            // data[4] == 0x2 --> phone off hook (i.e you are holding it next to your ear :)
            // data[4] == 0x3 --> phone off hook  with an incoming PSTN call
            { 0x8d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x72 },

            // KEY PRESS     [URB_KEYPRESS] // b2k and b3g, but b3g returns more results
            //  data[0]      on return returns the key event sequence, if it changes there's a new key pressed.
            //  { 0x80,0x01,0x00,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62 },
            // if handset ON/OFF is  DATA[06] (00 == off, 01 == on )
            // on B3G, it returns even more data

            // data[4] == keypress                                 // b3g and b2k
            // data[5] == pstn_ring 01 -> on , 00 -> off           // b3g only
            // data[6] == handset   01 -> on hook , 00 -> off hook // b3g only
            { 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x7f },

            // KEY CODE      [URB_KEYCODE]
            // data[0]      on return returns the scancode
            { 0x81, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x7e },

            // deprecated ?
            // HOOK PRESS    [URB_HOOKPRESS]
            // on return returns the hook status 0xff on hook / 0xef off hook
            { 0x8b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x7e },

            // URB_DEVICE_MODEL // initializes the device
            // returns the version at data[4] and data[5]
            { 0x87, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x77 },
            // URB_DEVICE_MODEL_ALTERNATIVE
            // the same as above. I actually do no know the difference
            { 0x87, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x77 },

            // DEVICE_SERIAL_NUMBER // (USED TO BE URB_INIT )
            // returns the serial number. Usefull for machines with more than one Yealink
            // tested only on B2G and B3K
            { 0x8E, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x68 },

            // URB_PICKUP_PSTN_LINE
            // both on b2k and b3g, it makes PSTN line "busy" while the user
            // is talking thought the USB
            // on b3g, this must be called before USB_JOIN_USB_AND_PSTN
            { 0x07, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xf7 },

            // URB_HANGUP_PSTN_LINE
            // the oposite of the above
            { 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xf8 },
            //  070100000000000000000000000000f8


            // USB_JOIN_USB_AND_PSTN	// b3g only
            // joins the USB Audio device with the PSTN line.
            // that allows call divert, answearing machine and so on
            // must be called after URB_PICKUP_PSTN_LINE
            { 0x14, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xea },

            // USB_DETATCH_USB_AND_PSTN    // b3g only
            // detaches USB and PSTN. Should be used whenever the diverted phonecall is done
            // only on B3G, makes the audio from/to the PSTN comes thought
            { 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xeb },
            // 140100000000000000000000000000eb


            // TEST
            { 0x82, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x7d },
            //  8201000000000000000000000000007d


            // TEST3
            { 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xde },
            //  210100000000000000000000000000de

            // TEST4
            { 0x15, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xea },
            // 150100000000000000000000000000ea


            // TEST6
            { 0x15, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0xbc, 0xe6, 0xf7, 0xe7,
                    0xfa, 0xdb, 0x3f, 0x6f, 0xe3 },

            // TEST7
            { 0x8E, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x67 },

            // TEST8

            { 0x8f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x70 },

            /* I must find out what THIS means: ( it came from a b2k )
             (i guess it is caller ID, but I don't have any to test ) */
            // TEST9
            { 0x18, 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xe1 },
            // TEST10
            { 0x18, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0xe6 },

    /*
     class interface: 180100000600000000000000000000e1
     bulk transfer:   1801000000000011000000000001d6ff
     class interface: 180100000100000000000000000000e6
     bulk transfer:   180100000200000000000000000000e5
     class interface: 1801000000000011000000000001d6ff
     bulk transfer:   180100000100000000000000000000e6
     class interface: 1801000000000011000000000001d6ff
     bulk transfer:
     class interface:

     */
    };

    return URB_CMD[urb];
}

/*
 char T_DECROCHE2[16]={ 0x04,0x01,0x00,0x0b,0xa6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4a };
 char T_DECROCHE3[16]={ 0x05,0x02,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfa };
 char T_DECROCHE4[16]={ 0x0e,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf1 };
 char T_TEST[16]={ 0x07,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf7 };
 */
