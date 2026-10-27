#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::camera_link_protocol {
constexpr size_t HEADER_SIZE = 20;
constexpr size_t MAX_JPEG = 65536;
enum Type : uint8_t { REQUEST = 1, FRAME = 2, STOP = 3 };
constexpr uint8_t MAGIC[]{'C', 'A', 'M', '1'};

inline uint32_t crc_step(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  return crc;
}
inline uint32_t read32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline void write32(uint8_t *p, uint32_t value) {
  for (int i = 0; i < 4; i++) p[i] = uint8_t(value >> (i * 8));
}
inline void make_header(uint8_t *header, Type type, uint32_t sequence, const uint8_t *data, size_t length) {
  std::memcpy(header, MAGIC, 4);
  header[4] = type;
  header[5] = 1;
  header[6] = header[7] = 0;
  write32(header + 8, sequence);
  write32(header + 12, length);
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 4; i < 16; i++) crc = crc_step(crc, header[i]);
  for (size_t i = 0; i < length; i++) crc = crc_step(crc, data[i]);
  write32(header + 16, crc ^ 0xFFFFFFFFU);
}

// The caller owns the fixed payload buffer. No allocation comes from wire data.
class Parser {
 public:
  Parser(uint8_t *payload = nullptr, size_t capacity = 0) : payload_(payload), capacity_(capacity) {}
  void set_buffer(uint8_t *payload, size_t capacity) { payload_ = payload; capacity_ = capacity; reset(); }
  void reset() { header_pos_ = payload_pos_ = length_ = 0; }
  bool partial() const { return header_pos_ != 0; }
  uint8_t type() const { return result_type_; }
  uint32_t sequence() const { return result_sequence_; }
  size_t length() const { return result_length_; }
  uint32_t errors() const { return errors_; }
  bool feed(uint8_t byte) {
    if (header_pos_ < HEADER_SIZE) {
      if (header_pos_ < 4 && byte != MAGIC[header_pos_]) {
        header_pos_ = byte == MAGIC[0] ? 1 : 0;
        if (header_pos_) header_[0] = byte;
        return false;
      }
      header_[header_pos_++] = byte;
      if (header_pos_ != HEADER_SIZE) return false;
      length_ = read32(header_ + 12);
      bool valid_type = header_[4] == REQUEST || header_[4] == FRAME || header_[4] == STOP;
      if (!valid_type || header_[5] != 1 || header_[6] || header_[7] || length_ > capacity_ ||
          (header_[4] != FRAME && length_ != 0) || (header_[4] == FRAME && length_ == 0)) {
        errors_++;
        reset();
        return false;
      }
      crc_ = 0xFFFFFFFFU;
      for (size_t i = 4; i < 16; i++) crc_ = crc_step(crc_, header_[i]);
      if (length_ != 0) return false;
    } else {
      payload_[payload_pos_++] = byte;
      crc_ = crc_step(crc_, byte);
      if (payload_pos_ != length_) return false;
    }
    bool valid = (crc_ ^ 0xFFFFFFFFU) == read32(header_ + 16);
    if (valid) {
      result_type_ = header_[4];
      result_sequence_ = read32(header_ + 8);
      result_length_ = length_;
    } else errors_++;
    reset();
    return valid;
  }
 private:
  uint8_t header_[HEADER_SIZE]{};
  uint8_t *payload_{};
  size_t capacity_{}, header_pos_{}, payload_pos_{}, length_{}, result_length_{};
  uint32_t crc_{}, result_sequence_{}, errors_{};
  uint8_t result_type_{};
};

// Validate the JPEG header before giving it to a decoder. Preview is strictly
// baseline, 8-bit, 320x240; still images use a separate future capture path.
inline bool is_preview_jpeg(const uint8_t *data, size_t size) {
  if (size < 4 || data[0] != 0xFF || data[1] != 0xD8 || data[size - 2] != 0xFF || data[size - 1] != 0xD9)
    return false;
  size_t pos = 2;
  while (pos + 1 < size) {
    if (data[pos++] != 0xFF) return false;
    while (pos < size && data[pos] == 0xFF) pos++;
    if (pos >= size) return false;
    uint8_t marker = data[pos++];
    if (marker == 0xDA || marker == 0xD9 || marker == 0xD8 || marker == 0) return false;
    if (pos + 2 > size) return false;
    size_t length = (size_t(data[pos]) << 8) | data[pos + 1];
    if (length < 2 || length > size - pos) return false;
    if (marker == 0xC0) {
      if (length < 11 || data[pos + 2] != 8) return false;
      unsigned height = (unsigned(data[pos + 3]) << 8) | data[pos + 4];
      unsigned width = (unsigned(data[pos + 5]) << 8) | data[pos + 6];
      unsigned components = data[pos + 7];
      return width == 320 && height == 240 && (components == 1 || components == 3) &&
          length == 8 + 3 * components;
    }
    if (marker >= 0xC1 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
      return false;
    pos += length;
  }
  return false;
}
}  // namespace esphome::camera_link_protocol
