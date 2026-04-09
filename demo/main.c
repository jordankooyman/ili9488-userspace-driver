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

#define FRAME_DELAY_MS 70U
#define DIRTY_BOX_SIZE 42U
#define PATH_MARGIN    24U

typedef enum {
    MOVE_RIGHT = 0,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_UP
} move_direction_t;

int main(void)
{
    printf("ILI9488 Demo\n");

    if (!hal_display_initialize(PIXEL_FORMAT_18BIT, ROTATION_0_NORMAL)) {
        fprintf(stderr, "Error: display initialisation failed\n");
        return EXIT_FAILURE;
    }

    printf("Display ready -- starting dirty-region smear demo\n");

    size_t fb_pixels = gfx_framebuffer_required_pixels(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (fb_pixels == 0U) {
        fprintf(stderr, "Error: invalid framebuffer dimensions\n");
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

    uint16_t *fb_storage = (uint16_t *)malloc(fb_pixels * sizeof(uint16_t));
    if (fb_storage == NULL) {
        fprintf(stderr, "Error: framebuffer allocation failed\n");
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

    gfx_framebuffer_t framebuffer;
    if (!gfx_framebuffer_bind(&framebuffer,
                              fb_storage,
                              fb_pixels,
                              DISPLAY_WIDTH,
                              DISPLAY_HEIGHT,
                              0U)) {
        fprintf(stderr, "Error: framebuffer bind failed\n");
        free(fb_storage);
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

    if (!gfx_fill(&framebuffer, COLOR_BLACK) || !gfx_flush(&framebuffer)) {
        fprintf(stderr, "Error: initial framebuffer clear/flush failed\n");
        gfx_framebuffer_unbind(&framebuffer);
        free(fb_storage);
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

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

    int color_count = (int)(sizeof(colors) / sizeof(colors[0]));

    uint16_t path_left = PATH_MARGIN;
    uint16_t path_top = PATH_MARGIN;
    uint16_t path_right = DISPLAY_WIDTH - PATH_MARGIN - DIRTY_BOX_SIZE;
    uint16_t path_bottom = DISPLAY_HEIGHT - PATH_MARGIN - DIRTY_BOX_SIZE;

    if (path_right <= path_left || path_bottom <= path_top) {
        fprintf(stderr, "Error: dirty region path does not fit display\n");
        gfx_framebuffer_unbind(&framebuffer);
        free(fb_storage);
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

    uint16_t box_x = path_left;
    uint16_t box_y = path_top;
    int color_index = 0;
    move_direction_t direction = MOVE_RIGHT;

    while (1) {
        uint16_t box_x2 = box_x + DIRTY_BOX_SIZE - 1U;
        uint16_t box_y2 = box_y + DIRTY_BOX_SIZE - 1U;

        if (!gfx_draw_filled_rectangle(&framebuffer, box_x, box_y, box_x2, box_y2, colors[color_index].color)) {
            fprintf(stderr, "Error: framebuffer draw failed while drawing %s\n", colors[color_index].name);
            gfx_framebuffer_unbind(&framebuffer);
            free(fb_storage);
            hal_display_deinitialize();
            return EXIT_FAILURE;
        }

        if (!gfx_flush_dirty(&framebuffer)) {
            fprintf(stderr, "Error: dirty flush failed while drawing %s\n", colors[color_index].name);
            gfx_framebuffer_unbind(&framebuffer);
            free(fb_storage);
            hal_display_deinitialize();
            return EXIT_FAILURE;
        }

        color_index = (color_index + 1) % color_count;

        switch (direction) {
        case MOVE_RIGHT:
            if (box_x < path_right) {
                box_x++;
            } else {
                direction = MOVE_DOWN;
                box_y++;
            }
            break;
        case MOVE_DOWN:
            if (box_y < path_bottom) {
                box_y++;
            } else {
                direction = MOVE_LEFT;
                box_x--;
            }
            break;
        case MOVE_LEFT:
            if (box_x > path_left) {
                box_x--;
            } else {
                direction = MOVE_UP;
                box_y--;
            }
            break;
        case MOVE_UP:
        default:
            if (box_y > path_top) {
                box_y--;
            } else {
                direction = MOVE_RIGHT;
                box_x++;
            }
            break;
        }

        usleep(FRAME_DELAY_MS * 1000U);
    }

    /* Unreachable in this demo */
    gfx_framebuffer_unbind(&framebuffer);
    free(fb_storage);
    hal_display_deinitialize();

    return EXIT_SUCCESS;
}
