#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "DHT.h"

// WiFi + Telegram Credentials
const char* ssid = "--";
const char* password = "--"; 
String botToken = "--"; 
String chatID = "--";

// Sensors
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define MQ2_PIN 34
#define FLAME_PIN 35

DHT dht(DHT_PIN, DHT_TYPE);

// Sensitivity Settings
#define GAS_THRESHOLD 1000   
#define TEMP_THRESHOLD 35.0
#define FIRE_DETECT_COUNT 2  

int fireCount = 0;
bool alertSent = false;
unsigned long lastMeasureTime = 0;
const long interval = 2000; // Measure every 2 seconds

void sendTelegramMessage(String msg) {
  Serial.println("📱 SENDING TELEGRAM...");
  WiFiClientSecure client_temp;
  client_temp.setInsecure(); 
  
  HTTPClient http;
  // Proper URL Encoding for Telegram
  String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + msg;
  url.replace(" ", "%20");
  url.replace("\n", "%0A");

  if (http.begin(client_temp, url)) {
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("📱 HTTP Response: %d\n", httpCode);
    } else {
      Serial.printf("❌ Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(FLAME_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);
  
  Serial.print("Connecting WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  
  sendTelegramMessage("🔥 Fire Monitoring System Online");
}

void loop() {
  unsigned long currentMillis = millis();

  // Non-blocking timer instead of delay()
  if (currentMillis - lastMeasureTime >= interval) {
    lastMeasureTime = currentMillis;

    float temp = dht.readTemperature();
    int gas = analogRead(MQ2_PIN);
    int flame = digitalRead(FLAME_PIN); // Usually 0 is flame, 1 is safe

    if (isnan(temp)) {
      Serial.println("⚠️ DHT Sensor Error");
      temp = 0;
    }

    bool flameDetected = (flame == LOW); // LOW (0) means IR detected a flame
    bool gasHigh = (gas > GAS_THRESHOLD);
    bool tempHigh = (temp > TEMP_THRESHOLD);

    // Debugging output
    Serial.println("--- Reading ---");
    Serial.printf("Temp: %.1f | Gas: %d | Flame: %s\n", temp, gas, flameDetected ? "YES" : "NO");

    // CRITICAL LOGIC: If ANY sensor detects fire, increase count
    if (flameDetected || gasHigh || tempHigh) {
      fireCount++;
      Serial.printf("🔥 Alert Level: %d/%d\n", fireCount, FIRE_DETECT_COUNT);
    } else {
      // Slow reset to prevent "flickering" sensor data from stopping the alert
      if (fireCount > 0) fireCount--; 
    }

    // Trigger Alert
    if (fireCount >= FIRE_DETECT_COUNT && !alertSent) {
      String message = "🚨 FIRE ALERT! 🚨\n";
      if (flameDetected) message += "- FLAME DETECTED!\n";
      if (gasHigh) message += "- HIGH SMOKE/GAS!\n";
      message += "Temp: " + String(temp, 1) + "C";
      
      sendTelegramMessage(message);
      alertSent = true;
    }

    // Reset when everything is safe for a while
    if (!flameDetected && !gasHigh && !tempHigh && alertSent) {
       if (fireCount == 0) {
         sendTelegramMessage("✅ Area is now safe.");
         alertSent = false;
       }
    }
  }
}
