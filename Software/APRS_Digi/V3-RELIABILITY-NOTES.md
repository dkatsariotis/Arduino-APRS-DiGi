# Reliability v3 notes

- AVR hardware watchdog: 8 s
- pending-relay hard failsafe: 5 s
- existing normal relay defer limit: 1.2 s
- TX timeout: 4 s, followed by explicit AFSK modem/FIFO abort and PTT OFF
- optional 60-second health heartbeat when `ENABLE_SERIAL_DIAGNOSTICS=1`
- diagnostic counters compile out when `ENABLE_SERIAL_DIAGNOSTICS=0`
- `DIGI TX` diagnostics print the complete payload
