#include "ui.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <qrcode.h>

#include "alert_glyphs.h"
#include "config.h"

namespace {

TFT_eSPI tft;
TFT_eSprite chartSprite(&tft);  // 圖表先畫在記憶體，再一次推到螢幕，避免重畫時閃爍
bool chartSpriteReady = false;

constexpr uint16_t COLOR_BG = TFT_BLACK;
constexpr uint16_t COLOR_TEXT = TFT_WHITE;
constexpr uint16_t COLOR_LABEL = TFT_DARKGREY;
constexpr uint16_t COLOR_OK = TFT_GREEN;
constexpr uint16_t COLOR_WARN = TFT_YELLOW;
constexpr uint16_t COLOR_BAD = TFT_RED;
constexpr uint16_t COLOR_DIM = 0x4208;   // 暗灰：非目前頁的指示點
constexpr uint16_t COLOR_GRID = 0x2104;  // 更暗的灰：圖表格線

// 版面一律以 320x170 為準。實機螢幕就是這個大小；Wokwi 模擬用的 ILI9341 是 320x240，
// 多出來的下方區域保持黑色，畫面看起來就和實機一致
constexpr int UI_W = 320;
constexpr int UI_H = 170;

// 字型 2 高 16px、字型 4 高 26px（TFT_eSPI 內建，只含 ASCII，中文會顯示成亂碼）
constexpr uint8_t FONT_SMALL = 2;
constexpr uint8_t FONT_LARGE = 4;

enum class Screen { None, Splash, Connecting, Portal, Status, Chart, Ota, Alert };
Screen currentScreen = Screen::None;
int currentSub = 0;  // 同一種畫面下的子頁編號（圖表頁有多張）

// 切換畫面時清空整個螢幕；回傳 true 表示這次是剛切換進來，需要畫靜態內容
bool enterScreen(Screen next, int sub = 0) {
  if (currentScreen == next && currentSub == sub) return false;
  currentScreen = next;
  currentSub = sub;
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

// ---- OTA 更新畫面 ----

constexpr int OTA_BAR_X = 20;
constexpr int OTA_BAR_Y = 96;
constexpr int OTA_BAR_H = 14;
int lastOtaPercent = -1;

// ---- 狀態頁 ----

constexpr int LABEL_X = 10;
constexpr int VALUE_X = 100;
constexpr int VALUE_W = 220 - 4;
constexpr int UPTIME_W = 140;  // 運行時間最寬約 83px；右邊留給 OTA 標籤
constexpr int OTA_TAG_X = 244;
constexpr int OTA_TAG_W = 320 - OTA_TAG_X - 2;
constexpr int BARS_X = 262;
constexpr int ROW_SSID_Y = 36;
constexpr int ROW_IP_Y = 64;
constexpr int ROW_RSSI_Y = 92;
constexpr int ROW_GW_Y = 124;
constexpr int ROW_UP_Y = 146;

struct StatusCache {
  String title, ssid, ip, rssi, gateway, uptime, ota;
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

// ---- 頁面指示點 ----

// 在螢幕左邊緣垂直排列，目前頁是白色大點，其餘是暗灰小點
void drawPageDots(int page, int pageCount) {
  if (pageCount < 2) return;
  const int spacing = 11;
  const int centerY = UI_H / 2;
  for (int i = 0; i < pageCount; i++) {
    int y = centerY + (2 * i - (pageCount - 1)) * spacing / 2;
    if (i == page) {
      tft.fillCircle(3, y, 3, TFT_WHITE);
    } else {
      tft.fillCircle(3, y, 2, COLOR_DIM);
    }
  }
}

// ---- 圖表頁 ----

constexpr int CHART_X = 8;
constexpr int CHART_W = HISTORY_LEN;  // 一個取樣一個像素
constexpr int CHART_H = 56;
constexpr int HEADER_H = 26;          // 每張圖上方的標題列（字型 4 的高度）
constexpr int PANEL_H = 85;           // 兩張圖各佔 85px，剛好填滿 170
constexpr int AXIS_X = CHART_X + CHART_W + 4;
constexpr int VALUE_RIGHT_X = 318;
constexpr int VALUE_PAD_W = 110;      // 「-100 dBm」（字型 4）約 109px
constexpr int CAPTION_X = 90;
constexpr int CAPTION_PAD_W = VALUE_RIGHT_X - VALUE_PAD_W - CAPTION_X - 2;  // 說明文字不可蓋到數值區

uint16_t accentColor(Accent accent) {
  switch (accent) {
    case Accent::Cyan: return TFT_CYAN;
    case Accent::Green: return TFT_GREEN;
    case Accent::Orange: return TFT_ORANGE;
    case Accent::Magenta: return TFT_MAGENTA;
  }
  return TFT_WHITE;
}

uint16_t valueColor(const ChartSpec &spec, float v) {
  if (isnan(spec.warn) || isnan(spec.bad)) return accentColor(spec.accent);
  bool bad = spec.higherIsWorse ? v >= spec.bad : v <= spec.bad;
  bool warn = spec.higherIsWorse ? v >= spec.warn : v <= spec.warn;
  return bad ? COLOR_BAD : (warn ? COLOR_WARN : COLOR_OK);
}

// 數值轉字串。Arduino 的 String(float, 小數位數) 在這個核心有多載歧義，改用 snprintf
String formatNumber(float v, uint8_t decimals) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", (int)decimals, (double)v);
  return String(buf);
}

// 自動縮放時，把上限往上取整成好讀的刻度
float niceCeil(float v) {
  static const float steps[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
  for (float s : steps) {
    if (v <= s) return s;
  }
  return v;
}

// 統計目前畫面上的資料：有效筆數、遺失筆數、最小與最大值
struct SeriesStats {
  int valid = 0;
  int lost = 0;
  float mn = INFINITY;
  float mx = -INFINITY;
};

SeriesStats computeStats(const ChartSpec &spec, const History &h) {
  SeriesStats s;
  for (size_t i = 0; i < h.size(); i++) {
    float v = h.at(i);
    if (isnan(v)) continue;
    if (spec.negativeIsLoss && v < 0) {
      s.lost++;
      continue;
    }
    s.valid++;
    if (v < s.mn) s.mn = v;
    if (v > s.mx) s.mx = v;
  }
  return s;
}

void computeRange(const ChartSpec &spec, const SeriesStats &s, float &lo, float &hi) {
  if (!spec.autoRange) {
    lo = spec.lo;
    hi = spec.hi;
    return;
  }
  float mn = s.valid ? s.mn : 0;
  float mx = s.valid ? s.mx : 0;
  if (spec.zeroBased) {
    lo = 0;
    hi = niceCeil(mx > spec.minSpan ? mx : spec.minSpan);
    return;
  }
  float span = mx - mn;
  if (span < spec.minSpan) {
    float mid = (mx + mn) / 2;
    lo = mid - spec.minSpan / 2;
    hi = mid + spec.minSpan / 2;
  } else {
    lo = mn - span * 0.1f;
    hi = mx + span * 0.1f;
  }
}

// 畫折線；G 可以是螢幕（TFT_eSPI）或記憶體畫布（TFT_eSprite），兩者的繪圖函式同名
template <class G>
void drawChartBody(G &g, int ox, int oy, const ChartSpec &spec, const History &h, float lo, float hi) {
  g.fillRect(ox, oy, CHART_W, CHART_H, COLOR_BG);

  // 上、中、下三條虛線格線
  for (int i = 0; i < 3; i++) {
    int y = oy + (CHART_H - 1) * i / 2;
    for (int x = ox; x < ox + CHART_W; x += 4) g.drawPixel(x, y, COLOR_GRID);
  }

  const uint16_t color = accentColor(spec.accent);
  const float range = (hi > lo) ? (hi - lo) : 1.0f;
  const int plotH = CHART_H - 2;  // 折線粗 2px，下緣多留 1px
  const int n = (int)h.size();
  const int x0 = ox + CHART_W - n;  // 靠右對齊，最新的資料在最右邊

  int px = 0, py = 0;
  bool havePrev = false;
  for (int i = 0; i < n; i++) {
    float v = h.at(i);
    int x = x0 + i;
    if (isnan(v)) {
      havePrev = false;  // 沒資料的地方斷線
      continue;
    }
    if (spec.negativeIsLoss && v < 0) {
      g.drawFastVLine(x, oy, CHART_H, COLOR_BAD);
      havePrev = false;
      continue;
    }
    float t = (v - lo) / range;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int y = oy + plotH - (int)(t * plotH);
    if (havePrev) {
      g.drawLine(px, py, x, y, color);
      g.drawLine(px, py + 1, x, y + 1, color);
    } else {
      g.drawPixel(x, y, color);
      g.drawPixel(x, y + 1, color);
    }
    px = x;
    py = y;
    havePrev = true;
  }
}

// 一張圖：標題列（名稱、說明、目前數值）＋折線＋右側刻度
void drawChartPanel(int panelY, const ChartSpec &spec, const History &h) {
  const SeriesStats stats = computeStats(spec, h);
  float lo, hi;
  computeRange(spec, stats, lo, hi);

  // 標題列左邊：名稱
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setTextPadding(0);
  tft.drawString(spec.label, CHART_X, panelY + 5, FONT_SMALL);

  // 標題列中間：一般圖表顯示可見範圍的最小～最大值，ping 顯示遺失率
  String caption = " ";
  if (spec.negativeIsLoss) {
    int total = stats.valid + stats.lost;
    if (total > 0) caption = String("loss ") + (stats.lost * 100 / total) + "%";
  } else if (stats.valid > 0) {
    caption = formatNumber(stats.mn, spec.decimals) + " .. " + formatNumber(stats.mx, spec.decimals);
  }
  tft.setTextPadding(CAPTION_PAD_W);
  tft.drawString(caption, CAPTION_X, panelY + 5, FONT_SMALL);

  // 標題列右邊：目前數值，依門檻上色
  const float latest = h.latest();
  String valueText;
  uint16_t valueCol;
  if (isnan(latest)) {
    valueText = "--";
    valueCol = COLOR_LABEL;
  } else if (spec.negativeIsLoss && latest < 0) {
    valueText = "LOST";
    valueCol = COLOR_BAD;
  } else {
    valueText = formatNumber(latest, spec.decimals) + " " + spec.unit;
    valueCol = valueColor(spec, latest);
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(valueCol, COLOR_BG);
  tft.setTextPadding(VALUE_PAD_W);
  tft.drawString(valueText, VALUE_RIGHT_X, panelY, FONT_LARGE);

  // 折線
  const int chartY = panelY + HEADER_H + 1;
  if (chartSpriteReady) {
    drawChartBody(chartSprite, 0, 0, spec, h, lo, hi);
    chartSprite.pushSprite(CHART_X, chartY);
  } else {
    drawChartBody(tft, CHART_X, chartY, spec, h, lo, hi);  // 記憶體不足時直接畫在螢幕上，會閃爍
  }

  // 右側刻度：上限與下限
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setTextPadding(320 - AXIS_X);
  tft.drawString(formatNumber(hi, spec.decimals), AXIS_X, chartY, 1);
  tft.drawString(formatNumber(lo, spec.decimals), AXIS_X, chartY + CHART_H - 8, 1);
  tft.setTextPadding(0);
}

// ---- 警報畫面（EVA 風格）----
//
// 黑底，紅橘六邊形蜂巢像水波一樣明暗起伏；中央一塊黑底面板，上方是代碼與優先度，
// 中間是長体漢字，下面是加寬字距的英文與一行說明，邊框閃爍，偶爾故障抖動。
// 整張 320x170 先畫在 8 位元畫布（約 54KB）再推到螢幕；畫布只在警報期間存在，結束就釋放。

constexpr uint16_t EVA_RED = 0xF800;
constexpr uint16_t EVA_ORANGE = 0xFC60;
constexpr uint16_t EVA_YELLOW = 0xFFE0;
constexpr uint16_t EVA_DARKRED = 0x8000;
constexpr uint16_t EVA_DIM = 0x3000;

constexpr int ALERT_PX = 50;   // 中央面板位置與大小
constexpr int ALERT_PY = 12;
constexpr int ALERT_PW = 220;
constexpr int ALERT_PH = 146;

// 平頂六邊形：外接圓半徑 14、半高 12，欄距 21、列距 24，奇數欄往下錯半格剛好密鋪
constexpr int HEX_R = 14;
constexpr int HEX_HALF_H = 12;
constexpr int HEX_COL_W = 21;
constexpr int HEX_ROW_H = 24;
const int8_t HEX_VX[6] = {14, 7, -7, -14, -7, 7};
const int8_t HEX_VY[6] = {0, 12, 12, 0, -12, -12};

TFT_eSprite alertSprite(&tft);
bool alertSpriteReady = false;

struct AlertStyle {
  uint8_t pair;          // 漢字組：0 警告、1 緊急、2 異常、3 危険（順序同 tools/gen_alert_glyphs.py）
  const char *word;      // 英文
  const char *code;      // 代碼
  const char *priority;  // 優先度
  uint16_t main;         // 主色
  uint16_t hot;          // 亮色
};

AlertStyle alertStyle(AlertKind kind) {
  switch (kind) {
    case AlertKind::NetworkLost: return {1, "EMERGENCY", "601", "AAA", EVA_RED, EVA_ORANGE};
    case AlertKind::MemoryLow: return {3, "DANGER", "335", "AA", EVA_RED, EVA_ORANGE};
    case AlertKind::PingLoss: return {2, "ANOMALY", "473", "A", EVA_ORANGE, EVA_YELLOW};
    default: return {0, "WARNING", "107", "B", EVA_ORANGE, EVA_YELLOW};
  }
}

// 蜂巢背景：每個六邊形依位置與時間算一個亮度，形成斜向掃過的波紋
template <class G>
void drawHexField(G &g, const AlertStyle &st, uint32_t now) {
  const float t = now * 0.0065f;  // 波紋週期約 1 秒
  const int cols = UI_W / HEX_COL_W + 2;
  for (int c = -1; c < cols; c++) {
    const int cx = c * HEX_COL_W;
    const int rowShift = (c & 1) ? HEX_HALF_H : 0;
    for (int cy = rowShift - HEX_ROW_H; cy < UI_H + HEX_ROW_H; cy += HEX_ROW_H) {
      // 完全被中央面板蓋住的不用畫
      if (cx - HEX_R >= ALERT_PX && cx + HEX_R <= ALERT_PX + ALERT_PW && cy - HEX_HALF_H >= ALERT_PY &&
          cy + HEX_HALF_H <= ALERT_PY + ALERT_PH) {
        continue;
      }
      float b = (sinf(t - cx * 0.035f - cy * 0.05f) + 1.0f) * 0.5f;
      uint16_t color = b > 0.82f ? st.hot : (b > 0.5f ? st.main : (b > 0.25f ? EVA_DARKRED : EVA_DIM));
      for (int k = 0; k < 6; k++) {
        int n = (k + 1) % 6;
        g.drawLine(cx + HEX_VX[k], cy + HEX_VY[k], cx + HEX_VX[n], cy + HEX_VY[n], color);
      }
    }
  }
}

// 畫一個畫格。G 可以是記憶體畫布或螢幕本身（畫布配不到記憶體時的退路，會閃爍）
template <class G>
void drawAlertFrame(G &g, const AlertView &view, uint32_t now) {
  const AlertStyle st = alertStyle(view.kind);
  const bool flash = (now / 250) & 1;  // 邊框每 0.25 秒換一次顏色

  g.fillRect(0, 0, UI_W, UI_H, TFT_BLACK);
  drawHexField(g, st, now);

  // 中央面板與閃爍邊框，四角各伸出一小段
  const uint16_t border = flash ? st.hot : st.main;
  g.fillRect(ALERT_PX, ALERT_PY, ALERT_PW, ALERT_PH, TFT_BLACK);
  g.drawRect(ALERT_PX, ALERT_PY, ALERT_PW, ALERT_PH, border);
  g.drawRect(ALERT_PX + 1, ALERT_PY + 1, ALERT_PW - 2, ALERT_PH - 2, border);
  g.drawFastHLine(ALERT_PX - 8, ALERT_PY, 8, st.hot);
  g.drawFastVLine(ALERT_PX, ALERT_PY - 8, 8, st.hot);
  g.drawFastHLine(ALERT_PX + ALERT_PW, ALERT_PY, 8, st.hot);
  g.drawFastVLine(ALERT_PX + ALERT_PW - 1, ALERT_PY - 8, 8, st.hot);
  g.drawFastHLine(ALERT_PX - 8, ALERT_PY + ALERT_PH - 1, 8, st.hot);
  g.drawFastVLine(ALERT_PX, ALERT_PY + ALERT_PH, 8, st.hot);
  g.drawFastHLine(ALERT_PX + ALERT_PW, ALERT_PY + ALERT_PH - 1, 8, st.hot);
  g.drawFastVLine(ALERT_PX + ALERT_PW - 1, ALERT_PY + ALERT_PH, 8, st.hot);

  // 面板內由上往下掃的暗線，墊在文字後面
  const int scanY = ALERT_PY + 2 + (now / 7) % (ALERT_PH - 4);
  g.drawFastHLine(ALERT_PX + 2, scanY, ALERT_PW - 4, EVA_DIM);
  g.drawFastHLine(ALERT_PX + 2, scanY + 1, ALERT_PW - 4, EVA_DIM);

  // 偶爾故障：每隔一陣子有一格畫面左右抖動，漢字再疊一個暗紅殘影
  const uint32_t slot = now / 90;
  const bool glitch = (slot % 17) == 0 || (slot % 29) == 3;
  const int shake = glitch ? (int)((slot * 7) % 9) - 4 : 0;

  // 頂端一行：代碼與優先度
  g.setTextColor(st.main);
  g.setTextDatum(TL_DATUM);
  g.drawString(String("CODE:") + st.code, ALERT_PX + 8, ALERT_PY + 6, FONT_SMALL);
  g.setTextDatum(TR_DATUM);
  g.drawString(String("PRIORITY:") + st.priority, ALERT_PX + ALERT_PW - 8, ALERT_PY + 6, FONT_SMALL);

  // 中央大漢字，上下各一條分隔線；每 1.2 秒熄滅 0.1 秒，像接觸不良的燈
  const int kanjiY = ALERT_PY + 28;
  const int pairX = (UI_W - (2 * GLYPH_W + 10)) / 2 + shake;
  g.drawFastHLine(ALERT_PX + 8, kanjiY - 4, ALERT_PW - 16, st.main);
  g.drawFastHLine(ALERT_PX + 8, kanjiY + GLYPH_H + 3, ALERT_PW - 16, st.main);
  if (((now / 100) % 12) != 0) {
    const uint8_t *left = ALERT_GLYPHS[st.pair * 2];
    const uint8_t *right = ALERT_GLYPHS[st.pair * 2 + 1];
    if (glitch) {
      g.drawBitmap(pairX + 4, kanjiY + 2, left, GLYPH_W, GLYPH_H, EVA_DARKRED);
      g.drawBitmap(pairX + GLYPH_W + 14, kanjiY + 2, right, GLYPH_W, GLYPH_H, EVA_DARKRED);
    }
    g.drawBitmap(pairX, kanjiY, left, GLYPH_W, GLYPH_H, st.main);
    g.drawBitmap(pairX + GLYPH_W + 10, kanjiY, right, GLYPH_W, GLYPH_H, st.main);
  }

  // 英文：字與字之間加寬，置中
  const String word = st.word;
  const int spacing = 5;
  int total = -spacing;
  for (size_t i = 0; i < word.length(); i++) total += g.textWidth(String(word[i]), FONT_LARGE) + spacing;
  int x = (UI_W - total) / 2 + shake;
  g.setTextColor(st.main);
  g.setTextDatum(TL_DATUM);
  for (size_t i = 0; i < word.length(); i++) {
    String ch = String(word[i]);
    g.drawString(ch, x, kanjiY + GLYPH_H + 9, FONT_LARGE);
    x += g.textWidth(ch, FONT_LARGE) + spacing;
  }

  // 一行說明（目前的數值）
  g.setTextColor(st.hot);
  g.setTextDatum(TC_DATUM);
  g.drawString(view.detail, UI_W / 2, ALERT_PY + ALERT_PH - 22, FONT_SMALL);

  // 面板下方的小提示：怎麼靜音
  g.fillRect(100, UI_H - 10, 120, 10, TFT_BLACK);
  g.setTextColor(EVA_DARKRED);
  g.setTextDatum(TC_DATUM);
  g.drawString(String("BOOT = MUTE ") + (ALERT_MUTE_MS / 1000) + "S", UI_W / 2, UI_H - 9, 1);
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

  // 圖表畫布 280x56，16 位元色約 31KB；配不到記憶體就退回直接畫在螢幕上
  chartSpriteReady = chartSprite.createSprite(CHART_W, CHART_H) != nullptr;
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
    tft.fillRect(0, 0, PORTAL_PANEL_X, UI_H, COLOR_BG);
    drawQr(qrText, QR_X, QR_Y, QR_BOX);
  }

  // 右側文字區整塊清掉重畫
  tft.fillRect(PORTAL_PANEL_X, 0, PORTAL_PANEL_W, UI_H, COLOR_BG);
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

void showWifiStatus(const WifiInfo &info, int page, int pageCount) {
  if (enterScreen(Screen::Status)) {
    status = StatusCache();
    drawPageDots(page, pageCount);
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
  drawValue(status.uptime, VALUE_X, ROW_UP_Y, FONT_SMALL, formatUptime(info.uptimeSec), COLOR_TEXT, UPTIME_W);
  drawValue(status.ota, OTA_TAG_X, ROW_UP_Y, FONT_SMALL, info.otaEnabled ? "OTA ready" : "OTA off", info.otaEnabled ? COLOR_OK : COLOR_LABEL, OTA_TAG_W);
}

void showOta(OtaPhase phase, unsigned int percent, const String &detail) {
  if (phase == OtaPhase::Start) currentScreen = Screen::None;  // 重新開始時一律整頁重畫
  if (enterScreen(Screen::Ota)) {
    lastOtaPercent = -1;
    drawCentered(14, FONT_LARGE, "UPDATING", COLOR_WARN);
    drawCentered(56, FONT_SMALL, "Do not power off", COLOR_LABEL);
    tft.drawRect(OTA_BAR_X - 1, OTA_BAR_Y - 1, tft.width() - 2 * OTA_BAR_X + 2, OTA_BAR_H + 2, COLOR_LABEL);
  }

  if (phase == OtaPhase::Failed) {
    tft.fillRect(0, 0, tft.width(), UI_H, COLOR_BG);
    drawCentered(14, FONT_LARGE, "UPDATE FAILED", COLOR_BAD);
    drawCentered(70, FONT_LARGE, detail, COLOR_TEXT);
    drawCentered(120, FONT_SMALL, "Running the old firmware", COLOR_LABEL);
    return;
  }

  if (phase == OtaPhase::Done) {
    tft.fillRect(0, 0, tft.width(), 50, COLOR_BG);
    drawCentered(14, FONT_LARGE, "UPDATE OK", COLOR_OK);
    drawCentered(56, FONT_SMALL, "Restarting...", COLOR_LABEL);
    percent = 100;
  }

  if (percent > 100) percent = 100;
  if ((int)percent == lastOtaPercent) return;
  lastOtaPercent = (int)percent;

  int barMax = tft.width() - 2 * OTA_BAR_X;
  tft.fillRect(OTA_BAR_X, OTA_BAR_Y, (int)(percent * barMax / 100), OTA_BAR_H, COLOR_OK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextPadding(90);
  tft.drawString(String(percent) + "%", tft.width() / 2, 122, FONT_LARGE);
  tft.setTextPadding(0);
}

void showAlert(const AlertView &view, uint32_t now) {
  if (currentScreen != Screen::Alert) {
    currentScreen = Screen::Alert;
    currentSub = 0;
    tft.fillScreen(TFT_BLACK);
    if (!alertSpriteReady) {
      alertSprite.setColorDepth(8);  // 8 位元色（RRRGGGBB）：紅、橘、黃都夠用，記憶體只要 16 位元的一半
      alertSpriteReady = alertSprite.createSprite(UI_W, UI_H) != nullptr;
    }
  }
  if (alertSpriteReady) {
    drawAlertFrame(alertSprite, view, now);
    alertSprite.pushSprite(0, 0);
  } else {
    drawAlertFrame(tft, view, now);  // 配不到記憶體：直接畫在螢幕上，會閃爍但功能正常
  }
}

void endAlert() {
  if (alertSpriteReady) {
    alertSprite.deleteSprite();
    alertSpriteReady = false;
  }
  currentScreen = Screen::None;  // 讓下一個畫面一定整頁重畫
}

void showChartPage(int page, int pageCount, const ChartSpec &specA, const History &histA, const ChartSpec &specB, const History &histB) {
  if (enterScreen(Screen::Chart, page)) drawPageDots(page, pageCount);
  drawChartPanel(0, specA, histA);
  drawChartPanel(PANEL_H, specB, histB);
}

}  // namespace Ui
