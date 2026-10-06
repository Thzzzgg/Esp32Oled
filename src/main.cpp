// V0.4.0 Wi-Fi 配網、連線狀態、即時圖表與 OTA 無線更新
//
// 流程：
//   開機 → 有儲存的帳密就連線；沒有或連線逾時就開配網熱點
//   配網：手機掃螢幕上的 QR code 加入熱點，在網頁輸入要連的 Wi-Fi
//   連上後有三頁：連線狀態（SSID、IP…）、網路圖表（RSSI、ping）、系統圖表（記憶體、晶片溫度）
//   頁面每 10 秒自動輪播；短按 BOOT 鍵切到下一頁並停止輪播
//   長按 BOOT 鍵 3 秒：清除已儲存的 Wi-Fi 並重新開機進入配網
//   已連線時可用 PlatformIO 的 ota 環境無線更新韌體（需要 OTA 密碼，見 README）
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "monitor.h"
#include "ota_update.h"
#include "ui.h"
#include "wifi_portal.h"
#include "wifi_store.h"

enum class State {
  Connecting,        // 用已儲存的帳密連線
  Portal,            // 配網熱點開著，等手機送出帳密
  PortalConnecting,  // 嘗試用手機送出的帳密連線
  PortalDone,        // 配網成功，保留熱點一小段時間讓手機網頁讀到結果
  Connected,         // 正常運作，顯示連線狀態
};

State state = State::Connecting;
uint32_t stateSince = 0;
uint32_t lastUiMs = 0;

String savedSsid, savedPass;  // 目前使用中的帳密
String trySsid, tryPass;      // 配網時正在嘗試的帳密
PortalView portalView;

uint32_t lostSince = 0;   // 斷線開始的時間，0 表示目前沒斷線
uint32_t lastRetry = 0;

// 已連線後的頁面：0 狀態、1 網路圖表、2 系統圖表
constexpr int PAGE_COUNT = 3;
int page = 0;
uint32_t pageSince = 0;     // 切到目前頁的時間，自動輪播用
bool autoRotate = true;     // 按過按鍵後改成 false
bool shortPressed = false;  // 由 checkButton() 設定、loopConnected() 取用

// 圖表規格欄位順序：標題、單位、主色、自動縮放、固定下限、固定上限、最小跨度、下限為 0、
//                  黃色門檻、紅色門檻、越大越糟、負值為封包遺失、小數位數
const ChartSpec SPEC_RSSI = {"RSSI", "dBm", Accent::Cyan, false, -100, -30, 0, false, -65, -80, false, false, 0};
const ChartSpec SPEC_PING = {"PING", "ms", Accent::Green, true, 0, 0, 10, true, 50, 150, true, true, 0};
const ChartSpec SPEC_HEAP = {"HEAP", "KB", Accent::Orange, true, 0, 0, 10, false, NAN, NAN, false, false, 0};
const ChartSpec SPEC_TEMP = {"TEMP", "C", Accent::Magenta, true, 0, 0, 4, false, NAN, NAN, false, false, 1};

void enterState(State next) {
  state = next;
  stateSince = millis();
  lastUiMs = 0;  // 立刻刷新畫面
}

void startStation(const String &ssid, const String &pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
}

void enterPortal() {
  Ui::showSplash("Scanning Wi-Fi...");
  WifiPortal::begin();
  portalView = PortalView();
  portalView.apSsid = WifiPortal::apSsid();
  portalView.apPass = WifiPortal::apPassword();
  portalView.apIp = WifiPortal::apIp();
  Serial.printf("配網熱點：%s 密碼：%s 網址：http://%s\n", portalView.apSsid.c_str(), portalView.apPass.c_str(), portalView.apIp.c_str());
  enterState(State::Portal);
}

void onConnected() {
  Serial.printf("已連線：%s IP：%s RSSI：%d dBm\n", savedSsid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  lostSince = 0;
  page = 0;
  pageSince = millis();
  autoRotate = true;
  OtaUpdate::begin();
  enterState(State::Connected);
}

WifiInfo currentInfo() {
  WifiInfo info;
  info.connected = WiFi.status() == WL_CONNECTED;
  info.ssid = savedSsid;
  if (info.connected) {
    info.ip = WiFi.localIP().toString();
    info.gateway = WiFi.gatewayIP().toString();
    info.rssi = WiFi.RSSI();
  }
  info.uptimeSec = millis() / 1000;
  info.otaEnabled = OtaUpdate::enabled();
  return info;
}

// 按鍵：短按（放開時按下的時間介於去彈跳與長按門檻之間）切換頁面；長按清除 Wi-Fi 設定。
// GPIO0 是 strapping 腳，只在開機後才當按鍵用（開機時按住會進入燒錄模式）
void checkButton() {
  static uint32_t pressedAt = 0;
  uint32_t now = millis();

  if (digitalRead(PIN_BTN) == LOW) {
    if (pressedAt == 0) {
      pressedAt = now ? now : 1;
    } else if (now - pressedAt >= BTN_LONG_PRESS_MS) {
      Serial.println("長按 BOOT：清除 Wi-Fi 設定並重新開機");
      WifiStore::clear();
      Ui::showSplash("Wi-Fi settings cleared", "Restarting...");
      delay(1500);
      ESP.restart();
    }
    return;
  }

  if (pressedAt != 0) {
    uint32_t held = now - pressedAt;
    if (held >= BTN_DEBOUNCE_MS && held < BTN_LONG_PRESS_MS) shortPressed = true;
    pressedAt = 0;
  }
}

void updatePortalUi(uint32_t now) {
  if (now - lastUiMs < UI_REFRESH_MS) return;
  lastUiMs = now;
  portalView.clientJoined = WifiPortal::clientJoined();
  Ui::showPortal(portalView);
}

void loopConnecting(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    onConnected();
    return;
  }
  if (now - stateSince >= WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("連線逾時，改開配網熱點");
    WiFi.disconnect();
    enterPortal();
    return;
  }
  if (now - lastUiMs >= UI_REFRESH_MS) {
    lastUiMs = now;
    Ui::showConnecting(savedSsid, now - stateSince);
  }
}

void loopPortal(uint32_t now) {
  WifiPortal::handle();
  updatePortalUi(now);

  if (WifiPortal::takeSubmitted(trySsid, tryPass)) {
    Serial.printf("收到手機送出的帳密，嘗試連線：%s\n", trySsid.c_str());
    portalView.note = PortalNote::Connecting;
    portalView.noteDetail = trySsid;
    WifiPortal::setResult(WifiPortal::Result::Connecting);
    WiFi.begin(trySsid.c_str(), tryPass.c_str());  // 熱點開著，維持 AP+STA 模式
    enterState(State::PortalConnecting);
  }
}

void loopPortalConnecting(uint32_t now) {
  WifiPortal::handle();
  updatePortalUi(now);

  if (WiFi.status() == WL_CONNECTED) {
    savedSsid = trySsid;
    savedPass = tryPass;
    WifiStore::save(savedSsid, savedPass);
    String ip = WiFi.localIP().toString();
    Serial.printf("配網成功：%s IP：%s\n", savedSsid.c_str(), ip.c_str());
    portalView.note = PortalNote::Connected;
    portalView.noteDetail = ip;
    WifiPortal::setResult(WifiPortal::Result::Ok, ip);
    enterState(State::PortalDone);
  } else if (now - stateSince >= WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("配網連線失敗，回到配網畫面");
    WiFi.disconnect();  // 停止重試，否則 STA 反覆掃描頻道會干擾熱點
    portalView.note = PortalNote::Failed;
    WifiPortal::setResult(WifiPortal::Result::Failed);
    enterState(State::Portal);
  }
}

void loopPortalDone(uint32_t now) {
  WifiPortal::handle();
  updatePortalUi(now);

  if (now - stateSince >= PORTAL_DONE_HOLD_MS) {
    WifiPortal::end();
    onConnected();
  }
}

void renderPage() {
  switch (page) {
    case 0: Ui::showWifiStatus(currentInfo(), page, PAGE_COUNT); break;
    case 1: Ui::showChartPage(page, PAGE_COUNT, SPEC_RSSI, Monitor::rssi(), SPEC_PING, Monitor::ping()); break;
    default: Ui::showChartPage(page, PAGE_COUNT, SPEC_HEAP, Monitor::heapKb(), SPEC_TEMP, Monitor::chipTemp()); break;
  }
}

void loopConnected(uint32_t now) {
  OtaUpdate::handle();  // 收到更新時會卡在這裡直到結束（成功後自動重新開機）
  bool sampled = Monitor::tick(now);

  bool pageChanged = false;
  if (shortPressed) {
    autoRotate = false;  // 手動切換後停止自動輪播
    page = (page + 1) % PAGE_COUNT;
    pageChanged = true;
  } else if (autoRotate && now - pageSince >= AUTO_ROTATE_MS) {
    page = (page + 1) % PAGE_COUNT;
    pageChanged = true;
  }
  if (pageChanged) pageSince = now;

  // 狀態頁要每 UI_REFRESH_MS 更新（運行時間、訊號）；圖表頁只在有新取樣時重畫
  bool redraw = pageChanged || (page == 0 ? now - lastUiMs >= UI_REFRESH_MS : sampled);
  if (redraw) {
    lastUiMs = now;
    renderPage();
  }

  // 斷線時 Arduino 核心會自動重連；超過一段時間仍沒連上，再主動要求重連
  if (WiFi.status() == WL_CONNECTED) {
    lostSince = 0;
    return;
  }
  if (lostSince == 0) lostSince = now;
  if (now - lostSince >= WIFI_RETRY_AFTER_MS && now - lastRetry >= WIFI_RETRY_AFTER_MS) {
    lastRetry = now;
    Serial.println("Wi-Fi 斷線，嘗試重連");
    WiFi.reconnect();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN, INPUT_PULLUP);

  Ui::begin();
  Ui::showSplash("Booting...");

  // 帳密由 WifiStore 自己管理，不讓 Wi-Fi 驅動另外寫入 flash
  WiFi.persistent(false);
  WiFi.setHostname(WIFI_HOSTNAME);

#ifdef WOKWI_SIM
  // Wokwi 模擬：沒有手機可以配網，直接連官方虛擬 Wi-Fi（開放網路、頻道 6，指定頻道連線較快）
  savedSsid = "Wokwi-GUEST";
  savedPass = "";
  Serial.println("Wokwi 模擬：連線到 Wokwi-GUEST");
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str(), 6);
  enterState(State::Connecting);
#else
  if (WifiStore::load(savedSsid, savedPass)) {
    Serial.printf("使用已儲存的 Wi-Fi：%s\n", savedSsid.c_str());
    startStation(savedSsid, savedPass);
    enterState(State::Connecting);
  } else {
    Serial.println("尚未設定 Wi-Fi，進入配網");
    enterPortal();
  }
#endif
}

void loop() {
  checkButton();
  uint32_t now = millis();

  switch (state) {
    case State::Connecting: loopConnecting(now); break;
    case State::Portal: loopPortal(now); break;
    case State::PortalConnecting: loopPortalConnecting(now); break;
    case State::PortalDone: loopPortalDone(now); break;
    case State::Connected: loopConnected(now); break;
  }
  shortPressed = false;  // 只有已連線狀態會用到短按，其他狀態把它丟掉，避免之後誤觸發

  delay(2);  // 讓出 CPU 給 Wi-Fi 背景工作
}
