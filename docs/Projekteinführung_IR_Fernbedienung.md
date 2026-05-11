
**Problemstellung:** Zu viele einzelne Fernbedienungen im Haushalt. 
**Lösung:** Eine lernfähige ESP32-basierte IR-Fernbedienung.

**1. Zieldefinition**

Was soll das Gerät leisten?

1. **Lernfunktion:** IR-Signale von bestehenden Fernbedienungen auslesen.
2. **Sendefunktion:** Gespeicherte IR-Signale präzise wiedergeben.
3. **Multigeräte-Support:** Profile für mehrere Geräte speichern.
4. **Benutzerschnittstelle:** Integriertes 0,96" OLED-Display (Heltec ESP32) mit Menüführung.
5. **Portabilität:** Akkubetrieb für den mobilen Einsatz.
6. **Maximale Kompatibilität:** Ziel ist es, nahezu alle auf dem Markt existierenden IR-Protokolle lesen und emulieren zu können.

---

**2. Hardware-Komponenten**

- **Mikrocontroller:** Heltec ESP32 (inkl. 0,96" OLED-Display und JST-Akku Anschlussmöglichkeit).
- **IR-Empfänger:** TSOP38238.
- **IR-Sender:** TSAL6200 IR-LED mit einem **2N2222A-Transistor** zur Signalverstärkung.
- **Bedienelemente:** 4 Taster für die Navigation und Auswahl (die Taster übernehmen je nach Menüpunkt Steuerungs- oder Sendefunktionen).
- **Energieversorgung:** 3,7V LiPo-Akku (1500 mAh) über JST-Stecker am Heltec-Board.
- **Gehäuse:** 3D gedrucktes Gehäuse.
- **Pin-Belegung (GPIOs):**

- **GPIO 17:** IR-Empfänger
- **GPIO 18:** IR-Sender
- **GPIO 32, 33, 27, 14:** Taster-Eingänge

---

**3. Software-Logik & Features**

**UI-Konzept**

- **Fokus:** Hohe Zuverlässigkeit und einfache Bedienung.
- **Struktur:** Das Hauptmenü bietet Platz für **5 Geräteprofile** (Gerät 1 bis 5).
- **Funktionsweise:** Nach Auswahl eines Geräts kann der Nutzer zwischen dem „Sniffen“ (Empfangen) und „Senden“ wählen. Die Taster dienen im Untermenü sowohl der Navigation als auch dem Auslösen der gespeicherten Befehle.

**Menü-Unterteilung:**

- **Ebene 1:** Geräteliste (1-5).
- **Ebene 2:** Aktion wählen (Lernen / Senden / Löschen).
- **Ebene 3 (Lernen):** Auswahl des Speicherplatzes (z.B. Power, Vol+, Vol-).
- **Ebene 3 (Senden):** Direkte Belegung der 4 physischen Taster mit den wichtigsten Funktionen des gewählten Geräts.

**Analyse-Methoden**

Um eine maximale Kompatibilität zu erreichen, werden zwei Methoden kombiniert:

|**Methode**|**Funktionsweise**|**Vorteile**|**Nachteile**|
|---|---|---|---|
|**1. Protokoll-basiert**|Dekodierung von Protokoll, Adresse und Befehl (Command).|Sehr speichereffizient und präzise.|Funktioniert nur bei bekannten Protokollen (NEC, Sony, etc.).|
|**2. RAW-Replay**|Speicherung des exakten Timing-Arrays (Puls/Pause-Dauern).|Funktioniert auch bei exotischen oder unbekannten Geräten.|Speicherintensiv und kritisch bei Timing-Abweichungen.|


**Anpassung für den Seriellen Monitor:** Die Software soll so modifiziert werden, dass bei jedem Lesevorgang **beide Datensätze** (Protokoll-Daten UND RAW-Array) gleichzeitig im Seriellen Monitor ausgegeben und für die spätere Verwendung gespeichert werden.