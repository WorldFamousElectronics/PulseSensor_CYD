# PulseSensor on CYD (Cheap Yellow Display)

Turn a $15 display into a standalone heartbeat monitor — no coding required.

![PulseSensor CYD Demo](images/demo.gif)
<!-- TODO: Add actual demo image/gif -->

## ⚡ Flash Now

**[Click here to flash your CYD](https://worldfamouselectronics.github.io/PulseSensor_CYD/)** — works in Chrome, Edge, or Opera.

No Arduino IDE needed. Just plug in your CYD and click.

## What's a CYD?

The **Cheap Yellow Display** (ESP32-2432S028R) is a ~$15 development board with a built-in 2.8" color touchscreen. The name was coined by Irish maker [Brian Lough](https://www.youtube.com/@BrianLough), who built a thriving community around it. His [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) repo is the go-to resource for CYD projects.

## Hardware

You need:
- **CYD** — ESP32-2432S028R ([Amazon](https://www.amazon.com/s?k=ESP32-2432S028R) or AliExpress, ~$15)
- **PulseSensor** — [pulsesensor.com](https://pulsesensor.com/products/pulse-sensor-amped)
- **USB cable** — micro-USB, data-capable (not charge-only)

## Wiring

| PulseSensor Wire | CYD Connection |
|------------------|----------------|
| 🔴 **RED** (+V) | 3.3V on CN1 connector |
| ⚫ **BLACK** (GND) | GND on P3 or CN1 |
| 🟣 **PURPLE** (Signal) | GPIO 35 on P3 connector |

![Wiring Diagram](images/wiring.png)
<!-- TODO: Add actual wiring diagram -->

## What You'll See

Once flashed and wired, the display shows:

- **BPM** — Large heart rate reading
- **Waveform** — Smooth scrolling pulse wave
- **IBI** — Inter-beat interval (ms)
- **Heart indicator** — Flashes red with each beat

When no finger is detected for 3 seconds, the display resets.

## Building from Source

If you want to modify the code:

### Requirements
- Arduino IDE 2.x
- ESP32 board package (Arduino Core 3.x)
- Libraries:
  - [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
  - [PulseSensor Playground](https://github.com/WorldFamousElectronics/PulseSensorPlayground)

### TFT_eSPI Setup

The CYD requires specific TFT_eSPI configuration. Copy `User_Setup.h` from this repo to your TFT_eSPI library folder, or see [Brian Lough's guide](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/SETUP.md).

### Compile

1. Open `PulseSensor_CYD.ino` in Arduino IDE
2. Select board: **ESP32 Dev Module**
3. Upload speed: **115200** (if 921600 fails)
4. Upload

### Export .bin for WebSerial

To update the WebSerial flasher:

```
Arduino IDE → Sketch → Export Compiled Binary
```

Copy the `.bin` file to the `firmware/` folder.

## File Structure

```
PulseSensor_CYD/
├── PulseSensor_CYD.ino    # Main Arduino sketch
├── User_Setup.h           # TFT_eSPI config for CYD
├── index.html             # WebSerial flash page
├── manifest.json          # ESP Web Tools config
├── firmware/
│   └── PulseSensor_CYD.bin
├── images/
│   ├── demo.gif
│   └── wiring.png
└── README.md
```

## Customization

Colors and layout are defined at the top of the sketch:

```cpp
#define COLOR_BG          0x18C3    // Background
#define COLOR_WAVEFORM    0xE1E9    // Waveform line
#define COLOR_BPM         0x4F10    // BPM text
#define COLOR_HEART       0xEA29    // Heart indicator
```

## Troubleshooting

**No serial port in flasher?**  
Try a different USB cable — many are charge-only.

**Flat line on waveform?**  
Check that purple wire connects to GPIO 35.

**Erratic readings?**  
Apply gentle, steady pressure. Insulate the back of your PulseSensor with the velcro dot from your kit.

## License

MIT — hack away.

## Links

- [PulseSensor.com/cyd](https://pulsesensor.com/pages/cyd) — Tutorial page
- [PulseSensor Playground](https://github.com/WorldFamousElectronics/PulseSensorPlayground) — Arduino library
- [Brian Lough's CYD repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — Community hub

---

Made with ♥ by [World Famous Electronics](https://pulsesensor.com)
