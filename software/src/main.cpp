// =============================================================================
// main.cpp – ESP32 IR-Universalfernbedienung
// Hardware: Heltec WiFi Kit 32 (ESP32 + 0,96" OLED SSD1306)
// =============================================================================

#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>

// =============================================================================
// PINS & KONSTANTEN
// =============================================================================

#define PIN_IR_RECV     17    // TSOP38238
#define PIN_IR_SEND     18    // 2N2222A Basis
#define PIN_BTN_UP      32    // Hoch / Befehl 1
#define PIN_BTN_DOWN    33    // Runter / Befehl 2
#define PIN_BTN_BACK    27    // Zurück / Befehl 3
#define PIN_BTN_SELECT  14    // OK / Befehl 4

// Heltec ESP32 OLED (SSD1306, I2C)
#define PIN_OLED_SDA     4
#define PIN_OLED_SCL    15
#define PIN_OLED_RST    16

#define NUM_DEVICES      5    // Geräteprofile
#define NUM_CMDS         4    // Befehle pro Gerät (= Anzahl Taster)
#define MAX_RAW_LEN    250    // Maximale RAW-Timing-Einträge
#define IR_TIMEOUT_MS 15000   // Lernmodus-Timeout
#define IR_BUF_SIZE    300    // IRrecv-Puffergröße
#define DEBOUNCE_MS     50    // Taster-Entprellzeit

// =============================================================================
// DATENSTRUKTUREN
// =============================================================================

struct IRCommand {
    char     name[16];
    bool     isEmpty;

    // Methode 1: Protokoll-basiert (speichereffizient, präzise)
    bool          hasProtocol;
    decode_type_t protocol;
    uint64_t      value;
    uint16_t      bits;

    // Methode 2: RAW-Replay (universell, auch für exotische Geräte)
    bool     hasRaw;
    uint16_t rawData[MAX_RAW_LEN];
    uint16_t rawLen;
};

struct DeviceProfile {
    char      name[16];
    bool      isUsed;
    IRCommand commands[NUM_CMDS];
};

// Menü-Zustände
enum MenuLevel : uint8_t {
    MENU_MAIN,          // Ebene 1: Geräteliste
    MENU_ACTION,        // Ebene 2: Lernen / Senden / Löschen
    MENU_LEARN_SELECT,  // Ebene 3a: Slot auswählen
    MENU_LEARN_WAIT,    // Warte auf IR-Signal (blockierend)
    MENU_SEND,          // Ebene 3b: Taster senden direkt
    MENU_DELETE_CONFIRM // Lösch-Dialog
};

enum ButtonEvent : uint8_t { BTN_NONE, BTN_UP, BTN_DOWN, BTN_BACK, BTN_SELECT };

// =============================================================================
// GLOBALE OBJEKTE
// =============================================================================

static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
    U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA, PIN_OLED_RST);

static IRrecv* irRecv = nullptr;
static IRsend* irSend = nullptr;
static Preferences prefs;

static DeviceProfile profiles[NUM_DEVICES];
static MenuLevel     menuLevel     = MENU_MAIN;
static uint8_t       selDevice     = 0;
static uint8_t       selAction     = 0;   // 0=Lernen, 1=Senden, 2=Löschen
static uint8_t       selSlot       = 0;
static bool          delConfirm    = false;

static const char* CMD_NAMES[NUM_CMDS] = {"Power", "Vol+", "Vol-", "OK"};

// =============================================================================
// TASTER – Entprellung
// =============================================================================

struct BtnState {
    uint8_t     pin;
    ButtonEvent event;
    bool        lastRaw;
    bool        stable;
    uint32_t    lastChange;
    bool        handled;
};

static BtnState btns[] = {
    {PIN_BTN_UP,     BTN_UP,     true, true, 0, false},
    {PIN_BTN_DOWN,   BTN_DOWN,   true, true, 0, false},
    {PIN_BTN_BACK,   BTN_BACK,   true, true, 0, false},
    {PIN_BTN_SELECT, BTN_SELECT, true, true, 0, false},
};

void buttons_init() {
    for (auto& b : btns) {
        pinMode(b.pin, INPUT_PULLUP);
        b.lastRaw = b.stable = digitalRead(b.pin);
    }
}

ButtonEvent buttons_read() {
    uint32_t now = millis();
    for (auto& b : btns) {
        bool raw = digitalRead(b.pin);
        if (raw != b.lastRaw) { b.lastRaw = raw; b.lastChange = now; }
        if ((now - b.lastChange) >= DEBOUNCE_MS && raw != b.stable) {
            b.stable = raw; b.handled = false;
        }
        if (!b.stable && !b.handled) { b.handled = true; return b.event; }
    }
    return BTN_NONE;
}

// =============================================================================
// DISPLAY – Alle Ansichten
// =============================================================================

static void drawHeader(const char* title) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 11, title);
    u8g2.setDrawColor(1);
}

static void drawCursor(uint8_t row) {
    u8g2.drawStr(0, 15 + row * 13 + 10, ">");
}

void dispMain() {
    u8g2.clearBuffer();
    drawHeader(" Geraete-Liste");
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_DEVICES; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%u: %s", i + 1,
                 profiles[i].isUsed ? profiles[i].name : "[Leer]");
        u8g2.drawStr(8, 15 + i * 13 + 10, line);
    }
    drawCursor(selDevice);
    u8g2.sendBuffer();
}

void dispAction() {
    static const char* act[] = {"Lernen", "Senden", "Loeschen"};
    u8g2.clearBuffer();
    char h[20]; snprintf(h, sizeof(h), " %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < 3; i++)
        u8g2.drawStr(8, 15 + i * 13 + 10, act[i]);
    drawCursor(selAction);
    u8g2.sendBuffer();
}

void dispLearnSelect() {
    u8g2.clearBuffer();
    char h[20]; snprintf(h, sizeof(h), " Lernen: %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_CMDS; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%s%s", profiles[selDevice].commands[i].name,
                 profiles[selDevice].commands[i].isEmpty ? "" : " [OK]");
        u8g2.drawStr(8, 15 + i * 13 + 10, line);
    }
    drawCursor(selSlot);
    u8g2.sendBuffer();
}

void dispLearnWait(const char* slotName) {
    u8g2.clearBuffer();
    drawHeader(" Lernmodus");
    u8g2.setFont(u8g2_font_6x10_tf);
    char l[22]; snprintf(l, sizeof(l), "Slot: %s", slotName);
    u8g2.drawStr(2, 28, l);
    u8g2.drawStr(2, 42, "Fernbed. richten und");
    u8g2.drawStr(2, 55, "Taste druecken...");
    u8g2.sendBuffer();
}

void dispLearnResult(bool ok, const char* slotName) {
    u8g2.clearBuffer();
    drawHeader(ok ? " Gespeichert!" : " Timeout!");
    u8g2.setFont(u8g2_font_6x10_tf);
    if (ok) {
        char l[22]; snprintf(l, sizeof(l), "'%s' gelernt.", slotName);
        u8g2.drawStr(2, 32, l);
        u8g2.drawStr(2, 48, "Protokoll + RAW OK");
    } else {
        u8g2.drawStr(2, 36, "Kein Signal.");
        u8g2.drawStr(2, 50, "Bitte wiederholen.");
    }
    u8g2.sendBuffer();
    delay(1500);
}

void dispSendMode() {
    u8g2.clearBuffer();
    char h[20]; snprintf(h, sizeof(h), " Senden: %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* icons[] = {"[^]", "[v]", "[<]", "[>]"};
    for (uint8_t i = 0; i < NUM_CMDS; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%s %s", icons[i],
                 profiles[selDevice].commands[i].isEmpty ? "(leer)"
                                                         : profiles[selDevice].commands[i].name);
        u8g2.drawStr(2, 15 + i * 13 + 10, line);
    }
    u8g2.sendBuffer();
}

void dispSendFeedback(const char* name) {
    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_8x13B_tf);
    uint8_t w = u8g2.getStrWidth(name);
    u8g2.drawStr((128 - w) / 2, 38, name);
    u8g2.sendBuffer();
    delay(300);
    u8g2.setDrawColor(1);
}

void dispDeleteConfirm() {
    u8g2.clearBuffer();
    drawHeader(" Loeschen?");
    u8g2.setFont(u8g2_font_6x10_tf);
    char l[22]; snprintf(l, sizeof(l), "'%s'", profiles[selDevice].name);
    u8g2.drawStr(2, 28, l);
    u8g2.drawStr(2, 41, "wirklich loeschen?");
    // JA / NEIN mit Invertierung
    if (delConfirm) {
        u8g2.drawBox(8, 50, 28, 12); u8g2.setDrawColor(0);
        u8g2.drawStr(12, 60, "JA"); u8g2.setDrawColor(1);
        u8g2.drawStr(60, 60, "NEIN");
    } else {
        u8g2.drawStr(12, 60, "JA");
        u8g2.drawBox(56, 50, 36, 12); u8g2.setDrawColor(0);
        u8g2.drawStr(60, 60, "NEIN"); u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();
}

// =============================================================================
// STORAGE – NVS via Preferences
// =============================================================================

void storage_load() {
    char ns[12];
    for (uint8_t d = 0; d < NUM_DEVICES; d++) {
        snprintf(ns, sizeof(ns), "ir_dev_%u", d);
        if (!prefs.begin(ns, true)) {
            // Namespace leer → Defaults
            memset(&profiles[d], 0, sizeof(DeviceProfile));
            snprintf(profiles[d].name, sizeof(profiles[d].name), "Geraet %u", d + 1);
            for (uint8_t c = 0; c < NUM_CMDS; c++) {
                profiles[d].commands[c].isEmpty = true;
                strncpy(profiles[d].commands[c].name, CMD_NAMES[c], 16);
            }
            continue;
        }
        prefs.getString("name", profiles[d].name, sizeof(profiles[d].name));
        profiles[d].isUsed = prefs.getBool("used", false);

        for (uint8_t c = 0; c < NUM_CMDS; c++) {
            IRCommand& cmd = profiles[d].commands[c];
            char key[14];

            snprintf(key, sizeof(key), "c%u_empty", c);
            cmd.isEmpty = prefs.getBool(key, true);
            snprintf(key, sizeof(key), "c%u_name", c);
            prefs.getString(key, cmd.name, sizeof(cmd.name));
            if (!strlen(cmd.name)) strncpy(cmd.name, CMD_NAMES[c], 16);

            snprintf(key, sizeof(key), "c%u_hprot", c);
            cmd.hasProtocol = prefs.getBool(key, false);
            if (cmd.hasProtocol) {
                snprintf(key, sizeof(key), "c%u_prot", c);
                cmd.protocol = (decode_type_t)prefs.getUInt(key, UNKNOWN);
                snprintf(key, sizeof(key), "c%u_val", c);
                cmd.value = prefs.getULong64(key, 0);
                snprintf(key, sizeof(key), "c%u_bits", c);
                cmd.bits = prefs.getUShort(key, 0);
            }

            snprintf(key, sizeof(key), "c%u_hraw", c);
            cmd.hasRaw = prefs.getBool(key, false);
            if (cmd.hasRaw) {
                snprintf(key, sizeof(key), "c%u_rlen", c);
                cmd.rawLen = prefs.getUShort(key, 0);
                snprintf(key, sizeof(key), "c%u_rdat", c);
                prefs.getBytes(key, cmd.rawData, cmd.rawLen * sizeof(uint16_t));
            }
        }
        prefs.end();
    }
}

void storage_save(uint8_t d) {
    char ns[12];
    snprintf(ns, sizeof(ns), "ir_dev_%u", d);
    if (!prefs.begin(ns, false)) return;

    prefs.putString("name", profiles[d].name);
    prefs.putBool("used", profiles[d].isUsed);

    for (uint8_t c = 0; c < NUM_CMDS; c++) {
        const IRCommand& cmd = profiles[d].commands[c];
        char key[14];

        snprintf(key, sizeof(key), "c%u_empty", c); prefs.putBool(key, cmd.isEmpty);
        snprintf(key, sizeof(key), "c%u_name",  c); prefs.putString(key, cmd.name);
        snprintf(key, sizeof(key), "c%u_hprot", c); prefs.putBool(key, cmd.hasProtocol);
        if (cmd.hasProtocol) {
            snprintf(key, sizeof(key), "c%u_prot", c); prefs.putUInt(key, cmd.protocol);
            snprintf(key, sizeof(key), "c%u_val",  c); prefs.putULong64(key, cmd.value);
            snprintf(key, sizeof(key), "c%u_bits", c); prefs.putUShort(key, cmd.bits);
        }
        snprintf(key, sizeof(key), "c%u_hraw", c); prefs.putBool(key, cmd.hasRaw);
        if (cmd.hasRaw) {
            snprintf(key, sizeof(key), "c%u_rlen", c); prefs.putUShort(key, cmd.rawLen);
            snprintf(key, sizeof(key), "c%u_rdat", c);
            prefs.putBytes(key, cmd.rawData, cmd.rawLen * sizeof(uint16_t));
        }
    }
    prefs.end();
    Serial.printf("[Storage] Geraet %u gespeichert.\n", d + 1);
}

void storage_delete(uint8_t d) {
    char ns[12];
    snprintf(ns, sizeof(ns), "ir_dev_%u", d);
    if (prefs.begin(ns, false)) { prefs.clear(); prefs.end(); }
    Serial.printf("[Storage] Geraet %u geloescht.\n", d + 1);
}

// =============================================================================
// IR – Lernen & Senden
// =============================================================================

bool ir_learn(IRCommand& cmd) {
    Serial.printf("\n[IR] Lernmodus – warte %u s auf Signal...\n", IR_TIMEOUT_MS / 1000);
    irRecv->resume();

    decode_results res;
    uint32_t deadline = millis() + IR_TIMEOUT_MS;

    while (millis() < deadline) {
        if (!irRecv->decode(&res)) { delay(10); continue; }

        // --- Methode 1: Protokoll ---
        cmd.hasProtocol = (res.decode_type != UNKNOWN && res.decode_type != RAW);
        if (cmd.hasProtocol) {
            cmd.protocol = res.decode_type;
            cmd.value    = res.value;
            cmd.bits     = res.bits;
        }

        // --- Methode 2: RAW ---
        cmd.hasRaw = (res.rawlen > 1 && (res.rawlen - 1) <= MAX_RAW_LEN);
        if (cmd.hasRaw) {
            cmd.rawLen = res.rawlen - 1;
            for (uint16_t i = 1; i < res.rawlen; i++)
                cmd.rawData[i - 1] = res.rawbuf[i] * RAWTICK; // → Mikrosekunden
        }

        cmd.isEmpty = false;

        // --- Serielle Ausgabe beider Datensätze ---
        Serial.println(F("\n╔═══════════════════════════════════╗"));
        Serial.println(F("║       IR-Signal empfangen!        ║"));
        Serial.println(F("╚═══════════════════════════════════╝"));

        Serial.println(F("\n[ PROTOKOLL-DATEN ]"));
        if (cmd.hasProtocol) {
            Serial.printf("  Protokoll : %s\n", typeToString(cmd.protocol).c_str());
            Serial.printf("  Wert (HEX): 0x%llX\n", cmd.value);
            Serial.printf("  Bits      : %u\n", cmd.bits);
        } else {
            Serial.println(F("  Protokoll : UNBEKANNT – nur RAW verfügbar."));
        }

        Serial.println(F("\n[ RAW-TIMING-ARRAY ] (uint16_t, µs, Puls/Pause)"));
        if (cmd.hasRaw) {
            Serial.printf("  Länge: %u\n  { ", cmd.rawLen);
            for (uint16_t i = 0; i < cmd.rawLen; i++) {
                Serial.printf("%u%s", cmd.rawData[i], i < cmd.rawLen - 1 ? ", " : " }");
                if ((i + 1) % 16 == 0) Serial.print(F("\n    "));
            }
            Serial.println();
        }

        irRecv->resume();
        return true;
    }

    Serial.println(F("[IR] TIMEOUT – kein Signal empfangen."));
    return false;
}

void ir_send(const IRCommand& cmd) {
    if (cmd.isEmpty) return;

    if (cmd.hasProtocol) {
        // Methode 1: Protokoll-basiert (bevorzugt)
        Serial.printf("[IR] Sende via Protokoll: %s | 0x%llX | %u Bit\n",
                      typeToString(cmd.protocol).c_str(), cmd.value, cmd.bits);
        irSend->send(cmd.protocol, cmd.value, cmd.bits);
    } else if (cmd.hasRaw) {
        // Methode 2: RAW-Replay (Fallback)
        Serial.printf("[IR] Sende via RAW (%u Einträge, 38 kHz)\n", cmd.rawLen);
        irSend->sendRaw(cmd.rawData, cmd.rawLen, 38);
    }
}

// =============================================================================
// MENU – Zustandsmaschine
// =============================================================================

static uint8_t cycleUp(uint8_t v, uint8_t n)   { return v == 0 ? n - 1 : v - 1; }
static uint8_t cycleDown(uint8_t v, uint8_t n) { return (v + 1) % n; }

static void ensureProfile(uint8_t d) {
    if (!profiles[d].isUsed) {
        snprintf(profiles[d].name, sizeof(profiles[d].name), "Geraet %u", d + 1);
        profiles[d].isUsed = true;
        for (uint8_t c = 0; c < NUM_CMDS; c++) {
            profiles[d].commands[c].isEmpty = true;
            strncpy(profiles[d].commands[c].name, CMD_NAMES[c], 16);
        }
    }
}

void menu_update(ButtonEvent ev) {
    if (ev == BTN_NONE) return;

    switch (menuLevel) {

    // --- Ebene 1: Geräteliste ---
    case MENU_MAIN:
        if (ev == BTN_UP)     { selDevice = cycleUp(selDevice, NUM_DEVICES);   dispMain(); }
        if (ev == BTN_DOWN)   { selDevice = cycleDown(selDevice, NUM_DEVICES); dispMain(); }
        if (ev == BTN_SELECT) {
            ensureProfile(selDevice);
            selAction = 0;
            menuLevel = MENU_ACTION;
            dispAction();
        }
        break;

    // --- Ebene 2: Aktion wählen ---
    case MENU_ACTION:
        if (ev == BTN_UP)   { selAction = cycleUp(selAction, 3);   dispAction(); }
        if (ev == BTN_DOWN) { selAction = cycleDown(selAction, 3); dispAction(); }
        if (ev == BTN_BACK) { menuLevel = MENU_MAIN;  dispMain(); }
        if (ev == BTN_SELECT) {
            if (selAction == 0) {
                selSlot = 0; menuLevel = MENU_LEARN_SELECT; dispLearnSelect();
            } else if (selAction == 1) {
                menuLevel = MENU_SEND; dispSendMode();
            } else {
                delConfirm = false; menuLevel = MENU_DELETE_CONFIRM; dispDeleteConfirm();
            }
        }
        break;

    // --- Ebene 3a: Lern-Slot auswählen ---
    case MENU_LEARN_SELECT:
        if (ev == BTN_UP)   { selSlot = cycleUp(selSlot, NUM_CMDS);   dispLearnSelect(); }
        if (ev == BTN_DOWN) { selSlot = cycleDown(selSlot, NUM_CMDS); dispLearnSelect(); }
        if (ev == BTN_BACK) { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            IRCommand& slot = profiles[selDevice].commands[selSlot];
            menuLevel = MENU_LEARN_WAIT;
            dispLearnWait(slot.name);
            bool ok = ir_learn(slot);
            if (ok) storage_save(selDevice);
            dispLearnResult(ok, slot.name);
            menuLevel = MENU_LEARN_SELECT;
            dispLearnSelect();
        }
        break;

    // --- Ebene 3b: Sende-Modus ---
    case MENU_SEND: {
        int8_t idx = -1;
        if (ev == BTN_UP)     idx = 0;
        if (ev == BTN_DOWN)   idx = 1;
        if (ev == BTN_SELECT) idx = 3;
        if (ev == BTN_BACK)   { menuLevel = MENU_ACTION; dispAction(); break; }
        if (idx >= 0 && !profiles[selDevice].commands[idx].isEmpty) {
            dispSendFeedback(profiles[selDevice].commands[idx].name);
            ir_send(profiles[selDevice].commands[idx]);
            dispSendMode();
        }
        break;
    }

    // --- Lösch-Bestätigung ---
    case MENU_DELETE_CONFIRM:
        if (ev == BTN_UP || ev == BTN_DOWN) { delConfirm = !delConfirm; dispDeleteConfirm(); }
        if (ev == BTN_BACK)  { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            if (delConfirm) {
                storage_delete(selDevice);
                memset(&profiles[selDevice], 0, sizeof(DeviceProfile));
                snprintf(profiles[selDevice].name, sizeof(profiles[selDevice].name),
                         "Geraet %u", selDevice + 1);
                for (uint8_t c = 0; c < NUM_CMDS; c++) {
                    profiles[selDevice].commands[c].isEmpty = true;
                    strncpy(profiles[selDevice].commands[c].name, CMD_NAMES[c], 16);
                }
            }
            menuLevel = MENU_MAIN; dispMain();
        }
        break;

    default: break;
    }
}

// =============================================================================
// SETUP & LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println(F("\n[Main] IR-Fernbedienung startet..."));

    // OLED
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(14, 30, "IR-Fernbedienung");
    u8g2.drawStr(20, 46, "Initialisiert...");
    u8g2.sendBuffer();
    delay(1500);

    // Hardware init
    buttons_init();

    irRecv = new IRrecv(PIN_IR_RECV, IR_BUF_SIZE, 15, true);
    irRecv->enableIRIn();

    irSend = new IRsend(PIN_IR_SEND);
    irSend->begin();

    // Profile aus NVS laden
    storage_load();

    // Hauptmenü anzeigen
    dispMain();
    Serial.println(F("[Main] Bereit."));
}

void loop() {
    menu_update(buttons_read());
    delay(5);
}