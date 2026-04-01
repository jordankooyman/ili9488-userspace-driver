/**
 * @file ili9488_hal.c
 * @brief ILI9488 Hardware Abstraction Layer (HAL) Implementation
 *
 * This layer implements high-level display commands by coordinating
 * low-level SPI and GPIO operations. Each function encapsulates the
 * exact sequence from the ILI9488 datasheet.
 *
 * References: ILI9488 Datasheet, Section 8 (Command Set)
 */

#include "ili9488_hal.h"
#include "ili9488_spi.h"
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief Convert 16-bit address to big-endian byte pair
 *
 * ILI9488 datasheet requires all 16-bit values transmitted MSB-first.
 * This helper splits a 16-bit address into two bytes with correct order.
 *
 * Example: address 320 (0x0140) becomes [0x01, 0x40]
 *
 * @param address 16-bit value
 * @param msb_byte Pointer to store high byte
 * @param lsb_byte Pointer to store low byte
 */
static inline void _address_to_bytes(uint16_t address,
                                      uint8_t *msb_byte,
                                      uint8_t *lsb_byte)
{
    *msb_byte = (uint8_t)((address >> 8) & 0xFF);
    *lsb_byte = (uint8_t)(address & 0xFF);
}

/**
 * @brief Track current display configuration state
 *
 * Caches Memory Access Control register (0x36) to avoid redundant reads
 * and track current rotation/mirror/format settings.
 */
static uint8_t g_current_mac_register = 0x00;

/**
 * @brief Track currently set pixel format
 *
 * Caches last set pixel format to calculate byte counts correctly.
 */
static hal_pixel_format_t g_current_pixel_format = PIXEL_FORMAT_16BIT;

/* ============================================================================
 * Initialization & Configuration
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_display_initialize(pixel_format, rotation)
 *   {
 *     // HARDWARE RESET SEQUENCE
 *     // Reference: Datasheet Section 4 (Initialization and Power Supply)
 *
 *     // Step 1: Pull RSTB (reset) low for 1ms minimum
 *     spi_gpio_set_state(GPIO_RESET, GPIO_STATE_LOW);
 *     spi_delay_ms(1);
 *
 *     // Step 2: Release RSTB high
 *     spi_gpio_set_state(GPIO_RESET, GPIO_STATE_HIGH);
 *
 *     // Step 3: Wait for oscillator stabilization (120ms recommended)
 *     spi_delay_ms(120);
 *
 *     // SOFTWARE RESET & POWER UP
 *     // Reference: Datasheet Section 8.2.1 (Software Reset - 0x01)
 *
 *     // Step 4: Execute software reset
 *     spi_transmit_command(0x01);
 *
 *     // Step 5: Wait 5ms for reset completion
 *     spi_delay_ms(5);
 *
 *     // Step 6: Sleep out (wake from any sleep mode)
 *     // Reference: Datasheet Section 8.2.10 (Sleep Out - 0x11)
 *     spi_transmit_command(0x11);
 *
 *     // Step 7: Wait 120ms for power supply stabilization
 *     spi_delay_ms(120);
 *
 *     // DISPLAY INTERFACE CONFIGURATION
 *     // Reference: Datasheet Section 8.2.5 (Pixel Format Set - 0x3A)
 *
 *     // Step 8: Set pixel color format (12/16/18 bpp)
 *     hal_pixel_format_set(pixel_format);
 *     g_current_pixel_format = pixel_format;
 *
 *     // Step 9: Set memory access control (rotation, mirror, refresh direction)
 *     // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *     hal_display_rotation_set(rotation,
 *                              HORIZONAL_DIRECTION_DEFAULT,
 *                              VERTICAL_DIRECTION_DEFAULT);
 *
 *     // POWER SUPPLY CONFIGURATION
 *     // Reference: Datasheet Section 8.2.13-17 (Power Control)
 *
 *     // Step 10: Power Control 1 - Set GVDD voltage
 *     // Command 0xC0: Typical value 0x17 = 4.0V
 *     hal_power_gvdd_set(0x17);
 *
 *     // Step 11: Power Control 2 - VCIOUT voltage
 *     // Command 0xC1
 *     hal_power_vci_set(0x41);
 *
 *     // Step 12: Power Control 3 - VGH/VGL voltages
 *     // Command 0xC5: VGH=0x0A, VGL=-0x0A
 *     hal_power_vgh_vgl_set(0x0A, 0x0A);
 *
 *     // Step 13: VCOMH voltage control
 *     // Command 0xBB: Typical 0x30
 *     hal_power_vcomh_set(0x30);
 *
 *     // TIMING CONFIGURATION
 *     // Reference: Datasheet Section 8.2.12 (Frame Rate Control - 0xB1)
 *
 *     // Step 14: Frame rate control (set 60Hz)
 *     hal_frame_rate_set(0x18);
 *
 *     // GAMMA CORRECTION
 *     // Reference: Datasheet Section 8.2.27-28 (Gamma Curves)
 *
 *     // Step 15: Select gamma curve (curve 3 recommended)
 *     hal_gamma_curve_select(GAMMA_CURVE_3);
 *
 *     // ADDRESS WINDOW CONFIGURATION
 *     // Reference: Datasheet Section 8.2.18-19 (Column/Page Address Set)
 *
 *     // Step 16: Set column address window (0-319)
 *     hal_column_address_set(0, 319);
 *
 *     // Step 17: Set row address window (0-479)
 *     hal_row_address_set(0, 479);
 *
 *     // Step 18: Normal display mode (no inversion)
 *     // Reference: Datasheet Section 8.2.24 (Normal Display Mode On - 0x13)
 *     hal_normal_display_mode_on();
 *
 *     // DISPLAY ENABLE
 *     // Reference: Datasheet Section 8.2.29 (Display On - 0x29)
 *
 *     // Step 19: Turn on display output
 *     hal_display_output_control(true);
 *
 *     // Step 20: Wait for display to stabilize (optional, 10ms recommended)
 *     spi_delay_ms(10);
 *
 *     return true;
 *   }
 */
bool hal_display_initialize(hal_pixel_format_t pixel_format,
                            hal_rotation_t rotation)
{
    /* PSEUDOCODE - Detailed sequence above in comments */

    // Step 1-3: Hardware reset sequence
    spi_gpio_set_state(GPIO_RESET, GPIO_STATE_LOW);
    spi_delay_ms(1);
    spi_gpio_set_state(GPIO_RESET, GPIO_STATE_HIGH);
    spi_delay_ms(120);

    // Step 4-7: Software reset and sleep out
    if (!spi_transmit_command(0x01)) return false;
    spi_delay_ms(5);
    if (!spi_transmit_command(0x11)) return false;
    spi_delay_ms(120);

    // Step 8-9: Pixel format and rotation
    if (!hal_pixel_format_set(pixel_format)) return false;
    if (!hal_display_rotation_set(rotation,
                                   HORIZONAL_DIRECTION_DEFAULT,
                                   VERTICAL_DIRECTION_DEFAULT)) return false;

    // Step 10-13: Power control sequence
    if (!hal_power_gvdd_set(0x17)) return false;
    if (!hal_power_vci_set(0x41)) return false;
    if (!hal_power_vgh_vgl_set(0x0A, 0x0A)) return false;
    if (!hal_power_vcomh_set(0x30)) return false;

    // Step 14-15: Timing and gamma
    if (!hal_frame_rate_set(0x18)) return false;
    if (!hal_gamma_curve_select(GAMMA_CURVE_3)) return false;

    // Step 16-18: Address window and display mode
    if (!hal_column_address_set(0, 319)) return false;
    if (!hal_row_address_set(0, 479)) return false;
    if (!hal_normal_display_mode_on()) return false;

    // Step 19-20: Enable display
    if (!hal_display_output_control(true)) return false;
    spi_delay_ms(10);

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_deinitialize()
 *   {
 *     // Safe shutdown sequence
 *
 *     // Step 1: Turn off display output
 *     // Reference: Datasheet Section 8.2.6 (Display Off - 0x28)
 *     hal_display_output_control(false);
 *
 *     // Step 2: Wait for display to fully turn off (5ms)
 *     spi_delay_ms(5);
 *
 *     // Step 3: Enter sleep mode (reduced power consumption)
 *     // Reference: Datasheet Section 8.2.9 (Sleep In - 0x10)
 *     hal_power_sleep_mode_set(true);
 *
 *     // Step 4: Optional - hardware reset low (complete power off)
 *     // Uncomment if power supply will be disconnected
 *     // spi_gpio_set_state(GPIO_RESET, GPIO_STATE_LOW);
 *
 *     return true;
 *   }
 */
bool hal_display_deinitialize(void)
{
    /* PSEUDOCODE - Safe shutdown sequence */

    if (!hal_display_output_control(false)) return false;
    spi_delay_ms(5);
    if (!hal_power_sleep_mode_set(true)) return false;

    return true;
}

/* ============================================================================
 * System Control Commands
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_system_reset(reset_scope)
 *   {
 *     // Reference: Datasheet Section 8.2.1 (Software Reset - 0x01)
 *
 *     // All register values reset to default
 *     // GRAM content is NOT cleared (preserved after reset)
 *
 *     spi_transmit_command(0x01);
 *     spi_delay_ms(5);  // Minimum 5ms wait
 *
 *     return true;
 *   }
 */
bool hal_system_reset(hal_reset_scope_t reset_scope)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.1 (Software Reset - 0x01)
    (void)reset_scope;  // Currently only COMPLETE reset is supported

    if (!spi_transmit_command(0x01)) return false;
    spi_delay_ms(5);  // Wait for reset completion

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_sleep_mode_set(enable)
 *   {
 *     if (enable) {
 *       // Enter sleep mode
 *       // Reference: Datasheet Section 8.2.9 (Sleep In - 0x10)
 *       spi_transmit_command(0x10);
 *       spi_delay_ms(5);  // Wait for sleep mode entry
 *     } else {
 *       // Exit sleep mode
 *       // Reference: Datasheet Section 8.2.10 (Sleep Out - 0x11)
 *       spi_transmit_command(0x11);
 *       spi_delay_ms(120);  // Allow power stabilization
 *     }
 *
 *     return true;
 *   }
 */
bool hal_power_sleep_mode_set(bool enable)
{
    /* PSEUDOCODE */

    if (enable) {
        // Enter sleep mode: Command 0x10
        if (!spi_transmit_command(0x10)) return false;
        spi_delay_ms(5);
    } else {
        // Exit sleep mode: Command 0x11
        if (!spi_transmit_command(0x11)) return false;
        spi_delay_ms(120);
    }

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_sleep_exit()
 *   {
 *     // Reference: Datasheet Section 8.2.10 (Sleep Out - 0x11)
 *     spi_transmit_command(0x11);
 *     spi_delay_ms(120);  // Allow 120ms for power stabilization
 *
 *     return true;
 *   }
 */
bool hal_power_sleep_exit(void)
{
    /* PSEUDOCODE */

    if (!spi_transmit_command(0x11)) return false;
    spi_delay_ms(120);

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_output_control(enabled)
 *   {
 *     if (enabled) {
 *       // Display On command
 *       // Reference: Datasheet Section 8.2.29 (Display On - 0x29)
 *       spi_transmit_command(0x29);
 *     } else {
 *       // Display Off command
 *       // Reference: Datasheet Section 8.2.28 (Display Off - 0x28)
 *       spi_transmit_command(0x28);
 *     }
 *
 *     return true;
 *   }
 */
bool hal_display_output_control(bool enabled)
{
    /* PSEUDOCODE */

    uint8_t command = enabled ? 0x29 : 0x28;  // 0x29=Display On, 0x28=Display Off

    return spi_transmit_command(command);
}

/**
 * Pseudocode Implementation:
 *   hal_power_set_state(power_state)
 *   {
 *     switch (power_state) {
 *       case POWER_STATE_SLEEP:
 *         return hal_power_sleep_mode_set(true);
 *
 *       case POWER_STATE_NORMAL:
 *         // Ensure sleep exit + display on
 *         hal_power_sleep_exit();
 *         return hal_display_output_control(true);
 *
 *       case POWER_STATE_DISPLAY_OFF:
 *         // Display off but controller still active (can be re-enabled quickly)
 *         return hal_display_output_control(false);
 *
 *       default:
 *         return false;
 *     }
 *   }
 */
bool hal_power_set_state(hal_power_state_t power_state)
{
    /* PSEUDOCODE */

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
 * Pseudocode Implementation:
 *   hal_pixel_format_set(pixel_format)
 *   {
 *     // Reference: Datasheet Section 8.2.5 (Pixel Format Set - 0x3A)
 *
 *     // Command 0x3A takes 1 parameter byte:
 *     // DPI[2:0] field specifies color depth:
 *     //   - 0x03 = 12-bit (4096 colors)
 *     //   - 0x05 = 16-bit (65536 colors, RGB565) - RECOMMENDED
 *     //   - 0x06 = 18-bit (262144 colors)
 *
 *     uint8_t param = (uint8_t)pixel_format;
 *
 *     spi_transmit_command(0x3A);
 *     spi_transmit_data(&param, 1);
 *
 *     // Cache format for later byte count calculations
 *     g_current_pixel_format = pixel_format;
 *
 *     return true;
 *   }
 */
bool hal_pixel_format_set(hal_pixel_format_t pixel_format)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.5 (Pixel Format Set - 0x3A)

    uint8_t param = (uint8_t)pixel_format;

    if (!spi_transmit_command(0x3A)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    g_current_pixel_format = pixel_format;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_rotation_set(rotation, horizontal_flip, vertical_flip)
 *   {
 *     // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *
 *     // Memory Access Control byte layout:
 *     // Bit 7 (MY): Row address direction (1=bottom-top, 0=top-bottom)
 *     // Bit 6 (MX): Column address direction (1=right-left, 0=left-right)
 *     // Bit 5 (MV): Swap row/column (1=transpose, 0=normal)
 *     // Bit 4 (ML): Vertical/horizontal refresh (0=normal)
 *     // Bit 3 (RGB): Color order (0=RGB, 1=BGR)
 *     // Bits 2-0: Unused
 *
 *     uint8_t mac_byte = 0x00;
 *
 *     // Set rotation bits (MY, MX, MV)
 *     // Pre-calculated rotation values from Datasheet Section 8.2.3:
 *     switch (rotation) {
 *       case ROTATION_0_NORMAL:
 *         mac_byte |= 0x00;  // MY=0, MX=0, MV=0
 *         break;
 *       case ROTATION_90_CLOCKWISE:
 *         mac_byte |= 0x60;  // MY=1, MX=1, MV=0
 *         break;
 *       case ROTATION_180_INVERSE:
 *         mac_byte |= 0xC0;  // MY=1, MX=1, MV=0 (different combo)
 *         break;
 *       case ROTATION_270_COUNTERCLOKWISE:
 *         mac_byte |= 0xA0;  // MY=1, MX=0, MV=1
 *         break;
 *       default:
 *         return false;
 *     }
 *
 *     // Apply flips if requested
 *     if (horizontal_flip == HORIZONAL_DIRECTION_REVERSED)
 *       mac_byte ^= 0x40;  // XOR with MX bit
 *     if (vertical_flip == VERTICAL_DIRECTION_REVERSED)
 *       mac_byte ^= 0x80;  // XOR with MY bit
 *
 *     // Transmit updated MAC register
 *     spi_transmit_command(0x36);
 *     spi_transmit_data(&mac_byte, 1);
 *
 *     // Cache register value
 *     g_current_mac_register = mac_byte;
 *
 *     return true;
 *   }
 */
bool hal_display_rotation_set(hal_rotation_t rotation,
                              hal_horizontal_direction_t horizontal_flip,
                              hal_vertical_direction_t vertical_flip)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)

    uint8_t mac_byte = 0x00;

    // Set rotation encoding (MY, MX, MV bits)
    switch (rotation) {
        case ROTATION_0_NORMAL:
            mac_byte = 0x00;
            break;
        case ROTATION_90_CLOCKWISE:
            mac_byte = 0x60;
            break;
        case ROTATION_180_INVERSE:
            mac_byte = 0xC0;
            break;
        case ROTATION_270_COUNTERCLOKWISE:
            mac_byte = 0xA0;
            break;
        default:
            return false;
    }

    // Apply flips
    if (horizontal_flip == HORIZONAL_DIRECTION_REVERSED)
        mac_byte ^= 0x40;  // Flip MX
    if (vertical_flip == VERTICAL_DIRECTION_REVERSED)
        mac_byte ^= 0x80;  // Flip MY

    // Transmit Memory Access Control
    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_color_byte_order_set(byte_order)
 *   {
 *     // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *
 *     // Read current MAC register to preserve other bits
 *     uint8_t mac_byte = g_current_mac_register;
 *
 *     // Modify RGB bit [3]
 *     if (byte_order == BYTE_ORDER_BGR) {
 *       mac_byte |= 0x08;   // Set RGB bit to enable BGR mode
 *     } else {
 *       mac_byte &= ~0x08;  // Clear RGB bit for RGB mode
 *     }
 *
 *     // Update MAC register
 *     spi_transmit_command(0x36);
 *     spi_transmit_data(&mac_byte, 1);
 *
 *     g_current_mac_register = mac_byte;
 *
 *     return true;
 *   }
 */
bool hal_color_byte_order_set(hal_byte_order_t byte_order)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
    // RGB bit [3] controls byte order

    uint8_t mac_byte = g_current_mac_register;

    if (byte_order == BYTE_ORDER_BGR) {
        mac_byte |= 0x08;   // Set RGB bit
    } else {
        mac_byte &= ~0x08;  // Clear RGB bit
    }

    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_transfer_mode_set(transfer_mode)
 *   {
 *     // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *     // MV bit [5] controls row vs. column addressing
 *
 *     uint8_t mac_byte = g_current_mac_register;
 *
 *     if (transfer_mode == TRANSFER_MODE_PAGE_ADDRESS) {
 *       mac_byte |= 0x20;   // Set MV bit for column mode
 *     } else {
 *       mac_byte &= ~0x20;  // Clear MV bit for row mode
 *     }
 *
 *     spi_transmit_command(0x36);
 *     spi_transmit_data(&mac_byte, 1);
 *
 *     g_current_mac_register = mac_byte;
 *
 *     return true;
 *   }
 */
bool hal_transfer_mode_set(hal_transfer_mode_t transfer_mode)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
    // MV bit [5] control column/row addressing

    uint8_t mac_byte = g_current_mac_register;

    if (transfer_mode == TRANSFER_MODE_PAGE_ADDRESS) {
        mac_byte |= 0x20;   // Set MV bit
    } else {
        mac_byte &= ~0x20;  // Clear MV bit
    }

    if (!spi_transmit_command(0x36)) return false;
    if (!spi_transmit_data(&mac_byte, 1)) return false;

    g_current_mac_register = mac_byte;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_config_read(config_byte)
 *   {
 *     // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *
 *     // Read current MAC register value
 *     spi_transmit_command(0x36);       // Send read command
 *     uint8_t rx_buf[2];
 *     spi_receive_data(rx_buf, 2);      // Receive 2 bytes (first is status)
 *
 *     *config_byte = rx_buf[1];         // Second byte is actual MAC value
 *     g_current_mac_register = rx_buf[1];  // Update cache
 *
 *     return true;
 *   }
 */
bool hal_display_config_read(uint8_t *config_byte)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
    // Can read current register value to verify settings

    if (config_byte == NULL) return false;

    // Use cached value (more efficient than read)
    *config_byte = g_current_mac_register;

    return true;
}

/* ============================================================================
 * Memory Address & Access Commands
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_column_address_set(start_column, end_column)
 *   {
 *     // Reference: Datasheet Section 8.2.18 (Column Address Set - 0x2A)
 *
 *     // Command 0x2A takes 4 parameter bytes:
 *     // Byte 0-1: Column start address (16-bit, big-endian)
 *     // Byte 2-3: Column end address (16-bit, big-endian)
 *
 *     uint8_t params[4];
 *     _address_to_bytes(start_column, &params[0], &params[1]);
 *     _address_to_bytes(end_column, &params[2], &params[3]);
 *
 *     spi_transmit_command(0x2A);
 *     spi_transmit_data(params, 4);
 *
 *     return true;
 *   }
 */
bool hal_column_address_set(uint16_t start_column, uint16_t end_column)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.18 (Column Address Set - 0x2A)

    uint8_t params[4];
    _address_to_bytes(start_column, &params[0], &params[1]);
    _address_to_bytes(end_column, &params[2], &params[3]);

    if (!spi_transmit_command(0x2A)) return false;
    if (!spi_transmit_data(params, 4)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_row_address_set(start_row, end_row)
 *   {
 *     // Reference: Datasheet Section 8.2.19 (Page Address Set - 0x2B)
 *
 *     // Command 0x2B takes 4 parameter bytes:
 *     // Byte 0-1: Row/Page start address (16-bit, big-endian)
 *     // Byte 2-3: Row/Page end address (16-bit, big-endian)
 *
 *     uint8_t params[4];
 *     _address_to_bytes(start_row, &params[0], &params[1]);
 *     _address_to_bytes(end_row, &params[2], &params[3]);
 *
 *     spi_transmit_command(0x2B);
 *     spi_transmit_data(params, 4);
 *
 *     return true;
 *   }
 */
bool hal_row_address_set(uint16_t start_row, uint16_t end_row)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.19 (Page Address Set - 0x2B)

    uint8_t params[4];
    _address_to_bytes(start_row, &params[0], &params[1]);
    _address_to_bytes(end_row, &params[2], &params[3]);

    if (!spi_transmit_command(0x2B)) return false;
    if (!spi_transmit_data(params, 4)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_window_address_set(x_start, x_end, y_start, y_end)
 *   {
 *     // Convenience function combining column and row set
 *     hal_column_address_set(x_start, x_end);
 *     hal_row_address_set(y_start, y_end);
 *
 *     return true;
 *   }
 */
bool hal_window_address_set(uint16_t x_start, uint16_t x_end,
                            uint16_t y_start, uint16_t y_end)
{
    /* PSEUDOCODE */

    if (!hal_column_address_set(x_start, x_end)) return false;
    if (!hal_row_address_set(y_start, y_end)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_gram_write_start()
 *   {
 *     // Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)
 *
 *     // Command 0x2C initiates GRAM write mode
 *     // All subsequent data transmissions go directly to GRAM
 *     // GRAM address auto-increments with each pixel
 *
 *     spi_transmit_command(0x2C);
 *
 *     // No parameters required
 *
 *     return true;
 *   }
 */
bool hal_gram_write_start(void)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)

    return spi_transmit_command(0x2C);
}

/**
 * Pseudocode Implementation:
 *   hal_gram_read_start()
 *   {
 *     // Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)
 *
 *     // Command 0x2E initiates GRAM read mode
 *     // All subsequent SPI reads return GRAM content
 *     // Note: First byte is typically status, skip it
 *
 *     spi_transmit_command(0x2E);
 *
 *     return true;
 *   }
 */
bool hal_gram_read_start(void)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)

    return spi_transmit_command(0x2E);
}

/* ============================================================================
 * Pixel Data Transmission
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_gram_write_pixels(pixel_buffer, pixel_count, pixel_format)
 *   {
 *     // Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)
 *     // Must be called after hal_gram_write_start()
 *
 *     // Calculate byte count from pixel format
 *     size_t bytes_per_pixel = 0;
 *     switch (pixel_format) {
 *       case PIXEL_FORMAT_12BIT:
 *         bytes_per_pixel = 2;  // 12 bits requires 1.5 bytes (565 for efficiency)
 *         break;
 *       case PIXEL_FORMAT_16BIT:
 *         bytes_per_pixel = 2;  // RGB565: 16 bits = 2 bytes
 *         break;
 *       case PIXEL_FORMAT_18BIT:
 *         bytes_per_pixel = 3;  // RGB666: 18 bits = 3 bytes
 *         break;
 *       default:
 *         return false;
 *     }
 *
 *     size_t total_bytes = pixel_count * bytes_per_pixel;
 *
 *     // Transmit pixel data to GRAM
 *     return spi_transmit_bulkdata(pixel_buffer, total_bytes);
 *   }
 */
bool hal_gram_write_pixels(const uint8_t *pixel_buffer,
                           uint32_t pixel_count,
                           hal_pixel_format_t pixel_format)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)

    if (pixel_buffer == NULL || pixel_count == 0) return false;

    // Calculate byte count from pixel format
    size_t bytes_per_pixel;
    switch (pixel_format) {
        case PIXEL_FORMAT_12BIT:
            bytes_per_pixel = 2;  // Use RGB565 format (2 bytes)
            break;
        case PIXEL_FORMAT_16BIT:
            bytes_per_pixel = 2;  // RGB565: 2 bytes per pixel
            break;
        case PIXEL_FORMAT_18BIT:
            bytes_per_pixel = 3;  // RGB666: 3 bytes per pixel
            break;
        default:
            return false;
    }

    size_t total_bytes = (size_t)pixel_count * bytes_per_pixel;

    return spi_transmit_bulkdata(pixel_buffer, total_bytes);
}

/**
 * Pseudocode Implementation:
 *   hal_gram_read_pixels(pixel_buffer, pixel_count, pixel_format)
 *   {
 *     // Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)
 *     // Must be called after hal_gram_read_start()
 *
 *     // Calculate byte count from pixel format
 *     size_t bytes_per_pixel = 0;
 *     switch (pixel_format) {
 *       case PIXEL_FORMAT_12BIT:
 *         bytes_per_pixel = 2;
 *         break;
 *       case PIXEL_FORMAT_16BIT:
 *         bytes_per_pixel = 2;
 *         break;
 *       case PIXEL_FORMAT_18BIT:
 *         bytes_per_pixel = 3;
 *         break;
 *       default:
 *         return false;
 *     }
 *
 *     size_t total_bytes = pixel_count * bytes_per_pixel;
 *
 *     // Receive pixel data from GRAM
 *     return spi_receive_data(pixel_buffer, total_bytes);
 *   }
 */
bool hal_gram_read_pixels(uint8_t *pixel_buffer,
                          uint32_t pixel_count,
                          hal_pixel_format_t pixel_format)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)

    if (pixel_buffer == NULL || pixel_count == 0) return false;

    size_t bytes_per_pixel;
    switch (pixel_format) {
        case PIXEL_FORMAT_12BIT:
            bytes_per_pixel = 2;
            break;
        case PIXEL_FORMAT_16BIT:
            bytes_per_pixel = 2;
            break;
        case PIXEL_FORMAT_18BIT:
            bytes_per_pixel = 3;
            break;
        default:
            return false;
    }

    size_t total_bytes = (size_t)pixel_count * bytes_per_pixel;

    return spi_receive_data(pixel_buffer, total_bytes);
}

/**
 * Pseudocode Implementation:
 *   hal_fill_rectangle_solid(x_start, x_end, y_start, y_end, color_rgb565)
 *   {
 *     // High-level convenience function to fill a region with solid color
 *
 *     // Step 1: Set address window to target rectangle
 *     hal_window_address_set(x_start, x_end, y_start, y_end);
 *
 *     // Step 2: Initiate GRAM write
 *     hal_gram_write_start();
 *
 *     // Step 3: Calculate number of pixels in rectangle
 *     uint32_t width = x_end - x_start + 1;
 *     uint32_t height = y_end - y_start + 1;
 *     uint32_t pixel_count = width * height;
 *
 *     // Step 4: Create a buffer with color value encoded for pixel format
 *     // For RGB565 (16-bit): color is 2 bytes
 *     uint8_t color_bytes[2];
 *     color_bytes[0] = (color_rgb565 >> 8) & 0xFF;   // High byte
 *     color_bytes[1] = color_rgb565 & 0xFF;          // Low byte
 *
 *     // Step 5: Transmit color pixel_count times
 *     // Optimization: Use bulk transmit for efficiency
 *     for (uint32_t i = 0; i < pixel_count; i++) {
 *       spi_transmit_bulkdata(color_bytes, 2);
 *     }
 *
 *     return true;
 *   }
 */
bool hal_fill_rectangle_solid(uint16_t x_start, uint16_t x_end,
                              uint16_t y_start, uint16_t y_end,
                              uint16_t color_rgb565)
{
    /* PSEUDOCODE */

    // Convenience function to fill rectangular region with solid color

    if (!hal_window_address_set(x_start, x_end, y_start, y_end)) return false;
    if (!hal_gram_write_start()) return false;

    // Calculate pixel count
    uint32_t width = (x_end - x_start) + 1;
    uint32_t height = (y_end - y_start) + 1;
    uint32_t pixel_count = width * height;

    // Create color value in bytes (RGB565 = 2 bytes)
    uint8_t color_bytes[2];
    color_bytes[0] = (uint8_t)((color_rgb565 >> 8) & 0xFF);
    color_bytes[1] = (uint8_t)(color_rgb565 & 0xFF);

    // Write color repeatedly (could be optimized with larger buffer)
    for (uint32_t i = 0; i < pixel_count; i++) {
        if (!spi_transmit_bulkdata(color_bytes, 2)) return false;
    }

    return true;
}

/* ============================================================================
 * Power Supply & Voltage Control
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_power_gvdd_set(gvdd_voltage)
 *   {
 *     // Reference: Datasheet Section 8.2.13 (Power Control 1 - 0xC0)
 *
 *     // Command 0xC0 sets GVDD (core voltage) for display
 *     // Parameter: Voltage level (0x00-0x2C, typically 0x17 for 4.0V)
 *
 *     spi_transmit_command(0xC0);
 *     spi_transmit_data(&gvdd_voltage, 1);
 *
 *     return true;
 *   }
 */
bool hal_power_gvdd_set(uint8_t gvdd_voltage)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.13 (Power Control 1 - 0xC0)

    if (!spi_transmit_command(0xC0)) return false;
    if (!spi_transmit_data(&gvdd_voltage, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_vci_set(vci_voltage)
 *   {
 *     // Reference: Datasheet Section 8.2.14 (Power Control 2 - 0xC1)
 *
 *     spi_transmit_command(0xC1);
 *     spi_transmit_data(&vci_voltage, 1);
 *
 *     return true;
 *   }
 */
bool hal_power_vci_set(uint8_t vci_voltage)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.14 (Power Control 2 - 0xC1)

    if (!spi_transmit_command(0xC1)) return false;
    if (!spi_transmit_data(&vci_voltage, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_vgh_vgl_set(vgh_voltage, vgl_voltage)
 *   {
 *     // Reference: Datasheet Section 8.2.15 (Power Control 3 - 0xC5)
 *
 *     // Command 0xC5 takes 2 parameters:
 *     // Byte 0: VGH (positive gate driver voltage, typically 0x0A = 16V)
 *     // Byte 1: VGL (negative gate driver voltage, typically -0x0A = -10V)
 *
 *     uint8_t params[2] = {vgh_voltage, vgl_voltage};
 *
 *     spi_transmit_command(0xC5);
 *     spi_transmit_data(params, 2);
 *
 *     return true;
 *   }
 */
bool hal_power_vgh_vgl_set(uint8_t vgh_voltage, uint8_t vgl_voltage)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.15 (Power Control 3 - 0xC5)

    uint8_t params[2] = {vgh_voltage, vgl_voltage};

    if (!spi_transmit_command(0xC5)) return false;
    if (!spi_transmit_data(params, 2)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_vcomh_set(vcomh_voltage)
 *   {
 *     // Reference: Datasheet Section 8.2.17 (VCOM Control - 0xBB)
 *
 *     spi_transmit_command(0xBB);
 *     spi_transmit_data(&vcomh_voltage, 1);
 *
 *     return true;
 *   }
 */
bool hal_power_vcomh_set(uint8_t vcomh_voltage)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.17 (VCOM Control - 0xBB)

    if (!spi_transmit_command(0xBB)) return false;
    if (!spi_transmit_data(&vcomh_voltage, 1)) return false;

    return true;
}

/* ============================================================================
 * Display Timing & Frequency Control
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_frame_rate_set(frame_rate_code)
 *   {
 *     // Reference: Datasheet Section 8.2.12 (Frame Rate Control - 0xB1)
 *
 *     // Parameter: Frame rate divisor (typically 0x18 = 60 Hz)
 *     // Output Hz = fOSC / (divisor * pixel_count)
 *
 *     spi_transmit_command(0xB1);
 *     spi_transmit_data(&frame_rate_code, 1);
 *
 *     return true;
 *   }
 */
bool hal_frame_rate_set(uint8_t frame_rate_code)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.12 (Frame Rate Control - 0xB1)

    if (!spi_transmit_command(0xB1)) return false;
    if (!spi_transmit_data(&frame_rate_code, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_oscillator_frequency_set(osc_control_code)
 *   {
 *     // Reference: Datasheet Section 8.2.11 (Oscillator Control - 0xB0)
 *
 *     spi_transmit_command(0xB0);
 *     spi_transmit_data(&osc_control_code, 1);
 *
 *     return true;
 *   }
 */
bool hal_oscillator_frequency_set(uint8_t osc_control_code)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.11 (Oscillator Control - 0xB0)

    if (!spi_transmit_command(0xB0)) return false;
    if (!spi_transmit_data(&osc_control_code, 1)) return false;

    return true;
}

/* ============================================================================
 * Gamma Correction
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_gamma_curve_select(gamma_curve)
 *   {
 *     // Reference: Datasheet Section 8.2.26 (Gamma Set - 0x26)
 *
 *     // Parameter encodes gamma curve selection:
 *     // 0x01 = Gamma 1
 *     // 0x02 = Gamma 2
 *     // 0x04 = Gamma 3 (recommended)
 *     // 0x08 = Gamma 4
 *
 *     uint8_t param = (uint8_t)gamma_curve;
 *
 *     spi_transmit_command(0x26);
 *     spi_transmit_data(&param, 1);
 *
 *     return true;
 *   }
 */
bool hal_gamma_curve_select(hal_gamma_curve_t gamma_curve)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.26 (Gamma Set - 0x26)

    uint8_t param = (uint8_t)gamma_curve;

    if (!spi_transmit_command(0x26)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_gamma_curve_program(pos_gamma_table, neg_gamma_table)
 *   {
 *     // Reference: Datasheet Section 8.2.27-28 (Positive/Negative Gamma)
 *
 *     // Each table contains 15 bytes defining gamma curve
 *     // Command 0xE0: Positive gamma
 *     // Command 0xE1: Negative gamma
 *
 *     // Program positive gamma curve
 *     spi_transmit_command(0xE0);
 *     spi_transmit_data(pos_gamma_table, 15);
 *
 *     // Program negative gamma curve
 *     spi_transmit_command(0xE1);
 *     spi_transmit_data(neg_gamma_table, 15);
 *
 *     return true;
 *   }
 */
bool hal_gamma_curve_program(const uint8_t *pos_gamma_table,
                             const uint8_t *neg_gamma_table)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.27-28 (Positive/Negative Gamma)

    if (pos_gamma_table == NULL || neg_gamma_table == NULL) return false;

    // Program positive gamma (0xE0)
    if (!spi_transmit_command(0xE0)) return false;
    if (!spi_transmit_data(pos_gamma_table, 15)) return false;

    // Program negative gamma (0xE1)
    if (!spi_transmit_command(0xE1)) return false;
    if (!spi_transmit_data(neg_gamma_table, 15)) return false;

    return true;
}

/* ============================================================================
 * Status & Information Queries
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_display_id_read(id_code)
 *   {
 *     // Reference: Datasheet Section 8.2.33 (Read ID4 - 0xD3)
 *
 *     // Command 0xD3 returns 4-byte ID:
 *     // Byte 0: Status/Dummy
 *     // Bytes 1-3: ID Code (should be 0x009488)
 *
 *     spi_transmit_command(0xD3);
 *
 *     uint8_t rx_buf[4];
 *     spi_receive_data(rx_buf, 4);
 *
 *     *id_code = ((uint32_t)rx_buf[1] << 16) |
 *                ((uint32_t)rx_buf[2] << 8) |
 *                ((uint32_t)rx_buf[3]);
 *
 *     return true;
 *   }
 */
bool hal_display_id_read(uint32_t *id_code)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.33 (Read ID4 - 0xD3)

    if (id_code == NULL) return false;

    if (!spi_transmit_command(0xD3)) return false;

    uint8_t rx_buf[4];
    if (!spi_receive_data(rx_buf, 4)) return false;

    // Bytes 1-3 contain the ID (byte 0 is status)
    *id_code = ((uint32_t)rx_buf[1] << 16) |
               ((uint32_t)rx_buf[2] << 8) |
               (uint32_t)rx_buf[3];

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_power_mode_read(power_mode)
 *   {
 *     // Reference: Datasheet Section 8.2.29 (Read Power Mode - 0x0A)
 *
 *     spi_transmit_command(0x0A);
 *
 *     uint8_t rx_buf[2];
 *     spi_receive_data(rx_buf, 2);
 *
 *     *power_mode = rx_buf[1];  // Byte 1 contains mode, byte 0 is status
 *
 *     return true;
 *   }
 */
bool hal_power_mode_read(uint8_t *power_mode)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.29 (Read Power Mode - 0x0A)

    if (power_mode == NULL) return false;

    if (!spi_transmit_command(0x0A)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *power_mode = rx_buf[1];

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_status_read(status_byte)
 *   {
 *     // Reference: Datasheet Section 8.2.32 (Read Display Status - 0x0D)
 *
 *     spi_transmit_command(0x0D);
 *
 *     uint8_t rx_buf[2];
 *     spi_receive_data(rx_buf, 2);
 *
 *     *status_byte = rx_buf[1];
 *
 *     return true;
 *   }
 */
bool hal_display_status_read(uint8_t *status_byte)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.32 (Read Display Status - 0x0D)

    if (status_byte == NULL) return false;

    if (!spi_transmit_command(0x0D)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *status_byte = rx_buf[1];

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_display_mode_read(display_mode)
 *   {
 *     // Reference: Datasheet Section 8.2.30 (Read Display Mode - 0x0B)
 *
 *     spi_transmit_command(0x0B);
 *
 *     uint8_t rx_buf[2];
 *     spi_receive_data(rx_buf, 2);
 *
 *     *display_mode = rx_buf[1];
 *
 *     return true;
 *   }
 */
bool hal_display_mode_read(uint8_t *display_mode)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.30 (Read Display Mode - 0x0B)

    if (display_mode == NULL) return false;

    if (!spi_transmit_command(0x0B)) return false;

    uint8_t rx_buf[2];
    if (!spi_receive_data(rx_buf, 2)) return false;

    *display_mode = rx_buf[1];

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_pixel_format_read(pixel_format)
 *   {
 *     // Reference: Datasheet Section 8.2.31 (Read Pixel Format - 0x0C)
 *
 *     spi_transmit_command(0x0C);
 *
 *     uint8_t rx_buf[2];
 *     spi_receive_data(rx_buf, 2);
 *
 *     *pixel_format = rx_buf[1];
 *
 *     return true;
 *   }
 */
bool hal_pixel_format_read(uint8_t *pixel_format)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.31 (Read Pixel Format - 0x0C)

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
 * Pseudocode Implementation:
 *   hal_interface_mode_set(rim_code, dim_code)
 *   {
 *     // Reference: Datasheet Section 8.2.2 (Interface Pixel Format - 0x0D)
 *
 *     uint8_t params[2] = {rim_code, dim_code};
 *
 *     spi_transmit_command(0x0D);
 *     spi_transmit_data(params, 2);
 *
 *     return true;
 *   }
 */
bool hal_interface_mode_set(uint8_t rim_code, uint8_t dim_code)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.2 (Interface Pixel Format - 0x0D)

    uint8_t params[2] = {rim_code, dim_code};

    if (!spi_transmit_command(0x0D)) return false;
    if (!spi_transmit_data(params, 2)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_partial_mode_set(partial_area_enabled)
 *   {
 *     // Reference: Datasheet Section 8.2.8 (Partial Mode On - 0x12)
 *
 *     if (partial_area_enabled) {
 *       spi_transmit_command(0x12);  // Partial mode on
 *     } else {
 *       spi_transmit_command(0x13);  // Normal mode on (disable partial)
 *     }
 *
 *     return true;
 *   }
 */
bool hal_partial_mode_set(bool partial_area_enabled)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.8 (Partial Mode On - 0x12)

    uint8_t command = partial_area_enabled ? 0x12 : 0x13;

    return spi_transmit_command(command);
}

/**
 * Pseudocode Implementation:
 *   hal_scroll_area_set(top_fixed_lines, scroll_area_lines, bottom_fixed_lines)
 *   {
 *     // Reference: Datasheet Section 8.2.21 (Vertical Scrolling Definition - 0x33)
 *
 *     // Command 0x33 takes 6 parameters:
 *     // Bytes 0-1: TFA (Top Fixed Area) - MSB first
 *     // Bytes 2-3: VSA (Vertical Scroll Area) - MSB first
 *     // Bytes 4-5: BFA (Bottom Fixed Area) - MSB first
 *
 *     uint8_t params[6];
 *     _address_to_bytes(top_fixed_lines, &params[0], &params[1]);
 *     _address_to_bytes(scroll_area_lines, &params[2], &params[3]);
 *     _address_to_bytes(bottom_fixed_lines, &params[4], &params[5]);
 *
 *     spi_transmit_command(0x33);
 *     spi_transmit_data(params, 6);
 *
 *     return true;
 *   }
 */
bool hal_scroll_area_set(uint16_t top_fixed_lines,
                         uint16_t scroll_area_lines,
                         uint16_t bottom_fixed_lines)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.21 (Vertical Scrolling Definition - 0x33)

    uint8_t params[6];
    _address_to_bytes(top_fixed_lines, &params[0], &params[1]);
    _address_to_bytes(scroll_area_lines, &params[2], &params[3]);
    _address_to_bytes(bottom_fixed_lines, &params[4], &params[5]);

    if (!spi_transmit_command(0x33)) return false;
    if (!spi_transmit_data(params, 6)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_scroll_start_address_set(start_line)
 *   {
 *     // Reference: Datasheet Section 8.2.22 (Vertical Scrolling Start - 0x37)
 *
 *     uint8_t params[2];
 *     _address_to_bytes(start_line, &params[0], &params[1]);
 *
 *     spi_transmit_command(0x37);
 *     spi_transmit_data(params, 2);
 *
 *     return true;
 *   }
 */
bool hal_scroll_start_address_set(uint16_t start_line)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.22 (Vertical Scrolling Start - 0x37)

    uint8_t params[2];
    _address_to_bytes(start_line, &params[0], &params[1]);

    if (!spi_transmit_command(0x37)) return false;
    if (!spi_transmit_data(params, 2)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_normal_display_mode_on()
 *   {
 *     // Reference: Datasheet Section 8.2.24 (Normal Display Mode On - 0x13)
 *     // Disable inversion mode
 *
 *     spi_transmit_command(0x13);
 *
 *     return true;
 *   }
 */
bool hal_normal_display_mode_on(void)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.24 (Normal Display Mode On - 0x13)

    return spi_transmit_command(0x13);
}

/**
 * Pseudocode Implementation:
 *   hal_invert_display_mode_on()
 *   {
 *     // Reference: Datasheet Section 8.2.23 (Display Inversion On - 0x21)
 *     // Invert all colors
 *
 *     spi_transmit_command(0x21);
 *
 *     return true;
 *   }
 */
bool hal_invert_display_mode_on(void)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.23 (Display Inversion On - 0x21)

    return spi_transmit_command(0x21);
}

/* ============================================================================
 * Extended / Advanced Features
 * ========================================================================== */

/**
 * Pseudocode Implementation:
 *   hal_color_enhancement_set(ace_enabled)
 *   {
 *     // Reference: Datasheet Section 8.2.36 (ACE Function Control - 0xE8)
 *
 *     uint8_t param = ace_enabled ? 0x01 : 0x00;
 *
 *     spi_transmit_command(0xE8);
 *     spi_transmit_data(&param, 1);
 *
 *     return true;
 *   }
 */
bool hal_color_enhancement_set(bool ace_enabled)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.36 (ACE Function Control - 0xE8)

    uint8_t param = ace_enabled ? 0x01 : 0x00;

    if (!spi_transmit_command(0xE8)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_interface_control_set(wemode_enabled)
 *   {
 *     // Reference: Datasheet Section 8.2.37 (Interface Connection Control - 0xC6)
 *
 *     uint8_t param = wemode_enabled ? 0x02 : 0x00;  // WE mode bit
 *
 *     spi_transmit_command(0xC6);
 *     spi_transmit_data(&param, 1);
 *
 *     return true;
 *   }
 */
bool hal_interface_control_set(bool wemode_enabled)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.37 (Interface Connection Control - 0xC6)

    uint8_t param = wemode_enabled ? 0x02 : 0x00;

    if (!spi_transmit_command(0xC6)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_ram_protection_set(protection_enabled, key_code)
 *   {
 *     // Reference: Datasheet Section 8.2.35 (RAM Write Protection - 0xC9)
 *
 *     if (protection_enabled) {
 *       spi_transmit_command(0xC9);
 *       spi_transmit_data(&key_code, 1);
 *     } else {
 *       // Disable by sending 0x00
 *       uint8_t disable_code = 0x00;
 *       spi_transmit_command(0xC9);
 *       spi_transmit_data(&disable_code, 1);
 *     }
 *
 *     return true;
 *   }
 */
bool hal_ram_protection_set(bool protection_enabled, uint8_t key_code)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.35 (RAM Write Protection - 0xC9)

    uint8_t param = protection_enabled ? key_code : 0x00;

    if (!spi_transmit_command(0xC9)) return false;
    if (!spi_transmit_data(&param, 1)) return false;

    return true;
}

/**
 * Pseudocode Implementation:
 *   hal_gpio_configure(gpio_mask, gpio_config)
 *   {
 *     // Reference: Datasheet Section 8.2.34 (GPIO Configuration - 0xF7)
 *     // Note: Not all ILI9488 variants include GPIO support
 *
 *     uint8_t params[2] = {gpio_mask, gpio_config};
 *
 *     spi_transmit_command(0xF7);
 *     spi_transmit_data(params, 2);
 *
 *     return true;
 *   }
 */
bool hal_gpio_configure(uint8_t gpio_mask, uint8_t gpio_config)
{
    /* PSEUDOCODE */

    // Reference: Datasheet Section 8.2.34 (GPIO Configuration - 0xF7)

    uint8_t params[2] = {gpio_mask, gpio_config};

    if (!spi_transmit_command(0xF7)) return false;
    if (!spi_transmit_data(params, 2)) return false;

    return true;
}
