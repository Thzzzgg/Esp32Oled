#include "wifi_portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <vector>

namespace {

constexpr uint8_t AP_CHANNEL = 1;
constexpr uint8_t AP_MAX_CLIENTS = 2;
constexpr size_t MAX_SCAN_ENTRIES = 20;

struct ScanEntry {
  String ssid;
  int rssi;
};

WebServer server(80);
DNSServer dns;
bool handlersRegistered = false;

String ssidText;
String passText;
std::vector<ScanEntry> scanList;

bool hasSubmitted = false;
String submittedSsid;
String submittedPass;

WifiPortal::Result resultState = WifiPortal::Result::Idle;
String resultIp;

const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Wi-Fi 設定</title>
<style>
body{font-family:system-ui,sans-serif;max-width:420px;margin:0 auto;padding:16px;background:#111;color:#eee}
h1{font-size:20px}
label{display:block;margin:14px 0 4px;font-size:14px;color:#aaa}
input,button{width:100%;box-sizing:border-box;padding:12px;font-size:16px;border-radius:8px;border:1px solid #444;background:#222;color:#eee}
button{background:#1e88e5;border:0;margin-top:18px}
a{color:#64b5f6}
</style></head><body><h1>ESP32 Wi-Fi 設定</h1>
)HTML";

const char PAGE_TAIL[] PROGMEM = "</body></html>";

// 網頁表單送出後的畫面：每秒查一次 /status，顯示連線結果
const char PAGE_PROGRESS[] PROGMEM = R"HTML(
<p id="s">連線中，請稍候…</p>
<script>
async function poll(){
  try{
    const r=await fetch('/status');const j=await r.json();
    if(j.state==='ok'){
      s.textContent='已連線！裝置 IP：'+j.ip+'。請把手機切回原本的 Wi-Fi。';return;
    }
    if(j.state==='fail'){
      s.innerHTML='連線失敗，請確認密碼。<a href="/">返回重試</a>';return;
    }
  }catch(e){}
  setTimeout(poll,1000);
}
poll();
</script>
)HTML";

// 避免 SSID 內的特殊字元破壞 HTML
String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c;
    }
  }
  return out;
}

// 掃描附近 Wi-Fi，依 SSID 去重（同名保留訊號最強的一筆）
void scanNetworks() {
  scanList.clear();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;  // 隱藏網路
    int rssi = WiFi.RSSI(i);
    bool duplicated = false;
    for (auto &e : scanList) {
      if (e.ssid == ssid) {
        if (rssi > e.rssi) e.rssi = rssi;
        duplicated = true;
        break;
      }
    }
    if (!duplicated && scanList.size() < MAX_SCAN_ENTRIES) scanList.push_back({ssid, rssi});
  }
  WiFi.scanDelete();
}

String randomDigits(size_t count) {
  String s;
  for (size_t i = 0; i < count; i++) s += char('0' + esp_random() % 10);
  return s;
}

void sendPage(const String &body) {
  server.sendHeader("Cache-Control", "no-store");
  String html = FPSTR(PAGE_HEAD);
  html += body;
  html += FPSTR(PAGE_TAIL);
  server.send(200, "text/html", html);
}

void redirectToRoot() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
}

void handleRoot() {
  String body;
  body += F("<form method=\"POST\" action=\"/save\">");
  body += F("<label>Wi-Fi 名稱（SSID）</label>");
  body += F("<input name=\"ssid\" list=\"aps\" maxlength=\"32\" required autocomplete=\"off\" autocapitalize=\"none\">");
  body += F("<datalist id=\"aps\">");
  for (const auto &e : scanList) {
    body += F("<option value=\"");
    body += htmlEscape(e.ssid);
    body += F("\">");
    body += e.rssi;
    body += F(" dBm</option>");
  }
  body += F("</datalist>");
  body += F("<label>密碼（開放網路請留空）</label>");
  body += F("<input name=\"pass\" type=\"password\" maxlength=\"63\" autocomplete=\"off\">");
  body += F("<button type=\"submit\">連線</button></form>");
  body += F("<p><a href=\"/rescan\">重新掃描</a>（掃描時熱點可能短暫斷線，請重新整理頁面）</p>");
  sendPage(body);
}

void handleRescan() {
  scanNetworks();
  redirectToRoot();
}

void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  // WPA2 密碼長度為 8～63；開放網路密碼留空
  if (ssid.length() < 1 || ssid.length() > 32 || (pass.length() != 0 && (pass.length() < 8 || pass.length() > 63))) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "text/html; charset=utf-8",
                F("<meta charset=\"utf-8\"><p>SSID 需為 1～32 字元，密碼需為空白或 8～63 字元。</p><p><a href=\"/\">返回</a></p>"));
    return;
  }
  submittedSsid = ssid;
  submittedPass = pass;
  hasSubmitted = true;
  resultState = WifiPortal::Result::Connecting;
  resultIp = "";
  sendPage(FPSTR(PAGE_PROGRESS));
}

void handleStatus() {
  const char *state = "idle";
  switch (resultState) {
    case WifiPortal::Result::Connecting: state = "connecting"; break;
    case WifiPortal::Result::Ok: state = "ok"; break;
    case WifiPortal::Result::Failed: state = "fail"; break;
    default: break;
  }
  String json = String("{\"state\":\"") + state + "\",\"ip\":\"" + resultIp + "\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void registerHandlers() {
  if (handlersRegistered) return;
  handlersRegistered = true;
  server.on("/", HTTP_GET, handleRoot);
  server.on("/rescan", HTTP_GET, handleRescan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  // 手機的「連網檢查」網址（例如 /generate_204）一律導回設定頁，才會自動彈出配網視窗
  server.onNotFound(redirectToRoot);
}

}  // namespace

namespace WifiPortal {

void begin() {
  hasSubmitted = false;
  resultState = Result::Idle;
  resultIp = "";

  // 熱點名稱帶 MAC 後兩碼避免多台撞名；密碼每次隨機產生，顯示在螢幕上
  uint64_t mac = ESP.getEfuseMac();
  char name[24];
  snprintf(name, sizeof(name), "ESP32-LCD-%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  ssidText = name;
  passText = randomDigits(8);

  WiFi.mode(WIFI_AP_STA);
  scanNetworks();  // 先掃描再開熱點，避免掃描時干擾熱點

  WiFi.softAP(ssidText.c_str(), passText.c_str(), AP_CHANNEL, 0, AP_MAX_CLIENTS);
  dns.start(53, "*", WiFi.softAPIP());
  registerHandlers();
  server.begin();
}

void end() {
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

void handle() {
  dns.processNextRequest();
  server.handleClient();
}

const String &apSsid() { return ssidText; }

const String &apPassword() { return passText; }

String apIp() { return WiFi.softAPIP().toString(); }

bool clientJoined() { return WiFi.softAPgetStationNum() > 0; }

bool takeSubmitted(String &ssid, String &pass) {
  if (!hasSubmitted) return false;
  hasSubmitted = false;
  ssid = submittedSsid;
  pass = submittedPass;
  return true;
}

void setResult(Result result, const String &ip) {
  resultState = result;
  resultIp = ip;
}

}  // namespace WifiPortal
