// 最小測試程式：開背光、依序填紅／綠／藍／白、顯示文字
// 用途：板子到手後，確認接腳、背光、顏色（RGB/BGR、反相）與 170x320 偏移是否正確
#include <Arduino.h>
#include <TFT_eSPI.h>

constexpr uint8_t PIN_BLK = 32;  // 背光腳位（社群資料，待驗證）

TFT_eSPI tft;

// 填滿整個畫面並在中央顯示色名，方便肉眼核對顏色與邊緣是否有偏移
void showColor(uint16_t color, const char *name) {
  tft.fillScreen(color);
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(name, tft.width() / 2, tft.height() / 2, 4);
}

void setup() {
  Serial.begin(115200);

  // 社群資料：背光可能需要先設 INPUT_PULLUP 再輸出 HIGH 才會亮
  pinMode(PIN_BLK, INPUT_PULLUP);
  digitalWrite(PIN_BLK, HIGH);

  tft.init();
  tft.setRotation(1);  // 橫向，320x170
  Serial.printf("TFT 尺寸：%d x %d\n", tft.width(), tft.height());
}

void loop() {
  showColor(TFT_RED, "RED");
  delay(1000);
  showColor(TFT_GREEN, "GREEN");
  delay(1000);
  showColor(TFT_BLUE, "BLUE");
  delay(1000);
  showColor(TFT_WHITE, "WHITE");
  delay(1000);
}
