**Plan**

The graphics layer is currently the missing piece: include/ili9488_gfx.h and src/ili9488_gfx.c are empty, while the HAL already has the transport primitives needed to flush a framebuffer. The cleanest path is to keep framebuffer storage and drawing logic in the graphics layer, then use the existing HAL to set the address window and stream pixels to GRAM.

1. Define the public graphics contract in include/ili9488_gfx.h.
   - Add framebuffer dimensions and a clear ownership model.
   - Add an RGB565 helper macro.
   - Declare gfx_fill, gfx_draw_pixel, gfx_draw_rect, and a flush function such as ili9488_write_framebuffer or gfx_flush.
   - Keep the API small and portable.

2. Implement framebuffer storage and primitives in src/ili9488_gfx.c.
   - Use a single static framebuffer, ideally uint16_t fb[WIDTH * HEIGHT].
   - Make WIDTH and HEIGHT match the real panel target in this repo, not the 128x128 issue text.
   - gfx_fill should write the same color to every framebuffer entry.
   - gfx_draw_pixel should bounds-check and write one pixel.
   - gfx_draw_rect should clip once, then fill by scanlines instead of calling draw_pixel repeatedly.

3. Add a framebuffer flush path on top of the existing HAL in src/ili9488_hal.c.
   - Flush should set the full display window, call hal_gram_write_start, then stream the framebuffer.
   - If the active transport format is RGB565, send 2 bytes per pixel directly.
   - If the transport stays in 18-bit mode, add an explicit conversion step from RGB565 framebuffer values to the outgoing byte format.
   - Do not silently assume packed 12-bit or 16-bit behavior when the transport format is different.

4. Update the demo in demo/main.c.
   - Replace the current direct color-cycle test with a small graphics demo that uses gfx_fill, gfx_draw_pixel, and gfx_draw_rect.
   - Flush the framebuffer after each scene so the display is driven through the new graphics path.
   - Keep fail-fast error handling so transport issues surface immediately.

5. Align documentation after the implementation is stable.
   - Update README.md and docs/IMPLEMENTATION_GUIDE.md so they describe the real API and panel size used by this repository.
   - If you want to keep the issue’s 128x128 wording, treat it as a compatibility shim rather than the default implementation.

**Pseudocode recommendations**

- gfx_fill(color): iterate from 0 to WIDTH * HEIGHT - 1 and assign the same color into the framebuffer.
- gfx_draw_pixel(x, y, color): return immediately if x or y is outside bounds; otherwise write framebuffer[y * WIDTH + x] = color.
- gfx_draw_rect(x, y, w, h, color): compute clipped start and end coordinates once, then fill each row segment inside those limits.
- ili9488_write_framebuffer(): set the full window, begin GRAM write mode, then stream the framebuffer as one contiguous transfer when the active pixel format allows it.
- If 18-bit output remains the panel’s preferred mode, convert each RGB565 pixel to 8-bit-per-channel output before flushing.

**What is already done versus still missing**

- Already done:
  - The SPI/GPIO platform layer exists and is working on Linux.
  - The HAL already exposes the needed windowing, GRAM write, and control-command functions.
  - The demo can already initialize the display and show solid fills.

- Still missing:
  - The graphics framebuffer layer itself.
  - A real framebuffer flush API.
  - A demo that exercises fill, pixel, and rectangle primitives instead of only full-screen fills.