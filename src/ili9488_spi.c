/**
 * @file ili9488_spi.c
 * @brief ILI9488 SPI Interface - Linux spidev + libgpiod implementation
 *
 * Platform: Linux (Raspberry Pi 4B)
 * SPI:  /dev/spidev0.0 via ioctl (Mode 0, MSB-first, max 10 MHz write / 5 MHz read)
 * GPIO: libgpiod on /dev/gpiochip0
 *         RESET     → BCM 24 (Pin 18)
 *         D/C       → BCM 25 (Pin 22)
 *         CS (CE0)  → BCM  8 (Pin 24) — managed by spidev kernel driver
 *
 * References: ILI9488 Datasheet, Section 4.2 (DBI Type C Serial Interface)
 */

#include "ili9488_spi.h"
#include "config.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <time.h>

/* Maximum bytes per spidev ioctl transfer (kernel default buffer size) */
#define SPI_MAX_TRANSFER_BYTES 4096U

static const unsigned int g_gpio_bcm_pins[2] = {
    ILI9488_GPIO_RESET_BCM,
    ILI9488_GPIO_DC_BCM
};

/* ============================================================================
 * Static File-Scope Variables
 * ========================================================================== */

static int                  g_spi_fd           = -1;
static struct gpiod_chip   *g_gpio_chip        = NULL;
static struct gpiod_line   *g_gpio_lines[3]    = {NULL, NULL, NULL};
static bool                 g_spi_initialized  = false;
static spi_gpio_state_t     g_gpio_states[3];

/* ============================================================================
 * SPI Bus Initialization & Configuration
 * ========================================================================== */

bool spi_bus_initialize(const char *spi_device_path, uint32_t clock_speed_hz)
{
    if (g_spi_initialized || spi_device_path == NULL) {
        return false;
    }

    /* Clamp to max safe write clock — Datasheet Section 4.2 */
    if (clock_speed_hz > 10000000U) {
        clock_speed_hz = 10000000U;
    }

    g_spi_fd = open(spi_device_path, O_RDWR);
    if (g_spi_fd < 0) {
        return false;
    }

    /* SPI Mode 0: CPOL=0, CPHA=0 */
    uint8_t mode = SPI_MODE_0;
    if (ioctl(g_spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        goto fail_close;
    }

    /* 8 bits per word */
    uint8_t bits = 8;
    if (ioctl(g_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        goto fail_close;
    }

    /* Set maximum clock frequency */
    if (ioctl(g_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &clock_speed_hz) < 0) {
        goto fail_close;
    }

    /* Open GPIO chip for RESET and D/C lines */
    g_gpio_chip = gpiod_chip_open(ILI9488_GPIO_CHIP_PATH);
    if (g_gpio_chip == NULL) {
        goto fail_close;
    }

    g_spi_initialized = true;
    return true;

fail_close:
    close(g_spi_fd);
    g_spi_fd = -1;
    return false;
}

bool spi_bus_deinitialize(void)
{
    if (!g_spi_initialized) {
        return true;
    }

    if (g_spi_fd >= 0) {
        close(g_spi_fd);
        g_spi_fd = -1;
    }

    if (g_gpio_chip != NULL) {
        gpiod_chip_close(g_gpio_chip);
        g_gpio_chip = NULL;
    }

    g_spi_initialized = false;
    return true;
}

/* ============================================================================
 * GPIO Initialization & Control
 * ========================================================================== */

bool spi_gpio_initialize(spi_gpio_pin_t gpio_pin, spi_gpio_state_t initial_state)
{
    if (!g_spi_initialized) {
        return false;
    }

    /* GPIO_CHIP_SELECT is driven by the spidev kernel driver via SPI0_CE0 */
    if (gpio_pin == GPIO_CHIP_SELECT) {
        g_gpio_states[gpio_pin] = initial_state;
        return true;
    }

    if (gpio_pin >= 2 || g_gpio_chip == NULL) {
        return false;
    }

    struct gpiod_line *line = gpiod_chip_get_line(g_gpio_chip,
                                                   g_gpio_bcm_pins[gpio_pin]);
    if (line == NULL) {
        return false;
    }

    if (gpiod_line_request_output(line, "ili9488", (int)initial_state) < 0) {
        return false;
    }

    g_gpio_lines[gpio_pin] = line;
    g_gpio_states[gpio_pin] = initial_state;
    return true;
}

bool spi_gpio_set_state(spi_gpio_pin_t gpio_pin, spi_gpio_state_t state)
{
    if (gpio_pin >= 3) {
        return false;
    }

    /* GPIO_CHIP_SELECT is managed by spidev */
    if (gpio_pin == GPIO_CHIP_SELECT) {
        g_gpio_states[gpio_pin] = state;
        return true;
    }

    if (g_gpio_lines[gpio_pin] == NULL) {
        return false;
    }

    if (gpiod_line_set_value(g_gpio_lines[gpio_pin], (int)state) < 0) {
        return false;
    }

    g_gpio_states[gpio_pin] = state;
    return true;
}

bool spi_gpio_read_state(spi_gpio_pin_t gpio_pin, spi_gpio_state_t *state)
{
    if (gpio_pin >= 3 || state == NULL) {
        return false;
    }

    *state = g_gpio_states[gpio_pin];
    return true;
}

bool spi_gpio_deinitialize(spi_gpio_pin_t gpio_pin)
{
    if (gpio_pin >= 3) {
        return false;
    }

    if (gpio_pin == GPIO_CHIP_SELECT) {
        return true;
    }

    if (g_gpio_lines[gpio_pin] != NULL) {
        gpiod_line_release(g_gpio_lines[gpio_pin]);
        g_gpio_lines[gpio_pin] = NULL;
    }

    return true;
}

/* ============================================================================
 * SPI Transmit & Receive Operations
 * ========================================================================== */

bool spi_transmit_command(uint8_t command_byte)
{
    if (!g_spi_initialized || g_spi_fd < 0) {
        return false;
    }

    /* D/C = LOW selects command register — Datasheet Section 4.2.1 */
    if (!spi_gpio_set_state(GPIO_DC_SELECT, GPIO_STATE_LOW)) {
        return false;
    }

    struct spi_ioc_transfer xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf      = (unsigned long)&command_byte;
    xfer.len         = 1;
    xfer.bits_per_word = 8;

    return ioctl(g_spi_fd, SPI_IOC_MESSAGE(1), &xfer) >= 0;
}

bool spi_transmit_data(const uint8_t *data_buffer, size_t byte_count)
{
    if (!g_spi_initialized || g_spi_fd < 0) {
        return false;
    }

    if (data_buffer == NULL || byte_count == 0) {
        return false;
    }

    /* D/C = HIGH selects data/parameter register — Datasheet Section 4.2.1 */
    if (!spi_gpio_set_state(GPIO_DC_SELECT, GPIO_STATE_HIGH)) {
        return false;
    }

    /* Split into chunks to respect spidev kernel buffer limit */
    const uint8_t *ptr       = data_buffer;
    size_t         remaining = byte_count;

    while (remaining > 0) {
        size_t chunk = (remaining > SPI_MAX_TRANSFER_BYTES)
                       ? SPI_MAX_TRANSFER_BYTES : remaining;

        struct spi_ioc_transfer xfer;
        memset(&xfer, 0, sizeof(xfer));
        xfer.tx_buf        = (unsigned long)ptr;
        xfer.len           = (uint32_t)chunk;
        xfer.bits_per_word = 8;

        if (ioctl(g_spi_fd, SPI_IOC_MESSAGE(1), &xfer) < 0) {
            return false;
        }

        ptr       += chunk;
        remaining -= chunk;
    }

    return true;
}

bool spi_receive_data(uint8_t *data_buffer, size_t byte_count)
{
    if (!g_spi_initialized || g_spi_fd < 0) {
        return false;
    }

    if (data_buffer == NULL || byte_count == 0) {
        return false;
    }

    /* D/C = HIGH for parameter read — Datasheet Section 4.2.2 */
    if (!spi_gpio_set_state(GPIO_DC_SELECT, GPIO_STATE_HIGH)) {
        return false;
    }

    /* SPI is full-duplex: transmit dummy bytes to clock data in.
     * Read clock is capped at 5 MHz — Datasheet Section 4.2 */
    uint8_t *tx_dummy = (uint8_t *)calloc(byte_count, 1);
    if (tx_dummy == NULL) {
        return false;
    }

    struct spi_ioc_transfer xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf        = (unsigned long)tx_dummy;
    xfer.rx_buf        = (unsigned long)data_buffer;
    xfer.len           = (uint32_t)byte_count;
    xfer.speed_hz      = 5000000U;  /* 5 MHz read clock */
    xfer.bits_per_word = 8;

    bool ok = ioctl(g_spi_fd, SPI_IOC_MESSAGE(1), &xfer) >= 0;

    free(tx_dummy);
    return ok;
}

bool spi_transmit_command_with_params(uint8_t command_byte,
                                       const uint8_t *param_buffer,
                                       size_t param_count)
{
    if (!spi_transmit_command(command_byte)) {
        return false;
    }

    if (param_buffer != NULL && param_count > 0) {
        if (!spi_transmit_data(param_buffer, param_count)) {
            return false;
        }
    }

    return true;
}

bool spi_transmit_bulkdata(const uint8_t *pixel_data, size_t byte_count)
{
    /* Pixel data uses the same D/C=HIGH data path as parameter writes */
    return spi_transmit_data(pixel_data, byte_count);
}

/* ============================================================================
 * Software Delay Utility
 * ========================================================================== */

void spi_delay_ms(uint32_t milliseconds)
{
    struct timespec req = {
        .tv_sec  = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L
    };
    nanosleep(&req, NULL);
}
