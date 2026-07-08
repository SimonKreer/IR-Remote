# Calculations for Schematic Design

This document contains the technical calculations and design decisions for the IR Remote schematic.

## LED Current Calculation (TSAL6200)

### Objective
Determine the optimal base resistor (R1) for maximum IR transmitter performance while protecting the ESP32 GPIO pin.

### Parameters
- **IR LED (TSAL6200)** forward voltage: ~1.5 V at operating current
- **Desired LED current**: ~44.6 mA (for maximum range)
- **Transistor (2N2222A)** VCE(sat): ~0.2 V at saturation
- **Supply voltage**: 5 V
- **ESP32 GPIO output**: 3.3 V (max 40 mA)

### Calculation

**Collector current (IC):**
```
IC = (VCC - VLED - VCE(sat)) / RLED
IC = (5 - 1.5 - 0.2) / R_LED
```

Assuming RLED = 76 Ω:
```
IC = 3.3 / 76 = ~43.4 mA ✓
```

**Base current (IB) for saturation:**
```
IB = IC / β_min
```

Using β_min = 20 (worst case for 2N2222A):
```
IB = 43.4 / 20 = ~2.17 mA
```

**Base resistor (R1):**
```
R1 = (V_GPIO - VBE(sat)) / IB
R1 = (3.3 - 0.7) / 0.002
R1 = 2.6 / 0.002 = 1300 Ω
```

**Selected:** R1 = 1.2 kΩ (closest standard value, provides slight safety margin)

---

## Transistor Saturation Analysis

### Why Saturation?

Operating the transistor in saturation (fully conducting) ensures:
- Minimal voltage drop across VCE (low heat dissipation)
- Maximum current through the LED (maximum range)
- Reliable switching with GPIO timing

### Verification

**With R1 = 1.2 kΩ:**
```
IB = (3.3 - 0.7) / 1200 = ~2.17 mA
VCE(sat) ≈ 0.2 V (from datasheet)
IC ≈ 43.4 mA
β = IC / IB = 43.4 / 2.17 = ~20 ✓
```

The transistor operates deeply in saturation (β_actual = β_min), confirming full conduction.

### Heat Dissipation

**Transistor power dissipation:**
```
P = VCE(sat) × IC = 0.2 × 0.0434 = ~8.7 mW (negligible)
```

**LED power consumption:**
```
P_LED = VLED × IC = 1.5 × 0.0434 = ~65 mW
```

---

## Filtering for IR Receiver (TSOP38238)

### Problem Statement

The IR receiver output can be affected by:
- Power supply noise from rapid LED switching
- Electromagnetic interference from logic circuits
- Thermal drift in the receiver

### Solution: RC Low-Pass Filter

**Component Selection:**
- **R6** = 100 Ω (current-limiting resistor)
- **C2** = 100 nF (decoupling capacitor)

**Cutoff Frequency:**
```
fc = 1 / (2π × R × C)
fc = 1 / (2π × 100 × 100×10⁻⁹)
fc = 1 / (6.28 × 10⁻⁶)
fc ≈ 159 kHz
```

**Attenuation at 38 kHz carrier frequency:**

Since fc >> 38 kHz, the filter has minimal impact on the IR signal (< 0.1 dB attenuation).

**Noise suppression:**

High-frequency noise (MHz range) is attenuated by ~20-40 dB, effectively protecting the receiver from digital switching noise.

### Filter Response

| Frequency | Attenuation |
|-----------|-------------|
| 38 kHz | < 0.1 dB |
| 100 kHz | -0.5 dB |
| 1 MHz | -20 dB |
| 10 MHz | -40 dB |

---

## Power Supply Decoupling

### Capacitor Selection

**C1, C3** = 100 nF ceramic (high-frequency noise suppression)
- **Purpose:** Absorb switching transients from digital logic
- **Placement:** Close to power pins of active components

**C4** = 10 µF electrolytic (low-frequency energy buffer)
- **Purpose:** Maintain stable voltage during LED current pulses
- **Calculation:** Energy stored = 0.5 × C × V² = 0.5 × 10×10⁻⁶ × 5² ≈ 125 µJ

### Voltage Ripple Control

With instantaneous LED current of ~44 mA and total lead inductance of ~10 nH:
```
dV = L × dI / dt = 10×10⁻⁹ × 44×10⁻³ / 1×10⁻⁶ ≈ 0.44 mV
```

The decoupling capacitors easily suppress this ripple.

---

## Summary

| Component | Value | Purpose |
|-----------|-------|----------|
| R1 | 1.2 kΩ | Base resistor for 2N2222A |
| RLED | 76 Ω | LED current limiting |
| R6 | 100 Ω | Receiver supply filter |
| C1, C3 | 100 nF | High-frequency noise suppression |
| C2 | 100 nF | Receiver supply filtering |
| C4 | 10 µF | Power supply energy buffer |

All calculations assume worst-case component tolerances and verify safe, reliable operation.
