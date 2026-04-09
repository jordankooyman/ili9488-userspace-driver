/**
 * @file main.c
 * @brief ILI9488 Color Fill Demo
 *
 * Cycles through solid colors on a 320x480 ILI9488 display connected to a Raspberry Pi 4B via SPI0:
 *   SPI device : /dev/spidev0.0  (10 MHz)
 *   RESET GPIO : BCM 24 (Pin 18)
 *   D/C GPIO   : BCM 25 (Pin 22)
 *   CS         : BCM  8 / SPI0_CE0 (Pin 24) -- handled by spidev
 *
 * AI Usage Disclaimer: This file was mostly outlined then generated using AI tools. See ./AI_chats for the full conversation logs as best as could be exported.
 */

 // Note: Next ToDo: Update demo to use framebuffer API, doing a dirty region animation with color changes to show smearing and moving in a square
 // Also ToDo: Add something to do the single pixel draw demo
 // After ToDo: Add Line drawing to demo
 // Then ToDo: Add text drawing library, then add text to demo
 // Final ToDo: Review and clean up all code, finalize comments and documentation (readme and wiki), and prepare for final project submission

#include "ili9488_gfx.h"
#include "ili9488_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* RGB565 color values */
#define COLOR_RED     0xF800U
#define COLOR_GREEN   0x07E0U
#define COLOR_BLUE    0x001FU
#define COLOR_WHITE   0xFFFFU
#define COLOR_BLACK   0x0000U
#define COLOR_YELLOW  0xFFE0U
#define COLOR_CYAN    0x07FFU
#define COLOR_MAGENTA 0xF81FU

#define DISPLAY_WIDTH  ILI9488_GFX_DEFAULT_WIDTH
#define DISPLAY_HEIGHT ILI9488_GFX_DEFAULT_HEIGHT

#define COLOR_HOLD_MS 2000U

int main(void)
{
    printf("ILI9488 Demo\n");

    if (!hal_display_initialize(PIXEL_FORMAT_18BIT, ROTATION_0_NORMAL)) {
        fprintf(stderr, "Error: display initialisation failed\n");
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
        { COLOR_MAGENTA, "Magenta" }
    };

    int n = (int)(sizeof(colors) / sizeof(colors[0]));

    while (1) {
        int i;
        for (i = 0; i < n; i++) {
            printf("  %s\n", colors[i].name);
            if (!gfx_fill_screen_direct(colors[i].color)) {
                fprintf(stderr, "Error: fill failed while drawing %s\n", colors[i].name);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }
            usleep(COLOR_HOLD_MS * 1000);
        }
    }

    /* Unreachable in this demo */
    hal_display_deinitialize();

    return EXIT_SUCCESS;
}
