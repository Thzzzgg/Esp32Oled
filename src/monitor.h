// 資料來源：每秒取樣一次板載資料，存進歷史供圖表使用
#pragma once
#include <Arduino.h>

#include "history.h"

namespace Monitor {

// 放在 loop() 內；到了取樣時間才會真的取樣，回傳 true 表示這次有新資料
bool tick(uint32_t now);

const History &rssi();      // Wi-Fi 訊號強度（dBm），斷線為 NAN
const History &ping();      // 到閘道的 ping 延遲（ms），-1 為封包遺失，沒資料為 NAN
const History &heapKb();    // 剩餘記憶體（KB）
const History &chipTemp();  // 晶片內部溫度（°C），未校準，只能當趨勢參考

}  // namespace Monitor
