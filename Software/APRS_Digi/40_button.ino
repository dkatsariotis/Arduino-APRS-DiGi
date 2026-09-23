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
        Serial.println(F("Button released; manual beacon re-enabled."));
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
      Serial.println(F("Manual beacon button press detected."));
    }
    return;
  }

  // Still held.
  if (previousButtonState == LOW && currentButtonState == LOW) {
    if ((unsigned long)(millis() - pressStartedAt) > BUTTON_HOLD_TIMEOUT_MS) {
      Serial.println(F("Button held too long; locked until release."));
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
        Serial.print(F("Button ignored, short press ms="));
        Serial.println(pressDuration);
        return;
      }

      Serial.print(F("Manual beacon requested, press ms="));
      Serial.println(pressDuration);
      manualBeaconRequested = true;
    }
    return;
  }
}
