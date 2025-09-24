#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

// ===== Wi-Fi Credentials =====
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// ===== OpenWeatherMap API Setup =====
const char* apiKey = "8528f609167426822d208e161fa9b095";
const float latitude = 3.8476;
const float longitude = 32.2895;
String weatherUnits = "metric";

// ===== Hardware Setup =====
#define BUZZER_PIN 13

// ===== Web Server =====
WebServer server(80);

// ===== Global Variables & Hydration Settings =====
float weatherTemp = 0.0;
float weatherHumidity = 0.0;
String userGender = "male";
float userWeight = 60.0;
float waterGoal = 2000;
float intakeSoFar = 0.0;
String hydrationStatus = "Initializing...";
String lastDrinkTimeStr = "Not recorded";
String nextIntakeTimeStr = "Not calculated";
unsigned long lastWeatherFetch = 0;
const unsigned long weatherInterval = 15 * 60 * 1000UL;
const unsigned long defaultHydrationInterval = 60 * 60 * 1000UL;

unsigned long customInterval = defaultHydrationInterval; // User-customizable interval in milliseconds
bool manualTimeSet = false; // Flag to check if a manual time has been set
time_t nextDrinkTime = 0; // Timestamp for the next drink alert

// ===== HTML Page =====
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>💧 Hydration Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: #f0f8ff; text-align: center; padding: 20px; margin: 0; }
    .container { max-width: 600px; margin: auto; }
    .card { background: white; border-radius: 12px; padding: 25px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); margin-bottom: 20px; }
    h2 { color: #2c3e50; margin-bottom: 20px; }
    p { color: #555; margin: 10px 0; font-size: 1.1em; }
    span { font-weight: bold; color: #0077cc; }
    .progress { height: 25px; background: #e0e0e0; border-radius: 12px; margin-top: 15px; overflow: hidden; }
    .progress-bar { height: 100%; background: linear-gradient(90deg, #4caf50, #81c784); width: 0%; border-radius: 12px; transition: width 0.5s ease; }
    .btn { padding: 12px 25px; color: white; border: none; margin-top: 10px; border-radius: 8px; cursor: pointer; font-size: 1em; transition: background-color 0.3s; }
    .btn-refresh { background-color: #0077cc; }
    .btn-refresh:hover { background-color: #005fa3; }
    .btn-drink { background-color: #28a745; margin-left: 10px; }
    .btn-drink:hover { background-color: #218838; }
    .btn-reset { background-color: #e74c3c; margin-left: 10px; }
    .btn-reset:hover { background-color: #c0392b; }
    .input-group { margin: 15px 0; }
    .input-group input, .input-group select { padding: 10px; border-radius: 8px; border: 1px solid #ccc; width: 100px; text-align: center; font-size: 1em; }
    .input-group label { margin-right: 10px; color: #555; }
    .gender-select { display: flex; justify-content: center; align-items: center; gap: 20px; margin-top: 20px; }
    .gender-option { display: flex; align-items: center; gap: 5px; cursor: pointer; }
  </style>
</head>
<body>
  <div class="container">
    <h2>💧 Hydration Dashboard</h2>
    <div class="card">
      <p>🌡️ Local Temp: <span id="temp">--</span> °C | 💧 Humidity: <span id="humidity">--</span>%</p>
      <div class="input-group">
        <label for="weight-input">🏋️ Your Weight:</label>
        <input type="number" id="weight-input" min="1" step="0.1" value="60.0">
        <button class="btn btn-refresh" onclick="updateUser()">Set Weight</button>
      </div>
       <div class="gender-select">
        <div class="gender-option">
          <input type="radio" id="male" name="gender" value="male" onchange="updateUser()">
          <label for="male">Male</label>
        </div>
        <div class="gender-option">
          <input type="radio" id="female" name="gender" value="female" onchange="updateUser()">
          <label for="female">Female</label>
        </div>
      </div>
      <p>Gender: <b id="gender">--</b> | Current Weight: <span id="weight">--</span> kg</p>
    </div>
    <div class="card">
      <p>🧃 Intake: <span id="intake">--</span> ml / 🎯 Goal: <span id="goal">--</span> ml</p>
      <div class="progress"><div class="progress-bar" id="progressBar"></div></div>
      <p>Status: <b id="statusText">--</b></p>
      <p>⏱ Last Drink: <span id="lastdrink">--</span></p>
      <p>🔔 Next Drink: <span id="nextdrink">--</span></p>
    </div>
    <div class="card">
      <div class="input-group">
        <label for="next-time">Set Next Reminder Time:</label>
        <input type="time" id="next-time">
        <button class="btn btn-refresh" onclick="setNextReminder()">Set Time</button>
      </div>
      <div class="input-group">
        <label for="interval">Set Interval (min):</label>
        <input type="number" id="interval" value="60">
        <button class="btn btn-refresh" onclick="setInterval()">Set Interval</button>
      </div>
    </div>
    <button class="btn btn-drink" onclick="logDrink()">💧 I Drank 250ml</button>
    <button class="btn btn-reset" onclick="resetIntake()">🔄 Reset Intake</button>
  </div>
  <script>
    async function refresh() {
      const res = await fetch('/data');
      const data = await res.json();
      document.getElementById("temp").innerText = data.temp.toFixed(1);
      document.getElementById("humidity").innerText = data.humidity.toFixed(1);
      document.getElementById("weight").innerText = data.weight.toFixed(1);
      document.getElementById("intake").innerText = data.intake.toFixed(1);
      document.getElementById("goal").innerText = data.goal.toFixed(1);
      document.getElementById("lastdrink").innerText = data.lastDrink;
      document.getElementById("statusText").innerText = data.status;
      document.getElementById("nextdrink").innerText = data.nextDrink;
      document.getElementById("weight-input").value = data.weight;
      document.getElementById("gender").innerText = data.gender.charAt(0).toUpperCase() + data.gender.slice(1);
      document.getElementById(data.gender).checked = true;
      const pct = Math.min(100, (data.intake / data.goal) * 100);
      document.getElementById("progressBar").style.width = pct + "%";
    }
    
    async function updateUser() {
      const newWeight = document.getElementById("weight-input").value;
      const newGender = document.querySelector('input[name="gender"]:checked').value;
      try {
        await fetch(`/updateUser?weight=${newWeight}&gender=${newGender}`, { method: 'POST' });
        await refresh();
      } catch(err) {
        console.error("Error updating user info:", err);
      }
    }
    
    async function logDrink() {
      try {
        await fetch('/drink', { method: 'POST' });
        await refresh();
      } catch(err) {
        console.error("Error logging drink:", err);
      }
    }
    
    async function resetIntake() {
      try {
        await fetch('/reset', { method: 'POST' });
        await refresh();
      } catch(err) {
        console.error("Error resetting intake:", err);
      }
    }
    
    async function setNextReminder() {
      const timeInput = document.getElementById("next-time").value;
      await fetch(`/setNextTime?time=${timeInput}`, { method: 'POST' });
      await refresh();
    }
    
    async function setInterval() {
      const interval = document.getElementById("interval").value;
      await fetch(`/setInterval?minutes=${interval}`, { method: 'POST' });
      await refresh();
    }

    window.onload = refresh;
  </script>
</body>
</html>
)rawliteral";

// ===== Forward Declarations =====
void calculateHydrationGoal();
void fetchWeather();
void beepAlert();

// ===== Endpoint Handlers =====
void handleData() {
    StaticJsonDocument<512> doc;
    doc["temp"] = weatherTemp;
    doc["humidity"] = weatherHumidity;
    doc["gender"] = userGender;
    doc["weight"] = userWeight;
    doc["intake"] = intakeSoFar;
    doc["goal"] = waterGoal;
    doc["lastDrink"] = lastDrinkTimeStr;
    doc["status"] = hydrationStatus;
    doc["nextDrink"] = nextIntakeTimeStr;
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void handleDrink() {
    intakeSoFar += 250.0;
    
    time_t now = time(nullptr);
    struct tm* ptm = localtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%I:%M:%S %p", ptm);
    lastDrinkTimeStr = String(buffer);

    // Reset to interval-based reminder after logging a drink
    manualTimeSet = false; 
    nextDrinkTime = now + (customInterval / 1000);
    strftime(buffer, sizeof(buffer), "%I:%M:%S %p", localtime(&nextDrinkTime));
    nextIntakeTimeStr = String(buffer);
    
    calculateHydrationGoal();
    server.send(200, "text/plain", "OK");
    Serial.println("💧 Water intake logged: +250ml. New total: " + String(intakeSoFar) + "ml");
}

void handleUpdateUser() {
    if (server.hasArg("weight")) {
        float newWeight = server.arg("weight").toFloat();
        if (newWeight > 0) {
            userWeight = newWeight;
        }
    }
    if (server.hasArg("gender")) {
        String newGender = server.arg("gender");
        if (newGender == "male" || newGender == "female") {
            userGender = newGender;
        }
    }
    calculateHydrationGoal();
    server.send(200, "text/plain", "OK");
    Serial.println("🏋️ User info updated. Weight: " + String(userWeight) + " kg, Gender: " + userGender + ". New goal: " + String(waterGoal) + " ml.");
}

void handleResetIntake() {
    intakeSoFar = 0.0;
    lastDrinkTimeStr = "Not recorded";
    calculateHydrationGoal();
    Serial.println("💧 Water intake has been reset to 0ml.");
    server.send(200, "text/plain", "OK");
}

void handleSetCustomTime() {
    if (server.hasArg("time")) {
        String timeStr = server.arg("time");
        int hh = timeStr.substring(0, 2).toInt();
        int mm = timeStr.substring(3, 5).toInt();
        time_t now = time(nullptr);
        struct tm* ptm = localtime(&now);
        ptm->tm_hour = hh;
        ptm->tm_min = mm;
        ptm->tm_sec = 0;
        
        time_t newTime = mktime(ptm);

        // If the set time is in the past, add one day
        if (newTime < now) {
            newTime += 24 * 60 * 60; 
        }

        nextDrinkTime = newTime;
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%I:%M:%S %p", localtime(&nextDrinkTime));
        nextIntakeTimeStr = String(buffer);
        manualTimeSet = true;
        
        Serial.println("Next drink time manually set to: " + nextIntakeTimeStr);
        server.send(200, "text/plain", "OK");
    }
}

void handleSetInterval() {
    if (server.hasArg("minutes")) {
        int minutes = server.arg("minutes").toInt();
        if (minutes > 0) {
            customInterval = minutes * 60 * 1000UL;
            // Update next drink time based on new interval
            time_t now = time(nullptr);
            nextDrinkTime = now + (customInterval / 1000);
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%I:%M:%S %p", localtime(&nextDrinkTime));
            nextIntakeTimeStr = String(buffer);
        }
        manualTimeSet = false;
        Serial.println("Hydration interval set to: " + String(minutes) + " minutes.");
        server.send(200, "text/plain", "OK");
    }
}


// ===== Main Setup Function =====
void setup() {
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for time sync...");
    while (time(nullptr) < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nTime synchronized.");
    
    fetchWeather();
    calculateHydrationGoal();
    
    time_t now = time(nullptr);
    nextDrinkTime = now + (customInterval / 1000);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%I:%M:%S %p", localtime(&nextDrinkTime));
    nextIntakeTimeStr = String(buffer);
    
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", htmlPage); });
    server.on("/data", HTTP_GET, handleData);
    server.on("/drink", HTTP_POST, handleDrink);
    server.on("/updateUser", HTTP_POST, handleUpdateUser);
    server.on("/reset", HTTP_POST, handleResetIntake);
    server.on("/setNextTime", HTTP_POST, handleSetCustomTime);
    server.on("/setInterval", HTTP_POST, handleSetInterval);
    server.begin();
    Serial.println("HTTP server started.");
}

// ===== Main Loop =====
void loop() {
    server.handleClient();
    unsigned long currentMillis = millis();
    if (currentMillis - lastWeatherFetch >= weatherInterval) {
        lastWeatherFetch = currentMillis;
        fetchWeather();
        calculateHydrationGoal();
    }
    time_t now = time(nullptr);
    if (now >= nextDrinkTime && intakeSoFar < waterGoal) {
        beepAlert();
        // If a manual time was set, we don't automatically set the next reminder
        // The user must set another time or drink water to reset the reminder.
        // If no manual time was set, reset the next alert based on the interval.
        if (!manualTimeSet) {
            nextDrinkTime = now + (customInterval / 1000);
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%I:%M:%S %p", localtime(&nextDrinkTime));
            nextIntakeTimeStr = String(buffer);
        }
    }
}

// ===== Helper Functions =====
void fetchWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 4) + "&lon=" + String(longitude, 4) + "&appid=" + String(apiKey) + "&units=" + weatherUnits;
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            StaticJsonDocument<1024> doc;
            if (deserializeJson(doc, payload) == DeserializationError::Ok) {
                weatherTemp = doc["main"]["temp"].as<float>();
                weatherHumidity = doc["main"]["humidity"].as<float>();
            }
        }
        http.end();
    }
}

void calculateHydrationGoal() {
    float base = (userGender == "male") ? (35.0 * userWeight) : (30.0 * userWeight);
    float adjusted = base;
    if (weatherTemp > 30 && weatherHumidity > 70) adjusted *= 1.20;
    else if (weatherTemp > 30) adjusted *= 1.10;
    else if (weatherHumidity > 70) adjusted *= 1.15;
    waterGoal = adjusted;
    hydrationStatus = (intakeSoFar >= waterGoal) ? "Well Hydrated" : "Needs Water";
}

void beepAlert() {
    for(int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(500);
    }
}
