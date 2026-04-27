#include <Arduino.h>
#include <BleKeyboard.h>
#include <M5Unified.h>
#include <WiFi.h>

namespace {

BleKeyboard g_bleKeyboard("SJ3000 Remote", "Shadowjumper3000", 100);

enum class View : uint8_t {
  Main,
  Bluetooth,
  Power,
  PowerPoint,
  SlideDeckRemote,
};

constexpr const char* kMainItems[] = {"Bluetooth", "Power", "PowerPoint"};
constexpr const char* kBluetoothItems[] = {"Pair", "Connect", "Disconnect", "Back"};
constexpr const char* kPowerItems[] = {"Eco Mode", "Brightness", "Sleep Display", "Back"};
constexpr const char* kPowerPointItems[] = {"SlideDeck Remote", "Back"};

View g_view = View::Main;
uint8_t g_mainIndex = 0;
uint8_t g_btIndex = 0;
uint8_t g_powerIndex = 0;
uint8_t g_ppIndex = 0;

bool g_bleEnabled = false;
bool g_ecoMode = true;
uint8_t g_brightnessPreset = 1;  // 0=Low, 1=Med, 2=High
bool g_screenSleeping = false;

int g_cachedBatteryPct = -1;
uint32_t g_lastInputMs = 0;
uint32_t g_lastUiRefreshMs = 0;
uint32_t g_lastBatterySampleMs = 0;

String g_statusLine = "Ready";
uint32_t g_statusUntilMs = 0;

constexpr uint32_t kStatusDurationMs = 1800;
constexpr uint32_t kDimTimeoutMs = 12000;
constexpr uint32_t kSleepTimeoutMs = 30000;
constexpr uint8_t kDimBrightness = 6;
constexpr uint8_t kEcoBrightness[] = {22, 34, 52};
constexpr uint8_t kNormalBrightness[] = {40, 72, 110};

void setStatus(const String& text, uint32_t durationMs = kStatusDurationMs) {
  g_statusLine = text;
  g_statusUntilMs = millis() + durationMs;
}

const char* currentStatusText() {
  if (millis() < g_statusUntilMs) {
    return g_statusLine.c_str();
  }

  if (g_view == View::SlideDeckRemote) {
    if (!g_bleEnabled) {
      return "Remote: enable Bluetooth";
    }
    return g_bleKeyboard.isConnected() ? "Remote: connected" : "Remote: waiting host";
  }

  if (g_bleEnabled) {
    return g_bleKeyboard.isConnected() ? "Bluetooth connected" : "Bluetooth advertising";
  }

  return "Bluetooth off";
}

const char* brightnessName() {
  switch (g_brightnessPreset) {
    case 0:
      return "Low";
    case 1:
      return "Medium";
    default:
      return "High";
  }
}

uint8_t selectedBrightness() {
  const uint8_t* table = g_ecoMode ? kEcoBrightness : kNormalBrightness;
  return table[g_brightnessPreset];
}

uint8_t currentMenuIndex() {
  switch (g_view) {
    case View::Main:
      return g_mainIndex;
    case View::Bluetooth:
      return g_btIndex;
    case View::Power:
      return g_powerIndex;
    case View::PowerPoint:
      return g_ppIndex;
    case View::SlideDeckRemote:
    default:
      return 0;
  }
}

void setCurrentMenuIndex(uint8_t index) {
  switch (g_view) {
    case View::Main:
      g_mainIndex = index;
      return;
    case View::Bluetooth:
      g_btIndex = index;
      return;
    case View::Power:
      g_powerIndex = index;
      return;
    case View::PowerPoint:
      g_ppIndex = index;
      return;
    case View::SlideDeckRemote:
    default:
      return;
  }
}

size_t menuLengthForView(View view) {
  switch (view) {
    case View::Main:
      return sizeof(kMainItems) / sizeof(kMainItems[0]);
    case View::Bluetooth:
      return sizeof(kBluetoothItems) / sizeof(kBluetoothItems[0]);
    case View::Power:
      return sizeof(kPowerItems) / sizeof(kPowerItems[0]);
    case View::PowerPoint:
      return sizeof(kPowerPointItems) / sizeof(kPowerPointItems[0]);
    case View::SlideDeckRemote:
    default:
      return 0;
  }
}

const char* const* menuItemsForView(View view) {
  switch (view) {
    case View::Main:
      return kMainItems;
    case View::Bluetooth:
      return kBluetoothItems;
    case View::Power:
      return kPowerItems;
    case View::PowerPoint:
      return kPowerPointItems;
    case View::SlideDeckRemote:
    default:
      return nullptr;
  }
}

const char* menuTitleForView(View view) {
  switch (view) {
    case View::Main:
      return "Main Menu";
    case View::Bluetooth:
      return "Bluetooth";
    case View::Power:
      return "Power";
    case View::PowerPoint:
      return "PowerPoint";
    case View::SlideDeckRemote:
      return "SlideDeck Remote";
    default:
      return "Menu";
  }
}

void updateBatteryCache() {
  const uint32_t now = millis();
  if ((now - g_lastBatterySampleMs) < 2000) {
    return;
  }

  g_lastBatterySampleMs = now;
  const int battery = M5.Power.getBatteryLevel();
  if (battery != g_cachedBatteryPct) {
    g_cachedBatteryPct = battery;
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
  if (g_screenSleeping) {
    return;
  }

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

void drawBatteryAndStatus(int yTop) {
  const int width = M5.Display.width();

  M5.Display.drawFastHLine(0, yTop - 2, width, TFT_DARKGREY);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.setCursor(4, yTop);

  if (g_cachedBatteryPct >= 0) {
    M5.Display.printf("Bat:%d%% ", g_cachedBatteryPct);
  } else {
    M5.Display.print("Bat:n/a ");
  }

  if (g_bleEnabled) {
    M5.Display.print(g_bleKeyboard.isConnected() ? "BT:ON" : "BT:WAIT");
  } else {
    M5.Display.print("BT:OFF");
  }

  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(4, yTop + 10);
  M5.Display.printf("%-36s", currentStatusText());
}

String menuItemLabel(size_t index) {
  const char* const* items = menuItemsForView(g_view);
  String label = items[index];

  if (g_view == View::Power) {
    if (index == 0) {
      label += g_ecoMode ? " [ON]" : " [OFF]";
    } else if (index == 1) {
      label += " [";
      label += brightnessName();
      label += "]";
    }
  }

  return label;
}

void drawMenuScreen() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const size_t count = menuLengthForView(g_view);
  const uint8_t selected = currentMenuIndex();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print(menuTitleForView(g_view));
  M5.Display.drawFastHLine(0, 24, width, TFT_DARKGREY);

  M5.Display.setTextSize(1);
  for (size_t i = 0; i < count; ++i) {
    const int y = 30 + static_cast<int>(i) * 20;
    const bool active = (i == selected);
    const uint16_t bg = active ? TFT_DARKGREEN : TFT_BLACK;
    const uint16_t fg = active ? TFT_WHITE : TFT_LIGHTGREY;

    M5.Display.fillRoundRect(2, y - 2, width - 4, 18, 3, bg);
    M5.Display.setTextColor(fg, bg);
    M5.Display.setCursor(8, y + 2);
    M5.Display.print(active ? "> " : "  ");
    M5.Display.print(menuItemLabel(i));
  }

  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setCursor(4, height - 34);
  M5.Display.print("A:Select B:Next PWR:Back");

  drawBatteryAndStatus(height - 22);
}

void drawRemoteScreen() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("SlideDeck");
  M5.Display.drawFastHLine(0, 24, width, TFT_DARKGREY);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(8, 34);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.print("A : Next slide");

  M5.Display.setCursor(8, 50);
  M5.Display.print("B : Previous slide");

  M5.Display.setCursor(8, 66);
  M5.Display.print("PWR : Exit remote");

  M5.Display.setCursor(8, 84);
  if (g_bleEnabled && g_bleKeyboard.isConnected()) {
    M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    M5.Display.print("Host connected");
  } else {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.print("Waiting for host BT link");
  }

  drawBatteryAndStatus(height - 22);
}

void renderUi(bool force = false) {
  const uint32_t now = millis();
  if (!force && (now - g_lastUiRefreshMs) < 150) {
    return;
  }

  g_lastUiRefreshMs = now;
  updateBatteryCache();

  if (g_screenSleeping) {
    return;
  }

  if (g_view == View::SlideDeckRemote) {
    drawRemoteScreen();
    return;
  }

  drawMenuScreen();
}

void enableBluetooth() {
  if (!g_bleEnabled) {
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

void goBackOneLevel() {
  if (g_view == View::Main) {
    setStatus("Main menu");
    renderUi(true);
    return;
  }

  if (g_view == View::SlideDeckRemote) {
    g_view = View::PowerPoint;
    setStatus("PowerPoint menu");
    renderUi(true);
    return;
  }

  g_view = View::Main;
  setStatus("Main menu");
  renderUi(true);
}

void advanceSelection() {
  const size_t len = menuLengthForView(g_view);
  if (len == 0) {
    return;
  }

  const uint8_t next = static_cast<uint8_t>((currentMenuIndex() + 1) % len);
  setCurrentMenuIndex(next);
  renderUi(true);
}

void sendSlideCommand(uint8_t keyCode, const char* successText) {
  if (!g_bleEnabled) {
    setStatus("Enable Bluetooth first");
    return;
  }

  if (!g_bleKeyboard.isConnected()) {
    setStatus("No host connection");
    return;
  }

  g_bleKeyboard.write(keyCode);
  setStatus(successText, 800);
}

void handleBluetoothAction(uint8_t index) {
  switch (index) {
    case 0:
      enableBluetooth();
      setStatus("Pair mode enabled");
      break;
    case 1:
      enableBluetooth();
      if (g_bleKeyboard.isConnected()) {
        setStatus("Host connected");
      } else {
        setStatus("Waiting host connection");
      }
      break;
    case 2:
      disableBluetooth();
      break;
    default:
      goBackOneLevel();
      break;
  }
}

void handlePowerAction(uint8_t index) {
  switch (index) {
    case 0:
      g_ecoMode = !g_ecoMode;
      setStatus(g_ecoMode ? "Eco mode ON" : "Eco mode OFF");
      M5.Display.setBrightness(selectedBrightness());
      break;
    case 1:
      g_brightnessPreset = static_cast<uint8_t>((g_brightnessPreset + 1) % 3);
      setStatus(String("Brightness: ") + brightnessName());
      M5.Display.setBrightness(selectedBrightness());
      break;
    case 2:
      M5.Display.sleep();
      g_screenSleeping = true;
      setStatus("Display sleeping");
      break;
    default:
      goBackOneLevel();
      break;
  }
}

void handlePowerPointAction(uint8_t index) {
  if (index == 0) {
    enableBluetooth();
    g_view = View::SlideDeckRemote;
    setStatus("SlideDeck remote active");
    return;
  }

  goBackOneLevel();
}

void activateCurrentItem() {
  const uint8_t index = currentMenuIndex();

  switch (g_view) {
    case View::Main:
      if (index == 0) {
        g_view = View::Bluetooth;
      } else if (index == 1) {
        g_view = View::Power;
      } else {
        g_view = View::PowerPoint;
      }
      setStatus(menuTitleForView(g_view));
      break;
    case View::Bluetooth:
      handleBluetoothAction(index);
      break;
    case View::Power:
      handlePowerAction(index);
      break;
    case View::PowerPoint:
      handlePowerPointAction(index);
      break;
    case View::SlideDeckRemote:
    default:
      break;
  }

  renderUi(true);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);

  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);
  setCpuFrequencyMhz(80);

  g_lastInputMs = millis();
  M5.Display.setBrightness(selectedBrightness());
  updateBatteryCache();
  setStatus("Ready");
  renderUi(true);
}

void loop() {
  M5.update();

  const bool btnA = M5.BtnA.wasClicked();
  const bool btnB = M5.BtnB.wasClicked();
  const bool btnPwr = M5.BtnPWR.wasClicked();

  if (btnA || btnB || btnPwr) {
    registerInteraction();
    if (g_screenSleeping) {
      renderUi(true);
      M5.delay(12);
      return;
    }
  }

  if (g_view == View::SlideDeckRemote) {
    if (btnA) {
      sendSlideCommand(KEY_PAGE_DOWN, "Next slide");
      renderUi(true);
    }
    if (btnB) {
      sendSlideCommand(KEY_PAGE_UP, "Previous slide");
      renderUi(true);
    }
    if (btnPwr) {
      goBackOneLevel();
    }
  } else {
    if (btnA) {
      activateCurrentItem();
    }
    if (btnB) {
      advanceSelection();
    }
    if (btnPwr) {
      goBackOneLevel();
    }
  }

  applyIdlePowerPolicy();
  renderUi(false);
  M5.delay(12);
}