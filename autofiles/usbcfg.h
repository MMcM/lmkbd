/** Based on Microchip USB C18 Firmware Version 1.0 */

#ifndef USBCFG_H
#define USBCFG_H

/** D E F I N I T I O N S *******************************************/
#define EP0_BUFF_SIZE           8   // 8, 16, 32, or 64
#define MAX_NUM_INT             1   // For tracking Alternate Setting

/* Parameter definitions are defined in usbdrv.h */
#define MODE_PP                 _PPBM0
#define UCFG_VAL                _PUEN|_TRINT|_LS|MODE_PP

#define usb_bus_sense           1
#define self_power              0

/** D E V I C E  C L A S S  U S A G E *******************************/
#define USB_USE_HID

/*
 * MUID = Microchip USB Class ID
 * Used to identify which of the USB classes owns the current
 * session of control transfer over EP0
 */
#define MUID_NULL               0
#define MUID_USB9               1
#define MUID_HID                2
#define MUID_CDC                3

/** E N D P O I N T S  A L L O C A T I O N **************************/
/*
 * See usbmmap.c for an explanation of how the endpoint allocation works
 */

/* HID */
#define HID_INTF_ID             0x00
#define HID_UEP                 UEP1
#define HID_BD_OUT              ep1Bo
#define HID_INT_OUT_EP_SIZE     8
#define HID_BD_IN               ep1Bi
#define HID_INT_IN_EP_SIZE      8
#define HID_NUM_OF_DSC          1
#define HID_RPT01_SIZE          78
#define HID_FEATURE_SIZE        2

/* HID macros */
#define mUSBGetHIDDscAdr(ptr)               \
{                                           \
    if(usb_active_cfg == 1)                 \
        ptr = (rom byte*)&cfg01.hid_i00a00; \
}

#define mUSBGetHIDRptDscAdr(ptr)            \
{                                           \
    if(usb_active_cfg == 1)                 \
        ptr = (rom byte*)&hid_rpt01;        \
}

#define mUSBGetHIDRptDscSize(count)         \
{                                           \
    if(usb_active_cfg == 1)                 \
        count = sizeof(hid_rpt01);          \
}


#define MAX_EP_NUMBER           1           // UEP1

#endif //USBCFG_H
