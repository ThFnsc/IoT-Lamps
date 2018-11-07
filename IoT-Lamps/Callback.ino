void callback(char* topic, byte* payload, unsigned int length) {
  bool valid = false;
  payload[length] = '\0';
  log("< " + String(topic) + ":" + String((char*)payload), false);
  if (String(topic) == "settings/reboot" && String((char*)payload) == "ON") {
    Serial.println("Rebooting...");
    ESP.restart();
  } else if (String(topic) == "status/reset" && String((char*)payload) == "ON") {
    wattsHour = 0;
    periodicWattageUpdate = millis();
    client.publish("status/reset/s", "OFF");
  }
  for (byte i = 0; i < LAMPS; i++) {
    if (String(lamps[i].lightTopic) == String(topic)) {
      setLamp(i, String((char*)payload) == "ON", false);
      log(", valid", true, false);
      valid = true;
    }
  }
  if (!valid) log(", invalid", true, false);
}
