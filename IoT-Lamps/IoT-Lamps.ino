#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <PubSubClient.h>
#include <ShiftRegister74HC595.h>
#include <EEPROM.h>
#include <ShiftIn.h>

#define WIFI "<WIFI SSID>"
#define PASSWD "<WIFI PASSWD>"
#define SVRIP "<MQTT server IP>"
#define PORT 1883
#define BOOT_ANIMATION true
#define LAMPS sizeof(lamps) / sizeof(lamps[0])
#define MORSE_LAMP 1

#define PIN_BUZZER 3

void setLamp(byte pos, bool value, bool report = true);
void callback(char *topic, byte *payload, unsigned int length);
void log(String msg, bool lineBreak = true, bool timestamp = true);
int translate(int inPin);

struct Lamp
{
  byte pin;
  float watts;
  char lightTopic[25];
  char switchTopic[25];
  bool bedroom;
  bool ringSwitch;
  bool lightOn;
  bool switchPos;
  unsigned long nextChange;
  byte lampChange;
  bool lastOn;
} lamps[] = {
    {3, 2 * 12, "kitchen/light", "kitchen/switch", false, true},
    {1, 2 * 14 + 15, "dining/light", "dining/switch", false, true},
    {4, 2 * 4.5, "livingroom/light", "livingroom/switch", false, true},
    {5, 2 * 5, "balcony/light", "balcony/switch", false, true},
    {2, 2 * 14, "hallway/light", "hallway/switch", false, true},
    {6, 2 * 5, "bathroom/light", "bathroom/switch", false, true},
    {0, 2 * 4.5, "masterbedroom/light", "masterbedroom/switch", true, true}};

WiFiClient espClient;
PubSubClient client(SVRIP, PORT, callback, espClient);
ShiftRegister74HC595 sr(1, translate(8), translate(7), translate(6));
ShiftIn<1> si;

byte morse[] = {B10001011, B10101000, B10001110, B10001011, B10001110, B11101110, B00000000, B00000000};
byte morseLamps[] = {0, 3};
bool morseRunning = false;
int morsePos = 0;
unsigned long nextMorse;

unsigned long lastMsg = 0;
unsigned long loopHalfSecond = 0;
unsigned long periodicWattageUpdate = 0;
unsigned long lastLightUpdate = 0;
unsigned long nextSpooky;
bool wifiConnected = false;
bool spooky = false;
bool spookyHolding = false;
bool silent = false;
String serialIn;
char msg[50];
char converBuffer[15];
float wattsUsing = 0;
float wattsHour = 0;
float reaisPerKWh = 0;
int value = 0;
byte fails = 0;
