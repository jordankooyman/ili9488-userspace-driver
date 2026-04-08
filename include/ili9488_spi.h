/**
 * @file ili9488_spi.h
 * @brief ILI9488 SPI Interface Abstraction Layer
 *
 * This layer provides platform-independent abstractions for SPI communication
 * and GPIO control. All low-level hardware interactions are isolated here,
 * making it easy to port between platforms (Linux spidev, Arduino, ESP32, etc.).
 *
 * References: ILI9488 Datasheet, Section 6 (SPI Serial Interface)
 *
 * AI Usage Disclaimer: This file was mostly outlined then generated using AI tools. See ./AI_chats for the full conversation logs as best as could be exported.
 */

#ifndef ILI9488_SPI_H
#define ILI9488_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * SPI Configuration & Initialization
 * ========================================================================== */

/**
 * @brief SPI bus state initialization
 *
 * Configures the SPI bus frequency, mode, and data order. Must be called
 * before any other SPI operations.
 *
 * Reference: Datasheet Section 6.1 (Serial Interface Characteristics)
 * - Max SPI clock: 10 MHz for write, 5 MHz for read
 * - SPI Mode: Mode 0 (CPOL=0, CPHA=0)
 * - Data format: MSB-first
 *
 * @param spi_device_path Path to the SPI device (e.g., "/dev/spidev0.0")
 * @param clock_speed_hz Target SPI clock frequency in Hz
 * @return true if initialization successful, false otherwise
 */
bool spi_bus_initialize(const char *spi_device_path, uint32_t clock_speed_hz);

/**
 * @brief Release SPI bus resources
 *
 * Closes the SPI device handle and releases associated resources.
 * Must be called when SPI communication is complete.
 *
 * @return true if deinitialization successful, false otherwise
 */
bool spi_bus_deinitialize(void);

/* ============================================================================
 * GPIO Configuration & Control
 * ========================================================================== */

/**
 * @brief GPIO output pin mode enumeration
 *
 * Defines the functional role of GPIO pins used in ILI9488 communication.
 * References: Datasheet Section 5.2 (Pin Function Description)
 */
typedef enum {
    GPIO_RESET = 0,      /**< Active-low hardware reset (RSTB) - Pin A.18 */
    GPIO_DC_SELECT = 1,  /**< Data/Command select (A0/D/C) - Pin A.22 */
    GPIO_CHIP_SELECT = 2 /**< Chip select (CSB) - typically handled by SPI driver */
} spi_gpio_pin_t;

/**
 * @brief GPIO output state enumeration
 *
 * Logical state values for GPIO control.
 */
typedef enum {
    GPIO_STATE_LOW = 0,  /**< Low voltage level (0V) */
    GPIO_STATE_HIGH = 1  /**< High voltage level (3.3V) */
} spi_gpio_state_t;

/**
 * @brief Initialize a GPIO pin for output
 *
 * Configures a GPIO pin as a digital output with optional initial state.
 * Must be called for each GPIO before using it.
 *
 * Reference: Datasheet Section 5.2 (Pin Function Description)
 * - RSTB (Pin A.18): Must be pulled high during operation
 * - A0/D/C (Pin A.22): Selects command (low) vs. parameter/data (high) mode
 *
 * @param gpio_pin GPIO pin identifier (from spi_gpio_pin_t)
 * @param initial_state Initial GPIO voltage state
 * @return true if initialization successful, false otherwise
 */
bool spi_gpio_initialize(spi_gpio_pin_t gpio_pin, spi_gpio_state_t initial_state);

/**
 * @brief Set GPIO output state
 *
 * Changes the voltage level of a GPIO pin (high or low).
 * Used for reset sequencing and command/data mode selection.
 *
 * @param gpio_pin GPIO pin identifier
 * @param state Target GPIO state
 * @return true if state change successful, false otherwise
 */
bool spi_gpio_set_state(spi_gpio_pin_t gpio_pin, spi_gpio_state_t state);

/**
 * @brief Read current GPIO input state (if pin is configured as input)
 *
 * Reads the current voltage level of a GPIO pin.
 *
 * @param gpio_pin GPIO pin identifier
 * @param state Pointer to store the read state
 * @return true if read successful, false otherwise
 */
bool spi_gpio_read_state(spi_gpio_pin_t gpio_pin, spi_gpio_state_t *state);

/**
 * @brief Release GPIO pin resources
 *
 * Releases a GPIO pin and restores it to default state.
 *
 * @param gpio_pin GPIO pin identifier
 * @return true if deinitialization successful, false otherwise
 */
bool spi_gpio_deinitialize(spi_gpio_pin_t gpio_pin);

/* ============================================================================
 * SPI Transmit & Receive Operations
 * ========================================================================== */

/**
 * @brief Transmit command to ILI9488
 *
 * Sends a single-byte command code with D/C pin held LOW.
 * Command codes are 8-bit values from ILI9488 command set.
 *
 * Reference: Datasheet Section 6.2.1 (Writing Commands)
 * - D/C pin must be LOW for command transmission
 * - SPI sends MSB-first
 * - Maximum clock: 10 MHz
 *
 * Pseudocode:
 *   1. Pull D/C low (command mode)
 *   2. Transmit 1-byte command code via SPI
 *   3. D/C state depends on next operation
 *
 * @param command_byte 8-bit command code (0x00-0xFF)
 * @return true if transmission successful, false otherwise
 */
bool spi_transmit_command(uint8_t command_byte);

/**
 * @brief Transmit parameter bytes following a command
 *
 * Sends parameter data bytes with D/C pin held HIGH.
 * Typically called immediately after spi_transmit_command().
 *
 * Reference: Datasheet Section 6.2.2 (Writing Parameters)
 * - D/C pin must be HIGH for parameter transmission
 * - Parameters can be 1 to N bytes (command-specific)
 * - Used for setting registers, addresses, pixel data, etc.
 *
 * Pseudocode:
 *   1. Pull D/C high (parameter/data mode)
 *   2. Transmit N bytes via SPI
 *   3. Return status
 *
 * @param data_buffer Pointer to parameter data array
 * @param byte_count Number of bytes to transmit
 * @return true if transmission successful, false otherwise
 */
bool spi_transmit_data(const uint8_t *data_buffer, size_t byte_count);

/**
 * @brief Receive parameter bytes from ILI9488
 *
 * Reads parameter data bytes with D/C pin held HIGH.
 * Used for reading display status, ID codes, or framebuffer data.
 *
 * Reference: Datasheet Section 6.2.5 (Reading Parameters)
 * - D/C pin must be HIGH for parameter reception
 * - First byte may be dummy/status information (command-specific)
 * - Maximum read clock: 5 MHz
 *
 * Pseudocode:
 *   1. Pull D/C high (parameter/data mode)
 *   2. Receive N bytes via SPI
 *   3. Return received data
 *
 * @param data_buffer Pointer to storage for received bytes
 * @param byte_count Number of bytes to receive
 * @return true if reception successful, false otherwise
 */
bool spi_receive_data(uint8_t *data_buffer, size_t byte_count);

/**
 * @brief Combined command + parameter transmission
 *
 * Convenience function that transmits a command followed immediately
 * by parameter bytes in a single sequence.
 *
 * Reference: Datasheet Section 6.2 (Reading and Writing)
 *
 * Pseudocode:
 *   1. Call spi_transmit_command(command_byte)
 *   2. Call spi_transmit_data(param_buffer, param_count)
 *   3. Return combined status
 *
 * @param command_byte 8-bit command code
 * @param param_buffer Pointer to parameter array (NULL if no parameters)
 * @param param_count Number of parameter bytes (0 if no parameters)
 * @return true if both operations successful, false otherwise
 */
bool spi_transmit_command_with_params(uint8_t command_byte,
                                       const uint8_t *param_buffer,
                                       size_t param_count);

/**
 * @brief Bulk transmit data (for framebuffer/GRAM writes)
 *
 * Efficiently writes large blocks of data to display RAM.
 * Used for pixel RGB565 data during screen updates.
 *
 * Reference: Datasheet Section 8.2.25 (Memory Write Command - 0x2C)
 * - D/C pin must be HIGH (data mode)
 * - Data can be any length (12-bit RGB or 16-bit RGB565)
 * - Typically called after address set commands
 *
 * Pseudocode:
 *   1. Ensure D/C is high
 *   2. Transmit entire buffer via SPI (may use DMA for efficiency)
 *   3. Return status
 *
 * @param pixel_data Pointer to pixel data buffer
 * @param byte_count Total number of bytes to transmit
 * @return true if transmission successful, false otherwise
 */
bool spi_transmit_bulkdata(const uint8_t *pixel_data, size_t byte_count);

/* ============================================================================
 * Software Delay Utility
 * ========================================================================== */

/**
 * @brief Platform-independent delay function
 *
 * Blocks for the specified number of milliseconds.
 * Used in initialization sequences and reset timing.
 *
 * Reference: Datasheet Section 9 (Timing Specifications)
 * - RSTB pulse width: >= 1 ms (typically 10-100 ms)
 * - Power-on to first command: typically 5 ms minimum
 *
 * @param milliseconds Number of milliseconds to delay
 */
void spi_delay_ms(uint32_t milliseconds);

#endif /* ILI9488_SPI_H */
