#include "app.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

namespace sj3000 {

BleKeyboard g_bleKeyboard("SJ3000 Remote", "Shadowjumper3000", 100);

View g_view = View::Main;
uint8_t g_mainIndex = 0;
uint8_t g_btIndex = 0;
uint8_t g_powerIndex = 0;
uint8_t g_ppIndex = 0;
uint8_t g_musicIndex = 0;
uint8_t g_irIndex = 0;
uint8_t g_wifiIndex = 0;

bool g_bleEnabled = false;
bool g_ecoMode = true;
uint8_t g_brightnessPreset = 1;
bool g_screenSleeping = false;
bool g_bleWasEnabledBeforeWifi = false;

int g_cachedBatteryPct = -1;
uint32_t g_lastInputMs = 0;
bool g_mainUiDirty = true;
bool g_submenuUiDirty = true;

String g_statusLine = "Ready";
uint32_t g_statusUntilMs = 0;

namespace {

constexpr const char* kMainItems[] = {"Bluetooth", "Power", "PowerPoint", "Music", "IR", "WiFi"};
constexpr const char* kBluetoothItems[] = {"Advertise", "Disconnect", "Return"};
constexpr const char* kPowerItems[] = {"Eco Mode", "Brightness", "Sleep Display", "Return"};
constexpr const char* kPowerPointItems[] = {"SlideDeck Remote", "Return"};
constexpr const char* kMusicItems[] = {"Music Remote", "Return"};
constexpr const char* kIrItems[] = {"TV Power", "Volume Up", "Volume Down", "Mute", "Return"};
constexpr const char* kWifiItems[] = {"Start Portal", "Stop Portal", "Return"};

constexpr uint8_t kEcoBrightness[] = {22, 34, 52};
constexpr uint8_t kNormalBrightness[] = {40, 72, 110};

constexpr uint32_t kIrCodePower = 0x20DF10EF;
constexpr uint32_t kIrCodeVolUp = 0x20DF40BF;
constexpr uint32_t kIrCodeVolDown = 0x20DFC03F;
constexpr uint32_t kIrCodeMute = 0x20DF906F;
constexpr uint16_t kIrBits = 32;
constexpr uint16_t kIrRepeat = 0;

constexpr uint8_t kIrLedPin = 9;

constexpr const char* kWifiSsid = "SJ3000-Info";
constexpr const char* kWifiPassword = "";
const IPAddress kWifiIp(172, 16, 0, 1);
const IPAddress kWifiGateway(172, 16, 0, 1);
const IPAddress kWifiSubnet(255, 255, 255, 0);

WebServer g_webServer(80);
bool g_wifiActive = false;

IRsend g_irSend(kIrLedPin);
bool g_irReady = false;

const char kPortalPage[] PROGMEM =
    "<!doctype html>"
    "<html lang='en'>"
    "<head>"
    "<meta charset='utf-8'/>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
    "<title>SJ3000 Info</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#0c1b2a;color:#e6f1ff;margin:0;padding:24px;}"
    ".card{max-width:480px;margin:0 auto;background:#12263a;border-radius:12px;padding:20px;}"
    "h1{margin:0 0 12px;font-size:22px;}"
    "p{line-height:1.4;margin:0 0 12px;}"
    "small{color:#90a4b5;}"
    "</style>"
    "</head>"
    "<body>"
    "<div class='card'>"
    "<h1>Welcome</h1>"
    "<p>This device is running a local information portal. Customize this page to share details, links, or instructions.</p>"
    "<p>No data is collected or stored.</p>"
    "<small>SSID: SJ3000-Info</small>"
    "</div>"
    "</body>"
    "</html>";

}  // namespace

void setStatus(const String& text, uint32_t durationMs) {
  g_statusLine = text;
  g_statusUntilMs = millis() + durationMs;
  g_mainUiDirty = true;
  g_submenuUiDirty = true;
}

const char* currentStatusText() {
  if (millis() < g_statusUntilMs) {
    return g_statusLine.c_str();
  }

  if (g_view == View::SlideDeckRemote) {
    if (!g_bleEnabled) return "Remote: Bluetooth off";
    return g_bleKeyboard.isConnected() ? "Remote: connected" : "Remote: pairing";
  }

  if (g_view == View::MusicRemote) {
    if (!g_bleEnabled) return "Music: Bluetooth off";
    return g_bleKeyboard.isConnected() ? "Music: connected" : "Music: pairing";
  }

  if (g_view == View::Wifi || g_view == View::WifiPortal) {
    return g_wifiActive ? "WiFi portal active" : "WiFi portal off";
  }

  if (!g_bleEnabled) return "Bluetooth off";
  return g_bleKeyboard.isConnected() ? "Bluetooth connected" : "Bluetooth pairing";
}

const char* brightnessName() {
  switch (g_brightnessPreset) {
    case 0: return "Low";
    case 1: return "Medium";
    default: return "High";
  }
}

uint8_t selectedBrightness() {
  const uint8_t* table = g_ecoMode ? kEcoBrightness : kNormalBrightness;
  return table[g_brightnessPreset];
}

uint8_t currentMenuIndex() {
  switch (g_view) {
    case View::Main: return g_mainIndex;
    case View::Bluetooth: return g_btIndex;
    case View::Power: return g_powerIndex;
    case View::PowerPoint: return g_ppIndex;
    case View::Music: return g_musicIndex;
    case View::IR: return g_irIndex;
    case View::Wifi: return g_wifiIndex;
    case View::SlideDeckRemote:
    case View::MusicRemote:
    case View::IRRemote:
    case View::WifiPortal:
    default: return 0;
  }
}

void updateBatteryCache() {
  const uint32_t now = millis();
  if ((now - g_lastInputMs) < 2000) return;

  const int battery = M5.Power.getBatteryLevel();
  if (battery != g_cachedBatteryPct) {
    g_cachedBatteryPct = battery;
    g_mainUiDirty = true;
    g_submenuUiDirty = true;
    if (g_bleEnabled && battery >= 0) {
      g_bleKeyboard.setBatteryLevel(static_cast<uint8_t>(constrain(battery, 0, 100)));
    }
  }
}

void wakeDisplay() {
  if (g_screenSleeping) {
    M5.Display.wakeup();
    g_screenSleeping = false;
  }
  M5.Display.setBrightness(selectedBrightness());
}

void registerInteraction() {
  g_lastInputMs = millis();
  wakeDisplay();
}

void applyIdlePowerPolicy() {
  if (g_screenSleeping) return;

  const uint32_t idleMs = millis() - g_lastInputMs;
  if (idleMs >= kSleepTimeoutMs) {
    M5.Display.sleep();
    g_screenSleeping = true;
    return;
  }

  if (idleMs >= kDimTimeoutMs) {
    M5.Display.setBrightness(kDimBrightness);
    return;
  }

  M5.Display.setBrightness(selectedBrightness());
}

void enableBluetooth() {
  if (!g_bleEnabled) {
    NimBLEDevice::setSecurityAuth(false, false, true);
    g_bleKeyboard.begin();
    g_bleEnabled = true;
  }
}

void disableBluetooth() {
  if (!g_bleEnabled) {
    setStatus("Bluetooth already off");
    return;
  }

  g_bleKeyboard.end();
  g_bleEnabled = false;
  setStatus("Bluetooth disconnected");
}

void startWifiPortal() {
  if (g_wifiActive) {
    setStatus("WiFi already active");
    return;
  }
  // If Bluetooth is enabled, remember state and disable it to avoid radio
  // coexistence issues while running the WiFi softAP. We'll re-enable when
  // the portal stops.
  if (g_bleEnabled) {
    g_bleWasEnabledBeforeWifi = true;
    disableBluetooth();
  } else {
    g_bleWasEnabledBeforeWifi = false;
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kWifiIp, kWifiGateway, kWifiSubnet);
  WiFi.softAP(kWifiSsid, kWifiPassword);

  g_webServer.on("/", HTTP_GET, []() {
    g_webServer.send(200, "text/html", kPortalPage);
  });
  g_webServer.onNotFound([]() {
    g_webServer.send(200, "text/html", kPortalPage);
  });
  g_webServer.begin();

  g_wifiActive = true;
  setStatus("WiFi portal active");
}

void stopWifiPortal() {
  if (!g_wifiActive) {
    setStatus("WiFi already off");
    return;
  }

  g_webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  g_wifiActive = false;
  setStatus("WiFi portal stopped");

  // Restore BLE state if it was enabled before starting the WiFi portal.
  if (g_bleWasEnabledBeforeWifi) {
    enableBluetooth();
  }
}

bool wifiPortalActive() {
  return g_wifiActive;
}

void handleWifiClient() {
  if (g_wifiActive) g_webServer.handleClient();
}

void sendIrCode(uint32_t code, uint16_t bits, uint16_t repeat, const char* label) {
  if (!g_irReady) {
    g_irSend.begin();
    g_irReady = true;
  }
  g_irSend.sendNEC(code, bits, repeat);
  setStatus(label, 900);
}

size_t menuLengthForView(View view) {
  switch (view) {
    case View::Main: return sizeof(kMainItems) / sizeof(kMainItems[0]);
    case View::Bluetooth: return sizeof(kBluetoothItems) / sizeof(kBluetoothItems[0]);
    case View::Power: return sizeof(kPowerItems) / sizeof(kPowerItems[0]);
    case View::PowerPoint: return sizeof(kPowerPointItems) / sizeof(kPowerPointItems[0]);
    case View::Music: return sizeof(kMusicItems) / sizeof(kMusicItems[0]);
    case View::IR: return sizeof(kIrItems) / sizeof(kIrItems[0]);
    case View::Wifi: return sizeof(kWifiItems) / sizeof(kWifiItems[0]);
    case View::SlideDeckRemote:
    case View::MusicRemote:
    case View::IRRemote:
    case View::WifiPortal:
    default: return 0;
  }
}

const char* const* menuItemsForView(View view) {
  switch (view) {
    case View::Main: return kMainItems;
    case View::Bluetooth: return kBluetoothItems;
    case View::Power: return kPowerItems;
    case View::PowerPoint: return kPowerPointItems;
    case View::Music: return kMusicItems;
    case View::IR: return kIrItems;
    case View::Wifi: return kWifiItems;
    case View::SlideDeckRemote:
    case View::MusicRemote:
    case View::IRRemote:
    case View::WifiPortal:
    default: return nullptr;
  }
}

const char* menuTitleForView(View view) {
  switch (view) {
    case View::Main: return "Main Menu";
    case View::Bluetooth: return "Bluetooth";
    case View::Power: return "Power";
    case View::PowerPoint: return "PowerPoint";
    case View::Music: return "Music";
    case View::IR: return "IR";
    case View::Wifi: return "WiFi";
    case View::SlideDeckRemote: return "SlideDeck Remote";
    case View::MusicRemote: return "Music Remote";
    case View::IRRemote: return "IR Remote";
    case View::WifiPortal: return "WiFi Portal";
    default: return "Menu";
  }
}

bool submenuActionIsReturn(View view, uint8_t index) {
  const size_t len = menuLengthForView(view);
  return len > 0 && index == (len - 1);
}

void handleIrRemoteInput(bool btnA, bool btnB, bool btnPwr) {
  if (btnA) {
    sendIrCode(kIrCodePower, kIrBits, kIrRepeat, "TV Power sent");
  }
  if (btnB) {
    sendIrCode(kIrCodeVolUp, kIrBits, kIrRepeat, "Volume Up sent");
  }
  if (btnPwr) {
    goBackOneLevel();
  }
}

}  // namespace sj3000
