
### 1. IR-Sender: LED-Strom am TSAL6200

Um den Strom durch die Sendediode zu berechnen, nutzen wir die Maschengleichung im Kollektorzweig des Transistors.

**Relevante Datenblatt-Angaben (TSAL6200):**

- **Forward Voltage ($V_F$):** Typ. **1,35 V** (bei $I_F = 100\text{ mA}$).
    
- **Limit: Forward Current ($I_F$):** Max. **100 mA** (kontinuierlicher Betrieb).
    

**Zusatzangabe aus dem Transistor-Datenblatt (2N2222A):**

- **Collector-Emitter Saturation Voltage ($V_{CE_{sat}}$):** Max. **0,3 V**.
    

**Rechnung:**

$$I_{LED} = \frac{V_{CC} - V_F - V_{CE_{sat}}}{R2}$$

$$I_{LED} = \frac{3,3\text{ V} - 1,35\text{ V} - 0,3\text{ V}}{37\text{ }\Omega} = \frac{1,65\text{ V}}{37\text{ }\Omega} \approx \mathbf{44,6\text{ mA}}$$

**Beleg:** Der berechnete Strom von **44,6 mA** liegt sicher unter dem Limit von **100 mA** laut Datenblatt.

---

### 2. Transistor-Sättigung: Basiswiderstand R1 am 2N2222A

Prüfung, ob der Strom vom ESP32-GPIO ausreicht, um den Transistor voll durchzuschalten (Sättigung).

**Relevante Datenblatt-Angaben (2N2222A):**

- **DC Current Gain ($h_{FE}$):** Min. **100** (bei $I_C = 150\text{ mA}$, für Sättigungsberechnungen konservativ gewählt).
    
- **Base-Emitter Saturation Voltage ($V_{BE_{sat}}$):** Typ. **0,6 V bis 1,2 V** (wir rechnen mit **0,7 V** für Silizium).
    

**Rechnung tatsächlicher Basisstrom ($I_B$):**

$$I_B = \frac{V_{GPIO} - V_{BE}}{R1} = \frac{3,3\text{ V} - 0,7\text{ V}}{470\text{ }\Omega} \approx \mathbf{5,5\text{ mA}}$$

**Rechnung erforderlicher Mindest-Basisstrom ($I_{B_{min}}$):**

$$I_{B_{min}} = \frac{I_{LED}}{h_{FE}} = \frac{44,6\text{ mA}}{100} = \mathbf{0,446\text{ mA}}$$

**Beleg:** Da der tatsächliche Strom (**5,5 mA**) etwa das **12-fache** des benötigten Mindeststroms (**0,446 mA**) beträgt, ist der Transistor sicher in der Sättigung.

---

### 3. IR-Empfänger: Versorgungsfilterung am TSOP38238

Der Widerstand R6 dient als Teil eines RC-Filters, um Störungen der Versorgungsspannung vom Empfänger fernzuhalten.

**Relevante Datenblatt-Angaben (TSOP38238):**

- **Supply Voltage ($V_S$):** Bereich **2,5 V bis 5,5 V**.
    
- **Supply Current ($I_S$):** Typ. **0,35 mA** (Max. 0,45 mA).
    
- **Application Circuit:** Das Datenblatt empfiehlt explizit ein RC-Filter mit **$R_1 = 100\text{ }\Omega$** und **$C_1 > 0,1\text{ µF}$**.
    

**Rechnung Spannungsabfall an R6:**

$$\Delta V = I_S \times R6 = 0,45\text{ mA} \times 100\text{ }\Omega = \mathbf{0,045\text{ V}}$$

**Rechnung verbleibende Betriebsspannung ($V_{VS}$):**

$$V_{VS} = V_{CC} - \Delta V = 3,3\text{ V} - 0,045\text{ V} = \mathbf{3,255\text{ V}}$$

**Beleg:** Die Spannung von **3,255 V** liegt ideal innerhalb des vom Datenblatt geforderten Bereichs von **2,5 V bis 5,5 V**.

---

### 4. Zusammenfassung der Kondensatoren

Design-Richtlinien der Hersteller:

- **TSOP-Filter (C2, C3):** Das Datenblatt fordert min. 100 nF. Deine Wahl von **100 nF (Keramik)** für schnelle Impulse und zusätzlich **10 µF (Elko)** für die Glättung übertrifft die Mindestanforderungen und sorgt für ein sehr stabiles Signal.
    
- **Versorgung IR-Sender (C1, C4):** Da die IR-LED mit über 40 mA gepulst wird, sind die **10 µF** als lokaler Energiespeicher wichtig, damit die Spannung des ESP32 bei Sendevorgängen nicht kurzzeitig einbricht.