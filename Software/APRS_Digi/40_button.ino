// -----------------------------------------------------------------------------
// D8 manual beacon button - behavior preserved from the working tracker
// -----------------------------------------------------------------------------

void handleManualButton() {
  static bool previousButtonState = HIGH;
  static unsigned long pressStartedAt = 0;

  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (buttonLockout) {
    if (currentButtonState == HIGH) {
      delay(BUTTON_DEBOUNCE_MS);
      if (digitalRead(BUTTON_PIN) == HIGH) {
        buttonLockout = false;
        previousButtonState = HIGH;
        DBG_PRINTLN(F("Button released; manual beacon re-enabled."));
      }
    }
    return;
  }

  // New press: HIGH -> LOW.
  if (previousButtonState == HIGH && currentButtonState == LOW) {
    delay(BUTTON_DEBOUNCE_MS);
    if (digitalRead(BUTTON_PIN) == LOW) {
      pressStartedAt = millis();
      previousButtonState = LOW;
      DBG_PRINTLN(F("Manual beacon button press detected."));
    }
    return;
  }

  // Still held.
  if (previousButtonState == LOW && currentButtonState == LOW) {
    if ((unsigned long)(millis() - pressStartedAt) > BUTTON_HOLD_TIMEOUT_MS) {
      DBG_PRINTLN(F("Button held too long; locked until release."));
      buttonLockout = true;
    }
    return;
  }

  // Release: LOW -> HIGH. A valid beacon is triggered on release.
  if (previousButtonState == LOW && currentButtonState == HIGH) {
    delay(BUTTON_DEBOUNCE_MS);
    if (digitalRead(BUTTON_PIN) == HIGH) {
      previousButtonState = HIGH;
      unsigned long pressDuration = millis() - pressStartedAt;

      if (pressDuration < BUTTON_MIN_PRESS_MS) {
        DBG_PRINT(F("Button ignored, short press ms="));
        DBG_PRINTLN(pressDuration);
        return;
      }

      DBG_PRINT(F("Manual beacon requested, press ms="));
      DBG_PRINTLN(pressDuration);
      manualBeaconRequested = true;
    }
    return;
  }
}
