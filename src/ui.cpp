#include "ui.h"

#include <TFT_eSPI.h>
#include <qrcode.h>

#include "config.h"

namespace {

TFT_eSPI tft;

constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_TEXT = TFT_WHITE;
constexpr uint16_t COLOR_LABEL = TFT_DARKGREY;
constexpr uint16_t COLOR_OK = TFT_GREEN;
constexpr uint16_t COLOR_WARN = TFT_YELLOW;
constexpr uint16_t COLOR_BAD = TFT_RED;

// 字型 2 高 16px、字型 4 高 26px（TFT_eSPI 內建，只含 ASCII，中文會顯示成亂碼）
constexpr uint8_t FONT_SMALL = 2;
constexpr uint8_t FONT_LARGE = 4;

enum class Screen { None, Splash, Connecting, Portal, Status };
Screen currentScreen = Screen::None;

// 切換畫面時清空整個螢幕；回傳 true 表示這次是剛切換進來，需要畫靜態內容
bool enterScreen(Screen next) {
  if (currentScreen == next) return false;
  currentScreen = next;
  tft.fillScreen(COLOR_BG);
  return true;
}

// 字串太寬時截斷並加上 ".."（以像素寬度判斷，不是字元數）
String fitText(String s, uint8_t font, int maxPx) {
  if (tft.textWidth(s, font) <= maxPx) return s;
  while (s.length() > 1 && tft.textWidth(s + "..", font) > maxPx) s.remove(s.length() - 1);
  return s + "..";
}

// 在左上角座標 (x, y) 畫文字
void drawText(int x, int y, uint8_t font, const String &s, uint16_t color, int maxPx) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, COLOR_BG);
  tft.setTextPadding(0);
  tft.drawString(fitText(s, font, maxPx), x, y, font);
}

void drawCentered(int y, uint8_t font, const String &s, uint16_t color) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(color, COLOR_BG);
  tft.setTextPadding(0);
  tft.drawString(fitText(s, font, tft.width() - 8), tft.width() / 2, y, font);
}

// 值有變才重畫；padding 會把舊文字右側殘留的部分用背景色蓋掉，不用整塊清除所以不閃
void drawValue(String &cache, int x, int y, uint8_t font, const String &s, uint16_t color, int maxPx) {
  if (s == cache) return;
  cache = s;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, COLOR_BG);
  tft.setTextPadding(maxPx);
  tft.drawString(fitText(s, font, maxPx), x, y, font);
  tft.setTextPadding(0);
}

// ---- QR code ----

// 版本 4（33x33）、ECC 中等，可放 62 bytes；Wi-Fi 加入字串與網址都放得下
constexpr uint8_t QR_VERSION = 4;
constexpr uint8_t QR_ECC = 1;  // ECC_MEDIUM
constexpr int QR_QUIET = 2;    // 四周留白的模組數（深色背景上需要白底才掃得到）

void drawQr(const String &text, int x, int y, int box) {
  static uint8_t data[256];
  if (qrcode_getBufferSize(QR_VERSION) > sizeof(data)) return;
  QRCode qr;
  if (qrcode_initText(&qr, data, QR_VERSION, QR_ECC, text.c_str()) < 0) return;

  int scale = box / (qr.size + 2 * QR_QUIET);
  if (scale < 1) scale = 1;
  int side = (qr.size + 2 * QR_QUIET) * scale;
  int ox = x + (box - side) / 2;
  int oy = y + (box - side) / 2;

  tft.fillRect(ox, oy, side, side, TFT_WHITE);
  for (int my = 0; my < qr.size; my++) {
    for (int mx = 0; mx < qr.size; mx++) {
      if (qrcode_getModule(&qr, mx, my)) {
        tft.fillRect(ox + (QR_QUIET + mx) * scale, oy + (QR_QUIET + my) * scale, scale, scale, TFT_BLACK);
      }
    }
  }
}

// ---- 配網畫面 ----

constexpr int QR_X = 6;
constexpr int QR_Y = 11;
constexpr int QR_BOX = 148;
constexpr int PORTAL_PANEL_X = 166;
constexpr int PORTAL_PANEL_W = 154;

String portalKey;
String portalQrText;

// ---- 連線中畫面 ----

constexpr int BAR_X = 20;
constexpr int BAR_Y = 140;
constexpr int BAR_H = 14;
int lastDotPhase = -1;
int lastBarWidth = -1;

// ---- 狀態頁 ----

constexpr int LABEL_X = 10;
constexpr int VALUE_X = 100;
constexpr int VALUE_W = 220 - 4;
constexpr int BARS_X = 262;
constexpr int ROW_SSID_Y = 36;
constexpr int ROW_IP_Y = 64;
constexpr int ROW_RSSI_Y = 92;
constexpr int ROW_GW_Y = 124;
constexpr int ROW_UP_Y = 146;

struct StatusCache {
  String title, ssid, ip, rssi, gateway, uptime;
  int level = -1;
};
StatusCache status;

// RSSI 轉成 0～4 格
int rssiLevel(int rssi) {
  if (rssi > -55) return 4;
  if (rssi > -65) return 3;
  if (rssi > -75) return 2;
  if (rssi > -85) return 1;
  return 0;
}

uint16_t levelColor(int level) {
  if (level >= 3) return COLOR_OK;
  if (level == 2) return COLOR_WARN;
  return COLOR_BAD;
}

void drawSignalBars(int x, int y, int level) {
  for (int i = 0; i < 4; i++) {
    int h = 6 + i * 5;
    uint16_t color = (i < level) ? levelColor(level) : TFT_DARKGREY;
    tft.fillRect(x + i * 9, y + 24 - h, 6, h, color);
  }
}

String formatUptime(uint32_t sec) {
  uint32_t days = sec / 86400;
  uint32_t h = (sec / 3600) % 24;
  uint32_t m = (sec / 60) % 60;
  uint32_t s = sec % 60;
  char buf[24];
  if (days > 0) {
    snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", (unsigned long)days, (unsigned long)h, (unsigned long)m, (unsigned long)s);
  } else {
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  }
  return buf;
}

}  // namespace

namespace Ui {

void begin() {
  // 社群資料：背光可能需要先設 INPUT_PULLUP 再輸出 HIGH 才會亮
  pinMode(PIN_BLK, INPUT_PULLUP);
  digitalWrite(PIN_BLK, HIGH);

  tft.init();
  tft.setRotation(1);  // 橫向，320x170
  tft.fillScreen(COLOR_BG);
}

void showSplash(const char *line1, const char *line2) {
  currentScreen = Screen::Splash;
  tft.fillScreen(COLOR_BG);
  drawCentered(50, FONT_LARGE, "ESP32 LCD", COLOR_TEXT);
  drawCentered(90, FONT_SMALL, line1, COLOR_LABEL);
  if (line2[0] != '\0') drawCentered(112, FONT_SMALL, line2, COLOR_LABEL);
}

void showConnecting(const String &ssid, uint32_t elapsedMs) {
  if (enterScreen(Screen::Connecting)) {
    lastDotPhase = -1;
    lastBarWidth = -1;
    drawCentered(14, FONT_LARGE, "CONNECTING", COLOR_WARN);
    drawCentered(60, FONT_SMALL, "Wi-Fi network", COLOR_LABEL);
    drawCentered(80, FONT_LARGE, ssid, COLOR_TEXT);
    tft.drawRect(BAR_X - 1, BAR_Y - 1, tft.width() - 2 * BAR_X + 2, BAR_H + 2, COLOR_LABEL);
  }

  int dotPhase = (elapsedMs / 500) % 4;
  if (dotPhase != lastDotPhase) {
    lastDotPhase = dotPhase;
    String dots;
    for (int i = 0; i < dotPhase; i++) dots += '.';
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.setTextPadding(60);
    tft.drawString(dots, tft.width() / 2, 112, FONT_LARGE);
    tft.setTextPadding(0);
  }

  int barMax = tft.width() - 2 * BAR_X;
  int w = (int)((uint64_t)elapsedMs * barMax / WIFI_CONNECT_TIMEOUT_MS);
  if (w > barMax) w = barMax;
  if (w != lastBarWidth) {
    lastBarWidth = w;
    tft.fillRect(BAR_X, BAR_Y, w, BAR_H, COLOR_WARN);
  }
}

void showPortal(const PortalView &view) {
  String key = String(view.clientJoined) + "|" + (int)view.note + "|" + view.noteDetail + "|" + view.apSsid + "|" + view.apPass;
  bool entered = enterScreen(Screen::Portal);
  if (!entered && key == portalKey) return;
  portalKey = key;

  // 沒人連上熱點時，QR code 是「加入熱點」；有人連上後換成設定網頁網址
  String qrText = view.clientJoined ? String("http://") + view.apIp : String("WIFI:T:WPA;S:") + view.apSsid + ";P:" + view.apPass + ";;";
  if (entered || qrText != portalQrText) {
    portalQrText = qrText;
    tft.fillRect(0, 0, PORTAL_PANEL_X, tft.height(), COLOR_BG);
    drawQr(qrText, QR_X, QR_Y, QR_BOX);
  }

  // 右側文字區整塊清掉重畫
  tft.fillRect(PORTAL_PANEL_X, 0, PORTAL_PANEL_W, tft.height(), COLOR_BG);
  const int x = PORTAL_PANEL_X;
  const int w = PORTAL_PANEL_W;

  if (!view.clientJoined) {
    drawText(x, 6, FONT_LARGE, "Join Wi-Fi", COLOR_WARN, w);
    drawText(x, 44, FONT_SMALL, "SSID", COLOR_LABEL, w);
    drawText(x, 60, FONT_SMALL, view.apSsid, COLOR_TEXT, w);
    drawText(x, 84, FONT_SMALL, "PASS", COLOR_LABEL, w);
    drawText(x, 100, FONT_SMALL, view.apPass, COLOR_TEXT, w);
  } else {
    drawText(x, 6, FONT_LARGE, "Open page", COLOR_WARN, w);
    drawText(x, 44, FONT_SMALL, "URL", COLOR_LABEL, w);
    drawText(x, 60, FONT_LARGE, view.apIp, COLOR_TEXT, w);
  }

  // 右下角兩行提示
  switch (view.note) {
    case PortalNote::Connecting:
      drawText(x, 126, FONT_SMALL, "Connecting to", COLOR_WARN, w);
      drawText(x, 144, FONT_SMALL, view.noteDetail, COLOR_TEXT, w);
      break;
    case PortalNote::Failed:
      drawText(x, 126, FONT_SMALL, "Connect failed", COLOR_BAD, w);
      drawText(x, 144, FONT_SMALL, "Check password", COLOR_TEXT, w);
      break;
    case PortalNote::Connected:
      drawText(x, 126, FONT_SMALL, "Connected!", COLOR_OK, w);
      drawText(x, 144, FONT_SMALL, view.noteDetail, COLOR_TEXT, w);
      break;
    case PortalNote::None:
      if (!view.clientJoined) {
        drawText(x, 126, FONT_SMALL, "Scan QR or type", COLOR_LABEL, w);
        drawText(x, 144, FONT_SMALL, "SSID and PASS", COLOR_LABEL, w);
      } else {
        drawText(x, 126, FONT_SMALL, "Page not shown?", COLOR_LABEL, w);
        drawText(x, 144, FONT_SMALL, "Scan QR or open URL", COLOR_LABEL, w);
      }
      break;
  }
}

void showWifiStatus(const WifiInfo &info) {
  if (enterScreen(Screen::Status)) {
    status = StatusCache();
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setTextPadding(0);
    tft.drawString("SSID", LABEL_X, ROW_SSID_Y + 5, FONT_SMALL);
    tft.drawString("IP", LABEL_X, ROW_IP_Y + 5, FONT_SMALL);
    tft.drawString("SIGNAL", LABEL_X, ROW_RSSI_Y + 5, FONT_SMALL);
    tft.drawString("GATEWAY", LABEL_X, ROW_GW_Y, FONT_SMALL);
    tft.drawString("UPTIME", LABEL_X, ROW_UP_Y, FONT_SMALL);
  }

  // 標題列：連線／斷線
  const String title = info.connected ? "WIFI CONNECTED" : "WIFI LOST";
  drawValue(status.title, LABEL_X, 4, FONT_LARGE, title, info.connected ? COLOR_OK : COLOR_BAD, tft.width() - 2 * LABEL_X);

  const String none = "--";
  drawValue(status.ssid, VALUE_X, ROW_SSID_Y, FONT_LARGE, info.ssid.length() ? info.ssid : none, COLOR_TEXT, VALUE_W);
  drawValue(status.ip, VALUE_X, ROW_IP_Y, FONT_LARGE, info.connected ? info.ip : none, COLOR_TEXT, VALUE_W);

  int level = info.connected ? rssiLevel(info.rssi) : 0;
  uint16_t rssiColor = info.connected ? levelColor(level) : COLOR_LABEL;
  drawValue(status.rssi, VALUE_X, ROW_RSSI_Y, FONT_LARGE, info.connected ? String(info.rssi) + " dBm" : none, rssiColor, BARS_X - VALUE_X - 6);
  if (level != status.level) {
    status.level = level;
    drawSignalBars(BARS_X, ROW_RSSI_Y, level);
  }

  drawValue(status.gateway, VALUE_X, ROW_GW_Y, FONT_SMALL, info.connected ? info.gateway : none, COLOR_TEXT, VALUE_W);
  drawValue(status.uptime, VALUE_X, ROW_UP_Y, FONT_SMALL, formatUptime(info.uptimeSec), COLOR_TEXT, VALUE_W);
}

}  // namespace Ui
