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
  int startHour = 9;
  int startMinute = 0;
  int endHour = 22;
  int endMinute = 0;
  bool weekendEnabled = true;
  int playInterval = 60; // minutes
} settings;

// Function declarations
void saveSettings();
void loadSettings();
void setupWiFi();
void setupWebServer();
bool isWithinActiveHours();
void playScheduledAnnouncement();

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

  // Initialize time client
  timeClient.begin();
  timeClient.setTimeOffset(0); // Will be set based on timezone later

  // Load saved settings
  loadSettings();

  // Setup web server
  setupWebServer();
}

void setupWiFi()
{
  WiFiManager wifiManager;

  // Set config portal timeout
  wifiManager.setConfigPortalTimeout(180);

  // This will now try to connect using saved credentials first
  if (!wifiManager.autoConnect("ESP32_MP3_Player", "12345678"))
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
    DynamicJsonDocument doc(1024);
    doc["volume"] = settings.volume;
    doc["startHour"] = settings.startHour;
    doc["startMinute"] = settings.startMinute;
    doc["endHour"] = settings.endHour;
    doc["endMinute"] = settings.endMinute;
    doc["weekendEnabled"] = settings.weekendEnabled;
    doc["playInterval"] = settings.playInterval;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, (const char*)data);
      
      settings.volume = doc["volume"] | settings.volume;
      settings.startHour = doc["startHour"] | settings.startHour;
      settings.startMinute = doc["startMinute"] | settings.startMinute;
      settings.endHour = doc["endHour"] | settings.endHour;
      settings.endMinute = doc["endMinute"] | settings.endMinute;
      settings.weekendEnabled = doc["weekendEnabled"] | settings.weekendEnabled;
      settings.playInterval = doc["playInterval"] | settings.playInterval;
      
      myMP3.volume(settings.volume);
      saveSettings();
      
      request->send(200, "application/json", "{\"status\":\"success\"}"); });

  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    myMP3.playFolder(1, 1);
    request->send(200, "application/json", "{\"status\":\"playing\"}"); });

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

  DynamicJsonDocument doc(1024);
  doc["volume"] = settings.volume;
  doc["startHour"] = settings.startHour;
  doc["startMinute"] = settings.startMinute;
  doc["endHour"] = settings.endHour;
  doc["endMinute"] = settings.endMinute;
  doc["weekendEnabled"] = settings.weekendEnabled;
  doc["playInterval"] = settings.playInterval;

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

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.println("Failed to parse settings file");
    return;
  }

  settings.volume = doc["volume"] | settings.volume;
  settings.startHour = doc["startHour"] | settings.startHour;
  settings.startMinute = doc["startMinute"] | settings.startMinute;
  settings.endHour = doc["endHour"] | settings.endHour;
  settings.endMinute = doc["endMinute"] | settings.endMinute;
  settings.weekendEnabled = doc["weekendEnabled"] | settings.weekendEnabled;
  settings.playInterval = doc["playInterval"] | settings.playInterval;

  file.close();
}

bool isWithinActiveHours()
{
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentTime = currentHour * 60 + currentMinute;
  int startTime = settings.startHour * 60 + settings.startMinute;
  int endTime = settings.endHour * 60 + settings.endMinute;

  if (!settings.weekendEnabled)
  {
    time_t rawtime = timeClient.getEpochTime();
    struct tm *ti = localtime(&rawtime);
    if (ti->tm_wday == 0 || ti->tm_wday == 6)
    { // Weekend
      return false;
    }
  }

  return currentTime >= startTime && currentTime <= endTime;
}

void playScheduledAnnouncement()
{
  static unsigned long lastPlayTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastPlayTime >= (settings.playInterval * 60000))
  { // Convert minutes to milliseconds
    if (isWithinActiveHours())
    {
      myMP3.playFolder(1, 1);
      lastPlayTime = currentTime;
    }
  }
}

void loop()
{
  timeClient.update();
  playScheduledAnnouncement();
  delay(1000);
}