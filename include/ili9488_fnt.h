/**
 * @file ili9488_fnt.h
 * @brief Font tables used by the ILI9488 graphics layer.
 *
 * References:
 * - 6x8 source adapted from:
 *   https://github.com/idispatch/raster-fonts/blob/master/font-6x8.c
 *
 * AI Usage Disclaimer: This file was generated and edited with AI tools.
 * See ./AI_chats for available conversation logs.
 */

#ifndef ILI9488_FNT_H
#define ILI9488_FNT_H

#include <stdint.h>

typedef enum {
    ILI9488_FONT_6X8,
    ILI9488_FONT_8X12
} ili9488_font_t;

/*
 * Array of 95 ASCII Glyphs (32-126) for an 8x12 pixel font.
 * Each glyph is 12 rows, one byte per row (MSB = leftmost pixel).
 */
#define ili9488_font_8x12_min_char (0x20)
#define ili9488_font_8x12_max_char (0x7E)
#define ili9488_font_8x12_width_pixels (8)
#define ili9488_font_8x12_height_pixels (12)
extern const uint8_t ili9488_font_8x12[95][12];

/*
 * Array of 95 ASCII Glyphs (32-126) for a 6x8 pixel font.
 * Each glyph is 8 rows, one byte per row (MSB = leftmost pixel).
 */
#define ili9488_font_6x8_min_char (0x20)
#define ili9488_font_6x8_max_char (0x7E)
#define ili9488_font_6x8_width_pixels (6)
#define ili9488_font_6x8_height_pixels (8)
extern const uint8_t ili9488_font_6x8[95][8];

#endif