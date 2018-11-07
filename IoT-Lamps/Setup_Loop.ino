unsigned long lastCheck;

void itemsSetup() {
  si.begin(translate(5), translate(4), translate(1), translate(2));
  si.read();
  for (byte i = 0; i < LAMPS; i++)
    lamps[i].switchPos = si.state(lamps[i].pin);
}

char* floatToChar(float value) {
  dtostrf(value, 6, 6, converBuffer);
  return converBuffer;
}

void itemsLoop() {
  if(morseRunning && nextMorse <= millis()){
    for(int i=0; i<sizeof(morseLamps); i++)
      setLamp(morseLamps[i], bitRead(morse[morsePos/8],7-morsePos%8)%2, false);
    morsePos++;
    if(morsePos >= sizeof(morse)*8)
      morsePos=0;
    nextMorse=millis()+250;
  }
  
  if (lastCheck < millis()) {
    lastCheck = millis() + 25;
    if (spooky && nextSpooky < millis()) {
      nextSpooky = millis() + 100;
      byte toChange = random(0, 5);
      setLamp(toChange, !lamps[toChange].lightOn, false);
    }
    if (si.update()) {
      for (byte i = 0; i < LAMPS; i++) {
        if (si.state(lamps[i].pin) != lamps[i].switchPos) {
          lamps[i].switchPos = !lamps[i].switchPos;
          if (lamps[i].ringSwitch) {
            if (si.state(lamps[i].pin)) {
              lamps[i].lampChange = 0;
              lamps[i].nextChange = millis() + 500;
              if (si.state(lamps[1].pin) && si.state(lamps[4].pin) && !spookyHolding) { //Spooky scary skeletons
                spooky = !spooky;
                silent = spooky;
                spookyHolding = true;
                if (spooky) {
                  for (int i = 0; i < LAMPS; i++)
                    lamps[i].lastOn = lamps[i].lightOn;
                  nextSpooky = millis() + 3000;
                  Serial.println("Spooky mode on!");
                  buzz(5000, 200);
                } else {
                  for (int i = 0; i < LAMPS; i++)
                    setLamp(i, lamps[i].lastOn);
                  Serial.println("Spooky mode off!");
                }
              }
            } else {
              if (spookyHolding) {
                if (!si.state(lamps[1].pin) && !si.state(lamps[4].pin))
                  spookyHolding = false;
                if (!si.state(lamps[1].pin) || !si.state(lamps[4].pin))
                  continue;
              }
              if (lamps[i].lampChange == 0 && millis() - lamps[i].nextChange + 500 >= 50) {
                setLamp(i, !lamps[i].lightOn);
                log(String(lamps[i].switchTopic) + " changed");
              }
            }
          } else {
            setLamp(i, !lamps[i].lightOn);
            log(String(lamps[i].switchTopic) + " changed");
          }
          delay(25);
        }
      }
    }
    for (byte i = 0; i < LAMPS && !spooky; i++) {
      if (si.state(lamps[i].pin) && lamps[i].ringSwitch && lamps[i].lampChange <= LAMPS && lamps[i].nextChange < millis()) { //Se o botão da lâmpada está sendo pressionado, é tipo campainha, não excedeu as próximas lâmpadas e ta na hora de trocar
        if (lamps[i].lampChange == i) { //Se a lâmpada da vez for a mesma que está sendo pressionada
          lamps[i].lampChange++; //Pula pra próxima
          if (lamps[i].lampChange > 1) setLamp(lamps[i].lampChange - 2, !lamps[lamps[i].lampChange - 2].lightOn); //E troca o estado da anterior
        } else { //Senão
          if (lamps[i].lampChange < LAMPS) //Se a lâmpada da vez existe
            setLamp(lamps[i].lampChange, !lamps[lamps[i].lampChange].lightOn); //Inverte seu estado
          else if(i == MORSE_LAMP){ //Senão
            morseRunning=!morseRunning; //Ativa ou desativa a exibição do código morse
            if(morseRunning){
              morsePos = 0;
              nextMorse=millis()+3000;
            }
            log("Morse code " + String(morseRunning ? "running" : "stopped"));
          }
          lamps[i].lampChange++; //E pula pra próxima
          if (lamps[i].lampChange > 1 && lamps[i].lampChange - 2 != i) setLamp(lamps[i].lampChange - 2, !lamps[lamps[i].lampChange - 2].lightOn); //Se não é a primeira lâmpada a ser trocada e não é a próxima da que é pressioada inverte o estaod da anterior
          lamps[i].nextChange = millis() + 500; //Define a próxima verificação daqui meio segundo
        }
      }
    }
  }
  if (periodicWattageUpdate < millis()) {
    periodicWattageUpdate = millis() + 5000;
    updateWattsHour();
    client.publish("status/wattsused", floatToChar(wattsUsing));
    client.publish("status/KWh", floatToChar(wattsHour / 1000.0));
    client.publish("status/energyprice", floatToChar(reaisPerKWh));
    client.publish("status/powerusage", floatToChar(wattsHour / 1000.0 * reaisPerKWh));
  }
}

void startupAnimation() {
  for (byte i = 0; i < LAMPS; i++) {
    if (!lamps[i].bedroom) {
      setLamp(i, HIGH);
      delay(75);
    }
  }
  for (byte i = 0; i < LAMPS; i++) {
    if (!lamps[i].bedroom) {
      setLamp(i, LOW);
      delay(75);
    }
  }
}

void setupWifi() {
  WiFi.config(IPAddress(192, 168, 1, 14), IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  WiFi.begin(WIFI, PASSWD);
}

void setup() {
  pinMode(translate(PIN_BUZZER), OUTPUT);
  buzz(1000, 100);
  EEPROM.begin(512);
  EEPROM.get(0, wattsHour);
  EEPROM.get(10, reaisPerKWh);
  if (BOOT_ANIMATION) startupAnimation();
  Serial.begin(115200);
  log("\nStarting...", true, false);
  itemsSetup();
  log("Connecting to WiFi '" + String(WIFI) + "'");
  setupWifi();
}

void loop() {
  connectionLoop();
  if (wifiConnected) client.loop();
  itemsLoop();
  checkSerial();
}
