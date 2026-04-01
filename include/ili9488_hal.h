/**
 * @file ili9488_hal.h
 * @brief ILI9488 Hardware Abstraction Layer (HAL)
 *
 * This layer provides high-level command interface to the ILI9488 display.
 * All command logic and sequencing is encapsulated here. The layer uses the
 * ili9488_spi module for low-level communication, decoupling hardware details
 * from display logic.
 *
 * All functions map directly to ILI9488 datasheet command sections.
 * References: ILI9488 Datasheet, Section 8 (Command Set)
 */

#ifndef ILI9488_HAL_H
#define ILI9488_HAL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * System Command Enumerations
 * ========================================================================== */

/**
 * @brief Software reset operation scope
 *
 * Reference: Datasheet Section 8.2.1 (Software Reset - 0x01)
 */
typedef enum {
    RESET_COMPLETE = 0x00  /**< Full software reset (default) */
} hal_reset_scope_t;

/**
 * @brief Display power state
 *
 * Reference: Datasheet Section 8.2.9 (Sleep In - 0x10)
 *           Datasheet Section 8.2.10 (Sleep Out - 0x11)
 *           Datasheet Section 8.2.6 (Display On - 0x29)
 *           Datasheet Section 8.2.7 (Display Off - 0x28)
 */
typedef enum {
    POWER_STATE_SLEEP = 0,      /**< Sleep mode (low power) */
    POWER_STATE_NORMAL = 1,     /**< Normal operating mode */
    POWER_STATE_DISPLAY_OFF = 2 /**< Display off (content preserved in GRAM) */
} hal_power_state_t;

/**
 * @brief Pixel color format (bits per pixel)
 *
 * Reference: Datasheet Section 8.2.5 (Pixel Format Set - 0x3A)
 * Sets the color depth for all pixel data transfers.
 */
typedef enum {
    PIXEL_FORMAT_12BIT = 0x03,  /**< 4096 colors (12 bits per pixel) */
    PIXEL_FORMAT_16BIT = 0x05,  /**< 65536 colors (16 bits per pixel, RGB565) */
    PIXEL_FORMAT_18BIT = 0x06   /**< 262144 colors (18 bits per pixel) */
} hal_pixel_format_t;

/**
 * @brief Display rotation and mirror settings
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * Controls how framebuffer data is mapped to display output.
 */
typedef enum {
    ROTATION_0_NORMAL = 0x00,      /**< 0°: Portrait (default), X→right, Y→down */
    ROTATION_90_CLOCKWISE = 0x60,  /**< 90°: X→down, Y→left */
    ROTATION_180_INVERSE = 0xC0,   /**< 180°: Landscape inverted */
    ROTATION_270_COUNTERCLOKWISE = 0xA0 /**< 270°: X→up, Y→right */
} hal_rotation_t;

/**
 * @brief Horizontal refresh direction
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * MX bit: column address direction
 */
typedef enum {
    HORIZONAL_DIRECTION_DEFAULT = 0,    /**< Left to right (default) */
    HORIZONAL_DIRECTION_REVERSED = 1    /**< Right to left (mirrored) */
} hal_horizontal_direction_t;

/**
 * @brief Vertical refresh direction
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * MY bit: row address direction
 */
typedef enum {
    VERTICAL_DIRECTION_DEFAULT = 0,     /**< Top to bottom (default) */
    VERTICAL_DIRECTION_REVERSED = 1     /**< Bottom to top (flipped) */
} hal_vertical_direction_t;

/**
 * @brief Display interface pixel format mode
 *
 * Reference: Datasheet Section 8.2.4 (Display Mode Set - 0x13)
 * Controls whether pixels are transmitted as RGB or BGR order.
 */
typedef enum {
    BYTE_ORDER_RGB = 0,  /**< RGB byte order (R, G, B) */
    BYTE_ORDER_BGR = 1   /**< BGR byte order (B, G, R) */
} hal_byte_order_t;

/**
 * @brief Interface mode for column/row iteration
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * Determines whether pixels are written by column or row.
 */
typedef enum {
    TRANSFER_MODE_NORMAL = 0,        /**< Write pixels row-by-row */
    TRANSFER_MODE_PAGE_ADDRESS = 1   /**< Write pixels column-by-column */
} hal_transfer_mode_t;

/* ============================================================================
 * Color Configuration Enumerations
 * ========================================================================== */

/**
 * @brief Gamma curve selection
 *
 * Reference: Datasheet Section 8.2.26 (Gamma Set - 0x26)
 * Selects which gamma correction curve to apply to display output.
 */
typedef enum {
    GAMMA_CURVE_1 = 0x01,  /**< Gamma Curve 1 */
    GAMMA_CURVE_2 = 0x02,  /**< Gamma Curve 2 */
    GAMMA_CURVE_3 = 0x04,  /**< Gamma Curve 3 */
    GAMMA_CURVE_4 = 0x08   /**< Gamma Curve 4 */
} hal_gamma_curve_t;

/**
 * @brief Power control mode
 *
 * Reference: Datasheet Section 8.2.13 (Power Control 1 - 0xC0)
 * Adjusts GVDD voltage for power efficiency and display quality.
 */
typedef enum {
    VCORE_VOLTAGE_SOURCE_INTERNAL = 0,  /**< Internal charge pump */
    VCORE_VOLTAGE_SOURCE_EXTERNAL = 1   /**< External source */
} hal_vcore_source_t;

/**
 * @brief Backlight PWM frequency (if supported)
 *
 * Note: ILI9488 does not include integrated PWM, but frequency may be
 * relevant if using external backlight controller on same SPI interface.
 */
typedef enum {
    PWM_FREQUENCY_3_9KHZ = 0x00,
    PWM_FREQUENCY_7_8KHZ = 0x01,
    PWM_FREQUENCY_15_6KHZ = 0x02,
    PWM_FREQUENCY_31_2KHZ = 0x03
} hal_pwm_frequency_t;

/* ============================================================================
 * Initialization & Configuration
 * ========================================================================== */

/**
 * @brief Initialize ILI9488 display hardware
 *
 * Performs complete initialization sequence including:
 *   1. Hardware reset (RSTB sequence)
 *   2. Power supply configuration
 *   3. Display interface setup
 *   4. Timing and oscillator configuration
 *   5. Display turn-on sequence
 *
 * Reference: Datasheet Section 4 (Initialization and Power Supply)
 *           Datasheet Section 9 (Timing Specifications)
 *
 * Pseudocode:
 *   // Hardware reset sequence
 *   1. Pull RSTB low for 1 ms
 *   2. Release RSTB high
 *   3. Wait 120 ms for oscillator stabilization
 *
 *   // Soft reset and power-up
 *   4. Execute software reset (0x01)
 *   5. Wait 5 ms
 *   6. Execute sleep out (0x11)
 *   7. Wait 120 ms
 *
 *   // Configure display interface
 *   8. Set pixel format to RGB565 (0x3A with 0x05)
 *   9. Set memory access control/rotation (0x36)
 *   10. Set display mode (0x13 - optional)
 *
 *   // Configure power and timing
 *   11. Execute power control sequence (0xC0, 0xC1, 0xC5)
 *   12. Set frame rate controller (0xB1)
 *   13. Configure gamma correction (0xE0, 0xE1)
 *
 *   // Display enable
 *   14. Set column address (0x2A): 0-319
 *   15. Set row address (0x2B): 0-479
 *   16. Set pixel format again (0x3A)
 *   17. Normal mode on (0x13)
 *   18. Display on (0x29)
 *
 * @param pixel_format Target color depth (RGB565, RGB444, RGB666)
 * @param rotation Display rotation angle
 * @return true if initialization successful, false otherwise
 */
bool hal_display_initialize(hal_pixel_format_t pixel_format,
                            hal_rotation_t rotation);

/**
 * @brief Deinitialize and power down display
 *
 * Safely shuts down the display:
 *   1. Turn off display (0x28)
 *   2. Put into sleep mode (0x10)
 *   3. Power off display module
 *   4. Release resources
 *
 * Reference: Datasheet Section 8.2.6 (Display Off - 0x28)
 *           Datasheet Section 8.2.9 (Sleep In - 0x10)
 * @return true if successful, false otherwise
 */
bool hal_display_deinitialize(void);

/* ============================================================================
 * System Control Commands
 * ========================================================================== */

/**
 * @brief Execute software reset
 *
 * Resets all display controller registers to default values.
 * Equivalent to a power-on reset but software-triggered.
 * Must wait at least 5 ms after reset before next command.
 *
 * Reference: Datasheet Section 8.2.1 (Software Reset - 0x01)
 *
 * Pseudocode:
 *   1. Transmit command 0x01
 *   2. Wait 5 ms for reset completion
 *
 * @param reset_scope Type of reset (currently only COMPLETE is supported)
 * @return true if reset successful, false otherwise
 */
bool hal_system_reset(hal_reset_scope_t reset_scope);

/**
 * @brief Put display into sleep mode
 *
 * Enters sleep mode to reduce power consumption. Display content
 * is preserved in GRAM but not displayed. SPI interface remains active.
 *
 * Reference: Datasheet Section 8.2.9 (Sleep In - 0x10)
 * Must wait at least 5 ms after entering sleep before next command.
 *
 * @param enable true to sleep, false to wake (use hal_power_set_state instead)
 * @return true if command successful, false otherwise
 */
bool hal_power_sleep_mode_set(bool enable);

/**
 * @brief Exit sleep mode (power on display)
 *
 * Exits sleep mode and brings display back to normal operation.
 * Equivalent to powering on after soft reset.
 *
 * Reference: Datasheet Section 8.2.10 (Sleep Out - 0x11)
 * Must wait at least 120 ms after sleep out before displaying content.
 *
 * Pseudocode:
 *   1. Transmit command 0x11
 *   2. Wait 120 ms for power stabilization
 *
 * @return true if command successful, false otherwise
 */
bool hal_power_sleep_exit(void);

/**
 * @brief Control display on/off state
 *
 * Turns the display output on or off without affecting GRAM content
 * or controller operation. Different from sleep mode.
 *
 * Reference: Datasheet Section 8.2.6 (Display Off - 0x28)
 *           Datasheet Section 8.2.7 (Display On - 0x29)
 *
 * @param enabled true for display on, false for display off
 * @return true if command successful, false otherwise
 */
bool hal_display_output_control(bool enabled);

/**
 * @brief Set display power state
 *
 * Convenience function for managing overall display power:
 *   - SLEEP: Reduces power consumption, GRAM preserved
 *   - NORMAL: Normal operation, display on and active
 *   - DISPLAY_OFF: Display off but controller active
 *
 * Reference: Datasheet Section 8.2.6-11 (Power Control Commands)
 *
 * @param power_state Target power state
 * @return true if state transition successful, false otherwise
 */
bool hal_power_set_state(hal_power_state_t power_state);

/* ============================================================================
 * Display Configuration Commands
 * ========================================================================== */

/**
 * @brief Set pixel color format
 *
 * Configures color depth for all subsequent data/pixel transfers.
 * Must be set before writing pixel data to GRAM.
 *
 * Reference: Datasheet Section 8.2.5 (Pixel Format Set - 0x3A)
 *
 * Pseudocode:
 *   1. Transmit command 0x3A
 *   2. Transmit parameter byte (DPI field):
 *      - 0x03 for 12 bits per pixel (4096 colors)
 *      - 0x05 for 16 bits per pixel (65536 colors, RGB565)
 *      - 0x06 for 18 bits per pixel (262144 colors)
 *   3. Parameter is stored in DPI bits [2:0] only
 *
 * @param pixel_format Color depth setting (RGB565 recommended)
 * @return true if command successful, false otherwise
 */
bool hal_pixel_format_set(hal_pixel_format_t pixel_format);

/**
 * @brief Set display rotation and orientation
 *
 * Controls memory-to-display mapping for all subsequent operations.
 * Allows 90° rotations and mirror/flip transformations without
 * rearranging GRAM data.
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *
 * Pseudocode:
 *   1. Transmit command 0x36
 *   2. Transmit parameter with control bits:
 *      - MY [7]: Row address direction (0=top-bottom, 1=bottom-top)
 *      - MX [6]: Column address direction (0=left-right, 1=right-left)
 *      - MV [5]: Swap X/Y (0=normal, 1=transpose)
 *      - ... other bits for RGB/BGR, refresh direction
 *   3. Selected rotation pre-encodes optimal MY/MX/MV values
 *
 * @param rotation Target rotation (0°, 90°, 180°, 270°)
 * @param horizontal_flip Reverse left-right direction
 * @param vertical_flip Reverse top-bottom direction
 * @return true if command successful, false otherwise
 */
bool hal_display_rotation_set(hal_rotation_t rotation,
                              hal_horizontal_direction_t horizontal_flip,
                              hal_vertical_direction_t vertical_flip);

/**
 * @brief Set display byte order for colors
 *
 * Selects RGB vs. BGR byte order for pixel data.
 * Must be configured before writing pixel data.
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * RGB bit [3]: 0 for RGB mode (R first), 1 for BGR mode (B first)
 *
 * Pseudocode:
 *   1. Read current memory access control byte (0x36)
 *   2. Modify RGB bit [3]:
 *      - 0 for RGB byte order
 *      - 1 for BGR byte order
 *   3. Write back modified byte via 0x36
 *
 * @param byte_order RGB or BGR order
 * @return true if command successful, false otherwise
 */
bool hal_color_byte_order_set(hal_byte_order_t byte_order);

/**
 * @brief Set GRAM addressing mode
 *
 * Controls how address pointers increment when writing pixel data.
 * Determines row-wise vs. column-wise pixel transmission order.
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 * MV bit [5]: 0 for row addressing, 1 for column addressing
 *
 * @param transfer_mode Row-wise or column-wise addressing
 * @return true if command successful, false otherwise
 */
bool hal_transfer_mode_set(hal_transfer_mode_t transfer_mode);

/**
 * @brief Get current display configuration
 *
 * Reads the Memory Access Control register (0x36) to determine
 * current rotation, mirror, and byte order settings.
 *
 * Reference: Datasheet Section 8.2.3 (Memory Access Control - 0x36)
 *
 * Pseudocode:
 *   1. Transmit command 0x36
 *   2. Receive parameter byte
 *   3. Return configuration bits to caller
 *
 * @param config_byte Pointer to store current MAC register value
 * @return true if read successful, false otherwise
 */
bool hal_display_config_read(uint8_t *config_byte);

/* ============================================================================
 * Memory Address & Access Commands
 * ========================================================================== */

/**
 * @brief Set column address range
 *
 * Defines the horizontal address window [start_column : end_column].
 * All subsequent pixel writes target this range.
 *
 * Reference: Datasheet Section 8.2.18 (Column Address Set - 0x2A)
 *
 * Pseudocode:
 *   1. Transmit command 0x2A
 *   2. Transmit 4-byte parameter:
 *      - Bytes [0:1]: Column start address (16-bit big-endian)
 *      - Bytes [2:3]: Column end address (16-bit big-endian)
 *   3. Both addresses are 0-based, must be < 320 for normal orientation
 *
 * @param start_column Leftmost column (0-319)
 * @param end_column Rightmost column (0-319)
 * @return true if command successful, false otherwise
 */
bool hal_column_address_set(uint16_t start_column, uint16_t end_column);

/**
 * @brief Set row address range
 *
 * Defines vertical address window [start_row : end_row].
 * All subsequent pixel writes target this range.
 *
 * Reference: Datasheet Section 8.2.19 (Page Address Set - 0x2B)
 *
 * Pseudocode:
 *   1. Transmit command 0x2B
 *   2. Transmit 4-byte parameter:
 *      - Bytes [0:1]: Page (row) start address (16-bit big-endian)
 *      - Bytes [2:3]: Page (row) end address (16-bit big-endian)
 *   3. Both addresses are 0-based, must be < 480 for normal orientation
 *
 * @param start_row Top row (0-479)
 * @param end_row Bottom row (0-479)
 * @return true if command successful, false otherwise
 */
bool hal_row_address_set(uint16_t start_row, uint16_t end_row);

/**
 * @brief Set complete screen address window
 *
 * Convenience function to set both column and row address ranges
 * for a rectangular screen region.
 *
 * Reference: Datasheet Section 8.2.18-19 (Column/Page Address Set)
 *
 * Pseudocode:
 *   1. Call hal_column_address_set(x_start, x_end)
 *   2. Call hal_row_address_set(y_start, y_end)
 *   3. Return combined status
 *
 * @param x_start Leftmost column (0-319)
 * @param x_end Rightmost column (0-319)
 * @param y_start Top row (0-479)
 * @param y_end Bottom row (0-479)
 * @return true if both commands successful, false otherwise
 */
bool hal_window_address_set(uint16_t x_start, uint16_t x_end,
                            uint16_t y_start, uint16_t y_end);

/**
 * @brief Begin GRAM write sequence
 *
 * Prepares display for pixel data transfer. All subsequent data
 * writes are directed to the memory window defined by last address
 * commands. GRAM pointer auto-increments with each pixel.
 *
 * Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)
 *
 * Pseudocode:
 *   1. Transmit command 0x2C
 *   2. (No parameters required)
 *   3. Next SPI data transmissions go directly to GRAM
 *
 * Typical usage pattern:
 *   1. hal_window_address_set(0, 319, 0, 479)  // Set full screen
 *   2. hal_gram_write_start()                   // Enter GRAM write mode
 *   3. spi_transmit_bulkdata(pixel_buffer, 307200)  // Send all 320x480 pixels
 *
 * @return true if command successful, false otherwise
 */
bool hal_gram_write_start(void);

/**
 * @brief Begin GRAM read sequence
 *
 * Prepares display for pixel data readback. Reads from the memory
 * window defined by last address commands.
 *
 * Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)
 * Note: First byte read is typically status, valid pixel data follows.
 *
 * Pseudocode:
 *   1. Transmit command 0x2E
 *   2. (No parameters)
 *   3. Next SPI reads retrieve GRAM content
 *
 * @return true if command successful, false otherwise
 */
bool hal_gram_read_start(void);

/* ============================================================================
 * Pixel Data Transmission
 * ========================================================================== */

/**
 * @brief Write pixel data to GRAM
 *
 * Transmits RGB pixel data to the display after hal_gram_write_start()
 * has been called. GRAM address auto-increments to next pixel.
 *
 * Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)
 * Data format depends on previously set pixel format (RGB565 typical).
 *
 * Pseudocode:
 *   1. Check pixel_format for current color depth
 *   2. Transmit pixel_buffer via SPI data line
 *   3. Display GRAM address auto-increments
 *
 * Example for RGB565 (16 bits per pixel):
 *   - Each pixel is 2 bytes: [R5G6][B5G3]
 *   - Write 320x480 = 307200 pixels = 614400 bytes total
 *
 * @param pixel_buffer Pointer to RGB pixel data
 * @param pixel_count Number of pixels (not bytes)
 * @param pixel_format Color depth of each pixel
 * @return true if write successful, false otherwise
 */
bool hal_gram_write_pixels(const uint8_t *pixel_buffer,
                           uint32_t pixel_count,
                           hal_pixel_format_t pixel_format);

/**
 * @brief Read pixel data from GRAM
 *
 * Retrieves RGB pixel data from display after hal_gram_read_start()
 * has been called. GRAM address auto-increments to next pixel.
 *
 * Reference: Datasheet Section 8.2.27 (Memory Read - 0x2E)
 * Note: First byte returned is typically status/dummy, skip it.
 *
 * @param pixel_buffer Pointer to storage for retrieved pixels
 * @param pixel_count Number of pixels to read (not bytes)
 * @param pixel_format Color depth of each pixel (determines data width)
 * @return true if read successful, false otherwise
 */
bool hal_gram_read_pixels(uint8_t *pixel_buffer,
                          uint32_t pixel_count,
                          hal_pixel_format_t pixel_format);

/**
 * @brief Fill rectangular region with solid color
 *
 * High-level convenience function to fill a screen region with
 * a single color. Internally:
 *   1. Sets address window
 *   2. Transmits color value repeatedly
 *
 * Reference: Datasheet Section 8.2.25 (Memory Write - 0x2C)
 *
 * Pseudocode:
 *   1. Call hal_window_address_set(x1, x2, y1, y2)
 *   2. Call hal_gram_write_start()
 *   3. Repeat transmission of color value (pixel_count) times
 *   4. Return status
 *
 * @param x_start Leftmost column
 * @param x_end Rightmost column
 * @param y_start Top row
 * @param y_end Bottom row
 * @param color_rgb565 16-bit RGB565 color value
 * @return true if fill successful, false otherwise
 */
bool hal_fill_rectangle_solid(uint16_t x_start, uint16_t x_end,
                              uint16_t y_start, uint16_t y_end,
                              uint16_t color_rgb565);

/* ============================================================================
 * Power Supply & Voltage Control
 * ========================================================================== */

/**
 * @brief Configure GVDD (core voltage) levels
 *
 * Sets the positive and negative voltage references for the display.
 * Critical for display contrast, brightness, and power efficiency.
 *
 * Reference: Datasheet Section 8.2.13 (Power Control 1 - 0xC0)
 *
 * Pseudocode:
 *   1. Transmit command 0xC0
 *   2. Transmit parameter: GVDD voltage (3 bits, 0x00-0x1F)
 *      Register value = (decimal 3.0 to 4.8V mapped to 0x00-0x2C)
 *   3. Typical value: 0x17 = approximately 4.0V
 *
 * @param gvdd_voltage Voltage level (0x00-0x2C, represents 3.0V-4.8V)
 * @return true if command successful, false otherwise
 */
bool hal_power_gvdd_set(uint8_t gvdd_voltage);

/**
 * @brief Configure VCIOUT voltage
 *
 * Sets the operational voltage for internal amplifiers.
 * Affects display power consumption and performance.
 *
 * Reference: Datasheet Section 8.2.14 (Power Control 2 - 0xC1)
 *
 * @param vci_voltage Voltage level code
 * @return true if command successful, false otherwise
 */
bool hal_power_vci_set(uint8_t vci_voltage);

/**
 * @brief Configure VGH and VGL voltages
 *
 * Sets gate driver voltages (high and low). Critical for proper
 * pixel drive levels and image quality.
 *
 * Reference: Datasheet Section 8.2.15 (Power Control 3 - 0xC5)
 *
 * Pseudocode:
 *   1. Transmit command 0xC5
 *   2. Transmit up to 2 parameter bytes:
 *      - VGH (positive gate voltage)
 *      - VGL (negative gate voltage)
 *   3. Typical: VGH=0x0A (16V), VGL=-0x0A (-10V)
 *
 * @param vgh_voltage Positive gate voltage
 * @param vgl_voltage Negative gate voltage
 * @return true if command successful, false otherwise
 */
bool hal_power_vgh_vgl_set(uint8_t vgh_voltage, uint8_t vgl_voltage);

/**
 * @brief Configure VCOMH voltage
 *
 * Sets the VCOM reference voltage used for LCD common electrode.
 * Affects contrast and gray levels.
 *
 * Reference: Datasheet Section 8.2.17 (VCOM Control - 0xBB)
 *
 * @param vcomh_voltage VCOM voltage code
 * @return true if command successful, false otherwise
 */
bool hal_power_vcomh_set(uint8_t vcomh_voltage);

/* ============================================================================
 * Display Timing & Frequency Control
 * ========================================================================== */

/**
 * @brief Configure frame rate
 *
 * Sets the display refresh rate (frames per second).
 * Typical value: 60 Hz for smooth animation and low flicker.
 *
 * Reference: Datasheet Section 8.2.12 (Frame Rate Control - 0xB1)
 *
 * Pseudocode:
 *   1. Transmit command 0xB1
 *   2. Transmit parameter: frame rate control field
 *      - Bits [4:0]: Divisor for pixel clock
 *      - Output Hz = fOSC / (divisor * pixel_count)
 *      - Typical: 0x18 = 60 Hz
 *   3. Return status
 *
 * @param frame_rate_code Frame rate control code (typically 0x18 = 60 Hz)
 * @return true if command successful, false otherwise
 */
bool hal_frame_rate_set(uint8_t frame_rate_code);

/**
 * @brief Configure oscillator frequency
 *
 * Sets the internal clock frequency for the display controller.
 * Affects timing of all internal operations.
 *
 * Reference: Datasheet Section 8.2.11 (Oscillator Control - 0xB0)
 *
 * @param osc_control_code Oscillator configuration code
 * @return true if command successful, false otherwise
 */
bool hal_oscillator_frequency_set(uint8_t osc_control_code);

/* ============================================================================
 * Gamma Correction
 * ========================================================================== */

/**
 * @brief Select gamma curve
 *
 * Chooses one of the predefined gamma correction curves (1, 2, 3, 4).
 * Gamma corrects pixel values for accurate color rendering.
 *
 * Reference: Datasheet Section 8.2.26 (Gamma Set - 0x26)
 *
 * Pseudocode:
 *   1. Transmit command 0x26
 *   2. Transmit parameter:
 *      - 0x01 for Gamma 1
 *      - 0x02 for Gamma 2
 *      - 0x04 for Gamma 3 (recommended)
 *      - 0x08 for Gamma 4
 *
 * @param gamma_curve Gamma curve selection (1, 2, 3, or 4)
 * @return true if command successful, false otherwise
 */
bool hal_gamma_curve_select(hal_gamma_curve_t gamma_curve);

/**
 * @brief Program custom gamma correction values
 *
 * Uploads custom gamma lookup table values for fine-tuned
 * color correction. Each defines 15-level gamma curve.
 *
 * Reference: Datasheet Section 8.2.27-28 (Positive/Negative Gamma - 0xE0/0xE1)
 *
 * Pseudocode:
 *   1. Transmit command 0xE0 (positive gamma)
 *   2. Transmit 15 bytes of gamma values
 *   3. Transmit command 0xE1 (negative gamma)
 *   4. Transmit 15 bytes of gamma values
 *   5. Return status
 *
 * Gamma values map gray levels [0-255] to output intensities.
 * Typical table: [0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
 *                 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00]
 *
 * @param pos_gamma_table Pointer to 15-byte positive gamma table
 * @param neg_gamma_table Pointer to 15-byte negative gamma table
 * @return true if programming successful, false otherwise
 */
bool hal_gamma_curve_program(const uint8_t *pos_gamma_table,
                             const uint8_t *neg_gamma_table);

/* ============================================================================
 * Status & Information Queries
 * ========================================================================== */

/**
 * @brief Read display identification number
 *
 * Retrieves the ILI9488 IC identification code (should be 0x9488).
 * Useful for verifying hardware connectivity.
 *
 * Reference: Datasheet Section 8.2.33 (Read ID4 - 0xD3)
 *
 * Pseudocode:
 *   1. Transmit command 0xD3
 *   2. Receive 4 bytes:
 *      - Byte 1: Dummy/Status
 *      - Bytes 2-4: ID code (should be 0x009488)
 *   3. Return ID value
 *
 * @param id_code Pointer to store 3-byte ID code
 * @return true if read successful, false otherwise
 */
bool hal_display_id_read(uint32_t *id_code);

/**
 * @brief Read display power mode
 *
 * Retrieves current power state (sleep, display on/off, etc.).
 * Useful for status monitoring and debugging.
 *
 * Reference: Datasheet Section 8.2.29 (Read Power Mode - 0x0A)
 *
 * Pseudocode:
 *   1. Transmit command 0x0A
 *   2. Receive 2 bytes:
 *      - Byte 1: Dummy/Status
 *      - Byte 2: Power mode register
 *   3. Extract relevant status bits
 *
 * @param power_mode Pointer to store power mode byte
 * @return true if read successful, false otherwise
 */
bool hal_power_mode_read(uint8_t *power_mode);

/**
 * @brief Read display status register
 *
 * Retrieves detailed display status flags (display on/off, sleep,
 * booster, gamma correction, etc.).
 *
 * Reference: Datasheet Section 8.2.32 (Read Display Status - 0x0D)
 *
 * @param status_byte Pointer to store status register value
 * @return true if read successful, false otherwise
 */
bool hal_display_status_read(uint8_t *status_byte);

/**
 * @brief Read signal mode (display/orientation info)
 *
 * Retrieves information about current display mode settings:
 * rotation, color format, addressing mode, etc.
 *
 * Reference: Datasheet Section 8.2.30 (Read Display Mode - 0x0B)
 *
 * @param display_mode Pointer to store display mode byte
 * @return true if read successful, false otherwise
 */
bool hal_display_mode_read(uint8_t *display_mode);

/**
 * @brief Read pixel format currently in use
 *
 * Retrieves the active pixel format (12/16/18-bit) and color space.
 *
 * Reference: Datasheet Section 8.2.31 (Read Pixel Format - 0x0C)
 *
 * @param pixel_format Pointer to store pixel format byte
 * @return true if read successful, false otherwise
 */
bool hal_pixel_format_read(uint8_t *pixel_format);

/* ============================================================================
 * Interface & Advanced Configuration
 * ========================================================================== */

/**
 * @brief Configure interface memory format (optional)
 *
 * Sets detailed RIM (ROM Interface Mode) and DIM (Display Interface Mode).
 * Affects data pin assignment and command interface details.
 *
 * Reference: Datasheet Section 8.2.2 (Interface Pixel Format - 0x0D)
 *
 * @param rim_code ROM interface mode code
 * @param dim_code Display interface mode code
 * @return true if configuration successful, false otherwise
 */
bool hal_interface_mode_set(uint8_t rim_code, uint8_t dim_code);

/**
 * @brief Set partial display mode
 *
 * Enters partial display mode where only a specified region is
 * refreshed to save power. Regions outside partial area are black.
 *
 * Reference: Datasheet Section 8.2.8 (Partial Mode On - 0x12)
 *           Datasheet Section 8.2.20-21 (Partial Area / Scroll Area)
 *
 * Pseudocode:
 *   1. Transmit command 0x12 (or 0x30 for partial area definition)
 *   2. Transmit address window for partial region
 *   3. Only pixels in this region are updated; outside area is blank
 *
 * @param partial_area_enabled true to enable partial mode
 * @return true if command successful, false otherwise
 */
bool hal_partial_mode_set(bool partial_area_enabled);

/**
 * @brief Define scrolling area
 *
 * Sets a scrolling window region for vertical/horizontal scrolling
 * operations. Pixels within TFA/BFA scroll; others fixed.
 *
 * Reference: Datasheet Section 8.2.21 (Vertical Scrolling Definition - 0x33)
 *
 * Pseudocode:
 *   1. Transmit command 0x33
 *   2. Transmit 6 parameters:
 *      - TFA (Top Fixed Area): rows fixed at top [0 : TFA-1]
 *      - VSA (Vertical Scroll Area): rows to scroll [TFA : TFA+VSA-1]
 *      - BFA (Bottom Fixed Area): rows fixed at bottom [TFA+VSA : 479]
 *   3. Later, VSP (0x36) offsets VSA rows
 *
 * @param top_fixed_lines Rows fixed at top
 * @param scroll_area_lines Rows in scroll region
 * @param bottom_fixed_lines Rows fixed at bottom
 * @return true if configuration successful, false otherwise
 */
bool hal_scroll_area_set(uint16_t top_fixed_lines,
                         uint16_t scroll_area_lines,
                         uint16_t bottom_fixed_lines);

/**
 * @brief Set vertical scroll offset
 *
 * Offsets the vertical scroll region by N rows (wraps around).
 * Used in conjunction with hal_scroll_area_set().
 *
 * Reference: Datasheet Section 8.2.22 (Vertical Scrolling Start Address - 0x37)
 *
 * @param start_line Scroll start offset (0-479)
 * @return true if command successful, false otherwise
 */
bool hal_scroll_start_address_set(uint16_t start_line);

/**
 * @brief Normal display mode on
 *
 * Disables inversion mode if currently enabled.
 * All pixels display at normal intensity levels.
 *
 * Reference: Datasheet Section 8.2.24 (Normal Display Mode On - 0x13)
 *
 * @return true if command successful, false otherwise
 */
bool hal_normal_display_mode_on(void);

/**
 * @brief Invert display mode on
 *
 * Inverts all colors: black becomes white, white becomes black, etc.
 * Useful for some display modes or special effects.
 *
 * Reference: Datasheet Section 8.2.23 (Display Inversion On - 0x21)
 *
 * @return true if command successful, false otherwise
 */
bool hal_invert_display_mode_on(void);

/* ============================================================================
 * Extended / Advanced Features
 * ========================================================================== */

/**
 * @brief Configure area color enhancement (ACE)
 *
 * Enables adaptive color enhancement to improve displayed colors.
 * Typically used for RGB666 color format.
 *
 * Reference: Datasheet Section 8.2.36 (ACE Function Control - 0xE8)
 *
 * @param ace_enabled true to enable color enhancement
 * @return true if command successful, false otherwise
 */
bool hal_color_enhancement_set(bool ace_enabled);

/**
 * @brief Configure interface connection control
 *
 * Sets WEMODE (write enable emulation) and related 3-wire/4-wire
 * SPI interface options.
 *
 * Reference: Datasheet Section 8.2.37 (Interface Connection Control - 0xC6)
 *
 * @param wemode_enabled true to enable write enable emulation
 * @return true if command successful, false otherwise
 */
bool hal_interface_control_set(bool wemode_enabled);

/**
 * @brief Set memory protection key
 *
 * Enables/disables memory write protection. When enabled, only
 * commands matching a specific code can write to GRAM.
 *
 * Reference: Datasheet Section 8.2.35 (RAM Write Protection - 0xC9)
 *
 * @param protection_enabled true to enable write protection
 * @param key_code Protection key code (if enabled)
 * @return true if command successful, false otherwise
 */
bool hal_ram_protection_set(bool protection_enabled, uint8_t key_code);

/**
 * @brief Configure GPIO pins (if ILI9488 variant supports it)
 *
 * Some ILI9488 variants include programmable GPIO outputs.
 * This configures their behavior (pulled not available on all chips).
 *
 * Reference: Datasheet Section 8.2.34 (GPIO Configuration - 0xF7)
 *
 * @param gpio_mask Bitmask of GPIO pins to configure
 * @param gpio_config Configuration values for enabled pins
 * @return true if configuration successful, false otherwise
 */
bool hal_gpio_configure(uint8_t gpio_mask, uint8_t gpio_config);

#endif /* ILI9488_HAL_H */
