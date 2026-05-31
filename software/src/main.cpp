// =============================================================================
// Sehr geehrter Herr Backes, ich habe versucht, diesen Code so ausführlich und 
// einfach zu kommentieren, dass ihn auch eine Person ohne jegliche 
// Programmierkenntnisse problemlos verstehen kann. Alle Fachbegriffe habe ich 
// durch Alltagsbeispiele ersetzt.
// =============================================================================

// =============================================================================
// main.cpp – ESP32 IR-Universalfernbedienung
// Hardware: Ein kleiner Minicomputer (ESP32) mit eingebautem Bildschirm (OLED)
// Sinn des Programms: Eine lernfähige Universalfernbedienung für den Fernseher.
// =============================================================================

// Werkzeugkästen (Bibliotheken) laden:
// Hier holen wir uns fertige Bausteine, damit der Minicomputer weiß, wie man
// mit dem Bildschirm, den Infrarot-Lämpchen und den Knöpfen redet.
#include <Arduino.h>     // Basis-Funktionen für den Minicomputer
#include <IRrecv.h>      // Werkzeug zum Empfangen von Infrarot-Signalen (Auge)
#include <IRsend.h>      // Werkzeug zum Senden von Infrarot-Signalen (Taschenlampe)
#include <IRutils.h>     // Helfer für Infrarot-Daten
#include <Preferences.h> // Werkzeug, um Daten dauerhaft zu merken (wie eine Festplatte)
#include <U8g2lib.h>     // Werkzeug, um Text auf dem Bildschirm anzuzeigen
#include <Wire.h>        // Werkzeug für die Datenkabel zum Bildschirm

// =============================================================================
// STECKPLÄTZE (PINS) & FESTE WERTE
// Hier sagen wir dem Computer, welche koponenten an welchen pins angeschlossen sind und wie viel Platz wir für die Daten reservieren. 
// =============================================================================

#define PIN_IR_RECV     17  // Das Infrarot-Auge steckt an Stecker 17
#define PIN_IR_SEND     18  // Das Infrarot-Sende-Lämpchen steckt an Stecker 18

// Die vier Bedienknöpfe: Hoch, Runter, Zurück, Bestätigen
#define PIN_BTN_UP      32
#define PIN_BTN_DOWN    33
#define PIN_BTN_BACK    27
#define PIN_BTN_SELECT  14

// Die Kabel für den kleinen Bildschirm
#define PIN_OLED_SDA     4
#define PIN_OLED_SCL    15
#define PIN_OLED_RST    16

// Grenzen für das Gedächtnis der Fernbedienung
#define NUM_DEVICES      5  // Es können maximal 5 verschiedene Geräte (z.B. TV, DVD) gespeichert werden
#define NUM_CMDS         4  // Jedes Gerät hat genau 4 Knöpfe (Power, Lauter, Leiser, OK)
#define IR_BUF_SIZE    300  // Wie viel Platz wir für ein Signal reservieren
#define MAX_RAW_LEN    300  // Maximale Länge eines ungefilterten Signals
#define IR_TIMEOUT_MS 15000 // Beim Lernen wartet die Fernbedienung max. 15 Sekunden auf ein Signal
#define DEBOUNCE_MS     20  // Wartezeit, um versehentliches "Doppelklicken" bei Knöpfen zu verhindern

// =============================================================================
// FILTER-PARAMETER (Der Signal-Staubsauger)
// Infrarot-Signale aus der Luft sind oft "verrauscht" (wie ein schlechtes Radiosignal).
// Diese mathematischen Werte helfen dem Computer, das Signal glattzubügeln.
// =============================================================================
static const float   CLUSTER_RATIO_THRESH = 1.20f; // Ab welcher Abweichung ist es ein neues Signalteil?
static const float   GRID_SNAP_TOL        = 0.15f; // Wie stark darf das Signal vom perfekten Raster abweichen?
static const uint8_t MAX_CLUSTERS         = 24;    // In wie viele Schubladen sortieren wir das Signal maximal?

static uint8_t g_lastClusterCount = 0; // Merkzettel: Wie viele Schubladen wurden zuletzt benutzt?

// =============================================================================
// DATEN-SCHUBLADEN (Strukturen)
// Hier bauen wir uns Schablonen, um Daten ordentlich zu sortieren.
// =============================================================================

// Schablone für einen einzelnen Fernbedienungs-Knopf
struct IRCommand {
    char          name[16];            // Name des Knopfes (z.B. "Power")
    bool          isEmpty;             // Ja/Nein: Ist dieser Knopf noch leer?
    bool          hasProtocol;         // Ja/Nein: Spricht der Fernseher eine bekannte Sprache (z.B. Sony-Sprache)?
    decode_type_t protocol;            // Der Name dieser bekannten Sprache
    uint64_t      value;               // Der geheime Zahlen-Code für diesen Knopf
    uint16_t      bits;                // Wie lang ist der Code (Anzahl der Nullen und Einsen)
    bool          hasRaw;              // Ja/Nein: Haben wir die Rohdaten (falls die Sprache unbekannt ist)?
    uint16_t      rawData[MAX_RAW_LEN];// Das genaue Blink-Muster (wie ein Morsecode in Mikrosekunden)
    uint16_t      rawLen;              // Wie lang ist dieses Blink-Muster
    uint16_t      frequency;           // Wie schnell flackert das Infrarotlicht (meistens 38 kHz)
};

// Schablone für ein ganzes Gerät (z.B. "Wohnzimmer TV")
struct DeviceProfile {
    char      name[16];          // Name des Geräts
    bool      isUsed;            // Ja/Nein: Ist dieser Speicherplatz belegt?
    IRCommand commands[NUM_CMDS];// Die 4 Knöpfe, die dieses Gerät steuern kann
};

// Der Menü-Kompass: Wo befindet sich der Nutzer gerade auf dem Bildschirm?
enum MenuLevel : uint8_t {
    MENU_MAIN,           // Hauptmenü (Gerät auswählen)
    MENU_ACTION,         // Aktion wählen (Lernen, Senden oder Löschen)
    MENU_LEARN_SELECT,   // Welcher Knopf soll gelernt werden?
    MENU_LEARN_WAIT,     // "Bitte echten Fernbedienung drücken"-Bildschirm
    MENU_SEND,           // Signal wird gerade gesendet
    MENU_DELETE_CONFIRM  // Sicherheitsabfrage: "Wirklich löschen?"
};

// Welcher Knopf wurde gedrückt?
enum ButtonEvent : uint8_t { BTN_NONE, BTN_UP, BTN_DOWN, BTN_BACK, BTN_SELECT };

// =============================================================================
// GLOBALE OBJEKTE (Die Arbeiter)
// Hier werden die virtuellen Arbeiter erstellt, die das Programm steuern.
// =============================================================================

// Der Arbeiter für den Bildschirm (wir sagen ihm, wo die Kabel stecken)
static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
    U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA, PIN_OLED_RST);

static IRrecv* irRecv = nullptr; // Das Infrarot-Auge (wird später gestartet)
static IRsend* irSend = nullptr; // Das Infrarot-Sende-Lämpchen (wird später gestartet)
static Preferences prefs;        // Der "Festplatten"-Manager für dauerhaftes Speichern

static DeviceProfile profiles[NUM_DEVICES]; // Unser Speicherplatz für die 5 Geräte
static MenuLevel     menuLevel  = MENU_MAIN; // Wir starten im Hauptmenü
static uint8_t       selDevice  = 0;         // Welches Gerät ist gerade im Menü ausgewählt?
static uint8_t       selAction  = 0;         // Welche Aktion (Lernen/Senden...) ist ausgewählt?
static uint8_t       selSlot    = 0;         // Welcher Knopf-Steckplatz ist ausgewählt?
static bool          delConfirm = false;     // Steht der Lösch-Zeiger auf "JA" oder "NEIN"?

// Die Standard-Namen für unsere 4 Knöpfe
static const char* CMD_NAMES[NUM_CMDS] = {"Power", "Vol+", "Vol-", "OK"};

// =============================================================================
// TASTER-KONTROLLE (Das Entprellen)
// Wenn man einen echten Knopf drückt, federt das Metall im Inneren ganz kurz.
// Der Computer würde denken, man hat den Knopf 10-mal gedrückt. Das verhindern wir hier.
// =============================================================================

struct BtnState {
    uint8_t     pin;        // Welcher Stecker-Pin?
    ButtonEvent event;      // Welches Signal sendet dieser Knopf?
    bool        lastRaw;    // Wie war der Knopf im allerletzten Sekundenbruchteil?
    bool        stable;     // Ist der Knopf gerade wirklich gedrückt oder federt er nur?
    uint32_t    lastChange; // Wann hat sich der Zustand zuletzt geändert?
    bool        handled;    // Haben wir den Knopfdruck schon verarbeitet?
};

// Unsere 4 physischen Knöpfe werden hier registriert
static BtnState btns[] = {
    {PIN_BTN_UP,     BTN_UP,     true, true, 0, false},
    {PIN_BTN_DOWN,   BTN_DOWN,   true, true, 0, false},
    {PIN_BTN_BACK,   BTN_BACK,   true, true, 0, false},
    {PIN_BTN_SELECT, BTN_SELECT, true, true, 0, false},
};

// Knöpfe startklar machen
void buttons_init() {
    for (auto& b : btns) {
        pinMode(b.pin, INPUT_PULLUP); // Aktiviert den internen Stromkreislauf für den Knopf
        b.lastRaw = b.stable = digitalRead(b.pin); // Liest den aktuellen Zustand
    }
}

// Diese Funktion schaut ununterbrochen nach, ob ein Knopf gedrückt wurde
ButtonEvent buttons_read() {
    uint32_t now = millis(); // Aktuelle Uhrzeit des Computers in Millisekunden
    for (auto& b : btns) {
        bool raw = digitalRead(b.pin); // Knopf prüfen
        if (raw != b.lastRaw) { b.lastRaw = raw; b.lastChange = now; } // Zustand hat sich geändert
        
        // Wenn sich der Zustand länger als 20ms (DEBOUNCE_MS) nicht verändert hat, ist er stabil
        if ((now - b.lastChange) >= DEBOUNCE_MS && raw != b.stable) {
            b.stable  = raw;
            b.handled = false;
        }
        // Wenn der Knopf gedrückt ist (LOW/false) und wir es noch nicht gemeldet haben: Melden!
        if (!b.stable && !b.handled) { b.handled = true; return b.event; }
    }
    return BTN_NONE; // Kein Knopf gedrückt
}

// Wartet so lange, bis der Nutzer den "Bestätigen"-Knopf wieder loslässt
void waitForSelectRelease() {
    while (digitalRead(PIN_BTN_SELECT) == LOW) delay(10);
    for (auto& b : btns) {
        if (b.pin == PIN_BTN_SELECT) {
            b.lastRaw = b.stable = true;
            b.handled = true;
        }
    }
}

// =============================================================================
// BILDSCHIRM-ANSICHTEN (Die Grafik-Abteilung)
// Hier wird bestimmt, was auf dem kleinen Display angezeigt wird.
// =============================================================================

// Malt den schwarzen Balken ganz oben mit der Überschrift
static void drawHeader(const char* title) {
    u8g2.setDrawColor(1); // Weiße Farbe
    u8g2.drawBox(0, 0, 128, 14); // Balken zeichnen
    u8g2.setDrawColor(0); // Schwarze Farbe für die Schrift
    u8g2.setFont(u8g2_font_6x10_tf); // Schriftart wählen
    u8g2.drawStr(2, 11, title); // Text schreiben
    u8g2.setDrawColor(1); // Farbe wieder zurück auf Weiß stellen
}

// Zeigt die Liste der 5 Geräte an
void dispMain() {
    u8g2.clearBuffer(); // Bildschirm im Hintergrund sauber wischen
    drawHeader(" Geraete-Liste");
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_DEVICES; i++) {
        char line[22];
        // Wenn belegt, Name zeigen, sonst "[Leer]" hinschreiben
        snprintf(line, sizeof(line), "%u: %s", i + 1,
                 profiles[i].isUsed ? profiles[i].name : "[Leer]");
        u8g2.drawStr(8, 21 + i * 10, line);
    }
    u8g2.drawStr(0, 21 + selDevice * 10, ">"); // Der Auswahlpfeil
    u8g2.sendBuffer(); // Das gezeichnete Bild auf das echte Display schieben
}

// Zeigt das Menü: Was willst du tun? (Lernen, Senden, Löschen)
void dispAction() {
    static const char* act[] = {"Lernen", "Senden", "Loeschen"};
    u8g2.clearBuffer();
    char h[32]; snprintf(h, sizeof(h), " %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < 3; i++)
        u8g2.drawStr(8, 25 + i * 13, act[i]);
    u8g2.drawStr(0, 25 + selAction * 13, ">");
    u8g2.sendBuffer();
}

// Zeigt die 4 Knöpfe des Geräts im Lernmodus an
void dispLearnSelect() {
    u8g2.clearBuffer();
    char h[32]; snprintf(h, sizeof(h), " Lernen: %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_CMDS; i++) {
        char line[22];
        // Zeigt ein "[OK]" hinter dem Knopf an, wenn er schon programmiert wurde
        snprintf(line, sizeof(line), "%s%s", profiles[selDevice].commands[i].name,
                 profiles[selDevice].commands[i].isEmpty ? "" : " [OK]");
        u8g2.drawStr(8, 21 + i * 11, line);
    }
    u8g2.drawStr(0, 21 + selSlot * 11, ">");
    u8g2.sendBuffer();
}

// Aufforderung, die echte Fernbedienung vor das Infrarot-Auge zu halten
void dispLearnWait(const char* slotName) {
    u8g2.clearBuffer();
    drawHeader(" Lernmodus");
    u8g2.setFont(u8g2_font_6x10_tf);
    char l[32]; snprintf(l, sizeof(l), "Slot: %s", slotName);
    u8g2.drawStr(2, 28, l);
    u8g2.drawStr(2, 42, "Warte auf Signal...");
    u8g2.drawStr(2, 55, "(BACK zum Abbrechen)");
    u8g2.sendBuffer();
}

// Zeigt das Ergebnis des Anlernens (Erfolg, Zeit abgelaufen oder abgebrochen)
void dispLearnResult(bool ok, const char* slotName, bool aborted = false) {
    u8g2.clearBuffer();
    if (aborted) {
        drawHeader(" Abgebrochen");
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(2, 36, "Lernmodus beendet.");
    } else {
        drawHeader(ok ? " Gespeichert!" : " Timeout!");
        u8g2.setFont(u8g2_font_6x10_tf);
        if (ok) {
            char l[24]; snprintf(l, sizeof(l), "'%s' gelernt.", slotName);
            u8g2.drawStr(2, 25, l);
            char c[24];
            if (g_lastClusterCount > 0)
                snprintf(c, sizeof(c), "%u Cluster | bereinigt", g_lastClusterCount);
            else
                snprintf(c, sizeof(c), "Signal bereinigt.");
            u8g2.drawStr(2, 39, c);
            u8g2.drawStr(2, 53, "Gespeichert!");
        } else {
            u8g2.drawStr(2, 36, "Kein Signal.");
            u8g2.drawStr(2, 50, "Bitte wiederholen.");
        }
    }
    u8g2.sendBuffer();
    delay(1500); // Zeige diese Nachricht für 1,5 Sekunden an
}

// Zeigt die Liste der Knöpfe an, die man jetzt abschicken (senden) kann
void dispSendMode() {
    u8g2.clearBuffer();
    char h[32]; snprintf(h, sizeof(h), " Senden: %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_CMDS; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%s",
                 profiles[selDevice].commands[i].isEmpty
                     ? "(leer)"
                     : profiles[selDevice].commands[i].name);
        u8g2.drawStr(8, 21 + i * 11, line);
    }
    u8g2.drawStr(0, 21 + selSlot * 11, ">");
    u8g2.sendBuffer();
}

// Kurzes Aufblitzen des Bildschirms beim Senden als visuelle Rückmeldung
void dispSendFeedback(const char* name) {
    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, 128, 64); // Macht den ganzen Bildschirm kurz weiß
    u8g2.setDrawColor(0); // Schriftfarbe auf Schwarz umstellen
    u8g2.setFont(u8g2_font_8x13B_tf);
    uint8_t w = u8g2.getStrWidth(name);
    u8g2.drawStr((128 - w) / 2, 38, name); // Schreibt den Namen des Knopfes zentriert
    u8g2.sendBuffer();
    delay(200); // 0,2 Sekunden aufblitzen lassen
    u8g2.setDrawColor(1); // Farbe wieder zurückstellen
}

// Die Sicherheitsabfrage vor dem Löschen eines Geräts
void dispDeleteConfirm() {
    u8g2.clearBuffer();
    drawHeader(" Loeschen?");
    u8g2.setFont(u8g2_font_6x10_tf);
    char l[32]; snprintf(l, sizeof(l), "'%s'", profiles[selDevice].name);
    u8g2.drawStr(2, 28, l);
    u8g2.drawStr(2, 41, "wirklich loeschen?");
    
    // Je nachdem, was ausgewählt ist, wird "JA" oder "NEIN" mit einem Kasten hinterlegt
    if (delConfirm) {
        u8g2.drawBox(8, 50, 28, 12); u8g2.setDrawColor(0);
        u8g2.drawStr(12, 60, "JA");  u8g2.setDrawColor(1);
        u8g2.drawStr(60, 60, "NEIN");
    } else {
        u8g2.drawStr(12, 60, "JA");
        u8g2.drawBox(56, 50, 36, 12); u8g2.setDrawColor(0);
        u8g2.drawStr(60, 60, "NEIN"); u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();
}

// =============================================================================
// SIGNAL-REINIGUNG (Die Mathematik hinter dem Filter)
// Weil Infrarotsignale durch Umgebungslicht (z.B. Lampen) ungenau gemessen werden,
// runden wir die Werte hier mathematisch sinnvoll ab/auf, damit sie perfekt sind.
// =============================================================================

static uint8_t ir_clean_signal(IRCommand& cmd) {
    if (!cmd.hasRaw || cmd.rawLen < 3) return 0;

    const uint16_t n = cmd.rawLen;

    // Wir kopieren das Signal und sortieren es von klein nach groß
    uint16_t sorted[MAX_RAW_LEN];
    memcpy(sorted, cmd.rawData, n * sizeof(uint16_t));

    // Ein klassischer Sortieralgorithmus (Insertion Sort)
    for (uint16_t i = 1; i < n; i++) {
        uint16_t key = sorted[i];
        int16_t  j   = (int16_t)i - 1;
        while (j >= 0 && sorted[j] > key) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = key;
    }

    // Wir erstellen "Schubladen" (Cluster), um ähnliche Längen zusammenzufassen
    struct Cluster {
        uint32_t sum;    
        uint16_t count;  
        uint16_t low;    
        uint16_t high;   
        uint16_t mean;   
    };

    Cluster cl[MAX_CLUSTERS];
    uint8_t numCl = 0;

    uint16_t firstVal = sorted[0] ? sorted[0] : 1;
    cl[0] = { firstVal, 1, firstVal, firstVal, 0 };
    numCl = 1;

    // Hier werden die Signalzeiten in die Schubladen einsortiert
    for (uint16_t i = 1; i < n; i++) {
        uint16_t prev = sorted[i - 1] ? sorted[i - 1] : 1;
        float    ratio = (float)sorted[i] / (float)prev;

        // Wenn der Sprung zum nächsten Wert zu groß ist, machen wir eine neue Schublade auf
        if (ratio > CLUSTER_RATIO_THRESH && numCl < MAX_CLUSTERS) {
            cl[numCl - 1].mean = (uint16_t)(cl[numCl - 1].sum / cl[numCl - 1].count);
            cl[numCl] = { sorted[i], 1, sorted[i], sorted[i], 0 };
            numCl++;
        } else {
            // Ansonsten gehört der Wert in die aktuelle Schublade
            cl[numCl - 1].sum  += sorted[i];
            cl[numCl - 1].count++;
            cl[numCl - 1].high  = sorted[i];  
        }
    }
    cl[numCl - 1].mean = (uint16_t)(cl[numCl - 1].sum / cl[numCl - 1].count);

    // Technische Kontrollausgabe für den Computer-Monitor (Serieller Monitor)
    Serial.printf("[IR-Filter] %u Messwerte → %u Cluster erkannt:\n", n, numCl);
    for (uint8_t c = 0; c < numCl; c++) {
        float jitter = (cl[c].mean > 0)
                       ? 50.0f * (float)(cl[c].high - cl[c].low) / (float)cl[c].mean
                       : 0.0f;
        Serial.printf("  [%u] Mittel=%5u µs | n=%-3u | Bereich=[%u…%u] | Jitter±%.1f%%\n",
                      c, cl[c].mean, cl[c].count, cl[c].low, cl[c].high, jitter);
    }

    // Jetzt ersetzen wir die ungenauen Wackel-Werte durch den glatten Durchschnitt der Schublade
    for (uint16_t i = 0; i < n; i++) {
        uint16_t val   = cmd.rawData[i];
        uint16_t best  = val;
        uint32_t bestD = 0xFFFFFFFFUL;
        for (uint8_t c = 0; c < numCl; c++) {
            uint32_t d = (val >= cl[c].mean) ? (val - cl[c].mean) : (cl[c].mean - val);
            if (d < bestD) { bestD = d; best = cl[c].mean; }
        }
        cmd.rawData[i] = best;
    }

    // Versuch, das Signal auf ein mathematisches Raster (z.B. Vielfache von 500) zu runden ("Grid-Snap")
    uint16_t baseUnit = cl[0].mean;
    bool     canSnap  = (baseUnit >= 50);

    if (canSnap) {
        for (uint8_t c = 1; c < numCl && canSnap; c++) {
            float ratio = (float)cl[c].mean / (float)baseUnit;
            float mult  = roundf(ratio);
            if (mult < 1.0f) mult = 1.0f;
            if (fabsf(ratio - mult) > GRID_SNAP_TOL) canSnap = false;
        }
    }

    if (canSnap) {
        Serial.printf("[IR-Filter] Grid-Snap: Einheit = %u µs – alle Werte normiert.\n", baseUnit);
        for (uint16_t i = 0; i < n; i++) {
            float    ratio   = (float)cmd.rawData[i] / (float)baseUnit;
            uint16_t mult    = (uint16_t)(ratio + 0.5f);
            if (mult < 1)    mult = 1;
            uint32_t snapped = (uint32_t)mult * (uint32_t)baseUnit;
            cmd.rawData[i]   = (snapped > 0xFFFF) ? 0xFFFF : (uint16_t)snapped;
        }
    } else {
        Serial.println(F("[IR-Filter] Grid-Snap: kein reguläres Raster erkannt – Cluster-Mittelwerte verwendet."));
    }

    g_lastClusterCount = numCl;
    return numCl;
}

// =============================================================================
// SPEICHER-VERWALTUNG (Die Langzeit-Datenbank)
// Hier werden die Fernbedienungscodes dauerhaft abgespeichert, damit sie nach
// dem Ausschalten nicht weg sind.
// =============================================================================

// Lädt alle gespeicherten Knöpfe und Geräte von der Festplatte
void storage_load() {
    char ns[12];
    for (uint8_t d = 0; d < NUM_DEVICES; d++) {
        snprintf(ns, sizeof(ns), "ir_dev_%u", d);
        // Wenn noch nie etwas gespeichert wurde, erstellen wir leere Standard-Geräte
        if (!prefs.begin(ns, true)) {
            Serial.printf("[Storage] '%s' noch nicht angelegt (Erststart OK).\n", ns);
            memset(&profiles[d], 0, sizeof(DeviceProfile));
            snprintf(profiles[d].name, sizeof(profiles[d].name), "Geraet %u", d + 1);
            for (uint8_t c = 0; c < NUM_CMDS; c++) {
                profiles[d].commands[c].isEmpty = true;
                strncpy(profiles[d].commands[c].name, CMD_NAMES[c], 16);
            }
            continue;
        }
        // Ansonsten: Namen und Daten aus dem Speicher laden
        prefs.getString("name", profiles[d].name, sizeof(profiles[d].name));
        profiles[d].isUsed = prefs.getBool("used", false);

        for (uint8_t c = 0; c < NUM_CMDS; c++) {
            IRCommand& cmd = profiles[d].commands[c];
            char key[14];

            snprintf(key, sizeof(key), "c%u_empty", c);  cmd.isEmpty = prefs.getBool(key, true);
            snprintf(key, sizeof(key), "c%u_name", c);   prefs.getString(key, cmd.name, sizeof(cmd.name));
            if (!strlen(cmd.name)) strncpy(cmd.name, CMD_NAMES[c], 16);

            snprintf(key, sizeof(key), "c%u_freq", c);   cmd.frequency = prefs.getUShort(key, 38);
            snprintf(key, sizeof(key), "c%u_hprot", c);  cmd.hasProtocol = prefs.getBool(key, false);
            if (cmd.hasProtocol) {
                snprintf(key, sizeof(key), "c%u_prot", c); cmd.protocol = (decode_type_t)prefs.getUInt(key, UNKNOWN);
                snprintf(key, sizeof(key), "c%u_val", c);  cmd.value = prefs.getULong64(key, 0);
                snprintf(key, sizeof(key), "c%u_bits", c); cmd.bits = prefs.getUShort(key, 0);
            }

            snprintf(key, sizeof(key), "c%u_hraw", c);   cmd.hasRaw = prefs.getBool(key, false);
            if (cmd.hasRaw) {
                snprintf(key, sizeof(key), "c%u_rlen", c); cmd.rawLen = prefs.getUShort(key, 0);
                snprintf(key, sizeof(key), "c%u_rdat", c); prefs.getBytes(key, cmd.rawData, cmd.rawLen * sizeof(uint16_t));
            }
        }
        prefs.end(); // Speicher schließen
    }
}

// Speichert die Daten eines bestimmten Geräts auf der Festplatte ab
void storage_save(uint8_t d) {
    char ns[12];
    snprintf(ns, sizeof(ns), "ir_dev_%u", d);
    if (!prefs.begin(ns, false)) return;

    prefs.putString("name", profiles[d].name);
    prefs.putBool("used", profiles[d].isUsed);

    for (uint8_t c = 0; c < NUM_CMDS; c++) {
        const IRCommand& cmd = profiles[d].commands[c];
        char key[14];

        prefs.putBool(key, cmd.isEmpty);
        prefs.putString(key, cmd.name);
        prefs.putUShort(key, cmd.frequency);
        prefs.putBool(key, cmd.hasProtocol);
        if (cmd.hasProtocol) {
            prefs.putUInt(key, cmd.protocol);
            prefs.putULong64(key, cmd.value);
            prefs.putUShort(key, cmd.bits);
        }
        prefs.putBool(key, cmd.hasRaw);
        if (cmd.hasRaw) {
            prefs.putUShort(key, cmd.rawLen);
            prefs.putBytes(key, cmd.rawData, cmd.rawLen * sizeof(uint16_t));
        }
    }
    prefs.end();
    Serial.printf("[Storage] Geraet %u gespeichert.\n", d + 1);
}

// Löscht ein Gerät komplett aus dem Dauerspeicher
void storage_delete(uint8_t d) {
    char ns[12];
    snprintf(ns, sizeof(ns), "ir_dev_%u", d);
    if (prefs.begin(ns, false)) { prefs.clear(); prefs.end(); }
    Serial.printf("[Storage] Geraet %u geloescht.\n", d + 1);
}

// =============================================================================
// INFRAROT-AKTIONEN (Lernen & Senden)
// Hier schlägt das Herz der Hardware-Interaktion.
// =============================================================================

// Diese Funktion aktiviert das Infrarot-Auge und fängt ein Signal aus der Luft ab
int ir_learn(IRCommand& cmd) {
    Serial.printf("\n[IR] Lernmodus – warte %u s auf Signal...\n", IR_TIMEOUT_MS / 1000);
    irRecv->resume(); // Auge öffnen und empfangsbereit machen

    decode_results res; // Hier drin landet das abgefangene Signal
    uint32_t deadline = millis() + IR_TIMEOUT_MS; // Berechnet, wann die 15 Sek Exo-Zeit um sind

    // Warteschleife: Läuft so lange, bis die Zeit um ist
    while (millis() < deadline) {
        ButtonEvent ev = buttons_read();
        if (ev == BTN_BACK) { // Wenn der Nutzer "Zurück" drückt, brechen wir ab
            Serial.println(F("[IR] Lernmodus vom Benutzer abgebrochen."));
            waitForSelectRelease();
            return -1; // -1 steht für "Abgebrochen"
        }

        if (!irRecv->decode(&res)) { delay(10); continue; } // Wenn noch kein Signal in der Luft war, weitersuchen

        // Sonderfall für Panasonic-Geräte, da diese eine andere Frequenz nutzen
        if (res.decode_type == PANASONIC) {
            cmd.hasProtocol = false;
            cmd.frequency   = 37;   
        } else {
            // Prüfen, ob wir die Sprache (Protokoll) des Fernsehers kennen
            cmd.hasProtocol = (res.decode_type != UNKNOWN && res.decode_type != RAW);
            cmd.frequency   = 38;   
        }

        // Wenn die Sprache bekannt ist, speichern wir einfach die kurze Code-Nummer
        if (cmd.hasProtocol) {
            cmd.protocol = res.decode_type;
            cmd.value    = res.value;
            cmd.bits     = res.bits;
        }

        // Wenn die Sprache unbekannt ist, müssen wir das rohe Blinkmuster aufzeichnen
        cmd.hasRaw = (res.rawlen > 1 && (res.rawlen - 1) <= MAX_RAW_LEN);
        if (cmd.hasRaw) {
            cmd.rawLen = res.rawlen - 1;
            for (uint16_t i = 1; i < res.rawlen; i++) {
                // Umrechnung der internen Zeiteinheiten des Computers in echte Mikrosekunden
                uint32_t calcMicroseconds = (uint32_t)res.rawbuf[i] * kRawTick;
                cmd.rawData[i - 1] = (calcMicroseconds > 0xFFFF) ? 0xFFFF : (uint16_t)calcMicroseconds;
            }

            // Das rohe Wackelsignal durch unseren mathematischen Reinigungsfilter jagen
            ir_clean_signal(cmd);
        }

        cmd.isEmpty = false; // Dieser Knopf ist nun nicht mehr leer!

        Serial.println(F("\n[IR] Signal empfangen & bereinigt!"));
        irRecv->resume();
        waitForSelectRelease();
        return 1; // 1 steht für "Erfolgreich gelernt!"
    }

    Serial.println(F("[IR] TIMEOUT – kein Signal empfangen."));
    g_lastClusterCount = 0;
    waitForSelectRelease();
    return 0; // 0 steht für "Zeit abgelaufen, nix empfangen"
}

// Diese Funktion lässt das Infrarot-Lämpchen blinken, um den Fernseher zu steuern
void ir_send(const IRCommand& cmd) {
    if (cmd.isEmpty) return; // Wenn der Knopf leer ist, senden wir natürlich nix

    // Wenn wir die Sprache kennen, nutzen wir die fertige Funktion der Bibliothek
    if (cmd.hasProtocol) {
        Serial.printf("[IR] Sende via Protokoll: %s | 0x%llX | %u Bit\n",
                      typeToString(cmd.protocol).c_str(), cmd.value, cmd.bits);
        switch (cmd.protocol) {
            case NEC:     irSend->sendNEC(cmd.value, cmd.bits);     break;
            case SONY:    irSend->sendSony(cmd.value, cmd.bits);    break;
            case SAMSUNG: irSend->sendSAMSUNG(cmd.value, cmd.bits); break;
            case RC5:     irSend->sendRC5(cmd.value, cmd.bits);     break;
            case RC6:     irSend->sendRC6(cmd.value, cmd.bits);     break;
            default:
                // Falls die Sprache zwar bekannt, aber hier nicht gelistet ist, senden wir das Roh-Muster
                if (cmd.hasRaw) {
                    uint16_t f = cmd.frequency ? cmd.frequency : 38;
                    irSend->sendRaw(cmd.rawData, cmd.rawLen, f);
                }
                break;
        }
    } else if (cmd.hasRaw) {
        // Unbekannte Sprache: Wir morsten einfach das gelernte Roh-Blinkmuster ab
        uint16_t f = cmd.frequency ? cmd.frequency : 38;
        Serial.printf("[IR] Sende via RAW (%u Eintraege, %u kHz)\n", cmd.rawLen, f);
        irSend->sendRaw(cmd.rawData, cmd.rawLen, f);
    }
}

// =============================================================================
// MENÜ-LOGIK (Die Zustandsmaschine)
// Das ist das Gehirn des Menüs. Es sorgt dafür, dass sich beim Drücken von Knöpfen
// das richtige Fenster öffnet.
// =============================================================================

// Helfer-Funktionen: Sorgen dafür, dass der Auswahlzeiger am Ende der Liste wieder oben ankommt
static uint8_t cycleUp(uint8_t v, uint8_t n)   { return v == 0 ? n - 1 : v - 1; }
static uint8_t cycleDown(uint8_t v, uint8_t n) { return (v + 1) % n; }

// Stellt sicher, dass ein Gerät existiert und nicht abstürzt, wenn man es auswählt
static void ensureProfile(uint8_t d) {
    if (!profiles[d].isUsed) {
        snprintf(profiles[d].name, sizeof(profiles[d].name), "Geraet %u", d + 1);
        profiles[d].isUsed = true;
        for (uint8_t c = 0; c < NUM_CMDS; c++) {
            profiles[d].commands[c].isEmpty   = true;
            profiles[d].commands[c].frequency = 38;
            strncpy(profiles[d].commands[c].name, CMD_NAMES[c], 16);
        }
    }
}

// Die große Weiche: Was passiert bei welchem Knopfdruck in welchem Menü?
void menu_update(ButtonEvent ev) {
    if (ev == BTN_NONE) return; // Kein Knopf gedrückt? Dann tun wir nix.

    switch (menuLevel) {

    case MENU_MAIN: // WIR SIND IM HAUPTMENÜ
        if (ev == BTN_UP)     { selDevice = cycleUp(selDevice, NUM_DEVICES);   dispMain(); }
        if (ev == BTN_DOWN)   { selDevice = cycleDown(selDevice, NUM_DEVICES); dispMain(); }
        if (ev == BTN_SELECT) {
            ensureProfile(selDevice);
            selAction = 0;
            menuLevel = MENU_ACTION; // Wechsel ins Aktionsmenü
            dispAction();
            waitForSelectRelease();
        }
        break;

    case MENU_ACTION: // WIR WÄHLEN DIE AKTION (Lernen/Senden/Löschen)
        if (ev == BTN_UP)   { selAction = cycleUp(selAction, 3);   dispAction(); }
        if (ev == BTN_DOWN) { selAction = cycleDown(selAction, 3); dispAction(); }
        if (ev == BTN_BACK) { menuLevel = MENU_MAIN; dispMain(); } // Zurück ins Hauptmenü
        if (ev == BTN_SELECT) {
            if (selAction == 0) { // Lernen gewählt
                selSlot = 0; menuLevel = MENU_LEARN_SELECT; dispLearnSelect();
            } else if (selAction == 1) { // Senden gewählt
                selSlot = 0; menuLevel = MENU_SEND; dispSendMode();
            } else { // Löschen gewählt
                delConfirm = false; menuLevel = MENU_DELETE_CONFIRM; dispDeleteConfirm();
            }
            waitForSelectRelease();
        }
        break;

    case MENU_LEARN_SELECT: // WIR WÄHLEN DEN KNOPF ZUM LERNEN AUS
        if (ev == BTN_UP)   { selSlot = cycleUp(selSlot, NUM_CMDS);   dispLearnSelect(); }
        if (ev == BTN_DOWN) { selSlot = cycleDown(selSlot, NUM_CMDS); dispLearnSelect(); }
        if (ev == BTN_BACK) { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            IRCommand& slot = profiles[selDevice].commands[selSlot];
            menuLevel = MENU_LEARN_WAIT;
            dispLearnWait(slot.name);

            int result = ir_learn(slot); // Starte das echte Signal-Abfangen
            if (result == 1) storage_save(selDevice); // Wenn erfolgreich, sofort auf Festplatte sichern

            dispLearnResult(result == 1, slot.name, result == -1); // Ergebnis anzeigen
            menuLevel = MENU_LEARN_SELECT;
            dispLearnSelect();
            waitForSelectRelease();
        }
        break;

    case MENU_SEND: // WIR DRÜCKEN EINEN KNOPF ZUM SENDEN
        if (ev == BTN_UP)   { selSlot = cycleUp(selSlot, NUM_CMDS);   dispSendMode(); }
        if (ev == BTN_DOWN) { selSlot = cycleDown(selSlot, NUM_CMDS); dispSendMode(); }
        if (ev == BTN_BACK) { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            if (!profiles[selDevice].commands[selSlot].isEmpty) {
                ir_send(profiles[selDevice].commands[selSlot]); // Signal über Diode abschicken!
                dispSendFeedback(profiles[selDevice].commands[selSlot].name); // Bildschirm aufblitzen lassen
                dispSendMode();
                waitForSelectRelease();
                buttons_init();   
            } else {
                waitForSelectRelease(); // Leere Knöpfe tun einfach nix
            }
        }
        break;

    case MENU_DELETE_CONFIRM: // SICHERHEITSABFRAGE LÖSCHEN
        if (ev == BTN_UP || ev == BTN_DOWN) { delConfirm = !delConfirm; dispDeleteConfirm(); }
        if (ev == BTN_BACK)  { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            if (delConfirm) { // Wenn der Nutzer auf "JA" steht und drückt
                storage_delete(selDevice); // Von Festplatte löschen
                // Speicher im Arbeitsspeicher auf Werkseinstellungen zurücksetzen
                memset(&profiles[selDevice], 0, sizeof(DeviceProfile));
                snprintf(profiles[selDevice].name, sizeof(profiles[selDevice].name),
                         "Geraet %u", selDevice + 1);
                for (uint8_t c = 0; c < NUM_CMDS; c++) {
                    profiles[selDevice].commands[c].isEmpty   = true;
                    profiles[selDevice].commands[c].frequency = 38;
                    strncpy(profiles[selDevice].commands[c].name, CMD_NAMES[c], 16);
                }
            }
            menuLevel = MENU_MAIN; dispMain(); // Zurück zum Hauptmenü
            waitForSelectRelease();
        }
        break;

    default: break;
    }
}

// =============================================================================
// START & DAUERSCHLEIFE (Die Pflichtfunktionen)
// Jedes Arduino/ESP32-Programm braucht genau diese beiden Funktionen.
// =============================================================================

// Wird genau EINMAL aufgerufen, wenn der Minicomputer Strom bekommt
void setup() {
    Serial.begin(115200); // Startet die USB-Leitung zum PC für Fehlerdiagnosen
    delay(300);
    Serial.println(F("\n[Main] IR-Fernbedienung v2.1 startet..."));

    // Bildschirm hochfahren und Startbildschirm anzeigen
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(14, 25, "IR-Fernbedienung");
    u8g2.drawStr(22, 38, "v2.1 – Clustering");
    u8g2.drawStr(20, 51, "Initialisiert...");
    u8g2.sendBuffer();
    delay(1500); // Zeige das Logo für 1,5 Sekunden

    buttons_init(); // Knöpfe aktivieren

    // Das Infrarot-Auge am Stecker-Pin aufwecken
    irRecv = new IRrecv(PIN_IR_RECV, IR_BUF_SIZE, 15, true);
    irRecv->enableIRIn();

    // Das Infrarot-Sende-Lämpchen aufwecken
    irSend = new IRsend(PIN_IR_SEND);
    irSend->begin();

    storage_load(); // Alle alten Speicherstände von der Festplatte laden
    dispMain();     // Das Hauptmenü auf dem Bildschirm anzeigen
    Serial.println(F("[Main] Bereit."));
}

// Diese Schleife läuft unendlich oft im Kreis (viele tausend Male pro Sekunde),
// solange das Gerät eingeschaltet ist.
void loop() {
    menu_update(buttons_read()); // Schau nach, ob ein Knopf gedrückt wurde und aktualisiere das Menü
    delay(5); // Ganz kurze Atempause (5 Millisekunden) für den Prozessor, damit er nicht heißläuft
}