// ESP32 Medicine Dispenser - Full Sketch
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <ESP32Servo.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>  // 🔥 Added for NTP sync

// ---------------- LCD & RTC ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// ---------------- Servo & Buzzer ----------------
Servo pillServo;
const int servoPin = 19;
const int buzzerPin = 23;
int currentAngle = 0;

// ---------------- Keypad ----------------
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------------- WiFi & Backend ----------------
const char* ssid = "Connected, no";
const char* wifiPassword = "12345678";
String backendURL = "http://10.12.104.10:5000/schedule";
String notifyURL = "http://10.12.104.10:5000/notify_taken";
String passwordURL = "http://10.12.104.10:5000/password";

// ---------------- Password ----------------
String password = "";   // will be fetched from backend
String inputPassword = "";

// ---------------- Schedule flags ----------------
bool alertTriggered = false;
bool doseTaken = false;
String medicineName = "";
int medicineID = -1;

// ---------------- Next medicine info ----------------
String nextMedName = "";
int nextMedHour = -1;
int nextMedMinute = -1;

// ---------------- Missed dose timer ----------------
unsigned long alertStart = 0;

// ---------------- Display state ----------------
bool inAlertMode = false;

// ---------------- Time sync variables ----------------
unsigned long lastNTPSync = 0;

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n----- ESP32 Medicine Dispenser Boot -----");
  
  Wire.begin(21, 22);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize Servo
  delay(500);
  pillServo.attach(servoPin);
  pillServo.write(0);
  pinMode(buzzerPin, OUTPUT);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1) delay(1000);
  }

  // Check if RTC lost power
  if (rtc.lostPower()) {
    Serial.println("WARNING: RTC lost power, setting time to compile time");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC Reset!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(2000);
  }

  // Display current RTC time
  DateTime now = rtc.now();
  Serial.printf("Current RTC Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  beepBuzzer(200);
  delay(1000);
  lcd.clear();

  // Connect to WiFi
  connectWiFi();

  // 🔥 Sync RTC with NTP once at startup
  syncWithNTP();

  // Fetch dispenser password from backend
  fetchPassword();

  // Initial schedule fetch
  delay(2000);
  fetchSchedule();
}

// ---------------- Main Loop ----------------
void loop() {
  DateTime now = rtc.now();

  // 🔄 Periodically resync with NTP every 6 hours
  if (millis() - lastNTPSync > 6UL * 60UL * 60UL * 1000UL) {
    syncWithNTP();
  }

  // WiFi reconnection check every 30 seconds
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      connectWiFi();
      fetchPassword(); // try to re-fetch password after reconnect
    }
    lastWiFiCheck = millis();
  }

  // Normal display mode (when no alert)
  if (!alertTriggered) {
    lcd.setCursor(0, 0);
    char buf[17];
    sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    lcd.print(buf);
    lcd.print("       "); // Clear extra characters

    lcd.setCursor(0, 1);
    if (nextMedHour >= 0) {
      char medBuf[17];
      String shortName = nextMedName.substring(0, 6);
      sprintf(medBuf, "%s %02d:%02d", shortName.c_str(), nextMedHour, nextMedMinute);
      lcd.print(medBuf);
      lcd.print("      "); // Clear extra characters
    } else {
      lcd.print("No schedule     ");
    }
    inAlertMode = false;
  }

  // Fetch schedule every 1 minute
  static unsigned long lastFetch = 0;
  if (millis() - lastFetch > 60000) {
    Serial.println("Fetching schedule...");
    fetchSchedule();
    lastFetch = millis();
  }

  // Handle alert mode
  if (alertTriggered && !doseTaken) {
    if (!inAlertMode) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time:");
      lcd.setCursor(6, 0);
      String shortName = medicineName.substring(0, 10);
      lcd.print(shortName);
      lcd.setCursor(0, 1);
      lcd.print("Enter password");
      inAlertMode = true;
      Serial.println("Alert triggered for: " + medicineName);
    }

    char key = keypad.getKey();
    if (key) {
      Serial.println("Key pressed: " + String(key));
      
      if (key == '#') { 
        if (inputPassword == password) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Correct!");
          lcd.setCursor(0, 1);
          lcd.print("Dispensing...");
          Serial.println("Password correct, dispensing medicine");
          
          rotateServo();
          doseTaken = true;
          alertTriggered = false;
          alertStart = 0;
          notifyBackend(true); 
          delay(2000);
          lcd.clear();
        } else {
          lcd.setCursor(0, 1);
          lcd.print("Wrong password! ");
          Serial.println("Wrong password entered");
          beepBuzzer(500);
          delay(1500);
          lcd.setCursor(0, 1);
          lcd.print("                ");
        }
        inputPassword = "";
      } else if (key == '*') {
        inputPassword = "";
        lcd.setCursor(0, 1);
        lcd.print("                ");
        Serial.println("Password cleared");
      } else {
        inputPassword += key;
        lcd.setCursor(0, 1);
        lcd.print("Pass: ");
        for (int i = 0; i < inputPassword.length(); i++) {
          lcd.print("*");
        }
      }
    }

    if (alertStart == 0) {
      alertStart = millis();
      Serial.println("Alert timer started");
    }
    
    if (millis() - alertStart > 300000) { 
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dose Missed!");
      Serial.println("MISSED DOSE: " + medicineName);
      
      beepBuzzer(1000);
      notifyBackend(false);
      
      doseTaken = true;
      alertTriggered = false;
      alertStart = 0;
      delay(3000);
      lcd.clear();
    }
  }

  delay(200);
}

// ---------------- Functions ----------------

void connectWiFi() {
  WiFi.begin(ssid, wifiPassword);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    yield();
    Serial.print(".");
    lcd.setCursor(0, 1);
    for (int i = 0; i < (attempts % 16); i++) lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    delay(1500);
    lcd.clear();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    Serial.println("\nWiFi Connection Failed!");
    delay(2000);
    lcd.clear();
  }
}

void syncWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping NTP sync: WiFi not connected");
    return;
  }

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // IST = UTC+5:30 (19800 sec)
  struct tm timeinfo;

  Serial.print("Syncing with NTP...");
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 15) {
    Serial.print(".");
    delay(500);
    retries++;
  }

  if (getLocalTime(&timeinfo)) {
    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));
    Serial.println("\n✅ RTC synced with NTP!");
  } else {
    Serial.println("\n⚠️ Failed to sync with NTP");
  }

  lastNTPSync = millis();
}

void fetchSchedule() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping fetch");
    return;
  }

  HTTPClient http;
  http.begin(backendURL);
  http.setTimeout(5000);
  
  int code = http.GET();
  Serial.println("HTTP GET Response Code: " + String(code));
  
  if (code == 200) {
    String payload = http.getString();
    Serial.println("Payload length: " + String(payload.length()));
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("JSON Parse Error: " + String(error.f_str()));
      http.end();
      return;
    }

    JsonArray arr = doc.as<JsonArray>();
    Serial.println("Schedule entries: " + String(arr.size()));

    nextMedHour = -1;
    nextMedMinute = -1;
    nextMedName = "";

    DateTime now = rtc.now();
    int currentTotalMin = now.hour() * 60 + now.minute();

    for (JsonObject obj : arr) {
      String timeStr = obj["time"].as<String>();
      int colon = timeStr.indexOf(':');
      if (colon == -1) continue;
      int h = timeStr.substring(0, colon).toInt();
      int m = timeStr.substring(colon + 1).toInt();

      bool taken = obj["taken"] | false;
      String medName = obj["medicine_name"].as<String>();
      int medID = obj["id"].as<int>();

      Serial.printf("Entry: %s at %02d:%02d, taken=%d, id=%d\n", 
                    medName.c_str(), h, m, taken, medID);

      if (now.hour() == h && now.minute() == m && !alertTriggered && !taken) {
        alertTriggered = true;
        doseTaken = false;
        medicineName = medName;
        medicineID = medID;
        inputPassword = "";
        alertStart = 0;
        
        beepBuzzer(1000);
        Serial.println("ALERT: Time for " + medicineName);
      }

      if (!taken) {
        int medTotalMin = h * 60 + m;
        if (medTotalMin > currentTotalMin) {
          if (nextMedHour < 0 || medTotalMin < (nextMedHour * 60 + nextMedMinute)) {
            nextMedHour = h;
            nextMedMinute = m;
            nextMedName = medName;
          }
        }
      }
    }

    if (nextMedHour >= 0) {
      Serial.printf("Next medicine: %s at %02d:%02d\n", 
                    nextMedName.c_str(), nextMedHour, nextMedMinute);
    } else {
      Serial.println("No upcoming medicines scheduled");
    }

  } else {
    Serial.println("HTTP GET Failed with code: " + String(code));
  }
  
  http.end();
}

void rotateServo() {
  int targetAngle = currentAngle + 60;
  if (targetAngle >= 180) targetAngle = 0;

  Serial.print("Rotating servo slowly from ");
  Serial.print(currentAngle);
  Serial.print("° to ");
  Serial.print(targetAngle);
  Serial.println("°");

  // Move slowly in small steps
  if (targetAngle > currentAngle) {
    for (int angle = currentAngle; angle <= targetAngle; angle++) {
      pillServo.write(angle);
      delay(20);  // increase delay for slower movement
    }
  } else {
    for (int angle = currentAngle; angle >= targetAngle; angle--) {
      pillServo.write(angle);
      delay(20);
    }
  }

  beepBuzzer(200);
  currentAngle = targetAngle;
  delay(500);
}


void notifyBackend(bool taken) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot notify backend: WiFi not connected");
    return;
  }

  HTTPClient http;
  http.begin(notifyURL);
  http.addHeader("Content-Type", "application/json");
  
  String status = taken ? "taken" : "missed";
  String body = "{\"id\":" + String(medicineID) + ",\"status\":\"" + status + "\"}";
  
  Serial.println("Notifying backend: " + body);
  
  int code = http.POST(body);
  Serial.println("Notify response code: " + String(code));
  
  if (code > 0) {
    String response = http.getString();
    Serial.println("Backend response: " + response);
  }
  
  http.end();
}

void beepBuzzer(int duration) {
  digitalWrite(buzzerPin, HIGH);
  delay(duration);
  digitalWrite(buzzerPin, LOW);
}

void fetchPassword() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping password fetch");
    return;
  }

  HTTPClient http;
  http.begin(passwordURL);
  http.setTimeout(5000);
  int code = http.GET();
  Serial.println("Password fetch code: " + String(code));

  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      if (doc.containsKey("password")) {
        password = doc["password"].as<String>();
        password.trim();
        Serial.println("Fetched dispenser password: '" + password + "'");
      } else {
        Serial.println("Password key missing in response");
      }
    } else {
      Serial.println("JSON parse error for password fetch");
    }
  } else {
    Serial.println("Password fetch failed with code: " + String(code));
  }

  http.end();
}
