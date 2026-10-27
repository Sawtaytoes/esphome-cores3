# ESPHome CoreS3 board support

Original, datasheet-based power, reset, backlight, battery, and beep-amplifier
support for M5Stack CoreS3 and CoreS3-SE. No M5Stack software is included.
Mainline ESPHome supplies the LCD, touch, I2S speaker, and RFID drivers.

## Supported configuration

This component targets ESPHome **2026.10.0** with ESP-IDF. It is an initial
hardware bring-up, not a camera application or voice satellite.

```yaml
external_components:
  - source: github://Sawtaytoes/esphome-cores3@<reviewed-commit>
    components: [cores3_board]

i2c:
  - id: board_bus
    sda: GPIO12
    scl: GPIO11
    setup_priority: 1001
  - id: grove_bus
    sda: GPIO9
    scl: GPIO8
    setup_priority: 999

cores3_board:
  id: board_power
  i2c_id: board_bus
```

The board component runs at priority **1000**. The internal bus must initialize
first, and the external bus must initialize after the board has powered Grove.
This order includes external-bus recovery, not only the RFID component.
Do not override the board's priority without preserving that ordering.

For `mipi_spi` model `M5CORE`, explicitly override all three preset pins:
CS **3**, DC **35**, and reset `{cores3_board: board_power, number: 9}`.
SPI uses SCK **36** and MOSI **37**. Do not configure MISO on GPIO35.
Use quad 80 MHz PSRAM and 16 MB flash on `esp32-s3-devkitc-1`.

The board owns touch reset P0_0 and waits 300 ms after releasing it. Configure
mainline `ft63x6` on the internal bus **without a reset or interrupt pin**.
Touch IRQ is an expander input, so use polling. The GPIO provider exposes only
reset pins 0 and 9; it is not a general expander API.

`set_backlight(level)` maps positive levels to the 2.6-3.3 V backlight range.
Zero turns off DLDO1. CPU rails and charging settings remain unchanged.
`battery_voltage()`, `battery_level()`, `charging()`, and `usb_power()` expose
polled PMIC telemetry. Voltage and level become NaN on failed reads.

The AW88298 is configured for **standard I2S, 16 kHz, 16-bit slots, 32 BCLK per
frame, left channel**, with -18 dB hardware attenuation and pass-through boost.
I2S uses BCLK34, LRCLK33, and DOUT13. Start silent I2S clocks before calling
`set_amplifier(true)`. The call waits at most 30 ms each for PLL lock and
switching, and keeps output muted on failure. Call `set_amplifier(false)` after
audio drains and **before** stopping I2S. It mutes and powers down the amplifier.
Brief, faded PCM tones are sufficient; no microphone or audio-input clock is
configured. Amplifier startup failure leaves display/power support usable.

## Power and port limitations

- AW9523B P1_7 is boost enable; P0_1 powers **all** Grove ports.
- P0_5 is USB OTG enable and remains low to preserve USB input.
- P0_4 is microSD detect and remains an input. Interrupt pins are inputs too.
- Port A: SDA2/SCL1. Port B: GPIO9/8, usable as a separate I2C bus.
- Port C: GPIO17/18. Camera UART direction and firmware must be verified separately.
- Grove power uses an increasing eight-pulse precharge before final connection.
- Shared Grove power cannot independently switch a camera while keeping RFID powered.

No live camera preview, photo storage, NAS upload, microphone input, wake word,
or battery-runtime claim is supplied by this component. Verify cold battery
boots with the exact attached load, USB/dock transitions, Grove voltage,
LCD colors, touch corners, and audible output before treating a product as ready.

## Hardware sources and provenance

Implementation uses register descriptions and circuit nets, not vendor driver code:

- [CoreS3-SE schematic, sheets 1, 4, 5, and 6](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/498/Sch_M5_CoreS3_SE_v1.0.pdf)
- [AXP2101 datasheet V1.0](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/AXP2101_Datasheet_V1.0_en.pdf), PMIC status, ADC, LDO control, and voltage registers.
- [AW9523B datasheet](https://files.waveshare.com/upload/3/34/AW9523B-EN.pdf), GPIO mode, direction, output, and global-control registers.
- [AW88298 datasheet V1.6](https://datasheet.lcsc.com/datasheet/pdf/619bd0da051cf7ab47789da79a41c5d1.pdf?productCode=C5162557), reset ID, power/mute, I2S, attenuation, and boost registers.
- [FT6336U datasheet](https://cdn-shop.adafruit.com/product-files/5826/FT6336U-DataSheet-V1.1.pdf), reset/startup timing.
- [Reported weak-supply Grove inrush](https://github.com/m5stack/M5Unified/pull/381), hardware behavior only. No code is copied or linked.
- [ESPHome 2026.10.0](https://github.com/esphome/esphome/tree/2026.10.0), public component, I2C, GPIO, LCD, touch, and speaker interfaces.

Validation requires an actual dashboard compile and board readback. YAML validation
alone does not check C++ lambdas or establish correct hardware behavior.
