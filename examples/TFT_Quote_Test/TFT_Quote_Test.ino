#define USER_SETUP_LOADED

#define ST7789_DRIVER

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_MISO -1
#define TFT_MOSI 23 // SDA    
#define TFT_SCLK 18 // SCL
#define TFT_CS 5    // CS
#define TFT_DC 4    // DC
#define TFT_RST 2   // RES
#define TFT_BL 13   // Backlight control pin

// PWM backlight control
#define TFT_BACKLIGHT_ON HIGH         // Level to turn backlight on
#define PWM_CHANNEL 0                 // ESP32 PWM channel to use for backlight
#define PWM_FREQ 5000                 // PWM frequency for backlight
#define PWM_BITS 8                    // PWM resolution bits
#define PWM_MAX ((1 << PWM_BITS) - 1) // Maximum PWM value

#define LOAD_GLCD  // Font 1
#define LOAD_FONT2 // Font 2
#define LOAD_FONT4 // Font 4
#define LOAD_FONT6 // Font 6
#define LOAD_FONT7 // Font 7
#define LOAD_FONT8 // Font 8
#define LOAD_GFXFF // FreeFonts

#define SMOOTH_FONT

// Now include the libraries
#include <TFT_eSPI.h>
#include <SPI.h>

// Color definitions
#define BACKGROUND TFT_BLACK
#define TEXT_COLOR TFT_WHITE
#define ACCENT_COLOR TFT_GOLD
#define BORDER_COLOR TFT_DARKGREY

// Initialize TFT
TFT_eSPI tft = TFT_eSPI();

// Rumi quotes array
const char *quotes[] = {
    "The wound is the place\nwhere the Light\nenters you",
    "What you seek is\nseeking you",
    "Let the beauty of what\nyou love be what\nyou do",
    "Yesterday I was clever,\nso I wanted to change\nthe world. Today I am\nwise, so I am changing\nmyself"};
const int NUM_QUOTES = 4;
int currentQuote = 0;

// Animation variables
int fadeLevel = 0;
bool fadeIn = true;
const int FADE_DELAY = 50;
const int QUOTE_DELAY = 5000;
unsigned long lastFadeUpdate = 0;
unsigned long quoteStartTime = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== TFT Quote Display Test ===");

    // Setup backlight pin
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight initialized");

    // Initialize TFT
    Serial.println("Initializing display...");
    tft.init();
    tft.setRotation(1); // Landscape
    tft.fillScreen(BACKGROUND);
    Serial.println("Display initialized");

    // Set text wrap
    tft.setTextWrap(true);

    // Test display
    Serial.println("Testing display with welcome screen...");
    showWelcomeScreen();
    if (tft.width() > 0)
    {
        Serial.printf("Display size: %dx%d\n", tft.width(), tft.height());
    }
    else
    {
        Serial.println("Warning: Display might not be properly initialized");
    }

    delay(2000);
    quoteStartTime = millis();
    Serial.println("Setup complete");
}

void showWelcomeScreen()
{
    tft.fillScreen(BACKGROUND);

    // Draw decorative border
    drawBorder();

    // Welcome text
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.setCursor(40, 60);
    tft.println("Rumi Quotes");

    // Subtitle
    tft.setTextSize(1);
    tft.setCursor(30, 100);
    tft.println("A journey through wisdom");

    // Draw some decorative elements
    drawDecorations();
}

void drawBorder()
{
    // Outer border
    tft.drawRect(5, 5, tft.width() - 10, tft.height() - 10, BORDER_COLOR);
    // Inner border
    tft.drawRect(8, 8, tft.width() - 16, tft.height() - 16, BORDER_COLOR);
}

void drawDecorations()
{
    // Corner decorations
    int cornerSize = 10;
    // Top left
    tft.drawLine(5, 5, 5 + cornerSize, 5, ACCENT_COLOR);
    tft.drawLine(5, 5, 5, 5 + cornerSize, ACCENT_COLOR);
    // Top right
    tft.drawLine(tft.width() - 5 - cornerSize, 5, tft.width() - 5, 5, ACCENT_COLOR);
    tft.drawLine(tft.width() - 5, 5, tft.width() - 5, 5 + cornerSize, ACCENT_COLOR);
    // Bottom left
    tft.drawLine(5, tft.height() - 5, 5 + cornerSize, tft.height() - 5, ACCENT_COLOR);
    tft.drawLine(5, tft.height() - 5 - cornerSize, 5, tft.height() - 5, ACCENT_COLOR);
    // Bottom right
    tft.drawLine(tft.width() - 5 - cornerSize, tft.height() - 5, tft.width() - 5, tft.height() - 5, ACCENT_COLOR);
    tft.drawLine(tft.width() - 5, tft.height() - 5 - cornerSize, tft.width() - 5, tft.height() - 5, ACCENT_COLOR);
}

void showQuote(const char *quote)
{
    tft.fillScreen(BACKGROUND);
    drawBorder();
    drawDecorations();

    // Split and center the quote
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);

    // Parse and display multiline quote
    String quoteStr = String(quote);
    int y = 40;
    int startIndex = 0;
    int nextIndex;

    while ((nextIndex = quoteStr.indexOf('\n', startIndex)) != -1)
    {
        String line = quoteStr.substring(startIndex, nextIndex);
        // Center each line
        int x = (tft.width() - (line.length() * 12)) / 2;
        tft.setCursor(x, y);
        tft.println(line);
        y += 30;
        startIndex = nextIndex + 1;
    }
    // Print last line
    String line = quoteStr.substring(startIndex);
    int x = (tft.width() - (line.length() * 12)) / 2;
    tft.setCursor(x, y);
    tft.println(line);

    // Attribution
    tft.setTextColor(ACCENT_COLOR);
    tft.setTextSize(1);
    tft.setCursor(tft.width() - 50, tft.height() - 20);
    tft.println("- Rumi");

    // Quote number
    tft.setTextColor(BORDER_COLOR);
    tft.setCursor(10, tft.height() - 20);
    tft.printf("%d/%d", currentQuote + 1, NUM_QUOTES);
}

void fadeBacklight(bool in)
{
    // Simple on/off for now
    digitalWrite(TFT_BL, in ? HIGH : LOW);
}

void loop()
{
    unsigned long currentTime = millis();

    // Handle fade effect
    if (currentTime - lastFadeUpdate >= FADE_DELAY)
    {
        lastFadeUpdate = currentTime;

        if (fadeIn)
        {
            fadeLevel += 5;
            if (fadeLevel >= 255)
            {
                fadeLevel = 255;
                fadeIn = false;
            }
        }

        fadeBacklight(fadeIn);
    }

    // Change quote after delay
    if (currentTime - quoteStartTime >= QUOTE_DELAY)
    {
        fadeIn = true;
        fadeLevel = 0;
        currentQuote = (currentQuote + 1) % NUM_QUOTES;
        showQuote(quotes[currentQuote]);
        quoteStartTime = currentTime;
    }
}