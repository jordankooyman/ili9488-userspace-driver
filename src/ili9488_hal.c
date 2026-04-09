/**
 * @file ili9488_hal.c
 * @brief ILI9488 Hardware Abstraction Layer (HAL) Implementation
 *
 * Implements display commands by coordinating SPI and GPIO operations.
 * Each function encodes the exact sequence from the ILI9488 datasheet.
 *
 * References: ILI9488 Datasheet, Section 5 (Command Description)
 * Cross-checked against TFT_eSPI's working ILI9488 SPI init sequence.
 *
 * AI Usage Disclaimer: This file was mostly outlined then generated using AI tools. See ./AI_chats for the full conversation logs as best as could be exported.
 */

#include "ili9488_hal.h"
#include "ili9488_spi.h"
#include <string.h>

/* ============================================================================
 * Internal Helper
 * ========================================================================== */

/**
 * @brief Split a 16-bit address into MSB/LSB bytes (big-endian, as required by
 *        all multi-byte address parameters in the ILI9488 command set).
 * @param address 16-bit address value to split.
 * @param msb_byte Output pointer for the most-significant byte.
 * @param lsb_byte Output pointer for the least-significant byte.
 * @return None.
 */
static inline void _address_to_bytes(uint16_t address,
                                      uint8_t *msb_byte,
                                      uint8_t *lsb_byte)
{
    *msb_byte = (uint8_t)((address >> 8) & 0xFF);
    *lsb_byte = (uint8_t)(address & 0xFF);
}

/* ============================================================================
 * Cached Register State
 * ========================================================================== */

/** Last written value of Memory Access Control register (0x36) */
static uint8_t g_current_mac_register = 0x00;

/** Last configured pixel format */
static hal_pixel_format_t g_current_pixel_format = PIXEL_FORMAT_16BIT;

/* ============================================================================
 * Initialization & Configuration
 * ========================================================================== */

/**
 * @brief Initialize the display hardware, controller state, and key runtime settings.
 * @param pixel_format Transfer pixel format to configure on the panel.
 * @param rotation Initial display rotation and memory mapping mode.
 * @return true if initialization succeeds end-to-end; otherwise false.
 */
bool hal_display_initialize(hal_pixel_format_t pixel_format,
                            hal_rotation_t rotation)
{
    /* Hardware driver initialization */
    if (!spi_bus_initialize("/dev/spidev0.0", 10000000)) {
        // fprintf(stderr, "Error: failed to open SPI bus\n");
        return false;
    }

    if (!spi_gpio_initialize(GPIO_RESET, GPIO_STATE_HIGH)) {
        // fprintf(stderr, "Error: failed to initialise RESET GPIO (BCM 24)\n");
        spi_bus_deinitialize();
        return false;
    }

    if (!spi_gpio_initialize(GPIO_DC_SELECT, GPIO_STATE_HIGH)) {
        // fprintf(stderr, "Error: failed to initialise D/C GPIO (BCM 25)\n");
        spi_gpio_deinitialize(GPIO_RESET);
        spi_bus_deinitialize();
        return false;
    }

    // printf("GPIO and SPI ready\n");

    /* Hardware reset sequence — Datasheet Section 4 (Power On/Off Sequence) */
    spi_gpio_set_state(GPIO_RESET, GPIO_STATE_LOW);
    spi_delay_ms(1);
    spi_gpio_set_state(GPIO_RESET, GPIO_STATE_HIGH);
    spi_delay_ms(120);

    /* Software reset — Datasheet Section 5.2.2 (0x01) */
    if (!spi_transmit_command(0x01)) {hal_display_deinitialize(); return false;}
    spi_delay_ms(5);

    /* Sleep out — Datasheet Section 5.2.13 (0x11) */
    if (!spi_transmit_command(0x11)) {hal_display_deinitialize(); return false;}
    spi_delay_ms(120);

    /* Pixel format and rotation */
    if (!hal_pixel_format_set(pixel_format)) {hal_display_deinitialize(); return false;}
    if (!hal_display_rotation_set(rotation,
                                   HORIZONAL_DIRECTION_DEFAULT,
                                   VERTICAL_DIRECTION_DEFAULT)) {hal_display_deinitialize(); return false;}

    /* Power supply configuration — Datasheet Section 5.3.12–5.3.17 */
    if (!hal_power_gvdd_set(0x17)) {hal_display_deinitialize(); return false;}
    if (!hal_power_vci_set(0x41)) {hal_display_deinitialize(); return false;}
    if (!hal_power_vgh_vgl_set(0x0A, 0x0A)) {hal_display_deinitialize(); return false;}
    if (!hal_power_vcomh_set(0x30)) {hal_display_deinitialize(); return false;}

    /* Working TFT_eSPI path also programs the 0xB0/0xB1/0xB4/0xB6/0xB7 registers. */
    if (!hal_oscillator_frequency_set(0x00)) {hal_display_deinitialize(); return false;}
    if (!hal_frame_rate_set(0xA0)) {hal_display_deinitialize(); return false;}
    if (!hal_display_inversion_control_set(0x02)) {hal_display_deinitialize(); return false;}
    if (!hal_display_function_control_set(0x02, 0x02, 0x3B)) {hal_display_deinitialize(); return false;}
    if (!hal_entry_mode_set(0xC6)) {hal_display_deinitialize(); return false;}

    /* Gamma — Datasheet Section 5.2.26 (0x26) */
    if (!hal_gamma_curve_select(GAMMA_CURVE_3)) {hal_display_deinitialize(); return false;}

    /* Full-screen address window */
    if (!hal_column_address_set(0, 319)) {hal_display_deinitialize(); return false;}
    if (!hal_row_address_set(0, 479)) {hal_display_deinitialize(); return false;}

    /* Normal display mode — Datasheet Section 5.2.15 (0x13) */
    if (!hal_normal_display_mode_on()) {hal_display_deinitialize(); return false;}

    /* Display on — Datasheet Section 5.2.21 (0x29) */
    if (!hal_display_output_control(true)) {hal_display_deinitialize(); return false;}
    spi_delay_ms(25);

    return true;
}

/**
 * @brief Power down the display path and release transport resources.
 * @param None.
 * @return true if deinitialization succeeds; otherwise false.
 */
bool hal_display_deinitialize(void)
{
    if (!hal_display_output_control(false)) return false;
    spi_delay_ms(5);
    if (!hal_power_sleep_mode_set(true)) return false;

    /* Hardware Driver deinitialize */
    spi_gpio_deinitialize(GPIO_DC_SELECT);
    spi_gpio_deinitialize(GPIO_RESET);
    spi_bus_deinitialize();

    return true;
}

/* ============================================================================
 * System Control Commands
 * ========================================================================== */

/**
 * @brief Issue a software reset command to the controller.
 * @param reset_scope Reset scope selector (reserved by current implementation).
 * @return true if the reset command transaction succeeds; otherwise false.
 */
bool hal_system_reset(hal_reset_scope_t reset_scope)
{
    /* Software Reset — Datasheet Section 5.2.2 (0x01) */
    (void)reset_scope;

    if (!spi_transmit_command(0x01)) return false;
    spi_delay_ms(5);
    return true;
}

/**
 * @brief Enter or exit sleep mode.
 * @param enable true to enter sleep mode, false to exit sleep mode.
 * @return true if the sleep state change succeeds; otherwise false.
 */
bool hal_power_sleep_mode_set(bool enable)
{
    if (enable) {
        /* Sleep In — Datasheet Section 5.2.12 (0x10) */
        if (!spi_transmit_command(0x10)) return false;
        spi_delay_ms(5);
    } else {
        /* Sleep Out — Datasheet Section 5.2.13 (0x11) */
        if (!spi_transmit_command(0x11)) return false;
        spi_delay_ms(120);
    }
    return true;
}

/**
 * @brief Exit sleep mode with required wake timing delay.
 * @param None.
 * @return true if the wake command succeeds; otherwise false.
 */
bool hal_power_sleep_exit(void)
{
    /* Sleep Out — Datasheet Section 5.2.13 (0x11) */
    if (!spi_transmit_command(0x11)) return false;
    spi_delay_ms(120);
    return true;
}

/**
 * @brief Enable or disable display output.
 * @param enabled true to send Display ON, false to send Display OFF.
 * @return true if the command succeeds; otherwise false.
 */
bool hal_display_output_control(bool enabled)
{
    /* Display On (0x29) / Display Off (0x28) — Datasheet Section 5.2.20–5.2.21 */
    uint8_t command = enabled ? 0x29 : 0x28;
    return spi_transmit_command(command);
}

/**
 * @brief Apply one of the high-level power state modes.
 * @param power_state Requested abstract power state.
 * @return true if the mode transition succeeds; otherwise false.
 */
bool hal_power_set_state(hal_power_state_t power_state)
{
    switch (power_state) {
        case POWER_STATE_SLEEP:
            return hal_power_sleep_mode_set(true);

        case POWER_STATE_NORMAL:
            hal_power_sleep_exit();
            return hal_display_output_control(true);

        case POWER_STATE_DISPLAY_OFF:
            return hal_display_output_control(false);

        default:
            return false;
    }
}

/* ============================================================================
 * Display Configuration Commands
 * ========================================================================== */

/**
 * @brief Configure panel transfer pixel format.
 * @param pixel_format Pixel format enum value to write.
 * @return true if the command succeeds; otherwise false.
 */
bool hal_pixel_format_set(hal_pixel_format_t pixel_format)
{
    /* Interface Pixel Format — Datasheet Section 5.2.34 (0x3A)
     * 0x03 = 12-bit, 0x05 = 16-bit RGB565, 0x06 = 18-bit */
    uint8_t param = (uint8_t)pixel_format;

    if (!spi_transmit_command(0x3A)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    g_current_pixel_format = pixel_format;
    return true;
}

/**
 * @brief Configure display rotation plus optional horizontal/vertical flips.
 * @param rotation Base rotation preset.
 * @param horizontal_flip Horizontal direction override.
 * @param vertical_flip Vertical direction override.
 * @return true if register programming succeeds; otherwise false.
 */
bool hal_display_rotation_set(hal_rotation_t rotation,
                              hal_horizontal_direction_t horizontal_flip,
                              hal_vertical_direction_t vertical_flip)
{
    /* Memory Access Control — Datasheet Section 5.2.30 (0x36)
     * Bit 7 (MY): row direction, Bit 6 (MX): col direction, Bit 5 (MV): row/col swap
     * Bit 4 (ML): vertical order, Bit 3 (BGR): colour order.
     * The base values below mirror TFT_eSPI's working ILI9488 rotation table. */
    uint8_t mac_byte;

    switch (rotation) {
        case ROTATION_0_NORMAL:             mac_byte = 0x48; break;
        case ROTATION_90_CLOCKWISE:         mac_byte = 0x28; break;
        case ROTATION_180_INVERSE:          mac_byte = 0x88; break;
        case ROTATION_270_COUNTERCLOKWISE:  mac_byte = 0xE8; break;
        default: return false;
    }

    if (horizontal_flip == HORIZONAL_DIRECTION_REVERSED) mac_byte ^= 0x40;
    if (vertical_flip   == VERTICAL_DIRECTION_REVERSED)  mac_byte ^= 0x80;

    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;
    return true;
}

/**
 * @brief Configure RGB/BGR byte order bit in MADCTL.
 * @param byte_order Desired color byte order mode.
 * @return true if register programming succeeds; otherwise false.
 */
bool hal_color_byte_order_set(hal_byte_order_t byte_order)
{
    /* Memory Access Control — Bit 3 (BGR) — Datasheet Section 5.2.30 (0x36) */
    uint8_t mac_byte = g_current_mac_register;

    if (byte_order == BYTE_ORDER_BGR) {
        mac_byte |= 0x08;
    } else {
        mac_byte &= (uint8_t)~0x08;
    }

    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;
    return true;
}

/**
 * @brief Configure transfer stepping mode in MADCTL.
 * @param transfer_mode Desired transfer mode enum.
 * @return true if register programming succeeds; otherwise false.
 */
bool hal_transfer_mode_set(hal_transfer_mode_t transfer_mode)
{
    /* Memory Access Control — Bit 5 (MV) — Datasheet Section 5.2.30 (0x36) */
    uint8_t mac_byte = g_current_mac_register;

    if (transfer_mode == TRANSFER_MODE_PAGE_ADDRESS) {
        mac_byte |= 0x20;
    } else {
        mac_byte &= (uint8_t)~0x20;
    }

    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;
    return true;
}

/**
 * @brief Read the cached display configuration byte maintained by HAL.
 * @param config_byte Output pointer receiving cached MADCTL value.
 * @return true if output pointer is valid; otherwise false.
 */
bool hal_display_config_read(uint8_t *config_byte)
{
    if (config_byte == NULL) return false;
    *config_byte = g_current_mac_register;
    return true;
}

/* ============================================================================
 * Memory Address & Access Commands
 * ========================================================================== */

/**
 * @brief Set active column address range.
 * @param start_column Inclusive start column.
 * @param end_column Inclusive end column.
 * @return true if command transaction succeeds; otherwise false.
 */
bool hal_column_address_set(uint16_t start_column, uint16_t end_column)
{
    /* Column Address Set — Datasheet Section 5.2.22 (0x2A)
     * Params: [SC15:SC8][SC7:SC0][EC15:EC8][EC7:EC0] */
    uint8_t params[4];
    _address_to_bytes(start_column, &params[0], &params[1]);
    _address_to_bytes(end_column,   &params[2], &params[3]);

    if (!spi_transmit_command(0x2A)) return false;
    if (!spi_transmit_data(params, 4)) return false;
    return true;
}

/**
 * @brief Set active row address range.
 * @param start_row Inclusive start row.
 * @param end_row Inclusive end row.
 * @return true if command transaction succeeds; otherwise false.
 */
bool hal_row_address_set(uint16_t start_row, uint16_t end_row)
{
    /* Page Address Set — Datasheet Section 5.2.23 (0x2B)
     * Params: [SP15:SP8][SP7:SP0][EP15:EP8][EP7:EP0] */
    uint8_t params[4];
    _address_to_bytes(start_row, &params[0], &params[1]);
    _address_to_bytes(end_row,   &params[2], &params[3]);

    if (!spi_transmit_command(0x2B)) return false;
    if (!spi_transmit_data(params, 4)) return false;
    return true;
}

/**
 * @brief Set both column and row address ranges for a drawing window.
 * @param x_start Inclusive start column.
 * @param x_end Inclusive end column.
 * @param y_start Inclusive start row.
 * @param y_end Inclusive end row.
 * @return true if both address commands succeed; otherwise false.
 */
bool hal_window_address_set(uint16_t x_start, uint16_t x_end,
                            uint16_t y_start, uint16_t y_end)
{
    if (!hal_column_address_set(x_start, x_end)) return false;
    if (!hal_row_address_set(y_start, y_end)) return false;
    return true;
}

/**
 * @brief Send memory-write start command.
 * @param None.
 * @return true if command succeeds; otherwise false.
 */
bool hal_gram_write_start(void)
{
    /* Memory Write — Datasheet Section 5.2.24 (0x2C)
     * Subsequent data bytes are written sequentially to GRAM */
    return spi_transmit_command(0x2C);
}

/**
 * @brief Send memory-read start command.
 * @param None.
 * @return true if command succeeds; otherwise false.
 */
bool hal_gram_read_start(void)
{
    /* Memory Read — Datasheet Section 5.2.25 (0x2E) */
    return spi_transmit_command(0x2E);
}

/* ============================================================================
 * Pixel Data Transmission
 * ========================================================================== */

/**
 * @brief Stream pixel payload to GRAM in the selected transfer format.
 * @param pixel_buffer Pointer to pixel payload bytes.
 * @param pixel_count Number of pixels represented by pixel_buffer.
 * @param pixel_format Transfer format describing bytes per pixel.
 * @return true if transmit succeeds; otherwise false.
 */
bool hal_gram_write_pixels(const uint8_t *pixel_buffer,
                           uint32_t pixel_count,
                           hal_pixel_format_t pixel_format)
{
    if (pixel_buffer == NULL || pixel_count == 0) return false;

    size_t bytes_per_pixel;
    switch (pixel_format) {
        case PIXEL_FORMAT_16BIT: bytes_per_pixel = 2; break;
        case PIXEL_FORMAT_18BIT: bytes_per_pixel = 3; break;
        case PIXEL_FORMAT_12BIT:
        default:
            return false;
    }

    return spi_transmit_bulkdata(pixel_buffer, (size_t)pixel_count * bytes_per_pixel);
}

/**
 * @brief Read pixel payload from GRAM in the selected transfer format.
 * @param pixel_buffer Destination buffer for received pixel bytes.
 * @param pixel_count Number of pixels to read.
 * @param pixel_format Transfer format describing bytes per pixel.
 * @return true if receive succeeds; otherwise false.
 */
bool hal_gram_read_pixels(uint8_t *pixel_buffer,
                          uint32_t pixel_count,
                          hal_pixel_format_t pixel_format)
{
    if (pixel_buffer == NULL || pixel_count == 0) return false;

    size_t bytes_per_pixel;
    switch (pixel_format) {
        case PIXEL_FORMAT_16BIT: bytes_per_pixel = 2; break;
        case PIXEL_FORMAT_18BIT: bytes_per_pixel = 3; break;
        case PIXEL_FORMAT_12BIT:
        default:
            return false;
    }

    return spi_receive_data(pixel_buffer,
                            (size_t)pixel_count * bytes_per_pixel);
}

/**
 * @brief Fill one rectangular window with a single RGB565 color.
 * @param x_start Inclusive start column.
 * @param x_end Inclusive end column.
 * @param y_start Inclusive start row.
 * @param y_end Inclusive end row.
 * @param color_rgb565 RGB565 color value to write.
 * @return true if fill operation completes; otherwise false.
 */
bool hal_fill_rectangle_solid(uint16_t x_start, uint16_t x_end,
                              uint16_t y_start, uint16_t y_end,
                              uint16_t color_rgb565)
{
    if (!hal_window_address_set(x_start, x_end, y_start, y_end)) return false;
    if (!hal_gram_write_start()) return false;
    spi_delay_ms(1);

    uint32_t width  = (uint32_t)(x_end - x_start) + 1;
    uint32_t height = (uint32_t)(y_end - y_start) + 1;

    size_t bytes_per_pixel;
    if (g_current_pixel_format == PIXEL_FORMAT_16BIT) {
        bytes_per_pixel = 2;
    } else if (g_current_pixel_format == PIXEL_FORMAT_18BIT) {
        bytes_per_pixel = 3;
    } else {
        return false;
    }

    if (width > 320U) return false;

    /* Build one scanline and reuse it for each row transfer. */
    uint8_t row_buf[320 * 3];
    uint32_t i;
    if (bytes_per_pixel == 2) {
        uint8_t color_hi = (uint8_t)((color_rgb565 >> 8) & 0xFF);
        uint8_t color_lo = (uint8_t)(color_rgb565 & 0xFF);

        for (i = 0; i < width; i++) {
            row_buf[i * 2]     = color_hi;
            row_buf[i * 2 + 1] = color_lo;
        }
    } else {
        /* Expand RGB565 to 8-bit channels for 18-bit (3-byte) writes. */
        uint8_t r8 = (uint8_t)((((color_rgb565 >> 11) & 0x1F) << 3) | (((color_rgb565 >> 11) & 0x1F) >> 2));
        uint8_t g8 = (uint8_t)((((color_rgb565 >> 5) & 0x3F) << 2) | (((color_rgb565 >> 5) & 0x3F) >> 4));
        uint8_t b8 = (uint8_t)(((color_rgb565 & 0x1F) << 3) | ((color_rgb565 & 0x1F) >> 2));

        for (i = 0; i < width; i++) {
            row_buf[i * 3]     = r8;
            row_buf[i * 3 + 1] = g8;
            row_buf[i * 3 + 2] = b8;
        }
    }

    uint32_t row;
    for (row = 0; row < height; row++) {
        if (!spi_transmit_bulkdata(row_buf, width * bytes_per_pixel)) return false;
    }

    return true;
}

/* ============================================================================
 * Power Supply & Voltage Control
 * ========================================================================== */

/**
 * @brief Program GVDD setting.
 * @param gvdd_voltage Controller register value for GVDD.
 * @return true if command succeeds; otherwise false.
 */
bool hal_power_gvdd_set(uint8_t gvdd_voltage)
{
    /* Power Control 1 — Datasheet Section 5.3.12 (0xC0) */
    if (!spi_transmit_command(0xC0)) return false;
    if (!spi_transmit_data(&gvdd_voltage, 1)) return false;
    return true;
}

/**
 * @brief Program VCI setting.
 * @param vci_voltage Controller register value for VCI.
 * @return true if command succeeds; otherwise false.
 */
bool hal_power_vci_set(uint8_t vci_voltage)
{
    /* Power Control 2 — Datasheet Section 5.3.13 (0xC1) */
    if (!spi_transmit_command(0xC1)) return false;
    if (!spi_transmit_data(&vci_voltage, 1)) return false;
    return true;
}

/**
 * @brief Program VGH and VGL settings.
 * @param vgh_voltage Controller register value for positive gate voltage.
 * @param vgl_voltage Controller register value for negative gate voltage.
 * @return true if command succeeds; otherwise false.
 */
bool hal_power_vgh_vgl_set(uint8_t vgh_voltage, uint8_t vgl_voltage)
{
    /* VCOM Control — Datasheet Section 5.3.17 (0xC5)
     * Byte 0: VGH (positive gate driver), Byte 1: VGL (negative gate driver) */
    uint8_t params[2] = {vgh_voltage, vgl_voltage};

    if (!spi_transmit_command(0xC5)) return false;
    if (!spi_transmit_data(params, 2)) return false;
    return true;
}

/**
 * @brief Program VCOMH setting.
 * @param vcomh_voltage Controller register value for VCOMH.
 * @return true if command succeeds; otherwise false.
 */
bool hal_power_vcomh_set(uint8_t vcomh_voltage)
{
    if (!spi_transmit_command(0xBB)) return false;
    if (!spi_transmit_data(&vcomh_voltage, 1)) return false;
    return true;
}

/* ============================================================================
 * Display Timing & Frequency Control
 * ========================================================================== */

/**
 * @brief Program frame-rate control value.
 * @param frame_rate_code Controller register value for frame timing.
 * @return true if command succeeds; otherwise false.
 */
bool hal_frame_rate_set(uint8_t frame_rate_code)
{
    /* Frame Rate Control (Normal Mode) — Datasheet Section 5.3.2 (0xB1) */
    if (!spi_transmit_command(0xB1)) return false;
    if (!spi_transmit_data(&frame_rate_code, 1)) return false;
    return true;
}

/**
 * @brief Program oscillator/interface control value.
 * @param osc_control_code Controller register value for oscillator control.
 * @return true if command succeeds; otherwise false.
 */
bool hal_oscillator_frequency_set(uint8_t osc_control_code)
{
    /* Interface Mode Control — Datasheet Section 5.3.1 (0xB0)
     * TFT_eSPI programs this with 0x00 in the verified ILI9488 init path. */
    if (!spi_transmit_command(0xB0)) return false;
    if (!spi_transmit_data(&osc_control_code, 1)) return false;
    return true;
}

/**
 * @brief Program display inversion control register.
 * @param inversion_control_code Controller register value for inversion behavior.
 * @return true if command succeeds; otherwise false.
 */
bool hal_display_inversion_control_set(uint8_t inversion_control_code)
{
    /* Display Inversion Control — Datasheet Section 5.3.5 (0xB4) */
    if (!spi_transmit_command(0xB4)) return false;
    if (!spi_transmit_data(&inversion_control_code, 1)) return false;
    return true;
}

/**
 * @brief Program display function control register triplet.
 * @param parameter1 First control byte.
 * @param parameter2 Second control byte.
 * @param parameter3 Third control byte.
 * @return true if command succeeds; otherwise false.
 */
bool hal_display_function_control_set(uint8_t parameter1,
                                      uint8_t parameter2,
                                      uint8_t parameter3)
{
    /* Display Function Control — Datasheet Section 5.3.7 (0xB6) */
    uint8_t params[3] = {parameter1, parameter2, parameter3};

    if (!spi_transmit_command(0xB6)) return false;
    if (!spi_transmit_data(params, 3)) return false;
    return true;
}

/**
 * @brief Program entry mode register.
 * @param entry_mode_code Controller register value for entry mode behavior.
 * @return true if command succeeds; otherwise false.
 */
bool hal_entry_mode_set(uint8_t entry_mode_code)
{
    /* Entry Mode Set — Datasheet Section 5.3.8 (0xB7) */
    if (!spi_transmit_command(0xB7)) return false;
    if (!spi_transmit_data(&entry_mode_code, 1)) return false;
    return true;
}

/* ============================================================================
 * Gamma Correction
 * ========================================================================== */

/**
 * @brief Select one predefined gamma curve.
 * @param gamma_curve Gamma curve selector enum value.
 * @return true if command succeeds; otherwise false.
 */
bool hal_gamma_curve_select(hal_gamma_curve_t gamma_curve)
{
    /* Gamma Set — Datasheet Section 5.2.26 (0x26)
     * 0x01=GC1, 0x02=GC2, 0x04=GC3 (default), 0x08=GC4 */
    uint8_t param = (uint8_t)gamma_curve;

    if (!spi_transmit_command(0x26)) return false;
    if (!spi_transmit_data(&param, 1)) return false;
    return true;
}

/**
 * @brief Program positive and negative gamma tables.
 * @param pos_gamma_table Pointer to 15-byte positive gamma table.
 * @param neg_gamma_table Pointer to 15-byte negative gamma table.
 * @return true if both tables are written successfully; otherwise false.
 */
bool hal_gamma_curve_program(const uint8_t *pos_gamma_table,
                             const uint8_t *neg_gamma_table)
{
    /* Positive Gamma Control — Datasheet Section 5.3.33 (0xE0), 15 bytes
     * Negative Gamma Control — Datasheet Section 5.3.34 (0xE1), 15 bytes */
    if (pos_gamma_table == NULL || neg_gamma_table == NULL) return false;

    if (!spi_transmit_command(0xE0)) return false;
    if (!spi_transmit_data(pos_gamma_table, 15)) return false;

    if (!spi_transmit_command(0xE1)) return false;
    if (!spi_transmit_data(neg_gamma_table, 15)) return false;

    return true;
}

/* ============================================================================
 * Status & Information Queries
 * ========================================================================== */

/**
 * @brief Read display ID bytes and assemble ID code.
 * @param id_code Output pointer receiving assembled ID value.
 * @return true if read succeeds; otherwise false.
 */
bool hal_display_id_read(uint32_t *id_code)
{
    /* Read ID4 — Datasheet Section 5.3.30 (0xD3)
     * Returns 4 bytes: [dummy][ID1][ID2][ID3], expected value 0x009488 */
    if (id_code == NULL) return false;

    if (!spi_transmit_command(0xD3)) return false;

    uint8_t rx_buf[4];
    if (!spi_receive_data(rx_buf, 4)) return false;

    *id_code = ((uint32_t)rx_buf[1] << 16) |
               ((uint32_t)rx_buf[2] <<  8) |
                (uint32_t)rx_buf[3];
    return true;
}

/**
 * @brief Read power mode status byte.
 * @param power_mode Output pointer receiving power mode byte.
 * @return true if read succeeds; otherwise false.
 */
bool hal_power_mode_read(uint8_t *power_mode)
{
    /* Read Display Power Mode — Datasheet Section 5.2.6 (0x0A) */
    if (power_mode == NULL) return false;

    if (!spi_transmit_command(0x0A)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *power_mode = rx_buf[1];
    return true;
}

/**
 * @brief Read display status byte.
 * @param status_byte Output pointer receiving status byte.
 * @return true if read succeeds; otherwise false.
 */
bool hal_display_status_read(uint8_t *status_byte)
{
    /* Read Display Status — Datasheet Section 5.2.5 (0x09) */
    if (status_byte == NULL) return false;

    if (!spi_transmit_command(0x09)) return false;

    uint8_t rx_buf[5];
    if (!spi_receive_data(rx_buf, 5)) return false;

    *status_byte = rx_buf[1];
    return true;
}

/**
 * @brief Read display mode byte.
 * @param display_mode Output pointer receiving mode byte.
 * @return true if read succeeds; otherwise false.
 */
bool hal_display_mode_read(uint8_t *display_mode)
{
    /* Read Display MADCTL — Datasheet Section 5.2.7 (0x0B) */
    if (display_mode == NULL) return false;

    if (!spi_transmit_command(0x0B)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *display_mode = rx_buf[1];
    return true;
}

/**
 * @brief Read pixel format byte.
 * @param pixel_format Output pointer receiving format byte.
 * @return true if read succeeds; otherwise false.
 */
bool hal_pixel_format_read(uint8_t *pixel_format)
{
    /* Read Display Pixel Format — Datasheet Section 5.2.8 (0x0C) */
    if (pixel_format == NULL) return false;

    if (!spi_transmit_command(0x0C)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *pixel_format = rx_buf[1];
    return true;
}

/* ============================================================================
 * Interface & Advanced Configuration
 * ========================================================================== */

/**
 * @brief Program interface mode control values.
 * @param rim_code RIM control byte.
 * @param dim_code DIM control byte.
 * @return true if command succeeds; otherwise false.
 */
bool hal_interface_mode_set(uint8_t rim_code, uint8_t dim_code)
{
    uint8_t params[2] = {rim_code, dim_code};

    if (!spi_transmit_command(0x0D)) return false;
    if (!spi_transmit_data(params, 2)) return false;
    return true;
}

/**
 * @brief Enable partial mode or return to normal mode.
 * @param partial_area_enabled true to enable partial mode, false for normal mode.
 * @return true if command succeeds; otherwise false.
 */
bool hal_partial_mode_set(bool partial_area_enabled)
{
    /* Partial Mode ON (0x12) / Normal Display Mode ON (0x13) — Datasheet Section 5.2.14–5.2.15 */
    uint8_t command = partial_area_enabled ? 0x12 : 0x13;
    return spi_transmit_command(command);
}

/**
 * @brief Define top fixed area, scroll area, and bottom fixed area.
 * @param top_fixed_lines Number of lines in top fixed area.
 * @param scroll_area_lines Number of lines in scrollable area.
 * @param bottom_fixed_lines Number of lines in bottom fixed area.
 * @return true if command succeeds; otherwise false.
 */
bool hal_scroll_area_set(uint16_t top_fixed_lines,
                         uint16_t scroll_area_lines,
                         uint16_t bottom_fixed_lines)
{
    /* Vertical Scrolling Definition — Datasheet Section 5.2.27 (0x33)
     * Params (6 bytes): [TFA_H][TFA_L][VSA_H][VSA_L][BFA_H][BFA_L] */
    uint8_t params[6];
    _address_to_bytes(top_fixed_lines,     &params[0], &params[1]);
    _address_to_bytes(scroll_area_lines,   &params[2], &params[3]);
    _address_to_bytes(bottom_fixed_lines,  &params[4], &params[5]);

    if (!spi_transmit_command(0x33)) return false;
    if (!spi_transmit_data(params, 6)) return false;
    return true;
}

/**
 * @brief Set vertical scrolling start line address.
 * @param start_line Start line index for vertical scroll.
 * @return true if command succeeds; otherwise false.
 */
bool hal_scroll_start_address_set(uint16_t start_line)
{
    /* Vertical Scrolling Start Address — Datasheet Section 5.2.31 (0x37) */
    uint8_t params[2];
    _address_to_bytes(start_line, &params[0], &params[1]);

    if (!spi_transmit_command(0x37)) return false;
    if (!spi_transmit_data(params, 2)) return false;
    return true;
}

/**
 * @brief Send normal display mode command.
 * @param None.
 * @return true if command succeeds; otherwise false.
 */
bool hal_normal_display_mode_on(void)
{
    /* Normal Display Mode ON — Datasheet Section 5.2.15 (0x13) */
    return spi_transmit_command(0x13);
}

/**
 * @brief Send display inversion ON command.
 * @param None.
 * @return true if command succeeds; otherwise false.
 */
bool hal_invert_display_mode_on(void)
{
    /* Display Inversion ON — Datasheet Section 5.2.17 (0x21) */
    return spi_transmit_command(0x21);
}

/**
 * @brief Enable or disable adaptive color enhancement.
 * @param ace_enabled true to enable enhancement, false to disable.
 * @return true if command succeeds; otherwise false.
 */
bool hal_color_enhancement_set(bool ace_enabled)
{
    /* Color Enhancement Control 1 — Datasheet Section 5.3.9 (0xB9) */
    uint8_t param = ace_enabled ? 0x01 : 0x00;

    if (!spi_transmit_command(0xB9)) return false;
    if (!spi_transmit_data(&param, 1)) return false;
    return true;
}

/**
 * @brief Configure interface write-enable mode behavior.
 * @param wemode_enabled true to enable write-enable mode, false to disable.
 * @return true if command succeeds; otherwise false.
 */
bool hal_interface_control_set(bool wemode_enabled)
{
    /* CABC Control 1 — Datasheet Section 5.3.18 (0xC6) */
    uint8_t param = wemode_enabled ? 0x02 : 0x00;

    if (!spi_transmit_command(0xC6)) return false;
    if (!spi_transmit_data(&param, 1)) return false;
    return true;
}

/**
 * @brief Configure RAM protection keying behavior.
 * @param protection_enabled true to enable RAM protection, false to disable.
 * @param key_code Protection key value written when protection is enabled.
 * @return true if command succeeds; otherwise false.
 */
bool hal_ram_protection_set(bool protection_enabled, uint8_t key_code)
{
    /* CABC Control 3 — Datasheet Section 5.3.20 (0xC9) */
    uint8_t param = protection_enabled ? key_code : 0x00;

    if (!spi_transmit_command(0xC9)) return false;
    if (!spi_transmit_data(&param, 1)) return false;
    return true;
}

/**
 * @brief Configure optional controller GPIO behavior.
 * @param gpio_mask Bitmask selecting GPIO channels to update.
 * @param gpio_config Configuration value applied to selected channels.
 * @return true if command succeeds; otherwise false.
 */
bool hal_gpio_configure(uint8_t gpio_mask, uint8_t gpio_config)
{
    /* Vendor-specific adjustment sequence used by the verified TFT_eSPI init path.
     * Keep this helper isolated so the command bytes can be swapped per module. */
    uint8_t params[2] = {gpio_mask, gpio_config};

    if (!spi_transmit_command(0xF7)) return false;
    if (!spi_transmit_data(params, 2)) return false;
    return true;
}
