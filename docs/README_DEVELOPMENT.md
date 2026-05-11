# Entwickler-Anleitung

## Setup mit PlatformIO

### Anforderungen
- **VS Code** oder kompatible IDE
- **PlatformIO Extension** für VS Code
- **Python 3.6+** (wird von PlatformIO benötigt)
- **ESP32 USB-Treiber** (CH340 oder CP2102, je nach Board-Version)

### Installation

1. **VS Code Extension installieren**
   - Öffne VS Code
   - Gehe zu Extensions (Ctrl+Shift+X)
   - Suche nach "PlatformIO"
   - Klicke auf "Install"

2. **Projekt öffnen**
   ```bash
   git clone https://github.com/leckmichamarsch1/Infrarot_Fernbedienung.git
   cd Infrarot_Fernbedienung
   # Öffne den Ordner in VS Code
   ```

3. **Board auswählen** (falls nicht ESP32 DevKit)
   - Öffne `platformio.ini`
   - Änere `board = esp32doit-devkit-v1` zu deinem Board
   - Speichern

### Kompilieren und Upload

**Variante 1: Über UI**
- Klick auf "PlatformIO" Icon (Ameise) in der linken Sidebar
- Wähle "Build" zum Kompilieren
- Wähle "Upload" zum auf Board übertragen

**Variante 2: Terminal**
```bash
# Kompilieren
pio run

# Kompilieren und hochladen
pio run -t upload

# Serial Monitor öffnen
pio device monitor
```

### Dateistruktur

```
src/
  ├── main.cpp              # Hauptprogramm
  ├── config.h              # Konfigurationen
  ├── ir_receiver.h         # IR-Empfänger Header
  └── ir_sender.h           # IR-Sender Header

include/
  └── (externe Libraries)

lib/
  └── (lokale Libraries)

platformio.ini             # PlatformIO Konfiguration
```

### GPIO-Pinbelegung

Überprüfe deine Verdrahtung gegen diese Pins (angepassbar in `src/config.h`):

| Komponente | ESP32 Pin | Beschreibung |
|-----------|-----------|------------|
| IR-Empfänger (TSOP38238) | GPIO 35 | Input |
| IR-Sender (PWM) | GPIO 33 | Output |
| GND | GND | Ground |
| 5V | 5V | Stromversorgung |

### Häufige Probleme

**Fehler: "Board not found"**
- Überprüfe die USB-Verbindung
- Installiere den richtigen USB-Treiber für dein Board
- Starte VS Code neu

**Kompilierungsfehler**
- Überprüfe die C++ Syntax
- Stelle sicher, dass alle Includes korrekt sind
- Lies die Fehlermeldung genau durch

**Upload schlägt fehl**
- Versuche einen anderen USB-Port
- Starte das ESP32 Board neu (kurz trennen)
- Überprüfe die Baudrate in `platformio.ini`

### Nächste Schritte

1. Implementiere die `ir_receiver.cpp` Funktionen
2. Implementiere die `ir_sender.cpp` Funktionen
3. Teste mit deiner Fernbedienung
4. Erweitere um weitere Features (z.B. Speicherung, Webinterface)

### Weitere Ressourcen

- [PlatformIO Dokumentation](https://docs.platformio.org/)
- [ESP32 Dokumentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Arduino Referenz](https://www.arduino.cc/reference/en/)
