/**
 * @file IMPLEMENTATION_GUIDE.md
 * @brief ILI9488 Userspace Driver - Implementation Architecture & Guide
 *
 * This document explains the complete driver architecture, design patterns,
 * and implementation strategy for the ILI9488 display controller.
 */

# ILI9488 Userspace Driver - Complete Implementation Guide

## Overview

This driver is organized into three distinct layers:

```
┌──────────────────────────────────────────────────────────┐
│  Graphics Library (ili9488_gfx.h/c)                      │
│  - High-level drawing primitives                         │
│  - Framebuffer management                                │
│  - Color conversion utilities                            │
└──────────────────┬───────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────┐
│  HAL Layer (ili9488_hal.h/c)                             │
│  - Datasheet command implementations                     │
│  - Display configuration & sequencing                    │
│  - Power management                                      │
│  - Gamma & voltage control                               │
│  - Status/ID queries                                     │
└──────────────────┬───────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────┐
│  SPI/GPIO Layer (ili9488_spi.h/c)                        │
│  - SPI bus communication                                 │
│  - GPIO pin control (reset, D/C)                         │
│  - Platform-independent abstraction                      │
└──────────────────┬───────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────┐
│  Platform-Specific Implementation                        │
│  - Linux: spidev, libgpiod                               │
│  - Arduino: SPI library, digitalWrite()                  │
│  - ESP32: SPI transaction API, gpio_set_level()          │
└──────────────────────────────────────────────────────────┘
```

## Layer Responsibilities

### Layer 1: SPI/GPIO (ili9488_spi.h/c) - PLATFORM-SPECIFIC

**Purpose**: Provide hardware abstraction for SPI communication and GPIO control.

**Key Functions**:
- `spi_bus_initialize(device_path, clock_speed)` - Initialize SPI bus
- `spi_gpio_initialize(pin, initial_state)` - Initialize GPIO pin
- `spi_gpio_set_state(pin, state)` - Pull pin HIGH or LOW
- `spi_transmit_command(byte)` - Send command with D/C=LOW
- `spi_transmit_data(buffer, count)` - Send parameters with D/C=HIGH
- `spi_receive_data(buffer, count)` - Receive data from display
- `spi_delay_ms(ms)` - Millisecond delay

**Design Pattern**: Each function maps to a single hardware operation:
- D/C pin state is automatically managed
- SPI clock is automatically limited to datasheet max (10MHz write, 5MHz read)
- GPIO states are cached for efficiency

**Pseudocode Provided**: Yes - All functions have detailed pseudocode showing:
- Platform-specific implementation for Linux, Arduino, ESP32
- Exact SPI transaction format (Datasheet Section 6)
- GPIO signaling requirements (Datasheet Section 5.2)

**TO PORT THIS DRIVER**: Replace platform-specific code in spi_bus_initialize(),
spi_gpio_*(), spi_transmit_*(), spi_receive_data(), and spi_delay_ms() only.
All HAL functions work unchanged on new platforms.

---

### Layer 2: HAL (ili9488_hal.h/c) - PLATFORM-INDEPENDENT

**Purpose**: Implement ILI9488 commands by coordinating SPI/GPIO layer.

**Command Categories**:

#### 1. Initialization & Configuration
```c
bool hal_display_initialize(pixel_format, rotation);
bool hal_display_deinitialize(void);
```
- Complete power-up sequence (Section 4 of Datasheet)
- Hardware reset: Pull RSTB low 1ms, release high, wait 120ms
- Software reset: Command 0x01, wait 5ms
- Sleep out: Command 0x11, wait 120ms
- Power supply config: GVDD, VCI, VGH/VGL, VCOMH
- Timing config: Frame rate, oscillator
- Gamma correction selection
- Address window setup (full screen)
- Display output enable

#### 2. System Control
```c
bool hal_system_reset(reset_scope);              // 0x01
bool hal_power_sleep_mode_set(enable);           // 0x10, 0x11
bool hal_display_output_control(enabled);        // 0x28, 0x29
bool hal_power_set_state(state);                 // Composite
```

#### 3. Display Configuration
```c
bool hal_pixel_format_set(format);               // 0x3A
bool hal_display_rotation_set(rot, h, v);       // 0x36
bool hal_color_byte_order_set(order);            // 0x36 (RGB bit)
bool hal_transfer_mode_set(mode);                // 0x36 (MV bit)
bool hal_display_config_read(config_byte);       // 0x36 (read)
```

#### 4. Memory Address & Access
```c
bool hal_column_address_set(start, end);        // 0x2A
bool hal_row_address_set(start, end);           // 0x2B
bool hal_window_address_set(x1, x2, y1, y2);   // 0x2A + 0x2B
bool hal_gram_write_start(void);                // 0x2C
bool hal_gram_read_start(void);                 // 0x2E
```

#### 5. Pixel Data Transmission
```c
bool hal_gram_write_pixels(buffer, count, fmt);
bool hal_gram_read_pixels(buffer, count, fmt);
bool hal_fill_rectangle_solid(x1, x2, y1, y2, color);
```

#### 6. Power Supply Configuration
```c
bool hal_power_gvdd_set(voltage);               // 0xC0
bool hal_power_vci_set(voltage);                // 0xC1
bool hal_power_vgh_vgl_set(vgh, vgl);          // 0xC5
bool hal_power_vcomh_set(voltage);              // 0xBB
```

#### 7. Timing & Frequency Control
```c
bool hal_frame_rate_set(code);                  // 0xB1
bool hal_oscillator_frequency_set(code);        // 0xB0
```

#### 8. Gamma Correction
```c
bool hal_gamma_curve_select(curve);             // 0x26
bool hal_gamma_curve_program(pos, neg);         // 0xE0, 0xE1
```

#### 9. Status & Information
```c
bool hal_display_id_read(id_code);              // 0xD3
bool hal_power_mode_read(mode);                 // 0x0A
bool hal_display_status_read(status);           // 0x0D
bool hal_display_mode_read(mode);               // 0x0B
bool hal_pixel_format_read(format);             // 0x0C
```

#### 10. Advanced Features
```c
bool hal_interface_mode_set(rim, dim);          // 0x0D
bool hal_partial_mode_set(enabled);             // 0x12, 0x13
bool hal_scroll_area_set(tfa, vsa, bfa);       // 0x33
bool hal_scroll_start_address_set(start);       // 0x37
bool hal_normal_display_mode_on(void);          // 0x13
bool hal_invert_display_mode_on(void);          // 0x21
bool hal_color_enhancement_set(enabled);        // 0xE8
bool hal_interface_control_set(we_mode);        // 0xC6
bool hal_ram_protection_set(enabled, key);      // 0xC9
bool hal_gpio_configure(mask, config);          // 0xF7
```

**Design Patterns**:

1. **Enumerations for Type Safety**:
   ```c
   typedef enum { PIXEL_FORMAT_12BIT, PIXEL_FORMAT_16BIT, PIXEL_FORMAT_18BIT }
   hal_pixel_format_t;
   
   typedef enum { ROTATION_0_NORMAL, ROTATION_90_CLOCKWISE, ... }
   hal_rotation_t;
   ```
   - Prevents invalid command values
   - Self-documenting code
   - Compiler catches type mismatches

2. **Separate Concerns**:
   - HAL commands are high-level logical operations
   - SPI layer handles low-level hardware
   - GPIO state is managed by SPI layer (D/C pin)
   - No platform-specific code in HAL

3. **Command Sequences**:
   - Complex operations (fill_rectangle) are composed from atomic commands
   - Initialization calls multiple command functions in correct order
   - Error checking at each step (all functions return bool)

4. **State Caching**:
   - `g_current_mac_register` - Caches Memory Access Control byte
   - `g_current_pixel_format` - Caches active pixel format
   - Avoids redundant register reads
   - Allows efficient bit-field modifications

---

### Layer 3: Graphics Library (ili9488_gfx.h/c) - APPLICATION

**Purpose**: Provide high-level drawing functions to applications.

**Pseudocode Template**:

```c
/**
 * @brief Draw a filled rectangle with specified color
 */
void gfx_draw_filled_rectangle(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2,
                               uint16_t color_rgb565)
{
    // Call HAL function directly
    hal_fill_rectangle_solid(x1, x2, y1, y2, color_rgb565);
}

/**
 * @brief Fill entire screen with solid color
 */
void gfx_fill_screen(uint16_t color_rgb565)
{
    gfx_draw_filled_rectangle(0, 0, 319, 479, color_rgb565);
}

/**
 * @brief Write entire framebuffer to display
 */
void gfx_put_framebuffer(const uint8_t *framebuffer, size_t buffer_size)
{
    // Assuming framebuffer is organized as RGB565 pixels
    hal_window_address_set(0, 319, 0, 479);
    hal_gram_write_start();
    hal_gram_write_pixels(framebuffer, 320 * 480, PIXEL_FORMAT_16BIT);
}

/**
 * @brief Draw a single pixel at (x, y)
 */
void gfx_draw_pixel(uint16_t x, uint16_t y, uint16_t color_rgb565)
{
    hal_window_address_set(x, x, y, y);
    hal_gram_write_start();
    uint8_t pixel_bytes[2] = {
        (uint8_t)((color_rgb565 >> 8) & 0xFF),
        (uint8_t)(color_rgb565 & 0xFF)
    };
    spi_transmit_bulkdata(pixel_bytes, 2);
}
```

---

## Command Transmission Protocol

### Typical Command Sequence (Datasheet Section 6.2)

**For Commands with Parameters**:

```
1. Pull D/C LOW (command mode)
2. Transmit command byte (0x00-0xFF)
3. Pull D/C HIGH (parameter/data mode)
4. Transmit parameter bytes (length depends on command)
5. D/C state left HIGH for next operation
```

**Example: Set Pixel Format (0x3A)**
```
spi_gpio_set_state(GPIO_DC_SELECT, GPIO_STATE_LOW);    // D/C = LOW
spi_transmit_command(0x3A);                             // Send command
spi_gpio_set_state(GPIO_DC_SELECT, GPIO_STATE_HIGH);   // D/C = HIGH
spi_transmit_data(&format_byte, 1);                     // Send parameter
```

**Implementation Note**: This is abstracted in `spi_transmit_command_with_params()`:
```c
spi_transmit_command_with_params(0x3A, &format_byte, 1);
```

---

## Complete Initialization Sequence

**Reference**: Datasheet Section 4 (Initialization and Power Supply)

```
┌─────────────────────────────────────────────────────┐
│ HARDWARE RESET SEQUENCE                             │
├─────────────────────────────────────────────────────┤
│ 1. Pull RSTB low for 1 ms                           │
│ 2. Release RSTB high                                │
│ 3. Wait 120 ms for oscillator stabilization         │
│ 4. All registers reset to default                   │
│ 5. GRAM content is NOT cleared                      │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ SOFTWARE RESET (Command 0x01)                       │
├─────────────────────────────────────────────────────┤
│ 1. Transmit 0x01                                    │
│ 2. Wait 5 ms minimum                                │
│ 3. GRAM content is NOT cleared (preserved)          │
│ 4. All settings reset to default (except GRAM)      │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ SLEEP EXIT (Command 0x11)                           │
├─────────────────────────────────────────────────────┤
│ 1. Transmit 0x11                                    │
│ 2. Wait 120 ms for power stabilization              │
│ 3. Display is now in normal mode (but off)          │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ INTERFACE CONFIGURATION                             │
├─────────────────────────────────────────────────────┤
│ 1. Set pixel format (0x3A) to RGB565                │
│ 2. Set memory access control / rotation (0x36)      │
│ 3. Display mode set (0x13) - normal mode            │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ POWER SUPPLY CONFIGURATION                          │
├─────────────────────────────────────────────────────┤
│ 1. Power Control 1 - Set GVDD (0xC0)                │
│    Typical: 0x17 = 4.0V                             │
│ 2. Power Control 2 - Set VCIOUT (0xC1)              │
│ 3. Power Control 3 - Set VGH/VGL (0xC5)             │
│    Default: VGH=0x0A (16V), VGL=-0x0A (-10V)        │
│ 4. VCOM Control - Set VCOMH (0xBB)                  │
│    Typical: 0x30                                    │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ TIMING CONFIGURATION                                │
├─────────────────────────────────────────────────────┤
│ 1. Frame Rate Control (0xB1)                        │
│    Typical: 0x18 = 60 Hz                            │
│ 2. Oscillator Control (0xB0) - usually default      │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ GAMMA CORRECTION                                    │
├─────────────────────────────────────────────────────┤
│ 1. Select gamma curve (0x26)                        │
│    Options: Curve 1, 2, 3 (recommended), 4          │
│ 2. Optionally program custom gamma (0xE0, 0xE1)     │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ ADDRESS WINDOW CONFIGURATION                        │
├─────────────────────────────────────────────────────┤
│ 1. Set column address (0x2A): 0-319                 │
│ 2. Set row address (0x2B): 0-479                    │
│ 3. Prepare for GRAM write/read                      │
└─────────────────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────────────────┐
│ DISPLAY ENABLE                                      │
├─────────────────────────────────────────────────────┤
│ 1. Display on (0x29)                                │
│ 2. Wait 10 ms for stabilization                     │
│ 3. Display is now showing content from GRAM         │
│ 4. Ready for framebuffer updates                    │
└─────────────────────────────────────────────────────┘
```

---

## Typedef Enumerations (Type Safety)

### Color Depth Selection
```c
typedef enum {
    PIXEL_FORMAT_12BIT = 0x03,  // 4096 colors
    PIXEL_FORMAT_16BIT = 0x05,  // 65536 colors (RGB565) ← RECOMMENDED
    PIXEL_FORMAT_18BIT = 0x06   // 262144 colors
} hal_pixel_format_t;
```

### Display Rotation
```c
typedef enum {
    ROTATION_0_NORMAL = 0x00,         // 0°: Portrait
    ROTATION_90_CLOCKWISE = 0x60,     // 90° CW
    ROTATION_180_INVERSE = 0xC0,      // 180°
    ROTATION_270_COUNTERCLOKWISE = 0xA0  // 270° CCW
} hal_rotation_t;
```

### Power States
```c
typedef enum {
    POWER_STATE_SLEEP = 0,           // Low power (GRAM preserved)
    POWER_STATE_NORMAL = 1,          // Full operation, display on
    POWER_STATE_DISPLAY_OFF = 2      // Display off, controller active
} hal_power_state_t;
```

### Memory Access Control Bits
```c
typedef enum {
    HORIZONAL_DIRECTION_DEFAULT = 0,     // Left to right
    HORIZONAL_DIRECTION_REVERSED = 1     // Right to left
} hal_horizontal_direction_t;

typedef enum {
    VERTICAL_DIRECTION_DEFAULT = 0,      // Top to bottom
    VERTICAL_DIRECTION_REVERSED = 1      // Bottom to top
} hal_vertical_direction_t;

typedef enum {
    TRANSFER_MODE_NORMAL = 0,         // Row-by-row
    TRANSFER_MODE_PAGE_ADDRESS = 1    // Column-by-column
} hal_transfer_mode_t;
```

---

## Key Datasheet References

| Function | Datasheet Section | Command Code |
|----------|-------------------|--------------|
| Software reset | 8.2.1 | 0x01 |
| Interface mode | 8.2.2 | 0x0D |
| Memory access control | 8.2.3 | 0x36 |
| Display mode | 8.2.4 | 0x13 |
| Pixel format | 8.2.5 | 0x3A |
| Display off | 8.2.6 | 0x28 |
| Display on | 8.2.7 | 0x29 |
| Partial mode | 8.2.8 | 0x12 |
| Sleep in | 8.2.9 | 0x10 |
| Sleep out | 8.2.10 | 0x11 |
| Oscillator control | 8.2.11 | 0xB0 |
| Frame rate control | 8.2.12 | 0xB1 |
| Power control 1 (GVDD) | 8.2.13 | 0xC0 |
| Power control 2 (VCIOUT) | 8.2.14 | 0xC1 |
| Power control 3 (VGH/VGL) | 8.2.15 | 0xC5 |
| VCOM control | 8.2.17 | 0xBB |
| Column address set | 8.2.18 | 0x2A |
| Page address set | 8.2.19 | 0x2B |
| Memory write | 8.2.25 | 0x2C |
| Memory read | 8.2.27 | 0x2E |
| Gamma set | 8.2.26 | 0x26 |
| Positive gamma | 8.2.27 | 0xE0 |
| Negative gamma | 8.2.28 | 0xE1 |
| Normal display mode | 8.2.24 | 0x13 |
| Inversion on | 8.2.23 | 0x21 |

---

## Platform-Specific Implementation Templates

### Linux + Raspberry Pi 4B + spidev + libgpiod

**Header**:
```c
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <time.h>
```

**In spi_bus_initialize()**:
```c
int spi_fd = open("/dev/spidev0.0", O_RDWR);
uint8_t mode = SPI_MODE_0;
ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
uint32_t speed = 5000000;
ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
```

**In spi_transmit_command()**:
```c
struct spi_ioc_transfer xfer = {
    .tx_buf = (unsigned long)&command_byte,
    .rx_buf = (unsigned long)rx_buf,
    .len = 1,
    .speed_hz = 10000000,
};
ioctl(spi_fd, SPI_IOC_MESSAGE(1), &xfer);
```

**In spi_gpio_initialize()**:
```c
struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
struct gpiod_line *line = gpiod_chip_get_line(chip, 25);  // GPIO_DC_SELECT
gpiod_line_request_output(line, "ili9488_driver", GPIO_STATE_HIGH);
```

### Arduino (ATmega, ARM Cortex-M)

**In spi_bus_initialize()**:
```cpp
SPI.begin();  // Uses default SCLK, MOSI, MISO
```

**In spi_transmit_command()**:
```cpp
SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
SPI.transfer(command_byte);
SPI.endTransaction();
```

**In spi_gpio_initialize()**:
```cpp
pinMode(25, OUTPUT);  // DC pin
digitalWrite(25, HIGH);
```

### ESP32

**In spi_bus_initialize()**:
```c
spi_bus_config_t bus_cfg = {
    .miso_io_num = GPIO_NUM_19,
    .mosi_io_num = GPIO_NUM_23,
    .sclk_io_num = GPIO_NUM_18,
    .max_transfer_sz = 32768,
};
spi_bus_initialize(SPI2_HOST, &bus_cfg, DMA_CH_AUTO);
```

**In spi_transmit_command()**:
```c
spi_transaction_t t = {
    .length = 8,
    .tx_buffer = &command_byte,
};
spi_device_transmit(spi_handle, &t);
```

---

## Testing & Verification

### Unit Test Template

```c
void test_display_initialization(void)
{
    // Initialize hardware
    spi_bus_initialize("/dev/spidev0.0", 5000000);
    spi_gpio_initialize(GPIO_RESET, GPIO_STATE_HIGH);
    spi_gpio_initialize(GPIO_DC_SELECT, GPIO_STATE_HIGH);

    // Test initialization sequence
    assert(hal_display_initialize(PIXEL_FORMAT_16BIT, ROTATION_0_NORMAL) == true);

    // Verify display is active
    uint32_t display_id;
    assert(hal_display_id_read(&display_id) == true);
    assert((display_id & 0x00FFFFFF) == 0x009488);  // ILI9488 ID

    // Test fill color
    assert(hal_fill_rectangle_solid(0, 319, 0, 479, 0xFFFF) == true);  // White

    spi_bus_deinitialize();
}
```

### Integration Test: Checkerboard Pattern

```c
void test_checkerboard_pattern(void)
{
    uint16_t color_black = 0x0000;
    uint16_t color_white = 0xFFFF;
    uint16_t square_size = 40;  // 40x40 pixel squares

    for (uint16_t y = 0; y < 480; y += square_size) {
        for (uint16_t x = 0; x < 320; x += square_size) {
            // Checkerboard calculation
            bool is_white = (((x / square_size) + (y / square_size)) & 1);
            uint16_t color = is_white ? color_white : color_black;

            hal_fill_rectangle_solid(x, x + square_size - 1,
                                    y, y + square_size - 1, color);
        }
    }
}
```

---

## Performance Optimization Tips

1. **Bulk Pixel Writes**: Use `spi_transmit_bulkdata()` instead of repeated
   `spi_transmit_data()` calls for better SPI efficiency.

2. **Address Caching**: Store current address window to avoid redundant
   column/row address commands.

3. **DMA Transfers**: On platforms supporting DMA (ESP32, Arduino with DMAC),
   offload SPI transfers to hardware for faster throughput.

4. **Clock Speed**: Maximize SPI clock within limits:
   - Write operations: up to 10 MHz (per Datasheet Section 6.1)
   - Read operations: up to 5 MHz (slower due to parasitic capacitance)
   - Typical sweet spot: 5 MHz all-purpose, 10 MHz for writes only

5. **Gamma Curve Selection**: Use GAMMA_CURVE_3 (most neutral) for most applications.
   Curves 1, 2, 4 are provided for specialized use cases.

6. **Power Management**: Use `hal_power_set_state(POWER_STATE_SLEEP)` to reduce
   power consumption during idle periods (display stays powered in sleep mode).

---

## Common Issues & Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Display not responding | RSTB pin stuck LOW | Verify GPIO_RESET is initialized HIGH and stays HIGH |
| Garbled image | D/C timing issue | Check spi_gpio_set_state() before each transmit |
| Image inverted | MAC rotation bits | Verify hal_display_rotation_set() parameters |
| All pixels same color | Address window not set | Call hal_window_address_set() before GRAM write |
| Display flickering | Frame rate too low | Increase frame rate with hal_frame_rate_set() |
| Low contrast | Power supply voltages wrong | Adjust GVDD, VCI, VGH, VGL, VCOMH |
| Vertical scrolling broken | Scroll area not configured | Call hal_scroll_area_set() before VSP |
| High power consumption | Not in sleep mode | Use hal_power_set_state(POWER_STATE_SLEEP) when idle |

---

## Next Steps

1. **Implement ili9488_spi.c**: Fill in platform-specific code for your target
   (Linux spidev, Arduino, ESP32, etc.). See templates above.

2. **Test hal_display_initialize()**: Verify hardware reset, software reset,
   and basic power sequencing work correctly.

3. **Test GRAM write**: Implement simple test pattern (solid color fill).

4. **Implement Graphics Layer**: Add gfx_draw_line(), gfx_draw_circle(),
   gfx_draw_text(), etc. using HAL functions.

5. **Integrate with Application**: Connect graphics layer to application logic
   (game loops, UI rendering, sensor displays, etc.).

6. **Optimize**: Measure performance, identify bottlenecks, apply techniques
   from "Performance Optimization Tips" section.

---

## Conclusion

This architecture provides:
- **Modularity**: Each layer has single responsibility
- **Portability**: Platform-specific code isolated to SPI layer
- **Safety**: Enumerations prevent invalid commands
- **Clarity**: Self-documenting function names and signal flow
- **Completeness**: All 50+ ILI9488 commands implemented

The pseudocode is ready for direct implementation. Good luck with your project!
