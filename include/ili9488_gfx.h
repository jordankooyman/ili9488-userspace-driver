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
    uint16_t *pixels;       /* Row-major RGB565 pixels */
    uint16_t width;         /* Visible width in pixels */
    uint16_t height;        /* Visible height in pixels */
    uint32_t stride_pixels; /* Pixels per row in memory (>= width) */

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
 * Framebuffer Lifecycle (User-managed Storage)
 * ========================================================================== */

/**
 * @brief Return required pixel count for width/height.
 */
size_t gfx_framebuffer_required_pixels(uint16_t width, uint16_t height);

/**
 * @brief Return required byte count for width/height in RGB565.
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
 * @brief Clear framebuffer object metadata (does not free caller storage).
 */
void gfx_framebuffer_unbind(gfx_framebuffer_t *framebuffer);

/* ============================================================================
 * Dirty Tracking Helpers
 * ========================================================================== */

/**
 * @brief Mark entire framebuffer dirty.
 */
void gfx_mark_dirty_full(gfx_framebuffer_t *framebuffer);

/**
 * @brief Clear dirty state.
 */
void gfx_clear_dirty(gfx_framebuffer_t *framebuffer);

/**
 * @brief Query dirty flag.
 */
bool gfx_is_dirty(const gfx_framebuffer_t *framebuffer);

/* ============================================================================
 * Drawing Primitives (Memory Only)
 * ========================================================================== */

/**
 * @brief Fill entire framebuffer with one RGB565 color.
 */
bool gfx_fill(gfx_framebuffer_t *framebuffer, uint16_t color_rgb565);

/**
 * @brief Draw one pixel with bounds check.
 */
bool gfx_draw_pixel(gfx_framebuffer_t *framebuffer,
                    uint16_t x,
                    uint16_t y,
                    uint16_t color_rgb565);

/**
 * @brief Draw a filled rectangle inclusive of both corners.
 * Coordinates are clipped to framebuffer bounds.
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
 */
bool gfx_draw_line(gfx_framebuffer_t *framebuffer,
                   uint16_t x1,
                   uint16_t y1,
                   uint16_t x2,
                   uint16_t y2,
                   uint16_t color_rgb565);

/* ============================================================================
 * Display Flush (Graphics Layer -> HAL)
 * ========================================================================== */

/**
 * @brief Flush full framebuffer to display.
 *
 * Typical implementation:
 *   hal_window_address_set(0, width-1, 0, height-1)
 *   hal_gram_write_start()
 *   hal_gram_write_pixels(...)
 *
 * @return true on successful transfer, false otherwise
 */
bool gfx_flush(const gfx_framebuffer_t *framebuffer);

/**
 * @brief Flush only a specified region.
 * Region is clipped to framebuffer bounds.
 */
bool gfx_flush_region(const gfx_framebuffer_t *framebuffer,
                      uint16_t x1,
                      uint16_t y1,
                      uint16_t x2,
                      uint16_t y2);

/**
 * @brief Flush dirty region if any, then clear dirty state on success.
 * @return true if no dirty data existed or flush succeeded
 */
bool gfx_flush_dirty(gfx_framebuffer_t *framebuffer);

#endif /* ILI9488_GFX_H */