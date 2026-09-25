// -----------------------------------------------------------------------------
// Local station beacon generation and scheduling
// -----------------------------------------------------------------------------

static const char *commentForPeriodicSequence(uint8_t sequence) {
  if (!BEACON_ALTERNATE_COMMENTS) return APRS_COMMENT_1;
  return (sequence & 0x01) ? APRS_COMMENT_2 : APRS_COMMENT_1;
}

static bool buildBeaconInfo(char *out, size_t outSize, const char *comment) {
  char lat[9];
  char lon[10];

  if (!formatAprsCoordinates(STATION_LATITUDE, STATION_LONGITUDE, lat, lon)) {
    DBG_PRINTLN(F("BEACON ERROR: invalid coordinates"));
    return false;
  }

  int written = snprintf(out, outSize,
                         "!%s%c%s%c%s%s",
                         lat,
                         APRS_SYMBOL_TABLE,
                         lon,
                         APRS_SYMBOL_CODE,
                         APRS_PHG,
                         comment);

  if (written < 0 || written >= (int)outSize) {
    DBG_PRINTLN(F("BEACON ERROR: information field too long"));
    return false;
  }
  return true;
}

static uint8_t buildOwnPath(AX25Call *path,
                            const char *path1Call, uint8_t path1Ssid,
                            const char *path2Call, uint8_t path2Ssid) {
  setAx25Call(path[0], APRS_TOCALL, APRS_TOCALL_SSID);
  setAx25Call(path[1], STATION_CALLSIGN, STATION_SSID);
  uint8_t len = 2;

  if (path1Call != NULL && path1Call[0] != '\0') {
    setAx25Call(path[len++], path1Call, path1Ssid);
  }
  if (path2Call != NULL && path2Call[0] != '\0' && len < AX25_MAX_RPT + 2) {
    setAx25Call(path[len++], path2Call, path2Ssid);
  }
  return len;
}

static bool sendOwnBeaconVia(const char *label,
                             const char *comment,
                             const char *path1Call, uint8_t path1Ssid,
                             const char *path2Call, uint8_t path2Ssid) {
  if (!configLooksSafe()) return false;

  // Beacon payload is needed only during this synchronous TX call; keep it on
  // the stack rather than permanently occupying SRAM.
  char beaconInfo[BEACON_INFO_BUFFER_SIZE];
  if (!buildBeaconInfo(beaconInfo, sizeof(beaconInfo), comment)) return false;

  AX25Call path[4];
  uint8_t pathLen = buildOwnPath(path, path1Call, path1Ssid, path2Call, path2Ssid);

  DBG_PRINT(F("BEACON TX ["));
  DBG_PRINT(label);
  DBG_PRINT(F("] "));
  printPath(path, pathLen, 0);
  DBG_PRINT(F(":"));
  DBG_PRINTLN(beaconInfo);

  if (transmitFrame(path, pathLen, 0,
                    (const uint8_t *)beaconInfo, strlen(beaconInfo),
                    BEACON_MAX_CHANNEL_WAIT_MS)) {
    STAT_INC(beaconTx);
    return true;
  }

  STAT_INC(beaconFailed);
  DBG_PRINTLN(F("BEACON deferred/skipped: channel not available"));
  return false;
}

bool sendPeriodicBeacon() {
  const char *comment = commentForPeriodicSequence(beaconSequence);

  if (BEACON_PATH_MODE == BEACON_PATH_DIRECT) {
    if (sendOwnBeaconVia("DIRECT", comment, "", 0, "", 0)) {
      beaconSequence++;
      return true;
    }
    return false;
  }

  if (BEACON_PATH_MODE == BEACON_PATH_FIXED) {
    if (sendOwnBeaconVia("FIXED", comment,
                         BEACON_FIXED_PATH1_CALL, BEACON_FIXED_PATH1_SSID,
                         BEACON_FIXED_PATH2_CALL, BEACON_FIXED_PATH2_SSID)) {
      beaconSequence++;
      return true;
    }
    return false;
  }

  // Proportional sequence: DIRECT, WIDE2-1, DIRECT, WIDE2-2.
  switch (beaconSequence & 0x03) {
    case 0:
    case 2:
      if (sendOwnBeaconVia("PROP DIRECT", comment, "", 0, "", 0)) {
        beaconSequence++;
        return true;
      }
      break;

    case 1:
      if (sendOwnBeaconVia("PROP 1-HOP", comment,
                           BEACON_ONE_HOP_CALL, BEACON_ONE_HOP_SSID,
                           "", 0)) {
        beaconSequence++;
        return true;
      }
      break;

    default:
      if (sendOwnBeaconVia("PROP 2-HOP", comment,
                           BEACON_TWO_HOP_CALL, BEACON_TWO_HOP_SSID,
                           "", 0)) {
        beaconSequence++;
        return true;
      }
      break;
  }

  return false;
}

bool sendManualBeacon() {
  // Manual D8 tests always use COMMENT_1 and do not advance the automatic
  // COMMENT_1/COMMENT_2 sequence.
  return sendOwnBeaconVia("MANUAL D8", APRS_COMMENT_1,
                          MANUAL_PATH1_CALL, MANUAL_PATH1_SSID,
                          MANUAL_PATH2_CALL, MANUAL_PATH2_SSID);
}

void scheduleNextBeacon() {
  long jitterMs = 0;
  if (BEACON_JITTER_SECONDS > 0) {
    long span = (long)BEACON_JITTER_SECONDS * 1000L;
    jitterMs = random(-span, span + 1L);
  }

  unsigned long baseMs = (unsigned long)BEACON_INTERVAL_MINUTES * 60000UL;
  long delaySigned = (long)baseMs + jitterMs;
  if (delaySigned < 1000L) delaySigned = 1000L;

  nextBeaconAt = millis() + (unsigned long)delaySigned;

  DBG_PRINT(F("Next beacon in "));
  DBG_PRINT((unsigned long)delaySigned / 1000UL);
  DBG_PRINTLN(F(" s"));
}

void serviceBeaconScheduler() {
  if ((long)(millis() - nextBeaconAt) < 0) {
    return;
  }

  if (sendPeriodicBeacon()) {
    scheduleNextBeacon();
  } else {
    nextBeaconAt = millis() +
                   ((unsigned long)BEACON_RETRY_AFTER_BUSY_SECONDS * 1000UL);
    DBG_PRINT(F("Beacon retry in "));
    DBG_PRINT(BEACON_RETRY_AFTER_BUSY_SECONDS);
    DBG_PRINTLN(F(" s"));
  }
}
