#pragma once

#include <cmath>
#include <cstdio>
#include <optional>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::cores3_board {

// The internal I2C bus must run at priority 1001, this board at 1000,
// and the external I2C bus at 999. Grove power precedes bus recovery.
class CoreS3Board : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return 1000.0f; }
  void set_backlight(float level);
  void set_amplifier(bool enabled);
  void write_reset(uint8_t pin, bool value);
  float battery_voltage() const { return battery_voltage_; }
  float battery_level() const { return battery_level_; }
  std::optional<bool> charging() const { return charging_valid_ ? std::optional<bool>(charging_) : std::nullopt; }
  std::optional<bool> usb_power() const { return usb_valid_ ? std::optional<bool>(usb_power_) : std::nullopt; }
  bool audio_ready() const { return audio_ready_; }

 protected:
  bool read8_(uint8_t address, uint8_t reg, uint8_t &value);
  bool write8_(uint8_t address, uint8_t reg, uint8_t value);
  bool modify8_(uint8_t address, uint8_t reg, uint8_t mask, uint8_t value);
  bool read16_(uint8_t reg, uint16_t &value);
  bool write16_(uint8_t reg, uint16_t value);
  bool modify16_(uint8_t reg, uint16_t mask, uint16_t value);
  bool initialize_();
  bool audio_ready_{false};
  bool charging_{false};
  bool usb_power_{false};
  bool charging_valid_{false};
  bool usb_valid_{false};
  float battery_voltage_{NAN};
  float battery_level_{NAN};
};

class ResetPin : public GPIOPin {
 public:
  void set_parent(CoreS3Board *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override { return false; }
  void digital_write(bool value) override { parent_->write_reset(pin_, value != inverted_); }
  size_t dump_summary(char *buffer, size_t len) const override {
    return snprintf(buffer, len, "CoreS3 reset %u", pin_);
  }
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
 protected:
  CoreS3Board *parent_{};
  uint8_t pin_{};
  bool inverted_{};
};

}  // namespace esphome::cores3_board
