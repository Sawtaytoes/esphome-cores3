#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/components/camera_link_protocol/protocol.h"

namespace esphome::camera_link_sender {
class CameraLinkSender : public Component, public uart::UARTDevice, public camera::CameraListener {
 public:
  void set_camera(esp32_camera::ESP32Camera *camera) { camera_ = camera; }
  void setup() override;
  void loop() override;
  void on_shutdown() override { set_enabled(false); }
  void set_enabled(bool enabled);
  void dump_config() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  uint32_t frames_sent() const { return frames_; }
 private:
  esp32_camera::ESP32Camera *camera_{};
  camera_link_protocol::Parser parser_;
  std::shared_ptr<camera::CameraImage> image_;
  HighFrequencyLoopRequester high_frequency_;
  uint8_t header_[camera_link_protocol::HEADER_SIZE]{};
  uint32_t sequence_{}, requested_at_{}, last_rx_{}, frames_{}, sending_at_{};
  size_t sent_{};
  int64_t requested_us_{};
  bool awaiting_{}, retry_capture_{}, enabled_{true};
};
}  // namespace esphome::camera_link_sender
