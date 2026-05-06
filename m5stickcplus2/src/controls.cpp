#include "app.h"

namespace sj3000 {

namespace {

void handleBluetoothAction(uint8_t index) {
  switch (index) {
    case 0:
      enableBluetooth();
      setStatus("Bluetooth pairing");
      break;
    case 1:
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
    g_submenuUiDirty = true;
    setStatus("SlideDeck remote active");
    return;
  }

  goBackOneLevel();
}

void handleMusicAction(uint8_t index) {
  if (index == 0) {
    enableBluetooth();
    g_view = View::MusicRemote;
    g_submenuUiDirty = true;
    setStatus("Music remote active");
    return;
  }

  goBackOneLevel();
}

void handleIrAction(uint8_t index) {
  switch (index) {
    case 0:
      sendIrCode(0x20DF10EF, 32, 0, "TV Power sent");
      break;
    case 1:
      sendIrCode(0x20DF40BF, 32, 0, "Volume Up sent");
      break;
    case 2:
      sendIrCode(0x20DFC03F, 32, 0, "Volume Down sent");
      break;
    case 3:
      sendIrCode(0x20DF906F, 32, 0, "Mute sent");
      break;
    default:
      goBackOneLevel();
      break;
  }
}

void handleWifiAction(uint8_t index) {
  switch (index) {
    case 0:
      startWifiPortal();
      break;
    case 1:
      stopWifiPortal();
      break;
    default:
      goBackOneLevel();
      break;
  }
}

}  // namespace

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

void sendMediaCommand(const MediaKeyReport& key, const char* successText) {
  if (!g_bleEnabled) {
    setStatus("Enable Bluetooth first");
    return;
  }

  if (!g_bleKeyboard.isConnected()) {
    setStatus("No host connection");
    return;
  }

  g_bleKeyboard.write(key);
  setStatus(successText, 800);
}

void goBackOneLevel() {
  if (g_view == View::Main) {
    setStatus("Main menu");
    renderUi(true);
    return;
  }

  if (g_view == View::SlideDeckRemote) {
    g_view = View::PowerPoint;
    g_submenuUiDirty = true;
    setStatus("PowerPoint menu");
    renderUi(true);
    return;
  }

  if (g_view == View::MusicRemote) {
    g_view = View::Music;
    g_submenuUiDirty = true;
    setStatus("Music menu");
    renderUi(true);
    return;
  }

  if (g_view == View::IRRemote) {
    g_view = View::IR;
    g_submenuUiDirty = true;
    setStatus("IR menu");
    renderUi(true);
    return;
  }

  if (g_view == View::WifiPortal) {
    g_view = View::Wifi;
    g_submenuUiDirty = true;
    setStatus("WiFi menu");
    renderUi(true);
    return;
  }

  g_view = View::Main;
  g_mainUiDirty = true;
  g_submenuUiDirty = true;
  setStatus("Main menu");
  renderUi(true);
}

void advanceSelection() {
  const size_t len = menuLengthForView(g_view);
  if (len == 0) return;

  const uint8_t next = static_cast<uint8_t>((currentMenuIndex() + 1) % len);
  switch (g_view) {
    case View::Main: g_mainIndex = next; break;
    case View::Bluetooth: g_btIndex = next; break;
    case View::Power: g_powerIndex = next; break;
    case View::PowerPoint: g_ppIndex = next; break;
    case View::Music: g_musicIndex = next; break;
    case View::IR: g_irIndex = next; break;
    case View::Wifi: g_wifiIndex = next; break;
    case View::SlideDeckRemote:
    case View::MusicRemote:
    case View::IRRemote:
    case View::WifiPortal:
      break;
  }

  g_mainUiDirty = true;
  g_submenuUiDirty = true;
  renderUi(true);
}

void handleMusicRemoteInput(bool btnA, bool btnB, bool btnPwr) {
  if (btnA && btnB) {
    sendMediaCommand(KEY_MEDIA_NEXT_TRACK, "Next track");
    renderUi(true);
    return;
  }

  if (btnA) {
    sendMediaCommand(KEY_MEDIA_PLAY_PAUSE, "Play/Pause");
    renderUi(true);
  }

  if (btnB) {
    sendMediaCommand(KEY_MEDIA_PREVIOUS_TRACK, "Previous track");
    renderUi(true);
  }

  if (btnPwr) {
    goBackOneLevel();
  }
}

void activateCurrentItem() {
  const uint8_t index = currentMenuIndex();

  switch (g_view) {
    case View::Main:
      if (index == 0) g_view = View::Bluetooth;
      else if (index == 1) g_view = View::Power;
      else if (index == 2) g_view = View::PowerPoint;
      else if (index == 3) g_view = View::Music;
      else if (index == 4) g_view = View::IR;
      else g_view = View::Wifi;
      g_mainUiDirty = true;
      g_submenuUiDirty = true;
      setStatus(menuTitleForView(g_view));
      break;
    case View::Bluetooth:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handleBluetoothAction(index);
      break;
    case View::Power:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handlePowerAction(index);
      break;
    case View::PowerPoint:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handlePowerPointAction(index);
      break;
    case View::Music:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handleMusicAction(index);
      break;
    case View::IR:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handleIrAction(index);
      break;
    case View::Wifi:
      if (submenuActionIsReturn(g_view, index)) goBackOneLevel();
      else handleWifiAction(index);
      break;
    case View::SlideDeckRemote:
    case View::MusicRemote:
    case View::IRRemote:
    case View::WifiPortal:
      break;
  }

  renderUi(true);
}

void processInput(bool btnA, bool btnB, bool btnPwr) {
  if (g_view == View::SlideDeckRemote) {
    if (btnA) {
      sendSlideCommand(KEY_RIGHT_ARROW, "Next slide");
      renderUi(true);
    }
    if (btnB) {
      sendSlideCommand(KEY_LEFT_ARROW, "Previous slide");
      renderUi(true);
    }
    if (btnPwr) goBackOneLevel();
    return;
  }

  if (g_view == View::MusicRemote) {
    handleMusicRemoteInput(btnA, btnB, btnPwr);
    return;
  }

  if (g_view == View::IRRemote) {
    handleIrRemoteInput(btnA, btnB, btnPwr);
    return;
  }

  if (btnA) activateCurrentItem();
  if (btnB) advanceSelection();
  if (btnPwr) goBackOneLevel();
}

}  // namespace sj3000
