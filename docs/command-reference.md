# ILI9488 HAL command reference

This file maps HAL functions to ILI9488 command bytes used by the current implementation.

Notes:

- Some HAL functions are convenience wrappers over multiple commands.
- A few advanced calls rely on vendor-specific behavior from known working init paths.

## Core system and display control

| Function | Command(s) | Purpose |
|---|---|---|
| `hal_system_reset` | `0x01` | Software reset |
| `hal_power_sleep_mode_set(true)` | `0x10` | Enter sleep |
| `hal_power_sleep_mode_set(false)` / `hal_power_sleep_exit` | `0x11` | Exit sleep |
| `hal_display_output_control(false)` | `0x28` | Display off |
| `hal_display_output_control(true)` | `0x29` | Display on |
| `hal_normal_display_mode_on` | `0x13` | Normal display mode |
| `hal_invert_display_mode_on` | `0x21` | Display inversion on |

## Pixel format and addressing mode

| Function | Command(s) | Purpose |
|---|---|---|
| `hal_pixel_format_set` | `0x3A` | Set transfer pixel format |
| `hal_display_rotation_set` | `0x36` | Set rotation and axis mapping |
| `hal_color_byte_order_set` | `0x36` | Set RGB or BGR bit |
| `hal_transfer_mode_set` | `0x36` | Set row or column stepping mode |
| `hal_display_config_read` | `0x0B` | Read display mode state used by HAL |

## Address window and GRAM access

| Function | Command(s) | Purpose |
|---|---|---|
| `hal_column_address_set` | `0x2A` | Set column address range |
| `hal_row_address_set` | `0x2B` | Set row address range |
| `hal_window_address_set` | `0x2A` + `0x2B` | Set full drawing window |
| `hal_gram_write_start` | `0x2C` | Start memory write |
| `hal_gram_read_start` | `0x2E` | Start memory read |
| `hal_gram_write_pixels` | `0x2C` data phase | Stream pixel payload |
| `hal_gram_read_pixels` | `0x2E` data phase | Read pixel payload |
| `hal_fill_rectangle_solid` | `0x2A` + `0x2B` + `0x2C` | Fill rectangle with one RGB565 color |

## Power, timing, and tuning registers

| Function | Command(s) |
|---|---|
| `hal_power_gvdd_set` | `0xC0` |
| `hal_power_vci_set` | `0xC1` |
| `hal_power_vgh_vgl_set` | `0xC5` |
| `hal_power_vcomh_set` | `0xBB` |
| `hal_frame_rate_set` | `0xB1` |
| `hal_oscillator_frequency_set` | `0xB0` |
| `hal_display_inversion_control_set` | `0xB4` |
| `hal_display_function_control_set` | `0xB6` |
| `hal_entry_mode_set` | `0xB7` |
| `hal_gamma_curve_select` | `0x26` |
| `hal_gamma_curve_program` | `0xE0` + `0xE1` |

## Readback and status

| Function | Command(s) | Purpose |
|---|---|---|
| `hal_display_id_read` | `0xD3` | Read display ID bytes |
| `hal_power_mode_read` | `0x0A` | Read power mode |
| `hal_display_status_read` | `0x09` | Read display status |
| `hal_display_mode_read` | `0x0B` | Read display mode |
| `hal_pixel_format_read` | `0x0C` | Read pixel format |

## Advanced and optional controls

| Function | Command(s) |
|---|---|
| `hal_interface_mode_set` | `0xB0` |
| `hal_partial_mode_set(true)` | `0x12` |
| `hal_partial_mode_set(false)` | `0x13` |
| `hal_scroll_area_set` | `0x33` |
| `hal_scroll_start_address_set` | `0x37` |
| `hal_color_enhancement_set` | `0xE8` |
| `hal_interface_control_set` | `0xC6` |
| `hal_ram_protection_set` | `0xC9` |
| `hal_gpio_configure` | `0xF7` |

## Where command logic lives

- API declarations: [include/ili9488_hal.h](../include/ili9488_hal.h)
- Implementations: [src/ili9488_hal.c](../src/ili9488_hal.c)
