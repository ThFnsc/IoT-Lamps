void connectionLoop() {
  if (loopHalfSecond < millis()) {
    loopHalfSecond = millis() + 500;
    if (wifiConnected != isWifiConnected()) {
      wifiConnected = isWifiConnected();
      if (wifiConnected) {
        log("WiFi connected");
        buzz(2000, 100);
        log("IP address: " + WiFi.localIP().toString());
      } else {
        log("WiFi connection lost");
      }
    }
    if (wifiConnected) {
      if (!client.connected()) {
        log("Connecting to MQTT broker... ");
        if (client.connect("ESP8266Client", "Thiago", "329619044921")) {
          log("Success");
          buzz(3000, 100);
          client.publish("settings/reboot/s", "OFF");
          subscribe("status/reset");
          subscribe("settings/reboot");
          for (byte i = 0; i < LAMPS; i++)
            subscribe(lamps[i].lightTopic);
        } else {
          log("Failed, trying again in 10 seconds", false);
          buzz(700, 750);
          loopHalfSecond = millis() + 10000;
        }
      }
    } else {
      log("Waiting for wifi... (" + String(WiFi.status()) + ")");
      fails++;
      if (fails > 10){
        WiFi.disconnect();
        setupWifi();
        log("Trying to restart wifi...");
        fails=0;
      }
      buzz(700, 100);
    }
  }
}

void subscribe(char* topic) {
  if (client.subscribe(topic))
    log("Subscribed to " + String(topic));
  else log("Failed to subscribe to " + String(topic));
}

bool isWifiConnected() {
  return WiFi.status() == 3;
}

void log(String msg, bool lineBreak, bool timestamp) {
  msg = (timestamp ? "[" + String(millis()) + "] " : "") + msg + (lineBreak ? "\n" : "");
  Serial.print(msg);
}

void checkSerial() {
  while (Serial.available() > 0) {
    serialIn += (char)Serial.read();
    if (serialIn.endsWith("\r\n")) {
      serialIn = serialIn.substring(0, serialIn.length() - 2);
      serialIn.toLowerCase();
      if (serialIn.startsWith("set energy price ")) {
        reaisPerKWh = serialIn.substring(17, serialIn.length()).toFloat();
        Serial.println("Done, R$" + String(reaisPerKWh, 6) + "/KWh");
        EEPROM.put(10, reaisPerKWh);
        EEPROM.commit();
      } else if (serialIn.startsWith("set watts hour ")) {
        wattsHour = serialIn.substring(15, serialIn.length()).toFloat();
        Serial.println("Done, " + String(wattsHour, 6) + "Wh");
        EEPROM.put(0, wattsHour);
        EEPROM.commit();
      } else if (serialIn.startsWith("set lamp ")) {
        serialIn = serialIn.substring(9, serialIn.length());
        if (serialIn.substring(0, 1) == "*") {
          for (byte i = 0; i < LAMPS; i++)
            setLamp(i, serialIn.substring(2, 3) == "1");
        } else {
          setLamp(serialIn.substring(0, 1).toInt(), serialIn.substring(2, 3) == "1");
        }
      } else if (serialIn == "reboot") {
        Serial.println("Rebooting...");
        ESP.restart();
      } else if (serialIn == "status") {
        updateWattsHour();
        Serial.println("~~ Status report ~~");
        Serial.println("Wifi: " + (wifiConnected ? "connected with IP Address " + WiFi.localIP().toString() : "disconnected"));
        Serial.println("MQTT: " + String(client.connected() ? "connected" : "disconnected"));
        Serial.print("Watts being pulled: ");
        Serial.println(wattsUsing);
        Serial.print("Wh: ");
        Serial.println(wattsHour);
        float KWh = wattsHour / 1000.0;
        Serial.print("KWh: ");
        Serial.println(KWh, 4);
        Serial.print("Preço: R$ ");
        Serial.print(reaisPerKWh, 6);
        Serial.print("/KWh\nTotal: R$ ");
        Serial.println(KWh * reaisPerKWh, 6);
        for (byte i = 0; i < LAMPS; i++)
          Serial.println("Lamp " + String(i) + " (" + lamps[i].lightTopic + ") is " + boolToOnOff(lamps[i].lightOn));
      } else if (serialIn == "reset kwh") {
        wattsHour = 0;
        Serial.println("Done.");
      } else
        Serial.println("Command '" + serialIn + "' not recognized\nTry:\n* Reboot\n* Status\n* Reset KWh\n* set energy price %\n* set lamp [0-" + String(LAMPS) + "] [0/1]\n* set watts hour %");
      serialIn = "";
    }
  }
}

void sendMQTT(char topic[], bool value) {
  log((client.publish(topic, value ? "ON" : "OFF") ? "> " : "!> ") + String(topic) + ":" + boolToOnOff(value));
}
