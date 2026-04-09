/**
 * @file ili9488_gfx.h
 * @brief ILI9488 graphics framebuffer API (user-managed framebuffer objects)
 *
 * This layer is intentionally higher-level than HAL/SPI:
 * - Owns in-memory drawing primitives.
 * - Uses HAL only when flushing to the physical display.
 * - Supports multiple framebuffer instances; caller chooses which one to use.
 *
 * AI Usage Disclaimer: This file was mostly outlined then generated using AI tools. See ./AI_chats for the full conversation logs as best as could be exported.
 *
 */

#ifndef ILI9488_GFX_H
#define ILI9488_GFX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/* ============================================================================
 * Display Defaults
 * ========================================================================== */

#define ILI9488_GFX_DEFAULT_WIDTH   320U
#define ILI9488_GFX_DEFAULT_HEIGHT  480U

/* RGB565 pack helper:
 * r: 0-255, g: 0-255, b: 0-255
 */
#define RGB565(r, g, b) \
    (uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
               (((uint16_t)(g) & 0xFCU) << 3) | \
               (((uint16_t)(b) & 0xF8U) >> 3))

/* ============================================================================
 * Framebuffer Object
 * ========================================================================== */

typedef struct {
    uint16_t *pixel_data;       /* Row-major RGB565 pixels */
    uint16_t width;         /* Visible width in pixels */
    uint16_t height;        /* Visible height in pixels */
    uint32_t stride; /* Pixels per row in memory (>= width) */

    /* Dirty bounds tracked by drawing ops for optional partial flushes.
     * If dirty == false, bounds are ignored.
     */
    bool dirty;
    uint16_t dirty_x1;
    uint16_t dirty_y1;
    uint16_t dirty_x2;
    uint16_t dirty_y2;
} gfx_framebuffer_t;


/* ============================================================================
 * Display-Direct Drawing (Bypass Framebuffer)
 * ========================================================================== */

/**
 * @brief Draw a filled rectangle with specified color
 * Coordinates are inclusive of both corners.
 * Will do nothing if dimensions exceed display bounds at all, or are ordered wrong.
 * @param x1 Leftmost column
 * @param y1 Top row
 * @param x2 Rightmost column
 * @param y2 Bottom row
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixels drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_filled_rectangle_direct(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2,
                               uint16_t color_rgb565);

/**
 * @brief Fill entire screen with solid color
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixels drawn successfully, false if coordinates out of bounds
 */
bool gfx_fill_screen_direct(uint16_t color_rgb565);

/**
 * @brief Draw a single pixel at (x, y)
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixel drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_pixel_direct(uint16_t x, uint16_t y, uint16_t color_rgb565);

/* ============================================================================
 * Framebuffer Lifecycle (User-managed Storage)
 * ========================================================================== */

/**
 * @brief Return required pixel count for width/height.
 * @return Number of pixels required for given dimensions, or 0 if invalid
 */
size_t gfx_framebuffer_required_pixels(uint16_t width, uint16_t height);

/**
 * @brief Return required byte count for width/height in RGB565.
 * @return Number of bytes required for given dimensions, or 0 if invalid
 */
size_t gfx_framebuffer_required_bytes(uint16_t width, uint16_t height);

/**
 * @brief Bind caller-provided pixel storage to a framebuffer object.
 *
 * @param framebuffer   Framebuffer object to initialize
 * @param pixel_storage Caller-owned RGB565 buffer
 * @param storage_pixels Number of uint16_t entries available in pixel_storage
 * @param width         Logical framebuffer width
 * @param height        Logical framebuffer height
 * @param stride_pixels Pixels per row in memory (0 => stride = width)
 * @return true on success, false on invalid args/capacity
 */
bool gfx_framebuffer_bind(gfx_framebuffer_t *framebuffer,
                          uint16_t *pixel_storage,
                          size_t storage_pixels,
                          uint16_t width,
                          uint16_t height,
                          uint32_t stride_pixels);

/**
 * @brief Clear framebuffer object metadata (does not free caller storage, should be done before calling).
 */
void gfx_framebuffer_unbind(gfx_framebuffer_t *framebuffer);

/* ============================================================================
 * Dirty Tracking Helpers
 * ========================================================================== */

/**
 * @brief Mark entire framebuffer dirty.
 * @param framebuffer Framebuffer object to mark dirty
 */
void gfx_mark_dirty_full(gfx_framebuffer_t *framebuffer);

/**
 * @brief Mark a rectangular region dirty, expanding existing dirty region if necessary.
 * Coordinates are inclusive and clipped to framebuffer bounds.
 * @param framebuffer Framebuffer object to mark dirty
 * @param x1 Leftmost column of dirty region
 * @param y1 Top row of dirty region
 * @param x2 Rightmost column of dirty region
 * @param y2 Bottom row of dirty region
 */
void gfx_mark_dirty_region(gfx_framebuffer_t *framebuffer,
                             uint16_t x1, uint16_t y1,
                             uint16_t x2, uint16_t y2);

/**
 * @brief Clear dirty state.
 * @param framebuffer Framebuffer object to clear dirty state
 */
void gfx_clear_dirty(gfx_framebuffer_t *framebuffer);

/**
 * @brief Query dirty flag.
 * @param framebuffer Framebuffer object to query
 * @return true if framebuffer is dirty, false if clean or invalid pointer
 */
bool gfx_is_dirty(const gfx_framebuffer_t *framebuffer);

/* ============================================================================
 * Drawing Primitives (Memory Only)
 * ========================================================================== */

/**
 * @brief Fill entire framebuffer with one RGB565 color.
 * @param framebuffer Framebuffer object to fill
 */
bool gfx_fill(gfx_framebuffer_t *framebuffer, uint16_t color_rgb565);

/**
 * @brief Draw one pixel with bounds check.
 * @param framebuffer Framebuffer object to modify
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixel drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_pixel(gfx_framebuffer_t *framebuffer,
                    uint16_t x,
                    uint16_t y,
                    uint16_t color_rgb565);

/**
 * @brief Draw a filled rectangle inclusive of both corners.
 * Coordinates are clipped to framebuffer bounds.
 * Will draw slim rectangle along the edge if fully out of bounds.
 * @param framebuffer Framebuffer object to modify
 * @param x1 Leftmost column of rectangle
 * @param y1 Top row of rectangle
 * @param x2 Rightmost column of rectangle
 * @param y2 Bottom row of rectangle
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixels drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_filled_rectangle(gfx_framebuffer_t *framebuffer,
                               uint16_t x1,
                               uint16_t y1,
                               uint16_t x2,
                               uint16_t y2,
                               uint16_t color_rgb565);

/**
 * @brief Draw a line using integer rasterization.
 * Coordinates outside bounds are clipped by implementation.
 * @param framebuffer Framebuffer object to modify
 * @param x1 Leftmost column of line start
 * @param y1 Top row of line start
 * @param x2 Rightmost column of line end
 * @param y2 Bottom row of line end
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixels drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_line(gfx_framebuffer_t *framebuffer,
                   int16_t x1,
                   int16_t y1,
                   int16_t x2,
                   int16_t y2,
                   uint16_t color_rgb565);

/* ============================================================================
 * Text Rendering (Memory Only)
 * ========================================================================== */

/**
 * @brief Draw a 1bpp bitmap where each set bit draws one pixel.
 * Bitmap is expected row-major, MSB-first in each byte.
 *
 * @param framebuffer Framebuffer object to modify
 * @param bitmap Pointer to monochrome bitmap data
 * @param x Leftmost destination column
 * @param y Top destination row
 * @param bitmap_width Bitmap width in pixels
 * @param bitmap_height Bitmap height in pixels
 * @param color_rgb565 RGB565 color for set bits
 * @return true if drawing completed, false on invalid args
 */
bool gfx_draw_mono_bitmap(gfx_framebuffer_t *framebuffer,
                          const uint8_t *bitmap,
                          uint16_t x,
                          uint16_t y,
                          uint16_t bitmap_width,
                          uint16_t bitmap_height,
                          uint16_t color_rgb565);

/**
 * @brief Draw one ASCII character using built-in font tables.
 *
 * @param framebuffer Framebuffer object to modify
 * @param c Character to draw
 * @param x Leftmost destination column
 * @param y Top destination row
 * @param color_rgb565 RGB565 color for glyph pixels
 * @param scale Integer glyph scale factor (1 = native size)
 * @return true if drawing completed, false on invalid args/unsupported input
 */
bool gfx_draw_char(gfx_framebuffer_t *framebuffer,
                   char c,
                   uint16_t x,
                   uint16_t y,
                   uint16_t color_rgb565,
                   uint8_t scale);

/**
 * @brief Draw a null-terminated string using built-in font tables.
 *
 * @param framebuffer Framebuffer object to modify
 * @param text Null-terminated string
 * @param x Leftmost destination column
 * @param y Top destination row
 * @param color_rgb565 RGB565 color for glyph pixels
 * @param scale Integer glyph scale factor (1 = native size)
 * @return true if drawing completed, false on invalid args
 */
bool gfx_draw_string(gfx_framebuffer_t *framebuffer,
                     const char *text,
                     uint16_t x,
                     uint16_t y,
                     uint16_t color_rgb565,
                     uint8_t scale);

/* ============================================================================
 * Display Flush (Graphics Layer -> HAL)
 * ========================================================================== */

/**
 * @brief Flush only a specified region.
 * Region is clipped to framebuffer bounds.
 * Resets dirty state on success, regardless of dirty region.
 * Always writes in 18-bit mode currently
 * @param framebuffer Framebuffer object to flush
 * @param x1 Leftmost column of region
 * @param y1 Top row of region
 * @param x2 Rightmost column of region
 * @param y2 Bottom row of region
 * @return true if region flushed successfully, false otherwise
 */
bool gfx_flush_region(gfx_framebuffer_t *framebuffer,
                      uint16_t x1,
                      uint16_t y1,
                      uint16_t x2,
                      uint16_t y2);

/**
 * @brief Flush full framebuffer to display. Must be RGB565 format
 * Always writes in 18-bit mode currently
 * @param framebuffer Framebuffer object to flush
 * @return true on successful transfer, false otherwise
 */
bool gfx_flush(gfx_framebuffer_t *framebuffer);

/**
 * @brief Flush dirty region if any, then clear dirty state on success.
 * Always writes in 18-bit mode currently
 * @param framebuffer Framebuffer object to flush
 * @return true if no dirty data existed or flush succeeded
 */
bool gfx_flush_dirty(gfx_framebuffer_t *framebuffer);

#endif /* ILI9488_GFX_H */