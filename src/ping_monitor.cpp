#include "ping_monitor.h"

#include <lwip/ip_addr.h>
#include <ping/ping_sock.h>

#include <math.h>

namespace {

constexpr uint32_t START_RETRY_MS = 5000;

esp_ping_handle_t session = nullptr;
uint32_t activeTarget = 0;  // 目前 ping 的位址（IPAddress 轉成 uint32_t），0 表示沒在 ping
uint32_t lastStartTry = 0;

// 這兩個計數器由 ping 工作更新、loop 讀取；32 位元對齊讀寫在 ESP32 上是原子的
volatile uint32_t replyCount = 0;
volatile uint32_t timeoutCount = 0;
volatile uint32_t lastReplyMs = 0;

uint32_t seenReply = 0;
uint32_t seenTimeout = 0;
float held = NAN;  // 上一次的結果

void onPingSuccess(esp_ping_handle_t hdl, void *) {
  uint32_t elapsed = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
  lastReplyMs = elapsed;
  replyCount = replyCount + 1;
}

void onPingTimeout(esp_ping_handle_t, void *) { timeoutCount = timeoutCount + 1; }

void stopSession() {
  if (session) {
    esp_ping_stop(session);
    esp_ping_delete_session(session);
    session = nullptr;
  }
  activeTarget = 0;
  held = NAN;
}

bool startSession(const IPAddress &target) {
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.count = ESP_PING_COUNT_INFINITE;
  config.interval_ms = 1000;
  config.timeout_ms = 1000;
  config.data_size = 32;
  IP_ADDR4(&config.target_addr, target[0], target[1], target[2], target[3]);

  esp_ping_callbacks_t callbacks = {};
  callbacks.on_ping_success = onPingSuccess;
  callbacks.on_ping_timeout = onPingTimeout;

  if (esp_ping_new_session(&config, &callbacks, &session) != ESP_OK) {
    session = nullptr;
    return false;
  }
  seenReply = replyCount;
  seenTimeout = timeoutCount;
  held = NAN;
  esp_ping_start(session);
  return true;
}

}  // namespace

namespace PingMonitor {

void update(bool connected, const IPAddress &target) {
  uint32_t want = connected ? (uint32_t)target : 0;  // 0.0.0.0 視為沒有閘道

  if (want == 0) {
    if (session) stopSession();
    return;
  }
  if (session && want == activeTarget) return;

  // 目標變了（重連後閘道不同）或之前啟動失敗：重新開始，失敗時隔一段時間再試
  uint32_t now = millis();
  if (!session && lastStartTry != 0 && now - lastStartTry < START_RETRY_MS) return;
  lastStartTry = now ? now : 1;
  if (session) stopSession();
  if (startSession(target)) {
    activeTarget = want;
    Serial.printf("開始 ping 閘道：%s\n", target.toString().c_str());
  } else {
    Serial.println("ping 工作啟動失敗，稍後重試");
  }
}

float takeSample() {
  if (!session) return NAN;
  uint32_t replies = replyCount;
  uint32_t timeouts = timeoutCount;
  if (replies != seenReply) {
    held = (float)lastReplyMs;
  } else if (timeouts != seenTimeout) {
    held = -1;
  }
  seenReply = replies;
  seenTimeout = timeouts;
  return held;
}

}  // namespace PingMonitor
