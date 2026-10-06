#include "wifi_store.h"

#include <Preferences.h>

namespace {
constexpr char NAMESPACE[] = "wifi";
constexpr char KEY_SSID[] = "ssid";
constexpr char KEY_PASS[] = "pass";
}  // namespace

namespace WifiStore {

bool load(String &ssid, String &pass) {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);  // 唯讀
  ssid = prefs.getString(KEY_SSID, "");
  pass = prefs.getString(KEY_PASS, "");
  prefs.end();
  return ssid.length() > 0;
}

void save(const String &ssid, const String &pass) {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putString(KEY_SSID, ssid);
  prefs.putString(KEY_PASS, pass);
  prefs.end();
}

void clear() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

}  // namespace WifiStore
