#include <NeoPixelBus.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <TZ.h>
#include <coredecls.h> // required for settimeofday_cb() (NTP sync callback)
#include <BlynkSimpleEsp8266.h>

#include "defines.h"

// NTP status and vars
boolean timeIsSet = false;
time_t lastNtpSet = 0;
time_t currentTime = time(nullptr); // time_t = seconds since epoch
struct tm * timeinfo;
time_t previousEffectTime = time(nullptr);

int previousClockSecond = -1;
int millisOffset = 0; // Offset compared to millis() to get partial seconds in sync with the NTP seconds

char ssid[60];
char wifiPassword[60];

char blynkServer[60];
int blynkPort = 9905;
char blynkAuth[60];

// Global color from blynk
int red = 0;
int green = 0;
int blue = 0;

// Number of rings
#define RINGS 9

int brightness; // Brightness between 0 and 255

// For Esp8266, the Pin is omitted and it uses GPIO3(Rx) due to DMA hardware use.
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PIXEL_COUNT);

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// Record the NPT set time
void timeUpdated() {
  timeIsSet = true;
  lastNtpSet = time(nullptr);
  Serial.print("NTP Updated: "); Serial.println(ctime(&lastNtpSet));
}

void clearStrip() {
  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();
}

void handlingDelay(int delayMillis) {
  for (int i = 0; i < delayMillis; i += 10) {
    delay(10);
    ArduinoOTA.handle();
    server.handleClient();
    Blynk.run();

    // Make sure the software watchdog does not trigger
    ESP.wdtFeed();
  }
}

void setPixel(int pixelNumber, RgbColor color) {
  int red = min((int)color.R, MAX_BRIGHTNESS);
  int green = min((int)color.G, MAX_BRIGHTNESS);
  int blue = min((int)color.B, MAX_BRIGHTNESS);
  RgbColor limitedColor = RgbColor(red, green, blue);
  strip.SetPixelColor(pixelNumber, limitedColor);
}

void setAllPixels(RgbColor color) {
  for(int i = 0; i<PIXEL_COUNT; i++) {
    setPixel(i, color);
  }
}

void setRandomSeed() {
  uint32_t seed;

  // random works best with a seed that can use 31 bits
  // analogRead on a unconnected pin tends toward less than four bits
  seed = analogRead(0);
  delay(1);

  for (int shifts = 3; shifts < 31; shifts += 3) {
    seed ^= analogRead(0) << shifts;
    delay(1);
  }

  // Serial.println(seed);
  randomSeed(seed);
}

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial attach

  // For the ESP the flash has to be read to a buffer
  EEPROM.begin(512);

  strip.Begin();
  strip.Show();

  // Setup the wifi
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(WIFI_PASSWORD_ADDR, wifiPassword);
  Serial.print("\r\nConnecting to WIFI '");
  Serial.print(String(ssid));
  Serial.print("'...\r\n");
  strip.SetPixelColor(0, RgbColor(0, 0, 50));
  strip.Show();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(String(ssid), String(wifiPassword));
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Continuing...");
    strip.SetPixelColor(0, RgbColor(50, 0, 0));
    strip.Show();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(HOSTNAME);
  } else {
    Serial.println("Connected to WIFI...");
    strip.SetPixelColor(0, RgbColor(0, 50, 0));
    strip.Show();
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(1000);

  // Setup Blynk
  EEPROM.get(BLYNK_SERVER_ADDR, blynkServer);
  EEPROM.get(BLYNK_AUTH_ADDR, blynkAuth);

  // implement NTP update of timekeeping (with automatic hourly updates)
  configTime(MY_TZ, NTP_SERVERS);

  // callback, when NTP changes the time
  settimeofday_cb(timeUpdated);

  setupOTA();

  EEPROM.get(BRIGHTNESS_ADDR, brightness);

  // Setup the web server
  httpUpdater.setup(&server);
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.begin();

  // Setup Blynk
  Blynk.config(blynkAuth, blynkServer, blynkPort);

  clearStrip();

  setRandomSeed();

  Serial.println();
  Serial.println("Running...");
}

void executeEffect(int choice) {
  switch (choice) {
    case 0:
      sparkle();
      break;
  }
}

BLYNK_WRITE(V0) {
  red = param.asInt();
}
BLYNK_WRITE(V1) {
  green = param.asInt();
}
BLYNK_WRITE(V2) {
  blue = param.asInt();
}

void loop() {
  currentTime = time(nullptr); // time_t = seconds since epoch

  if (previousEffectTime != currentTime) {
    previousEffectTime = currentTime;
    if (random(50) == 0) {
      int effectChoice = random(1);
      executeEffect(effectChoice);
    }
  }

  // Follow blynk color
  setAllPixels(RgbColor(red, green, blue));

  // Show Blynk status
  if (!Blynk.connected()) {
    setPixel(1, RgbColor(50, 0, 0));
  } else {
    setPixel(1, RgbColor(0, 50, 0));
  }
  strip.Show();

  ArduinoOTA.handle();
  server.handleClient();
  Blynk.run();  
}
