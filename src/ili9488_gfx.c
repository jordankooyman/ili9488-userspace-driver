/**
 * @file ili9488_gfx.c
 * @brief ILI9488 Graphics Interface, using RGB565 color format
 *
 * Platform: Linux (Raspberry Pi 4B)
 *
 * References: 
 */

#include "ili9488_gfx.h"
#include "ili9488_hal.h"
#include <stdlib.h>


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
                               uint16_t color_rgb565)
{
    if (x1 >= ILI9488_GFX_DEFAULT_WIDTH || x2 >= ILI9488_GFX_DEFAULT_WIDTH ||
        y1 >= ILI9488_GFX_DEFAULT_HEIGHT || y2 >= ILI9488_GFX_DEFAULT_HEIGHT) {
        return false; // Out of bounds, do nothing
    }
    if (x2 < x1 || y2 < y1) {
        return false; // Invalid rectangle, do nothing
    }
    // Call HAL function directly
    return hal_fill_rectangle_solid(x1, x2, y1, y2, color_rgb565);
}


/**
 * @brief Fill entire screen with solid color
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixels drawn successfully, false if coordinates out of bounds
 */
bool gfx_fill_screen_direct(uint16_t color_rgb565)
{
    return gfx_draw_filled_rectangle_direct(0, 0, ILI9488_GFX_DEFAULT_WIDTH - 1, ILI9488_GFX_DEFAULT_HEIGHT - 1, color_rgb565);
}


/**
 * @brief Draw a single pixel at (x, y)
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if pixel drawn successfully, false if coordinates out of bounds
 */
bool gfx_draw_pixel_direct(uint16_t x, uint16_t y, uint16_t color_rgb565)
{
    if (x >= ILI9488_GFX_DEFAULT_WIDTH || y >= ILI9488_GFX_DEFAULT_HEIGHT) {
        return false; // Out of bounds, do nothing
    }
    return gfx_draw_filled_rectangle_direct(x, y, x, y, color_rgb565);
}


/* ============================================================================
 * Framebuffer Lifecycle (User-managed Storage)
 * ========================================================================== */

/**
 * @brief Return required pixel count for width/height.
 * @return Number of pixels required for given dimensions, or 0 if invalid
 */
size_t gfx_framebuffer_required_pixels(uint16_t width, uint16_t height)
{
    if (width > ILI9488_GFX_DEFAULT_WIDTH || height > ILI9488_GFX_DEFAULT_HEIGHT) {
        return 0; // Invalid dimensions
    }
    return width * height; // If either dimension is zero, this will correctly return 0
}


/**
 * @brief Return required byte count for width/height in RGB565.
 * @return Number of bytes required for given dimensions, or 0 if invalid
 */
size_t gfx_framebuffer_required_bytes(uint16_t width, uint16_t height)
{
    size_t pixel_count = gfx_framebuffer_required_pixels(width, height);
    return pixel_count * 2; // 2 bytes per pixel for RGB565 (If invalid, this will correctly return 0)
}


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
                          uint32_t stride_pixels)
{
    // Validate pointers
    if (framebuffer == NULL || pixel_storage == NULL) {
        return false;
    }

    // Validate dimensions (must be > 0 and within display limits)
    if (width == 0 || height == 0 || width > ILI9488_GFX_DEFAULT_WIDTH || height > ILI9488_GFX_DEFAULT_HEIGHT) {
        return false;
    }

    // Set stride to width if not specified
    if (stride_pixels == 0) {
        stride_pixels = width;
    }

    // Validate dimensions and storage capacity
    if (stride_pixels < width || storage_pixels < (size_t)(stride_pixels * height)) {
        return false; // Not enough storage for given dimensions and stride
    }

    // Initialize framebuffer metadata
    framebuffer->pixel_data = pixel_storage;
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->stride = stride_pixels;
    framebuffer->dirty = false; // Initially clean, no data
    
    return true;
}


/**
 * @brief Clear framebuffer object metadata (does not free caller storage, should be done before calling).
 */
void gfx_framebuffer_unbind(gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return;
    }

    // Clear metadata but do not free pixel storage (caller-managed)
    framebuffer->pixel_data = NULL;
    framebuffer->width = 0;
    framebuffer->height = 0;
    framebuffer->stride = 0;
    framebuffer->dirty = false;

    return;
}


/* ============================================================================
 * Dirty Tracking Helpers
 * ========================================================================== */

/**
 * @brief Mark entire framebuffer dirty.
 * @param framebuffer Framebuffer object to mark dirty
 */
void gfx_mark_dirty_full(gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return;
    }
    if (framebuffer->pixel_data == NULL) {
        return; // Unbound framebuffer, do nothing
    }

    framebuffer->dirty = true;
    framebuffer->dirty_x1 = 0;
    framebuffer->dirty_y1 = 0;
    framebuffer->dirty_x2 = framebuffer->width > 0 ? framebuffer->width - 1 : 0;
    framebuffer->dirty_y2 = framebuffer->height > 0 ? framebuffer->height - 1 : 0;

    return;
}


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
                             uint16_t x2, uint16_t y2)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return;
    }
    if (framebuffer->pixel_data == NULL) {
        return; // Unbound framebuffer, do nothing
    }

    // Check coordinates against framebuffer bounds
    if (x1 >= framebuffer->width || y1 >= framebuffer->height ||
        x2 >= framebuffer->width || y2 >= framebuffer->height) {
        return; // Out of bounds, do nothing
    }
    if (x2 < x1 || y2 < y1) {
        return; // Invalid rectangle, do nothing
    }

    if (!framebuffer->dirty) {
        // First dirty region
        framebuffer->dirty_x1 = x1;
        framebuffer->dirty_y1 = y1;
        framebuffer->dirty_x2 = x2;
        framebuffer->dirty_y2 = y2;
        framebuffer->dirty = true;
    }
    else {
        // Expand existing dirty region to include new region
        if (x1 < framebuffer->dirty_x1) framebuffer->dirty_x1 = x1;
        if (y1 < framebuffer->dirty_y1) framebuffer->dirty_y1 = y1;
        if (x2 > framebuffer->dirty_x2) framebuffer->dirty_x2 = x2;
        if (y2 > framebuffer->dirty_y2) framebuffer->dirty_y2 = y2;
    }

    return;
}


/**
 * @brief Clear dirty state.
 * @param framebuffer Framebuffer object to clear dirty state
 */
void gfx_clear_dirty(gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return;
    }
    if (framebuffer->pixel_data == NULL) {
        return; // Unbound framebuffer, do nothing
    }

    framebuffer->dirty = false;

    return;
}


/**
 * @brief Query dirty flag.
 * @param framebuffer Framebuffer object to query
 * @return true if framebuffer is dirty, false if clean or invalid pointer
 */
bool gfx_is_dirty(const gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false; // Invalid pointer, treat as clean
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    return framebuffer->dirty;
}


/* ============================================================================
 * Drawing Primitives (Memory Only)
 * ========================================================================== */

/**
 * @brief Fill entire framebuffer with one RGB565 color.
 * @param framebuffer Framebuffer object to fill
 */
bool gfx_fill(gfx_framebuffer_t *framebuffer, uint16_t color_rgb565)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Fill pixel data with color
    uint32_t row;
    for (row = 0; row < framebuffer->height; row++) {
        uint32_t col;
        for (col = 0; col < framebuffer->width; col++) {
            framebuffer->pixel_data[row * framebuffer->stride + col] = color_rgb565;
        }
    }

    gfx_mark_dirty_full(framebuffer);
    return true;
}


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
                    uint16_t color_rgb565)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Validate coordinates
    if (x >= framebuffer->width || y >= framebuffer->height) {
        return false; // Out of bounds, do nothing
    }

    // Update Pixel
    framebuffer->pixel_data[y * framebuffer->stride + x] = color_rgb565;

    // Update dirty region
    gfx_mark_dirty_region(framebuffer, x, y, x, y);

    return true;
}


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
                               uint16_t color_rgb565)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Validate/clip coordinates
    if (x1 >= framebuffer->width) {x1 = framebuffer->width - 1;}
    if (y1 >= framebuffer->height) {y1 = framebuffer->height - 1;}
    if (x2 >= framebuffer->width) {x2 = framebuffer->width - 1;}
    if (y2 >= framebuffer->height) {y2 = framebuffer->height - 1;}
    if (x2 < x1) { uint16_t temp = x1; x1 = x2; x2 = temp; } // Flip if necessary to ensure x1 <= x2
    if (y2 < y1) { uint16_t temp = y1; y1 = y2; y2 = temp; } // Flip if necessary to ensure y1 <= y2

    // Draw filled rectangle
    uint32_t row;
    for (row = y1; row <= y2; row++) {
        uint32_t col;
        for (col = x1; col <= x2; col++) {
            framebuffer->pixel_data[row * framebuffer->stride + col] = color_rgb565;
        }
    }

    // Update dirty region
    gfx_mark_dirty_region(framebuffer, x1, y1, x2, y2);

    return true;
}


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
                   uint16_t color_rgb565)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Ensure line is on the screen at least partially
    if (x1 >= framebuffer->width && x2 >= framebuffer->width) {
        return false; // Line is completely to the right of the screen
    }
    if (y1 >= framebuffer->height && y2 >= framebuffer->height) {
        return false; // Line is completely below the screen
    }
    if (x1 < 0 && x2 < 0) {
        return false; // Line is completely to the left of the screen
    }
    if (y1 < 0 && y2 < 0) {
        return false; // Line is completely above the screen
    }

    // Implement Bresenham's line algorithm with clipping to framebuffer bounds 
    // Note: Copilot generated using inline chat with the comment as prompt, manually reviewed and edited
    int32_t dx = abs(x2 - x1);
    int32_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx - dy;
    
    int16_t x = x1;
    int16_t y = y1;
    
    while (1) {
        // Clip and draw pixel if within bounds
        if (x >= 0 && x < framebuffer->width && y >= 0 && y < framebuffer->height) {
            // Update Pixel
            framebuffer->pixel_data[y * framebuffer->stride + x] = color_rgb565;
        }
        
        // Check if endpoint reached
        if (x == x2 && y == y2) {
            break;
        }
        
        // Calculate error and update coordinates
        int32_t err2 = 2 * err;
        if (err2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (err2 < dx) {
            err += dx;
            y += sy;
        }
    }    
    
    // Track bounding box for dirty region
    int16_t x_min = (x1 < x2) ? x1 : x2;
    int16_t x_max = (x1 > x2) ? x1 : x2;
    int16_t y_min = (y1 < y2) ? y1 : y2;
    int16_t y_max = (y1 > y2) ? y1 : y2;

    // Roughly clip to framebuffer bounds if line extends outside
    x_min = (x_min < 0) ? 0 : x_min;
    x_max = (x_max < framebuffer->width) ? x_max : framebuffer->width - 1;
    y_min = (y_min < 0) ? 0 : y_min;
    y_max = (y_max < framebuffer->height) ? y_max : framebuffer->height - 1;
    
    // Update dirty region to include entire line bounding box
    gfx_mark_dirty_region(framebuffer, (uint16_t)x_min, (uint16_t)y_min, (uint16_t)x_max, (uint16_t)y_max);
    
    return true;
}


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
                      uint16_t y2)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Clip region to framebuffer bounds
    if (x1 >= framebuffer->width || y1 >= framebuffer->height) {
        return false; // Region completely outside bounds
    }
    if (x2 >= framebuffer->width) x2 = framebuffer->width - 1;
    if (y2 >= framebuffer->height) y2 = framebuffer->height - 1;
    if (x2 < x1 || y2 < y1) {
        return false; // Invalid region
    }

    // Set address window for region
    if (!hal_window_address_set(x1, x2, y1, y2)) {
        return false; // Failed to set address window
    }

    // Start GRAM write sequence
    if (!hal_gram_write_start()) {
        return false; // Failed to start GRAM write
    }

    // Calculate region dimensions
    uint16_t region_width = x2 - x1 + 1;
    uint16_t region_height = y2 - y1 + 1;
    uint32_t pixel_count = (uint32_t)(region_width * region_height);

    uint8_t *buffer_18bit = (uint8_t *)malloc(pixel_count * 3);
    if (buffer_18bit == NULL) {
        return false; // Memory allocation failed
    }

    const uint16_t *pixel_data_16bit = (const uint16_t *)framebuffer->pixel_data;
    uint32_t buffer_index = 0;
    
    // Extract and convert only the specified region
    for (uint16_t y = y1; y <= y2; y++) {
        for (uint16_t x = x1; x <= x2; x++) {
            uint32_t pixel_offset = (uint32_t)(y * framebuffer->stride + x);
            uint16_t rgb565 = pixel_data_16bit[pixel_offset];
            
            // Extract RGB565 components
            uint8_t r = (rgb565 >> 11) & 0x1F;  // 5 bits
            uint8_t g = (rgb565 >> 5) & 0x3F;   // 6 bits
            uint8_t b = rgb565 & 0x1F;          // 5 bits
            
            // Convert to 8-bit components
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            
            // Store as 18-bit (3-byte) format
            buffer_18bit[buffer_index * 3] = r;
            buffer_18bit[buffer_index * 3 + 1] = g;
            buffer_18bit[buffer_index * 3 + 2] = b;
            buffer_index++;
        }
    }

    // Write 18-bit pixel data to GRAM
    bool write_success = hal_gram_write_pixels(buffer_18bit,
                                      pixel_count,
                                      PIXEL_FORMAT_18BIT);
    free(buffer_18bit);

    // Clear dirty state if write successful
    if (write_success) {
        gfx_clear_dirty(framebuffer);
    }

    return write_success;
}


/**
 * @brief Flush full framebuffer to display. Must be RGB565 format
 * Always writes in 18-bit mode currently
 * @param framebuffer Framebuffer object to flush
 * @return true on successful transfer, false otherwise
 */
bool gfx_flush(gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Flush with full region of framebuffer (handles clipping internally)
    return gfx_flush_region(framebuffer, 0, 0, framebuffer->width - 1, framebuffer->height - 1);
}


/**
 * @brief Flush dirty region if any, then clear dirty state on success.
 * Always writes in 18-bit mode currently
 * @param framebuffer Framebuffer object to flush
 * @return true if no dirty data existed or flush succeeded
 */
bool gfx_flush_dirty(gfx_framebuffer_t *framebuffer)
{
    // Validate pointer
    if (framebuffer == NULL) {
        return false;
    }
    if (framebuffer->pixel_data == NULL) {
        return false; // Unbound framebuffer, do nothing
    }

    // Get Dirty Region
    if (gfx_is_dirty(framebuffer)) {
        if (!gfx_flush_region(framebuffer, framebuffer->dirty_x1, framebuffer->dirty_y1, framebuffer->dirty_x2, framebuffer->dirty_y2)) {
            return false; // Flush failed
        }
    }

    // No dirty data or flush succeeded
    return true; 
}