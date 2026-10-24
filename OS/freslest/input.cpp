#include "input.h"
#include "button_sampler.h"
#include "src/freslest/audio_manager.h"
#include <atomic>
#ifndef FRESLEST_INPUT_HOST_TEST
#include "usb_input.h"
#endif
#ifndef FRESLEST_INPUT_HOST_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rtc_io.h>
#endif

Button bUp, bDown, bLeft, bRight, bOk, bBack, bFire;

namespace {
constexpr uint32_t PRESS_MASK = 127u << 8;
constexpr int pins[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
                        BTN_OK, BTN_BACK, BTN_FIRE};
Button *const buttons[] = {&bUp, &bDown, &bLeft, &bRight, &bOk, &bBack, &bFire};
ButtonSampler sampler;
std::atomic<uint32_t> sampled{0}, gamePresses{0};
uint8_t previousGamePad = 0;
bool samplerStarted = false, gameInputReady = false;

uint8_t physicalButtons() {
  uint8_t held = 0;
  for (unsigned i = 0; i < 7; ++i)
    if (digitalRead(pins[i]) == LOW)
      held |= 1u << i;
  return held;
}
[[maybe_unused]] void sampleButtons(uint32_t now) {
  uint8_t presses = sampler.sample(physicalButtons(), now);
  uint32_t prior = sampled.load(), next;
  do {
    next = (prior & ~127u) | sampler.held | ((uint32_t)presses << 8);
  } while (!sampled.compare_exchange_weak(prior, next));
  uint8_t pad = nesPadForButtons(sampler.held);
  gamePresses.fetch_or(pad & ~previousGamePad);
  previousGamePad = pad;
}
#ifndef FRESLEST_INPUT_HOST_TEST
void inputWorker(void *) {
  TickType_t wake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1;
  for (;;) {
    sampleButtons(millis());
    vTaskDelayUntil(&wake, period);
  }
}
#endif
uint8_t foregroundButtons() {
  uint8_t held = 0;
  for (unsigned i = 0; i < 7; ++i)
    if (buttons[i]->down())
      held |= 1u << i;
  return held;
}
} // namespace

void Button::begin(int p) {
  pin = p;
#ifndef FRESLEST_INPUT_HOST_TEST
  // EXT1 wake keeps OK/BACK in RTC mode until explicitly released.
  if(p==BTN_OK || p==BTN_BACK){rtc_gpio_hold_dis((gpio_num_t)p);rtc_gpio_deinit((gpio_num_t)p);}
#endif
  pinMode(pin, INPUT_PULLUP);
  stable = lastRead = digitalRead(pin);
  lastChange = millis();
}
void Button::update(uint32_t now, uint16_t db) {
  pressedEdge = false;
  bool r = digitalRead(pin);
  if (r != lastRead) {
    lastRead = r;
    lastChange = now;
  }
  if (now - lastChange >= db && stable != lastRead) {
    stable = lastRead;
    pressedEdge = (stable == LOW);
  }
}
void inputBegin() {
  bFire.begin(BTN_FIRE);
  bUp.begin(BTN_UP);
  bDown.begin(BTN_DOWN);
  bLeft.begin(BTN_LEFT);
  bRight.begin(BTN_RIGHT);
  bOk.begin(BTN_OK);
  bBack.begin(BTN_BACK);
  uint8_t held = physicalButtons();
  sampler.reset(held, millis());
  sampled.store(held);
  previousGamePad = nesPadForButtons(held);
  gamePresses.store(0);
  gameInputReady = false;
#ifndef FRESLEST_INPUT_HOST_TEST
  if (!samplerStarted) {
    samplerStarted = xTaskCreatePinnedToCore(inputWorker, "buttons", 3072,
                                             nullptr, 3, nullptr, 0) == pdPASS;
    if (!samplerStarted)
      Serial.println(
          "Buttons: background sampler unavailable; using frame polling.");
  }
#endif
}
void inputUpdate(uint32_t now) {
  if (samplerStarted) {
    uint32_t snapshot = sampled.fetch_and(~PRESS_MASK);
    for (unsigned i = 0; i < 7; ++i) {
      buttons[i]->stable = (snapshot & (1u << i)) ? LOW : HIGH;
      buttons[i]->pressedEdge = (snapshot & (1u << (i + 8))) != 0;
    }
  } else {
    bUp.update(now);
    bDown.update(now);
    bLeft.update(now);
    bRight.update(now);
    bOk.update(now);
    bBack.update(now);
    bFire.update(now);
  }
#ifndef FRESLEST_INPUT_HOST_TEST
  usbInputUpdate(now);
  uint8_t held = usbInputHeld(), edges = usbInputPressed();
  for (unsigned i = 0; i < 7; ++i) {
    if (held & (1u << i))
      buttons[i]->stable = LOW;
    buttons[i]->pressedEdge |= (edges & (1u << i)) != 0;
  }
#endif
}
void inputNesBegin() {
  gamePresses.store(0);
  gameInputReady = false;
}
uint8_t inputNesPad() {
  uint8_t held = samplerStarted ? (sampled.load() & 127u) : foregroundButtons();
  uint8_t presses = samplerStarted ? (uint8_t)gamePresses.exchange(0) : 0;
  if (!gameInputReady) {
    // One neutral launch frame, without making held controls wait for release.
    gameInputReady = true;
    return 0;
  }
  uint8_t pad = nesPadForButtons(held) | presses;
  // START/SELECT chords suppress the direction/A edges that preceded the chord.
  if (pad & 0x08)
    pad &= ~(0x10 | 0x01);
  if (pad & 0x04)
    pad &= ~(0x20 | 0x01);
#ifndef FRESLEST_INPUT_HOST_TEST
  pad |= usbInputNesPad();
#endif
  return pad;
}
void handleGlobalClick() {
#ifndef FRESLEST_INPUT_HOST_TEST
  if (usbPointer.click) {
    audioClick();
    return;
  }
#endif
  if (bOk.pressed() || bUp.pressed() || bDown.pressed() || bLeft.pressed() ||
      bRight.pressed() || bBack.pressed())
    audioClick();
}
