#include "app.h"

#include <NimBLEDevice.h>

namespace sj3000 {

BleKeyboard g_bleKeyboard("SJ3000 Remote", "Shadowjumper3000", 100);

View g_view = View::Main;
uint8_t g_mainIndex = 0;
uint8_t g_btIndex = 0;
uint8_t g_powerIndex = 0;
uint8_t g_ppIndex = 0;
uint8_t g_musicIndex = 0;

bool g_bleEnabled = false;
bool g_ecoMode = true;
uint8_t g_brightnessPreset = 1;
bool g_screenSleeping = false;

int g_cachedBatteryPct = -1;
uint32_t g_lastInputMs = 0;
bool g_mainUiDirty = true;
bool g_submenuUiDirty = true;

String g_statusLine = "Ready";
uint32_t g_statusUntilMs = 0;

namespace {

constexpr const char* kMainItems[] = {"Bluetooth", "Power", "PowerPoint", "Music"};
constexpr const char* kBluetoothItems[] = {"Advertise", "Disconnect", "Return"};
constexpr const char* kPowerItems[] = {"Eco Mode", "Brightness", "Sleep Display", "Return"};
constexpr const char* kPowerPointItems[] = {"SlideDeck Remote", "Return"};
constexpr const char* kMusicItems[] = {"Music Remote", "Return"};

constexpr uint8_t kEcoBrightness[] = {22, 34, 52};
constexpr uint8_t kNormalBrightness[] = {40, 72, 110};

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
    case View::SlideDeckRemote:
    case View::MusicRemote:
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

size_t menuLengthForView(View view) {
  switch (view) {
    case View::Main: return sizeof(kMainItems) / sizeof(kMainItems[0]);
    case View::Bluetooth: return sizeof(kBluetoothItems) / sizeof(kBluetoothItems[0]);
    case View::Power: return sizeof(kPowerItems) / sizeof(kPowerItems[0]);
    case View::PowerPoint: return sizeof(kPowerPointItems) / sizeof(kPowerPointItems[0]);
    case View::Music: return sizeof(kMusicItems) / sizeof(kMusicItems[0]);
    case View::SlideDeckRemote:
    case View::MusicRemote:
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
    case View::SlideDeckRemote:
    case View::MusicRemote:
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
    case View::SlideDeckRemote: return "SlideDeck Remote";
    case View::MusicRemote: return "Music Remote";
    default: return "Menu";
  }
}

bool submenuActionIsReturn(View view, uint8_t index) {
  const size_t len = menuLengthForView(view);
  return len > 0 && index == (len - 1);
}

}  // namespace sj3000
