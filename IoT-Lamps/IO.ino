void setLamp(byte pos, bool value, bool report) {
  if (!silent && lamps[pos].lightOn != value && report) buzz(1000, 50);
  updateWattsHour();
  lamps[pos].lightOn = value;
  sr.set(lamps[pos].pin, value);
  if (report) {
    log(String(lamps[pos].lightTopic) + " changed to " + boolToOnOff(lamps[pos].lightOn));
    sendMQTT(lamps[pos].switchTopic, lamps[pos].lightOn);
  }
}

void updateWattsHour() {
  wattsUsing = 0;
  for (byte i = 0; i < LAMPS; i++)
    if (lamps[i].lightOn)
      wattsUsing += lamps[i].watts;
  wattsHour += wattsUsing * (millis() - lastLightUpdate) / 1000.0 / 3600.0;
  EEPROM.put(0, wattsHour);
  EEPROM.commit();
  lastLightUpdate = millis();
}

void buzz(int frequency, int duration) {
  tone(translate(PIN_BUZZER), frequency, duration);
}
