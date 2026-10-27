#include "../components/camera_link_protocol/protocol.h"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
using namespace esphome::camera_link_protocol;
std::vector<uint8_t> packet(Type type, uint32_t seq, const std::vector<uint8_t> &payload = {}) {
  std::vector<uint8_t> wire(HEADER_SIZE + payload.size());
  make_header(wire.data(), type, seq, payload.data(), payload.size());
  std::copy(payload.begin(), payload.end(), wire.begin() + HEADER_SIZE);
  return wire;
}
bool feed(Parser &parser, const std::vector<uint8_t> &wire) {
  bool result = false;
  for (auto byte : wire) result = parser.feed(byte) || result;
  return result;
}
int main() {
  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : std::vector<uint8_t>{'1','2','3','4','5','6','7','8','9'}) crc = crc_step(crc, byte);
  assert((crc ^ 0xFFFFFFFF) == 0xCBF43926);  // Standard IEEE check value.
  std::vector<uint8_t> storage(MAX_JPEG);
  Parser parser(storage.data(), storage.size());
  auto request = packet(REQUEST, 0xFEDCBA98);
  assert(feed(parser, request) && parser.sequence() == 0xFEDCBA98 && parser.length() == 0);
  auto payload = std::vector<uint8_t>(MAX_JPEG, 0xBC);
  auto frame = packet(FRAME, 42, payload);
  assert(read32(frame.data() + 12) == 65536);
  assert(feed(parser, frame) && parser.length() == MAX_JPEG && storage == payload);
  for (size_t i : {size_t(4), size_t(8), size_t(12), size_t(16), HEADER_SIZE, frame.size() - 1}) {
    auto bad = frame; bad[i] ^= 0x01;
    assert(!feed(parser, bad));
    parser.reset(); assert(feed(parser, request));
  }
  for (size_t length : {size_t(0), MAX_JPEG + 1}) {
    auto bad = request; bad[4] = FRAME; write32(bad.data()+12, length);
    assert(!feed(parser, bad)); assert(feed(parser, request));
  }
  // Byte deletion/insertion cannot publish a corrupt frame; timeout reset recovers.
  auto deleted = frame; deleted.erase(deleted.begin()+100);
  assert(!feed(parser, deleted)); parser.reset(); assert(feed(parser, request));
  auto inserted = frame; inserted.insert(inserted.begin()+100, 0);
  assert(!feed(parser, inserted)); parser.reset(); assert(feed(parser, request));
  for (size_t cut : {size_t(1), size_t(19), size_t(20), size_t(1000), frame.size()-1}) {
    assert(!feed(parser, std::vector<uint8_t>(frame.begin(),frame.begin()+cut)));
    parser.reset(); assert(feed(parser, request));
  }
  Parser commands;
  assert(!feed(commands, frame)); commands.reset(); assert(feed(commands, request));
  assert(feed(parser, packet(STOP, 0xFFFFFFFF)));
  assert(feed(parser, packet(REQUEST, 0)));  // Wrap is legal; caller keeps sequence/session state.
  std::vector<uint8_t> jpeg{0xFF,0xD8,0xFF,0xC0,0,11,8,0,240,1,64,1,1,0x11,0,0xFF,0xD9};
  assert(is_preview_jpeg(jpeg.data(),jpeg.size()));
  for (auto index : {3,6,8,10,11}) { auto bad=jpeg; bad[index]^=1; assert(!is_preview_jpeg(bad.data(),bad.size())); }
  for (size_t cut=0;cut<jpeg.size();cut++) assert(!is_preview_jpeg(jpeg.data(),cut));
  // Arbitrary wire bytes and malformed JPEG headers remain bounded under sanitizers.
  std::mt19937 random(0xC0FE);
  std::vector<uint8_t> noise(1000000);
  for (auto &byte : noise) byte=uint8_t(random());
  feed(parser,noise); parser.reset(); assert(feed(parser, request));
  for (size_t i=0;i<noise.size();i+=100) is_preview_jpeg(noise.data()+i,std::min<size_t>(100,noise.size()-i));
  std::cout << "Protocol boundaries, corruption, recovery, JPEG headers, and fuzz checks passed\n";
}
