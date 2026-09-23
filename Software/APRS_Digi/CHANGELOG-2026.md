# 2026 port notes

## From the working SV3GKD beacon tracker

Retained / adapted:

- D3 active-HIGH PTT
- D4-D7 resistor DAC order: 8k2 / 3k9 / 2k2 / 1k
- D8 manual push button behavior
- 350 ms AFSK preamble
- 80 ms AFSK tail
- 150 ms radio key-up delay
- 1000 ms minimum TX window
- 30 ms post-TX PTT hold
- 4000 ms TX safety timeout
- Arduino Uno / ATmega328P target

Changed for digipeater operation:

- A2 is now the actual 1200-baud AFSK receive ADC, not just a separate `analogRead()` carrier detector.
- channel activity is derived from the continuous 9600 Hz ADC stream.
- DAC ISR preserves D0-D3 so D3 PTT can coexist with D4-D7 DAC.
- receive and transmit AX.25 path handling includes H bits.
- added 30 s duplicate cache.
- added traceable WIDE1-1 / WIDE2-n handling.
- source, destination and information are preserved during digipeating.
- legacy aliases are not serviced.

## Station defaults

- `SV3GKD-15`
- coordinates `3811.24N / 02142.38E`
- `/#` digipeater symbol
- `PHG6750`
- comment `/W2 DiGi 144.800 in memory of SV3CYL`
- 15 minute base beacon interval
- proportional own-beacon paths: DIRECT / WIDE2-1 / DIRECT / WIDE2-2
