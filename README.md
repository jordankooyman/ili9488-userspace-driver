# ILI9488 Linux Userspace Driver

**Userspace display library for the ILI9488 320x480 TFT LCD Controller**

[![Project](https://img.shields.io/badge/ECEN_5713-Final_Project-blue)](https://github.com/cu-ecen-aeld/final-project-assignment-jordankooyman)
[![Platform](https://img.shields.io/badge/Platform-Raspberry_Pi_4B-red)](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/)
[![Build](https://img.shields.io/badge/Build_System-Buildroot-orange)](https://buildroot.org/)

## Overview

This library provides a complete userspace driver for the ILI9488, a 320(RGB)×480 pixel 65K-color TFT LCD display controller/driver IC, a part of the [MSP3520](https://www.lcdwiki.com/3.5inch_SPI_Module_ILI9488_SKU%3AMSP3520) display. The library uses Linux's `spidev` kernel driver for SPI communication and `libgpiod` for GPIO control, enabling display operation entirely from userspace without requiring a custom kernel module.

**Part of:** [AESD Final Project - SPI Display Userspace Driver](https://github.com/cu-ecen-aeld/final-project-jordankooyman)

## Features

- **Full ILI9488 initialization** - Hardware reset, oscillator config, power-on sequence
- **RGB565 framebuffer** - Software framebuffer with efficient bulk writes
- **Graphics primitives** - Fill screen, draw pixel, draw rectangle
- **Portable architecture** - Clean HAL boundary allows porting to Arduino/ESP32
- **No kernel dependencies** - Uses standard `spidev` and `libgpiod` interfaces

## Hardware Support

### Display IC: ILI9488
- Resolution: 320(RGB) × 480 pixels
- Color depth: 65K (RGB565)
- Interface: SPI-compatible serial (up to 10MHz write)

### Tested Platform
- **Raspberry Pi 4 Model B** (BCM2711 SoC)
- **OS:** Raspberry Pi OS (64-bit) / Buildroot custom image
- **Kernel:** Linux 6.1+ with `spi-bcm2835` and `spidev` enabled

## Repository Structure

```
ili9488-userspace-driver/
├── README.md                    # This file
├── Makefile                     # Build system
├── include/
│   ├── ili9488_hal.h             # HAL layer API
│   ├── ili9488_spi.h             # SPI/GPIO abstraction API
│   └── ili9488_gfx.h             # Graphics layer API
├── src/
│   ├── ili9488_hal.c             # Command encoder, init sequence, GRAM writes
│   ├── ili9488_spi_linux.c       # Linux spidev + libgpiod implementation
│   └── ili9488_gfx.c             # Drawing primitives, framebuffer management
├── demo/
│   └── main.c                   # Example application
├── docs/
│   ├── wiring.md                # RPi4B GPIO pinout
│   └── ILI9488_datasheet.pdf     # IC datasheet (reference)
└── tests/
    └── test_colors.c            # Solid color fill test
```

## Quick Start (Proposed)

### 1. Hardware Wiring

Connect the ILI9488 to Raspberry Pi 4B SPI0:

| RPi4B Pin | GPIO | Function | ILI9488 Pin |
|-----------|------|----------|------------|
| 19        | GPIO10 | MOSI   | SDA (D1)   |
| 23        | GPIO11 | SCLK   | SCL (D0)   |
| 24        | GPIO8  | CE0    | CSB        |
| 22        | GPIO25 | GPIO   | A0 (D/C)   |
| 18        | GPIO24 | GPIO   | RSTB       |
| 1         | 3.3V   | Power  | VDD        |
| 6         | GND    | Ground | VSSD       |

See [`docs/wiring.md`](docs/wiring.md) for complete pinout and power supply details.

### 2. Enable SPI on Raspberry Pi OS

```bash
# Enable SPI interface
sudo raspi-config
# Interface Options → SPI → Yes

# Or edit /boot/config.txt:
echo "dtparam=spi=on" | sudo tee -a /boot/config.txt
sudo reboot

# Verify SPI device exists
ls -l /dev/spidev0.0
```

### 3. Install Dependencies

```bash
# Raspberry Pi OS
sudo apt update
sudo apt install -y build-essential libgpiod-dev git

# Verify libgpiod
pkg-config --modversion libgpiod
```

### 4. Build the Library

```bash
git clone https://github.com/jordankooyman/ili9488-userspace-driver.git
cd ili9488-userspace-driver

# Build library and demo
make

# Output:
# - libili9488.a (static library)
# - demo (example application)
```

### 5. Run the Demo

```bash
# Run as root (required for /dev/spidev and GPIO access)
sudo ./demo

# Expected output:
# [ILI9488] Initializing display...
# [ILI9488] Hardware reset
# [ILI9488] Software reset
# [ILI9488] Display ON
# Demo running...
```

## API Usage

### Basic Example

```c
#include "ili9488_hal.h"
#include "ili9488_gfx.h"

int main(void) {
    // Initialize the display
    if (ili9488_init("/dev/spidev0.0", 25, 24) != 0) {
        fprintf(stderr, "Display init failed\n");
        return 1;
    }

    // Fill screen with red
    gfx_fill(RGB565(255, 0, 0));
    ili9488_write_framebuffer();

    // Draw a green rectangle
    gfx_draw_rect(32, 32, 64, 64, RGB565(0, 255, 0));
    ili9488_write_framebuffer();

    // Cleanup
    ili9488_close();
    return 0;
}
```

### Graphics Functions

```c
// Fill entire screen
void gfx_fill(uint16_t color);

// Draw single pixel
void gfx_draw_pixel(int x, int y, uint16_t color);

// Draw filled rectangle
void gfx_draw_rect(int x, int y, int w, int h, uint16_t color);

// RGB565 color packing
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
```

## Cross-Compilation for Buildroot

This library is packaged as a Buildroot external package. See the main project repository for build instructions:

[buildroot-external/package/ili9488/](https://github.com/cu-ecen-aeld/final-project-assignment-jordankooyman/tree/main/buildroot-external/package/ili9488)

## Architecture

The library uses a three-layer architecture for portability:

```
┌─────────────────────────────────────┐
│   Application / Demo (main.c)       │
├─────────────────────────────────────┤
│   Graphics Layer (ili9488_gfx.c)    │  ← Portable
│   - Framebuffer management          │
│   - Drawing primitives              │
├─────────────────────────────────────┤
│   HAL Layer (ili9488_hal.c)         │  ← Portable
│   - ILI9488 command encoding        │
│   - Initialization sequence         │
│   - GRAM writes                     │
├─────────────────────────────────────┤
│   SPI/GPIO Abstraction              │  ← Platform-specific
│   (ili9488_spi_linux.c)             │     (Linux spidev + libgpiod)
└─────────────────────────────────────┘
```

Only `ili9488_spi_linux.c` is platform-specific. To port to Arduino/ESP32, replace this file with an equivalent implementation for that platform's SPI and GPIO APIs.

## Troubleshooting

### Display shows nothing / stays dark

1. **Check power supply:** ILI9488 requires VDD (3.3-5.0V)
2. **Verify SPI bus:** Run `sudo ./tests/spi_loopback` to confirm SPI is working
3. **Check wiring:** Verify MOSI, SCLK, CSB, A0, RSTB connections
4. **Increase init delay:** Edit `ili9488_hal.c` and increase reset pulse width or post-init delays

### "Permission denied" opening /dev/spidev0.0

- Run with `sudo`, or add user to `spi` group:
  ```bash
  sudo usermod -a -G spi $USER
  # Log out and back in
  ```

### "Failed to open GPIO chip"

- Ensure `/dev/gpiochip0` exists and is accessible
- `libgpiod` version must be ≥1.6
- Run with `sudo` or configure udev rules for GPIO access

## Development Status

See the [Project Schedule](https://github.com/cu-ecen-aeld/final-project-assignment-jordankooyman/wiki/Schedule) for detailed task breakdown.

## Contributing

This is a solo course project and is not accepting external contributions. However, feedback and bug reports are welcome via [Issues](https://github.com/jordankooyman/ili9488-userspace-driver/issues).

## References

- [ILI9488 Datasheet](docs/ILI9488%20datasheet.pdf)
- [Linux spidev Documentation](https://www.kernel.org/doc/Documentation/spi/spidev)
- [libgpiod Documentation](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/about/)

## License

This project is submitted as coursework for ECEN 5713, University of Colorado Boulder. See course policies for usage terms.

## AI Assistance Disclosure

AI tools (Claude by Anthropic) were used in planning this project's architecture and drafting documentation. All code implementation is original work. See the [AI Assistance Log](https://github.com/cu-ecen-aeld/final-project-assignment-jordankooyman/wiki/Project-Overview#ai-assistance-log) for conversation links.
