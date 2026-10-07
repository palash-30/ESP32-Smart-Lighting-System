#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// =====================================================
// WiFi Credentials
// =====================================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// =====================================================
// Firebase
// =====================================================
const char* FIREBASE_HOST = "YOUR_FIREBASE_DATABASE_URL";
const char* FIREBASE_AUTH = "YOUR_FIREBASE_AUTH_KEY";

// Example:
// https://your-project-default-rtdb.firebaseio.com
// Do NOT add / at the end.

// =====================================================
// Google Apps Script
// =====================================================
const char* GOOGLE_SCRIPT_URL = "YOUR_GOOGLE_APPS_SCRIPT_URL";

// =====================================================
// GPIO Pins
// =====================================================
#define PIR_PIN 13
#define RELAY_PIN 14

// =====================================================
// Web Server
// =====================================================
WebServer server(80);

// =====================================================
// Variables
// =====================================================
int motionCount = 0;
int lastPIRState = LOW;

bool lightState = false;
bool autoMode = true;

// =====================================================
// Timers
// =====================================================
unsigned long lastFiveMinuteReport = 0;

const unsigned long REPORT_INTERVAL = 300000UL; // 5 minutes

// Number of motions since last email
int motionsSinceLastReport = 0;

// =====================================================
// NTP Time
// India = UTC + 5:30
// =====================================================
const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;

// =====================================================
// Relay Control
// Active LOW Relay
// LOW  = Relay ON
// HIGH = Relay OFF
// =====================================================

void lightON() {

  digitalWrite(RELAY_PIN, LOW);
  lightState = true;
}

void lightOFF() {

  digitalWrite(RELAY_PIN, HIGH);
  lightState = false;
}

// =====================================================
// Get Current Date
// =====================================================

String getDate() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "Unknown";
  }

  char buffer[20];

  strftime(buffer, sizeof(buffer), "%d-%m-%Y", &timeinfo);

  return String(buffer);
}

// =====================================================
// Get Current Time
// =====================================================

String getTimeNow() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "Unknown";
  }

  char buffer[20];

  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

  return String(buffer);
}

// =====================================================
// Get Current Timestamp
// =====================================================

String getTimestamp() {

  return getDate() + " " + getTimeNow();
}

// =====================================================
// Firebase Logging
// =====================================================

void logToFirebase(String eventType) {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Firebase: WiFi not connected");

    return;
  }

  HTTPClient http;

  WiFiClientSecure client;

  client.setInsecure();

  String url = String(FIREBASE_HOST) +
               "/SmartLighting/Logs.json?auth=" +
               FIREBASE_AUTH;

  http.begin(client, url);

  http.addHeader("Content-Type", "application/json");

  String json = "{";

  json += "\"date\":\"" + getDate() + "\",";
  json += "\"time\":\"" + getTimeNow() + "\",";
  json += "\"timestamp\":\"" + getTimestamp() + "\",";
  json += "\"event\":\"" + eventType + "\",";
  json += "\"motionCount\":" + String(motionCount) + ",";
  json += "\"light\":\"" + String(lightState ? "ON" : "OFF") + "\",";
  json += "\"mode\":\"" + String(autoMode ? "AUTO" : "MANUAL") + "\",";
  json += "\"pir\":\"" +
          String(digitalRead(PIR_PIN) ? "MOTION" : "NO MOTION") +
          "\"";

  json += "}";

  int httpCode = http.POST(json);

  Serial.print("Firebase Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String response = http.getString();

    Serial.print("Firebase Response: ");
    Serial.println(response);
  }

  http.end();
}

// =====================================================
// Google Sheets Logging
// =====================================================

void logToGoogleSheet(String eventType) {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Google Sheet: WiFi not connected");

    return;
  }

  HTTPClient http;

  String url = String(GOOGLE_SCRIPT_URL);

  url += "?date=" + getDate();
  url += "&time=" + getTimeNow();
  url += "&event=" + eventType;
  url += "&motion=" + String(motionCount);
  url += "&light=" + String(lightState ? "ON" : "OFF");
  url += "&mode=" + String(autoMode ? "AUTO" : "MANUAL");
  url += "&pir=" +
         String(digitalRead(PIR_PIN) ? "MOTION" : "NO_MOTION");

  http.begin(url);

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();

  Serial.print("Google Sheet Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String response = http.getString();

    Serial.print("Google Sheet Response: ");
    Serial.println(response);
  }

  http.end();
}

// =====================================================
// Log Event to Both Firebase + Google Sheet
// =====================================================

void logEvent(String eventType) {

  Serial.println();
  Serial.println("==============================");
  Serial.println("LOGGING EVENT");
  Serial.println("==============================");

  Serial.print("Date: ");
  Serial.println(getDate());

  Serial.print("Time: ");
  Serial.println(getTimeNow());

  Serial.print("Event: ");
  Serial.println(eventType);

  Serial.print("Motion Count: ");
  Serial.println(motionCount);

  Serial.print("Light: ");
  Serial.println(lightState ? "ON" : "OFF");

  Serial.print("Mode: ");
  Serial.println(autoMode ? "AUTO" : "MANUAL");

  // Google Sheets
  logToGoogleSheet(eventType);

  // Firebase
  logToFirebase(eventType);

  Serial.println("==============================");
}

// =====================================================
// Send 5-Minute Email Report
// =====================================================

void sendFiveMinuteReport() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Email Report: WiFi not connected");

    return;
  }

  HTTPClient http;

  String url = String(GOOGLE_SCRIPT_URL);

  url += "?report=5min";
  url += "&date=" + getDate();
  url += "&time=" + getTimeNow();
  url += "&motion=" + String(motionsSinceLastReport);
  url += "&totalMotion=" + String(motionCount);
  url += "&light=" + String(lightState ? "ON" : "OFF");
  url += "&mode=" + String(autoMode ? "AUTO" : "MANUAL");
  url += "&ip=" + WiFi.localIP().toString();

  Serial.println();
  Serial.println("==============================");
  Serial.println("SENDING 5-MINUTE REPORT");
  Serial.println("==============================");

  http.begin(url);

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();

  Serial.print("Email Report Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String response = http.getString();

    Serial.print("Email Report Response: ");
    Serial.println(response);
  }

  http.end();

  // Reset 5-minute counter
  motionsSinceLastReport = 0;

  Serial.println("5-minute counter reset.");
}

// =====================================================
// Web Dashboard
// =====================================================

void handleRoot() {

  String html = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>ESP32 Smart Lighting</title>

<style>

body {
  font-family: Arial, sans-serif;
  background: #f2f4f7;
  text-align: center;
  margin: 0;
  padding: 20px;
}

.container {
  max-width: 500px;
  margin: auto;
  background: white;
  padding: 25px;
  border-radius: 20px;
  box-shadow: 0 4px 15px rgba(0,0,0,0.15);
}

h1 {
  margin-bottom: 5px;
}

.status {
  font-size: 24px;
  font-weight: bold;
  margin: 20px;
}

.on {
  color: green;
}

.off {
  color: red;
}

button {
  width: 90%;
  padding: 15px;
  margin: 8px;
  border: none;
  border-radius: 10px;
  font-size: 18px;
  cursor: pointer;
}

.onButton {
  background: green;
  color: white;
}

.offButton {
  background: red;
  color: white;
}

.autoButton {
  background: #007bff;
  color: white;
}

.info {
  background: #f5f5f5;
  padding: 15px;
  border-radius: 10px;
  margin-top: 20px;
}

</style>

<script>

function control(url) {

  fetch(url)
  .then(response => response.text())
  .then(data => {

    location.reload();

  });

}

</script>

</head>

<body>

<div class="container">

<h1>ESP32 Smart Lighting</h1>

<p>PIR + Relay + IoT Monitoring</p>

<hr>

)rawliteral";

  // ===================================================
  // Light Status
  // ===================================================

  html += "<div class='status'>";

  if (lightState) {

    html += "<span class='on'>LIGHT ON</span>";

  } else {

    html += "<span class='off'>LIGHT OFF</span>";
  }

  html += "</div>";

  // ===================================================
  // Mode
  // ===================================================

  html += "<h3>Control Mode: ";

  html += autoMode ? "AUTO" : "MANUAL";

  html += "</h3>";

  // ===================================================
  // Buttons
  // ===================================================

  html += R"rawliteral(

<button class="onButton"
onclick="control('/on')">
LIGHT ON
</button>

<button class="offButton"
onclick="control('/off')">
LIGHT OFF
</button>

<button class="autoButton"
onclick="control('/auto')">
AUTO MODE
</button>

<div class="info">

)rawliteral";

  html += "<p><b>Motion Count:</b> ";
  html += String(motionCount);
  html += "</p>";

  html += "<p><b>5-Min Motion Count:</b> ";
  html += String(motionsSinceLastReport);
  html += "</p>";

  html += "<p><b>PIR Status:</b> ";

  if (digitalRead(PIR_PIN) == HIGH) {

    html += "MOTION DETECTED";

  } else {

    html += "NO MOTION";
  }

  html += "</p>";

  html += "<p><b>ESP32 IP:</b> ";
  html += WiFi.localIP().toString();
  html += "</p>";

  html += R"rawliteral(

</div>

</div>

</body>
</html>

)rawliteral";

  server.send(200, "text/html", html);
}

// =====================================================
// Manual ON
// =====================================================

void handleON() {

  autoMode = false;

  lightON();

  Serial.println("Web Control: LIGHT ON");

  logEvent("MANUAL_LIGHT_ON");

  server.send(200, "text/plain", "Light ON");
}

// =====================================================
// Manual OFF
// =====================================================

void handleOFF() {

  autoMode = false;

  lightOFF();

  Serial.println("Web Control: LIGHT OFF");

  logEvent("MANUAL_LIGHT_OFF");

  server.send(200, "text/plain", "Light OFF");
}

// =====================================================
// AUTO Mode
// =====================================================

void handleAUTO() {

  autoMode = true;

  Serial.println("Control Mode: AUTO");

  logEvent("AUTO_MODE");

  server.send(200, "text/plain", "AUTO MODE");
}

// =====================================================
// Setup
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);

  // Light OFF initially
  lightOFF();

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 SMART LIGHTING SYSTEM");
  Serial.println("==============================");

  // ===================================================
  // WiFi
  // ===================================================

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("WiFi Connected!");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // ===================================================
  // NTP Time
  // ===================================================

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov"
  );

  Serial.println("Synchronizing time...");

  delay(2000);

  Serial.print("Current Time: ");
  Serial.println(getTimestamp());

  // ===================================================
  // Web Server Routes
  // ===================================================

  server.on("/", handleRoot);

  server.on("/on", handleON);

  server.on("/off", handleOFF);

  server.on("/auto", handleAUTO);

  // ===================================================
  // Start Web Server
  // ===================================================

  server.begin();

  Serial.println("Web Server Started!");

  Serial.println("Open the IP address in your browser.");

  Serial.println("==============================");

  // Start 5-minute timer
  lastFiveMinuteReport = millis();
}

// =====================================================
// Main Loop
// =====================================================

void loop() {

  // Handle web requests
  server.handleClient();

  // ===================================================
  // PIR Automatic Control
  // ===================================================

  if (autoMode) {

    int pirState = digitalRead(PIR_PIN);

    // Detect LOW → HIGH transition
    if (pirState == HIGH && lastPIRState == LOW) {

      motionCount++;

      motionsSinceLastReport++;

      Serial.println();
      Serial.println("Motion Detected!");

      Serial.print("Motion Count = ");
      Serial.println(motionCount);

      // ===============================================
      // Odd Motion = ON
      // ===============================================

      if (motionCount % 2 == 1) {

        lightON();

        Serial.println("AUTO: LIGHT ON");

        logEvent("MOTION_LIGHT_ON");

      }

      // ===============================================
      // Even Motion = OFF
      // ===============================================

      else {

        lightOFF();

        Serial.println("AUTO: LIGHT OFF");

        logEvent("MOTION_LIGHT_OFF");
      }

      Serial.println("--------------------------");

      // Prevent duplicate detection
      delay(1000);
    }

    lastPIRState = pirState;
  }

  // ===================================================
  // 5-MINUTE EMAIL REPORT
  // ===================================================

  if (millis() - lastFiveMinuteReport >= REPORT_INTERVAL) {

    lastFiveMinuteReport = millis();

    sendFiveMinuteReport();
  }

  delay(50);
}