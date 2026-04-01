#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// Color sequence for the fill test
const uint16_t colors[] = {
  TFT_RED, TFT_GREEN, TFT_BLUE,
  TFT_YELLOW, TFT_CYAN, TFT_MAGENTA,
  TFT_WHITE, TFT_BLACK
};
const char* colorNames[] = {
  "RED", "GREEN", "BLUE",
  "YELLOW", "CYAN", "MAGENTA",
  "WHITE", "BLACK"
};
const uint8_t numColors = 8;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ILI9488 Functional Test — Starting");

  tft.init();
  tft.setRotation(1);  // Landscape. Try 0–3 if orientation looks wrong.

  Serial.println("Display initialized.");

  // --- Phase 1: Full-screen color fills ---
  Serial.println("Phase 1: Color fills");
  for (uint8_t i = 0; i < numColors; i++) {
    tft.fillScreen(colors[i]);
    uint16_t labelColor = (colors[i] == TFT_BLACK) ? TFT_WHITE : TFT_BLACK;
    tft.setTextColor(labelColor, colors[i]);
    tft.setTextSize(3);
    tft.setCursor(10, 10);
    tft.print(colorNames[i]);
    delay(800);
  }

  // --- Phase 2: Grid pattern ---
  Serial.println("Phase 2: Grid pattern");
  tft.fillScreen(TFT_BLACK);
  for (int x = 0; x < tft.width(); x += 40)
    tft.drawFastVLine(x, 0, tft.height(), TFT_DARKGREY);
  for (int y = 0; y < tft.height(); y += 40)
    tft.drawFastHLine(0, y, tft.width(), TFT_DARKGREY);
  delay(1500);

  // --- Phase 3: Text and shapes ---
  Serial.println("Phase 3: Text and shapes");
  tft.fillScreen(TFT_NAVY);

  // Title
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("ILI9488 OK");

  // Subtitle
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW, TFT_NAVY);
  tft.setCursor(10, 50);
  tft.print("ESP32-S3 + TFT_eSPI");

  // Resolution info
  tft.setTextColor(TFT_GREEN, TFT_NAVY);
  tft.setCursor(10, 80);
  tft.printf("Display: %dx%d", tft.width(), tft.height());

  // Colored rectangles
  tft.fillRect(10,  120, 80, 50, TFT_RED);
  tft.fillRect(100, 120, 80, 50, TFT_GREEN);
  tft.fillRect(190, 120, 80, 50, TFT_BLUE);
  tft.fillRect(280, 120, 80, 50, TFT_YELLOW);

  // Circles
  tft.fillCircle(50,  230, 35, TFT_CYAN);
  tft.fillCircle(150, 230, 35, TFT_MAGENTA);
  tft.fillCircle(250, 230, 35, TFT_ORANGE);
  tft.fillCircle(350, 230, 35, TFT_PURPLE);

  // Triangle
  tft.fillTriangle(
    tft.width()/2, tft.height() - 20,
    tft.width()/2 - 60, tft.height() - 90,
    tft.width()/2 + 60, tft.height() - 90,
    TFT_WHITE
  );

  Serial.println("Phase 3 complete. Test finished — checking Serial for errors.");
}

void loop() {
  // Blink the backlight label over the triangle to prove loop is running
  static bool toggle = false;
  static uint32_t lastMs = 0;

  if (millis() - lastMs > 1000) {
    lastMs = millis();
    toggle = !toggle;
    tft.setTextColor(toggle ? TFT_RED : TFT_WHITE, TFT_NAVY);
    tft.setTextSize(2);
    tft.setCursor(10, 300);
    tft.print(toggle ? "LOOP: TICK" : "LOOP: TOCK");
    Serial.println(toggle ? "Tick" : "Tock");
  }
}