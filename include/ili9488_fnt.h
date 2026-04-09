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

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9488_FONT_FIRST_CHAR 32
#define ILI9488_FONT_LAST_CHAR 126
#define ILI9488_FONT_GLYPH_COUNT 95

/*
 * Array of 95 ASCII Glyphs (32-126) for an 8x12 pixel font.
 * Each glyph is 12 rows, one byte per row (MSB = leftmost pixel).
 */
extern const uint8_t ili9488_font_8x12[ILI9488_FONT_GLYPH_COUNT][12];

/*
 * Array of 95 ASCII Glyphs (32-126) for a 6x8 pixel font.
 * Each glyph is 8 rows, one byte per row.
 */
extern const uint8_t ili9488_font_6x8[ILI9488_FONT_GLYPH_COUNT][8];

#ifdef __cplusplus
}
#endif

#endif