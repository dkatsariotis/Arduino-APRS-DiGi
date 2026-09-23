// -----------------------------------------------------------------------------
// Radio / PTT / clear-channel / TX safety
// -----------------------------------------------------------------------------

void forcePttOff() {
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, PTT_ACTIVE_HIGH ? LOW : HIGH);
}

void forcePttOn() {
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, PTT_ACTIVE_HIGH ? HIGH : LOW);
}

bool waitForClearChannel(unsigned long maxWaitMs) {
  if (!CHANNEL_BUSY_DETECT_ENABLED) return true;

  unsigned long started = millis();
  unsigned long clearSince = 0;

  while (true) {
    if (ENABLE_WATCHDOG) wdt_reset();

    // Keep decoding while waiting. The digipeater callback may reject/drop
    // additional traffic if one relay is already pending.
    APRS_poll();

    if (!APRS_channelBusy()) {
      if (clearSince == 0) {
        clearSince = millis();
      }
      if ((unsigned long)(millis() - clearSince) >= CHANNEL_CLEAR_HOLD_MS) {
        return true;
      }
    } else {
      clearSince = 0;
    }

    if ((unsigned long)(millis() - started) >= maxWaitMs) {
      return false;
    }

    delay(2);
  }
}

bool transmitFrame(const AX25Call *path, uint8_t pathLen, uint8_t repeatedMask,
                   const uint8_t *info, uint16_t infoLen,
                   unsigned long maxChannelWaitMs) {
  if (path == NULL || pathLen < 2 || info == NULL || infoLen == 0) {
    return false;
  }
  if (radioTxActive || APRS_isSending()) {
    return false;
  }

  radioTxActive = true;

  if (!waitForClearChannel(maxChannelWaitMs)) {
    forcePttOff();
    radioTxActive = false;
    return false;
  }

  forcePttOn();

  // Preserve the working tracker's pre-key behavior. The patched AFSK ISR
  // now preserves D3, so this delay really does keep PTT asserted before AFSK.
  unsigned long keyStart = millis();
  while ((unsigned long)(millis() - keyStart) < RADIO_KEYUP_MS) {
    if (ENABLE_WATCHDOG) wdt_reset();
    delay(2);
  }

  unsigned long txStarted = millis();
  bool timedOut = false;

  APRS_sendViaH(path, pathLen, repeatedMask, info, infoLen);

  while (APRS_isSending() || (unsigned long)(millis() - txStarted) < TX_MIN_TOTAL_MS) {
    if (ENABLE_WATCHDOG) wdt_reset();

    if ((unsigned long)(millis() - txStarted) > TX_TIMEOUT_MS) {
      timedOut = true;
      break;
    }
    delay(2);
  }

  delay(TX_POST_HOLD_MS);
  forcePttOff();

  // Discard any self-audio/garbage accumulated by the RX path during TX.
  APRS_resetReceiver();
  radioTxActive = false;

  if (timedOut) {
    DBG_PRINTLN(F("TX ERROR: timeout, PTT forced OFF"));
    return false;
  }

  return true;
}
