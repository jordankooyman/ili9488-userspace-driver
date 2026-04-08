//gfx_draw_mono_bitmap(bitmap, x, y, size_x, size_y, color, framebuffer) -> draws a monochrome bitmap where each bit represents a pixel (1=draw, 0=skip) at the specified location, using the specified color. The bitmap is stored as an array of bytes, where each byte contains 8 pixels (MSB first). Size is for the bitmap array read.

//gfx_draw_char(char, x, y, color, size, framebuffer) -> looks up char in font array, gets bitmap, calls gfx_draw_mono_bitmap with appropriate parameters for size and color

//gfx_draw_string(string, x, y, color, size, framebuffer) -> calls gfx_draw_char in a loop with appropriate x offsets for each character, and handles newlines by resetting x and incrementing y by character height * size.