/** Based on Microchip USB C18 Firmware Version 1.0 */

#ifndef IO_CFG_H
#define IO_CFG_H

#include "autofiles\usbcfg.h"

/** Refer to LMKBD.SCH for layout. **/

#define LED1 LATAbits.LATA0     // RA0
#define LED2 LATAbits.LATA1     // RA1
#define LED3 LATAbits.LATA2     // RA2

#define InitPortA() ADCON1 |= 0x0F; TRISA = 0x00;


#define SW1 PORTBbits.RB0       // RB0
#define SW2 PORTBbits.RB1       // RB1
#define KBD_SW (PORTB & 0x03)   // RB<0:1>
#define TK_KBDIN PORTBbits.RB2  // RB2
#define SMBX_KBDIN PORTBbits.RB3 // RB3

#define InitPortB() INTCON2bits.RBPU = 0; TRISB = 0x1F;


#define TK_KBDCLK LATCbits.LATC0 // RC0
#define SMBX_KBDNEXT LATCbits.LATC1 // RC1
#define SMBX_KBDSCAN LATCbits.LATC2 // RC2

#define InitPortC() TRISC = 0x00;


#define InitPorts() InitPortA(); InitPortB(); InitPortC();

#endif //IO_CFG_H
