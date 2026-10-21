# Arduino APRS Digipeater

A source-based 1200-baud AFSK / AX.25 APRS digipeater for **Arduino Uno (ATmega328P)** and a compatible VHF FM transceiver.

This repository contains the **generic public project**. Its sketch defaults are intended to be reviewed and configured by each operator before any RF transmission.

## Project files

The Arduino IDE sketch and its full documentation are in:

**[Software/APRS_Digi](Software/APRS_Digi/)**

Start with the [sketch README](Software/APRS_Digi/README.md). It covers configuration, pinout, beacon behavior, digipeater policy, diagnostics, safety features, and compilation.

Additional project material:
- [Hardware / EasyEDA files](EasyEDA/)
- [Software source](Software/APRS_Digi/)

## Main features

- 1200-baud AFSK and AX.25 packet handling.
- New-N-style digipeating for supported WIDE paths.
- Configurable periodic position beacons and path modes.
- Manual beacon button.
- Channel-busy handling, duplicate suppression, and TX safety timeouts.
- Optional serial diagnostics and unattended-operation watchdog/failsafes.

See the sketch README and source for the exact behavior of the current revision.

## Hardware overview

Reference pin mapping for the Arduino Uno:

| Function | Arduino pin |
|---|---|
| RX audio | A2 / ADC2 |
| PTT control | D3, active HIGH at the sketch interface |
| 4-bit AFSK DAC | D4–D7 |
| Manual beacon button | D8 to GND |
| TX LED | D13 |

The radio interface must provide suitable audio coupling and a safe PTT switching stage. Do not connect a transceiver's PTT or audio lines directly to Arduino pins unless the interface circuitry explicitly supports it.

## Build and upload

1. Download or clone this repository.
2. Keep the complete `Software/APRS_Digi/` sketch directory and its `src/LibAPRS_Digi/` library together.
3. Open `Software/APRS_Digi/APRS_Digi.ino` in the Arduino IDE.
4. Select **Arduino Uno** under **Arduino AVR Boards**.
5. Read [Software/APRS_Digi/README.md](Software/APRS_Digi/README.md) and review the user configuration.
6. Compile and bench-test the modem, audio, and PTT interface.
7. Enable RF transmission only after entering valid station details and explicitly confirming the configuration in the sketch.

Do not copy only the `.ino` tabs: the bundled modem library is required.

## Important

- The public template is not ready to transmit until the station configuration has been reviewed and confirmed.
- Check the local band plan, applicable amateur-radio regulations, callsign, APRS paths, and beacon interval before operating.
- Compile and test the exact revision you intend to use; do not assume a repository update has been RF-tested.

## License and attribution

See [Software/APRS_Digi/LICENSE-NOTICE.md](Software/APRS_Digi/LICENSE-NOTICE.md) and the license files distributed with the bundled library. Preserve applicable upstream notices when redistributing the project.
