# 2026 modernization notes

This public-repository version modernizes an older Arduino/LibAPRS-style 1200-baud APRS modem/digipeater design for a maintainable Arduino IDE workflow.

## Hardware/reference interface

- Arduino Uno / ATmega328P target
- A2 / ADC2 receive audio
- D3 active-HIGH PTT
- D4-D7 4-bit resistor DAC: 8k2 / 3k9 / 2k2 / 1k
- D8 manual push button
- D13 TX LED
- DAC ISR preserves D0-D3 so D3 PTT can coexist with D4-D7 DAC

## Digipeater changes

- traceable WIDE1-1 / WIDE2-n handling
- 30-second duplicate suppression
- loop prevention
- source, destination and information field preserved while digipeating
- AX.25 H-bit handling in repeated paths
- configurable large-N trap/reject behavior
- legacy RELAY / bare WIDE / TRACE aliases not serviced
- RF TCPIP/TCPXX paths rejected

## Operator configuration

- decimal-degree latitude/longitude converted to APRS uncompressed coordinates
- configurable callsign, SSID, symbol, PHG and comments
- two alternating beacon comments
- direct, fixed, or proportional own-beacon pathing
- independent manual-beacon path
- public-template RF safety interlock with `CONFIGURATION_CONFIRMED`

## Reliability hardening (v3)

- 8-second AVR hardware watchdog enabled for unattended service
- watchdog is disabled at the start of setup and re-enabled after initialization
- independent 5-second pending-relay failsafe, checked before TX-state early returns
- low-level AFSK TX abort/recovery on TX timeout so stale modem sending state cannot block future traffic
- 60-second optional health heartbeat with RX / digi TX / duplicate / drop / beacon counters
- runtime counters compile out with `ENABLE_SERIAL_DIAGNOSTICS=0`
- `DIGI TX` diagnostics now print the complete TNC2-style frame including the information field
