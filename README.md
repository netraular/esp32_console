Simple speaker app using ESP32-S3-N16R8, MAX98357A, 20x20x5mm Round 8 Ohm 1W speaker and a micro SD card reader. The ESP32-S3-N16R8 does not have a built-in DAC but has I2S.
Esp-idf config edited options:
Main task stack size = 8192
PSRAM -> Support for external, SPI-connected RAM
SPI RAM config -> Mode (QUAD/OCT) of SPI RAM chip in use -> Octal Mode PSRAM
SPI RAM config -> Set RAM clock speed -> 80MHz clock speed
Flash size -> 16 MB
CPU frequency -> 240 MHz
FAT Filesystem support -> Long filename support with heap
Partition table -> Custom partition table
Max WiFi TX power (dBm) -> 10
LVGL -> 3rd Party Libraries -> PNG decoder library -> LV_USE_LODEPNG
LVGL -> File system on top of littlefs API
LVGL -> Enable system monitor component

To use libpng as the decoder library you need to edit the lvgl component managed_components\lvgl__lvgl\idf_component.yml and append the following text:
```
dependencies:
  idf: ">=4.1"
  # --- ADD THIS DEPENDENCY ---
  espressif/libpng: ">=1.6.0" 
```
And also add "espressif/libpng: ^1.6.39~1" component to main\idf_component.yml