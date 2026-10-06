#include "ota_update.h"

#include <Arduino.h>

#include "config.h"
#include "ui.h"

// 有提供密碼才啟用；Wokwi 模擬不支援 mDNS 與 OTA，一律停用
#if defined(OTA_PASSWORD) && !defined(WOKWI_SIM)
#define OTA_ACTIVE 1
#endif

#ifdef OTA_ACTIVE
#include <ArduinoOTA.h>
#endif

namespace {

bool started = false;

#ifdef OTA_ACTIVE
const char *errorName(ota_error_t error) {
  switch (error) {
    case OTA_AUTH_ERROR: return "Auth failed";
    case OTA_BEGIN_ERROR: return "Begin failed";
    case OTA_CONNECT_ERROR: return "Connect failed";
    case OTA_RECEIVE_ERROR: return "Receive failed";
    case OTA_END_ERROR: return "End failed";
    default: return "Unknown error";
  }
}
#endif

}  // namespace

namespace OtaUpdate {

bool enabled() {
#ifdef OTA_ACTIVE
  return true;
#else
  return false;
#endif
}

void begin() {
  if (started) return;
  started = true;

#ifdef OTA_ACTIVE
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA 開始");
    Ui::showOta(OtaPhase::Start, 0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    unsigned int percent = total ? (unsigned int)((uint64_t)done * 100 / total) : 0;
    Ui::showOta(OtaPhase::Progress, percent);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA 完成，重新開機");
    Ui::showOta(OtaPhase::Done, 100);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA 失敗：%s\n", errorName(error));
    Ui::showOta(OtaPhase::Failed, 0, errorName(error));
    delay(3000);  // 讓錯誤訊息停留一下，之後畫面會自動切回狀態頁
  });

  ArduinoOTA.begin();
  Serial.printf("OTA 已啟用：%s.local\n", WIFI_HOSTNAME);
#elif defined(WOKWI_SIM)
  Serial.println("Wokwi 模擬不支援 OTA，已停用");
#else
  Serial.println("未提供 OTA 密碼，無線更新停用（見 README）");
#endif
}

void handle() {
#ifdef OTA_ACTIVE
  if (started) ArduinoOTA.handle();
#endif
}

}  // namespace OtaUpdate
