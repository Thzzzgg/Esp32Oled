// 全域設定：腳位與時間常數
#pragma once
#include <Arduino.h>

constexpr uint8_t PIN_BLK = 32;  // 背光腳位（社群資料，待驗證）
constexpr uint8_t PIN_BTN = 0;   // BOOT 按鍵（一般 ESP32 開發板都有，本板待驗證）

constexpr char WIFI_HOSTNAME[] = "esp32-lcd";

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;  // 連線逾時，逾時後改開配網熱點
constexpr uint32_t PORTAL_DONE_HOLD_MS = 5000;       // 配網成功後保留熱點的時間，讓手機網頁讀到結果
constexpr uint32_t WIFI_RETRY_AFTER_MS = 10000;      // 斷線超過這段時間，主動要求重連
constexpr uint32_t BTN_LONG_PRESS_MS = 3000;         // 長按 BOOT 清除 Wi-Fi 設定
constexpr uint32_t UI_REFRESH_MS = 250;              // 畫面更新間隔
