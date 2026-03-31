# Wiring ILI9488 Display to Raspberry Pi 4

The display is connected to the Raspberry Pi using the following GPIO pins:

| Display Pin | Raspberry Pi Pin | Raspberry Pi GPIO Function |
|-------------|------------------|----------------------------|
| SDO         | Pin 21           | GPIO 9 (SPI0_MISO)         |
| LED         | Pin 2            | 5V Power                   |
| SCK         | Pin 23           | GPIO 11 (SPI0_SCLK)        |
| SDI         | Pin 19           | GPIO 10 (SPI0_MOSI)        |
| DC/RS       | Pin 22           | GPIO 25                    |
| RESET       | Pin 18           | GPIO 24                    |
| CS          | Pin 24           | GPIO 8 (SPI0_CE0)          |
| GND         | Pin 20           | Ground                     |
| VCC         | Pin 17           | 3.3V Power                 |