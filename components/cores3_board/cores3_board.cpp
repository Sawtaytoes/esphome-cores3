#include "cores3_board.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::cores3_board {
static const char *const TAG = "cores3_board";
static constexpr uint8_t PMU = 0x34, IO = 0x58, AMP = 0x36;

bool CoreS3Board::read8_(uint8_t address, uint8_t reg, uint8_t &value) {
  return bus_->write_readv(address, &reg, 1, &value, 1) == i2c::ERROR_OK;
}
bool CoreS3Board::write8_(uint8_t address, uint8_t reg, uint8_t value) {
  const uint8_t data[]{reg, value};
  return bus_->write_readv(address, data, sizeof(data), nullptr, 0) == i2c::ERROR_OK;
}
bool CoreS3Board::modify8_(uint8_t address, uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t previous;
  return read8_(address, reg, previous) && write8_(address, reg, (previous & ~mask) | (value & mask));
}
bool CoreS3Board::read16_(uint8_t reg, uint16_t &value) {
  uint8_t data[2];
  if (bus_->write_readv(AMP, &reg, 1, data, 2) != i2c::ERROR_OK) return false;
  value = (uint16_t(data[0]) << 8) | data[1];
  return true;
}
bool CoreS3Board::write16_(uint8_t reg, uint16_t value) {
  const uint8_t data[]{reg, uint8_t(value >> 8), uint8_t(value)};
  return bus_->write_readv(AMP, data, sizeof(data), nullptr, 0) == i2c::ERROR_OK;
}
bool CoreS3Board::modify16_(uint8_t reg, uint16_t mask, uint16_t value) {
  uint16_t previous;
  return read16_(reg, previous) && write16_(reg, (previous & ~mask) | (value & mask));
}

bool CoreS3Board::initialize_() {
  uint8_t chip;
  if (!read8_(IO, 0x10, chip) || chip != 0x23) return false;
  // AW9523B: GPIO function, P0 push-pull, preload outputs before direction.
  // P0_0 touch reset, P0_1 Grove bus, P0_2 amplifier reset, P0_5 USB OTG.
  // P1_1 LCD reset, P1_7 5 V boost. USB OTG remains off to avoid backfeed.
  if (!modify8_(IO, 0x04, 0x18, 0x18) || !modify8_(IO, 0x05, 0x0C, 0x0C) ||
      !modify8_(IO, 0x12, 0x27, 0x27) || !modify8_(IO, 0x13, 0x82, 0x82) ||
      !modify8_(IO, 0x11, 0x10, 0x10) ||
      !modify8_(IO, 0x02, 0x27, 0x00) || !modify8_(IO, 0x03, 0x82, 0x00) ||
      !modify8_(IO, 0x04, 0x27, 0x00) || !modify8_(IO, 0x05, 0x82, 0x00)) return false;
  // Leave CPU/DC-DC rails, charging voltage/current, and thermal protection intact.
  // ALDO1 supplies amplifier digital I/O, ALDO2 the analog rail.
  // DLDO1 supplies LCD backlight.
  if (!modify8_(PMU, 0x92, 0x1F, 13) || !modify8_(PMU, 0x93, 0x1F, 28) ||
      !modify8_(PMU, 0x99, 0x1F, 23) || !modify8_(PMU, 0x90, 0x83, 0x83) ||
      !modify8_(PMU, 0x30, 0x05, 0x05)) return false;
  if (!modify8_(IO, 0x03, 0x80, 0x80)) return false;
  delay(10);
  // Gradual bus connection limits inrush to the Grove load on a battery boot.
  for (uint8_t step = 1; step <= 8; step++) {
    if (!modify8_(IO, 0x02, 0x02, 0x02)) return false;
    delay_microseconds_safe(step * 100);
    if (!modify8_(IO, 0x02, 0x02, 0x00)) return false;
    delay_microseconds_safe(1000 - step * 100);
  }
  if (!modify8_(IO, 0x02, 0x07, 0x07) || !modify8_(IO, 0x03, 0x02, 0x02)) return false;
  delay(300);  // FT6336U reset-to-first-report startup time.
  uint8_t port0, port1;
  return read8_(IO, 0x02, port0) && read8_(IO, 0x03, port1) &&
      (port0 & 0x27) == 0x07 && (port1 & 0x82) == 0x82;
}

void CoreS3Board::setup() {
  if (!initialize_()) {
    ESP_LOGE(TAG, "Power/reset initialization failed");
    mark_failed();
    return;
  }
  // AW88298 16-bit big-endian registers. Standard I2S, left, 16 kHz,
  // 16-bit slots, 32 BCLK/frame. Start muted and powered down.
  uint16_t chip;
  audio_ready_ = read16_(0x00, chip) && chip == 0x1852 &&
      modify16_(0x04, 0x00FF, 0x0043) &&
      modify16_(0x05, 0x0010, 0x0010) &&
      modify16_(0x06, 0x3FFF, 0x1403) &&
      modify16_(0x61, 0x7000, 0x0000) &&  // Pass-through boost, conservative bring-up.
      modify16_(0x0C, 0xFF00, 0x3000);  // -18 dB ceiling for the 1 W speaker.
  if (!audio_ready_) ESP_LOGW(TAG, "Amplifier initialization failed");
  ESP_LOGI(TAG, "Grove power and display resets ready; microphone capture disabled");
  update();
}

void CoreS3Board::write_reset(uint8_t pin, bool value) {
  if (is_failed() || (pin != 0 && pin != 9)) return;
  uint8_t mask = 1U << (pin % 8);
  if (!modify8_(IO, 0x02 + pin / 8, mask, value ? mask : 0)) status_set_warning();
}

void CoreS3Board::set_backlight(float level) {
  if (is_failed() || !std::isfinite(level)) return;
  level = std::fmax(0.0f, std::fmin(1.0f, level));
  // Useful backlight range 2.6-3.3 V; zero disables DLDO1 only.
  const uint8_t voltage = 21 + uint8_t(std::lround(level * 7));
  if (!modify8_(PMU, 0x99, 0x1F, voltage) ||
      !modify8_(PMU, 0x90, 0x80, level > 0 ? 0x80 : 0)) status_set_warning();
}

void CoreS3Board::set_amplifier(bool enabled) {
  if (!audio_ready_) return;
  bool ok;
  if (enabled) {
    // Caller starts silent I2S first. Keep the amplifier muted until clocks lock.
    ok = modify16_(0x04, 0x0003, 0x0002);
    uint16_t status = 0;
    for (uint8_t attempt = 0; ok && attempt < 30; attempt++) {
      ok = read16_(0x01, status);
      if (status & 1) break;
      delay(1);
    }
    ok = ok && (status & 1) && modify16_(0x04, 0x0002, 0x0000);
    for (uint8_t attempt = 0; ok && attempt < 30; attempt++) {
      ok = read16_(0x01, status);
      if (status & 0x100) break;
      delay(1);
    }
    ok = ok && (status & 0x100) && modify16_(0x05, 0x0010, 0x0000);
  } else {
    ok = modify16_(0x05, 0x0010, 0x0010) && modify16_(0x04, 0x0003, 0x0003);
  }
  if (!ok) {
    modify16_(0x05, 0x0010, 0x0010);
    modify16_(0x04, 0x0003, 0x0003);
    ESP_LOGW(TAG, "Amplifier transition failed; output muted");
    status_set_warning();
  }
}

void CoreS3Board::update() {
  uint8_t hi, lo, percent, status0, status1;
  if (read8_(PMU, 0x34, hi) && read8_(PMU, 0x35, lo))
    battery_voltage_ = (((hi & 0x3F) << 8) | lo) / 1000.0f;
  else battery_voltage_ = NAN;
  if (read8_(PMU, 0xA4, percent) && percent <= 100) battery_level_ = percent;
  else battery_level_ = NAN;
  if (read8_(PMU, 0x00, status0)) usb_power_ = (status0 & 0x20) != 0;
  if (read8_(PMU, 0x01, status1)) charging_ = ((status1 >> 5) & 3) == 1;
  ESP_LOGD(TAG, "Battery %.3f V, %.0f%%, USB %d, charging %d", battery_voltage_, battery_level_, usb_power_, charging_);
}

void CoreS3Board::dump_config() {
  ESP_LOGCONFIG(TAG, "CoreS3 board: original power/reset/audio driver");
  ESP_LOGCONFIG(TAG, "  Amplifier ready: %s", YESNO(audio_ready_));
}
}  // namespace esphome::cores3_board
