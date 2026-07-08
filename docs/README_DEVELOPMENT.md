# Developer Guide

## Setup with PlatformIO

### Requirements
- **VS Code** or compatible IDE
- **PlatformIO Extension** for VS Code
- **Python 3.6+** (required by PlatformIO)
- **ESP32 USB Drivers** (CH340 or CP2102, depending on board version)

### Installation

1. **Install VS Code Extension**
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X)
   - Search for "PlatformIO"
   - Click "Install"

2. **Open Project**
   ```bash
   git clone https://github.com/SimonKreer/IR-Remote.git
   cd IR-Remote
   # Open the folder in VS Code
   ```

3. **Select Board** (if not ESP32 DevKit)
   - Open `platformio.ini`
   - Change `board = esp32doit-devkit-v1` to your board
   - Save

### Build and Upload

**Option 1: Via UI**
- Click "PlatformIO" icon (ant) in the left sidebar
- Select "Build" to compile
- Select "Upload" to transfer to board

**Option 2: Via Terminal**
```bash
# Compile
pio run

# Compile and upload
pio run -t upload

# Open serial monitor
pio device monitor
```

### File Structure

```
src/
  ├── main.cpp              # Main program
  ├── config.h              # Configuration
  ├── ir_receiver.h         # IR Receiver header
  └── ir_sender.h           # IR Sender header

include/
  └── (external libraries)

lib/
  └── (local libraries)

platformio.ini             # PlatformIO configuration
```

### GPIO Pin Assignment

Verify your wiring against these pins (configurable in `src/config.h`):

| Component | ESP32 Pin | Description |
|-----------|-----------|----------|
| IR Receiver (TSOP38238) | GPIO 35 | Input |
| IR Transmitter (PWM) | GPIO 33 | Output |
| GND | GND | Ground |
| 5V | 5V | Power supply |

### Troubleshooting

**Error: "Board not found"**
- Check USB connection
- Install correct USB driver for your board
- Restart VS Code

**Compilation errors**
- Check C++ syntax
- Ensure all includes are correct
- Read error messages carefully

**Upload fails**
- Try different USB port
- Restart ESP32 board (disconnect briefly)
- Check baud rate in `platformio.ini`

### Next Steps

1. Implement `ir_receiver.cpp` functions
2. Implement `ir_sender.cpp` functions
3. Test with your remote control
4. Extend with additional features (e.g., storage, web interface)

### Further Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Arduino Reference](https://www.arduino.cc/reference/en/)
