// -----------------------------------------------------------------------------
// Serial diagnostics
// -----------------------------------------------------------------------------

static void printAddress(const AX25Call &a) {
  Serial.print(a.call);
  if (a.ssid > 0) {
    Serial.print('-');
    Serial.print(a.ssid);
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
  Serial.print('>');
  printAddress(path[0]);

  uint8_t rptCount = pathLen - 2;
  int8_t lastUsed = lastUsedRepeater(repeatedMask, rptCount);
  for (uint8_t i = 2; i < pathLen; ++i) {
    Serial.print(',');
    printAddress(path[i]);
    if ((int8_t)(i - 2) == lastUsed) Serial.print('*');
  }
}

void printPacket(const AX25Msg *msg) {
  printAddress(msg->src);
  Serial.print('>');
  printAddress(msg->dst);

  int8_t lastUsed = lastUsedRepeater(msg->rpt_flags, msg->rpt_count);
  for (uint8_t i = 0; i < msg->rpt_count; ++i) {
    Serial.print(',');
    printAddress(msg->rpt_list[i]);
    if ((int8_t)i == lastUsed) Serial.print('*');
  }

  Serial.print(':');
  for (size_t i = 0; i < msg->len; ++i) {
    Serial.write(msg->info[i]);
  }
  Serial.println();
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
  Serial.println();
  Serial.println(F("=== SV3GKD APRS Digi 2026 configuration ==="));

  Serial.print(F("Station:        "));
  Serial.print(STATION_CALLSIGN);
  Serial.print('-');
  Serial.println(STATION_SSID);

  Serial.print(F("TOCALL:         "));
  Serial.println(APRS_TOCALL);

  Serial.print(F("Position:       "));
  Serial.print(aprsLat);
  Serial.print(' ');
  Serial.println(aprsLon);

  Serial.print(F("Symbol:         "));
  Serial.print(APRS_SYMBOL_TABLE);
  Serial.println(APRS_SYMBOL_CODE);

  Serial.print(F("PHG:            "));
  Serial.println(APRS_PHG);

  Serial.print(F("Comment 1:      "));
  Serial.println(APRS_COMMENT_1);

  Serial.print(F("Comment 2:      "));
  Serial.println(APRS_COMMENT_2);

  Serial.print(F("Comment loop:   "));
  Serial.println(BEACON_ALTERNATE_COMMENTS ? F("1 / 2 / 1 / 2") : F("comment 1 only"));

  Serial.print(F("Beacon every:   "));
  Serial.print(BEACON_INTERVAL_MINUTES);
  Serial.print(F(" min, jitter +/-"));
  Serial.print(BEACON_JITTER_SECONDS);
  Serial.println(F(" s"));

  Serial.print(F("Beacon mode:    "));
  if (BEACON_PATH_MODE == BEACON_PATH_DIRECT) Serial.println(F("DIRECT"));
  else if (BEACON_PATH_MODE == BEACON_PATH_FIXED) Serial.println(F("FIXED"));
  else Serial.println(F("PROPORTIONAL DIRECT / WIDE2-1 / DIRECT / WIDE2-2"));

  Serial.print(F("Digipeater:     "));
  Serial.println(DIGI_ENABLED ? F("enabled") : F("disabled"));

  Serial.print(F("WIDE max n:     "));
  Serial.println(DIGI_MAX_WIDE_N);

  Serial.print(F("Large-N trap:   "));
  Serial.println(DIGI_TRAP_LARGE_N ? F("enabled") : F("disabled"));

  Serial.print(F("Dupe window:    "));
  Serial.print(DIGI_DUPLICATE_WINDOW_MS / 1000UL);
  Serial.println(F(" s"));

  Serial.println(F("Hardware:       RX=A2 PTT=D3 DAC=D4..D7 BUTTON=D8 TXLED=D13"));
  Serial.println(F("DAC ladder:     D4=8k2 D5=3k9 D6=2k2 D7=1k"));

  Serial.print(F("Channel detect: "));
  Serial.println(CHANNEL_BUSY_DETECT_ENABLED ? F("enabled") : F("disabled"));

  Serial.print(F("Preamble/tail:  "));
  Serial.print(APRS_PREAMBLE_MS);
  Serial.print('/');
  Serial.print(APRS_TAIL_MS);
  Serial.println(F(" ms"));

  Serial.print(F("Free SRAM est.: "));
  Serial.println(freeMemory());
  Serial.println(F("==========================================="));
  Serial.println();
}
