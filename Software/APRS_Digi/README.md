# APRS_Digi

Source-only Arduino IDE project for a 1200-baud APRS digipeater on **Arduino Uno / ATmega328P**.
No precompiled HEX or external firmware generator is required.

The sketch is intentionally **safe by default for a public repository**: it ships with `NOCALL`, placeholder coordinates, and `CONFIGURATION_CONFIRMED = false`. RF transmission remains disabled until the operator edits the configuration and explicitly confirms it.

## Reference hardware

The bundled modem code is configured for this pinout:

- RX audio: **A2 / ADC2**
- PTT: **D3**, active HIGH
- 4-bit AFSK DAC:
  - D4 = 8k2
  - D5 = 3k9
  - D6 = 2k2
  - D7 = 1k
- manual beacon push button: **D8 to GND**, using `INPUT_PULLUP`
- TX LED: **D13**

The modem / AX.25 code is bundled under `src/LibAPRS_Digi`, so there is no separate Arduino Library Manager dependency. Install the normal **Arduino AVR Boards** core and select **Arduino Uno**.

## Open in Arduino IDE

1. Keep the project folder named `APRS_Digi`.
2. Open `APRS_Digi.ino`.
3. Select **Tools -> Board -> Arduino AVR Boards -> Arduino Uno**.
4. Select the correct serial port.
5. Edit the **USER CONFIGURATION** section.
6. Use **Sketch -> Verify/Compile**.
7. Bench-test PTT and AFSK audio before connecting the transmitter to an antenna.
8. Only after all settings have been verified, set `CONFIGURATION_CONFIRMED = true`, compile again, and upload.

## User configuration

Normal sysop settings are grouped near the top of `APRS_Digi.ino`.
The public template starts like this:

```cpp
const bool CONFIGURATION_CONFIRMED = false;

const char STATION_CALLSIGN[] = "NOCALL";
const uint8_t STATION_SSID = 15;

const char APRS_TOCALL[] = "APZ3GK";
const uint8_t APRS_TOCALL_SSID = 0;

const double STATION_LATITUDE = 0.0;
const double STATION_LONGITUDE = 0.0;

const char APRS_SYMBOL_TABLE = '/';
const char APRS_SYMBOL_CODE = '#';

const char APRS_PHG[] = "";

const bool BEACON_ALTERNATE_COMMENTS = true;
const char APRS_COMMENT_1[] = "/W2 APRS DIGI";
const char APRS_COMMENT_2[] = "/Arduino APRS Digipeater";

const uint16_t BEACON_INTERVAL_MINUTES = 15;
```

At minimum, review/change:

- `STATION_CALLSIGN`
- `STATION_SSID`
- `STATION_LATITUDE` / `STATION_LONGITUDE`
- APRS symbol table/code
- `APRS_PHG` if used
- one or both comments
- beacon interval
- own-beacon path mode and WIDE paths
- digipeater WIDE policy for your local network
- PTT polarity/timing for your radio interface
- finally, `CONFIGURATION_CONFIRMED = true`

## Decimal coordinates

Enter normal decimal degrees. South and West are negative. The firmware converts them automatically to classic uncompressed APRS coordinates (`DDMM.mmN/S` and `DDDMM.mmE/W`) at startup.

Example only:

```cpp
const double STATION_LATITUDE = 40.123456;
const double STATION_LONGITUDE = 22.654321;
```

Classic uncompressed APRS positions have 0.01-minute resolution, so some rounding is expected.

## APRS symbol / icon

The symbol is defined by the combination:

```cpp
APRS_SYMBOL_TABLE
APRS_SYMBOL_CODE
```

The default `/#` is the normal digipeater symbol.

## PHG

`APRS_PHG` contains the complete optional PHG extension. The public template leaves it empty intentionally:

```cpp
const char APRS_PHG[] = "";
```

If you have calculated and verified a value, enter all seven characters, for example:

```cpp
const char APRS_PHG[] = "PHG5130";
```

Do not copy somebody else's PHG: it describes the RF installation.

## Alternating comments

Two comments can rotate automatically:

```cpp
const bool BEACON_ALTERNATE_COMMENTS = true;
const char APRS_COMMENT_1[] = "/W2 APRS DIGI";
const char APRS_COMMENT_2[] = "/Arduino APRS Digipeater";
```

Successful periodic/startup beacons use:

```text
COMMENT_1 -> COMMENT_2 -> COMMENT_1 -> COMMENT_2 -> ...
```

Set `BEACON_ALTERNATE_COMMENTS = false` to always transmit `COMMENT_1`.
The manual D8 beacon uses `COMMENT_1` and does not advance the periodic comment sequence.

`APRS_PHG + comment` is validated against the classic 43-character position-extension/comment budget.

## Own beacon path

Three modes are available:

```cpp
BEACON_PATH_DIRECT
BEACON_PATH_FIXED
BEACON_PATH_PROPORTIONAL
```

The default is proportional pathing. At a 15-minute base interval:

```text
1st beacon  DIRECT
2nd beacon  WIDE2-1
3rd beacon  DIRECT
4th beacon  WIDE2-2
repeat
```

For a single fixed path, select `BEACON_PATH_FIXED` and edit:

```cpp
BEACON_FIXED_PATH1_CALL
BEACON_FIXED_PATH1_SSID
BEACON_FIXED_PATH2_CALL
BEACON_FIXED_PATH2_SSID
```

Network density varies by region. Adjust beacon paths conservatively for the local APRS network.

## Digipeater behavior

The forwarding engine implements a traceable New-N style profile:

- only the first unused AX.25 repeater address is serviced
- explicit routing to the station's own callsign/SSID is supported
- `WIDE1-1` and `WIDE2-n` are traceable
- `WIDE2-2` is transformed to `MYCALL*,WIDE2-1`
- source, destination, and APRS information field are preserved
- H / has-been-repeated bits are handled in the outgoing via path
- duplicate suppression defaults to 30 seconds and excludes the via path from the fingerprint
- packets already containing this digipeater as a used repeater are not repeated again
- RF paths containing `TCPIP` or `TCPXX` are rejected
- legacy `RELAY`, bare `WIDE`, and `TRACE` aliases are not serviced
- large `WIDE3-n` through `WIDE7-n` requests can be trapped to one local hop or rejected
- trailing markers such as `NOGATE` / `RFONLY` are preserved

Relevant settings:

```cpp
const uint8_t DIGI_MAX_WIDE_N = 2;
const bool DIGI_TRAP_LARGE_N = true;
const uint32_t DIGI_DUPLICATE_WINDOW_MS = 30000UL;
```

## Manual beacon button on D8

Reference wiring:

```text
D8 ---- push button ---- GND
```

The sketch uses `INPUT_PULLUP` and provides:

- 50 ms debounce
- minimum 700 ms valid press
- TX request on button release
- 5 s stuck/noisy-button lockout until release
- one valid press = one manual TX attempt
- successful manual beacon restarts the normal automatic-beacon timer

The manual beacon path is configured independently with `MANUAL_PATH*` settings.

## Unattended-operation failsafes

The current build includes three complementary protections for 24/7 service:

- **8-second ATmega328P hardware watchdog** (`ENABLE_WATCHDOG=true`). A hard CPU/modem stall causes an automatic MCU reset instead of leaving the digipeater dead indefinitely.
- **5-second pending-relay failsafe** (`DIGI_PENDING_FAILSAFE_MS`). This is independent of the normal 1.2-second relay defer limit and is checked even if a stale TX/modem state would otherwise block relay servicing.
- **AFSK TX timeout recovery**: a TX timeout explicitly aborts the low-level modem/FIFO state before releasing PTT, so a stale `APRS_isSending()` flag cannot permanently block later traffic.
- **60-second serial health heartbeat** when `ENABLE_SERIAL_DIAGNOSTICS=1`. It reports uptime, RX/queue/TX/duplicate/drop counters, pending/channel/TX state and estimated free SRAM. The heartbeat and counters are compiled out when diagnostics are disabled.

Typical diagnostic line:

```text
HEALTH up=32760s rx=1832 q=91 digiTx=87 dup=215 drop=4 fs=0 txto=0 bcn=36 bcnFail=0 pending=0 busy=0 afskTx=0 ram=830
```

For bench diagnosis, use `ENABLE_SERIAL_DIAGNOSTICS=1`. For an unattended production Uno, `0` recovers the HardwareSerial buffers and removes the diagnostic counters/prints while leaving the watchdog and relay failsafe active.

## Radio / AFSK timing

Conservative starting values are exposed in the USER CONFIGURATION section:

```cpp
APRS_PREAMBLE_MS = 350
APRS_TAIL_MS = 80
RADIO_KEYUP_MS = 150
TX_MIN_TOTAL_MS = 1000
TX_POST_HOLD_MS = 30
TX_TIMEOUT_MS = 4000
```

These are hardware/radio dependent. Verify decoding and RF deviation before shortening them.

## Receive audio and channel-busy detection

The bundled modem continuously samples **A2 / ADC2** at the modem sample rate for 1200-baud AFSK receive. Channel activity detection uses that same sample stream rather than a separate `analogRead(A2)`, avoiding disruption of modem timing.

For bench testing only, channel-busy detection can be disabled with:

```cpp
const bool CHANNEL_BUSY_DETECT_ENABLED = false;
```

For normal unattended RF service it should normally remain enabled.

## D3 PTT and D4-D7 DAC

PTT and the DAC share AVR `PORTD`. The bundled modem has been modified so its DAC ISR updates only D4-D7 and preserves D0-D3, allowing **D3 PTT** to remain asserted while AFSK samples are generated.

D3 and D8 are high-level sketch settings. A2 and the D4-D7 DAC mapping are low-level modem mappings under:

```text
src/LibAPRS_Digi/device.h
```

## TOCALL

The template uses:

```cpp
const char APRS_TOCALL[] = "APZDIY";
```

`APZxxx` is the APRS experimental/development family. Treat this as a development placeholder, not as a claimed permanent product allocation. A project distributed as a named device/firmware should use an appropriate current APRS device identifier.

## Files / Arduino tabs

- `APRS_Digi.ino` - user configuration, state, `setup()` and `loop()`
- `05_utils.ino` - configuration validation and AX.25 helpers
- `10_radio.ino` - PTT, channel-busy logic and TX safety
- `20_beacon.ino` - position beacon, comments and path scheduler
- `30_digipeater.ino` - New-N path processing and duplicate suppression
- `40_button.ino` - D8 manual beacon button
- `50_diagnostics.ino` - Serial Monitor output
- `src/LibAPRS_Digi/` - bundled/modified AFSK and AX.25 modem library

## Safety default

This public version intentionally will not transmit until:

```cpp
const bool CONFIGURATION_CONFIRMED = true;
```

and the callsign passes validation. This is meant to reduce the chance that somebody flashes the repository defaults and unintentionally transmits placeholder identification or coordinates.

## License

The bundled `src/LibAPRS_Digi` directory retains its upstream license file. Keep that license with redistributed copies and review the applicable GPL/copyright obligations before publishing modified versions.

## Build status

The high-level path engine and sketch structure were host-tested during development, but each public-repository revision should still be **Verify/Compile** tested using the Arduino IDE / Arduino AVR Boards toolchain before RF use.

## Arduino IDE folder layout

The sketch directory must be kept intact. The expected layout is:

```text
APRS_Digi/
  APRS_Digi.ino
  05_utils.ino
  10_radio.ino
  20_beacon.ino
  30_digipeater.ino
  40_button.ino
  50_diagnostics.ino
  src/
    LibAPRS_Digi/
      LibAPRS.h
      LibAPRS.cpp
      AFSK.h
      AFSK.cpp
      AX25.h
      AX25.cpp
      ...
```

Do not copy only the `.ino` files. The bundled modem library under `src/LibAPRS_Digi/` is part of the sketch and is required for compilation.

## ATmega328P SRAM note

Arduino Uno has only 2048 bytes of SRAM. This project uses a lean LibAPRS_Digi
wrapper which removes the old tracker/location/message state that the digipeater
does not use. Serial diagnostic string literals use Arduino's `F()` macro, so
those literals remain in flash instead of being copied to SRAM.

The large remaining SRAM consumers are intentional modem/network buffers:
AFSK RX/TX FIFOs, the full AX.25 receive frame buffer, the pending relay frame,
and the duplicate-suppression cache. They should not be reduced casually just
to silence the IDE memory warning.


### Serial diagnostics and SRAM

The Uno has only 2048 bytes of SRAM. `ENABLE_SERIAL_DIAGNOSTICS` is a compile-time
switch in `APRS_Digi.ino`:

```cpp
#define ENABLE_SERIAL_DIAGNOSTICS 0
```

`0` is recommended for unattended production use. All debug output is compiled
out and the sketch does not reference Arduino `Serial`, allowing the linker to
omit HardwareSerial buffers. Set it to `1` during bench testing when you need the
Serial Monitor. All fixed diagnostic strings use `F()` and therefore remain in
flash rather than SRAM when diagnostics are enabled.

This build also uses a lean LibAPRS_Digi wrapper: the legacy tracker/location/
message configuration state was removed because the digipeater sends frames
directly through the AX.25 API. The full AX.25 frame size, 256-byte APRS info
relay capacity, AFSK FIFOs and duplicate cache are intentionally retained.


## ATmega328P SRAM optimization (v2)

The operational build uses a zero-copy relay queue: the pending relay references
the current AX.25 receive buffer instead of allocating a second 256-byte information
buffer. While a relay is pending, the sketch deliberately does not parse another
frame until that relay has been transmitted or dropped. The AFSK ISR continues to
fill the RX FIFO during the short relay holdoff.

The duplicate cache stores a 16-bit seconds timestamp, the TX FIFO is 32 bytes,
and the beacon information/coordinate buffers are stack-local. These changes are
intended to preserve APRS frame capacity while freeing SRAM on an Arduino Uno.
### RX monitor and RX-level diagnostics
- D10 (PB2) is the green RX audio-activity monitor LED; D13 remains the TX LED. Use a series resistor (e.g. 330 Ω) with the external LED.
- `serviceRxAudioDiagnostics()` reports `RXADC min`, `max`, and `pp` approximately once per second when `ENABLE_SERIAL_DIAGNOSTICS` is enabled. The capture is taken from the A2 ADC ISR; it is not a calibrated voltage measurement.
