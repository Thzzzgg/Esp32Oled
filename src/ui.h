// 畫面模組：所有 TFT 繪圖都集中在這裡，main.cpp 只丟資料進來
#pragma once
#include <Arduino.h>

// 連線狀態頁要顯示的資料
struct WifiInfo {
  bool connected = false;
  String ssid;
  String ip;
  String gateway;
  int rssi = 0;
  uint32_t uptimeSec = 0;
};

// 配網畫面右下角的提示
enum class PortalNote : uint8_t { None, Connecting, Failed, Connected };

// 配網畫面要顯示的資料
struct PortalView {
  String apSsid;
  String apPass;
  String apIp;
  bool clientJoined = false;
  PortalNote note = PortalNote::None;
  String noteDetail;  // Connecting 時為目標 SSID，Connected 時為 IP
};

namespace Ui {

// 開背光、初始化螢幕（橫向 320x170）
void begin();

// 開機等待畫面
void showSplash(const char *line1, const char *line2 = "");

// 連線中畫面：顯示目標 SSID 與逾時進度條
void showConnecting(const String &ssid, uint32_t elapsedMs);

// 配網畫面：左邊 QR code，右邊文字說明
void showPortal(const PortalView &view);

// 連線狀態頁：SSID、IP、訊號強度、閘道、運行時間
void showWifiStatus(const WifiInfo &info);

}  // namespace Ui
