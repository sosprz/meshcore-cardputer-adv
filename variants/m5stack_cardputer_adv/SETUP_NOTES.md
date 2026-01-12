# M5Stack Cardputer ADV + LoRa868 Cap Setup Notes

## Hardware
- **Device**: M5Stack Cardputer ADV (ESP32-S3)
- **LoRa Module**: Cap LoRa868 (U201) with SX1262 + GPS ATGM336H
- **Frequency**: 868 MHz (Europe)

## Pin Configuration (VERIFIED)

### SX1262 LoRa Pins
```
NSS (CS):    GPIO 5
MOSI:        GPIO 14
MISO:        GPIO 39
SCLK:        GPIO 40
IRQ (DIO1):  GPIO 4
RESET:       GPIO 3
BUSY:        GPIO 6
```

### GPS ATGM336H Pins
```
RX:  GPIO 13
TX:  GPIO 16
```

### I2C Pins (Cardputer)
```
SDA: GPIO 2
SCL: GPIO 1
```

### Other
```
User Button: GPIO 0
```

## Configuration Settings
- **TCXO Voltage**: 1.8V
- **Current Limit**: 140mA
- **TX Power**: 22 dBm
- **Frequency**: 868.0 MHz
- **Bandwidth**: 125 kHz
- **Spreading Factor**: 11
- **USB CDC on Boot**: Enabled (required for ESP32-S3)

## Current Status

### What Works
✅ Serial output via USB
✅ Pin configuration verified
✅ Firmware compiles and runs
✅ I2C initialization
✅ RTC clock initialization

### Known Issue
❌ **Radio initialization fails with error -707** (RADIOLIB_ERR_SPI_CMD_FAILED)

This error indicates the SX1262 module is not responding to SPI commands.

## Troubleshooting Attempted

1. ✅ Corrected I2C pins (SDA=2, SCL=1)
2. ✅ Corrected GPS pins (RX=13, TX=16)
3. ✅ Verified all SX1262 pins
4. ✅ Enabled USB CDC for ESP32-S3
5. ✅ Tried various TCXO voltages (0.0, 1.8)
6. ✅ Tried enabling potential power pins (46, 10, 12, 21, 11)
7. ✅ Removed GPS to avoid pin conflicts
8. ✅ Verified hardware is seated properly
9. ✅ Antenna is connected

## Possible Causes

1. **Hardware Incompatibility**: The LoRa868 Cap (U201) may have compatibility issues with the Cardputer ADV. GitHub issue #2 in the Stachugit fork reports the same error -707.

2. **Missing Power Enable**: The module might require a specific GPIO to be set HIGH to enable power, which we haven't identified.

3. **Hardware Defect**: The LoRa Cap or Cardputer ADV connector could be defective.

4. **SPI Bus Conflict**: Another device might be interfering with the SPI bus.

## Next Steps to Try

1. **Check with M5Stack Support**: Contact M5Stack to ask about proper initialization of LoRa868 Cap with Cardputer ADV.

2. **Try Different LoRa Cap**: If possible, test with a different LoRa868 Cap to rule out hardware defect.

3. **Arduino IDE Test**: Try a simple Arduino sketch to test SPI communication:
   ```cpp
   SPIClass spi;
   spi.begin(40, 39, 14, 5);  // SCLK, MISO, MOSI, CS
   // Try basic SPI read/write operations
   ```

4. **Check for Power Regulator**: Some modules have an onboard power regulator that needs enabling.

## Files Created

```
variants/m5stack_cardputer_adv/
├── platformio.ini        # Build configuration
├── target.h              # Header with hardware definitions
├── target.cpp            # Implementation with radio init
├── README.md             # User documentation
└── SETUP_NOTES.md        # This file - technical notes
```

## Build Commands

```bash
# Build repeater variant
FIRMWARE_VERSION=v1.0.0 ./build.sh build-firmware M5Stack_Cardputer_ADV_repeater

# Build BLE companion radio
FIRMWARE_VERSION=v1.0.0 ./build.sh build-firmware M5Stack_Cardputer_ADV_companion_radio_ble

# Flash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem* write_flash 0x0 out/FIRMWARE.bin

# Monitor
pio device monitor --port /dev/cu.usbmodem* --baud 115200
```

## References
- [M5Stack LoRa868 Cap Documentation](https://docs.m5stack.com/en/cap/Cap_LoRa868)
- [GitHub Issue #2 - Same error](https://github.com/Stachugit/MeshCore-Cardputer-ADV/issues/2)
- [MeshCore Project](https://github.com/geoffwhittington/MeshCore)

## Conclusion

The configuration is correct according to M5Stack documentation, but the SX1262 module is not responding via SPI. This appears to be a known issue with this specific hardware combination. The error -707 suggests either:
- A missing hardware initialization step (power enable, reset sequence, etc.)
- Hardware incompatibility between the LoRa868 Cap and Cardputer ADV
- Defective hardware

**Note**: The same error (-707) was reported by another user with the same hardware setup, suggesting this is not a configuration issue but a hardware/compatibility issue.
