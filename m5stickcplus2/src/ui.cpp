#include "app.h"

namespace sj3000 {

namespace {

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

  M5.Display.print(g_bleEnabled ? (g_bleKeyboard.isConnected() ? "BT:ON" : "BT:PAIR") : "BT:OFF");

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
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print(menuTitleForView(g_view));
  M5.Display.drawFastHLine(0, 16, width, TFT_DARKGREY);

  for (size_t i = 0; i < count; ++i) {
    const int y = 22 + static_cast<int>(i) * 16;
    const bool active = (i == selected);
    const uint16_t bg = active ? TFT_DARKGREEN : TFT_BLACK;
    const uint16_t fg = active ? TFT_WHITE : TFT_LIGHTGREY;

    M5.Display.fillRoundRect(2, y - 1, width - 4, 14, 3, bg);
    M5.Display.setTextColor(fg, bg);
    M5.Display.setCursor(6, y + 1);
    M5.Display.print(active ? "> " : "  ");
    M5.Display.print(menuItemLabel(i));
  }

  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setCursor(4, height - 20);
  M5.Display.print("A:Select B:Next PWR:Back");

  drawBatteryAndStatus(height - 10);
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
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 34);
  M5.Display.print("A : Next slide");
  M5.Display.setCursor(8, 50);
  M5.Display.print("B : Previous slide");
  M5.Display.setCursor(8, 66);
  M5.Display.print("PWR : Exit remote");
  M5.Display.setCursor(8, 84);
  M5.Display.print(g_bleEnabled && g_bleKeyboard.isConnected() ? "Connected" : "Waiting for host");

  drawBatteryAndStatus(height - 22);
}

void drawIrRemoteScreen() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("IR Remote");
  M5.Display.drawFastHLine(0, 24, width, TFT_DARKGREY);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 34);
  M5.Display.print("A : Power");
  M5.Display.setCursor(8, 50);
  M5.Display.print("B : Volume Up");
  M5.Display.setCursor(8, 66);
  M5.Display.print("PWR : Exit remote");

  drawBatteryAndStatus(height - 10);
}

void drawMusicRemoteScreen() {
  const int width = M5.Display.width();
  const int height = M5.Display.height();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("Music Remote");
  M5.Display.drawFastHLine(0, 16, width, TFT_DARKGREY);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 24);
  M5.Display.print("A : Play/Pause");
  M5.Display.setCursor(8, 40);
  M5.Display.print("B : Previous track");
  M5.Display.setCursor(8, 56);
  M5.Display.print("A+B : Next track");
  M5.Display.setCursor(8, 72);
  M5.Display.print("PWR : Exit remote");

  drawBatteryAndStatus(height - 10);
}

}  // namespace

void renderUi(bool force) {
  if (!force) {
    if (g_view == View::Main && !g_mainUiDirty) return;
    if (g_view != View::Main && !g_submenuUiDirty) return;
  }

  updateBatteryCache();

  if (g_screenSleeping) return;

  if (g_view == View::SlideDeckRemote) {
    drawRemoteScreen();
  } else if (g_view == View::MusicRemote) {
    drawMusicRemoteScreen();
  } else if (g_view == View::IRRemote) {
    drawIrRemoteScreen();
  } else {
    drawMenuScreen();
  }

  g_mainUiDirty = false;
  g_submenuUiDirty = false;
}

}  // namespace sj3000
