# Project Introduction - Universal Infrared Remote Control

This document provides a comprehensive introduction to the IR Remote project.

## Overview

The IR Remote is a universal infrared remote control based on the ESP32 microcontroller. It combines hardware components for signal reception and transmission with firmware that enables:

- **Learning Mode**: Capture IR signals from existing remotes
- **Transmission Mode**: Store and reproduce captured signals
- **Multi-Device Support**: Manage profiles for different devices
- **User-Friendly Interface**: OLED display-based menu system

## Technical Architecture

### Core Components

**Microcontroller**: Heltec ESP32 with integrated OLED display  
**IR Receiver**: TSOP38238 demodulating receiver  
**IR Transmitter**: TSAL6200 IR LED with 2N2222A transistor amplifier  
**User Input**: 4 tactile buttons for navigation  

### Signal Processing

The system supports two complementary methods:

1. **Protocol-Based Analysis**: Efficiently decodes known IR protocols (NEC, Sony, RC5, etc.)
2. **RAW-Capture**: Records raw timing data for unknown or proprietary protocols

## Project Structure

```
IR-Remote/
├── README.md                    # Project overview
├── docs/                        # Documentation
│   ├── README.md               # Documentation index
│   ├── README_DEVELOPMENT.md   # Developer setup guide
│   ├── CHANGELOG.md            # Version history
│   └── Technical_Foundations_Universal_IR_Remote.md
├── hardware/                   # Hardware designs
│   ├── schematic/              # Circuit diagrams and datasheets
│   ├── pcb/                    # PCB design files and Gerber output
│   └── Component_List.xlsx     # Bill of materials
└── software/                   # Firmware (PlatformIO project)
    ├── src/
    ├── include/
    └── lib/
```

## Getting Started

### Prerequisites
- VS Code or compatible IDE
- PlatformIO extension
- Python 3.6+
- ESP32 USB drivers (CH340 or CP2102)

### Quick Setup

1. Clone the repository
2. Open in VS Code
3. Install PlatformIO extension
4. Connect your ESP32 board
5. Click "Build" and "Upload" in PlatformIO

For detailed instructions, see [README_DEVELOPMENT.md](./README_DEVELOPMENT.md).

## Features & Capabilities

### Receiving IR Signals
- Auto-detection of protocol type
- Raw timing capture as fallback
- Storage in internal memory

### Transmitting IR Signals
- Precise timing reproduction
- Variable transmission power
- Support for multiple device profiles

### User Interface
- 128x64 OLED display
- 4-button navigation
- Menu-driven device selection
- Signal learning and transmission controls

## Technical Resources

Detailed technical information is available in:
- [Technical Foundations](./Technical_Foundations_Universal_IR_Remote.md) - Electronics and signal theory
- [Hardware Documentation](../hardware/) - Circuit designs and component specifications
- [Developer Guide](./README_DEVELOPMENT.md) - Firmware development setup
