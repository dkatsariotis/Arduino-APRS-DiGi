#include "Arduino.h"
#include "AFSK.h"
#include "AX25.h"
#include <stdint.h>

// Lean LibAPRS wrapper for the APRS digipeater build.
// The original LibAPRS tracker/location/message globals were intentionally
// removed because this project builds its own AX.25 path and information field
// and sends them through APRS_sendViaH(). On ATmega328P this saves a useful
// amount of the 2 KB SRAM without changing the AFSK/AX.25 modem core.

Afsk modem;
AX25Ctx AX25;
extern void aprs_msg_callback(struct AX25Msg *msg);

int LibAPRS_vref = REF_3V3;
bool LibAPRS_open_squelch = false;

unsigned long custom_preamble = 350UL;
unsigned long custom_tail = 50UL;

void APRS_init(int reference, bool open_squelch) {
    LibAPRS_vref = reference;
    LibAPRS_open_squelch = open_squelch;

    AFSK_init(&modem);
    ax25_init(&AX25, aprs_msg_callback);
}

void APRS_poll(void) {
    ax25_poll(&AX25);
}

bool APRS_channelBusy(void) {
    return AFSK_channelBusy();
}

void APRS_resetReceiver(void) {
    AFSK_flushRx(&modem);
    ax25_reset(&AX25);
}

void APRS_sendViaH(const AX25Call *path, size_t path_len, uint8_t repeated_mask,
                   const void *buffer, size_t length) {
    ax25_sendViaH(&AX25, path, path_len, repeated_mask, buffer, length);
}

void APRS_setPreamble(unsigned long pre) {
    custom_preamble = pre;
}

void APRS_setTail(unsigned long tail) {
    custom_tail = tail;
}

bool APRS_isSending(void) {
    return modem.sending;
}

// Runtime free-SRAM diagnostic. This is not part of the modem state and costs
// no large persistent buffer.
extern unsigned int __heap_start;
extern void *__brkval;

struct __freelist {
  size_t sz;
  struct __freelist *nx;
};

extern struct __freelist *__flp;

static int freeListSize() {
  struct __freelist* current;
  int total = 0;
  for (current = __flp; current; current = current->nx) {
    total += 2;
    total += (int) current->sz;
  }
  return total;
}

int freeMemory() {
  int free_memory;
  intptr_t stack_addr = (intptr_t)&free_memory;
  intptr_t heap_start_addr = (intptr_t)&__heap_start;
  intptr_t brk_addr = (intptr_t)__brkval;

  if (brk_addr == 0) {
    free_memory = (int)(stack_addr - heap_start_addr);
  } else {
    free_memory = (int)(stack_addr - brk_addr);
    free_memory += freeListSize();
  }
  return free_memory;
}
