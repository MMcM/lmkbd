
typedef enum {
  TK = 0, SPACE_CADET = 1, TI = 2, SMBX = 3
} lmkbd_Keyboard;

typedef enum {
  CADR = 0, EXPLORER
} lmkbd_EventMode;

typedef enum {
  HUT1 = 1, EMACS
} lmkbd_TranslationMode;

typedef int BOOL;
#define FALSE 0
#define TRUE 1

/** Open a LispM keyboard via USB. */
BOOL lmkbd_Open(lmkbd_EventMode eventMode);

/** Close any open keyboard. */
void lmkbd_Close();

/** Get the type of keyboard opened. */
lmkbd_Keyboard lmkbd_GetKeyboard(void);

/** Get the next event in CADR format (24-bit integer). */
int lmkbd_Read(long timeout);
