// 畫面模組：所有 TFT 繪圖都集中在這裡，main.cpp 只丟資料進來
#pragma once
#include <Arduino.h>

#include "history.h"

// 圖表折線與數值的主色
enum class Accent : uint8_t { Cyan, Green, Orange, Magenta };

// 一張圖表的外觀與縮放規則
struct ChartSpec {
  const char *label;    // 左上角標題，例如 "RSSI"（內建字型只含 ASCII）
  const char *unit;     // 單位，例如 "dBm"
  Accent accent;
  bool autoRange;       // true：依畫面上的資料自動縮放；false：固定用 lo～hi
  float lo, hi;         // 固定範圍
  float minSpan;        // 自動縮放的最小跨度，避免資料很平時把雜訊放大
  bool zeroBased;       // 自動縮放時下限固定為 0（用於 ping 這類不會是負數的值）
  float warn, bad;      // 數值上色的門檻（黃、紅）；NAN 表示不上色，一律用主色
  bool higherIsWorse;   // true：數值越大越糟（ping）；false：越小越糟（RSSI）
  bool negativeIsLoss;  // true：負值代表封包遺失，畫成紅色直線（ping）
  uint8_t decimals;     // 數值顯示的小數位數
};

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

// 連線狀態頁：SSID、IP、訊號強度、閘道、運行時間；page / pageCount 用來畫左側的頁面指示點
void showWifiStatus(const WifiInfo &info, int page, int pageCount);

// 圖表頁：上下兩張即時折線圖；有新資料時呼叫即可重畫
void showChartPage(int page, int pageCount, const ChartSpec &specA, const History &histA, const ChartSpec &specB, const History &histB);

}  // namespace Ui
