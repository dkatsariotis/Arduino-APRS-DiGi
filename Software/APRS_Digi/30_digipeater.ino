// -----------------------------------------------------------------------------
// APRS digipeater logic
//
// Design goals:
//   * process only the first unused via address
//   * explicit own-call routing
//   * traceable WIDE1-1 / WIDE2-1 / WIDE2-2
//   * configurable large-N trap
//   * never alter source, destination or information field
//   * 30 s duplicate suppression, ignoring the via path
//   * loop protection and TCPIP/TCPXX RF protection
// -----------------------------------------------------------------------------

static bool callIs(const AX25Call &a, const char *literal) {
  char temp[7] = {0};
  strncpy(temp, literal, 6);
  return strncmp(a.call, temp, 6) == 0;
}

static bool parseWide(const AX25Call &call, uint8_t &n, uint8_t &remaining) {
  if (strncmp(call.call, "WIDE", 4) != 0) return false;
  if (call.call[4] < '1' || call.call[4] > '7') return false;
  if (call.call[5] != '\0') return false;

  n = (uint8_t)(call.call[4] - '0');
  remaining = call.ssid;
  return true;
}

static bool hasTcpInternetPath(const AX25Msg *msg) {
  if (!DIGI_BLOCK_TCP_PATHS) return false;

  for (uint8_t i = 0; i < msg->rpt_count; ++i) {
    if (callIs(msg->rpt_list[i], "TCPIP") || callIs(msg->rpt_list[i], "TCPXX")) {
      return true;
    }
  }
  return false;
}

static uint32_t fnv1aByte(uint32_t h, uint8_t b) {
  h ^= b;
  h *= 16777619UL;
  return h;
}

static uint32_t packetHash(const AX25Msg *msg) {
  uint32_t h = 2166136261UL;

  for (uint8_t i = 0; i < 6; ++i) h = fnv1aByte(h, (uint8_t)msg->src.call[i]);
  h = fnv1aByte(h, msg->src.ssid);

  for (uint8_t i = 0; i < 6; ++i) h = fnv1aByte(h, (uint8_t)msg->dst.call[i]);
  h = fnv1aByte(h, msg->dst.ssid);

  h = fnv1aByte(h, (uint8_t)(msg->len & 0xFF));
  h = fnv1aByte(h, (uint8_t)((msg->len >> 8) & 0xFF));

  for (size_t i = 0; i < msg->len; ++i) h = fnv1aByte(h, msg->info[i]);

  // Deliberately do NOT include the digipeater via path.
  return h;
}

static bool duplicateSeen(uint32_t hash, unsigned long nowMs) {
  for (uint8_t i = 0; i < DIGI_DUPE_CACHE_SIZE; ++i) {
    if (!dupeCache[i].valid) continue;
    if (dupeCache[i].hash != hash) continue;

    if ((unsigned long)(nowMs - dupeCache[i].whenMs) <= DIGI_DUPLICATE_WINDOW_MS) {
      return true;
    }
  }
  return false;
}

static void rememberDuplicate(uint32_t hash, unsigned long nowMs) {
  dupeCache[dupeCacheNext].hash = hash;
  dupeCache[dupeCacheNext].whenMs = nowMs;
  dupeCache[dupeCacheNext].valid = true;
  dupeCacheNext = (uint8_t)((dupeCacheNext + 1) % DIGI_DUPE_CACHE_SIZE);
}

static void replaceRepeaterWithOwnCall(RelayFrame &out, uint8_t rptIndex) {
  setAx25Call(out.path[rptIndex + 2], STATION_CALLSIGN, STATION_SSID);
  out.repeatedMask |= _BV(rptIndex);
}

static DigiDecision prepareRelayFrame(const AX25Msg *msg, RelayFrame &out) {
  if (msg->len > DIGI_MAX_INFO_LEN) return DIGI_TOO_LONG;
  if (sameAx25Call(msg->src, STATION_CALLSIGN, STATION_SSID)) return DIGI_OWN_SOURCE;
  if (msg->rpt_count == 0) return DIGI_NO_PATH;
  if (hasTcpInternetPath(msg)) return DIGI_TCP_PATH;

  uint8_t firstUnused = 0xFF;

  for (uint8_t i = 0; i < msg->rpt_count; ++i) {
    bool used = (msg->rpt_flags & _BV(i)) != 0;

    if (used && sameAx25Call(msg->rpt_list[i], STATION_CALLSIGN, STATION_SSID)) {
      return DIGI_LOOP;
    }

    if (!used && firstUnused == 0xFF) {
      firstUnused = i;
    }
  }

  if (firstUnused == 0xFF) return DIGI_NO_PATH;

  // Preserve source, destination, information, and the complete existing path.
  out.path[0] = msg->dst;
  out.path[1] = msg->src;
  for (uint8_t i = 0; i < msg->rpt_count; ++i) {
    out.path[i + 2] = msg->rpt_list[i];
  }
  out.pathLen = msg->rpt_count + 2;
  out.repeatedMask = msg->rpt_flags;
  out.infoLen = msg->len;
  memcpy(out.info, msg->info, msg->len);
  out.hash = packetHash(msg);

  AX25Call &first = out.path[firstUnused + 2];

  // Traditional explicit AX.25 route through this exact callsign/SSID.
  if (sameAx25Call(first, STATION_CALLSIGN, STATION_SSID)) {
    out.repeatedMask |= _BV(firstUnused);
    return DIGI_EXPLICIT;
  }

  uint8_t n = 0;
  uint8_t remaining = 0;
  if (!parseWide(first, n, remaining)) {
    return DIGI_UNSUPPORTED;
  }

  // Generic New-N validity checks.
  if (remaining == 0 || remaining > n || remaining > 7) {
    return DIGI_UNSUPPORTED;
  }

  if (n > DIGI_MAX_WIDE_N) {
    if (!DIGI_TRAP_LARGE_N) return DIGI_UNSUPPORTED;

    // Consume the excessive path locally and identify this digi. No remaining
    // WIDEn-N element is left behind, so the large-N request cannot propagate.
    replaceRepeaterWithOwnCall(out, firstUnused);
    return DIGI_WIDE_TRAP;
  }

  // Example WIDE1-1 / WIDE2-1: consume final remaining hop by replacing the
  // generic address with the actual digipeater callsign and setting H.
  if (remaining == 1) {
    replaceRepeaterWithOwnCall(out, firstUnused);
    return DIGI_WIDE_REPLACE;
  }

  // Example WIDE2-2: insert MYCALL* before WIDE2-1.
  if (msg->rpt_count < AX25_MAX_RPT) {
    // Shift repeater addresses at and after firstUnused one position right.
    for (int8_t i = (int8_t)msg->rpt_count - 1; i >= (int8_t)firstUnused; --i) {
      out.path[i + 3] = out.path[i + 2];
    }

    // Shift H-bit mask in the same way.
    uint16_t lowMask = (firstUnused == 0) ? 0 : ((1U << firstUnused) - 1U);
    uint16_t lower = out.repeatedMask & lowMask;
    uint16_t upper = out.repeatedMask & ~lowMask;
    out.repeatedMask = (uint8_t)(lower | ((upper << 1) & 0xFF));

    setAx25Call(out.path[firstUnused + 2], STATION_CALLSIGN, STATION_SSID);
    out.repeatedMask |= _BV(firstUnused);

    // The shifted original WIDEn-N element is now one slot later.
    out.path[firstUnused + 3].ssid = remaining - 1;

    out.pathLen++;
    return DIGI_WIDE_INSERT;
  }

  // Full AX.25 repeater list: fail safe by consuming it as a one-hop trap.
  replaceRepeaterWithOwnCall(out, firstUnused);
  return DIGI_WIDE_TRAP;
}

void initDigipeater() {
  memset(dupeCache, 0, sizeof(dupeCache));
  dupeCacheNext = 0;
  relayPending = false;
}

void aprs_msg_callback(struct AX25Msg *msg) {
  statRxPackets++;

  if (SERIAL_LOG_RX_PACKETS) {
    Serial.print(F("RX "));
    printPacket(msg);
  }

  if (!DIGI_ENABLED || !stationConfigValid) return;

  uint32_t hash = packetHash(msg);
  unsigned long now = millis();

  if (duplicateSeen(hash, now)) {
    statDuplicates++;
    if (SERIAL_LOG_DROPS) Serial.println(F("DIGI DROP: duplicate within window"));
    return;
  }

  if (relayPending) {
    if (pendingRelay.hash == hash) {
      statDuplicates++;
      if (SERIAL_LOG_DROPS) Serial.println(F("DIGI DROP: duplicate already queued"));
    } else {
      statDropped++;
      if (SERIAL_LOG_DROPS) Serial.println(F("DIGI DROP: relay queue busy"));
    }
    return;
  }

  DigiDecision decision = prepareRelayFrame(msg, pendingRelay);

  if (!(decision == DIGI_EXPLICIT ||
        decision == DIGI_WIDE_REPLACE ||
        decision == DIGI_WIDE_INSERT ||
        decision == DIGI_WIDE_TRAP)) {
    if (SERIAL_LOG_DROPS && decision != DIGI_NO_PATH) {
      Serial.print(F("DIGI DROP: "));
      Serial.println(digiDecisionText(decision));
    }
    return;
  }

  // packetHash() was calculated both for the early dupe check and inside the
  // prepared frame. Keep the prepared copy authoritative.
  pendingRelay.queuedAtMs = now;
  unsigned long holdoff = DIGI_RANDOM_HOLDOFF_MIN_MS;
  if (DIGI_RANDOM_HOLDOFF_MAX_MS > DIGI_RANDOM_HOLDOFF_MIN_MS) {
    holdoff = random(DIGI_RANDOM_HOLDOFF_MIN_MS,
                     (long)DIGI_RANDOM_HOLDOFF_MAX_MS + 1L);
  }
  pendingRelay.notBeforeMs = now + holdoff;
  relayPending = true;

  Serial.print(F("DIGI QUEUE: "));
  Serial.print(digiDecisionText(decision));
  Serial.print(F(" -> "));
  printPath(pendingRelay.path, pendingRelay.pathLen, pendingRelay.repeatedMask);
  Serial.println();
}

void serviceDigipeater() {
  if (!relayPending || radioTxActive || APRS_isSending()) return;

  unsigned long now = millis();
  unsigned long age = now - pendingRelay.queuedAtMs;

  if (age > DIGI_MAX_DEFER_MS) {
    statDropped++;
    relayPending = false;
    if (SERIAL_LOG_DROPS) Serial.println(F("DIGI DROP: defer timeout"));
    return;
  }

  if ((long)(now - pendingRelay.notBeforeMs) < 0) return;

  if (transmitFrame(pendingRelay.path,
                    pendingRelay.pathLen,
                    pendingRelay.repeatedMask,
                    pendingRelay.info,
                    pendingRelay.infoLen,
                    DIGI_CHANNEL_WAIT_MS)) {
    rememberDuplicate(pendingRelay.hash, millis());
    statDigiTx++;
    Serial.print(F("DIGI TX: "));
    printPath(pendingRelay.path, pendingRelay.pathLen, pendingRelay.repeatedMask);
    Serial.println();
  } else {
    statDropped++;
    if (SERIAL_LOG_DROPS) Serial.println(F("DIGI DROP: channel busy"));
  }

  relayPending = false;
}
