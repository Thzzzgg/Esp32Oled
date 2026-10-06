// Wi-Fi 帳密儲存（ESP32 的 NVS，斷電後仍保留）
#pragma once
#include <Arduino.h>

namespace WifiStore {

// 讀出已儲存的帳密；沒有儲存過 SSID 時回傳 false
bool load(String &ssid, String &pass);

void save(const String &ssid, const String &pass);

void clear();

}  // namespace WifiStore
