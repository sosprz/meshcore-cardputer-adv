# M5Stack Cardputer ADV Firmware Flash Guide

## Download Firmware

Get the latest release files here:  
[[https://github.com/sosprz/meshcore-cardputer-adv/releases)](https://github.com/sosprz/meshcore-cardputer-adv/releases)


## Firmware Update (M5Burner)

1) Install and open `M5Burner`.
2) Connect your M5Stack Cardputer ADV by USB-C.
3) Choose Your device 
4) Coming soon

## Firmware Update (ESP Flasher)

You can also flash with any `ESP Flasher` desktop tool that supports ESP32-S3.

1) Connect Cardputer ADV with USB-C.
2) Select chip `ESP32-S3`.
3) Select firmware `.bin` file.
4) Set flash address to `0x0`.
5) Start flash and wait for completion.
6) Reboot the device.

## Firmware Update (Web Flasher)

You can also flash using ESPConnect web flasher:

https://thelastoutpostworkshop.github.io/ESPConnect/

1) Open the link in Chrome/Edge.
2) Connect Cardputer ADV with USB-C.
3) Click `Connect`, select the serial port, then choose firmware.
4) Start flash and wait until completion.

## Firmware Update (esptool.py)

1) Connect Cardputer ADV with USB-C.
2) Find your serial port:
   - macOS: `/dev/cu.usbmodem*` or `/dev/cu.SLAB_USBtoUART`
   - Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
   - Windows: `COMx`
3) Flash firmware:

```bash
esptool.py --chip esp32s3 --port <PORT> --baud 460800 write_flash 0x0 <firmware.bin>
```

4) Reboot the device after flashing.


## Thanks
If you like my work, you can support me here:

[![Buy Me a Coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=☕&slug=przemeks&button_colour=ff8800&font_colour=000000&font_family=Lato&outline_colour=000000&coffee_colour=FFDD00)](https://buymeacoffee.com/przemeks)

# Firmware contains UI changes and base on MeshCore:
https://github.com/meshcore-dev/MeshCore
