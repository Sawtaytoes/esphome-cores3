#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static uint16_t screen[320 * 240];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  for (int y = area->y1; y <= area->y2; y++) {
    for (int x = area->x1; x <= area->x2; x++) {
      size_t offset = (x - area->x1) * 2;
      // Preserve the generated panel byte order when capturing its flush buffer.
      screen[y * 320 + x] = LV_COLOR_16_SWAP ?
          uint16_t(pixels[offset] << 8 | pixels[offset + 1]) :
          uint16_t(pixels[offset] | pixels[offset + 1] << 8);
    }
    pixels += (area->x2 - area->x1 + 1) * 2;
  }
  lv_display_flush_ready(display);
}
static void save(const char *path) {
  auto *file = std::fopen(path, "wb"); assert(file);
  std::fprintf(file, "P6\n320 240\n255\n");
  for (auto pixel : screen) {
    uint8_t rgb[] = {uint8_t(((pixel >> 11) & 31) * 255 / 31),
                     uint8_t(((pixel >> 5) & 63) * 255 / 63), uint8_t((pixel & 31) * 255 / 31)};
    std::fwrite(rgb, 1, 3, file);
  }
  std::fclose(file);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  lv_init();
  auto *display = lv_display_create(320, 240);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  static uint16_t draw[320 * 240];
  lv_display_set_buffers(display, draw, nullptr, sizeof(draw), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display, flush);
  auto *page = lv_screen_active();
  lv_obj_set_style_bg_color(page, lv_color_black(), 0);
  auto *image = lv_image_create(page);
  lv_image_set_scale(image, 192);
  lv_obj_center(image);
  std::vector<uint16_t> pixels(320 * 240);
  const uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF,
                              0xFFFF, 0x7800, 0x03E0, 0x000F, 0x7BEF, 0xFC00};
  lv_image_dsc_t descriptor{};
  descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
  descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
  descriptor.header.w = 320; descriptor.header.h = 240; descriptor.header.stride = 640;
  descriptor.data = reinterpret_cast<const uint8_t *>(pixels.data()); descriptor.data_size = pixels.size() * 2;
  unsigned mismatches = 0;
  // Replace the same allocation/descriptor repeatedly, changing every cell each frame.
  for (int frame = 0; frame < 3; frame++) {
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(image, nullptr);
    lv_image_cache_drop(&descriptor);
    for (int y = 0; y < 240; y++)
      for (int x = 0; x < 320; x++) pixels[y * 320 + x] = colors[((y / 80) * 4 + x / 80 + frame) % 12];
    lv_image_cache_drop(&descriptor);
    lv_image_set_src(image, &descriptor);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(image);
    lv_refr_now(display);
    // Interior samples vary across rows and columns, beyond the JPEG MCU boundaries.
    for (int row = 0; row < 3; row++)
      for (int col = 0; col < 4; col++) {
        uint16_t actual = screen[(30 + row * 60 + 30) * 320 + 40 + col * 60 + 30];
        if (actual != colors[(row * 4 + col + frame) % 12]) mismatches++;
      }
  }
  save(argv[1]);
  std::printf("real LVGL RGB565 scale/replacement: %u mismatched samples\n", mismatches);
  lv_deinit();
  return mismatches ? 1 : 0;
}
