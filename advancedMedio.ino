#include <Arduino.h>
#include <DFPlayerMini_Fast.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Pin definitions
#define MP3_RX 16
#define MP3_TX 17
#define BUTTON_PIN 13 // Using GPIO13 (D13)

// Global objects
DFPlayerMini_Fast myMP3;
HardwareSerial mySoftwareSerial(1);
AsyncWebServer server(80);
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
const unsigned long DEBOUNCE_TIME = 200; // Debounce time in milliseconds

// Function declarations
void saveSettings();
void loadSettings();
void setupWiFi();
void setupWebServer();
bool isWithinActiveHours();
void playScheduledAnnouncement();
void handleButton();

void setup()
{
  Serial.begin(115200);

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
}

void setupWiFi()
{
  WiFiManager wifiManager;

  // Set config portal timeout
  wifiManager.setConfigPortalTimeout(180);

  // This will now try to connect using saved credentials first
  if (!wifiManager.autoConnect("ZenChime"))
  {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  Serial.println("WiFi connected!");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());
}

void setupWebServer()
{
  // Check if SPIFFS is mounted correctly
  if (!SPIFFS.exists("/index.html"))
  {
    Serial.println("Warning: index.html not found in SPIFFS");
    Serial.println("Did you upload the data folder?");
  }

  // Serve static files
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // API endpoints
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request)
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
    request->send(200, "application/json", response); });

  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      
      // Debug incoming data
      Serial.println("Received settings:");
      serializeJsonPretty(doc, Serial);
      Serial.println();
      
      settings.volume = doc["volume"] | settings.volume;
      
      // Debug timezone changes
      int oldTimezone = settings.timezone;
      settings.timezone = doc["timezone"] | settings.timezone;
      Serial.printf("Timezone changed from UTC%+d to UTC%+d\n", 
                   oldTimezone, settings.timezone);
      
      timeClient.setTimeOffset(settings.timezone * 3600);
      
      // Handle scheduled times
      JsonArray hours = doc["scheduledHours"];
      JsonArray minutes = doc["scheduledMinutes"];
      int newCount = doc["activeTimeCount"] | 0;
      
      Serial.printf("Received %d scheduled times\n", newCount);
      
      settings.activeTimeCount = min((size_t)settings.MAX_TIMES, (size_t)newCount);
      
      for(int i = 0; i < settings.activeTimeCount; i++) {
        settings.scheduledHours[i] = hours[i];
        settings.scheduledMinutes[i] = minutes[i];
        Serial.printf("Saved time slot %d: %02d:%02d\n", 
                     i + 1, 
                     settings.scheduledHours[i], 
                     settings.scheduledMinutes[i]);
      }
      
      myMP3.volume(settings.volume);
      saveSettings();
      
      request->send(200, "application/json", "{\"status\":\"success\"}"); });

  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    myMP3.playFolder(1, 1);
    request->send(200, "application/json", "{\"status\":\"playing\"}"); });

  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    myMP3.stop();
    isPlaying = false;
    request->send(200, "application/json", "{\"status\":\"stopped\"}"); });

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
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    // Debounce
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress > DEBOUNCE_TIME)
    {
      lastButtonPress = currentTime;

      // Toggle playback
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
  }
}

void loop()
{
  timeClient.update();
  playScheduledAnnouncement();
  handleButton();
  delay(100);
}