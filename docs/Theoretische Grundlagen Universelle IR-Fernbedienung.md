

## 1. Das Gehirn: Heltec ESP32 (U1)

Der **Heltec WiFi LoRa 32 (V2/V3)** fungiert als zentrale Recheneinheit. Er basiert auf dem leistungsstarken ESP32-SoC (System-on-a-Chip).

·       **Rechenleistung:** Mit einem Dual-Core-Prozessor und hohen Taktfrequenzen ermöglicht er die präzise Erfassung und Erzeugung von Mikrosekunden-Impulsen, die für IR-Protokolle essenziell sind.

·       **Peripherie:** Das integrierte **0,96" OLED-Display** wird über den **I2C-Bus** angesteuert. Dies erlaubt eine benutzerfreundliche Menüführung zur Auswahl von Geräteprofilen.

·       **Konnektivität:** Durch WiFi und Bluetooth bietet das Board Potenzial für zukünftige Smart-Home-Erweiterungen.

---

## 2. Physikalische Basis: Infrarot-Kommunikation

Die Datenübertragung erfolgt drahtlos mittels Lichtwellen im Infrarotbereich (ca. 940 nm), was knapp oberhalb des für Menschen sichtbaren Spektrums liegt.

·       **Modulation:** Um Interferenzen durch Sonnenlicht oder Raumbeleuchtung zu vermeiden, werden die Signale mit einer **Trägerfrequenz** (meist **38 kHz**) moduliert. Eine "logische Eins" wird dabei als Paket von hochfrequenten Lichtblitzen gesendet.

·       **Protokolle:** Die Software unterscheidet zwischen **protokollbasierten Daten** (z. B. NEC oder Sony Standard) und **RAW-Daten**, bei denen lediglich die Zeitabstände zwischen den Lichtimpulsen aufgezeichnet werden.

---

## 3. Die Sendestufe (Infrarot-Emitter)

Die Sendestufe hat die Aufgabe, elektrische Signale des Mikrocontrollers in kräftige Lichtimpulse umzuwandeln.

### IR-Sendediode (IR1 - TSAL6200)

Diese Hochleistungs-LED ist auf maximale Abstrahlung im 940-nm-Bereich optimiert. Da die Reichweite direkt vom fließenden Strom abhängt, wird die Diode nahe an ihrer Spezifikation betrieben.

### Schalttransistor (Q1 - 2N2222A)

Da die GPIO-Pins des ESP32 nicht genügend Strom (benötigt werden ca. 45–100 mA) für eine hohe Reichweite liefern können, fungiert der **2N2222A NPN-Transistor** als elektronischer Schalter.

·       **Funktionsweise:** Ein minimaler Strom am Basis-Pin (gesteuert durch den ESP32) schaltet die Kollektor-Emitter-Strecke durch, wodurch der Hauptstromkreis der LED geschlossen wird.

·       **Basiswiderstand (R1):** Er schützt den Mikrocontroller vor Überlastung und stellt sicher, dass der Transistor im Sättigungsbereich arbeitet (vollständig leitet).

---

## 4. Die Empfangsstufe (Infrarot-Sensor)

Der **TSOP38238 (LED1)** ist ein spezialisierter Empfänger für modulierte Infrarotsignale.

·       **Interne Signalverarbeitung:** Das Bauteil enthält eine Fotodiode, einen Verstärker und einen Bandpassfilter. Letzterer filtert alle Signale heraus, die nicht der Trägerfrequenz von 38 kHz entsprechen.

·       **Demodulation:** Am Ausgang des Sensors liegt ein bereinigtes digitales Signal an, das direkt vom ESP32 eingelesen werden kann.

·       **Versorgungsfilter (R6 & C2):** Um Störungen durch die restliche Elektronik zu minimieren, wird der Empfänger über ein **RC-Glied** (Tiefpassfilter) stabilisiert. Dies verhindert Fehltriggerungen durch Spannungsspitzen.

---

## 5. Spannungsstabilisierung und Entkopplung

In einer Schaltung, in der Komponenten wie IR-LEDs hohe Ströme pulsartig ziehen, ist die Stabilisierung der Versorgungsspannung kritisch.

·       **Kondensatoren (C1 - C4):**

o   **Keramikkondensatoren (100 nF):** Diese unterdrücken hochfrequentes Rauschen, das durch das schnelle Schalten der digitalen Logik entsteht.

o   **Elektrolyt- oder größere Keramikkondensatoren (10 µF):** Diese dienen als lokale Energiespeicher (Puffer). Sie verhindern, dass die Systemspannung während eines Sendevorgangs einbricht, was zu einem ungewollten Neustart des Mikrocontrollers führen könnte.

---

## 6. Benutzerschnittstelle (HMI)

Die Interaktion erfolgt über vier mechanische **SMD-Taster (U2–U5)**.

·       **Interne Pull-Ups/Pull-Downs:** Der ESP32 besitzt für fast alle GPIOs integrierte Widerstände (typischerweise im Bereich von **30 kΩ bis 80 kΩ**).

·       **Menü-Logik:** Die Software übersetzt die Tastendrucke in Navigationsbefehle auf dem OLED-Display, wodurch der Nutzer zwischen den Funktionen „Lernen“ (Sniffing) und „Senden“ sowie verschiedenen Geräte-Slots wählen kann.