# PCB Design

This folder contains all PCB design files and documentation.

## Files

### Ir_Remote_PCB.stp
STEP 3D model of the PCB design
- Format: STEP (for CAD programs)
- Shows the 3D geometry of the board
- Useful for enclosure design

### Ir_Remote_Gerber.zip
Gerber files for PCB manufacturing
- Format: Gerber RS-274X
- Contains all layers (copper, masks, assembly)
- Ready to send to PCB manufacturer

### Ir_Remote_routing.pdf
Routing diagram of the board
- Shows wiring on the board
- Useful for checking signal integrity

## Assembly Notes

1. **SMD vs. THT:** Check the component list for SMD or through-hole components
2. **Thermal Management:** The transistor may get warm; consider a heat sink
3. **ESD Protection:** Implement ESD protection measures during assembly

## Next Steps

- PCB Manufacturer: JLCPCB
- Assembly: Manual
