/*
  Arduino APRS Digipeater 2026 - Arduino Uno / ATmega328P

  Self-contained Arduino IDE project.

  Hardware used by this build:
    A2      RX audio (ADC2)
    D3      PTT, active HIGH
    D4-D7   4-bit AFSK DAC: D4=8k2, D5=3k9, D6=2k2, D7=1k
    D8      Manual beacon push button to GND (INPUT_PULLUP)
    D13     TX LED (handled by LibAPRS_Digi)

  IMPORTANT:
    The section marked USER CONFIGURATION is the normal place to edit the
    station. The other tabs contain modem, digipeater, beacon and button logic.
*/

#include "src/LibAPRS_Digi/LibAPRS.h"
#include <avr/wdt.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define ADC_REFERENCE REF_5V
#define OPEN_SQUELCH false

// Master compile-time serial diagnostics switch.
// 0 = no Arduino Serial dependency/buffers; 1 = Serial Monitor diagnostics.
#define ENABLE_SERIAL_DIAGNOSTICS 0

#if ENABLE_SERIAL_DIAGNOSTICS
  #define DBG_BEGIN(...)    Serial.begin(__VA_ARGS__)
  #define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DBG_WRITE(...)    Serial.write(__VA_ARGS__)
#else
  #define DBG_BEGIN(...)    do { } while (0)
  #define DBG_PRINT(...)    do { } while (0)
  #define DBG_PRINTLN(...)  do { } while (0)
  #define DBG_WRITE(...)    do { } while (0)
#endif

// =============================================================================
// USER CONFIGURATION - edit this section in Arduino IDE
// =============================================================================

// ---- Station identity --------------------------------------------------------
// Safe public-repository default. RF TX stays disabled until you have edited
// the station settings and explicitly changed CONFIGURATION_CONFIRMED to true.
const bool CONFIGURATION_CONFIRMED = false;

const char STATION_CALLSIGN[] = "NOCALL";  // change to your licensed callsign
const uint8_t STATION_SSID = 15;           // choose an SSID appropriate for your station

// APZxxx is the APRS experimental/development TOCALL family. If this firmware
// becomes a named/distributed product, obtain/use an appropriate registered
// device identifier instead of treating this placeholder as a permanent ID.
const char APRS_TOCALL[] = "APZDIY";
const uint8_t APRS_TOCALL_SSID = 0;

// ---- Position / symbol / PHG / comments -------------------------------------
// Enter ordinary decimal degrees here. South/West are negative.
// The firmware converts them at startup to classic uncompressed APRS
// DDMM.mmN/S and DDDMM.mmE/W coordinates.
// 0.0 / 0.0 is intentionally only a placeholder for the public template.
const double STATION_LATITUDE = 0.0;
const double STATION_LONGITUDE = 0.0;

// Digipeater symbol = /# (primary table + '#').
const char APRS_SYMBOL_TABLE = '/';
const char APRS_SYMBOL_CODE = '#';

// Optional PHG extension. Leave empty until you have calculated/verified it.
// If used, keep the complete seven-character field here, e.g. "PHG5130".
// It is transmitted immediately after the APRS symbol, with no added space.
const char APRS_PHG[] = "";

// Two alternating position comments. The first successful periodic/boot beacon
// uses COMMENT_1, the next COMMENT_2, then COMMENT_1 again, and so on.
// Keep APRS_PHG + each comment <= 43 characters for the classic position-comment
// field. Set BEACON_ALTERNATE_COMMENTS=false to always use COMMENT_1.
const bool BEACON_ALTERNATE_COMMENTS = true;
const char APRS_COMMENT_1[] = "/W2 APRS DIGI";
const char APRS_COMMENT_2[] = "/Arduino APRS Digipeater";

// ---- Periodic beacon ---------------------------------------------------------
const uint16_t BEACON_INTERVAL_MINUTES = 15;
const uint16_t BEACON_INITIAL_DELAY_SECONDS = 60;
const uint16_t BEACON_JITTER_SECONDS = 20;  // 0 = exact interval
const bool BEACON_ON_BOOT = true;
const uint16_t BEACON_RETRY_AFTER_BUSY_SECONDS = 60;

// Path mode for the digipeater's own position beacon.
enum BeaconPathMode : uint8_t {
  BEACON_PATH_DIRECT = 0,
  BEACON_PATH_FIXED = 1,
  BEACON_PATH_PROPORTIONAL = 2
};
const BeaconPathMode BEACON_PATH_MODE = BEACON_PATH_PROPORTIONAL;

// Used only when BEACON_PATH_MODE == BEACON_PATH_FIXED.
// Empty callsign = unused path entry.
const char BEACON_FIXED_PATH1_CALL[] = "WIDE2";
const uint8_t BEACON_FIXED_PATH1_SSID = 1;
const char BEACON_FIXED_PATH2_CALL[] = "";
const uint8_t BEACON_FIXED_PATH2_SSID = 0;

// Proportional pathing sequence at each base beacon interval:
//   1: DIRECT, 2: WIDE2-1, 3: DIRECT, 4: WIDE2-2, repeat.
const char BEACON_ONE_HOP_CALL[] = "WIDE2";
const uint8_t BEACON_ONE_HOP_SSID = 1;
const char BEACON_TWO_HOP_CALL[] = "WIDE2";
const uint8_t BEACON_TWO_HOP_SSID = 2;

// Manual D8 beacon path. One hop is useful for an end-to-end RF test.
const char MANUAL_PATH1_CALL[] = "WIDE2";
const uint8_t MANUAL_PATH1_SSID = 1;
const char MANUAL_PATH2_CALL[] = "";
const uint8_t MANUAL_PATH2_SSID = 0;

// ---- Digipeater behavior -----------------------------------------------------
const bool DIGI_ENABLED = true;

// Wide-area New-N profile: normal processing up to WIDE2-2.
const uint8_t DIGI_MAX_WIDE_N = 2;

// WIDE3-n ... WIDE7-n are trapped to one local hop instead of propagating
// excessive paths. Set false to reject them completely.
const bool DIGI_TRAP_LARGE_N = true;

// Current APRS practice uses a roughly 30 second duplicate-suppression window.
const uint32_t DIGI_DUPLICATE_WINDOW_MS = 30000UL;

// Drop RF packets which contain Internet-only TCPIP/TCPXX path markers.
const bool DIGI_BLOCK_TCP_PATHS = true;

// Relay timing. If another signal starts immediately after a received frame,
// the relay is deferred briefly; if the channel stays busy too long it is
// dropped rather than transmitted late into unrelated traffic.
const uint16_t DIGI_RANDOM_HOLDOFF_MIN_MS = 10;
const uint16_t DIGI_RANDOM_HOLDOFF_MAX_MS = 70;
const uint16_t DIGI_MAX_DEFER_MS = 1200;
const uint16_t DIGI_CHANNEL_WAIT_MS = 250;

// Independent failsafe for the zero-copy pending relay. This is intentionally
// longer than DIGI_MAX_DEFER_MS and is checked even if a modem/TX state flag
// would otherwise make serviceDigipeater() return early.
const uint16_t DIGI_PENDING_FAILSAFE_MS = 5000;

// ---- AFSK / radio timing -----------------------------------------------------
// Conservative starting values for the reference hardware. Tune only after bench/RF testing.
const unsigned long APRS_PREAMBLE_MS = 350;
const unsigned long APRS_TAIL_MS = 80;
const unsigned long RADIO_KEYUP_MS = 150;
const unsigned long TX_MIN_TOTAL_MS = 1000;
const unsigned long TX_POST_HOLD_MS = 30;
const unsigned long TX_TIMEOUT_MS = 4000;

// Beacon may wait longer for a clear channel than a digipeated packet.
const unsigned long BEACON_MAX_CHANNEL_WAIT_MS = 30000UL;
const bool CHANNEL_BUSY_DETECT_ENABLED = true;
const unsigned long CHANNEL_CLEAR_HOLD_MS = 80UL;

// ---- Manual beacon button ----------------------------------------------------
const uint8_t PTT_PIN = 3;
const bool PTT_ACTIVE_HIGH = true;
const uint8_t BUTTON_PIN = 8;
const unsigned long BUTTON_MIN_PRESS_MS = 700;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long BUTTON_HOLD_TIMEOUT_MS = 5000;

// ---- Diagnostics / safety ----------------------------------------------------
// Set to 0 for an unattended production digi on ATmega328P. When disabled,
// every debug print is removed at compile time and Arduino HardwareSerial is
// not referenced by this sketch, freeing its SRAM buffers.
const uint32_t SERIAL_BAUD = 115200UL;
const bool SERIAL_LOG_RX_PACKETS = true;
const bool SERIAL_LOG_DROPS = true;
// Serial text literals already use F(), so they stay in flash rather than SRAM.
// Set these logging flags false if you want less serial/flash overhead; the
// main SRAM saving comes from the lean LibAPRS_Digi wrapper, not from prints.
// 8 s AVR hardware watchdog. Keep this enabled for unattended operation.
// It catches hard stalls such as a blocked modem TX FIFO or a parser deadlock.
const bool ENABLE_WATCHDOG = true;

// Printed only when ENABLE_SERIAL_DIAGNOSTICS=1. No Serial dependency or
// heartbeat counters are retained in the production build when diagnostics=0.
const uint16_t HEALTH_HEARTBEAT_SECONDS = 60;

// =============================================================================
// END USER CONFIGURATION
// =============================================================================

#define DIGI_DUPE_CACHE_SIZE 16
#define DIGI_MAX_INFO_LEN 256
#define BEACON_INFO_BUFFER_SIZE 72

// SRAM-compact duplicate entry. A 16-bit seconds counter is sufficient for
// a 30-second duplicate window and remains wrap-safe with uint16_t subtraction.
struct DupeEntry {
  uint32_t hash;
  uint16_t whenSec;
};

struct RelayFrame {
  AX25Call path[AX25_MAX_RPT + 2];
  uint8_t pathLen;
  uint8_t repeatedMask;

  // Zero-copy relay: info points into the AX25 receive buffer. While a relay
  // is pending we intentionally do not parse another frame, so this memory
  // remains valid until the packet has been transmitted or dropped.
  const uint8_t *info;
  uint16_t infoLen;

  uint32_t hash;
  unsigned long queuedAtMs;
  unsigned long notBeforeMs;
};

enum DigiDecision : uint8_t {
  DIGI_NO_PATH = 0,
  DIGI_OWN_SOURCE,
  DIGI_LOOP,
  DIGI_TCP_PATH,
  DIGI_UNSUPPORTED,
  DIGI_EXPLICIT,
  DIGI_WIDE_REPLACE,
  DIGI_WIDE_INSERT,
  DIGI_WIDE_TRAP,
  DIGI_TOO_LONG,
  DIGI_QUEUE_BUSY,
  DIGI_DUPLICATE
};

// Runtime state.
RelayFrame pendingRelay;
bool relayPending = false;
bool radioTxActive = false;
bool manualBeaconRequested = false;
bool buttonLockout = false;
bool stationConfigValid = false;
bool positionConfigValid = false;
DupeEntry dupeCache[DIGI_DUPE_CACHE_SIZE];
uint8_t dupeCacheNext = 0;

unsigned long nextBeaconAt = 0;
uint8_t beaconSequence = 0;

// Reset flags captured at the start of setup(), before enabling our watchdog.
uint8_t bootResetFlags = 0;

#if ENABLE_SERIAL_DIAGNOSTICS
struct RuntimeStats {
  uint32_t rxPackets;
  uint32_t digiQueued;
  uint32_t digiTx;
  uint32_t duplicateDrops;
  uint32_t digiDrops;
  uint32_t relayFailsafeDrops;
  uint32_t txTimeouts;
  uint32_t beaconTx;
  uint32_t beaconFailed;
};
RuntimeStats runtimeStats = {0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long lastHealthAtMs = 0;
#define STAT_INC(field) do { runtimeStats.field++; } while (0)
#else
#define STAT_INC(field) do { } while (0)
#endif

// Forward declarations.
void forcePttOff();
void forcePttOn();
bool waitForClearChannel(unsigned long maxWaitMs, bool preserveRxFrame);
bool transmitFrame(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask,
                   const uint8_t *info, uint16_t infoLen,
                   unsigned long maxChannelWaitMs, bool preserveRxFrame = false);

void aprs_msg_callback(struct AX25Msg *msg);
void serviceDigipeater();
void initDigipeater();

bool sendPeriodicBeacon();
bool sendManualBeacon();
void scheduleNextBeacon();
void serviceBeaconScheduler();

void handleManualButton();

void printConfiguration();
void printBootResetCause();
void serviceHealthDiagnostics();
void printPacket(const AX25Msg *msg);
void printPath(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask);
const __FlashStringHelper *digiDecisionText(DigiDecision decision);

bool configLooksSafe();
bool formatAprsCoordinates(double latitude, double longitude,
                           char latOut[9], char lonOut[10]);
void setAx25Call(AX25Call &dst, const char *call, uint8_t ssid);
bool sameAx25Call(const AX25Call &a, const char *call, uint8_t ssid);

void setup() {
  // If the previous run ended in a watchdog reset, make sure the watchdog is
  // disabled while setup() initializes the modem and prints diagnostics.
  bootResetFlags = MCUSR;
  MCUSR = 0;
  wdt_disable();

  forcePttOff();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  DBG_BEGIN(SERIAL_BAUD);
  delay(100);

  randomSeed(micros() ^ analogRead(A5));

  // Validate that decimal coordinates can be represented in classic APRS
  // format. Keep the temporary strings on the stack instead of permanently
  // consuming SRAM.
  char latCheck[9];
  char lonCheck[10];
  positionConfigValid = formatAprsCoordinates(STATION_LATITUDE, STATION_LONGITUDE,
                                              latCheck, lonCheck);

  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setPreamble(APRS_PREAMBLE_MS);
  APRS_setTail(APRS_TAIL_MS);

  initDigipeater();
  stationConfigValid = positionConfigValid && configLooksSafe();
  printBootResetCause();
  printConfiguration();

  if (!stationConfigValid) {
    DBG_PRINTLN(F("CONFIG ERROR: RF transmission disabled until settings are corrected."));
    nextBeaconAt = ULONG_MAX;
  } else {
    if (BEACON_ON_BOOT) {
      nextBeaconAt = millis() + ((unsigned long)BEACON_INITIAL_DELAY_SECONDS * 1000UL);
    } else {
      scheduleNextBeacon();
    }
  }

  if (ENABLE_WATCHDOG) {
    wdt_enable(WDTO_8S);
  }

  DBG_PRINTLN(F("Arduino APRS Digipeater 2026 ready."));
}

void loop() {
  if (ENABLE_WATCHDOG) {
    wdt_reset();
  }

  serviceHealthDiagnostics();

  // Parse at most one complete received frame per call. When a relay is
  // pending, its information field points directly into the AX25 RX buffer,
  // so do not parse another frame until that relay is sent or dropped. The
  // AFSK ISR continues collecting bytes in the RX FIFO during the short holdoff.
  if (!relayPending) {
    APRS_poll();
  }

  // Invalid user configuration is receive/diagnostic-only: never key PTT.
  if (!stationConfigValid) {
    forcePttOff();
    return;
  }

  // RF relays have priority over local beacons.
  serviceDigipeater();

  handleManualButton();

  if (!relayPending && !radioTxActive) {
    if (manualBeaconRequested) {
      // Preserve the working tracker behavior: one button action = one TX
      // attempt. If the channel is busy, press the button again later.
      bool sent = sendManualBeacon();
      manualBeaconRequested = false;
      if (sent) {
        scheduleNextBeacon();
      }
    } else {
      serviceBeaconScheduler();
    }
  }
  serviceRxAudioDiagnostics();
}
