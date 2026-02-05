# PRTClock - DS3231 RTC Module

> Version: 260205D | Updated: 2026-02-05

Confirmed: your module is the "DS3231 For PI" (XY-597) mini RTC.

## Connector Layout
(left→right, component side, battery facing you, connector low):
VCC   SDA   SCL   GND   32K

Details


Power: 3.3 V – 5 V (Pi uses 3.3 V)


I²C address: 0x68


32K = optional 32 kHz output


SQW pin = not broken out on most boards


Backup cell: LIR2032 (rechargeable) — do not replace with CR2032




VCC   SDA   SCL   GND   32K
