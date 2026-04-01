/**
 * @file COMMAND_REFERENCE.md
 * @brief ILI9488 Command Quick Reference
 *
 * A quick lookup table for all ILI9488 commands implemented in this driver.
 * Each entry shows the function, command code, datasheet section, and typical usage.
 */

# ILI9488 Command Reference

## Quick Lookup by Category

### System Commands

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_system_reset()` | 0x01 | 8.2.1 | None | Full register reset (GRAM preserved) |
| `hal_power_sleep_mode_set(true)` | 0x10 | 8.2.9 | None | Enter sleep (low power) |
| `hal_power_sleep_exit()` | 0x11 | 8.2.10 | None | Exit sleep (wake up) |
| `hal_display_output_control(false)` | 0x28 | 8.2.6 | None | Display OFF (GRAM preserved) |
| `hal_display_output_control(true)` | 0x29 | 8.2.7 | None | Display ON |

### Display Configuration

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_interface_mode_set(rim, dim)` | 0x0D | 8.2.2 | 2 bytes | SPI interface mode selection |
| `hal_pixel_format_set(format)` | 0x3A | 8.2.5 | 1 byte | Set color depth (12/16/18 bit) |
| `hal_display_rotation_set(rot, h, v)` | 0x36 | 8.2.3 | 1 byte | Rotation (0°/90°/180°/270°) + flip |
| `hal_color_byte_order_set(order)` | 0x36 | 8.2.3 | Modified | RGB vs. BGR byte order |
| `hal_transfer_mode_set(mode)` | 0x36 | 8.2.3 | Modified | Row-wise vs. column-wise addressing |
| `hal_display_config_read(byte)` | 0x36 | 8.2.3 | Read | Read current MAC register |
| `hal_normal_display_mode_on()` | 0x13 | 8.2.24 | None | Disable color inversion |
| `hal_invert_display_mode_on()` | 0x21 | 8.2.23 | None | Invert all colors |

### Memory & Address Commands

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_column_address_set(start, end)` | 0x2A | 8.2.18 | 4 bytes | Set horizontal window [0-319] |
| `hal_row_address_set(start, end)` | 0x2B | 8.2.19 | 4 bytes | Set vertical window [0-479] |
| `hal_window_address_set()` | 0x2A+0x2B | 8.2.18-19 | 8 bytes | Set column + row window |
| `hal_gram_write_start()` | 0x2C | 8.2.25 | None | Enter GRAM write mode (pixel data follows) |
| `hal_gram_read_start()` | 0x2E | 8.2.27 | None | Enter GRAM read mode (read pixels) |

### Pixel Data

| Function | Usage | Purpose |
|----------|-------|---------|
| `hal_gram_write_pixels(buf, count, fmt)` | Called after 0x2C | Write pixel data to GRAM |
| `hal_gram_read_pixels(buf, count, fmt)` | Called after 0x2E | Read pixel data from GRAM |
| `hal_fill_rectangle_solid(x1,x2,y1,y2,color)` | Direct call | Fill rectangle with solid color |

### Power Supply & Voltage

| Function | Code | Datasheet | Parameters | Typical Value | Purpose |
|----------|------|-----------|------------|---------------|---------|
| `hal_power_gvdd_set(voltage)` | 0xC0 | 8.2.13 | 1 byte | 0x17 (4.0V) | Core voltage |
| `hal_power_vci_set(voltage)` | 0xC1 | 8.2.14 | 1 byte | 0x41 | I/O supply voltage |
| `hal_power_vgh_vgl_set(vgh, vgl)` | 0xC5 | 8.2.15 | 2 bytes | VGH=0x0A, VGL=0x0A | Gate voltages |
| `hal_power_vcomh_set(voltage)` | 0xBB | 8.2.17 | 1 byte | 0x30 | VCOM reference |

### Timing & Frequency

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_oscillator_frequency_set(code)` | 0xB0 | 8.2.11 | 1 byte | Internal clock frequency |
| `hal_frame_rate_set(code)` | 0xB1 | 8.2.12 | 1 byte | Display refresh rate (typically 0x18 = 60Hz) |

### Gamma Correction

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_gamma_curve_select(curve)` | 0x26 | 8.2.26 | 1 byte | Select predefined gamma (1/2/3/4) |
| `hal_gamma_curve_program(pos, neg)` | 0xE0/0xE1 | 8.2.27-28 | 15 bytes each | Program custom gamma curves |

### Status & Information

| Function | Code | Datasheet | Purpose |
|----------|------|-----------|---------|
| `hal_display_id_read(id)` | 0xD3 | 8.2.33 | Read IC ID (should be 0x009488) |
| `hal_power_mode_read(mode)` | 0x0A | 8.2.29 | Read current power state |
| `hal_display_status_read(status)` | 0x0D | 8.2.32 | Read display status flags |
| `hal_display_mode_read(mode)` | 0x0B | 8.2.30 | Read display mode settings |
| `hal_pixel_format_read(format)` | 0x0C | 8.2.31 | Read current pixel format |

### Advanced Features

| Function | Code | Datasheet | Parameters | Purpose |
|----------|------|-----------|------------|---------|
| `hal_partial_mode_set(enabled)` | 0x12/0x13 | 8.2.8 | None | Enable/disable partial display mode |
| `hal_scroll_area_set(tfa, vsa, bfa)` | 0x33 | 8.2.21 | 6 bytes | Configure scrolling region |
| `hal_scroll_start_address_set(start)` | 0x37 | 8.2.22 | 2 bytes | Set scroll offset |
| `hal_color_enhancement_set(enabled)` | 0xE8 | 8.2.36 | 1 byte | Enable adaptive color enhancement |
| `hal_interface_control_set(wemode)` | 0xC6 | 8.2.37 | 1 byte | Configure interface timing |
| `hal_ram_protection_set(ena, key)` | 0xC9 | 8.2.35 | 1 byte | Enable/disable write protection |
| `hal_gpio_configure(mask, config)` | 0xF7 | 8.2.34 | 2 bytes | Configure GPIO pins (if supported) |

---

## Usage Patterns

### Pattern 1: Basic Display Fill

```c
// Fill entire screen with red (RGB565: 1111100000000000 = 0xF800)
hal_fill_rectangle_solid(0, 319, 0, 479, 0xF800);
```

### Pattern 2: Write Custom Framebuffer

```c
// Assuming 'framebuffer' contains 320*480*2 bytes of RGB565 pixel data
hal_window_address_set(0, 319, 0, 479);
hal_gram_write_start();
hal_gram_write_pixels(framebuffer, 320 * 480, PIXEL_FORMAT_16BIT);
```

### Pattern 3: Rotate Display 90 Degrees Clockwise

```c
// Reconfigure memory access for 90° rotation
hal_display_rotation_set(ROTATION_90_CLOCKWISE,
                        HORIZONAL_DIRECTION_DEFAULT,
                        VERTICAL_DIRECTION_DEFAULT);

// After rotation, width/height are swapped (480x320 instead of 320x480)
// Fill rotated screen
hal_fill_rectangle_solid(0, 479, 0, 319, 0xFFFF);
```

### Pattern 4: Enable Display Scrolling

```c
// Define scrolling region: top 50 fixed, middle 400 scroll, bottom 30 fixed
hal_scroll_area_set(50, 400, 30);

// Scroll by 100 lines (wraps within the 400-line scroll region)
hal_scroll_start_address_set(100);

// Later: scroll by 200 lines
hal_scroll_start_address_set(200);
```

### Pattern 5: Adjust Color Contrast (Power Control)

```c
// Increase contrast by raising GVDD voltage
hal_power_gvdd_set(0x20);  // Higher than typical 0x17

// Adjust gate voltages for brighter whites
hal_power_vgh_vgl_set(0x0C, 0x0C);  // Higher VGH, higher VGL
```

### Pattern 6: Power Down to Sleep Mode

```c
// Set display to sleep (preserves GRAM, reduces power)
hal_power_set_state(POWER_STATE_SLEEP);

// Later, wake up
hal_power_set_state(POWER_STATE_NORMAL);
```

### Pattern 7: Verify Hardware ID

```c
uint32_t chip_id;
hal_display_id_read(&chip_id);

if ((chip_id & 0x00FFFFFF) == 0x009488) {
    printf("ILI9488 detected\n");
} else {
    printf("Unknown display: 0x%06X\n", chip_id);
}
```

---

## Typical Initialization Parameters

### Standard RGB565, 60Hz, Normal Orientation

```c
// Initialize with RGB565 (16-bit, 65K colors)
hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL);

// Result: 320x480 pixels, normal orientation
//         60 Hz refresh rate
//         Gamma curve 3 (neutral)
//         4.0V GVDD, standard power settings
```

### Custom Configuration Example

```c
// Minimal init just to power on display
spi_bus_initialize("/dev/spidev0.0", 5000000);
spi_gpio_initialize(GPIO_RESET, GPIO_STATE_HIGH);
spi_gpio_initialize(GPIO_DC_SELECT, GPIO_STATE_HIGH);

// Hardware reset
spi_gpio_set_state(GPIO_RESET, GPIO_STATE_LOW);
spi_delay_ms(1);
spi_gpio_set_state(GPIO_RESET, GPIO_STATE_HIGH);
spi_delay_ms(120);

// Soft reset + sleep out
hal_system_reset(RESET_COMPLETE);
hal_power_sleep_exit();

// Set pixel format
hal_pixel_format_set(PIXEL_FORMAT_16BIT);

// Set rotation
hal_display_rotation_set(ROTATION_0_NORMAL,
                        HORIZONAL_DIRECTION_DEFAULT,
                        VERTICAL_DIRECTION_DEFAULT);

// Power control
hal_power_gvdd_set(0x17);
hal_power_vci_set(0x41);
hal_power_vgh_vgl_set(0x0A, 0x0A);
hal_power_vcomh_set(0x30);

// Timing
hal_frame_rate_set(0x18);  // 60 Hz

// Gamma
hal_gamma_curve_select(GAMMA_CURVE_3);

// Address window (full screen)
hal_column_address_set(0, 319);
hal_row_address_set(0, 479);

// Enable display
hal_display_output_control(true);

// Fill with white to verify
hal_fill_rectangle_solid(0, 319, 0, 479, 0xFFFF);
```

---

## RGB565 Color Palette

Useful pre-defined colors in RGB565 format (5-bit R, 6-bit G, 5-bit B):

```c
#define COLOR_BLACK       0x0000  // 00000 000000 00000
#define COLOR_WHITE       0xFFFF  // 11111 111111 11111
#define COLOR_RED         0xF800  // 11111 000000 00000
#define COLOR_GREEN       0x07E0  // 00000 111111 00000
#define COLOR_BLUE        0x001F  // 00000 000000 11111
#define COLOR_YELLOW      0xFFE0  // 11111 111111 00000
#define COLOR_CYAN        0x07FF  // 00000 111111 11111
#define COLOR_MAGENTA     0xF81F  // 11111 000000 11111
#define COLOR_GRAY        0x8410  // 10000 100000 10000
#define COLOR_DARK_RED    0x8000  // 10000 000000 00000
#define COLOR_DARK_GREEN  0x0400  // 00000 100000 00000
#define COLOR_DARK_BLUE   0x0010  // 00000 000000 10000
```

Conversion formula from RGB888 to RGB565:
```c
uint16_t rgb888_to_rgb565(uint8_t r8, uint8_t g8, uint8_t b8)
{
    uint16_t r5 = (r8 >> 3) & 0x1F;  // Top 5 bits
    uint16_t g6 = (g8 >> 2) & 0x3F;  // Top 6 bits
    uint16_t b5 = (b8 >> 3) & 0x1F;  // Top 5 bits

    return (r5 << 11) | (g6 << 5) | b5;
}
```

---

## Command Transmission Timing (Datasheet Section 9)

| Operation | Min | Typical | Max | Notes |
|-----------|-----|---------|-----|-------|
| RSTB (reset) low | 1 μs | — | — | Pull low minimum 1 μs |
| RSTB release to first command | 120 ms | — | — | Allow oscillator stabilization |
| Software reset to next command | 5 ms | — | — | Internal register reset time |
| Sleep exit to next command | 120 ms | — | — | Power supply stabilization |
| Display on to first pixel | 10 ms | — | — | Recommended startup delay |
| SPI read cycle time | — | — | 200 ns @ 5MHz | Max clock 5 MHz |
| SPI write cycle time | — | — | 100 ns @ 10MHz | Max clock 10 MHz |
| Frame rate @ 60 Hz | — | — | 1/60 sec | Refresh interval = 16.67 ms |

---

## Troubleshooting Checklist

- [ ] SPI bus initialized at correct speed (≤ 5-10 MHz)
- [ ] GPIO_RESET pulled high and held high during operation
- [ ] GPIO_DC_SELECT properly toggling (LOW for commands, HIGH for data)
- [ ] 120 ms wait after hardware reset before soft reset
- [ ] 5 ms wait after software reset (0x01)
- [ ] 120 ms wait after sleep exit (0x11)
- [ ] Pixel format set (0x3A) before GRAM write
- [ ] Memory access control set before addressing (0x36)
- [ ] Column address set before row address (0x2A before 0x2B)
- [ ] GRAM write command (0x2C) issued before pixel transmit
- [ ] Power supply voltages in valid ranges (GVDD 3-5V typical)
- [ ] Display on command (0x29) issued after init sequence

---

## Examples by Use Case

### Use Case 1: Embedded Dashboard Display

```c
// Initialize once at startup
hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL);

// Main loop: update display every 100ms
while (1) {
    // Clear screen
    hal_fill_rectangle_solid(0, 319, 0, 479, COLOR_BLACK);

    // Draw widgets using fill functions
    hal_fill_rectangle_solid(10, 100, 10, 50, COLOR_BLUE);    // Temperature gauge
    hal_fill_rectangle_solid(10, 100, 70, 110, COLOR_GREEN);  // Pressure gauge
    hal_fill_rectangle_solid(10, 100, 130, 170, COLOR_RED);   // Status indicator

    usleep(100000);  // 100 ms update rate
}
```

### Use Case 2: Photo Display / Image Viewer

```c
// Load and decompress JPEG to framebuffer
uint8_t *framebuffer = load_jpeg_image("photo.jpg");

// Initialize display
hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL);

// Display image
hal_window_address_set(0, 319, 0, 479);
hal_gram_write_start();
hal_gram_write_pixels(framebuffer, 320 * 480, PIXEL_FORMAT_16BIT);

// Keep image displayed
sleep(5);  // Display for 5 seconds

// Transition: fade to black
for (int brightness = 255; brightness >= 0; brightness -= 10) {
    uint8_t new_gvdd = 0x17 * brightness / 255;
    hal_power_gvdd_set(new_gvdd);
    usleep(50000);
}

// Power down
hal_display_deinitialize();
```

### Use Case 3: Scrolling Text Display

```c
hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL);

// Configure scrolling: 50 pixels fixed top, 400 scrollable, 30 fixed bottom
hal_scroll_area_set(50, 400, 30);

// Copy pre-rendered text bitmap to GRAM (400*320 pixels high)
hal_window_address_set(0, 319, 50, 449);
hal_gram_write_start();
hal_gram_write_pixels(text_bitmap, 400 * 320, PIXEL_FORMAT_16BIT);

// Animate scroll
for (int offset = 0; offset < 400; offset += 10) {
    hal_scroll_start_address_set(offset);
    usleep(50000);
}
```

---

## Performance Benchmarks

Estimated performance on Raspberry Pi 4B @ 5 MHz SPI clock:

- Full screen clear (320x480 same color): ~1.6 seconds
- Full screen write from RAM (via spidev): ~2.0 seconds
- Display ID read: ~15 milliseconds
- Single pixel write: ~0.5 milliseconds
- 8x8 character write: ~0.8 milliseconds
- Gamma curve load (30 bytes): ~0.2 milliseconds

**Optimization potential**:
- Increase SPI to 10 MHz (2x faster for writes)
- Use DMA for bulk transfers (3-5x faster)
- Batch address + GRAM operations

---

## Conclusion

This reference guide covers all 50+ commands in the ILI9488 driver.
For detailed pseudocode and implementation notes, see [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md).
