# Arduino APRS Digipeater 2026

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

1. Keep the project folder named `Arduino_APRS_Digipeater_2026`.
2. Open `Arduino_APRS_Digipeater_2026.ino`.
3. Select **Tools -> Board -> Arduino AVR Boards -> Arduino Uno**.
4. Select the correct serial port.
5. Edit the **USER CONFIGURATION** section.
6. Use **Sketch -> Verify/Compile**.
7. Bench-test PTT and AFSK audio before connecting the transmitter to an antenna.
8. Only after all settings have been verified, set `CONFIGURATION_CONFIRMED = true`, compile again, and upload.

## User configuration

Normal sysop settings are grouped near the top of `Arduino_APRS_Digipeater_2026.ino`.
The public template starts like this:

```cpp
const bool CONFIGURATION_CONFIRMED = false;

const char STATION_CALLSIGN[] = "NOCALL";
const uint8_t STATION_SSID = 15;

const char APRS_TOCALL[] = "APZDIY";
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

- `Arduino_APRS_Digipeater_2026.ino` - user configuration, state, `setup()` and `loop()`
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
