// 持續 ping 閘道並記錄延遲（非阻塞，用 ESP-IDF 的 esp_ping，不會卡住 loop）
#pragma once
#include <Arduino.h>

namespace PingMonitor {

// 依連線狀態與目標位址啟停 ping 工作；沒有變化時什麼都不做，可以重複呼叫
void update(bool connected, const IPAddress &target);

// 取得最新一次結果：延遲毫秒數；逾時（封包遺失）回傳 -1；還沒有任何結果或沒在 ping 回傳 NAN
// 這一秒沒有新結果時，沿用上一次的值，避免圖表出現破洞
float takeSample();

}  // namespace PingMonitor
