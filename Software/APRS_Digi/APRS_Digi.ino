/*
  SV3GKD APRS Digipeater 2026 - Arduino Uno / ATmega328P

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

// =============================================================================
// USER CONFIGURATION - edit this section in Arduino IDE
// =============================================================================

// ---- Station identity --------------------------------------------------------
const char STATION_CALLSIGN[] = "SV3GKD";
const uint8_t STATION_SSID = 15;

// APBK?? is allocated to PY5BK Bravo Tracker. This independent firmware uses
// an APZ... experimental TOCALL until/unless a dedicated identifier is assigned.
const char APRS_TOCALL[] = "APZ3GK";
const uint8_t APRS_TOCALL_SSID = 0;

// ---- Position / symbol / PHG / comments -------------------------------------
// Enter ordinary decimal degrees here. South/West are negative.
// The firmware converts these to standard APRS uncompressed DDMM.mm/DDDMM.mm
// coordinates at startup. With the values below the transmitted position is:
//   3811.24N / 02142.38E
// This is about 6.5 m from the supplied decimal coordinate because the classic
// uncompressed APRS format has 0.01 minute position resolution.
const double STATION_LATITUDE = 38.1873919;
const double STATION_LONGITUDE = 21.70633239;

// Digipeater symbol = /# (primary table + '#').
const char APRS_SYMBOL_TABLE = '/';
const char APRS_SYMBOL_CODE = '#';

// Keep PHG immediately after the symbol, with no space.
const char APRS_PHG[] = "PHG6750";

// Two alternating position comments. The first successful periodic/boot beacon
// uses COMMENT_1, the next COMMENT_2, then COMMENT_1 again, and so on.
// Keep APRS_PHG + each comment <= 43 characters for the classic position-comment
// field. COMMENT_1 mirrors the old site/ASL beacon; COMMENT_2 is the memorial.
const bool BEACON_ALTERNATE_COMMENTS = true;
const char APRS_COMMENT_1[] = "/MINTILOGLI-PATRAS/ASL.22m/";
const char APRS_COMMENT_2[] = "/in memory of SV3CYL SK";

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

// ---- AFSK / radio timing -----------------------------------------------------
// These values come from the working fixed-beacon project for this hardware.
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
const uint32_t SERIAL_BAUD = 115200UL;
const bool SERIAL_LOG_RX_PACKETS = true;
const bool SERIAL_LOG_DROPS = true;
const bool ENABLE_WATCHDOG = false;  // enable only after bench testing

// =============================================================================
// END USER CONFIGURATION
// =============================================================================

#define DIGI_DUPE_CACHE_SIZE 16
#define DIGI_MAX_INFO_LEN 256
#define BEACON_INFO_BUFFER_SIZE 100

struct DupeEntry {
  uint32_t hash;
  unsigned long whenMs;
  bool valid;
};

struct RelayFrame {
  AX25Call path[AX25_MAX_RPT + 2];
  uint8_t pathLen;
  uint8_t repeatedMask;
  uint8_t info[DIGI_MAX_INFO_LEN];
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
char aprsLat[9] = "";
char aprsLon[10] = "";

DupeEntry dupeCache[DIGI_DUPE_CACHE_SIZE];
uint8_t dupeCacheNext = 0;

unsigned long nextBeaconAt = 0;
uint8_t beaconSequence = 0;

uint32_t statRxPackets = 0;
uint32_t statDigiTx = 0;
uint32_t statBeaconTx = 0;
uint32_t statDuplicates = 0;
uint32_t statDropped = 0;

// Forward declarations.
void forcePttOff();
void forcePttOn();
bool waitForClearChannel(unsigned long maxWaitMs);
bool transmitFrame(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask,
                   const uint8_t *info, uint16_t infoLen,
                   unsigned long maxChannelWaitMs);

void aprs_msg_callback(struct AX25Msg *msg);
void serviceDigipeater();
void initDigipeater();

bool sendPeriodicBeacon();
bool sendManualBeacon();
void scheduleNextBeacon();
void serviceBeaconScheduler();

void handleManualButton();

void printConfiguration();
void printPacket(const AX25Msg *msg);
void printPath(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask);
const __FlashStringHelper *digiDecisionText(DigiDecision decision);

bool configLooksSafe();
bool formatAprsCoordinates(double latitude, double longitude,
                           char latOut[9], char lonOut[10]);
void setAx25Call(AX25Call &dst, const char *call, uint8_t ssid);
bool sameAx25Call(const AX25Call &a, const char *call, uint8_t ssid);

void setup() {
  forcePttOff();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(SERIAL_BAUD);
  delay(100);

  randomSeed(micros() ^ analogRead(A5));

  positionConfigValid = formatAprsCoordinates(STATION_LATITUDE, STATION_LONGITUDE,
                                              aprsLat, aprsLon);

  APRS_init(ADC_REFERENCE, OPEN_SQUELCH);
  APRS_setCallsign((char *)STATION_CALLSIGN, STATION_SSID);
  APRS_setDestination((char *)APRS_TOCALL, APRS_TOCALL_SSID);
  APRS_setPath1((char *)"", 0);
  APRS_setPath2((char *)"", 0);
  APRS_setPreamble(APRS_PREAMBLE_MS);
  APRS_setTail(APRS_TAIL_MS);

  initDigipeater();
  stationConfigValid = positionConfigValid && configLooksSafe();
  printConfiguration();

  if (!stationConfigValid) {
    Serial.println(F("CONFIG ERROR: RF transmission disabled until settings are corrected."));
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

  Serial.println(F("SV3GKD APRS Digi 2026 ready."));
}

void loop() {
  if (ENABLE_WATCHDOG) {
    wdt_reset();
  }

  // Parse at most one complete received frame per call. The patched library
  // intentionally returns after one frame so an eligible relay can be serviced
  // before another complete frame is drained from the FIFO.
  APRS_poll();

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
}
