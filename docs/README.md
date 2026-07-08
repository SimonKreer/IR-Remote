# Infrared Remote Control - Documentation

Infrared remote control based on an ESP32 that can copy remote controls and retransmit their signals.

## Table of Contents

- [Project Overview](#project-overview)
- [Hardware](#hardware)
- [Software](#software)
- [Installation](#installation)
- [Documentation](#documentation)
- [License](#license)

## Project Overview

This project implements a **universal, learning IR remote control** based on the ESP32 microcontroller. The device can record IR signals from existing remote controls and retransmit them with precision.

### Key Features

- **Learning Function** - Read IR signals from existing remote controls  
- **Transmission Function** - Reproduce stored IR signals precisely  
- **Multi-Device Support** - Save profiles for multiple devices  
- **User-Friendly** - Integrated OLED display with menu navigation  
- **Portable** - Battery-powered for mobile use  
- **Compatibility** - Supports known and unknown IR protocols  

## Hardware

**Microcontroller:**
- Heltec ESP32 (with 0.96" OLED display and JST battery connector)

**IR Components:**
- IR Receiver: TSOP38238
- IR Transmitter: TSAL6200 IR LED + 2N2222A transistor for signal amplification

**User Interface:**
- 4 buttons for navigation and selection


**GPIO Pin Assignment:**
- GPIO 17: IR Receiver
- GPIO 18: IR Transmitter
- GPIO 32, 33, 27, 14: Button inputs

Detailed hardware documentation can be found in the [`hardware/`](../hardware/) folder.

## Software

The project uses **PlatformIO** for firmware development.

### Analysis Methods

For maximum compatibility, two analysis methods are combined:

1. **Protocol-Based** - Decoding of protocol, address, and command
   - Memory-efficient and precise
   - Works only with known protocols (NEC, Sony, etc.)

2. **RAW-Replay** - Storage of exact timing array
   - Works with exotic/unknown devices
   - Memory-intensive and critical with timing deviations

## Installation

See the detailed instructions in [`docs/README_DEVELOPMENT.md`](./README_DEVELOPMENT.md).

### Quick Start

```bash
# Clone repository
git clone https://github.com/SimonKreer/IR-Remote.git
cd IR-Remote

# Open in VS Code with PlatformIO
code .

# Compile and upload firmware
pio run -t upload
```

## Documentation

**Comprehensive Documentation:**

- [`Project_Introduction_IR_Remote.md`](./Project_Introduction_IR_Remote.md) - Detailed project description
- [`README_DEVELOPMENT.md`](./README_DEVELOPMENT.md) - Developer guide
- [`Technical_Foundations_Universal_IR_Remote.md`](./Technical_Foundations_Universal_IR_Remote.md) - Technical fundamentals

## License

This project is licensed under the **MIT License**.
See [`LICENSE`](../docs/LICENSE) for details.
