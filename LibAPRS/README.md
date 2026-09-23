# Patched LibAPRS for Fixed-Position APRS Beacon

This directory contains a patched copy of LibAPRS used by the Arduino fixed-position APRS beacon project.

It is included directly in this repository so the project can be built with the exact library version and hardware mapping expected by the sketch.

## Purpose of This Copy

This library copy is not a general-purpose upstream replacement.

It is patched specifically for:

* Arduino Uno operation
* fixed-position APRS beaconing
* raw APRS packet transmission
* clean APRS path handling
* PTT output on D3
* 4-bit AFSK DAC output on D4-D7
* TX LED on D13

## Main Changes

### Optional APRS Path Handling

The original library behavior always populated two digipeater path entries.

This copy allows the sketch to use only the required path, for example:

```text
WIDE2-1
```

instead of forcing:

```text
WIDE1-1,WIDE2-2
```

This is important for a fixed beacon where a short and conservative APRS path is preferred.

### Raw Packet Beacon Support

The beacon sketch sends the APRS information field directly using:

```cpp
APRS_sendPkt()
```

instead of using GPS-dependent location helper functions.

Example APRS information field:

```text
!3814.80N/02144.08E-2nd QTH Patras
```

### Transmission State Helper

A helper was added so the sketch can wait until APRS transmission has finished before continuing.

### Symbol Handling Fix

A minor symbol table comparison bug was corrected.

### Arduino Uno Hardware Mapping

Default mapping used by this project:

| Arduino pin | Function                               |
| ----------: | -------------------------------------- |
|          D3 | PTT output                             |
|       D4-D7 | 4-bit AFSK DAC                         |
|          A0 | ADC audio input, unused by this beacon |
|         D13 | TX LED                                 |

## How This Library Is Used

The sketch configures APRS parameters such as:

```cpp
APRS_setCallsign(APRS_CALLSIGN, APRS_SSID);
APRS_setDestination((char *)"APZARD", 0);
APRS_setPath1((char *)"WIDE2", 1);
APRS_setPath2((char *)"", 0);
```

Then it sends a fixed APRS position packet:

```cpp
APRS_sendPkt((void *)APRS_POSITION, strlen(APRS_POSITION));
```

## Notes

This copy is intended to be kept with the project source tree.

Do not replace it blindly with upstream LibAPRS unless the project sketch and pin mapping are reviewed again.

## Hardware Warning

The PTT output must not be connected directly to a radio PTT input unless the radio interface is known to be safe.

Use a transistor or optocoupler interface.

The AFSK DAC output must be attenuated before feeding a radio microphone input.
