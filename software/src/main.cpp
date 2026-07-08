#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>

#define PIN_IR_RECV     17
#define PIN_IR_SEND     18

#define PIN_BTN_UP      32
#define PIN_BTN_DOWN    33
#define PIN_BTN_BACK    27
#define PIN_BTN_SELECT  14

#define PIN_OLED_SDA     4
#define PIN_OLED_SCL    15
#define PIN_OLED_RST    16

#define NUM_DEVICES      5
#define NUM_CMDS         4
#define IR_BUF_SIZE    300
#define MAX_RAW_LEN    300
#define IR_TIMEOUT_MS 15000
#define DEBOUNCE_MS     20

static const float   CLUSTER_RATIO_THRESH = 1.20f;
static const float   GRID_SNAP_TOL        = 0.15f;
static const uint8_t MAX_CLUSTERS         = 24;

static uint8_t g_lastClusterCount = 0;

struct IRCommand {
    char          name[16];
    bool          isEmpty;
    bool          hasProtocol;
    decode_type_t protocol;
    uint64_t      value;
    uint16_t      bits;
    bool          hasRaw;
    uint16_t      rawData[MAX_RAW_LEN];
    uint16_t      rawLen;
    uint16_t      frequency;
};

struct DeviceProfile {
    char      name[16];
    bool      isUsed;
    IRCommand commands[NUM_CMDS];
};

enum MenuLevel : uint8_t {
    MENU_MAIN,
    MENU_ACTION,
    MENU_LEARN_SELECT,
    MENU_LEARN_WAIT,
    MENU_SEND,
    MENU_DELETE_CONFIRM
};

enum ButtonEvent : uint8_t { BTN_NONE, BTN_UP, BTN_DOWN, BTN_BACK, BTN_SELECT };

static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
    U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA, PIN_OLED_RST);

static IRrecv* irRecv = nullptr;
static IRsend* irSend = nullptr;
static Preferences prefs;

static DeviceProfile profiles[NUM_DEVICES];
static MenuLevel     menuLevel  = MENU_MAIN;
static uint8_t       selDevice  = 0;
static uint8_t       selAction  = 0;
static uint8_t       selSlot    = 0;
static bool          delConfirm = false;

static const char* CMD_NAMES[NUM_CMDS] = {"Power", "Vol+", "Vol-", "OK"};

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
            b.stable  = raw;
            b.handled = false;
        }
        if (!b.stable && !b.handled) { b.handled = true; return b.event; }
    }
    return BTN_NONE;
}

void waitForSelectRelease() {
    while (digitalRead(PIN_BTN_SELECT) == LOW) delay(10);
    for (auto& b : btns) {
        if (b.pin == PIN_BTN_SELECT) {
            b.lastRaw = b.stable = true;
            b.handled = true;
        }
    }
}

static void drawHeader(const char* title) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(2, 11, title);
    u8g2.setDrawColor(1);
}

void dispMain() {
    u8g2.clearBuffer();
    drawHeader(" Geraete-Liste");
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_DEVICES; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%u: %s", i + 1,
                 profiles[i].isUsed ? profiles[i].name : "[Leer]");
        u8g2.drawStr(8, 21 + i * 10, line);
    }
    u8g2.drawStr(0, 21 + selDevice * 10, ">");
    u8g2.sendBuffer();
}

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

void dispLearnSelect() {
    u8g2.clearBuffer();
    char h[32]; snprintf(h, sizeof(h), " Lernen: %s", profiles[selDevice].name);
    drawHeader(h);
    u8g2.setFont(u8g2_font_6x10_tf);
    for (uint8_t i = 0; i < NUM_CMDS; i++) {
        char line[22];
        snprintf(line, sizeof(line), "%s%s", profiles[selDevice].commands[i].name,
                 profiles[selDevice].commands[i].isEmpty ? "" : " [OK]");
        u8g2.drawStr(8, 21 + i * 11, line);
    }
    u8g2.drawStr(0, 21 + selSlot * 11, ">");
    u8g2.sendBuffer();
}

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
    delay(1500);
}

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

void dispSendFeedback(const char* name) {
    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_8x13B_tf);
    uint8_t w = u8g2.getStrWidth(name);
    u8g2.drawStr((128 - w) / 2, 38, name);
    u8g2.sendBuffer();
    delay(200);
    u8g2.setDrawColor(1);
}

void dispDeleteConfirm() {
    u8g2.clearBuffer();
    drawHeader(" Loeschen?");
    u8g2.setFont(u8g2_font_6x10_tf);
    char l[32]; snprintf(l, sizeof(l), "'%s'", profiles[selDevice].name);
    u8g2.drawStr(2, 28, l);
    u8g2.drawStr(2, 41, "wirklich loeschen?");

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

static uint8_t ir_clean_signal(IRCommand& cmd) {
    if (!cmd.hasRaw || cmd.rawLen < 3) return 0;

    const uint16_t n = cmd.rawLen;

    uint16_t sorted[MAX_RAW_LEN];
    memcpy(sorted, cmd.rawData, n * sizeof(uint16_t));

    for (uint16_t i = 1; i < n; i++) {
        uint16_t key = sorted[i];
        int16_t  j   = (int16_t)i - 1;
        while (j >= 0 && sorted[j] > key) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = key;
    }

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

    for (uint16_t i = 1; i < n; i++) {
        uint16_t prev = sorted[i - 1] ? sorted[i - 1] : 1;
        float    ratio = (float)sorted[i] / (float)prev;

        if (ratio > CLUSTER_RATIO_THRESH && numCl < MAX_CLUSTERS) {
            cl[numCl - 1].mean = (uint16_t)(cl[numCl - 1].sum / cl[numCl - 1].count);
            cl[numCl] = { sorted[i], 1, sorted[i], sorted[i], 0 };
            numCl++;
        } else {
            cl[numCl - 1].sum  += sorted[i];
            cl[numCl - 1].count++;
            cl[numCl - 1].high  = sorted[i];
        }
    }
    cl[numCl - 1].mean = (uint16_t)(cl[numCl - 1].sum / cl[numCl - 1].count);

    Serial.printf("[IR-Filter] %u Messwerte → %u Cluster erkannt:\n", n, numCl);
    for (uint8_t c = 0; c < numCl; c++) {
        float jitter = (cl[c].mean > 0)
                       ? 50.0f * (float)(cl[c].high - cl[c].low) / (float)cl[c].mean
                       : 0.0f;
        Serial.printf("  [%u] Mittel=%5u µs | n=%-3u | Bereich=[%u…%u] | Jitter±%.1f%%\n",
                      c, cl[c].mean, cl[c].count, cl[c].low, cl[c].high, jitter);
    }

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

void storage_load() {
    char ns[12];
    for (uint8_t d = 0; d < NUM_DEVICES; d++) {
        snprintf(ns, sizeof(ns), "ir_dev_%u", d);
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

void storage_delete(uint8_t d) {
    char ns[12];
    snprintf(ns, sizeof(ns), "ir_dev_%u", d);
    if (prefs.begin(ns, false)) { prefs.clear(); prefs.end(); }
    Serial.printf("[Storage] Geraet %u geloescht.\n", d + 1);
}

int ir_learn(IRCommand& cmd) {
    Serial.printf("\n[IR] Lernmodus – warte %u s auf Signal...\n", IR_TIMEOUT_MS / 1000);
    irRecv->resume();

    decode_results res;
    uint32_t deadline = millis() + IR_TIMEOUT_MS;

    while (millis() < deadline) {
        ButtonEvent ev = buttons_read();
        if (ev == BTN_BACK) {
            Serial.println(F("[IR] Lernmodus vom Benutzer abgebrochen."));
            waitForSelectRelease();
            return -1;
        }

        if (!irRecv->decode(&res)) { delay(10); continue; }

        if (res.decode_type == PANASONIC) {
            cmd.hasProtocol = false;
            cmd.frequency   = 37;
        } else {
            cmd.hasProtocol = (res.decode_type != UNKNOWN && res.decode_type != RAW);
            cmd.frequency   = 38;
        }

        if (cmd.hasProtocol) {
            cmd.protocol = res.decode_type;
            cmd.value    = res.value;
            cmd.bits     = res.bits;
        }

        cmd.hasRaw = (res.rawlen > 1 && (res.rawlen - 1) <= MAX_RAW_LEN);
        if (cmd.hasRaw) {
            cmd.rawLen = res.rawlen - 1;
            for (uint16_t i = 1; i < res.rawlen; i++) {
                uint32_t calcMicroseconds = (uint32_t)res.rawbuf[i] * kRawTick;
                cmd.rawData[i - 1] = (calcMicroseconds > 0xFFFF) ? 0xFFFF : (uint16_t)calcMicroseconds;
            }

            ir_clean_signal(cmd);
        }

        cmd.isEmpty = false;

        Serial.println(F("\n[IR] Signal empfangen & bereinigt!"));
        irRecv->resume();
        waitForSelectRelease();
        return 1;
    }

    Serial.println(F("[IR] TIMEOUT – kein Signal empfangen."));
    g_lastClusterCount = 0;
    waitForSelectRelease();
    return 0;
}

void ir_send(const IRCommand& cmd) {
    if (cmd.isEmpty) return;

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
                if (cmd.hasRaw) {
                    uint16_t f = cmd.frequency ? cmd.frequency : 38;
                    irSend->sendRaw(cmd.rawData, cmd.rawLen, f);
                }
                break;
        }
    } else if (cmd.hasRaw) {
        uint16_t f = cmd.frequency ? cmd.frequency : 38;
        Serial.printf("[IR] Sende via RAW (%u Eintraege, %u kHz)\n", cmd.rawLen, f);
        irSend->sendRaw(cmd.rawData, cmd.rawLen, f);
    }
}

static uint8_t cycleUp(uint8_t v, uint8_t n)   { return v == 0 ? n - 1 : v - 1; }
static uint8_t cycleDown(uint8_t v, uint8_t n) { return (v + 1) % n; }

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

void menu_update(ButtonEvent ev) {
    if (ev == BTN_NONE) return;

    switch (menuLevel) {

    case MENU_MAIN:
        if (ev == BTN_UP)     { selDevice = cycleUp(selDevice, NUM_DEVICES);   dispMain(); }
        if (ev == BTN_DOWN)   { selDevice = cycleDown(selDevice, NUM_DEVICES); dispMain(); }
        if (ev == BTN_SELECT) {
            ensureProfile(selDevice);
            selAction = 0;
            menuLevel = MENU_ACTION;
            dispAction();
            waitForSelectRelease();
        }
        break;

    case MENU_ACTION:
        if (ev == BTN_UP)   { selAction = cycleUp(selAction, 3);   dispAction(); }
        if (ev == BTN_DOWN) { selAction = cycleDown(selAction, 3); dispAction(); }
        if (ev == BTN_BACK) { menuLevel = MENU_MAIN; dispMain(); }
        if (ev == BTN_SELECT) {
            if (selAction == 0) {
                selSlot = 0; menuLevel = MENU_LEARN_SELECT; dispLearnSelect();
            } else if (selAction == 1) {
                selSlot = 0; menuLevel = MENU_SEND; dispSendMode();
            } else {
                delConfirm = false; menuLevel = MENU_DELETE_CONFIRM; dispDeleteConfirm();
            }
            waitForSelectRelease();
        }
        break;

    case MENU_LEARN_SELECT:
        if (ev == BTN_UP)   { selSlot = cycleUp(selSlot, NUM_CMDS);   dispLearnSelect(); }
        if (ev == BTN_DOWN) { selSlot = cycleDown(selSlot, NUM_CMDS); dispLearnSelect(); }
        if (ev == BTN_BACK) { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            IRCommand& slot = profiles[selDevice].commands[selSlot];
            menuLevel = MENU_LEARN_WAIT;
            dispLearnWait(slot.name);

            int result = ir_learn(slot);
            if (result == 1) storage_save(selDevice);

            dispLearnResult(result == 1, slot.name, result == -1);
            menuLevel = MENU_LEARN_SELECT;
            dispLearnSelect();
            waitForSelectRelease();
        }
        break;

    case MENU_SEND:
        if (ev == BTN_UP)   { selSlot = cycleUp(selSlot, NUM_CMDS);   dispSendMode(); }
        if (ev == BTN_DOWN) { selSlot = cycleDown(selSlot, NUM_CMDS); dispSendMode(); }
        if (ev == BTN_BACK) { menuLevel = MENU_ACTION; dispAction(); }
        if (ev == BTN_SELECT) {
            if (!profiles[selDevice].commands[selSlot].isEmpty) {
                ir_send(profiles[selDevice].commands[selSlot]);
                dispSendFeedback(profiles[selDevice].commands[selSlot].name);
                dispSendMode();
                waitForSelectRelease();
                buttons_init();
            } else {
                waitForSelectRelease();
            }
        }
        break;

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
                    profiles[selDevice].commands[c].isEmpty   = true;
                    profiles[selDevice].commands[c].frequency = 38;
                    strncpy(profiles[selDevice].commands[c].name, CMD_NAMES[c], 16);
                }
            }
            menuLevel = MENU_MAIN; dispMain();
            waitForSelectRelease();
        }
        break;

    default: break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println(F("\n[Main] IR-Fernbedienung v2.1 startet..."));

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(14, 25, "IR-Fernbedienung");
    u8g2.drawStr(22, 38, "v2.1 – Clustering");
    u8g2.drawStr(20, 51, "Initialisiert...");
    u8g2.sendBuffer();
    delay(1500);

    buttons_init();

    irRecv = new IRrecv(PIN_IR_RECV, IR_BUF_SIZE, 15, true);
    irRecv->enableIRIn();

    irSend = new IRsend(PIN_IR_SEND);
    irSend->begin();

    storage_load();
    dispMain();
    Serial.println(F("[Main] Bereit."));
}

void loop() {
    menu_update(buttons_read());
    delay(5);
}
