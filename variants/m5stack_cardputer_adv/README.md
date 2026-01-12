# M5Stack Cardputer ADV with LoRa868 Cap (SX1262 + GPS)

This variant adds support for the M5Stack Cardputer ADV with the LoRa868 Cap expansion module (U201).

## Hardware Specifications

- **Main Board**: M5Stack Cardputer ADV (ESP32-S3)
- **LoRa Module**: Cap LoRa868 with SX1262 chip
- **GPS Module**: AT6668-based GNSS module with ceramic antenna
- **Frequency Band**: 868-923 MHz (Europe)
- **Display**: Built-in ST7789 TFT display
- **Keyboard**: Built-in QWERTY keyboard

## Pin Configuration

### LoRa SX1262 Pins
- **NSS (Chip Select)**: GPIO 5
- **MOSI**: GPIO 14
- **MISO**: GPIO 39
- **SCLK**: GPIO 40
- **IRQ (DIO1)**: GPIO 4
- **RESET**: GPIO 3
- **BUSY**: GPIO 6

### GPS UART Pins
- **RX**: GPIO 13
- **TX**: GPIO 15

### I2C Pins (Cardputer)
- **SDA**: GPIO 2
- **SCL**: GPIO 1

### User Button
- **Button**: GPIO 0 (Boot button)

### Display (ST7789)
- **RST**: GPIO 33
- **DC**: GPIO 34
- **CS**: GPIO 37
- **MOSI (SDA)**: GPIO 35
- **SCLK**: GPIO 36
- **Backlight**: GPIO 38

## Available Firmware Variants

The following firmware variants are available:

1. **M5Stack_Cardputer_ADV_repeater** - Simple mesh repeater with GPS
2. **M5Stack_Cardputer_ADV_room_server** - BBS-style chat room server
3. **M5Stack_Cardputer_ADV_companion_radio_usb** - Companion radio via USB
4. **M5Stack_Cardputer_ADV_companion_radio_ble** - Companion radio via Bluetooth LE
5. **M5Stack_Cardputer_ADV_companion_radio_wifi** - Companion radio via WiFi
6. **M5Stack_Cardputer_ADV_sensor** - Sensor node with GPS telemetry

## Building the Firmware

### Using build.sh script
```bash
./build.sh M5Stack_Cardputer_ADV_repeater
```

### Using PlatformIO directly
```bash
pio run -e M5Stack_Cardputer_ADV_companion_radio_ble
```

### Building all variants
```bash
./build.sh m5stack
```

## Flashing the Firmware

The firmware will be generated in the `release/` directory with a `.bin` file.

To flash using esptool.py:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 release/FIRMWARE_VERSION-M5Stack_Cardputer_ADV_repeater.bin
```

Or use the Arduino IDE / PlatformIO upload feature.

## Configuration

### Changing WiFi Credentials (for WiFi variant)
Edit [platformio.ini](platformio.ini) and modify:
```ini
-D WIFI_SSID='"your_ssid"'
-D WIFI_PWD='"your_password"'
```

### Changing Admin Password
Edit [platformio.ini](platformio.ini) and modify:
```ini
-D ADMIN_PASSWORD='"your_password"'
```

### Setting Your Location (for repeater/sensor variants)
Edit [platformio.ini](platformio.ini) and set your GPS coordinates:
```ini
-D ADVERT_LAT=52.2297  ; Latitude
-D ADVERT_LON=21.0122  ; Longitude
```

### Production / Public Builds (disable debug)
Edit [platformio.ini](platformio.ini) and ensure debug is off:
```ini
-D CORE_DEBUG_LEVEL=0
```

## Features

- LoRa mesh networking with SX1262 radio
- GPS location tracking with ATGM336H/AT6668 module
- Support for multiple firmware modes (repeater, room server, companion radio, sensor)
- 868 MHz frequency band (European ISM band)
- USB connectivity via Cardputer's built-in USB-C port
- Bluetooth LE and WiFi support for companion radio mode

## Hardware References

- [M5Stack Cardputer ADV](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3)
- [Cap LoRa868 Documentation](https://docs.m5stack.com/en/cap/Cap_LoRa868)
- [LoRa+GPS Cap Product Page](https://shop.m5stack.com/products/lora-gps-cap-for-cardputer-adv-sx1262-atgm336h)

## Notes

- The LoRa868 Cap (U201) is marked as EOL (End of Life), but the newer Cap LoRa-1262 uses the same pinout
- The GPS module supports multi-constellation GNSS (GPS, BeiDou, GLONASS, Galileo, QZSS)
- Default frequency is 869.525 MHz (EU863-870 ISM band)
- Maximum TX power is 22 dBm
- The Cardputer's built-in display and keyboard can be used for future UI enhancements

## Troubleshooting

- If the LoRa module is not detected, check that the Cap is properly seated on the Cardputer ADV
- Ensure the SMA antenna is connected before transmitting
- GPS may take several minutes to acquire a fix when first powered on (cold start)
- For GPS issues, ensure you have a clear view of the sky
