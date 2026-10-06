#include "monitor.h"

#include <WiFi.h>

#include "config.h"
#include "ping_monitor.h"

namespace {

History rssiHistory;
History pingHistory;
History heapHistory;
History tempHistory;

uint32_t lastSampleMs = 0;
bool sampledOnce = false;

}  // namespace

namespace Monitor {

bool tick(uint32_t now) {
  if (sampledOnce && now - lastSampleMs < SAMPLE_INTERVAL_MS) return false;
  sampledOnce = true;
  lastSampleMs = now;

  bool connected = WiFi.status() == WL_CONNECTED;
  PingMonitor::update(connected, WiFi.gatewayIP());

  rssiHistory.push(connected ? (float)WiFi.RSSI() : NAN);
  pingHistory.push(connected ? PingMonitor::takeSample() : NAN);
  heapHistory.push(ESP.getFreeHeap() / 1024.0f);
  tempHistory.push(temperatureRead());
  return true;
}

const History &rssi() { return rssiHistory; }
const History &ping() { return pingHistory; }
const History &heapKb() { return heapHistory; }
const History &chipTemp() { return tempHistory; }

}  // namespace Monitor
