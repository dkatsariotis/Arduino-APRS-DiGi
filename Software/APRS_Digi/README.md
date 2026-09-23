# SV3GKD-15 APRS Digipeater 2026

Source-only Arduino IDE project for an Arduino Uno / ATmega328P APRS 1200-baud digipeater.
No precompiled HEX is required.

The project is tailored to the existing SV3GKD hardware:

- RX audio: **A2 / ADC2**
- PTT: **D3**, active HIGH
- 4-bit AFSK DAC:
  - D4 = 8k2
  - D5 = 3k9
  - D6 = 2k2
  - D7 = 1k
- manual beacon push button: **D8 to GND**, using `INPUT_PULLUP`
- TX LED: **D13**

The modem / AX.25 code is bundled under `src/LibAPRS_Digi`, so there is no separate Library Manager dependency.
You only need the normal **Arduino AVR Boards** core and select **Arduino Uno** in Arduino IDE.

## Open in Arduino IDE

1. Keep the whole folder named `SV3GKD_APRS_Digi_2026`.
2. Open `SV3GKD_APRS_Digi_2026.ino`.
3. Select **Tools -> Board -> Arduino AVR Boards -> Arduino Uno**.
4. Select the correct serial port.
5. First use **Sketch -> Verify/Compile**.
6. Bench-test PTT/audio before connecting the radio to an antenna.
7. Upload only after the configuration shown on Serial Monitor is correct.

## Normal configuration

Almost everything normally changed by the sysop is at the top of
`SV3GKD_APRS_Digi_2026.ino`, between:

```cpp
// USER CONFIGURATION
...
// END USER CONFIGURATION
```

The important settings are:

```cpp
const char STATION_CALLSIGN[] = "SV3GKD";
const uint8_t STATION_SSID = 15;

const char APRS_TOCALL[] = "APZ3GK";

const double STATION_LATITUDE = 38.1873919;
const double STATION_LONGITUDE = 21.70633239;

const char APRS_SYMBOL_TABLE = '/';
const char APRS_SYMBOL_CODE = '#';

const char APRS_PHG[] = "PHG6750";

const bool BEACON_ALTERNATE_COMMENTS = true;
const char APRS_COMMENT_1[] = "/MINTILOGLI-PATRAS/ASL.22m/";
const char APRS_COMMENT_2[] = "/in memory of SV3CYL SK";

const uint16_t BEACON_INTERVAL_MINUTES = 15;
```

The decimal position is converted automatically to the classic APRS on-air form:

```text
3811.24N / 02142.38E
```

The first periodic/boot beacon therefore has an information field such as:

```text
!3811.24N/02142.38E#PHG6750/MINTILOGLI-PATRAS/ASL.22m/

Alternating memorial beacon:

```text
!3811.24N/02142.38E#PHG6750/in memory of SV3CYL SK
```
```

The next successful periodic beacon uses `APRS_COMMENT_2`, then the sequence loops.

### Symbol / icon

The APRS symbol is the combination of:

```cpp
APRS_SYMBOL_TABLE
APRS_SYMBOL_CODE
```

For a normal APRS digipeater this project currently uses `/#`.

### PHG

`APRS_PHG` is deliberately a complete APRS PHG string instead of four generator fields.
This allows the existing RF installation value to remain exactly:

```text
PHG6750
```

### Own beacon path

Three modes are available:

```cpp
BEACON_PATH_DIRECT
BEACON_PATH_FIXED
BEACON_PATH_PROPORTIONAL
```

Default is `BEACON_PATH_PROPORTIONAL`.
At a 15 minute base interval the sequence is:

```text
15 min  DIRECT
30 min  WIDE2-1
45 min  DIRECT
60 min  WIDE2-2
repeat
```

If a single fixed path is preferred, select `BEACON_PATH_FIXED` and edit:

```cpp
BEACON_FIXED_PATH1_CALL
BEACON_FIXED_PATH1_SSID
BEACON_FIXED_PATH2_CALL
BEACON_FIXED_PATH2_SSID
```

For example, `WIDE2-1` is represented as:

```cpp
const char BEACON_FIXED_PATH1_CALL[] = "WIDE2";
const uint8_t BEACON_FIXED_PATH1_SSID = 1;
```

## Digipeater behavior

The forwarding code is intentionally separate from the old 2010s ExtDigi logic.
It implements a conservative current New-N style profile:

- only the **first unused** AX.25 repeater address is serviced
- explicit routing to `SV3GKD-15` is supported
- `WIDE1-1` is traceable
- `WIDE2-1` is traceable
- `WIDE2-2` becomes `SV3GKD-15*,WIDE2-1`
- the original source, destination and APRS information field are preserved
- H / has-been-repeated bits are generated correctly in outgoing via addresses
- duplicate suppression is **30 seconds**, with the via path excluded from the duplicate fingerprint
- the digi will not retransmit a packet which already contains `SV3GKD-15*`
- RF paths containing `TCPIP` or `TCPXX` are rejected
- obsolete `RELAY`, bare `WIDE`, and `TRACE` aliases are not serviced
- excessive `WIDE3-n` through `WIDE7-n` requests are, by default, trapped to one local hop instead of being allowed to propagate
- trailing path markers such as `NOGATE` / `RFONLY` are preserved; they are not treated as digipeater aliases

The normal wide-area limit is configured with:

```cpp
const uint8_t DIGI_MAX_WIDE_N = 2;
const bool DIGI_TRAP_LARGE_N = true;
const uint32_t DIGI_DUPLICATE_WINDOW_MS = 30000UL;
```

## Manual beacon button on D8

The button behavior is carried over from the working SV3GKD tracker project:

- D8 uses `INPUT_PULLUP`
- button connects D8 to GND
- 50 ms debounce
- press must last at least 700 ms
- beacon is requested when the button is released
- holding longer than 5 seconds is treated as stuck/noisy and locks the button until release
- one valid button action causes one TX attempt
- after a successful manual beacon, the automatic beacon timer is restarted

The default manual beacon path is `WIDE2-1` and can be changed independently.

## Radio / AFSK timing

The initial values are carried over from the working beacon-only tracker for this radio/interface:

```cpp
APRS_PREAMBLE_MS = 350
APRS_TAIL_MS = 80
RADIO_KEYUP_MS = 150
TX_MIN_TOTAL_MS = 1000
TX_POST_HOLD_MS = 30
TX_TIMEOUT_MS = 4000
```

These are editable in the same USER CONFIGURATION section.
Do not shorten them until the radio is tested with another APRS decoder and actual RF deviation has been checked.

## A2 receive audio and listen-before-transmit

The original tracker used its audio input only for channel activity checking. A digipeater must also decode APRS from the same audio input, so the bundled LibAPRS modem has been modified to use **ADC2 / A2** continuously for AFSK receive.

The channel-busy detector therefore uses the already-running ADC sample stream instead of calling `analogRead(A2)`, which would disturb the 9600 Hz modem sampling.

It can be disabled for bench testing with:

```cpp
const bool CHANNEL_BUSY_DETECT_ENABLED = false;
```

For normal RF digipeater operation it should remain `true`.

## D3 PTT and D4-D7 DAC

The original modem ISR wrote the full `PORTD`, which is incompatible with PTT on D3 because the DAC uses D4-D7 on the same AVR port.
The bundled library has been changed so the DAC ISR updates only the high nibble and preserves D0-D3.
This allows D3 PTT to remain asserted during AFSK output.

If the physical hardware pinout changes later, D3 and D8 are configurable in the main `.ino`.
A2 and the D4-D7 resistor ladder are low-level modem mappings and are defined in:

```text
src/LibAPRS_Digi/device.h
```

## TOCALL

`APBK??` is allocated to PY5BK / Bravo Tracker, so this independent firmware does not identify itself as `APBK19`.
The current sketch uses `APZ3GK` as an experimental/local identifier.
If this firmware is published for general use, request a dedicated TOCALL from the current APRS device-ID registry rather than treating the experimental value as a permanent allocation.

## Files / tabs

- `SV3GKD_APRS_Digi_2026.ino` - user configuration, state, `setup()` and `loop()`
- `05_utils.ino` - configuration validation and AX.25 helpers
- `10_radio.ino` - PTT, clear-channel logic and TX safety
- `20_beacon.ino` - fixed position beacon and path scheduler
- `30_digipeater.ino` - New-N path processing and duplicate suppression
- `40_button.ino` - D8 manual beacon button
- `50_diagnostics.ino` - Serial Monitor output
- `src/LibAPRS_Digi/` - bundled / modified AFSK and AX.25 modem library

## Validation done before packaging

The high-level Arduino sketch was syntax-checked with C++ stubs and the actual digipeater path engine was host-tested for:

- `WIDE1-1`
- `WIDE2-1`
- `WIDE2-2`
- a previously-used digi followed by `WIDE2-1` / `WIDE2-2`
- explicit `SV3GKD-15`
- large-N trap
- loop rejection
- own-source rejection while allowing another SV3GKD SSID
- `TCPIP` rejection
- preservation of trailing `NOGATE`
- duplicate fingerprints ignoring the via path

An AVR toolchain is not available in the packaging environment, so the final ATmega328P compile must still be performed with **Verify** in Arduino IDE before upload.

## References / provenance

The modem layer is derived from the LibAPRS / Arduino APRS modem code included in the user's previously working tracker project and retains its included license file.
The digipeater forwarding logic in this project is newly structured around current APRS/New-N behavior rather than copying the old ExtDigi forwarding callback.

Useful current references:

- APRS Documentation Project: https://github.com/wb2osz/aprsspec
- APRS Device ID allocations: https://github.com/aprsorg/aprs-deviceid
- New-N / traceable WIDEn-N background: https://www.aprs.org/fix14439.html
