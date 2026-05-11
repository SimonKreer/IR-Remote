# Infrarot Fernbedienung - Dokumentation

Fernbedienung basierend auf einem ESP32, die Fernbedienungen kopieren und deren Signale wieder senden kann.

## 📋 Inhaltsverzeichnis

- [Projektübersicht](#projektübersicht)
- [Hardware](#hardware)
- [Software](#software)
- [Installation](#installation)
- [Dokumentation](#dokumentation)
- [Lizenz](#lizenz)

## Projektübersicht

Dieses Projekt implementiert eine **universelle, lernfähige IR-Fernbedienung** basierend auf dem ESP32 Mikrocontroller. Das Gerät kann IR-Signale von bestehenden Fernbedienungen aufzeichnen und speichern, um diese später mit maximaler Kompatibilität erneut zu senden.

### Hauptmerkmale

✅ **Lernfunktion** - IR-Signale von bestehenden Fernbedienungen auslesen  
✅ **Sendefunktion** - Gespeicherte IR-Signale präzise wiedergeben  
✅ **Multigeräte-Support** - Profile für mehrere Geräte speichern  
✅ **Benutzerfreundlich** - Integriertes OLED-Display mit Menüführung  
✅ **Tragbar** - Akkubetrieb für mobilen Einsatz  
✅ **Kompatibilität** - Unterstützt bekannte und unbekannte IR-Protokolle  

## Hardware

**Mikrocontroller:**
- Heltec ESP32 (mit 0,96" OLED-Display und JST-Akku-Anschluss)

**IR-Komponenten:**
- IR-Empfänger: TSOP38238
- IR-Sender: TSAL6200 IR-LED + 2N2222A Transistor zur Signalverstärkung

**Bedienung & Energieversorgung:**
- 4 Taster für Navigation und Auswahl
- 3,7V LiPo-Akku (1500 mAh)

**GPIO-Pinbelegung:**
- GPIO 17: IR-Empfänger
- GPIO 18: IR-Sender
- GPIO 32, 33, 27, 14: Taster-Eingänge

Detaillierte Hardware-Dokumentation findest du im Ordner [`hardware/`](../hardware/).

## Software

Das Projekt nutzt **PlatformIO** für die Firmware-Entwicklung 

### Analysemethoden

Für maximale Kompatibilität werden zwei Analysemethoden kombiniert:

1. **Protokoll-basiert** - Dekodierung von Protokoll, Adresse und Befehl
   - Speichereffizient und präzise
   - Funktioniert nur bei bekannten Protokollen (NEC, Sony, etc.)

2. **RAW-Replay** - Speichern des exakten Timing-Arrays
   - Funktioniert bei exotischen/unbekannten Geräten
   - Speicherintensiv und kritisch bei Timing-Abweichungen

## Installation

Siehe die detaillierte Anleitung in [`docs/README_DEVELOPMENT.md`](./README_DEVELOPMENT.md).

### Schnelstart

```bash
# Repository klonen
git clone https://github.com/leckmichamarsch1/Infrarot_Fernbedienung.git
cd Infrarot_Fernbedienung

# In VS Code mit PlatformIO öffnen
code .

# Firmware kompilieren und hochladen
pio run -t upload
```

## Dokumentation

📚 **Ausführliche Dokumentation:**

- [`Projekteinführung_IR_Fernbedienung.md`](.docs/Projekteinführung_IR_Fernbedienung) - Detaillierte Projektbeschreibung
- [`README_DEVELOPMENT.md`](./README_DEVELOPMENT.md) - Entwickler-Anleitung
- [`Theoretische Grundlagen Universelle IR-Fernbedienung`](./docs/Theoretische Grundlagen Universelle IR-Fernbedienung) - Technische Grundlagen

## Lizenz

Dieses Projekt ist unter der **MIT License** lizenziert.
Siehe [`LICENSE`](../docs/LICENSE) für Details.

---

**Autor:** leckmichamarsch1  
**Status:** 🚧 In Entwicklung  
**Letzte Aktualisierung:** 2026-05-09
