// -----------------------------------------------------------------------------
// Serial diagnostics
// -----------------------------------------------------------------------------

void printBootResetCause() {
#if ENABLE_SERIAL_DIAGNOSTICS
  DBG_PRINT(F("Reset cause:     "));
  if (bootResetFlags == 0) {
    DBG_PRINTLN(F("unknown/cleared by bootloader"));
    return;
  }

  bool first = true;
  if (bootResetFlags & _BV(PORF))  { DBG_PRINT(F("POWER-ON")); first = false; }
  if (bootResetFlags & _BV(EXTRF)) { if (!first) DBG_PRINT('|'); DBG_PRINT(F("EXTERNAL")); first = false; }
  if (bootResetFlags & _BV(BORF))  { if (!first) DBG_PRINT('|'); DBG_PRINT(F("BROWN-OUT")); first = false; }
  if (bootResetFlags & _BV(WDRF))  { if (!first) DBG_PRINT('|'); DBG_PRINT(F("WATCHDOG")); first = false; }
  if (!first) DBG_PRINTLN();
  else DBG_PRINTLN(F("OTHER"));
#endif
}

void serviceHealthDiagnostics() {
#if ENABLE_SERIAL_DIAGNOSTICS
  if (HEALTH_HEARTBEAT_SECONDS == 0) return;

  unsigned long now = millis();
  unsigned long intervalMs = (unsigned long)HEALTH_HEARTBEAT_SECONDS * 1000UL;
  if ((unsigned long)(now - lastHealthAtMs) < intervalMs) return;
  lastHealthAtMs = now;

  DBG_PRINT(F("HEALTH up="));
  DBG_PRINT(now / 1000UL);
  DBG_PRINT(F("s rx="));
  DBG_PRINT(runtimeStats.rxPackets);
  DBG_PRINT(F(" q="));
  DBG_PRINT(runtimeStats.digiQueued);
  DBG_PRINT(F(" digiTx="));
  DBG_PRINT(runtimeStats.digiTx);
  DBG_PRINT(F(" dup="));
  DBG_PRINT(runtimeStats.duplicateDrops);
  DBG_PRINT(F(" drop="));
  DBG_PRINT(runtimeStats.digiDrops);
  DBG_PRINT(F(" fs="));
  DBG_PRINT(runtimeStats.relayFailsafeDrops);
  DBG_PRINT(F(" txto="));
  DBG_PRINT(runtimeStats.txTimeouts);
  DBG_PRINT(F(" bcn="));
  DBG_PRINT(runtimeStats.beaconTx);
  DBG_PRINT(F(" bcnFail="));
  DBG_PRINT(runtimeStats.beaconFailed);
  DBG_PRINT(F(" pending="));
  DBG_PRINT(relayPending ? 1 : 0);
  DBG_PRINT(F(" busy="));
  DBG_PRINT(APRS_channelBusy() ? 1 : 0);
  DBG_PRINT(F(" afskTx="));
  DBG_PRINT(APRS_isSending() ? 1 : 0);
  DBG_PRINT(F(" ram="));
  DBG_PRINTLN(freeMemory());
#endif
}

static void printAddress(const AX25Call &a) {
  DBG_PRINT(a.call);
  if (a.ssid > 0) {
    DBG_PRINT('-');
    DBG_PRINT(a.ssid);
  }
}

static int8_t lastUsedRepeater(uint8_t mask, uint8_t count) {
  int8_t last = -1;
  for (uint8_t i = 0; i < count; ++i) {
    if (mask & _BV(i)) last = (int8_t)i;
  }
  return last;
}

void printPath(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask) {
  if (pathLen < 2) return;

  // Standard TNC2 display order is SOURCE>DESTINATION,VIA... . Display the
  // asterisk only after the last used repeater; earlier used addresses are
  // implied even though their AX.25 H bits remain set on air.
  printAddress(path[1]);
  DBG_PRINT('>');
  printAddress(path[0]);

  uint8_t rptCount = pathLen - 2;
  int8_t lastUsed = lastUsedRepeater(repeatedMask, rptCount);
  for (uint8_t i = 2; i < pathLen; ++i) {
    DBG_PRINT(',');
    printAddress(path[i]);
    if ((int8_t)(i - 2) == lastUsed) DBG_PRINT('*');
  }
}

void printPacket(const AX25Msg *msg) {
  printAddress(msg->src);
  DBG_PRINT('>');
  printAddress(msg->dst);

  int8_t lastUsed = lastUsedRepeater(msg->rpt_flags, msg->rpt_count);
  for (uint8_t i = 0; i < msg->rpt_count; ++i) {
    DBG_PRINT(',');
    printAddress(msg->rpt_list[i]);
    if ((int8_t)i == lastUsed) DBG_PRINT('*');
  }

  DBG_PRINT(':');
  for (size_t i = 0; i < msg->len; ++i) {
    DBG_WRITE(msg->info[i]);
  }
  DBG_PRINTLN();
}

const __FlashStringHelper *digiDecisionText(DigiDecision decision) {
  switch (decision) {
    case DIGI_NO_PATH: return F("no eligible path");
    case DIGI_OWN_SOURCE: return F("own source");
    case DIGI_LOOP: return F("own callsign already used");
    case DIGI_TCP_PATH: return F("TCPIP/TCPXX path");
    case DIGI_UNSUPPORTED: return F("unsupported/invalid alias");
    case DIGI_EXPLICIT: return F("explicit MYCALL");
    case DIGI_WIDE_REPLACE: return F("WIDEn-1 replace");
    case DIGI_WIDE_INSERT: return F("WIDEn-N insert/decrement");
    case DIGI_WIDE_TRAP: return F("large-N trap");
    case DIGI_TOO_LONG: return F("information field too long");
    case DIGI_QUEUE_BUSY: return F("relay queue busy");
    case DIGI_DUPLICATE: return F("duplicate");
    default: return F("unknown");
  }
}

void printConfiguration() {
  DBG_PRINTLN();
  DBG_PRINTLN(F("=== Arduino APRS Digipeater 2026 configuration ==="));

  DBG_PRINT(F("Config armed:   "));
  DBG_PRINTLN(CONFIGURATION_CONFIRMED ? F("YES") : F("NO - RF TX disabled"));

  DBG_PRINT(F("Station:        "));
  DBG_PRINT(STATION_CALLSIGN);
  DBG_PRINT('-');
  DBG_PRINTLN(STATION_SSID);

  DBG_PRINT(F("TOCALL:         "));
  DBG_PRINTLN(APRS_TOCALL);

  DBG_PRINT(F("Position:       "));
#if ENABLE_SERIAL_DIAGNOSTICS
  {
    char lat[9];
    char lon[10];
    if (formatAprsCoordinates(STATION_LATITUDE, STATION_LONGITUDE, lat, lon)) {
      DBG_PRINT(lat);
      DBG_PRINT(' ');
      DBG_PRINTLN(lon);
    } else {
      DBG_PRINTLN(F("INVALID"));
    }
  }
#endif

  DBG_PRINT(F("Symbol:         "));
  DBG_PRINT(APRS_SYMBOL_TABLE);
  DBG_PRINTLN(APRS_SYMBOL_CODE);

  DBG_PRINT(F("PHG:            "));
  DBG_PRINTLN(APRS_PHG);

  DBG_PRINT(F("Comment 1:      "));
  DBG_PRINTLN(APRS_COMMENT_1);

  DBG_PRINT(F("Comment 2:      "));
  DBG_PRINTLN(APRS_COMMENT_2);

  DBG_PRINT(F("Comment loop:   "));
  DBG_PRINTLN(BEACON_ALTERNATE_COMMENTS ? F("1 / 2 / 1 / 2") : F("comment 1 only"));

  DBG_PRINT(F("Beacon every:   "));
  DBG_PRINT(BEACON_INTERVAL_MINUTES);
  DBG_PRINT(F(" min, jitter +/-"));
  DBG_PRINT(BEACON_JITTER_SECONDS);
  DBG_PRINTLN(F(" s"));

  DBG_PRINT(F("Beacon mode:    "));
  if (BEACON_PATH_MODE == BEACON_PATH_DIRECT) DBG_PRINTLN(F("DIRECT"));
  else if (BEACON_PATH_MODE == BEACON_PATH_FIXED) DBG_PRINTLN(F("FIXED"));
  else DBG_PRINTLN(F("PROPORTIONAL DIRECT / WIDE2-1 / DIRECT / WIDE2-2"));

  DBG_PRINT(F("Digipeater:     "));
  DBG_PRINTLN(DIGI_ENABLED ? F("enabled") : F("disabled"));

  DBG_PRINT(F("WIDE max n:     "));
  DBG_PRINTLN(DIGI_MAX_WIDE_N);

  DBG_PRINT(F("Large-N trap:   "));
  DBG_PRINTLN(DIGI_TRAP_LARGE_N ? F("enabled") : F("disabled"));

  DBG_PRINT(F("Dupe window:    "));
  DBG_PRINT(DIGI_DUPLICATE_WINDOW_MS / 1000UL);
  DBG_PRINTLN(F(" s"));

  DBG_PRINT(F("Pending failsafe: "));
  DBG_PRINT(DIGI_PENDING_FAILSAFE_MS);
  DBG_PRINTLN(F(" ms"));

  DBG_PRINT(F("Watchdog:       "));
  DBG_PRINTLN(ENABLE_WATCHDOG ? F("8 s enabled") : F("disabled"));

  DBG_PRINT(F("Health log:     "));
  if (HEALTH_HEARTBEAT_SECONDS > 0) {
    DBG_PRINT(HEALTH_HEARTBEAT_SECONDS);
    DBG_PRINTLN(F(" s"));
  } else {
    DBG_PRINTLN(F("disabled"));
  }

  DBG_PRINTLN(F("Hardware:       RX=A2 PTT=D3 DAC=D4..D7 BUTTON=D8 TXLED=D13"));
  DBG_PRINTLN(F("DAC ladder:     D4=8k2 D5=3k9 D6=2k2 D7=1k"));

  DBG_PRINT(F("Channel detect: "));
  DBG_PRINTLN(CHANNEL_BUSY_DETECT_ENABLED ? F("enabled") : F("disabled"));

  DBG_PRINT(F("Preamble/tail:  "));
  DBG_PRINT(APRS_PREAMBLE_MS);
  DBG_PRINT('/');
  DBG_PRINT(APRS_TAIL_MS);
  DBG_PRINTLN(F(" ms"));

  DBG_PRINT(F("Free SRAM est.: "));
  DBG_PRINTLN(freeMemory());
  DBG_PRINTLN(F("==========================================="));
  DBG_PRINTLN();
}

void serviceRxAudioDiagnostics() {
  if (ENABLE_SERIAL_DIAGNOSTICS) {
    static unsigned long lastPrint = 0;

    unsigned long now = millis();
    if (now - lastPrint < 1000UL)
      return;

    lastPrint = now;

    int8_t minVal;
    int8_t maxVal;

    AFSK_getRxLevel(&minVal, &maxVal);

    int16_t pp = (int16_t)maxVal - (int16_t)minVal;

    Serial.print(F("RXADC min="));
    Serial.print((int)minVal);

    Serial.print(F(" max="));
    Serial.print((int)maxVal);

    Serial.print(F(" pp="));
    Serial.println(pp);
  }
}