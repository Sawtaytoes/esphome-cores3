#include "camera_link_viewer.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <esp_heap_caps.h>
#include "src/misc/cache/instance/lv_image_cache.h"

namespace esphome::camera_link_viewer {
using namespace camera_link_protocol;
static const char *const TAG = "camera_link.viewer";

void CameraLinkViewer::setup() {
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_EXTERNAL);
  jpeg_buffer_ = allocator.allocate(MAX_JPEG);
  rgb_buffer_ = allocator.allocate(320 * 240 * 2);
  if (!jpeg_buffer_ || !rgb_buffer_) {
    ESP_LOGE(TAG, "Could not allocate preview buffers");
    if (jpeg_buffer_) allocator.deallocate(jpeg_buffer_, MAX_JPEG);
    if (rgb_buffer_) allocator.deallocate(rgb_buffer_, 320 * 240 * 2);
    jpeg_buffer_ = rgb_buffer_ = nullptr;
    mark_failed();
    return;
  }
  sequence_ = random_uint32();  // Keep rebooted requests distinct from buffered old replies.
  parser_.set_buffer(jpeg_buffer_, MAX_JPEG);
  publish_status_("Idle");
}
void CameraLinkViewer::publish_status_(const char *value) {
  if (!status_->has_state() || status_->state != value) status_->publish_state(value);
}
void CameraLinkViewer::hide_() {
  if (image_widget_) {
    lv_obj_add_flag(image_widget_, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(image_widget_, nullptr);
    lv_image_cache_drop(get_lv_image_dsc());
  }
  visible_ = false;
}
void CameraLinkViewer::set_enabled(bool enabled) {
  if (is_failed() || enabled_ == enabled) return;
  enabled_ = enabled;
  pending_ = false;
  parser_.reset();
  hide_();
  if (enabled) {
    if (!image_widget_) {
      image_widget_ = lv_image_create(parent_widget_);
      lv_image_set_scale(image_widget_, 192);  // 320x240 -> 240x180, preserving QVGA decode.
      lv_obj_center(image_widget_);
    }
    high_frequency_.start();
    next_request_ = millis();
    last_loop_ = next_request_;
    max_loop_gap_ = 0;
    publish_status_("Waiting for camera");
  } else {
    uint8_t header[HEADER_SIZE];
    make_header(header, STOP, ++sequence_, nullptr, 0);
    write_array(header, sizeof(header));
    high_frequency_.stop();
    publish_status_("Idle");
  }
}
void CameraLinkViewer::request_() {
  uint8_t header[HEADER_SIZE];
  make_header(header, REQUEST, ++sequence_, nullptr, 0);
  write_array(header, sizeof(header));
  pending_ = true;
  requested_at_ = millis();
}
void CameraLinkViewer::receive_() {
  if (!enabled_ || !pending_ || parser_.type() != FRAME || parser_.sequence() != sequence_) return;
  pending_ = false;
  next_request_ = millis() + 100;
  if (millis() - requested_at_ > 2000) {
    hide_();
    publish_status_("Camera unavailable");
    return;
  }
  size_t length = parser_.length();
  if (!is_preview_jpeg(jpeg_buffer_, length)) {
    hide_();
    publish_status_("Invalid camera image");
    return;
  }
  // Remove the old image before touching its backing memory or decoder state.
  hide_();
  uint32_t decode_started = millis();
  decoded_pixels_ = 0;
  bool opened = decoder_.openRAM(jpeg_buffer_, length, draw_jpeg_);
  bool ok = opened && decoder_.getJPEGType() == JPEG_MODE_BASELINE &&
      decoder_.getWidth() == 320 && decoder_.getHeight() == 240;
  if (ok) {
    decoder_.setUserPointer(this);
    decoder_.setPixelType(RGB565_LITTLE_ENDIAN);
    ok = decoder_.decode(0, 0, 0) && decoded_pixels_ == 320 * 240;
  }
  if (opened) decoder_.close();
  ok = ok && millis() - requested_at_ <= 2000;
  next_request_ = millis() + 100;
  if (!ok) {
    publish_status_("Camera decode failed");
    return;
  }
  data_start_ = rgb_buffer_;
  uint32_t decode_ms = millis() - decode_started;
  lv_image_cache_drop(get_lv_image_dsc());
  lv_image_set_src(image_widget_, get_lv_image_dsc());
  lv_obj_remove_flag(image_widget_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(image_widget_);
  visible_ = true;
  last_frame_ = millis();
  frames_++;
  max_jpeg_ = std::max<uint32_t>(max_jpeg_, length);
  publish_status_("Live");
  if (frames_ % 30 == 1) {
    ESP_LOGD(TAG, "Preview frames=%u JPEG=%uB roundtrip=%ums decode=%ums gap=%ums errors=%u heap=%u largest=%u PSRAM=%u",
             frames_, max_jpeg_, millis() - requested_at_, decode_ms, max_loop_gap_, parser_.errors(),
             unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  }
}
int CameraLinkViewer::draw_jpeg_(JPEGDRAW *block) {
  auto *self = static_cast<CameraLinkViewer *>(block->pUser);
  if (!self || !block->pPixels || block->x < 0 || block->y < 0 || block->iWidth <= 0 ||
      block->iHeight <= 0 || block->x + block->iWidth > 320 || block->y + block->iHeight > 240 ||
      millis() - self->requested_at_ > 2000) return 0;
  for (int row = 0; row < block->iHeight; row++) {
    std::memcpy(self->rgb_buffer_ + ((block->y + row) * 320 + block->x) * 2,
                block->pPixels + row * block->iWidth, block->iWidth * 2);
  }
  self->decoded_pixels_ += block->iWidth * block->iHeight;
  return 1;
}
void CameraLinkViewer::loop() {
  uint32_t now = millis();
  if (enabled_) max_loop_gap_ = std::max(max_loop_gap_, now - last_loop_);
  last_loop_ = now;
  if (parser_.partial() && now - last_rx_ > 500) parser_.reset();
  uint8_t byte;
  for (size_t i = 0; i < 8192 && available(); i++) {
    if (!read_byte(&byte)) break;
    last_rx_ = now;
    if (parser_.feed(byte)) receive_();
  }
  if (!enabled_) return;
  now = millis();  // Decoding may have advanced the clock since entering loop().
  if (visible_ && now - last_frame_ > 2000) {
    hide_();
    publish_status_("Camera unavailable");
  }
  if (pending_ && now - requested_at_ > 2000) {
    pending_ = false;
    parser_.reset();
    publish_status_("Camera unavailable");
  }
  if (!pending_ && int32_t(now - next_request_) >= 0 && parent_->available_for_write() >= int(HEADER_SIZE)) request_();
}
void CameraLinkViewer::dump_config() {
  ESP_LOGCONFIG(TAG, "Direct camera preview: QVGA RGB565 decode, 240x180 display, CRC32, sequence checks");
}
}  // namespace esphome::camera_link_viewer
