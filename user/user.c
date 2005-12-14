/** Based on Microchip USB C18 Firmware Version 1.0 */

#include <p18cxxx.h>
#include <usart.h>

#include "system\typedefs.h"
#include "system\usb\usb.h"

#include "io_cfg.h"
#include "user\user.h"

#include "delays.h"
#include "string.h"

typedef enum {
  TK = 0, SPACE_CADET = 1, TI = 2, SMBX = 3
} Keyboard;

typedef enum {
  HUT1 = 1, EMACS
} TranslationMode;

typedef enum {
  NONE = 0, 
  L_SHIFT = 1, R_SHIFT, L_CONTROL, R_CONTROL, L_META, R_META, L_SUPER, R_SUPER,
  L_HYPER, R_HYPER, L_SYMBOL, R_SYMBOL, L_GREEK, R_GREEK,
  CAPS_LOCK, MODE_LOCK, ALT_LOCK, 
  REPEAT
} KeyShift;

// Note that C18 differs from standard C in that it (a) constant will
// be char not int and (b) shift will be performed as char not int.
#define SHIFT(s) (1L << s)

#define L_TOP L_SYMBOL
#define R_TOP R_SYMBOL

#define L_ALT L_META
#define R_ALT R_META
#define L_GUI L_SUPER
#define R_GUI R_SUPER

#define MAX_USB_SHIFT R_GUI

typedef unsigned char HidUsageID;

// Information about each key.
typedef struct {
  HidUsageID hidUsageID;        // Currently always from the Keyboard / Keypad page.
  KeyShift shift;
  rom const char *keysym;       // Or NULL if an ordinary PC/AT-101 key with no symbol.
} KeyInfo;

// As much as possible, keysyms are taken from <gdk/gdkkeysyms.h>,
// which seems to be the most comprehensive list of X keysyms.

#define NO_KEY(idx) { 0, NONE, NULL }
#define SHIFT_KEY(idx,hid,shift) { hid, shift, NULL }
#define PC_KEY(idx,hid,keysym) { hid, NONE, keysym }
// Currently the same, but might want a flag to say how standard the
// non-symbol usage is.
#define LISP_KEY(idx,hid,keysym) { hid, NONE, keysym }

// Standard HID keyboard report.
#define N_KEYS_REPORT 6

typedef union {
  char chars[8];
  struct {
    unsigned char shifts;
    unsigned char reserved;
    HidUsageID keysDown[N_KEYS_REPORT];
  };
} KeyboardReport;

typedef struct {
  union {
    unsigned all;
    struct {
      unsigned cxsent:1;
      unsigned atsent:1;
      unsigned hyper:1;
      unsigned super:1;
      unsigned meta:1;
      unsigned shift:1;
      unsigned control:1;
      unsigned keysym:1;
    };
  } f;
  rom const char *chars;
  unsigned char nchars;
} EmacsEvent;

static void SendKeyReport(void);
static void CreateEmacsEvent(EmacsEvent *event, unsigned long shifts, 
                             rom const char *keysym);
static void SendEmacsEvent(void);
static void KeyDown(rom const KeyInfo *key);
static void KeyUp(rom const KeyInfo *key);

static void InitMIT(void);
static void ReadMIT(void);
static void InitTI(void);
static void ReadTI(void);
static void InitSMBX(void);
static void ScanSMBX(void);

#pragma udata

Keyboard CurrentKeyboard;
TranslationMode CurrentMode;

unsigned long CurrentShifts;
HidUsageID KeysDown[16];
unsigned char NKeysDown;

#define N_EMACS_EVENTS 8
EmacsEvent EventBuffers[N_EMACS_EVENTS];
unsigned char EmacsBufferIn, EmacsBufferOut;
unsigned char EmacsBufferedCount;

KeyboardReport CurrentReport;

char CurrentLEDs;

#pragma code

void UserInit(void)
{
  int i;

  InitPorts();
  // Flash all LEDs on until we receive a host report with their proper state.
  LATA = 0xFF;

  CurrentKeyboard = KBD_SW;
  CurrentMode = EMACS;

  hid_report_feature[0] = (byte)CurrentKeyboard;
  hid_report_feature[1] = (byte)CurrentMode;

  CurrentShifts = 0;
  NKeysDown = 0;
  for (i = 0; i < sizeof(KeysDown); i++)
    KeysDown[i] = 0;

  EmacsBufferIn = EmacsBufferOut = 0;
  EmacsBufferedCount = 0;

  for (i = 0; i < sizeof(CurrentReport); i++) {
    CurrentReport.chars[i] = 0;
  }

  switch (CurrentKeyboard) {
  case TK:
  case SPACE_CADET:
    InitMIT();
    break;
  case TI:
    InitTI();
    break;
  case SMBX:  
    InitSMBX();
    break;
  }
}

// User Application USB tasks.
void UserTasks(void)
{   
  CurrentMode = (TranslationMode)hid_report_feature[1];

  switch (CurrentKeyboard) {
  case TK:
  case SPACE_CADET:
    if (!TK_KBDIN)
      ReadMIT();
    break;
  case TI:
    ReadTI();
    break;
  case SMBX:  
    ScanSMBX();
    break;
  }
  
  if ((usb_device_state < CONFIGURED_STATE) || (UCONbits.SUSPND == 1)) 
    return;

  while ((EmacsBufferedCount > 0) && !mHIDTxIsBusy()) {
    SendEmacsEvent();
  }

  if (HIDRxReport(&CurrentLEDs, 1))
    LATA = CurrentLEDs;
}

// This sends an ordinary key report.
void SendKeyReport(void)
{
  unsigned char shifts;
  int i;

#define ADD_SHIFT(n,s)          \
  if (CurrentShifts & SHIFT(s)) \
    shifts |= (1 << n);

  shifts = 0;
  ADD_SHIFT(0,L_CONTROL);
  ADD_SHIFT(1,L_SHIFT);
  ADD_SHIFT(2,L_ALT);
  ADD_SHIFT(3,L_GUI);
  ADD_SHIFT(4,R_CONTROL);
  ADD_SHIFT(5,R_SHIFT);
  ADD_SHIFT(6,R_ALT);
  ADD_SHIFT(7,R_GUI);
  CurrentReport.shifts = shifts;

  if (NKeysDown > N_KEYS_REPORT) {
    for (i = 0; i < N_KEYS_REPORT; i++) {
      CurrentReport.keysDown[i] = 0x01; // ErrorRollOver
    }
  }
  else {
    for (i = 0; i < N_KEYS_REPORT; i++) {
      CurrentReport.keysDown[i] = (i < NKeysDown) ? KeysDown[i] : 0;
    }
  }

  if (!mHIDTxIsBusy())
    HIDTxReport(CurrentReport.chars, sizeof(CurrentReport));
}

void KeyDown(rom const KeyInfo *key)
{
  int i;

  switch (CurrentMode) {
  case EMACS:
    if (key->shift != NONE) {
      CurrentShifts |= SHIFT(key->shift);
      if (key->shift < L_SUPER)
        break;                  // Send in report.
      else
        return;                 // Don't send anything yet.
    }
    if (key->keysym != NULL) {
      if (EmacsBufferedCount < N_EMACS_EVENTS) {
        EmacsEvent *event = &EventBuffers[EmacsBufferIn];
        CreateEmacsEvent(event, CurrentShifts, key->keysym);
        if (event->nchars > 0) {
          // Found actual keysym; queue for sending.
          EmacsBufferIn = (EmacsBufferIn + 1) % N_EMACS_EVENTS;
          EmacsBufferedCount++;
          return;
        }
      }
    }
    if (NKeysDown < sizeof(KeysDown)) {
      KeysDown[NKeysDown++] = key->hidUsageID;
    }
    if (CurrentShifts & (SHIFT(L_SUPER) | SHIFT(R_SUPER) | 
                         SHIFT(L_HYPER) | SHIFT(R_HYPER))) {
      // An ordinary keysym, but with unusual shifts.  Send prefix.
      // When we later catch up, the actual key will be sent.
      EmacsEvent *event = &EventBuffers[EmacsBufferIn];
      CreateEmacsEvent(event, 
                       (CurrentShifts & (SHIFT(L_SUPER) | SHIFT(R_SUPER) | 
                                         SHIFT(L_HYPER) | SHIFT(R_HYPER))),
                       NULL);
      EmacsBufferIn = (EmacsBufferIn + 1) % N_EMACS_EVENTS;
      EmacsBufferedCount++;
      return;
    }
    if (EmacsBufferedCount > 0)
      return;                   // Will update after events.
    break;
    
  default:
    if (key->shift != NONE) {
      CurrentShifts |= SHIFT(key->shift);
      if (key->shift <= MAX_USB_SHIFT)
        break;                  // No need for usage entry.
    }
    if (NKeysDown < sizeof(KeysDown)) {
      KeysDown[NKeysDown++] = key->hidUsageID;
    }
    break;
  }

  SendKeyReport();
}

void KeyUp(rom const KeyInfo *key)
{
  char usage;
  int i;

  if (key->shift != NONE) {
    CurrentShifts &= ~SHIFT(key->shift);
  }

  usage = key->hidUsageID;
  for (i = 0; i < NKeysDown; i++) {
    if (KeysDown[i] == usage) {
      NKeysDown--;
      while (i < NKeysDown) {
        KeysDown[i] = KeysDown[i+1];
        i++;
      }
      break;
    }
  }

  if (EmacsBufferedCount == 0)  // Otherwise will catch up after last event.
    SendKeyReport();
}

void CreateEmacsEvent(EmacsEvent *event, unsigned long shifts, 
                      rom const char *keysym)
{
  event->f.all = 0;
  if (shifts & (SHIFT(L_HYPER) | SHIFT(R_HYPER)))
    event->f.hyper = 1;
  if (shifts & (SHIFT(L_SUPER) | SHIFT(R_SUPER)))
    event->f.super = 1;
  if (shifts & (SHIFT(L_META) | SHIFT(R_META)))
    event->f.meta = 1;
  if (shifts & (SHIFT(L_SHIFT) | SHIFT(R_SHIFT)))
    event->f.shift = 1;
  if (shifts & (SHIFT(L_CONTROL) | SHIFT(R_CONTROL)))
    event->f.control = 1;

  if (keysym == NULL) {
    event->chars = NULL;
    event->nchars = 0;
  }
  else {
    rom const char *chars;
    unsigned char nchars;
    int n;
    char ch;

     if (shifts & (SHIFT(L_GREEK) | SHIFT(R_GREEK)))
       n = 2;
     else if (shifts & (SHIFT(L_SYMBOL) | SHIFT(R_SYMBOL)))
       n = 1;
     else
       n = 0;
    
    chars = keysym;

    do {
      event->chars = chars;
      nchars = 0;
      while (TRUE) {
        ch = *chars;
        if (ch == '\0')
          break;
        chars++;
        if (ch == ',')
          break;
        nchars++;
      }
      if (ch == '\0')
        break;
    } while (n-- > 0);

    event->nchars = nchars;

    event->f.keysym = 1;
  }
}

char ASCII2HUT1(char ch)
{
  // TODO: Could have a lookup table if this gets much more complicated.
  if ((ch >= 'a') && (ch <= 'z'))
    return 0x04 + (ch - 'a');
  if ((ch >= 'A') && (ch <= 'Z'))
    return 0x04 + (ch - 'A');
  if (ch == '0')
    return 0x27;
  if ((ch >= '1') && (ch <= '9'))
    return 0x1E + (ch - '1');
  if ((ch == '-') || (ch == '_'))
    return 0x2D;
  return 0;
}

char IsKeyDown(char key)
{
  int i;
  if (!key) return FALSE;
  for (i = 0; i < NKeysDown; i++) {
    if (KeysDown[i] == key)
      return TRUE;
  }
  return FALSE;
}

void SendEmacsEvent(void)
{
  EmacsEvent *event;
  HidUsageID key;
  int i;

  event = &EventBuffers[EmacsBufferOut];
  
  // We try to avoid sending an extra report with no keys down between
  // characters.  However, when one is doubled, there is no alternative.
  CurrentReport.shifts = 0;
  for (i = 1; i < N_KEYS_REPORT; i++) {
    CurrentReport.keysDown[i] = 0;
  }
  
  if (event->f.all) {
    // Prefix stage.  Three substates: none, c-X sent, and c-X @ sent.
    if (!event->f.cxsent) {
      key = ASCII2HUT1('x');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.shifts = 1;
        CurrentReport.keysDown[0] = key;
        event->f.cxsent = 1;
      }
    }
    else if (!event->f.atsent) {
      key = ASCII2HUT1('2');    // @
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.shifts = 2;
        CurrentReport.keysDown[0] = key;
        event->f.atsent = 1;
      }
    }
    else if (event->f.hyper) {
      key = ASCII2HUT1('h');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.keysDown[0] = key;
        event->f.hyper = event->f.cxsent = event->f.atsent = 0;
      }
    }
    else if (event->f.super) {
      key = ASCII2HUT1('s');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.keysDown[0] = key;
        event->f.super = event->f.cxsent = event->f.atsent = 0;
      }
    }
    else if (event->f.meta) {
      key = ASCII2HUT1('m');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.keysDown[0] = key;
        event->f.meta = event->f.cxsent = event->f.atsent = 0;
      }
    }
    else if (event->f.shift) {
      key = ASCII2HUT1('s');    // S
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.shifts = 2;
        CurrentReport.keysDown[0] = key;
        event->f.shift = event->f.cxsent = event->f.atsent = 0;
      }
    }
    else if (event->f.control) {
      key = ASCII2HUT1('c');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.keysDown[0] = key;
        event->f.control = event->f.cxsent = event->f.atsent = 0;
      }
    }
    else if (event->f.keysym) { 
      key = ASCII2HUT1('k');
      if (key == CurrentReport.keysDown[0])
        CurrentReport.keysDown[0] = 0;
      else {
        CurrentReport.keysDown[0] = key;
        event->f.keysym = event->f.cxsent = event->f.atsent = 0;
      }
    }
  }
  else {
    // Keysym stage.
    if (event->chars != NULL) {
      if (event->nchars > 0) {
        key = ASCII2HUT1(*event->chars);
        if (key == CurrentReport.keysDown[0])
          CurrentReport.keysDown[0] = 0;
        else {
          CurrentReport.keysDown[0] = key;
          event->chars++;
          event->nchars--;
        }
      }
      else {
        key = 0x28;             // RET
        if (key == CurrentReport.keysDown[0])
          CurrentReport.keysDown[0] = 0;
        else {
          CurrentReport.keysDown[0] = key;
          event->chars = NULL;
        }
      }
    }
    else {
      // There is nothing left to do for this event.
      if ((EmacsBufferedCount == 1) &&
          IsKeyDown(CurrentReport.keysDown[0]))
        CurrentReport.keysDown[0] = 0;
      else {
        EmacsBufferOut = (EmacsBufferOut + 1) % N_EMACS_EVENTS;
        EmacsBufferedCount--;
        if (EmacsBufferedCount == 0)
          // Catch up with actual key settings.
          SendKeyReport();
        return;
      }
    }
  }

  // Have already checked mHIDTxIsBusy().
  HIDTxReport(CurrentReport.chars, sizeof(CurrentReport));
}

/**** Knight keyboards ****/

#pragma udata

rom KeyInfo TKKeys[64] = {
  LISP_KEY(00, 0x9B, "break"),  /* break | cancel */
  LISP_KEY(01, 0x29, "escape"), /* esc */
  PC_KEY(02, 0x1E, NULL),       /* 1 ! */
  PC_KEY(03, 0x1F, NULL),       /* 2 " */
  PC_KEY(04, 0x20, NULL),       /* 3 # */
  PC_KEY(05, 0x21, NULL),       /* 4 $ */
  PC_KEY(06, 0x22, NULL),       /* 5 % */
  PC_KEY(07, 0x23, NULL),       /* 6 & */
  PC_KEY(10, 0x24, NULL),       /* 7 ' */
  PC_KEY(11, 0x25, NULL),       /* 8 ( */
  PC_KEY(12, 0x26, NULL),       /* 9 ) */
  PC_KEY(13, 0x27, NULL),       /* 0 _ */
  PC_KEY(14, 0x2E, NULL),       /* - = */
  LISP_KEY(15, 0xCE, "atsign"), /* @ ` | keypad @ */
  LISP_KEY(16, 0xC3, "caret"),  /* ^ ~ | keypad ^ */
  PC_KEY(17, 0x49, NULL),       /* bs | insert */
  LISP_KEY(20, 0xA0, "call"),   /* call | out */
  LISP_KEY(21, 0x9C, "clear"),  /* clear */
  PC_KEY(22, 0x2B, NULL),       /* tab */
  LISP_KEY(23, 0x29, "altmode"), /* alt */
  PC_KEY(24, 0x14, NULL),       /* q conjunction */
  PC_KEY(25, 0x1A, NULL),       /* w disjunction */
  PC_KEY(26, 0x08, NULL),       /* e uplump */
  PC_KEY(27, 0x15, NULL),       /* r downlump */
  PC_KEY(30, 0x17, NULL),       /* t leftlump */
  PC_KEY(31, 0x1C, NULL),       /* y rightlump */
  PC_KEY(32, 0x18, NULL),       /* u elbow */
  PC_KEY(33, 0x0C, NULL),       /* i wheel */
  PC_KEY(34, 0x12, NULL),       /* o downarrow */
  PC_KEY(35, 0x13, NULL),       /* p uparrow */
  PC_KEY(36, 0x2F, NULL),       /* [ { */
  PC_KEY(37, 0x30, NULL),       /* ] } */
  PC_KEY(40, 0x31, NULL),       /* \ | */
  PC_KEY(41, 0x54, NULL),       /* / infinity | keypad / */
  PC_KEY(42, 0x56, NULL),       /* circle minus / delta | keypad - */
  PC_KEY(43, 0x57, NULL),       /* circle plus / del | keypad + */
  LISP_KEY(44, 0x9F, "form"),   /* form | keypad separator */
  LISP_KEY(45, 0x4E, "vt"),     /* vt | page down */
  PC_KEY(46, 0x2A, NULL),       /* rubout | delete (backspace) */
  PC_KEY(47, 0x04, NULL),       /* a less or equal */
  PC_KEY(50, 0x16, NULL),       /* s greater or equal */
  PC_KEY(51, 0x07, NULL),       /* d equivalence */
  PC_KEY(52, 0x09, NULL),       /* f partial */
  PC_KEY(53, 0x0A, NULL),       /* g not equal */
  PC_KEY(54, 0x0B, NULL),       /* h help */
  PC_KEY(55, 0x0D, NULL),       /* j leftarrow */
  PC_KEY(56, 0x0E, NULL),       /* k rightarrow */
  PC_KEY(57, 0x0F, NULL),       /* l botharrow */
  PC_KEY(60, 0x33, NULL),       /* ; plus */
  LISP_KEY(61, 0xCB, "colon"),  /* : * | keypad : */
  PC_KEY(62, 0x28, NULL),       /* return | enter */
  LISP_KEY(63, 0x58, "line"),   /* line | keypad enter */
  LISP_KEY(64, 0x9D, "backnext"), /* backnext | prior */
  PC_KEY(65, 0x1D, NULL),       /* z alpha */
  PC_KEY(66, 0x1B, NULL),       /* x beta */
  PC_KEY(67, 0x06, NULL),       /* c epsilon */
  PC_KEY(70, 0x19, NULL),       /* v lambda */
  PC_KEY(71, 0x05, NULL),       /* b pi */
  PC_KEY(72, 0x11, NULL),       /* n universal */
  PC_KEY(73, 0x10, NULL),       /* m existential */
  PC_KEY(74, 0x36, NULL),       /* , < */
  PC_KEY(75, 0x37, NULL),       /* . > */
  PC_KEY(76, 0x38, NULL),       /* / ? */
  PC_KEY(77, 0x2C, NULL)        /* space */
};

static void TKShiftKeys(unsigned short mask)
{
#define UPDATE_SHIFTS(n,s)              \
  if (mask & ((unsigned short)1 << n))  \
    CurrentShifts |= SHIFT(s);          \
  else                                  \
    CurrentShifts &= ~SHIFT(s);

  UPDATE_SHIFTS(6,L_SHIFT);
  UPDATE_SHIFTS(7,R_SHIFT);
  UPDATE_SHIFTS(8,L_TOP);
  UPDATE_SHIFTS(9,R_TOP);
  UPDATE_SHIFTS(10,L_CONTROL);
  UPDATE_SHIFTS(11,R_CONTROL);
  UPDATE_SHIFTS(12,L_META);
  UPDATE_SHIFTS(13,R_META);
  UPDATE_SHIFTS(14,CAPS_LOCK);
}

/**** Space Cadet keyboards ****/

#pragma udata

rom KeyInfo SpaceCadetKeys[128] = {
  NO_KEY(000),
  LISP_KEY(001, 0x43, "ii"),    /* II | F10 */
  LISP_KEY(002, 0x45, "iv"),    /* IV | F12 */
  SHIFT_KEY(003, 0x84, MODE_LOCK), /* mode lock | locking scroll lock */
  NO_KEY(004),
  SHIFT_KEY(005, 0xE3, L_SUPER), /* left super | left gui */
  NO_KEY(006),
  NO_KEY(007),
  NO_KEY(010),
  PC_KEY(011, 0x21, ",,cent"),  /* 4 */
  PC_KEY(012, 0x15, ",union,rho"),   /* r */
  PC_KEY(013, 0x09, ",righttack,phi"),   /* f */
  PC_KEY(014, 0x19, ",similarequal,varsigma"), /* v */
  SHIFT_KEY(015, 0x83, ALT_LOCK), /* alt lock | locking num lock */
  NO_KEY(016),
  LISP_KEY(017, 0x4F, "handright,,circleslash"), /* hand right | right arrow */
  SHIFT_KEY(020, 0xE0, L_CONTROL), /* left control */
  LISP_KEY(021, 0xCB, "colon,plusminus,section"), /* plus minus | keypad : */
  PC_KEY(022, 0x2B, NULL),      /* tab */
  PC_KEY(023, 0x2A, NULL),      /* rubout | delete (backspace) */
  SHIFT_KEY(024, 0xE1, L_SHIFT), /* left shift */
  SHIFT_KEY(025, 0xE5, R_SHIFT), /* right shift */
  SHIFT_KEY(026, 0xE4, R_CONTROL), /* right control */
  NO_KEY(027),
  LISP_KEY(030, 0x48, "holdoutput"), /* hold output | pause */
  PC_KEY(031, 0x25, ",,times"), /* 8 */
  PC_KEY(032, 0x0C, ",infinity,iota"), /* i */
  PC_KEY(033, 0x0E, ",rightarrow,kappa"), /* k */
  PC_KEY(034, 0x36, ",,guillemotleft"), /* comma */
  SHIFT_KEY(035, 0xED, R_GREEK), /* right greek */
  LISP_KEY(036, 0x58, "line"),  /* line | keypad enter */
  PC_KEY(037, 0x31, ",,doublevertbar"), /* back slash */
  LISP_KEY(040, 0xA1, "terminal"), /* terminal | oper */
  NO_KEY(041),
  LISP_KEY(042, 0x76, "network"), /* network | menu */
  NO_KEY(043),
  SHIFT_KEY(044, 0xEC, L_GREEK), /* left greek */
  SHIFT_KEY(045, 0xE2, L_META), /* left meta | left alt */
  LISP_KEY(046, 0x9A, "status"), /* status | sysreq / attention */
  LISP_KEY(047, 0x9E, "resume"), /* resume | return */
  LISP_KEY(050, 0xA2, "clearscreen"), /* clear screen	| clear / again */
  PC_KEY(051, 0x23, ",,quad"),  /* 6 */
  PC_KEY(052, 0x1C, ",contained,psi"),   /* y */
  PC_KEY(053, 0x0B, ",downarrow,eta"),   /* h */
  PC_KEY(054, 0x11, ",lessthanequal,nu"),    /* n */
  NO_KEY(055),
  NO_KEY(056),
  NO_KEY(057),
  NO_KEY(060),
  PC_KEY(061, 0x1F, ",,doubledagger"), /* 2 */
  PC_KEY(062, 0x1A, ",logicalor,omega"), /* w */
  PC_KEY(063, 0x16, ",uptack,sigma"), /* s */
  PC_KEY(064, 0x1B, ",ceiling,xi"),    /* x */
  SHIFT_KEY(065, 0xE7, R_SUPER), /* right super | right gui */
  NO_KEY(066),
  LISP_KEY(067, 0x78, "abort"), /* abort | stop */
  NO_KEY(070),
  PC_KEY(071, 0x26, ",,paragraph"), /* 9 */
  PC_KEY(072, 0x12, ",exists,omicron"), /* o */
  PC_KEY(073, 0x0F, ",doublearrow,lambda"), /* l */
  PC_KEY(074, 0x37, ",,guillemotright"),/* period */
  NO_KEY(075),
  NO_KEY(076),
  PC_KEY(077, 0x35, ",,notsign"), /* back quote */
  LISP_KEY(100, 0x79, "macro"), /* macro | again */
  LISP_KEY(101, 0x42, "i"),     /* I | F9 */
  LISP_KEY(102, 0x44, "iii"),   /* III | F11 */
  NO_KEY(103),
  SHIFT_KEY(104, 0xEA, L_TOP),  /* left top */
  NO_KEY(105),
  LISP_KEY(106, 0x52, "thumbup,,circleminus"), /* up thumb | up arrow */
  LISP_KEY(107, 0xA0, "call"),  /* call | out */
  LISP_KEY(110, 0x9C, "clearinput"), /* clear input | clear */
  PC_KEY(111, 0x22, ",,degree"), /* 5 */
  PC_KEY(112, 0x17, ",includes,tau"),   /* t */
  PC_KEY(113, 0x0A, ",uparrow,gamma"), /* g */
  PC_KEY(114, 0x05, ",identical,beta"),  /* b */
  SHIFT_KEY(115, 0xEE, REPEAT), /* repeat */
  LISP_KEY(116, 0x75, "help"),  /* help */
  LISP_KEY(117, 0x50, "handleft,,circletimes"), /* hand left | left arrow */
  LISP_KEY(120, 0xA4, "quote"), /* quote | exsel */
  PC_KEY(121, 0x1E, ",,dagger"), /* 1 */
  PC_KEY(122, 0x14, ",logicaland,theta"), /* q */
  PC_KEY(123, 0x04, ",downtack,alpha"), /* a */
  PC_KEY(124, 0x1D, ",floor,zeta"),  /* z */
  SHIFT_KEY(125, 0x82, CAPS_LOCK), /* caps lock | locking caps lock */
  PC_KEY(126, 0x2E, ",,approximate"), /* equals */
  NO_KEY(127),
  NO_KEY(130),
  PC_KEY(131, 0x2D, ",,horizbar"), /* minus */
  LISP_KEY(132, 0xB6, "parenleft,bracketleft,doublebracketleft"), /* ( | keypad ( */
  PC_KEY(133, 0x34, ",,periodcentered"), /* apostrophe */
  PC_KEY(134, 0x2C, NULL),      /* space */
  NO_KEY(135),
  PC_KEY(136, 0x28, NULL),      /* return (enter) */
  LISP_KEY(137, 0xB7, "parenright,bracketright,doublebracketright"), /* ) | keypad ) */
  NO_KEY(140),
  LISP_KEY(141, 0x65, "system"), /* system | application */
  NO_KEY(142),
  LISP_KEY(143, 0x29, "altmode"), /* alt mode | escape */
  NO_KEY(144),
  SHIFT_KEY(145, 0xE8, L_HYPER), /* left hyper */
  LISP_KEY(146, 0xB9, "braceright,rightanglebracket,broketright"), /* } | keypad } */
  NO_KEY(147),
  NO_KEY(150),
  PC_KEY(151, 0x24, ",,division"), /* 7 */
  PC_KEY(152, 0x18, ",forall,upsilon"), /* u */
  PC_KEY(153, 0x0D, ",leftarrow,vartheta"), /* j */
  PC_KEY(154, 0x10, ",greaterthanequal,mu"),    /* m */
  SHIFT_KEY(155, 0xEB, R_TOP),  /* right top */
  PC_KEY(156, 0x4D, NULL),      /* end */
  PC_KEY(157, 0x4C, NULL),      /* delete | delete forward */
  PC_KEY(160, 0x49, NULL),      /* overstrike | insert */
  PC_KEY(161, 0x20, ",,del"),   /* 3 */
  PC_KEY(162, 0x08, ",intersection,epsilon"), /* e */
  PC_KEY(163, 0x07, ",lefttack,delta"), /* d */
  PC_KEY(164, 0x06, ",notequal,chi"),   /* c */
  SHIFT_KEY(165, 0xE6, R_META), /* right meta | right alt */
  LISP_KEY(166, 0xB8, "braceleft,leftanglebracket,broketleft"), /* { | keypad { */
  LISP_KEY(167, 0x9B, "break"), /* break | cancel */
  LISP_KEY(170, 0x48, "stopoutput"), /* stop output | pause */
  PC_KEY(171, 0x27, ",,circle"), /* 0 */
  PC_KEY(172, 0x13, ",partialderivative,pi"), /* p */
  PC_KEY(173, 0x33, ",,doubbaselinedot"), /* semicolon */
  PC_KEY(174, 0x38, ",,integral"), /* question */
  SHIFT_KEY(175, 0xE9, R_HYPER), /* right hyper */
  LISP_KEY(176, 0x51, "thumbdown,,circleplus"), /* down thumb | down arrow */
  NO_KEY(177)
};

#pragma code

static void SpaceCadetAllKeysUp(unsigned short mask)
{
  HidUsageID key;
  unsigned long shift;
  int i, j;

  if (0 == mask) {
    CurrentShifts = 0;
    NKeysDown = 0;
  }
  else {
#define UPDATE_SHIFTS_LR(n,s)                   \
    if (mask & ((unsigned short)1 << n))        \
      CurrentShifts |= SHIFT(L_##s);            \
    else                                        \
      CurrentShifts &= ~(SHIFT(L_##s) | SHIFT(R_##s));

    UPDATE_SHIFTS_LR(0,SHIFT);
    UPDATE_SHIFTS_LR(1,GREEK);
    UPDATE_SHIFTS_LR(2,TOP);
    UPDATE_SHIFTS(3,CAPS_LOCK);
    UPDATE_SHIFTS_LR(4,CONTROL);
    UPDATE_SHIFTS_LR(5,META);
    UPDATE_SHIFTS_LR(6,SUPER);
    UPDATE_SHIFTS_LR(7,HYPER);
    UPDATE_SHIFTS(8,ALT_LOCK);
    UPDATE_SHIFTS(9,MODE_LOCK);
    UPDATE_SHIFTS(10,REPEAT);

    j = 0;
    for (i = 0; i < NKeysDown; i++) {
      key = KeysDown[i];
      // Check for shift keys that are sent as ordinary usage ids
      // instead of in the prefix.
      switch (key) {
#define MAP_SHIFT(u,s)          \
      case u:                   \
        shift = SHIFT(s);       \
        break;

      MAP_SHIFT(0x82,CAPS_LOCK);
      MAP_SHIFT(0x83,ALT_LOCK);
      MAP_SHIFT(0x84,MODE_LOCK);
      MAP_SHIFT(0xE8,L_HYPER);
      MAP_SHIFT(0xE9,R_HYPER);
      MAP_SHIFT(0xEA,L_SYMBOL);
      MAP_SHIFT(0xEB,R_SYMBOL);
      MAP_SHIFT(0xEC,L_GREEK);
      MAP_SHIFT(0xED,R_GREEK);
      MAP_SHIFT(0xEE,REPEAT);

      default:
        shift = 0;
      }
      if (CurrentShifts & shift) {
        // A still active shifting key is preserved.
        if (j != i)
          KeysDown[j] = key;
        j++;
      }
    }
    NKeysDown = j;
  }
  if (EmacsBufferedCount == 0)
    SendKeyReport();
}

#pragma udata

unsigned char tkBits[3];

#pragma code

void InitMIT(void)
{
  TK_KBDCLK = 1;                // Clock idle until data goes low.
}

/** Read and process 24 bits of code.
 * See MOON;KBD PROTOC for interpretation.
 */
void ReadMIT(void)
{
  int i,j;

  for (i = 0; i < 3; i++) {
    unsigned char code = 0;
    for (j = 0; j < 8; j++) {
      TK_KBDCLK = 0;            // Clock low.
      Delay100TCYx(4);          // 400 cycles @ 20MHz = 50kHz.
      if (TK_KBDIN) {
        code |= (1 << j);
      }
      TK_KBDCLK = 1;            // Clock high (idle).
      Delay100TCYx(4);
    }
    tkBits[i] = code;
  }
  switch (tkBits[2]) {
  case 0xF9:
    switch (tkBits[1] & 0xC0) {
    case 0:
      if (tkBits[1] & 0x01)
        KeyUp(&SpaceCadetKeys[tkBits[0]]);
      else
        KeyDown(&SpaceCadetKeys[tkBits[0]]);
      break;
    case 0x80:
      SpaceCadetAllKeysUp(tkBits[0] | (((unsigned short)tkBits[1] & 0x07) << 8));
      break;
    }
    break;
  case 0xFF:
    TKShiftKeys((tkBits[0] & 0xC0) | ((unsigned short)tkBits[1] << 8));
    NKeysDown = 0;              // There are no up transitions.
    KeyDown(&TKKeys[tkBits[0] & 0x3F]);
    break;
  }
}

#pragma udata

rom KeyInfo ExplorerKeys[128] = {
  NO_KEY(000),
  LISP_KEY(001, 0x75, NULL),    // HELP
  NO_KEY(002),
  SHIFT_KEY(003, 0x39, CAPS_LOCK), // CAPS-LOCK
  LISP_KEY(004, 0xEE, "boldlock"), // BOLD-LOCK (shift key? LED?)
  LISP_KEY(005, 0xEF, "itallock"), // ITAL-LOCK (shift key? LED?)
  SHIFT_KEY(006, 0x47, MODE_LOCK), // MODE-LOCK
  SHIFT_KEY(007, 0xE8, L_HYPER), // LEFT-HYPER
  LISP_KEY(010, 0x65, "system"), // SYSTEM
  LISP_KEY(011, 0x76, "network"), // NETWORK
  LISP_KEY(012, 0x9A, "status"), // STATUS
  LISP_KEY(013, 0xA1, "terminal"), // TERMINAL
  NO_KEY(014),
  LISP_KEY(015, 0xA2, "clearscreen"), // CLEAR-SCREEN
  LISP_KEY(016, 0x9C, "clearinput"), // CLEAR-INPUT
  LISP_KEY(017, 0x7A, "undo"),  // UNDO
  PC_KEY(020, 0x4D, NULL),      // END
  LISP_KEY(021, 0x42, "left"),  // LEFT (mouse keys? like i ii iii?)
  LISP_KEY(022, 0x43, "middle"), // MIDDLE
  LISP_KEY(023, 0x44, "right"), // RIGHT
  PC_KEY(024, 0x3A, NULL),      // F1
  PC_KEY(025, 0x3B, NULL),      // F2
  PC_KEY(026, 0x3C, NULL),      // F3
  PC_KEY(027, 0x3D, NULL),      // F4
  NO_KEY(030),
  NO_KEY(031),
  SHIFT_KEY(032, 0xE3, L_SUPER), // LEFT-SUPER
  SHIFT_KEY(033, 0xE2, L_META), // LEFT-META
  SHIFT_KEY(034, 0xE0, L_CONTROL), // LEFT-CONTROL
  SHIFT_KEY(035, 0xE4, R_CONTROL), // RIGHT-CONTROL
  SHIFT_KEY(036, 0xE6, R_META), // RIGHT-META
  SHIFT_KEY(037, 0xE7, R_SUPER), // RIGHT-SUPER
  SHIFT_KEY(040, 0xE9, R_HYPER), // RIGHT-HYPER
  LISP_KEY(041, 0x9E, "resume"), // RESUME
  NO_KEY(042),
  LISP_KEY(043, 0x29, "escape"), // ALT (ESCAPE actually?)
  PC_KEY(044, 0x1E, NULL),      // 1
  PC_KEY(045, 0x1F, NULL),      // 2
  PC_KEY(046, 0x20, NULL),      // 3
  PC_KEY(047, 0x21, NULL),      // 4
  PC_KEY(050, 0x22, NULL),      // 5
  PC_KEY(051, 0x23, NULL),      // 6
  PC_KEY(052, 0x24, NULL),      // 7
  PC_KEY(053, 0x25, NULL),      // 8
  PC_KEY(054, 0x26, NULL),      // 9
  PC_KEY(055, 0x27, NULL),      // 0
  PC_KEY(056, 0x2D, NULL),      // MINUS
  PC_KEY(057, 0x2E, NULL),      // EQUALS
  PC_KEY(060, 0xB8, NULL),      // BACK-QUOTE (` {)
  PC_KEY(061, 0xB9, NULL),      // TILDE (~ })
  PC_KEY(062, 0x67, NULL),      // KEYPAD-EQUAL
  PC_KEY(063, 0x57, NULL),      // KEYPAD-PLUS
  PC_KEY(064, 0xCD, NULL),      // KEYPAD-SPACE
  PC_KEY(065, 0xBA, NULL),      // KEYPAD-TAB
  LISP_KEY(066, 0x9B, "break"),  // BREAK
  NO_KEY(067),
  PC_KEY(070, 0x2B, NULL),      // TAB
  PC_KEY(071, 0x14, NULL),      // Q
  PC_KEY(072, 0x1A, NULL),      // W
  PC_KEY(073, 0x08, NULL),      // E
  PC_KEY(074, 0x15, NULL),      // R
  PC_KEY(075, 0x17, NULL),      // T
  PC_KEY(076, 0x1C, NULL),      // Y
  PC_KEY(077, 0x18, NULL),      // U
  PC_KEY(100, 0x0C, NULL),      // I
  PC_KEY(101, 0x12, NULL),      // O
  PC_KEY(102, 0x13, NULL),      // P
  LISP_KEY(103, 0xB6, "parenleft,bracketleft"), // OPEN-PARENTHESIS
  LISP_KEY(104, 0xB7, "parenright,bracketleft"), // CLOSE-PARENTHESIS
  NO_KEY(105),
  PC_KEY(106, 0x31, NULL),      // BACKSLASH
  PC_KEY(107, 0x52, NULL),      // UP-ARROW
  PC_KEY(110, 0x5F, NULL),      // KEYPAD-7
  PC_KEY(111, 0x60, NULL),      // KEYPAD-8
  PC_KEY(112, 0x61, NULL),      // KEYPAD-9
  PC_KEY(113, 0x56, NULL),      // KEYPAD-MINUS
  LISP_KEY(114, 0x78, "abort"), // ABORT
  NO_KEY(115),
  NO_KEY(116),
  PC_KEY(117, 0x2A, NULL),      // RUBOUT
  PC_KEY(120, 0x04, NULL),      // A
  PC_KEY(121, 0x16, NULL),      // S
  PC_KEY(122, 0x07, NULL),      // D
  PC_KEY(123, 0x09, NULL),      // F
  PC_KEY(124, 0x0A, NULL),      // G
  PC_KEY(125, 0x0B, NULL),      // H
  PC_KEY(126, 0x0D, NULL),      // J
  PC_KEY(127, 0x0E, NULL),      // K
  PC_KEY(130, 0x0F, NULL),      // L
  PC_KEY(131, 0x33, NULL),      // SEMICOLON
  PC_KEY(132, 0x34, NULL),      // APOSTROPHE
  PC_KEY(133, 0x28, NULL),      // RETURN
  LISP_KEY(134, 0xA5, "line"),  // LINE (others use keypad enter)
  PC_KEY(135, 0x50, NULL),      // LEFT-ARROW
  PC_KEY(136, 0x4A, NULL),      // HOME
  PC_KEY(137, 0x4F, NULL),      // RIGHT-ARROW
  PC_KEY(140, 0x5C, NULL),      // KEYPAD-4
  PC_KEY(141, 0x5D, NULL),      // KEYPAD-5
  PC_KEY(142, 0x5E, NULL),      // KEYPAD-6
  PC_KEY(143, 0x85, NULL),      // KEYPAD-COMMA
  NO_KEY(144),
  NO_KEY(145),
  SHIFT_KEY(146, 0xEA, L_SYMBOL), // LEFT-SYMBOL
  SHIFT_KEY(147, 0xE1, L_SHIFT), // LEFT-SHIFT
  PC_KEY(150, 0x1D, NULL),      // Z
  PC_KEY(151, 0x1B, NULL),      // X
  PC_KEY(152, 0x06, NULL),      // C
  PC_KEY(153, 0x19, NULL),      // V
  PC_KEY(154, 0x05, NULL),      // B
  PC_KEY(155, 0x11, NULL),      // N
  PC_KEY(156, 0x10, NULL),      // M
  PC_KEY(157, 0x36, NULL),      // COMMA
  PC_KEY(160, 0x37, NULL),      // PERIOD
  PC_KEY(161, 0x38, NULL),      // QUESTION
  SHIFT_KEY(162, 0xE5, R_SHIFT), // RIGHT-SHIFT
  NO_KEY(163),
  SHIFT_KEY(164, 0xEB, R_SYMBOL), // RIGHT-SYMBOL
  PC_KEY(165, 0x51, NULL),      // DOWN-ARROW
  PC_KEY(166, 0x59, NULL),      // KEYPAD-1
  PC_KEY(167, 0x5A, NULL),      // KEYPAD-2
  PC_KEY(170, 0x5B, NULL),      // KEYPAD-3
  NO_KEY(171),
  NO_KEY(172),
  PC_KEY(173, 0x2C, NULL),      // SPACE
  NO_KEY(174),
  PC_KEY(175, 0x62, NULL),      // KEYPAD-0
  PC_KEY(176, 0x63, NULL),      // KEYPAD-PERIOD
  PC_KEY(177, 0x58, NULL)       // KEYPAD-ENTER
};

#pragma code

void InitTI(void)
{
}

void ReadTI(void)
{
  char scan;

  // TODO: Actually read from the hardware somehow.  UART?
  scan = 0;
  
  if (scan & 0x80)
    KeyDown(&ExplorerKeys[scan & 0x7F]);
  else
    KeyUp(&ExplorerKeys[scan]);
}

#pragma udata

unsigned char smbxKeyStates[16], smbxNKeyStates[16];

rom KeyInfo SMBXKeyInfos[128] = {
  NO_KEY(000),
  NO_KEY(001),
  LISP_KEY(002, 0xA1, "local"), /* local | oper */
  SHIFT_KEY(003, 0x82, CAPS_LOCK), /* caps lock | locking caps lock */
  SHIFT_KEY(004, 0xE8, L_HYPER), /* left hyper */
  SHIFT_KEY(005, 0xE2, L_META), /* left meta | left alt */
  SHIFT_KEY(006, 0xE4, R_CONTROL), /* right control */
  SHIFT_KEY(007, 0xE7, R_SUPER), /* right super | right gui */
  LISP_KEY(010, 0x4E, "scroll"), /* scroll | page down */
  SHIFT_KEY(011, 0x84, MODE_LOCK), /* mode lock | locking scroll lock */
  NO_KEY(012),
  NO_KEY(013),
  NO_KEY(014),
  LISP_KEY(015, 0x65, "select"), /* select | application */
  SHIFT_KEY(016, 0xEA, L_SYMBOL), /* left symbol */
  SHIFT_KEY(017, 0xE3, L_SUPER), /* left super | left gui */
  SHIFT_KEY(020, 0xE0, L_CONTROL), /* left control */
  PC_KEY(021, 0x2C, NULL),      /* space */
  SHIFT_KEY(022, 0xE6, R_META), /* right meta | right alt */
  SHIFT_KEY(023, 0xE9, R_HYPER), /* right hyper */
  PC_KEY(024, 0x4D, NULL),      /* end */
  NO_KEY(025),
  NO_KEY(026),
  NO_KEY(027),
  PC_KEY(030, 0x1D, NULL),      /* z */
  PC_KEY(031, 0x06, NULL),      /* c */
  PC_KEY(032, 0x05, NULL),      /* b */
  PC_KEY(033, 0x10, NULL),      /* m */
  PC_KEY(034, 0x37, NULL),      /* . */
  SHIFT_KEY(035, 0xE5, R_SHIFT), /* right shift */
  SHIFT_KEY(036, 0xEE, REPEAT), /* repeat */
  LISP_KEY(037, 0x78, "abort"), /* abort | stop */
  NO_KEY(040),
  NO_KEY(041),
  NO_KEY(042),
  SHIFT_KEY(043, 0xE1, L_SHIFT), /* left shift */
  PC_KEY(044, 0x1B, NULL),      /* x */
  PC_KEY(045, 0x19, NULL),      /* v */
  PC_KEY(046, 0x11, NULL),      /* n */
  PC_KEY(047, 0x36, NULL),      /* , */
  PC_KEY(050, 0x38, NULL),      /* / */
  SHIFT_KEY(051, 0xEB, R_SYMBOL), /* right symbol */
  LISP_KEY(052, 0x75, "help"),  /* help */
  NO_KEY(053),
  NO_KEY(054),
  NO_KEY(055),
  PC_KEY(056, 0x2A, NULL),      /* rubout | delete (backspace) */
  PC_KEY(057, 0x16, NULL),      /* s */
  PC_KEY(060, 0x09, NULL),      /* f */
  PC_KEY(061, 0x0B, NULL),      /* h */
  PC_KEY(062, 0x0E, NULL),      /* k */
  PC_KEY(063, 0x33, NULL),      /* ; */
  PC_KEY(064, 0x28, NULL),      /* return | enter */
  LISP_KEY(065, 0xA4, "complete"), /* complete | exsel */
  NO_KEY(066),
  NO_KEY(067),
  NO_KEY(070),
  LISP_KEY(071, 0x76, "network"), /* network | menu */
  PC_KEY(072, 0x04, NULL),      /* a */
  PC_KEY(073, 0x07, NULL),      /* d */
  PC_KEY(074, 0x0A, NULL),      /* g */
  PC_KEY(075, 0x0D, NULL),      /* j */
  PC_KEY(076, 0x0F, NULL),      /* l */
  PC_KEY(077, 0x34, NULL),      /* ' */
  LISP_KEY(100, 0x58, "line"),  /* line | keypad enter */
  NO_KEY(101),
  NO_KEY(102),
  NO_KEY(103),
  LISP_KEY(104, 0x79, "function"), /* function | again */
  PC_KEY(105, 0x1A, NULL),      /* w */
  PC_KEY(106, 0x15, NULL),      /* r */
  PC_KEY(107, 0x1C, NULL),      /* y */
  PC_KEY(110, 0x0C, NULL),      /* i */
  PC_KEY(111, 0x13, NULL),      /* p */
  LISP_KEY(112, 0xB7, "parenright"), /* ) | keypad ) */
  LISP_KEY(113, 0x9F, "page"),  /* page | keypad separator */
  NO_KEY(114),
  NO_KEY(115),
  NO_KEY(116),
  PC_KEY(117, 0x2B, NULL),      /* tab */
  PC_KEY(120, 0x14, NULL),      /* q */
  PC_KEY(121, 0x08, NULL),      /* e */
  PC_KEY(122, 0x17, NULL),      /* t */
  PC_KEY(123, 0x18, NULL),      /* u */
  PC_KEY(124, 0x12, NULL),      /* o */
  LISP_KEY(125, 0xB6, "parenleft"), /* ( | keypad ( */
  PC_KEY(126, 0x49, NULL),      /* backspace | insert */
  NO_KEY(127),
  NO_KEY(130),
  NO_KEY(131),
  LISP_KEY(132, 0xCB, "colon"), /* : | keypad : */
  PC_KEY(133, 0x1F, NULL),      /* 2 */
  PC_KEY(134, 0x21, NULL),      /* 4 */
  PC_KEY(135, 0x23, NULL),      /* 6 */
  PC_KEY(136, 0x25, NULL),      /* 8 */
  PC_KEY(137, 0x27, NULL),      /* 0 */
  PC_KEY(140, 0x2E, NULL),      /* = */
  PC_KEY(141, 0x31, NULL),      /* \ */
  NO_KEY(142),
  NO_KEY(143),
  NO_KEY(144),
  PC_KEY(145, 0x1E, NULL),      /* 1 */
  PC_KEY(146, 0x20, NULL),      /* 3 */
  PC_KEY(147, 0x22, NULL),      /* 5 */
  PC_KEY(150, 0x24, NULL),      /* 7 */
  PC_KEY(151, 0x26, NULL),      /* 9 */
  PC_KEY(152, 0x2D, NULL),      /* - */
  PC_KEY(153, 0x35, NULL),      /* ` */
  LISP_KEY(154, 0xC9, "vertbar"), /* | | keypad | */
  NO_KEY(155),
  NO_KEY(156),
  NO_KEY(157),
  LISP_KEY(160, 0x29, "escape"), /* escape */
  LISP_KEY(161, 0xA2, "refresh"), /* refresh | clear / again */
  LISP_KEY(162, 0x42, "square"), /* square | F9 */
  LISP_KEY(163, 0x43, "circle"), /* circle | F10 */
  LISP_KEY(164, 0x44, "triangle"), /* triangle | F11 */
  LISP_KEY(165, 0x9C, "clearinput"), /* clear input | clear */
  LISP_KEY(166, 0x9B, "suspend"), /* suspend | cancel */
  LISP_KEY(167, 0x9E, "resume"), /* resume | return */
  NO_KEY(170),
  NO_KEY(171),
  NO_KEY(172),
  NO_KEY(173),
  NO_KEY(174),
  NO_KEY(175),
  NO_KEY(176),
  NO_KEY(177)
};

#pragma code

void InitSMBX(void)
{
  int i;

  SMBX_KBDSCAN = SMBX_KBDNEXT = 1;

  for (i = 0; i < 16; i++)
    smbxKeyStates[i] = 0;
}

void ScanSMBX(void)
{
  int i,j;

  SMBX_KBDSCAN = 0;
  SMBX_KBDSCAN = 1;
  for (i = 0; i < 16; i++) {
    unsigned char code = 0;
    for (j = 0; j < 8; j++) {
      SMBX_KBDNEXT = 0;
      SMBX_KBDNEXT = 1;
      if (!SMBX_KBDIN) {
        code |= (1 << j);
      }
    }
    smbxNKeyStates[i] = code;
  }

  for (i = 0; i < 16; i++) {
    unsigned char keys, change;
    keys = smbxNKeyStates[i];
    change = keys ^ smbxKeyStates[i];
    if (change == 0) continue;
    smbxKeyStates[i] = keys;
    for (j = 0; j < 8; j++) {
      if (change & (1 << j)) {
        int code = (i * 8) + j;
        if (keys & (1 << j)) {
          KeyDown(&SMBXKeyInfos[code]);
        }
        else {
          KeyUp(&SMBXKeyInfos[code]);
        }
      }
    }    
  }
}
