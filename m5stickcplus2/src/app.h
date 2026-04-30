#pragma once

#include <Arduino.h>
#include <BleKeyboard.h>
#include <M5Unified.h>

namespace sj3000 {

enum class View : uint8_t {
  Main,
  Bluetooth,
  Power,
  PowerPoint,
  Music,
  SlideDeckRemote,
  MusicRemote,
};

extern BleKeyboard g_bleKeyboard;
extern View g_view;
extern uint8_t g_mainIndex;
extern uint8_t g_btIndex;
extern uint8_t g_powerIndex;
extern uint8_t g_ppIndex;
extern uint8_t g_musicIndex;
extern bool g_bleEnabled;
extern bool g_ecoMode;
extern uint8_t g_brightnessPreset;
extern bool g_screenSleeping;
extern int g_cachedBatteryPct;
extern uint32_t g_lastInputMs;
extern bool g_mainUiDirty;
extern bool g_submenuUiDirty;
extern String g_statusLine;
extern uint32_t g_statusUntilMs;

constexpr uint32_t kStatusDurationMs = 1800;
constexpr uint32_t kDimTimeoutMs = 12000;
constexpr uint32_t kSleepTimeoutMs = 30000;
constexpr uint8_t kDimBrightness = 6;

void setStatus(const String& text, uint32_t durationMs = kStatusDurationMs);
const char* currentStatusText();
const char* brightnessName();
uint8_t selectedBrightness();
uint8_t currentMenuIndex();

void updateBatteryCache();
void wakeDisplay();
void registerInteraction();
void applyIdlePowerPolicy();

void enableBluetooth();
void disableBluetooth();

size_t menuLengthForView(View view);
const char* const* menuItemsForView(View view);
const char* menuTitleForView(View view);
bool submenuActionIsReturn(View view, uint8_t index);

void sendSlideCommand(uint8_t keyCode, const char* successText);
void sendMediaCommand(const MediaKeyReport& key, const char* successText);

void goBackOneLevel();
void advanceSelection();
void activateCurrentItem();
void handleMusicRemoteInput(bool btnA, bool btnB, bool btnPwr);
void processInput(bool btnA, bool btnB, bool btnPwr);

void renderUi(bool force = false);

}  // namespace sj3000
