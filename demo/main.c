/**
 * @file main.c
 * @brief ILI9488 Color Fill Demo
 *
 * Cycles through solid colors on a 320x480 ILI9488 display connected to a
 * Raspberry Pi 4B via SPI0:
 *   SPI device : /dev/spidev0.0  (10 MHz)
 *   RESET GPIO : BCM 24 (Pin 18)
 *   D/C GPIO   : BCM 25 (Pin 22)
 *   CS         : BCM  8 / SPI0_CE0 (Pin 24) -- handled by spidev
 */

#include "ili9488_spi.h"
#include "ili9488_hal.h"
#include <stdio.h>
#include <stdlib.h>

/* RGB565 color values */
#define COLOR_RED     0xF800U
#define COLOR_GREEN   0x07E0U
#define COLOR_BLUE    0x001FU
#define COLOR_WHITE   0xFFFFU
#define COLOR_BLACK   0x0000U
#define COLOR_YELLOW  0xFFE0U
#define COLOR_CYAN    0x07FFU
#define COLOR_MAGENTA 0xF81FU

#define DISPLAY_WIDTH  320U
#define DISPLAY_HEIGHT 480U

#define COLOR_HOLD_MS 2000U

int main(void)
{
    printf("ILI9488 Color Fill Demo\n");

    if (!spi_bus_initialize("/dev/spidev0.0", 10000000)) {
        fprintf(stderr, "Error: failed to open SPI bus\n");
        return EXIT_FAILURE;
    }

    if (!spi_gpio_initialize(GPIO_RESET, GPIO_STATE_HIGH)) {
        fprintf(stderr, "Error: failed to initialise RESET GPIO (BCM 24)\n");
        spi_bus_deinitialize();
        return EXIT_FAILURE;
    }

    if (!spi_gpio_initialize(GPIO_DC_SELECT, GPIO_STATE_HIGH)) {
        fprintf(stderr, "Error: failed to initialise D/C GPIO (BCM 25)\n");
        spi_gpio_deinitialize(GPIO_RESET);
        spi_bus_deinitialize();
        return EXIT_FAILURE;
    }

    printf("GPIO and SPI ready\n");

    if (!hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL)) {
        fprintf(stderr, "Error: display initialisation failed\n");
        spi_gpio_deinitialize(GPIO_DC_SELECT);
        spi_gpio_deinitialize(GPIO_RESET);
        spi_bus_deinitialize();
        return EXIT_FAILURE;
    }

    printf("Display ready -- starting color cycle\n");

    static const struct {
        uint16_t    color;
        const char *name;
    } colors[] = {
        { COLOR_RED,     "Red"     },
        { COLOR_GREEN,   "Green"   },
        { COLOR_BLUE,    "Blue"    },
        { COLOR_WHITE,   "White"   },
        { COLOR_BLACK,   "Black"   },
        { COLOR_YELLOW,  "Yellow"  },
        { COLOR_CYAN,    "Cyan"    },
        { COLOR_MAGENTA, "Magenta" },
    };

    int n = (int)(sizeof(colors) / sizeof(colors[0]));

    while (1) {
        int i;
        for (i = 0; i < n; i++) {
            printf("  %s\n", colors[i].name);
            hal_fill_rectangle_solid(0, (uint16_t)(DISPLAY_WIDTH  - 1),
                                     0, (uint16_t)(DISPLAY_HEIGHT - 1),
                                     colors[i].color);
            spi_delay_ms(COLOR_HOLD_MS);
        }
    }

    /* Unreachable in this demo */
    hal_display_deinitialize();
    spi_gpio_deinitialize(GPIO_DC_SELECT);
    spi_gpio_deinitialize(GPIO_RESET);
    spi_bus_deinitialize();
    return EXIT_SUCCESS;
}
