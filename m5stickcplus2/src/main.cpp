#include "app.h"
#include <WiFi.h>

using namespace sj3000;

void setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);

  M5.Display.setRotation(2);
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);
  setCpuFrequencyMhz(80);

  g_lastInputMs = millis();
  M5.Display.setBrightness(selectedBrightness());
  updateBatteryCache();
  setStatus("Ready");
  g_mainUiDirty = true;
  g_submenuUiDirty = true;
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

  processInput(btnA, btnB, btnPwr);
  applyIdlePowerPolicy();
  renderUi(false);
  M5.delay(12);
}
