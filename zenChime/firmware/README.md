# ZenChime Firmware

## Directory Structure

- `src/`: Contains the main Arduino sketch (zenChime.ino)
- `bin/`: Contains compiled firmware binaries and version information
- `lib/`: Contains any custom libraries

## Build Instructions

1. Open `src/zenChime.ino` in Arduino IDE
2. Select ESP32 board: Tools -> Board -> ESP32 -> ESP32 Dev Module
3. Set partition scheme: Tools -> Partition Scheme -> Default 4MB with spiffs
4. Install required libraries:
   - DFPlayerMini_Fast
   - WiFiManager
   - ArduinoJson
   - NTPClient

## OTA Updates

The firmware supports Over-The-Air updates. To create a new release:

1. Update version number in `src/zenChime.ino`
2. Export compiled binary: Sketch -> Export Compiled Binary
3. Copy the .bin file to `bin/firmware.bin`
4. Update `bin/version.json` with new version info
5. Commit and push changes to GitHub

## Pin Configuration

- MP3_RX: GPIO16
- MP3_TX: GPIO17
- BUTTON_PIN: GPIO13

## Development Guidelines

1. Always increment version number when making changes
2. Test OTA update before releasing
3. Document any new features in version.json
4. Keep serial debug messages informative
