# Graphics layer guide

This guide explains how to use the framebuffer graphics API in this repository.

## Purpose

The graphics layer keeps drawing state in memory and sends updates to the display through HAL.

- Header: [include/ili9488_gfx.h](../include/ili9488_gfx.h)
- Source: [src/ili9488_gfx.c](../src/ili9488_gfx.c)

## Framebuffer lifecycle

1. Compute storage requirement
2. Allocate memory
3. Bind memory to `gfx_framebuffer_t`
4. Draw into that framebuffer
5. Flush full or dirty region
6. Unbind and free memory

Example:

```c
uint16_t width = 320;
uint16_t height = 480;
size_t pixels = gfx_framebuffer_required_pixels(width, height);
uint16_t *storage = malloc(pixels * sizeof(uint16_t));

gfx_framebuffer_t fb;
if (!gfx_framebuffer_bind(&fb, storage, pixels, width, height, 0U)) {
    free(storage);
    return false;
}
```

## Drawing primitives

Supported primitives in memory:

- `gfx_fill`
- `gfx_draw_pixel`
- `gfx_draw_filled_rectangle`
- `gfx_draw_line`
- `gfx_draw_mono_bitmap`
- `gfx_draw_char`
- `gfx_draw_string`

Text rendering uses built-in ASCII fonts declared in [include/ili9488_fnt.h](../include/ili9488_fnt.h) and implemented in [src/ili9488_fnt.c](../src/ili9488_fnt.c).

## Dirty tracking

The framebuffer tracks modified bounds.

Useful helpers:

- `gfx_mark_dirty_full`
- `gfx_mark_dirty_region`
- `gfx_is_dirty`
- `gfx_clear_dirty`

Flush options:

- `gfx_flush`: always flush full framebuffer
- `gfx_flush_dirty`: flush only current dirty bounds
- `gfx_flush_region`: flush a specific region

## Color format details

- In-memory format: RGB565 (`uint16_t`)
- Use `RGB565(r, g, b)` macro for convenient packing

The HAL transfer format can differ from memory format. The flush path handles conversion needed for the active panel transfer mode.

## Practical usage pattern

```c
gfx_fill(&fb, RGB565(0, 0, 0));
gfx_draw_filled_rectangle(&fb, 20, 20, 120, 80, RGB565(255, 0, 0));
gfx_draw_line(&fb, 0, 0, 319, 479, RGB565(0, 255, 0));
gfx_draw_string(&fb, "ILI9488", 10, 100, false, 0, RGB565(255, 255, 255), ILI9488_FONT_8X12);

gfx_flush_dirty(&fb);
```

## Common mistakes

- Binding with insufficient storage size
- Drawing outside framebuffer bounds and assuming clipping for every API
- Forgetting to flush after drawing
- Reusing framebuffer memory after unbind/free

## Demo reference

The full integration example is in [demo/main.c](../demo/main.c).
