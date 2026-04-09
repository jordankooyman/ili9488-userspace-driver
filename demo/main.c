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
#include <time.h>
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
#define COLOR_ORANGE  0xFD20U
#define COLOR_PURPLE  0x8010U
#define COLOR_LIME    0x87E0U
#define COLOR_SILVER  0xC618U

#define DISPLAY_WIDTH  ILI9488_GFX_DEFAULT_WIDTH
#define DISPLAY_HEIGHT ILI9488_GFX_DEFAULT_HEIGHT

#ifndef DEMO_DEBUG_PRINTS
#define DEMO_DEBUG_PRINTS 0
#endif

#if DEMO_DEBUG_PRINTS
#define DEMO_DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEMO_DEBUG_PRINTF(...) ((void)0)
#endif

#define FRAME_DELAY_MS 200U
#define DIRTY_BOX_SIZE 108U
#define PATH_MARGIN    24U
#define STEP_PIXELS    12U

#define FULL_FILL_HOLD_MS         170U
#define FULL_FILL_SHOW_COUNT      2U

#define STARBURST_SHORT_RAYS      36U
#define STARBURST_LONG_RAYS       48U
#define STARBURST_RAYS_TOTAL      (STARBURST_SHORT_RAYS + STARBURST_LONG_RAYS)

typedef enum {
    DEMO_PHASE_RECTANGLE = 0,
    DEMO_PHASE_FILLS_AFTER_RECT,
    DEMO_PHASE_STARBURST,
    DEMO_PHASE_FILLS_AFTER_STARBURST
} demo_phase_t;

static uint64_t monotonic_time_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }

    return ((uint64_t)ts.tv_sec * 1000U) + ((uint64_t)ts.tv_nsec / 1000000U);
}

static void sleep_remaining_frame(uint64_t frame_start_ms, uint32_t frame_target_ms)
{
    uint64_t frame_end_ms = monotonic_time_ms();
    if (frame_end_ms <= frame_start_ms) {
        usleep(frame_target_ms * 1000U);
        return;
    }

    uint64_t elapsed_ms = frame_end_ms - frame_start_ms;
    if (elapsed_ms < frame_target_ms) {
        uint64_t remaining_ms = frame_target_ms - elapsed_ms;
        usleep((useconds_t)(remaining_ms * 1000U));
    }
}

static void rectangle_position_from_distance(uint32_t distance,
                                             uint16_t left,
                                             uint16_t top,
                                             uint16_t right,
                                             uint16_t bottom,
                                             uint16_t *x,
                                             uint16_t *y)
{
    uint32_t horizontal_span = (uint32_t)(right - left);
    uint32_t vertical_span = (uint32_t)(bottom - top);

    if (distance <= horizontal_span) {
        *x = (uint16_t)(left + distance);
        *y = top;
    } else if (distance <= (horizontal_span + vertical_span)) {
        *x = right;
        *y = (uint16_t)(top + (distance - horizontal_span));
    } else if (distance <= (2U * horizontal_span + vertical_span)) {
        *x = (uint16_t)(right - (distance - (horizontal_span + vertical_span)));
        *y = bottom;
    } else {
        *x = left;
        *y = (uint16_t)(bottom - (distance - (2U * horizontal_span + vertical_span)));
    }
}

int main(void)
{
    DEMO_DEBUG_PRINTF("ILI9488 Demo\n");

    if (!hal_display_initialize(PIXEL_FORMAT_18BIT, ROTATION_0_NORMAL)) {
        fprintf(stderr, "Error: display initialisation failed\n");
        return EXIT_FAILURE;
    }

    DEMO_DEBUG_PRINTF("Display ready -- starting dirty-region smear demo\n");

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
        { COLOR_MAGENTA, "Magenta" },
        { COLOR_ORANGE,  "Orange"  },
        { COLOR_PURPLE,  "Purple"  },
        { COLOR_LIME,    "Lime"    },
        { COLOR_SILVER,  "Silver"  }
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
    int pixel_color_index = 0;

    uint16_t pixel_x = DISPLAY_WIDTH / 2U;
    uint16_t pixel_y = DISPLAY_HEIGHT / 2U;
    int16_t pixel_dx = (int16_t)STEP_PIXELS;
    int16_t pixel_dy = (int16_t)STEP_PIXELS;
    demo_phase_t phase = DEMO_PHASE_RECTANGLE;
    unsigned int phase_step = 0U;
    unsigned int fill_color_cursor = 0U;
    bool starburst_phase_started = false;

    uint16_t center_x = DISPLAY_WIDTH / 2U;
    uint16_t center_y = DISPLAY_HEIGHT / 2U;

    uint16_t long_left = 0U;
    uint16_t long_top = 0U;
    uint16_t long_right = DISPLAY_WIDTH - 1U;
    uint16_t long_bottom = DISPLAY_HEIGHT - 1U;
    uint32_t long_perimeter = 2U * ((uint32_t)(long_right - long_left) + (uint32_t)(long_bottom - long_top));

    uint16_t short_half_width = DISPLAY_WIDTH / 8U;
    uint16_t short_half_height = DISPLAY_HEIGHT / 8U;
    uint16_t short_left = center_x - short_half_width;
    uint16_t short_top = center_y - short_half_height;
    uint16_t short_right = center_x + short_half_width;
    uint16_t short_bottom = center_y + short_half_height;
    uint32_t short_perimeter = 2U * ((uint32_t)(short_right - short_left) + (uint32_t)(short_bottom - short_top));

    uint32_t rect_horizontal_span = (uint32_t)(path_right - path_left);
    uint32_t rect_vertical_span = (uint32_t)(path_bottom - path_top);
    uint32_t rect_perimeter = 2U * (rect_horizontal_span + rect_vertical_span);
    uint32_t rect_start_distance = 0U;
    uint32_t rect_distance = rect_start_distance;
    uint32_t rect_travelled = 0U;

    if (rect_perimeter == 0U) {
        fprintf(stderr, "Error: rectangle perimeter is zero\n");
        gfx_framebuffer_unbind(&framebuffer);
        free(fb_storage);
        hal_display_deinitialize();
        return EXIT_FAILURE;
    }

    while (1) {
        uint64_t frame_start_ms = monotonic_time_ms();
        bool apply_frame_pacing = true;

        switch (phase) {
        case DEMO_PHASE_RECTANGLE: {
            rectangle_position_from_distance(rect_distance,
                                             path_left,
                                             path_top,
                                             path_right,
                                             path_bottom,
                                             &box_x,
                                             &box_y);

            uint16_t box_x2 = box_x + DIRTY_BOX_SIZE - 1U;
            uint16_t box_y2 = box_y + DIRTY_BOX_SIZE - 1U;

            if (!gfx_draw_filled_rectangle(&framebuffer, box_x, box_y, box_x2, box_y2, colors[color_index].color)) {
                fprintf(stderr, "Error: framebuffer draw failed while drawing %s\n", colors[color_index].name);
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            /* Placeholder for future text rendering API call:
             * Draw centered text "Hello ECEN-5713" during rectangle phase.
             */

            if (!gfx_draw_pixel(&framebuffer, pixel_x, pixel_y, colors[pixel_color_index].color)) {
                fprintf(stderr, "Error: single-pixel draw failed at (%u,%u)\n", pixel_x, pixel_y);
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            if (!gfx_flush_dirty(&framebuffer)) {
                fprintf(stderr, "Error: dirty flush failed during rectangle phase\n");
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            color_index = (color_index + 1) % color_count;
            pixel_color_index = (pixel_color_index + 1) % color_count;

            rect_distance = (rect_distance + STEP_PIXELS) % rect_perimeter;
            rect_travelled += STEP_PIXELS;

            if (rect_travelled >= rect_perimeter) {
                uint32_t back_step = (2U * STEP_PIXELS) % rect_perimeter;
                rect_start_distance = (rect_start_distance + rect_perimeter - back_step) % rect_perimeter;
                rect_distance = rect_start_distance;
                rect_travelled = 0U;
                phase = DEMO_PHASE_FILLS_AFTER_RECT;
                phase_step = 0U;
            }
            break;
        }

        case DEMO_PHASE_FILLS_AFTER_RECT:
        case DEMO_PHASE_FILLS_AFTER_STARBURST: {
            int fill_color_index = (int)fill_color_cursor % color_count;
            DEMO_DEBUG_PRINTF("  Full fill: %s\n", colors[fill_color_index].name);
            if (!gfx_fill(&framebuffer, colors[fill_color_index].color) || !gfx_flush(&framebuffer)) {
                fprintf(stderr, "Error: full-screen fill failed while drawing %s\n", colors[fill_color_index].name);
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            fill_color_cursor++;
            phase_step++;
            usleep(FULL_FILL_HOLD_MS * 1000U);

            if (phase_step >= FULL_FILL_SHOW_COUNT) {
                phase_step = 0U;
                if (phase == DEMO_PHASE_FILLS_AFTER_RECT) {
                    phase = DEMO_PHASE_STARBURST;
                    starburst_phase_started = false;
                } else {
                    phase = DEMO_PHASE_RECTANGLE;
                }
            }
            continue;
        }

        case DEMO_PHASE_STARBURST: {
            apply_frame_pacing = false;

            if (!starburst_phase_started) {
                if (!gfx_fill(&framebuffer, COLOR_BLACK) || !gfx_flush(&framebuffer)) {
                    fprintf(stderr, "Error: starburst phase clear failed\n");
                    gfx_framebuffer_unbind(&framebuffer);
                    free(fb_storage);
                    hal_display_deinitialize();
                    return EXIT_FAILURE;
                }
                starburst_phase_started = true;
            }

            uint32_t edge_pos;
            uint16_t ray_x = 0U;
            uint16_t ray_y = 0U;

            if (phase_step < STARBURST_SHORT_RAYS) {
                edge_pos = (phase_step * short_perimeter) / STARBURST_SHORT_RAYS;
                rectangle_position_from_distance(edge_pos,
                                                 short_left,
                                                 short_top,
                                                 short_right,
                                                 short_bottom,
                                                 &ray_x,
                                                 &ray_y);
            } else {
                uint32_t long_phase_step = phase_step - STARBURST_SHORT_RAYS;
                edge_pos = (long_phase_step * long_perimeter) / STARBURST_LONG_RAYS;
                rectangle_position_from_distance(edge_pos,
                                                 long_left,
                                                 long_top,
                                                 long_right,
                                                 long_bottom,
                                                 &ray_x,
                                                 &ray_y);
            }

            if (!gfx_draw_line(&framebuffer,
                               (int16_t)center_x,
                               (int16_t)center_y,
                               (int16_t)ray_x,
                               (int16_t)ray_y,
                               colors[color_index].color)) {
                fprintf(stderr, "Error: starburst line draw failed\n");
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            if (!gfx_draw_pixel(&framebuffer, pixel_x, pixel_y, colors[pixel_color_index].color)) {
                fprintf(stderr, "Error: single-pixel draw failed at (%u,%u)\n", pixel_x, pixel_y);
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            if (!gfx_flush_dirty(&framebuffer)) {
                fprintf(stderr, "Error: dirty flush failed during starburst phase\n");
                gfx_framebuffer_unbind(&framebuffer);
                free(fb_storage);
                hal_display_deinitialize();
                return EXIT_FAILURE;
            }

            color_index = (color_index + 1) % color_count;
            pixel_color_index = (pixel_color_index + 1) % color_count;

            phase_step++;
            if (phase_step >= STARBURST_RAYS_TOTAL) {
                phase = DEMO_PHASE_FILLS_AFTER_STARBURST;
                phase_step = 0U;
            }
            break;
        }

        default:
            phase = DEMO_PHASE_RECTANGLE;
            phase_step = 0U;
            break;
        }

        {
            int32_t next_pixel_x = (int32_t)pixel_x + pixel_dx;
            int32_t next_pixel_y = (int32_t)pixel_y + pixel_dy;

            if (next_pixel_x < 0) {
                next_pixel_x = 0;
                pixel_dx = (int16_t)(-pixel_dx);
            } else if (next_pixel_x >= (int32_t)DISPLAY_WIDTH) {
                next_pixel_x = (int32_t)DISPLAY_WIDTH - 1;
                pixel_dx = (int16_t)(-pixel_dx);
            }

            if (next_pixel_y < 0) {
                next_pixel_y = 0;
                pixel_dy = (int16_t)(-pixel_dy);
            } else if (next_pixel_y >= (int32_t)DISPLAY_HEIGHT) {
                next_pixel_y = (int32_t)DISPLAY_HEIGHT - 1;
                pixel_dy = (int16_t)(-pixel_dy);
            }

            pixel_x = (uint16_t)next_pixel_x;
            pixel_y = (uint16_t)next_pixel_y;
        }

        if (apply_frame_pacing) {
            sleep_remaining_frame(frame_start_ms, FRAME_DELAY_MS);
        }
    }

    /* Unreachable in this demo */
    gfx_framebuffer_unbind(&framebuffer);
    free(fb_storage);
    hal_display_deinitialize();

    return EXIT_SUCCESS;
}
