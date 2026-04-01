#ifndef CONFIG_H
#define CONFIG_H

#define PLATFORM LINUX // Options: LINUX, ARDUINO, ESP32

/*
 * Raspberry Pi 4B / Linux configuration.
 * BCM GPIO numbers are 0-27 on the 40-pin header for this driver.
 * Keep RESET and D/C on separate output-capable GPIOs.
 */
#define ILI9488_GPIO_CHIP_PATH   "/dev/gpiochip0"
#define ILI9488_GPIO_RESET_BCM   24U  /* BCM GPIO 24, physical pin 18, active-low reset */
#define ILI9488_GPIO_DC_BCM      25U  /* BCM GPIO 25, physical pin 22, data/command select */

/*
 * SPI device path used by the Raspberry Pi 4B test setup.
 * Typical values are /dev/spidev0.0 or /dev/spidev0.1.
 */
#define ILI9488_SPI_DEVICE_PATH  "/dev/spidev0.0"

#endif /* CONFIG_H */