#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${LVGL_SOURCE:?Set LVGL_SOURCE to the actual LVGL source checkout}"
: "${LVGL_CONFIG:?Set LVGL_CONFIG to the dashboard-generated lv_conf.h}"
: "${LVGL_TEST_OUTPUT:?Set LVGL_TEST_OUTPUT to an output directory}"
mkdir -p "$LVGL_TEST_OUTPUT"
for support in 0 1; do
  build="$LVGL_TEST_OUTPUT/support-$support"
  mkdir -p "$build"
  # Only adapt the allocator and draw-unit bootstrap for a standalone host.
  # All transform/color options come from the real generated configuration.
  sed -e 's/^#define LV_USE_STDLIB_MALLOC .*/#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB/' \
      -e 's/^#define LV_DRAW_SW_DRAW_UNIT_CNT .*/#define LV_DRAW_SW_DRAW_UNIT_CNT 1/' \
      -e "s/^#define LV_DRAW_SW_SUPPORT_RGB565A8 .*/#define LV_DRAW_SW_SUPPORT_RGB565A8 $support/" \
      "$LVGL_CONFIG" > "$build/lv_conf.h"
  cmake -S tests/lvgl -B "$build" -G Ninja -DLVGL_SOURCE="$LVGL_SOURCE" \
    -DLV_BUILD_CONF_PATH="$build/lv_conf.h" -DCMAKE_BUILD_TYPE=Debug > "$build/config.log" 2>&1
  cmake --build "$build" -j 4 > "$build/build.log" 2>&1
  if "$build/render" "$LVGL_TEST_OUTPUT/render-$support.ppm"; then
    [ "$support" = 1 ] || { echo 'Regression did not reproduce with support disabled'; exit 1; }
  else
    [ "$support" = 0 ] || exit 1
  fi
done
