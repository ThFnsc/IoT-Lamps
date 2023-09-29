#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ShiftRegister74HC595.h>
#include <ShiftIn.h>
#include <env.h>

#define SWITCH_2F_DELAY 500

WiFiClient espClient;
PubSubClient mqtt(espClient);
ShiftRegister74HC595<2> sr(D8, D7, D6);
ShiftIn<2> si;

byte nLamps;
byte nSensors;

class Lamp
{
private:
    byte pinIn;
    byte pinOut;
    bool lastButtonState;
    unsigned long lastStateChange;
    unsigned long btnPressed;
    byte *switchOrder;
    short lastLamp;

public:
    const char *mqttCommand;
    const char *mqttStatus;
    bool state;

    Lamp(byte pinIn, byte pinOut, const char *mqttCommand, const char *mqttStatus)
    {
        this->pinIn = pinIn;
        this->pinOut = pinOut;
        this->state = false;
        this->mqttCommand = mqttCommand;
        this->mqttStatus = mqttStatus;
        this->btnPressed = 0;
        lastButtonState = 0;
    }

    void setup()
    {
        switchOrder = (byte *)malloc(sizeof(byte) * nLamps);
        for (int i = 0; i < nLamps; i++)
            switchOrder[i] = i;
        set(false);
    }

    void loop(Lamp *lamps)
    {
        byte btnState = si.state(pinIn);
        if (btnState != lastButtonState)
        {                                                                                           // Button was pressed or released
            lastButtonState = btnState;                                                             // Sets history
            if (!btnState && millis() - btnPressed > 50 && millis() - btnPressed < SWITCH_2F_DELAY) // If the button was released and spent at least 50ms pressed (avoid registering electrical noise)
                invert();

            if (!btnState)                        // Button was released
                btnPressed = 0;                   // Resets timer
            else if (btnState && btnPressed == 0) // Button is pressed and no timer was set yet
                btnPressed = millis();            // Sets the timer
        }

        if (btnState)
        {                                                        // Button is currently pressed
            int pos = (millis() - btnPressed) / SWITCH_2F_DELAY; // Get the time interval divider
            if (pos <= 0)
            {                  // Still early to enable second function
                lastLamp = -1; // Resets counter at this stage, because why not
            }
            else
            {          // Passed initial threshold, so 2nd function is running
                pos--; // So it can be used to index the lamps
                if (lastLamp != pos)
                { // Didn't execute current cycle yet
                    if (pos == 0)
                        Serial.printf("Second function running for switch %s\n", mqttStatus);
                    if (lastLamp >= 0 && lastLamp < nLamps)    // Whether just started or needs to revert last lamp (and hasn't overflowed yet)
                        lamps[switchOrder[lastLamp]].invert(); // Reverts last lamp to the original state
                    if (pos < nLamps)                          // Make sure it hasn't overflowed
                        lamps[switchOrder[pos]].invert();      // Inverts state of the current lamp

                    lastLamp = pos;
                }
            }
        }
    }

    void invert()
    {
        set(!state);
    }

    void set(bool state, bool updateCommandTopic = true)
    {
        if (state == this->state)
            return;
        if (pinOut >= 0)
            sr.setNoUpdate(pinOut, !state);
        this->state = state;
        Serial.printf("Lamp %s set to %s\n", mqttStatus, state ? "ON" : "OFF");
        mqtt.publish(mqttStatus, state ? "ON" : "OFF", true);
        if (updateCommandTopic)
            mqtt.publish(mqttCommand, state ? "ON" : "OFF", true);
    }

    bool subscribe()
    {
        bool success = mqtt.subscribe(mqttCommand);
        Serial.printf(success ? "Subscribed to %s successfully\n" : "Failed to subscribe to %s\n", mqttCommand);
        return success;
    }
};

class Sensor
{
private:
    byte pinIn;
    unsigned long lastChangeTime;
    bool lastState;

public:
    const char *mqttStatus;

    Sensor(byte pinIn, const char *mqttStatus)
    {
        this->pinIn = pinIn;
        this->mqttStatus = mqttStatus;
        this->lastChangeTime = 1;
        this->lastState = LOW;
    }

    void setup()
    {
        lastState = !si.state(pinIn);
    }

    void loop()
    {
        bool currentState = si.state(pinIn);

        if (currentState != lastState)
        {
            if (lastChangeTime == 0)
                lastChangeTime = millis();
            else if (millis() - lastChangeTime >= 50)
                mqtt.publish(mqttStatus, (lastState = currentState) ? "ON" : "OFF", true);
            else
                lastChangeTime = 0;
        }
    }
};

Lamp lamps[] = {
    // IN, OUT
    Lamp(14, 4, "kitchen/command", "kitchen/state"),
    Lamp(10, 2, "dining/command", "dining/state"),
    Lamp(12, 5, "livingroom/command", "livingroom/state"),
    Lamp(11, 6, "balcony/command", "balcony/state"),
    Lamp(9, 3, "hallway/command", "hallway/state"),
    Lamp(13, 1, "kitchenisland/command", "kitchenisland/state"),
    Lamp(15, 7, "bathroom/command", "bathroom/state"),
    Lamp(8, 0, "bedroom/command", "bedroom/state"),
    Lamp(3, 11, "guestbedroom/command", "guestbedroom/state"),
    Lamp(1, 10, "ensuitebathroom/command", "ensuitebathroom/state"),
    Lamp(2, 9, "masterbedroom/command", "masterbedroom/state"),
    Lamp(5, -1, "livingroomspotlights/command", "livingroomspotlights/state")};

Sensor sensors[] = {
    Sensor(7, "home/doorsensor"),
    Sensor(4, "home/doorbell")};

unsigned long loopHalfSecond = 0;
bool wifiConnected = false;

void mqttCallback(char *topicRaw, byte *payload, unsigned int length)
{
    char body[length + 1];
    for (unsigned int i = 0; i < length; i++)
        body[i] = payload[i];
    body[length] = 0;
    Serial.printf("MQTT [%s]=[%s]\n", topicRaw, body);

    for (int i = 0; i < nLamps; i++)
        if (strcmp(lamps[i].mqttCommand, topicRaw) == 0)
        {
            lamps[i].set(strcmp("ON", body) == 0, false);
            break;
        }
}

unsigned long lastCheck;
unsigned long nextReport = 0;

bool isWifiConnected() { return WiFi.status() == 3; }

void ConnectionLoop()
{
    mqtt.loop();
    if (loopHalfSecond < millis())
    {
        if (wifiConnected != isWifiConnected())
        {
            wifiConnected = isWifiConnected();
            if (wifiConnected)
            {
                Serial.printf("WiFi connected after %ldms\n", millis());
                ArduinoOTA.begin();
            }
            else
            {
                Serial.println("WiFi connection lost");
            }
        }
        if (wifiConnected)
        {
            if (!mqtt.connected())
            {
                Serial.println("Connecting to MQTT broker... ");
                if (mqtt.connect(MQTT_NAME, MQTT_USER, MQTT_PASSWD))
                {
                    Serial.println("Connected to MQQT broker");

                    for (int i = 0; i < nLamps; i++)
                        lamps[i].subscribe();
                }
                else
                {
                    Serial.println("Failed, trying again in 10 seconds");
                    loopHalfSecond = millis() + 10000;
                }
            }
        }
        else
        {
            Serial.println("Waiting for wifi...");
            lamps[1].invert();
        }
        loopHalfSecond = millis() + 500;
    }
}

void itemsSetup()
{
    nLamps = sizeof(lamps) / sizeof(lamps[0]);
    nSensors = sizeof(sensors) / sizeof(sensors[0]);

    sr.setAllHigh();
    si.begin(D5, D1, D4);

    for (int i = 0; i < nLamps; i++)
        lamps[i].setup();

    for (int i = 0; i < nSensors; i++)
        sensors[i].setup();
}

void ItemsLoop()
{
    si.update();

    for (int i = 0; i < nLamps; i++)
        lamps[i].loop(lamps);

    for (int i = 0; i < nSensors; i++)
        sensors[i].loop();

    sr.updateRegisters();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    itemsSetup();

    Serial.println("Connecting to WiFi...");

    mqtt.setCallback(mqttCallback);
    mqtt.setServer(MQTT_HOST, MQTT_PORT);

    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname("Lights");

    ArduinoOTA.onStart([]()
                       { Serial.println("Start"); });
    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
                       Serial.printf("Error[%u]: ", error);
                       if (error == OTA_AUTH_ERROR)
                         Serial.println("Auth Failed");
                       else if (error == OTA_BEGIN_ERROR)
                         Serial.println("Begin Failed");
                       else if (error == OTA_CONNECT_ERROR)
                         Serial.println("Connect Failed");
                       else if (error == OTA_RECEIVE_ERROR)
                         Serial.println("Receive Failed");
                       else if (error == OTA_END_ERROR)
                         Serial.println("End Failed"); });
}

void loop()
{
    ArduinoOTA.handle();
    ConnectionLoop();
    ItemsLoop();
}
