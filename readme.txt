PIC for USB interface to LispM keyboards.    MMcM 10/05

The hardware and protocol are explained in CADR documents recovered by
Brad Parker and released by MIT under their license.  This software
uses essentially the same license.

Support is planned for Space Cadet, Knight, and Symbolics low-profile
keyboards.

The Space Cadet support was actually developed using an 8748 freshly
burned with LMIO1; UKBD PROM.

The system files are those in Microchip USB C18 Firmware Version 1.0,
with some minor fixes for SET_REPORT over EP0.  The user files are
adapted from various demos using it.
