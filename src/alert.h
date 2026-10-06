// 門檻警報：依 Monitor 的取樣判斷是否該拉警報，含去彈跳與靜音。只管判斷，畫面由 ui.cpp 負責
#pragma once
#include <Arduino.h>

// 依嚴重程度排序（數字越大越優先）：同時成立時只顯示最嚴重的一個
enum class AlertKind : uint8_t {
  None,
  SignalWeak,   // 訊號太弱      → 警告
  PingLoss,     // ping 連續遺失 → 異常
  MemoryLow,    // 記憶體不足    → 危險
  NetworkLost,  // Wi-Fi 斷線    → 緊急
};

// 警報畫面要顯示的資料
struct AlertView {
  AlertKind kind = AlertKind::None;
  String detail;  // 一行說明，例如 "SIGNAL -86 DBM"
};

namespace Alert {

// 每次 Monitor 有新取樣後呼叫一次（約每秒）
void update(bool connected);

// 有警報且沒被靜音
bool active(uint32_t now);

// 靜音目前的警報；時間到若仍未解除會再出現。警報種類改變或解除後，靜音即失效
void mute(uint32_t now, uint32_t durationMs);

// 目前警報的種類與說明文字（用最新的取樣值組出來）
AlertView view();

}  // namespace Alert
