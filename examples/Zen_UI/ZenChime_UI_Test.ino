#define USER_SETUP_LOADED
#define ST7789_DRIVER

// Screen dimensions
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Pin Definitions
#define TFT_MISO -1
#define TFT_MOSI 23   // SDA
#define TFT_SCLK 18   // SCL
#define TFT_CS 5      // CS
#define TFT_DC 4      // DC
#define TFT_RST 2     // RES
#define TFT_BL 13     // Backlight
#define BUTTON_PIN 13 // Button pin

// Load fonts
#define LOAD_GLCD  // Font 1
#define LOAD_FONT2 // Font 2
#define LOAD_FONT4 // Font 4
#define LOAD_FONT6 // Font 6
#define LOAD_FONT7 // Font 7
#define LOAD_FONT8 // Font 8
#define LOAD_GFXFF // FreeFonts
#define SMOOTH_FONT

// Include necessary libraries
#include <TFT_eSPI.h>
#include <SPI.h>

// Color Palette - Enhanced Zen inspired
#define ZEN_BLACK 0x0000  // Background
#define ZEN_WHITE 0xFFFF  // Text
#define ZEN_SAGE 0x8410   // Soft green for accents
#define ZEN_SAND 0xC618   // Warm grey
#define ZEN_MIST 0x7BEF   // Soft blue-grey
#define ZEN_DAWN 0xFDF7   // Soft warm white
#define ZEN_ALERT 0xF800  // Soft red for notifications
#define ZEN_ACTIVE 0x07E0 // Active session indicator

// Update sunset colors to better match the image
#define SUNSET_SKY_DARK 0xA041  // Darker red-purple
#define SUNSET_SKY_MID 0xE2C4   // Warm orange-red
#define SUNSET_SKY_LIGHT 0xFE07 // Bright yellow-orange
#define SUNSET_SUN 0xFFC0       // Bright warm yellow
#define SUNSET_MOUNTAIN 0x0821  // Deep blue-black
#define SUNSET_WATER 0x0011     // Dark blue for water
#define SUNSET_RIPPLE 0xFE07    // Reflection highlights

// Initialize Display
TFT_eSPI tft = TFT_eSPI();

// UI States
enum Screen
{
    MAIN_SCREEN,
    TIMER_SCREEN,
    QUOTE_SCREEN,
    SETTINGS_SCREEN,
    INVITE_SCREEN,
    SUNSET_SCREEN
};
Screen currentScreen = MAIN_SCREEN;

// Session States
enum SessionState
{
    IDLE,
    INVITE_RECEIVED,
    SESSION_ACTIVE,
    SESSION_ENDING
};
SessionState sessionState = IDLE;

// Time variables
int currentHour = 12;
int currentMinute = 0;
bool isEvening = false;

// Quotes array
const char *quotes[] = {
    "The wound is the place\nwhere the Light\nenters you",
    "What you seek is\nseeking you",
    "Let the beauty of what\nyou love be what\nyou do",
    "Yesterday I was clever,\nso I wanted to change\nthe world. Today I am\nwise, so I am changing\nmyself"};
const int NUM_QUOTES = 4;
int currentQuote = 0;

// Session info
char inviterName[32] = "";
bool hasInvite = false;
int activeParticipants = 0;
unsigned long sessionStartTime = 0;

// Add button handling variables after other global variables
bool buttonWasPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_TIME = 200;

// Add this function declaration before setup()
void drawSunsetScene();

// Add this function before setup()
void handleButton()
{
    bool buttonIsPressed = (digitalRead(BUTTON_PIN) == LOW);
    unsigned long currentTime = millis();

    if (buttonIsPressed && !buttonWasPressed && (currentTime - lastButtonPress > DEBOUNCE_TIME))
    {
        lastButtonPress = currentTime;
        buttonWasPressed = true;

        // Cycle through different states for demonstration
        switch (sessionState)
        {
        case IDLE:
            sessionState = INVITE_RECEIVED;
            strcpy(inviterName, "Sarah"); // Example inviter
            break;
        case INVITE_RECEIVED:
            sessionState = SESSION_ACTIVE;
            sessionStartTime = millis();
            activeParticipants = 3;
            break;
        case SESSION_ACTIVE:
            sessionState = SESSION_ENDING;
            break;
        case SESSION_ENDING:
            sessionState = IDLE;
            break;
        }

        // Update display immediately
        showMainScreen();

        Serial.printf("State changed to: %d\n", sessionState);
    }
    else if (!buttonIsPressed)
    {
        buttonWasPressed = false;
    }
}

void setup()
{
    Serial.begin(115200); // Initialize Serial communication
    delay(1000);          // Short delay to ensure serial is ready
    Serial.println("\n=== ZenChime UI Test ===");
    Serial.println("Initializing ZenChime UI...");

    // Initialize SPI first
    Serial.println("Initializing SPI...");
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    Serial.println("SPI initialized");

    // Initialize display with debug info
    Serial.println("Starting TFT initialization...");
    Serial.printf("TFT_MISO: %d\n", TFT_MISO);
    Serial.printf("TFT_MOSI: %d\n", TFT_MOSI);
    Serial.printf("TFT_SCLK: %d\n", TFT_SCLK);
    Serial.printf("TFT_CS: %d\n", TFT_CS);
    Serial.printf("TFT_DC: %d\n", TFT_DC);
    Serial.printf("TFT_RST: %d\n", TFT_RST);

    tft.init();
    Serial.println("TFT init completed");

    tft.setRotation(0); // Portrait mode
    Serial.println("Rotation set");

    tft.fillScreen(ZEN_BLACK);
    Serial.println("Screen filled with background color");

    // Initialize backlight with PWM
    Serial.println("Setting up backlight...");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight initialized");

    // Add button setup
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button initialized");

    showMainScreen();
    Serial.println("Main screen shown");
    Serial.println("Setup complete!");
}

void showMainScreen()
{
    tft.fillScreen(ZEN_BLACK);

    switch (sessionState)
    {
    case INVITE_RECEIVED:
        showInviteScreen();
        break;
    case SESSION_ACTIVE:
        showActiveSession();
        break;
    case SESSION_ENDING:
        drawSunsetScene(); // Show sunset scene when session is ending
        break;
    case IDLE:
    default:
        showQuoteWithInfo();
        break;
    }

    // Bottom navigation dots
    drawNavigationDots(0);
}

void showQuoteWithInfo()
{
    // Draw decorative frame
    tft.drawRect(10, 10, tft.width() - 20, tft.height() - 20, ZEN_MIST);

    // Show current quote
    tft.setTextColor(ZEN_WHITE);
    tft.setTextSize(1);

    // Display quote text
    String quoteStr = quotes[currentQuote];
    int y = 40;
    int startIndex = 0;
    int nextIndex;

    while ((nextIndex = quoteStr.indexOf('\n', startIndex)) != -1)
    {
        String line = quoteStr.substring(startIndex, nextIndex);
        int x = (tft.width() - (line.length() * 6)) / 2;
        tft.setCursor(x, y);
        tft.print(line);
        y += 20;
        startIndex = nextIndex + 1;
    }

    // Print last line
    String line = quoteStr.substring(startIndex);
    int x = (tft.width() - (line.length() * 6)) / 2;
    tft.setCursor(x, y);
    tft.print(line);

    // Show time
    tft.setTextSize(2);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", currentHour, currentMinute);
    tft.setCursor(10, tft.height() - 60);
    tft.print(timeStr);

    // Show next session
    tft.setTextSize(1);
    tft.setTextColor(ZEN_SAGE);
    tft.setCursor(tft.width() - 80, tft.height() - 60);
    tft.print("Next: 17:00");
}

void showInviteScreen()
{
    tft.fillScreen(ZEN_BLACK);

    // Show invitation alert
    tft.setTextColor(ZEN_ALERT);
    tft.setTextSize(2);
    tft.setCursor(20, 40);
    tft.print("Session Invite");

    // Show inviter name
    tft.setTextColor(ZEN_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 70);
    tft.print("From: ");
    tft.print(inviterName);

    // Show session info
    tft.setCursor(20, 100);
    tft.print("Duration: 20 minutes");

    // Draw accept/decline buttons
    drawButton("Accept", 40, 150, ZEN_SAGE);
    drawButton("Decline", 140, 150, ZEN_ALERT);
}

void showActiveSession()
{
    tft.fillScreen(ZEN_BLACK);

    // Show session timer
    int elapsed = (millis() - sessionStartTime) / 1000;
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;

    tft.setTextColor(ZEN_ACTIVE);
    tft.setTextSize(3);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
    int timeX = (tft.width() - (strlen(timeStr) * 18)) / 2;
    tft.setCursor(timeX, 60);
    tft.print(timeStr);

    // Show participants
    tft.setTextColor(ZEN_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.printf("Participants: %d", activeParticipants);

    // Draw breathing circle animation
    drawBreathingCircle(elapsed % 8); // 8-second breath cycle
}

void drawBreathingCircle(int phase)
{
    int centerX = tft.width() / 2;
    int centerY = 160;
    int maxRadius = 40;
    int minRadius = 20;

    // Calculate current radius based on breath phase
    float progress = phase / 8.0;
    int radius = minRadius + (sin(progress * 2 * PI) + 1) * (maxRadius - minRadius) / 2;

    tft.drawCircle(centerX, centerY, radius, ZEN_MIST);
    tft.drawCircle(centerX, centerY, radius + 1, ZEN_SAGE);
}

void drawButton(const char *text, int x, int y, uint16_t color)
{
    tft.drawRoundRect(x, y, 80, 30, 5, color);
    tft.setTextColor(color);
    tft.setTextSize(1);
    int textX = x + (80 - (strlen(text) * 6)) / 2;
    tft.setCursor(textX, y + 10);
    tft.print(text);
}

void drawNavigationDots(int activeIndex)
{
    int dotSpacing = 20;
    int startX = 120 - (dotSpacing * 2);
    int y = 300;

    for (int i = 0; i < 4; i++)
    {
        if (i == activeIndex)
        {
            tft.fillCircle(startX + (i * dotSpacing), y, 3, ZEN_WHITE);
        }
        else
        {
            tft.drawCircle(startX + (i * dotSpacing), y, 3, ZEN_SAND);
        }
    }
}

// Add this new function to draw the sunset scene
void drawSunsetScene()
{
    // Draw sky gradient with more color steps
    for (int y = 0; y < tft.height() / 2; y++)
    {
        // Calculate gradient color with smoother transition
        float progress = (float)y / (tft.height() / 2);
        uint16_t color;

        if (progress < 0.3)
        {
            color = SUNSET_SKY_DARK;
        }
        else if (progress < 0.6)
        {
            color = SUNSET_SKY_MID;
        }
        else
        {
            color = SUNSET_SKY_LIGHT;
        }

        tft.drawFastHLine(0, y, tft.width(), color);
        // Mirror for reflection with slight darkening
        tft.drawFastHLine(0, tft.height() - y - 1, tft.width(), color);
    }

    // Enhanced sun with glow effect
    int sunX = tft.width() * 2 / 5;
    int sunY = tft.height() / 3;
    int sunRadius = 20;

    // Draw sun glow
    for (int r = sunRadius + 10; r > sunRadius; r--)
    {
        tft.drawCircle(sunX, sunY, r, SUNSET_SKY_LIGHT);
    }
    tft.fillCircle(sunX, sunY, sunRadius, SUNSET_SUN);

    // Draw mountains with improved silhouette
    int mountainPoints[] = {
        0, tft.height() / 2,                     // Start
        tft.width() / 5, tft.height() / 2.8,     // First peak
        tft.width() / 3, tft.height() / 2.2,     // Valley
        tft.width() / 2, tft.height() / 3,       // Highest peak
        2 * tft.width() / 3, tft.height() / 2.5, // Small peak
        4 * tft.width() / 5, tft.height() / 2.3, // Last peak
        tft.width(), tft.height() / 2.1,         // End
        tft.width(), tft.height() / 2,           // Bottom right
        0, tft.height() / 2                      // Bottom left
    };

    // Fill mountains
    for (int i = 0; i < 16; i += 2)
    {
        tft.drawLine(mountainPoints[i], mountainPoints[i + 1],
                     mountainPoints[(i + 2) % 18], mountainPoints[(i + 3) % 18],
                     SUNSET_MOUNTAIN);
    }

    // Add more detailed water reflections
    for (int y = tft.height() / 2 + 1; y < tft.height(); y += 3)
    {
        for (int x = 0; x < tft.width(); x += 15)
        {
            int rippleWidth = random(3, 12);
            if (random(100) < 30)
            { // 30% chance for highlight
                tft.drawFastHLine(x, y, rippleWidth, SUNSET_RIPPLE);
            }
            else
            {
                tft.drawFastHLine(x, y, rippleWidth, SUNSET_WATER);
            }
        }
    }

    // Add birds with varying sizes
    for (int i = 0; i < 6; i++)
    {
        int birdX = random(tft.width());
        int birdY = random(tft.height() / 5);
        int size = random(2, 5);
        tft.drawLine(birdX, birdY, birdX + size, birdY + size, SUNSET_MOUNTAIN);
        tft.drawLine(birdX + size, birdY + size, birdX + size * 2, birdY, SUNSET_MOUNTAIN);
    }
}

void loop()
{
    handleButton(); // Add this at the start of loop

    static unsigned long lastUpdate = 0;
    unsigned long currentTime = millis();

    // Update display based on state
    if (currentTime - lastUpdate >= 1000)
    { // Update every second
        lastUpdate = currentTime;

        switch (sessionState)
        {
        case SESSION_ACTIVE:
            showActiveSession();
            break;
        case SESSION_ENDING:
            // Redraw sunset scene occasionally to animate ripples
            if (currentTime % 5000 == 0)
            { // Every 5 seconds
                drawSunsetScene();
            }
            break;
        case IDLE:
            if (currentTime % 30000 == 0)
            { // Change quote every 30 seconds
                currentQuote = (currentQuote + 1) % NUM_QUOTES;
                showQuoteWithInfo();
            }
            break;
        case INVITE_RECEIVED:
            // Blink invitation indicator
            if (currentTime % 1000 < 500)
            {
                tft.fillCircle(tft.width() - 20, 20, 5, ZEN_ALERT);
            }
            else
            {
                tft.fillCircle(tft.width() - 20, 20, 5, ZEN_BLACK);
            }
            break;
        }
    }

    // Update time (in real app, this would use actual time)
    static unsigned long timeUpdateLast = 0;
    if (currentTime - timeUpdateLast >= 60000)
    { // Update every minute
        timeUpdateLast = currentTime;
        currentMinute++;
        if (currentMinute >= 60)
        {
            currentMinute = 0;
            currentHour++;
            if (currentHour >= 12)
            {
                currentHour = 1;
                isEvening = !isEvening;
            }
        }
        if (sessionState == IDLE)
        {
            showQuoteWithInfo(); // Update the display with new time
        }
    }

    // In real app, add touch handling here
    delay(50);
}