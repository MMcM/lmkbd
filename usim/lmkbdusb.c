
#include "lmkbdusb.h"
#include <usb.h>
#include <string.h>
#include <stdio.h>

#define HID_REPORT_GET 0x01
#define HID_REPORT_SET 0x09

#define HID_RT_INPUT 0x01
#define HID_RT_OUTPUT 0x02
#define HID_RT_FEATURE 0x03

#define USB_TIMEOUT 10000

// Indexed by HUT page 7 usage id, give the original key codes for
// each keyboards.
static unsigned char KeyMappings[256][4] = {
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 00 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 01 ErrorRollOver */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 02 POSTF */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 03 ErrorUndefined */
  { 0047, 0123, 0120, 0072 },   /* 04 A */
  { 0071, 0114, 0154, 0032 },   /* 05 B */
  { 0067, 0164, 0152, 0031 },   /* 06 C */
  { 0051, 0163, 0122, 0073 },   /* 07 D */
  { 0026, 0162, 0073, 0121 },   /* 08 E */
  { 0052, 0013, 0123, 0060 },   /* 09 F */
  { 0053, 0113, 0124, 0074 },   /* 0A G */
  { 0054, 0053, 0125, 0061 },   /* 0B H */
  { 0033, 0032, 0100, 0110 },   /* 0C I */
  { 0055, 0153, 0126, 0075 },   /* 0D J */
  { 0056, 0033, 0127, 0062 },   /* 0E K */
  { 0057, 0073, 0130, 0076 },   /* 0F L */
  { 0073, 0154, 0156, 0033 },   /* 10 M */
  { 0072, 0054, 0155, 0046 },   /* 11 N */
  { 0034, 0072, 0101, 0124 },   /* 12 O */
  { 0035, 0172, 0102, 0111 },   /* 13 P */
  { 0024, 0122, 0071, 0120 },   /* 14 Q */
  { 0027, 0012, 0074, 0106 },   /* 15 R */
  { 0050, 0063, 0121, 0057 },   /* 16 S */
  { 0030, 0112, 0075, 0122 },   /* 17 T */
  { 0032, 0152, 0077, 0123 },   /* 18 U */
  { 0070, 0014, 0153, 0045 },   /* 19 V */
  { 0025, 0062, 0072, 0105 },   /* 1A W */
  { 0066, 0064, 0151, 0044 },   /* 1B X */
  { 0031, 0052, 0076, 0107 },   /* 1C Y */
  { 0065, 0124, 0150, 0030 },   /* 1D Z */
  { 0002, 0121, 0044, 0145 },   /* 1E 1 */
  { 0003, 0061, 0045, 0133 },   /* 1F 2 */
  { 0004, 0161, 0046, 0146 },   /* 20 3 */
  { 0005, 0011, 0047, 0134 },   /* 21 4 */
  { 0006, 0111, 0050, 0147 },   /* 22 5 */
  { 0007, 0051, 0051, 0135 },   /* 23 6 */
  { 0010, 0151, 0052, 0150 },   /* 24 7 */
  { 0011, 0031, 0053, 0136 },   /* 25 8 */
  { 0012, 0071, 0054, 0151 },   /* 26 9 */
  { 0013, 0171, 0055, 0137 },   /* 27 0 */
  { 0062, 0136, 0133, 0064 },   /* 28 return (enter) */
  { 0023, 0143, 0043, 0160 },   /* 29 escape | alt mode */
  { 0046, 0023, 0117, 0056 },   /* 2A delete (backspace) | rubout */
  { 0022, 0022, 0070, 0117 },   /* 2B tab */
  { 0077, 0134, 0173, 0021 },   /* 2C space */
  { 0xFF, 0131, 0056, 0152 },   /* 2D - _ */
  { 0014, 0126, 0057, 0140 },   /* 2E = + */
  { 0036, 0xFF, 0xFF, 0xFF },   /* 2F [ { */
  { 0037, 0xFF, 0xFF, 0xFF },   /* 30 ] } */
  { 0040, 0037, 0106, 0141 },   /* 31 \ | */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 32 Non-US # ~ */
  { 0060, 0173, 0131, 0063 },   /* 33 ; : */
  { 0xFF, 0133, 0132, 0077 },   /* 34 ' " */
  { 0xFF, 0077, 0xFF, 0153 },   /* 35 ` ~ */
  { 0074, 0034, 0157, 0047 },   /* 36 , < */
  { 0075, 0074, 0160, 0034 },   /* 37 . > */
  { 0076, 0174, 0161, 0050 },   /* 38 / ? */
  { 0xFF, 0xFF, 0003, 0xFF },   /* 39 Caps Lock */
  { 0xFF, 0xFF, 0024, 0xFF },   /* 3A F1 */
  { 0xFF, 0xFF, 0025, 0xFF },   /* 3B F2 */
  { 0xFF, 0xFF, 0026, 0xFF },   /* 3C F3 */
  { 0xFF, 0xFF, 0027, 0xFF },   /* 3D F4 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 3E F5 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 3F F6 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 40 F7 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 41 F8 */
  { 0xFF, 0101, 0021, 0162 },   /* 42 F9 | I | square */
  { 0xFF, 0001, 0022, 0163 },   /* 43 F10 | II | circle */
  { 0xFF, 0102, 0023, 0164 },   /* 44 F11 | III | triangle */
  { 0xFF, 0002, 0xFF, 0xFF },   /* 45 F12 | IV */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 46 PrintScreen */
  { 0xFF, 0xFF, 0006, 0xFF },   /* 47 Scroll Lock */
  { 0xFF, 0170, 0xFF, 0xFF },   /* 48 pause | stop output */
  { 0017, 0160, 0xFF, 0126 },   /* 49 insert | bs | overstrike | insert */
  { 0xFF, 0xFF, 0136, 0xFF },   /* 4A Home */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 4B PageUp */
  { 0xFF, 0157, 0xFF, 0xFF },   /* 4C delete forward | delete */
  { 0xFF, 0156, 0020, 0024 },   /* 4D end */
  { 0045, 0xFF, 0xFF, 0010 },   /* 4E page down | vt | scroll */
  { 0xFF, 0017, 0137, 0xFF },   /* 4F right arrow  | hand right */
  { 0xFF, 0117, 0135, 0xFF },   /* 50 left arrow | hand left */
  { 0xFF, 0176, 0165, 0xFF },   /* 51 down arrow | down thumb */
  { 0xFF, 0106, 0107, 0xFF },   /* 52 up arrow | up thumb */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 53 Keypad Num Lock */
  { 0041, 0xFF, 0xFF, 0xFF },   /* 54 keypad / | / infinity */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 55 Keypad * */
  { 0042, 0xFF, 0113, 0xFF },   /* 56 keypad - | circle minus / delta */
  { 0043, 0xFF, 0063, 0xFF },   /* 57 keypad + | circle plus / del */
  { 0063, 0036, 0177, 0100 },   /* 58 keypad enter | line */
  { 0xFF, 0xFF, 0166, 0xFF },   /* 59 Keypad 1 */
  { 0xFF, 0xFF, 0167, 0xFF },   /* 5A Keypad 2 */
  { 0xFF, 0xFF, 0170, 0xFF },   /* 5B Keypad 3 */
  { 0xFF, 0xFF, 0140, 0xFF },   /* 5C Keypad 4 */
  { 0xFF, 0xFF, 0141, 0xFF },   /* 5D Keypad 5 */
  { 0xFF, 0xFF, 0142, 0xFF },   /* 5E Keypad 6 */
  { 0xFF, 0xFF, 0110, 0xFF },   /* 5F Keypad 7 */
  { 0xFF, 0xFF, 0111, 0xFF },   /* 60 Keypad 8 */
  { 0xFF, 0xFF, 0112, 0xFF },   /* 61 Keypad 9 */
  { 0xFF, 0xFF, 0175, 0xFF },   /* 62 Keypad 0 */
  { 0xFF, 0xFF, 0176, 0xFF },   /* 63 Keypad .*/
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 64 Keypad Non-US \ | */
  { 0xFF, 0141, 0010, 0015 },   /* 65 application | system | select */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 66 Power */
  { 0xFF, 0xFF, 0062, 0xFF },   /* 67 Keypad = */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 68 F13 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 69 F14 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6A F15 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6B F16 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6C F17 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6D F18 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6E F19 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 6F F20 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 70 F21 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 71 F22 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 72 F23 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 73 F24 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 74 Execute */
  { 0xFF, 0116, 0001, 0052 },   /* 75 help */
  { 0xFF, 0042, 0011, 0071 },   /* 76 menu | network */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 77 Select */
  { 0xFF, 0067, 0114, 0037 },   /* 78 stop | abort */
  { 0xFF, 0100, 0xFF, 0104 },   /* 79 again | macro | function */
  { 0xFF, 0xFF, 0017, 0xFF },   /* 7A Undo */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 7B Cut */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 7C Copy */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 7D Paste */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 7E Find */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 7F Mute */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 80 Volume Up */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 81 Volume Down */
  { 0xFF, 0125, 0xFF, 0003 },   /* 82 locking caps lock */
  { 0xFF, 0015, 0xFF, 0xFF },   /* 83 locking num lock | alt lock */
  { 0xFF, 0003, 0xFF, 0011 },   /* 84 locking scroll lock | mode lock */
  { 0xFF, 0xFF, 0143, 0xFF },   /* 85 Keypad Comma */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 86 Keypad Equal Sign */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 87 International1 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 88 International2 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 89 International3 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8A International4 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8B International5 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8C International6 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8D International7 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8E International8 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 8F International9 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 90 LANG1 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 91 LANG2 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 92 LANG3 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 93 LANG4 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 94 LANG5 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 95 LANG6 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 96 LANG7 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 97 LANG8 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 98 LANG9 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* 99 Alternate Erase */
  { 0xFF, 0046, 0012, 0xFF },   /* 9A sysreq / attention | status */
  { 0000, 0167, 0066, 0166 },   /* 9B cancel | break | suspend */
  { 0021, 0110, 0016, 0165 },   /* 9C clear | clear input */
  { 0064, 0xFF, 0xFF, 0xFF },   /* 9D prior | backnext */
  { 0xFF, 0047, 0041, 0167 },   /* 9E return | resume */
  { 0044, 0xFF, 0xFF, 0113 },   /* 9F keypad separator | form | page */
  { 0020, 0107, 0xFF, 0xFF },   /* A0 out | call  */
  { 0xFF, 0040, 0013, 0002 },   /* A1 oper | terminal | local */
  { 0xFF, 0050, 0015, 0161 },   /* A2 clear / again | clear screen | refresh */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* A3 CrSel/Props */
  { 0xFF, 0120, 0xFF, 0065 },   /* A4 exsel | quote | complete */
  { 0xFF, 0xFF, 0134, 0xFF },   /* A5 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* A6 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* A7 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* A8 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* A9 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AA Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AB Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AC Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AD Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AE Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* AF Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B0 Keypad 00 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B1 Keypad 000 */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B2 Thousands Separator */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B3 Decimal Separator */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B4 Currency Unit */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* B5 Currency Sub-unit */
  { 0xFF, 0132, 0103, 0125 },   /* B6 keypad ( | ( */
  { 0xFF, 0137, 0104, 0112 },   /* B7 keypad ) | ) */
  { 0xFF, 0166, 0060, 0xFF },   /* B8 keypad { | { */
  { 0xFF, 0146, 0061, 0xFF },   /* B9 keypad } | } */
  { 0xFF, 0xFF, 0065, 0xFF },   /* BA Keypad Tab */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* BB Keypad Backspace */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* BC Keypad A */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* BD Keypad B */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* BE Keypad C */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* BF Keypad D */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C0 Keypad E */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C1 Keypad F */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C2 Keypad XOR */
  { 0016, 0xFF, 0xFF, 0xFF },   /* C3 keypad ^ | ^ ~ */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C4 Keypad % */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C5 Keypad < */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C6 Keypad > */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C7 Keypad & */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* C8 Keypad && */
  { 0xFF, 0xFF, 0xFF, 0154 },   /* C9 keypad | | | */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* CA Keypad || */
  { 0061, 0021, 0xFF, 0132 },   /* CB keypad : | : */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* CC Keypad # */
  { 0xFF, 0xFF, 0064, 0xFF },   /* CD Keypad Space */
  { 0015, 0xFF, 0xFF, 0xFF },   /* CE keypad @ | @ ` */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* CF Keypad ! */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D0 Keypad Memory Store */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D1 Keypad Memory Recall */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D2 Keypad Memory Clear */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D3 Keypad Memory Add */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D4 Keypad Memory Subtract */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D5 Keypad Memory Multiply */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D6 Keypad Memory Divide */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D7 Keypad +/- */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D8 Keypad Clear */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* D9 Keypad Clear Entry */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DA Keypad Binary */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DB Keypad Octal */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DC Keypad Decimal */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DD Keypad Hexadecimal */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DE Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* DF Reserved */
  { 0xFF, 0020, 0034, 0020 },   /* E0 left control */
  { 0xFF, 0024, 0147, 0043 },   /* E1 left shift */
  { 0xFF, 0045, 0033, 0005 },   /* E2 left alt | left meta */
  { 0xFF, 0005, 0032, 0017 },   /* E3 left gui | left super */
  { 0xFF, 0026, 0035, 0006 },   /* E4 right control */
  { 0xFF, 0025, 0162, 0035 },   /* E5 right shift */
  { 0xFF, 0165, 0036, 0022 },   /* E6 right alt | right meta */
  { 0xFF, 0065, 0037, 0007 },   /* E7 right gui | right super */
  { 0xFF, 0145, 0007, 0004 },   /* E8 | left hyper */
  { 0xFF, 0175, 0040, 0023 },   /* E9 | right hyper */
  { 0xFF, 0104, 0146, 0016 },   /* EA | left top | left symbol */
  { 0xFF, 0155, 0164, 0051 },   /* EB | right top | right symbol */
  { 0xFF, 0044, 0xFF, 0xFF },   /* EC | left greek */
  { 0xFF, 0035, 0xFF, 0xFF },   /* ED | right greek */
  { 0xFF, 0115, 0004, 0036 },   /* EE | repeat */
  { 0xFF, 0xFF, 0005, 0xFF },   /* EF Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F0 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F1 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F2 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F3 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F4 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F5 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F6 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F7 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F8 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* F9 Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* FA Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* FB Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* FC Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* FD Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF },   /* FE Reserved */
  { 0xFF, 0xFF, 0xFF, 0xFF }    /* FF Reserved */
};

typedef struct {
  unsigned long bits[8];
} UsageSet;

static BOOL init = FALSE;
static usb_dev_handle *openHandle = NULL;
static lmkbd_EventMode openMode;
static char features[2] = { 0xFF, 0xFF };
static lmkbd_TranslationMode oldMode = HUT1;
static UsageSet deviceUsages, clientUsages;

static const uint16_t VENDOR = 0x08DB;
static const uint16_t PRODUCT = 0x0001;

// Find and claim LispM keyboard.
static usb_dev_handle *FindKeyboard()
{
  struct usb_bus *bus;
  struct usb_device *dev;
  int i;

  usb_find_busses();
  usb_find_devices();

  for (bus = usb_get_busses(); NULL != bus; bus = bus->next) {
    for (dev = bus->devices; NULL != dev; dev = dev->next) {
      if ((dev->descriptor.idVendor == VENDOR) &&
          (dev->descriptor.idProduct == PRODUCT)) {
        usb_dev_handle *devh = usb_open(dev);
        if (NULL != devh) {
          for (i = 0; i < 4; i++) {
            if (usb_claim_interface(devh, 0) == 0)
              break;
            if (i > 3) {
              usb_close(devh);
              devh = NULL;
              break;
            }
            // Attempt to force open by releasing kernel claimant.
            usb_detach_kernel_driver_np(devh, 0);
          }
          if (NULL != devh) {
            printf("Found LispM keyboard at %s/%s.\n", bus->dirname, dev->filename);
            return devh;
          }
        }
      }
    }
  }
  return NULL;
}

BOOL lmkbd_Open(lmkbd_EventMode eventMode)
{
  if (!init) {
#if 0
    usb_set_debug(255);
#endif
    usb_init();
    init = TRUE;
  }

  lmkbd_Close();

  openHandle = FindKeyboard();
  if (NULL == openHandle)
    return FALSE;

  openMode = eventMode;

  int len;

  // Get features.
  len = usb_control_msg(openHandle, 
                        USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                        HID_REPORT_GET,
                        (HID_RT_FEATURE << 8) | 0,
                        0,
                        features, sizeof(features),
                        USB_TIMEOUT);
  if (len < 0) {
    lmkbd_Close();
    return FALSE;
  }
  lmkbd_TranslationMode mode = (lmkbd_TranslationMode)features[1];
  if (mode != HUT1) {
    features[1] = HUT1;         // Disable Emacs mode.
    len = usb_control_msg(openHandle, 
                          USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                          HID_REPORT_SET,
                          (HID_RT_FEATURE << 8) | 0,
                          0,
                          features, sizeof(features),
                          USB_TIMEOUT);
    if (len < 0) {
      lmkbd_Close();
      return FALSE;
    }
  }
  oldMode = mode;

#if 1
  char leds[1] = { 0x0A };      // Show state for debugging.
  len = usb_control_msg(openHandle, 
                        USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                        HID_REPORT_SET,
                        (HID_RT_OUTPUT << 8) | 0,
                        0,
                        leds, sizeof(leds), 
                        USB_TIMEOUT);
#endif

  memset(&deviceUsages, 0, sizeof(deviceUsages));
  memset(&clientUsages, 0, sizeof(clientUsages));

  return TRUE;
}

void lmkbd_Close()
{
  if (NULL == openHandle) return;

  int len;

  if (oldMode != HUT1) {
    features[1] = oldMode;      // Restore Emacs mode.
    len = usb_control_msg(openHandle, 
                          USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                          HID_REPORT_SET,
                          (HID_RT_FEATURE << 8) | 0,
                          0,
                          features, sizeof(features),
                          USB_TIMEOUT);
  }

#if 1
  char leds[1] = { 0x00 };      // Clear state for debugging.
  len = usb_control_msg(openHandle, 
                        USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
                        HID_REPORT_SET,
                        (HID_RT_OUTPUT << 8) | 0,
                        0,
                        leds, sizeof(leds), 
                        USB_TIMEOUT);
#endif
  
  usb_release_interface(openHandle, 0);
  usb_close(openHandle);
  openHandle = NULL;
}

lmkbd_Keyboard lmkbd_getKeyboard(void)
{
  return (lmkbd_Keyboard)features[0];
}

static inline BOOL IsShift(int usage)
{
  return ((usage >= 0xE0) ||
          ((usage >= 0x82) && (usage <= 0x84)));
}

static inline BOOL AnyNonShift()
{
  int i;
  for (i = 0; i < 7; i++) {
    if (i == 4) {
      // 82-84 are also shifts.
      if ((clientUsages.bits[i] & 0xFFFFFFE3) != 0)
        return TRUE;
    }
    else {
      if (clientUsages.bits[i] != 0)
        return TRUE;
    }
  }
  return FALSE;
}

static inline unsigned long OldShifts()
{
  unsigned long result = 0;

  unsigned long bits = clientUsages.bits[7];
  if (bits & (1 << 0x0)) {      // E0 left control
    result |= (1 << 4);
  }
  if (bits & (1 << 0x1)) {      // E1 left shift
    result |= (1 << 0);
  }
  if (bits & (1 << 0x2)) {      // E2 left alt | left meta
    result |= (1 << 6);
  }
  if (bits & (1 << 0x4)) {      // E4 right control
    result |= (1 << 5);
  }
  if (bits & (1 << 0x5)) {      // E5 right shift
    result |= (1 << 1);
  }
  if (bits & (1 << 0x6)) {      // E6 right alt | right meta
    result |= (1 << 6);
  }
  if (bits & (1 << 0xA)) {      // EA | left top | left symbol
    result |= (1 << 2);
  }
  if (bits & (1 << 0xB)) {      // EB | right top | right symbol
    result |= (1 << 3);
  }

  bits = clientUsages.bits[4];
  if (bits & (1 << 0x2)) {      // 82 locking caps lock
    result |= (1 << 8);
  }

  return result;
}

static inline unsigned long NewShifts()
{
  unsigned long result = 0;

  unsigned long bits = clientUsages.bits[7];
  if (bits & (1 << 0x0)) {      // E0 left control
    result |= (1 << 4);
  }
  if (bits & (1 << 0x1)) {      // E1 left shift
    result |= (1 << 0);
  }
  if (bits & (1 << 0x2)) {      // E2 left alt | left meta
    result |= (1 << 5);
  }
  if (bits & (1 << 0x3)) {      // E3 left gui | left super
    result |= (1 << 6);
  }
  if (bits & (1 << 0x4)) {      // E4 right control
    result |= (1 << 4);
  }
  if (bits & (1 << 0x5)) {      // E5 right shift
    result |= (1 << 0);
  }
  if (bits & (1 << 0x6)) {      // E6 right alt | right meta
    result |= (1 << 5);
  }
  if (bits & (1 << 0x7)) {      // E7 right gui | right super
    result |= (1 << 6);
  }
  if (bits & (1 << 0x8)) {      // E8 | left hyper
    result |= (1 << 7);
  }
  if (bits & (1 << 0x9)) {      // E9 | right hyper
    result |= (1 << 7);
  }
  if (bits & (1 << 0xA)) {      // EA | left top | left symbol
    result |= (1 << 2);
  }
  if (bits & (1 << 0xB)) {      // EB | right top | right symbol
    result |= (1 << 2);
  }
  if (bits & (1 << 0xC)) {      // EC | left greek
    result |= (1 << 1);
  }
  if (bits & (1 << 0xD)) {      // ED | right greek
    result |= (1 << 1);
  }
  if (bits & (1 << 0xE)) {      // EE | repeat
    result |= (1 << 10);
  }

  
  bits = clientUsages.bits[4];
  if (bits & (1 << 0x2)) {      // 82 locking caps lock
    result |= (1 << 3);
  }
  if (bits & (1 << 0x3)) {      // 83 locking num lock | alt lock
    result |= (1 << 8);
  }
  if (bits & (1 << 0x4)) {      // 82 locking scroll lock | mode lock
    result |= (1 << 9);
  }

  return result;
}

static inline void SetDeviceUsage(int usage)
{
  deviceUsages.bits[usage / 32] |= (1 << (usage % 32));
}

static inline void SetDeviceUsages(const unsigned char *pkt)
{
  memset(&deviceUsages, 0, sizeof(deviceUsages));
  int i;
  for (i = 0; i < 8; i++) {
    if (pkt[0] & (1 << i)) {
      SetDeviceUsage(0xE0 + i);
    }
  }
  for (i = 2; i < 8; i++) {
    if (pkt[i] != 0) {
      SetDeviceUsage(pkt[i]);
    }
  }
}

int lmkbd_Read(long timeout)
{
  unsigned char pkt[8];
  while (1) {
    // Look for difference in state and return it to client as event.
    int i, j;
    for (i = 0; i < 8; i++) {
      unsigned long dbits = deviceUsages.bits[i];
      unsigned long diffs = dbits ^ clientUsages.bits[i];
      if (0 != diffs) {
        for (j = 0; j < 32; j++) {
          unsigned long mask = (1 << j);
          if (diffs & mask) {
            int usage = i * 32 + j;
            switch (openMode) {
            case CADR:
              {
                BOOL newFormat = (lmkbd_getKeyboard() != TK);
                if (dbits & mask) {
                  clientUsages.bits[i] |= mask;
                  // Key down.
                  if (newFormat) {
                    return (0x00F90000 | KeyMappings[usage][SPACE_CADET]);
                  }
                  else if (!IsShift(usage)) { // No separate event for shift down.
                    return (0x00FF0000 | OldShifts() | KeyMappings[usage][TK]);
                  }
                }
                else {
                  clientUsages.bits[i] &= ~mask;
                  // Key up.
                  if (newFormat) {
                    if (IsShift(usage) || AnyNonShift()) {
                      return (0x00F90000 | 0x0100 | KeyMappings[usage][SPACE_CADET]);
                    }
                    else {
                      // All keys up for last non-shift.
                      return (0x00F90000 | 0x8000 | NewShifts());
                    }
                  }
                }
              }
              break;
            case EXPLORER:
              {
                if (dbits & mask) {
                  clientUsages.bits[i] |= mask;
                  // Key down.
                  return (0x80 | KeyMappings[usage][TI]);
                }
                else {
                  clientUsages.bits[i] &= ~mask;
                  // Key up.
                  return KeyMappings[usage][TI];
                }
              }
              break;
            }
          }
        }
      }
    }
    // Get usage report and update device state from it.
    int len = usb_interrupt_read(openHandle, 1, (char *)pkt, sizeof(pkt), timeout);
    if (len < 0) 
      return len;
#if 0
    for (i = 0; i < 8; i++) {
      printf("%02X ", pkt[i]);
    }
    printf("\n");
#endif
    SetDeviceUsages(pkt);
  }
}
