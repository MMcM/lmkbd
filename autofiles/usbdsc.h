/** Based on Microchip USB C18 Firmware Version 1.0 */

/*********************************************************************
 * Descriptor specific type definitions are defined in:
 * system\usb\usbdefs\usbdefs_std_dsc.h
 ********************************************************************/

#ifndef USBDSC_H
#define USBDSC_H

/** I N C L U D E S *************************************************/
#include "system\typedefs.h"
#include "autofiles\usbcfg.h"

#if defined(USB_USE_HID)
#include "system\usb\class\hid\hid.h"
#endif

#include "system\usb\usb.h"

/** D E F I N I T I O N S *******************************************/
#define CFG01 rom struct            \
{   USB_CFG_DSC     cd01;           \
    USB_INTF_DSC    i00a00;         \
    USB_HID_DSC     hid_i00a00;     \
    USB_EP_DSC      ep01i_i00a00;   \
} cfg01

/** E X T E R N S ***************************************************/
extern rom USB_DEV_DSC device_dsc;
extern CFG01;
extern rom const unsigned char *rom USB_CD_Ptr[];
extern rom const unsigned char *rom USB_SD_Ptr[];

extern rom struct{byte report[HID_RPT01_SIZE];} hid_rpt01;
extern rom pFunc ClassReqHandler[1];

#endif //USBDSC_H
