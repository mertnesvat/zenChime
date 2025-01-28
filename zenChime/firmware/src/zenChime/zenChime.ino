#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// Version control
#define FIRMWARE_VERSION "1.0.1"
#define GITHUB_REPO "mertnesvat/zenChime"
#define FIRMWARE_URL "https://raw.githubusercontent.com/" GITHUB_REPO "/main/zenChime/firmware/bin/firmware.bin"
#define VERSION_URL "https://raw.githubusercontent.com/" GITHUB_REPO "/main/zenChime/firmware/bin/version.json"

// Pin definitions
#define MP3_RX 16
#define MP3_TX 17
#define BUTTON_PIN 13 // Using GPIO13 (D13)

// Global objects
DFPlayerMini_Fast myMP3;
HardwareSerial mySoftwareSerial(1);
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Default settings
struct Settings
{
  int volume = 15;
  static const int MAX_TIMES = 10;                 // Maximum number of scheduled times
  int scheduledHours[MAX_TIMES] = {8, 11, 13, 17}; // Default times
  int scheduledMinutes[MAX_TIMES] = {0, 45, 0, 0};
  int activeTimeCount = 4; // Number of active scheduled times
  int timezone = 0;        // Changed default to UTC+0 (London)
} settings;

// Add these global variables at the top with other globals
bool isPlaying = false;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_TIME = 200;          // Debounce time in milliseconds
const unsigned long WIFI_RESET_PRESS_TIME = 5000; // 5 seconds for WiFi reset
unsigned long buttonPressStartTime = 0;
bool buttonWasPressed = false;

// Function declarations
void saveSettings();
void loadSettings();
void setupWiFi();
void setupWebServer();
bool isWithinActiveHours();
void playScheduledAnnouncement();
void handleButton();
String checkForUpdates();

void setup()
{
  Serial.begin(115200);
  Serial.println("\n=== ZenChime v1.0.1 Starting ===");
  Serial.println("New in this version: Improved startup message");

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // List files in SPIFFS
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println("Files in SPIFFS:");
  while (file)
  {
    Serial.print("  ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  // Initialize DFPlayer
  mySoftwareSerial.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  if (!myMP3.begin(mySoftwareSerial))
  {
    Serial.println("DFPlayer Mini failed!");
    return;
  }

  // Set initial volume
  myMP3.volume(settings.volume);

  // Setup WiFi using WiFiManager
  setupWiFi();

  // Initialize time client with timezone
  timeClient.begin();
  timeClient.setTimeOffset(settings.timezone * 3600); // Convert hours to seconds

  // Load saved settings
  loadSettings();

  // Setup web server
  setupWebServer();

  // Setup button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Check for updates on boot
  String updateStatus = checkForUpdates();
  Serial.println("Update status: " + updateStatus);
}

void setupWiFi()
{
  WiFiManager wifiManager;

  // Enable captive portal functionality
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  wifiManager.setShowPassword(true);        // Show password while typing
  wifiManager.setCaptivePortalEnable(true); // Enable captive portal
  wifiManager.setConfigPortalTimeout(180);

  // Use device name as password - common IoT practice
  if (!wifiManager.autoConnect("ZenChime", "12345678"))
  {
    ESP.restart();
  }

  // Initialize mDNS
  if (!MDNS.begin("zenchime"))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println("mDNS responder started");
    Serial.println("You can now access the device at: http://zenchime.local");
    // Add service to mDNS
    MDNS.addService("http", "tcp", 80);
  }
}

void setupWebServer()
{
  if (!SPIFFS.exists("/index.html"))
  {
    Serial.println("Warning: index.html not found in SPIFFS");
    Serial.println("Did you upload the data folder?");
  }

  // Serve static files
  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/index.html", SPIFFS, "/index.html");

  // API endpoints
  server.on("/api/settings", HTTP_GET, []()
            {
    JsonDocument doc;
    doc["volume"] = settings.volume;
    doc["timezone"] = settings.timezone;
    
    JsonArray hours = doc.createNestedArray("scheduledHours");
    JsonArray minutes = doc.createNestedArray("scheduledMinutes");
    
    for(int i = 0; i < settings.activeTimeCount; i++) {
      hours.add(settings.scheduledHours[i]);
      minutes.add(settings.scheduledMinutes[i]);
    }
    
    doc["activeTimeCount"] = settings.activeTimeCount;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response); });

  server.on("/api/settings", HTTP_POST, []()
            {
    if (server.hasArg("plain")) {
      String json = server.arg("plain");
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, json);
      
      if (error) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
      }

      settings.volume = doc["volume"] | settings.volume;
      
      int oldTimezone = settings.timezone;
      settings.timezone = doc["timezone"] | settings.timezone;
      timeClient.setTimeOffset(settings.timezone * 3600);
      
      JsonArray hours = doc["scheduledHours"];
      JsonArray minutes = doc["scheduledMinutes"];
      int newCount = doc["activeTimeCount"] | 0;
      
      settings.activeTimeCount = min((size_t)settings.MAX_TIMES, (size_t)newCount);
      
      for(int i = 0; i < settings.activeTimeCount; i++) {
        settings.scheduledHours[i] = hours[i];
        settings.scheduledMinutes[i] = minutes[i];
      }
      
      myMP3.volume(settings.volume);
      saveSettings();
      
      server.send(200, "application/json", "{\"status\":\"success\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
    } });

  server.on("/api/play", HTTP_POST, []()
            {
    myMP3.playFolder(1, 1);
    server.send(200, "application/json", "{\"status\":\"playing\"}"); });

  server.on("/api/stop", HTTP_POST, []()
            {
    myMP3.stop();
    isPlaying = false;
    server.send(200, "application/json", "{\"status\":\"stopped\"}"); });

  server.on("/api/check-update", HTTP_GET, []()
            {
    String updateStatus = checkForUpdates();
    server.send(200, "application/json", updateStatus); });

  server.begin();
  Serial.println("Web server started");
  Serial.print("You can access the interface at: http://");
  Serial.println(WiFi.localIP());
}

void saveSettings()
{
  File file = SPIFFS.open("/settings.json", "w");
  if (!file)
  {
    Serial.println("Failed to open settings file for writing");
    return;
  }

  JsonDocument doc;
  doc["volume"] = settings.volume;

  JsonArray hours = doc.createNestedArray("scheduledHours");
  JsonArray minutes = doc.createNestedArray("scheduledMinutes");

  for (int i = 0; i < settings.activeTimeCount; i++)
  {
    hours.add(settings.scheduledHours[i]);
    minutes.add(settings.scheduledMinutes[i]);
  }

  doc["activeTimeCount"] = settings.activeTimeCount;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write settings to file");
  }
  file.close();
}

void loadSettings()
{
  File file = SPIFFS.open("/settings.json", "r");
  if (!file)
  {
    Serial.println("No settings file found - using defaults");
    saveSettings();
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.println("Failed to parse settings file");
    return;
  }

  settings.volume = doc["volume"] | settings.volume;

  JsonArray hours = doc["scheduledHours"];
  JsonArray minutes = doc["scheduledMinutes"];

  settings.activeTimeCount = min((size_t)settings.MAX_TIMES, hours.size());

  for (int i = 0; i < settings.activeTimeCount; i++)
  {
    settings.scheduledHours[i] = hours[i] | settings.scheduledHours[i];
    settings.scheduledMinutes[i] = minutes[i] | settings.scheduledMinutes[i];
  }

  file.close();
}

bool isWithinActiveHours()
{
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  // Check if current time matches any scheduled time
  for (int i = 0; i < settings.activeTimeCount; i++)
  {
    if (currentHour == settings.scheduledHours[i] &&
        currentMinute == settings.scheduledMinutes[i])
    {
      return true;
    }
  }
  return false;
}

void playScheduledAnnouncement()
{
  static unsigned long lastCheck = 0;
  unsigned long currentMillis = millis();

  // Check every 30 seconds
  if (currentMillis - lastCheck >= 30000)
  {
    lastCheck = currentMillis;

    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();

    // Debug: Print current time
    Serial.printf("Current time: %02d:%02d\n", currentHour, currentMinute);
    Serial.println("Scheduled times:");
    for (int i = 0; i < settings.activeTimeCount; i++)
    {
      Serial.printf("  Time slot %d: %02d:%02d\n",
                    i + 1,
                    settings.scheduledHours[i],
                    settings.scheduledMinutes[i]);
    }

    // Check each scheduled time
    for (int i = 0; i < settings.activeTimeCount; i++)
    {
      // Debug: Print comparison
      Serial.printf("Checking slot %d: %02d:%02d == %02d:%02d ? ",
                    i + 1,
                    currentHour, currentMinute,
                    settings.scheduledHours[i],
                    settings.scheduledMinutes[i]);

      if (currentHour == settings.scheduledHours[i] &&
          currentMinute == settings.scheduledMinutes[i])
      {
        Serial.println("Match found!");
        static int lastPlayedHour = -1;
        static int lastPlayedMinute = -1;

        // Only play if we haven't played at this exact time
        if (currentHour != lastPlayedHour || currentMinute != lastPlayedMinute)
        {
          Serial.printf("Playing scheduled announcement at %02d:%02d\n",
                        currentHour, currentMinute);
          Serial.printf("Last played at: %02d:%02d\n",
                        lastPlayedHour, lastPlayedMinute);

          myMP3.playFolder(1, 1);
          lastPlayedHour = currentHour;
          lastPlayedMinute = currentMinute;

          Serial.println("Announcement played successfully");
        }
        else
        {
          Serial.println("Already played at this time");
        }
        break; // Exit loop after playing
      }
      else
      {
        Serial.println("No match");
      }
    }
    Serial.println("--------------------");
  }
}

void handleButton()
{
  // Read button state (LOW when pressed because of INPUT_PULLUP)
  bool buttonIsPressed = (digitalRead(BUTTON_PIN) == LOW);

  if (buttonIsPressed && !buttonWasPressed)
  {
    // Button just pressed
    buttonPressStartTime = millis();
    buttonWasPressed = true;
  }
  else if (buttonIsPressed && buttonWasPressed)
  {
    // Button is being held
    if ((millis() - buttonPressStartTime) >= WIFI_RESET_PRESS_TIME)
    {
      // Reset WiFi settings using WiFiManager
      WiFiManager wifiManager;
      wifiManager.resetSettings();
      ESP.restart(); // Restart device to enter initial config mode
      return;
    }
  }
  else if (!buttonIsPressed && buttonWasPressed)
  {
    // Button released
    if ((millis() - buttonPressStartTime) < WIFI_RESET_PRESS_TIME)
    {
      // Short press - handle normal playback toggle
      if (isPlaying)
      {
        Serial.println("Button pressed: Stopping playback");
        myMP3.stop();
        isPlaying = false;
      }
      else
      {
        Serial.println("Button pressed: Starting playback");
        myMP3.playFolder(1, 1);
        isPlaying = true;
      }
    }
    buttonWasPressed = false;
  }
}

String checkForUpdates()
{
  HTTPClient http;
  String payload = "";
  JsonDocument doc;

  // Check version file first
  http.begin(VERSION_URL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      const char *latestVersion = doc["version"];

      if (String(latestVersion) != String(FIRMWARE_VERSION))
      {
        Serial.println("New firmware version available");
        Serial.printf("Current version: %s, Latest version: %s\n", FIRMWARE_VERSION, latestVersion);

        // Create a new HTTPClient for the firmware update
        HTTPClient updateClient;
        updateClient.begin(FIRMWARE_URL);

        // Start update process with proper parameters
        t_httpUpdate_return ret = httpUpdate.update(updateClient, FIRMWARE_VERSION);

        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",
                        httpUpdate.getLastError(),
                        httpUpdate.getLastErrorString().c_str());
          return "{\"status\":\"error\",\"message\":\"Update failed\"}";

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          return "{\"status\":\"no_update\",\"message\":\"No updates available\"}";

        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          return "{\"status\":\"success\",\"message\":\"Update successful\"}";
        }
      }
      else
      {
        return "{\"status\":\"no_update\",\"message\":\"Already on latest version\"}";
      }
    }
  }

  http.end();
  return "{\"status\":\"error\",\"message\":\"Failed to check for updates\"}";
}

void loop()
{
  static unsigned long lastUpdateCheck = 0;
  unsigned long currentMillis = millis();

  // Check for updates every 24 hours
  if (currentMillis - lastUpdateCheck >= 24 * 60 * 60 * 1000UL)
  {
    lastUpdateCheck = currentMillis;
    String updateStatus = checkForUpdates();
    Serial.println("Update status: " + updateStatus);
  }

  server.handleClient();
  timeClient.update();
  playScheduledAnnouncement();
  handleButton();
  delay(100);
}
