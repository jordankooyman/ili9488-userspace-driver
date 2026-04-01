/**
 * @file ili9488_gfx.c
 * @brief ILI9488 Graphics Interface
 *
 * Platform: Linux (Raspberry Pi 4B)
 *
 * References: 
 */

/**
 * @brief Draw a filled rectangle with specified color
 */
void gfx_draw_filled_rectangle(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2,
                               uint16_t color_rgb565)
{
    // Call HAL function directly
    hal_fill_rectangle_solid(x1, x2, y1, y2, color_rgb565);
}

/**
 * @brief Fill entire screen with solid color
 */
void gfx_fill_screen(uint16_t color_rgb565)
{
    gfx_draw_filled_rectangle(0, 0, 319, 479, color_rgb565);
}

/**
 * @brief Write entire framebuffer to display
 */
void gfx_put_framebuffer(const uint8_t *framebuffer, size_t buffer_size)
{
    // Assuming framebuffer is organized as RGB565 pixels
    hal_window_address_set(0, 319, 0, 479);
    hal_gram_write_start();
    hal_gram_write_pixels(framebuffer, 320 * 480, PIXEL_FORMAT_16BIT);
}

/**
 * @brief Draw a single pixel at (x, y)
 */
void gfx_draw_pixel(uint16_t x, uint16_t y, uint16_t color_rgb565)
{
    hal_window_address_set(x, x, y, y);
    hal_gram_write_start();
    uint8_t pixel_bytes[2] = {
        (uint8_t)((color_rgb565 >> 8) & 0xFF),
        (uint8_t)(color_rgb565 & 0xFF)
    };
    spi_transmit_bulkdata(pixel_bytes, 2);
}

/**
 * @brief Draw a line with specified color
 */
void gfx_draw_line(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2,
                               uint16_t color_rgb565)
{
    return;
}

/**
 * @brief Draw a filled rectangle with specified color to a given framebuffer
 */
void gfx_draw_filled_rectangle_framebuffer(const uint8_t *framebuffer, size_t buffer_size, 
                                           uint16_t x1, uint16_t y1,
                                           uint16_t x2, uint16_t y2,
                                           uint16_t color_rgb565)
{
    return;
}

/**
 * @brief Fill entire screen with solid color to a given framebuffer
 */
void gfx_fill_framebuffer(const uint8_t *framebuffer, size_t buffer_size, uint16_t color_rgb565)
{
    gfx_draw_filled_rectangle_framebuffer(framebuffer, buffer_size, 0, 0, 319, 479, color_rgb565);
}

/**
 * @brief Draw a single pixel at (x, y) to a given framebuffer
 */
void gfx_draw_pixel_framebuffer(const uint8_t *framebuffer, size_t buffer_size, uint16_t x, uint16_t y, uint16_t color_rgb565)
{
    return;
}

/**
 * @brief Draw a line with specified color to a given framebuffer
 */
void gfx_draw_line_framebuffer(const uint8_t *framebuffer, size_t buffer_size, 
                                     uint16_t x1, uint16_t y1,
                                     uint16_t x2, uint16_t y2,
                                     uint16_t color_rgb565)
{
    return;
}