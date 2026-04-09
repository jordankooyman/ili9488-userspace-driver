# ILI9488 implementation guide

This guide describes how the current codebase is structured and how data moves from your app to the display.

## Architecture

The driver is split into three layers.

1. SPI and GPIO transport layer
Path: [src/ili9488_spi.c](../src/ili9488_spi.c), public API in [include/ili9488_spi.h](../include/ili9488_spi.h)
Role: open the SPI device, toggle D/C and RESET lines, and transmit or receive bytes.

2. HAL command layer
Path: [src/ili9488_hal.c](../src/ili9488_hal.c), public API in [include/ili9488_hal.h](../include/ili9488_hal.h)
Role: implement ILI9488 command sequences such as init, address windows, GRAM write/read, and power control.

3. Graphics and framebuffer layer
Path: [src/ili9488_gfx.c](../src/ili9488_gfx.c), public API in [include/ili9488_gfx.h](../include/ili9488_gfx.h)
Role: keep an RGB565 framebuffer in memory, provide draw primitives, and flush full or dirty regions through HAL.

## Current Linux hardware assumptions

- SPI device: `/dev/spidev0.0`
- GPIO chip: `/dev/gpiochip0`
- RESET GPIO: BCM 24
- D/C GPIO: BCM 25
- CS: SPI CE0, managed by spidev

These values are defined in [include/config.h](../include/config.h), and are used by the Linux transport implementation.

## Initialization flow

The usual startup sequence in an app is:

1. `hal_display_initialize(pixel_format, rotation)`
2. Allocate framebuffer storage
3. `gfx_framebuffer_bind(...)`
4. Draw with gfx primitives
5. Flush with `gfx_flush(...)` or `gfx_flush_dirty(...)`

`hal_display_initialize()` performs reset, sleep exit, pixel format and rotation setup, power and timing register setup, and display enable.

## Framebuffer model

- Storage format in memory: RGB565 (`uint16_t` per pixel)
- `gfx_framebuffer_t` wraps caller-owned memory
- Dirty tracking is built in (`dirty_x1/y1/x2/y2`)
- Drawing functions update memory and mark dirty bounds

Main entry points:

- `gfx_framebuffer_required_pixels()`
- `gfx_framebuffer_bind()`
- `gfx_fill()`
- `gfx_draw_pixel()`
- `gfx_draw_line()`
- `gfx_draw_filled_rectangle()`
- `gfx_draw_char()` and `gfx_draw_string()`
- `gfx_flush()` and `gfx_flush_dirty()`

## Display flush behavior

`gfx_flush_region()` converts memory pixels into the transport format expected by the current HAL mode and streams them through GRAM write operations.

For this repository state:

- The demo initializes the panel in 18-bit transfer mode.
- The framebuffer remains RGB565 in memory.
- The flush path expands each RGB565 pixel to 18-bit write payload bytes.

## Demo behavior

The demo source is [demo/main.c](../demo/main.c). It runs continuously and shows:

1. Dirty-region rectangle motion around a path
2. Full-screen color fills
3. Starburst line drawing from screen center
4. Repeat

The demo intentionally exercises both drawing primitives and dirty/full flush paths.

## Portability notes

The transport layer is the platform-specific boundary. To port this stack, keep HAL and GFX APIs intact and replace transport internals in [src/ili9488_spi.c](../src/ili9488_spi.c).

Items usually replaced for new platforms:

- SPI open/configure and transfer calls
- GPIO line request and set operations
- delay implementation

## Known limitations

- `hal_display_initialize()` currently uses a fixed SPI path and clock in source.
- The demo is currently an infinite loop and does not exit normally.
- Test scripts in [tests](../tests) are bring-up artifacts and not a formal automated test suite.

## Related docs

- [README.md](../README.md)
- [docs/command-reference.md](command-reference.md)
- [docs/graphics-guide.md](graphics-guide.md)
- [docs/wiring.md](wiring.md)
