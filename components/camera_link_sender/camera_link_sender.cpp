#include "camera_link_sender.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <esp_timer.h>

namespace esphome::camera_link_sender {
using namespace camera_link_protocol;
static const char *const TAG = "camera_link.sender";

void CameraLinkSender::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled) {
    awaiting_ = retry_capture_ = false;
    image_.reset();
    parser_.reset();
    high_frequency_.stop();
  }
}
void CameraLinkSender::setup() { camera_->add_listener(this); }
void CameraLinkSender::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (!enabled_ || !awaiting_ || image_ || !image->was_requested_by(camera::WEB_REQUESTER)) return;
  if (millis() - requested_at_ > 2000) {
    awaiting_ = retry_capture_ = false;
    return;
  }
  // The camera task can hold a frame captured before the request. Its timestamp
  // uses esp_timer_get_time(), so only accept exposure begun after this request.
  auto *fb = static_cast<esp32_camera::ESP32CameraImage *>(image.get())->get_raw_buffer();
  int64_t captured_us = int64_t(fb->timestamp.tv_sec) * 1000000 + fb->timestamp.tv_usec;
  if (captured_us < requested_us_) {
    retry_capture_ = true;  // Request next loop: ESPHome clears requester bits after callbacks.
    return;
  }
  awaiting_ = false;
  retry_capture_ = false;
  size_t length = image->get_data_length();
  if (length > MAX_JPEG || !is_preview_jpeg(image->get_data_buffer(), length)) {
    ESP_LOGW(TAG, "Rejected non-QVGA or oversized preview (%zu bytes)", length);
    return;
  }
  image_ = image;
  sent_ = 0;
  sending_at_ = millis();
  make_header(header_, FRAME, sequence_, image->get_data_buffer(), length);
  high_frequency_.start();
}
void CameraLinkSender::loop() {
  uint32_t now = millis();
  if (parser_.partial() && now - last_rx_ > 500) parser_.reset();
  uint8_t byte;
  for (size_t i = 0; i < 256 && available(); i++) {
    if (!read_byte(&byte)) break;
    last_rx_ = now;
    if (!parser_.feed(byte)) continue;
    if (parser_.type() == STOP) awaiting_ = retry_capture_ = false;
    if (parser_.type() == REQUEST && enabled_ && !image_ && !awaiting_ && !camera_->is_failed()) {
      sequence_ = parser_.sequence();
      requested_at_ = now;
      requested_us_ = esp_timer_get_time();
      retry_capture_ = false;
      awaiting_ = true;
      camera_->request_image(camera::WEB_REQUESTER);
    }
  }
  if (awaiting_ && now - requested_at_ > 2000) {
    awaiting_ = retry_capture_ = false;
    ESP_LOGW(TAG, "Camera frame request timed out");
  }
  if (awaiting_ && retry_capture_) {
    retry_capture_ = false;
    camera_->request_image(camera::WEB_REQUESTER);
  }
  if (!image_) return;
  const size_t total = HEADER_SIZE + image_->get_data_length();
  if (now - sending_at_ > 2000) {
    image_.reset();
    high_frequency_.stop();
    ESP_LOGW(TAG, "UART send timed out");
    return;
  }
  int available = parent_->available_for_write();
  if (available <= 0) return;
  size_t writable = std::min<size_t>(available, 2048);
  size_t count;
  if (sent_ < HEADER_SIZE) {
    count = std::min(writable, HEADER_SIZE - sent_);
    write_array(header_ + sent_, count);
  } else {
    count = std::min(writable, total - sent_);
    write_array(image_->get_data_buffer() + sent_ - HEADER_SIZE, count);
  }
  sent_ += count;
  if (sent_ == total) {
    image_.reset();
    high_frequency_.stop();
    frames_++;
    if (frames_ % 30 == 1) ESP_LOGD(TAG, "Sent preview %u (%zu bytes)", sequence_, total);
  }
}
void CameraLinkSender::dump_config() {
  ESP_LOGCONFIG(TAG, "Direct camera link sender: QVGA JPEG, bounded 64 KiB frames");
}
}  // namespace esphome::camera_link_sender
