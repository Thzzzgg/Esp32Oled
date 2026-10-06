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
constexpr uint32_t BTN_DEBOUNCE_MS = 40;             // 按鍵去彈跳，低於此時間視為雜訊

constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;        // 圖表資料的取樣間隔
constexpr size_t HISTORY_LEN = 280;                  // 保留的取樣數，等於圖表寬度（像素），約 4 分 40 秒
constexpr uint32_t AUTO_ROTATE_MS = 10000;           // 自動輪播頁面的間隔（按過按鍵後停止輪播）

// ---- 門檻警報（EVA 風格警報畫面）----
// 條件要連續成立 ALERT_RAISE_SAMPLES 次取樣（約秒）才觸發，連續不成立 ALERT_CLEAR_SAMPLES 次才解除，避免來回閃
constexpr float ALERT_RSSI_DBM = -80;            // 訊號強度低於或等於此值：警告（WARNING）
constexpr int ALERT_PING_LOSS_STREAK = 3;        // ping 連續遺失幾次：異常（ANOMALY）
constexpr float ALERT_HEAP_KB = 40;              // 剩餘記憶體低於此值（KB）：危險（DANGER）
                                                 // Wi-Fi 斷線：緊急（EMERGENCY）
constexpr int ALERT_RAISE_SAMPLES = 3;
constexpr int ALERT_CLEAR_SAMPLES = 5;
constexpr uint32_t ALERT_MUTE_MS = 60000;        // 警報中短按 BOOT 靜音多久，時間到若仍未解除會再次出現
constexpr uint32_t ALERT_FRAME_MS = 80;          // 警報動畫的畫格間隔（約 12 fps）
