#ifndef LIBAPRS_DIGI_H
#define LIBAPRS_DIGI_H

#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

#include "FIFO.h"
#include "CRC-CCIT.h"
#include "HDLC.h"
#include "AFSK.h"
#include "AX25.h"

// Minimal API used by Arduino_APRS_Digipeater_2026.
void APRS_init(int reference, bool open_squelch);
void APRS_poll(void);
bool APRS_channelBusy(void);
void APRS_resetReceiver(void);
void APRS_sendViaH(const AX25Call *path, size_t path_len,
                   uint8_t repeated_mask, const void *buffer, size_t length);

void APRS_setPreamble(unsigned long pre);
void APRS_setTail(unsigned long tail);
bool APRS_isSending(void);

int freeMemory();

#endif
