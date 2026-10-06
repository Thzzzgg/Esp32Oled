// 配網模組：開 Wi-Fi 熱點與設定網頁，讓手機輸入要連的 Wi-Fi 名稱與密碼
#pragma once
#include <Arduino.h>

namespace WifiPortal {

// 手機端網頁看到的連線結果
enum class Result : uint8_t { Idle, Connecting, Ok, Failed };

// 掃描附近的 Wi-Fi（約數秒，會卡住），再啟動熱點、DNS 與設定網頁
void begin();

// 關閉熱點與網頁，回到純 STA 模式（已連上的 Wi-Fi 不受影響）
void end();

// 放在 loop() 內，處理 DNS 與 HTTP 請求；不可長時間不呼叫
void handle();

const String &apSsid();
const String &apPassword();
String apIp();

// 是否已有手機連上熱點
bool clientJoined();

// 取走網頁送出的帳密；有新資料時回傳 true（只會回傳一次）
bool takeSubmitted(String &ssid, String &pass);

// 回報連線結果，供手機網頁的 /status 查詢
void setResult(Result result, const String &ip = "");

}  // namespace WifiPortal
