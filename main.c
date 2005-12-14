/** Based on Microchip USB C18 Firmware Version 1.0 */

#include <p18cxxx.h>
#include "system\typedefs.h"
#include "system\usb\usb.h"
#include "io_cfg.h"

#include "system\usb\usb_compile_time_validation.h"
#include "user\user.h"

static void SystemInit(void);
static void SystemTasks(void);

#pragma udata

#pragma code
void main(void)
{
  SystemInit();
  UserInit();
  while (TRUE) {
    SystemTasks();
    UserTasks();
  }
}

static void SystemInit(void)
{
  mInitializeUSBDriver();
}

static void SystemTasks(void)
{
  USBCheckBusStatus();          // Must use polling method
  if (UCFGbits.UTEYE!=1)
    USBDriverService();         // Interrupt or polling method
}
