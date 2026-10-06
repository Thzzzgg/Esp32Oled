#include "alert.h"

#include <math.h>

#include "config.h"
#include "monitor.h"

namespace {

AlertKind current = AlertKind::None;  // 目前生效中的警報
AlertKind pending = AlertKind::None;  // 條件成立、但還沒連續夠久的候選警報
int pendingCount = 0;
int clearCount = 0;
bool muted = false;
uint32_t mutedUntil = 0;

const char *kindName(AlertKind kind) {
  switch (kind) {
    case AlertKind::NetworkLost: return "NETWORK LOST";
    case AlertKind::MemoryLow: return "MEMORY LOW";
    case AlertKind::PingLoss: return "PING LOSS";
    case AlertKind::SignalWeak: return "SIGNAL WEAK";
    default: return "NONE";
  }
}

// ping 歷史尾端連續遺失（負值）的次數；沒資料（NAN）會中斷計數
int pingLossStreak() {
  const History &h = Monitor::ping();
  int streak = 0;
  for (int i = (int)h.size() - 1; i >= 0; i--) {
    float v = h.at(i);
    if (isnan(v) || v >= 0) break;
    streak++;
  }
  return streak;
}

// 依目前取樣挑出最嚴重的條件
AlertKind evaluate(bool connected) {
  if (!connected) return AlertKind::NetworkLost;

  float heap = Monitor::heapKb().latest();
  if (!isnan(heap) && heap < ALERT_HEAP_KB) return AlertKind::MemoryLow;

  if (pingLossStreak() >= ALERT_PING_LOSS_STREAK) return AlertKind::PingLoss;

  float rssi = Monitor::rssi().latest();
  if (!isnan(rssi) && rssi <= ALERT_RSSI_DBM) return AlertKind::SignalWeak;

  return AlertKind::None;
}

}  // namespace

namespace Alert {

void update(bool connected) {
  AlertKind candidate = evaluate(connected);

  if (candidate == AlertKind::None) {
    pending = AlertKind::None;
    pendingCount = 0;
    if (current != AlertKind::None && ++clearCount >= ALERT_CLEAR_SAMPLES) {
      Serial.printf("警報解除：%s\n", kindName(current));
      current = AlertKind::None;
      clearCount = 0;
      muted = false;
    }
    return;
  }

  clearCount = 0;
  if (candidate == current) {  // 已經是目前的警報，不用重新計數
    pending = AlertKind::None;
    pendingCount = 0;
    return;
  }

  if (candidate == pending) {
    pendingCount++;
  } else {
    pending = candidate;
    pendingCount = 1;
  }
  if (pendingCount >= ALERT_RAISE_SAMPLES) {
    Serial.printf("警報觸發：%s\n", kindName(candidate));
    current = candidate;
    pending = AlertKind::None;
    pendingCount = 0;
    muted = false;  // 新的警報（或換了種類）要立刻顯示
  }
}

bool active(uint32_t now) {
  if (current == AlertKind::None) return false;
  if (muted && (int32_t)(mutedUntil - now) > 0) return false;
  muted = false;  // 靜音時間已過
  return true;
}

void mute(uint32_t now, uint32_t durationMs) {
  if (current == AlertKind::None) return;
  muted = true;
  mutedUntil = now + durationMs;
  Serial.printf("警報靜音 %lu 秒\n", (unsigned long)(durationMs / 1000));
}

AlertView view() {
  AlertView v;
  v.kind = current;
  switch (current) {
    case AlertKind::NetworkLost:
      v.detail = "NETWORK LOST";
      break;
    case AlertKind::MemoryLow:
      v.detail = String("HEAP ") + (int)Monitor::heapKb().latest() + " KB";
      break;
    case AlertKind::PingLoss:
      v.detail = String("PING LOSS ") + pingLossStreak() + " SEC";
      break;
    case AlertKind::SignalWeak:
      v.detail = String("SIGNAL ") + (int)Monitor::rssi().latest() + " DBM";
      break;
    default:
      break;
  }
  return v;
}

}  // namespace Alert
