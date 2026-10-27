#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/image/image.h"
#include <JPEGDEC.h>
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/camera_link_protocol/protocol.h"

namespace esphome::camera_link_viewer {
class CameraLinkViewer : public Component, public uart::UARTDevice, public image::Image {
 public:
  CameraLinkViewer() : Image(nullptr, 320, 240, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE) {}
  void setup() override;
  void loop() override;
  void on_shutdown() override { set_enabled(false); }
  void dump_config() override;
  void set_parent_widget(lv_obj_t *widget) { parent_widget_ = widget; }
  void set_status(text_sensor::TextSensor *sensor) { status_ = sensor; }
  void set_enabled(bool enabled);
  uint32_t frames_received() const { return frames_; }
 private:
  static int draw_jpeg_(JPEGDRAW *block);
  void request_();
  void receive_();
  void hide_();
  void publish_status_(const char *value);
  lv_obj_t *parent_widget_{}, *image_widget_{};
  text_sensor::TextSensor *status_{};
  camera_link_protocol::Parser parser_;
  HighFrequencyLoopRequester high_frequency_;
  JPEGDEC decoder_;
  uint32_t decoded_pixels_{};
  uint8_t *jpeg_buffer_{}, *rgb_buffer_{};
  bool enabled_{}, pending_{}, visible_{};
  uint32_t sequence_{}, requested_at_{}, last_frame_{}, last_rx_{}, next_request_{}, frames_{}, last_loop_{}, max_loop_gap_{}, max_jpeg_{};
};
}  // namespace esphome::camera_link_viewer
