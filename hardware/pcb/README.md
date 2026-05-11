# PCB Design

Dieser Ordner enthält alle PCB-Design-Dateien und -Dokumentation.

## Dateien

### Ir_Fernbedienung_PCB.stp
D3D STEP-Modell des PCB-Designs
- Format: STEP (für CAD-Programme)
- Zeigt die 3D-Geometrie der Platine
- Nützlich für Gehäuse-Design

### Ir_Fernbedienung_Gerber.zip
Gerber-Dateien für die PCB-Fertigung
- Format: Gerber RS-274X
- Enthält alle Schichten (Kupfer, Masken, Bestückung)
- Ready to send zum PCB-Hersteller

### Ir_Fernbedienung_routing.pdf
Routing-Diagramm der Platine
- Zeigt die Verdrahtung auf der Platine
- Nützlich zur Überprüfung der Signal-Integrität

## Bestückungshinweise

1. **SMD vs. THT:** Überprüfe die Komponentenliste für SMD- oder Durchsteck-Komponenten
2. **Wärmemanagement:** Der Transistor kann warm werden; ggf. Kühlkörper überlegen
3. **ESD-Schutz:** Bei der Bestückung ESD-Schutzmaßnahmen ergreifen

## Weitere Schritte

- PCB-Hersteller: JLCPCB
- Bestückung: Von Hand 

