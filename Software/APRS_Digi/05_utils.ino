// -----------------------------------------------------------------------------
// Small shared helpers and configuration validation
// -----------------------------------------------------------------------------

bool formatAprsCoordinates(double latitude, double longitude,
                           char latOut[9], char lonOut[10]) {
  if (latitude < -90.0 || latitude > 90.0 ||
      longitude < -180.0 || longitude > 180.0) {
    latOut[0] = '\0';
    lonOut[0] = '\0';
    return false;
  }

  const char latHemisphere = (latitude < 0.0) ? 'S' : 'N';
  const char lonHemisphere = (longitude < 0.0) ? 'W' : 'E';

  double absLat = (latitude < 0.0) ? -latitude : latitude;
  double absLon = (longitude < 0.0) ? -longitude : longitude;

  uint16_t latDegrees = (uint16_t)absLat;
  uint16_t lonDegrees = (uint16_t)absLon;

  // Round minutes to 0.01 minute, the resolution of classic uncompressed APRS.
  uint16_t latMinuteHundredths =
      (uint16_t)(((absLat - (double)latDegrees) * 6000.0) + 0.5);
  uint16_t lonMinuteHundredths =
      (uint16_t)(((absLon - (double)lonDegrees) * 6000.0) + 0.5);

  // Handle rounding across a degree boundary (59.995 min -> 60.00 min).
  if (latMinuteHundredths >= 6000) {
    latMinuteHundredths = 0;
    latDegrees++;
  }
  if (lonMinuteHundredths >= 6000) {
    lonMinuteHundredths = 0;
    lonDegrees++;
  }

  if (latDegrees > 90 || lonDegrees > 180) {
    latOut[0] = '\0';
    lonOut[0] = '\0';
    return false;
  }

  snprintf(latOut, 9, "%02u%02u.%02u%c",
           (unsigned)latDegrees,
           (unsigned)(latMinuteHundredths / 100),
           (unsigned)(latMinuteHundredths % 100),
           latHemisphere);

  snprintf(lonOut, 10, "%03u%02u.%02u%c",
           (unsigned)lonDegrees,
           (unsigned)(lonMinuteHundredths / 100),
           (unsigned)(lonMinuteHundredths % 100),
           lonHemisphere);

  return true;
}


void setAx25Call(AX25Call &dst, const char *call, uint8_t ssid) {
  memset(&dst, 0, sizeof(dst));
  if (call != NULL) {
    strncpy(dst.call, call, 6);
    dst.call[6] = '\0';
  }
  dst.ssid = ssid & 0x0F;
}

bool sameAx25Call(const AX25Call &a, const char *call, uint8_t ssid) {
  char temp[7] = {0};
  strncpy(temp, call, 6);
  return (strncmp(a.call, temp, 6) == 0) && (a.ssid == (ssid & 0x0F));
}

static bool validCallField(const char *s, bool allowEmpty) {
  if (s == NULL) return false;
  size_t n = strlen(s);
  if (n == 0) return allowEmpty;
  if (n > 6) return false;

  for (size_t i = 0; i < n; ++i) {
    char c = s[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
      return false;
    }
  }
  return true;
}

static bool validPhg(const char *s) {
  if (s == NULL || s[0] == '\0') return true;
  if (strlen(s) != 7) return false;
  if (strncmp(s, "PHG", 3) != 0) return false;
  for (uint8_t i = 3; i < 7; ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  return true;
}

bool configLooksSafe() {
  if (!validCallField(STATION_CALLSIGN, false)) return false;
  if (STATION_SSID > 15) return false;
  if (!validCallField(APRS_TOCALL, false)) return false;
  if (APRS_TOCALL_SSID > 15) return false;

  if (!positionConfigValid) return false;
  if (strlen(aprsLat) != 8) return false;
  if (strlen(aprsLon) != 9) return false;

  if (!validPhg(APRS_PHG)) return false;

  if (BEACON_FIXED_PATH1_SSID > 15 || BEACON_FIXED_PATH2_SSID > 15) return false;
  if (BEACON_ONE_HOP_SSID > 15 || BEACON_TWO_HOP_SSID > 15) return false;
  if (MANUAL_PATH1_SSID > 15 || MANUAL_PATH2_SSID > 15) return false;

  if (!validCallField(BEACON_FIXED_PATH1_CALL, true)) return false;
  if (!validCallField(BEACON_FIXED_PATH2_CALL, true)) return false;
  if (!validCallField(BEACON_ONE_HOP_CALL, false)) return false;
  if (!validCallField(BEACON_TWO_HOP_CALL, false)) return false;
  if (!validCallField(MANUAL_PATH1_CALL, true)) return false;
  if (!validCallField(MANUAL_PATH2_CALL, true)) return false;

  if (BEACON_INTERVAL_MINUTES == 0) return false;
  if (DIGI_MAX_WIDE_N < 1 || DIGI_MAX_WIDE_N > 7) return false;
  if (DIGI_RANDOM_HOLDOFF_MAX_MS < DIGI_RANDOM_HOLDOFF_MIN_MS) return false;

  // APRS position data extension + position comment conventionally fits in
  // 43 characters. PHG occupies 7 of them, leaving up to 36 for each comment.
  if (strlen(APRS_PHG) + strlen(APRS_COMMENT_1) > 43) return false;
  if (strlen(APRS_PHG) + strlen(APRS_COMMENT_2) > 43) return false;

  size_t packetLen1 = 1 + 8 + 1 + 9 + 1 + strlen(APRS_PHG) + strlen(APRS_COMMENT_1);
  size_t packetLen2 = 1 + 8 + 1 + 9 + 1 + strlen(APRS_PHG) + strlen(APRS_COMMENT_2);
  if (packetLen1 >= BEACON_INFO_BUFFER_SIZE || packetLen2 >= BEACON_INFO_BUFFER_SIZE) return false;

  return true;
}
