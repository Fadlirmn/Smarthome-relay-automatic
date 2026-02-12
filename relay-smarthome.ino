#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

/* ================= WIFI ================= */
const char* ssid = "Your_ssid";
const char* pass = "Your_password";

/* ================= MQTT EMQX ================= */
const char* mqttServer   = "emqxsl.com";
const int   mqttPort     = 8883;//ssl kalo g pake ssl pake 1883
const char* mqttUser     = "username";
const char* mqttPassword = "password";

/* ================= WEATHER ================= */
const char* weatherApi =
"http://api.openweathermap.org/data/2.5/weather?q=City,country&appid=Your_API&units=metric";


/* ================= LCD ================= */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ================= CUSTOM ICON ================= */

// Relay ON
byte relayOnChar[8] = {
  0b00000,0b01110,0b11111,0b11111,
  0b11111,0b01110,0b00000,0b00000
};

// Relay OFF
byte relayOffChar[8] = {
  0b00000,0b01110,0b10001,0b10001,
  0b10001,0b01110,0b00000,0b00000
};

// Charging
byte chargeChar[8] = {
  0b00100,0b01100,0b11110,0b01100,
  0b00110,0b00100,0b00000,0b00000
};

/* ================= RELAY PIN (ESP32) ================= */
const uint8_t relayPin[4] = {18, 19, 23, 5};

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define BATTERY_ON_LEVEL   50
#define BATTERY_OFF_LEVEL  80

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

/* ================= STATE ================= */
bool relayState[4] = {0,0,0,0};

float currentTemp = -100.0;
String currentWeather = "Loading";

int serverBattery = -1;
String serverPower = "";

unsigned long lastBatteryUpdate = 0;
bool serverConnected = false;

unsigned long weatherTimer = 0;
unsigned long lcdTimer = 0;

/* ================= RELAY ================= */
void updateRelayGPIO() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(relayPin[i], relayState[i] ? RELAY_ON : RELAY_OFF);
  }
}

/* ================= MQTT CALLBACK ================= */
void mqttCallback(char* topic, byte* payload, unsigned int len) {

  String msg;
  for (unsigned int i = 0; i < len; i++)
    msg += (char)payload[i];

  if (String(topic).startsWith("home/relay/")) {
    int ch = topic[strlen(topic)-1] - '1';
    if (ch == 0) return;
    if (ch >= 1 && ch < 4) {
      relayState[ch] = msg == "1";
      updateRelayGPIO();
    }
    return;
  }

  if (String(topic) == "server/battery") {
    DynamicJsonDocument doc(256);
    if (!deserializeJson(doc, msg)) {
      serverBattery = doc["capacity"];
      serverPower   = doc["status"].as<String>();
      lastBatteryUpdate = millis();
      serverConnected = true;
    }
  }
}

/* ================= SERVER TIMEOUT ================= */
void checkServerConnection() {
  if (millis() - lastBatteryUpdate > 120000) {
    serverConnected = false;
  }
}

/* ================= BATTERY CONTROL ================= */
void controlRelayByBattery() {
  if (serverBattery < 0) return;

  if (serverBattery <= BATTERY_ON_LEVEL && !relayState[0]) {
    relayState[0] = true;
    updateRelayGPIO();
  }

  if (serverBattery >= BATTERY_OFF_LEVEL && relayState[0]) {
    relayState[0] = false;
    updateRelayGPIO();
  }
}

/* ================= WEATHER ================= */
void updateWeather() {

  if (millis() - weatherTimer < 600000) return;
  weatherTimer = millis();

  HTTPClient http;
  http.begin(weatherApi);

  if (http.GET() == 200) {
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, http.getString())) {
      currentTemp = doc["main"]["temp"].as<float>();
      currentWeather = doc["weather"][0]["main"].as<String>();
    }
  }
  http.end();
}

/* ================= LCD ================= */
void drawLCD() {

  time_t now = time(nullptr);
  if (now < 100000) return;

  struct tm* t = localtime(&now);

  lcd.clear();
  lcd.setCursor(0,0);

  // Time
  if (t->tm_hour < 10) lcd.print("0");
  lcd.print(t->tm_hour);
  lcd.print(":");
  if (t->tm_min < 10) lcd.print("0");
  lcd.print(t->tm_min);

  lcd.print(" ");

  // Relay
  for (int i = 0; i < 4; i++)
    lcd.write(relayState[i] ? byte(0) : byte(1));

  lcd.print(" ");

  // Battery
  if (serverBattery >= 0) {
    lcd.print(serverBattery);
    lcd.print("%");
  } else lcd.print("--%");

  // Server status
  if (serverConnected) lcd.print("OK");
  else lcd.print("X");

  // Line 2
  lcd.setCursor(0,1);

  if (currentTemp > -50) {
    lcd.print(currentTemp,1);
    lcd.print("C ");
  } else lcd.print("--C ");

  lcd.print(currentWeather);
}

/* ================= SETUP ================= */
void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22); // SDA, SCL ESP32

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, relayOnChar);
  lcd.createChar(1, relayOffChar);
  lcd.createChar(2, chargeChar);

  for (int i = 0; i < 4; i++) {
    pinMode(relayPin[i], OUTPUT);
    digitalWrite(relayPin[i], RELAY_OFF);
  }

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) delay(500);

  espClient.setInsecure();

  mqtt.setServer(mqttServer, mqttPort);
  mqtt.setCallback(mqttCallback);

  while (!mqtt.connected()) {
    String clientId = "esp32-";
    clientId += String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      mqtt.subscribe("home/relay/#");
      mqtt.subscribe("server/battery");
    } else delay(5000);
  }

  weatherTimer = millis() - 700000;
  updateWeather();
}

/* ================= LOOP ================= */
void loop() {

  if (!mqtt.connected()) {
    serverConnected = false;
    mqtt.connect("esp32-reconnect", mqttUser, mqttPassword);
    mqtt.subscribe("home/relay/#");
    mqtt.subscribe("server/battery");
  }

  mqtt.loop();

  controlRelayByBattery();
  checkServerConnection();
  updateWeather();

  if (millis() - lcdTimer > 1000) {
    lcdTimer = millis();
    drawLCD();
  }
}
