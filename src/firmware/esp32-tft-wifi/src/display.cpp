#include "display.h"
#include "config.h"
#include "state.h"
#include <TFT_eSPI.h>
#include <time.h>

static TFT_eSPI tft = TFT_eSPI();

static int16_t page = 0, lastPage = -1;
static unsigned long lastFrame = 0;
static uint16_t fi = 0;

static String sState, sOutput, sAgent;
static bool sConnected = false;
static bool dirty = true;

#define W 320
#define H 240

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static uint16_t dim(uint16_t c, uint8_t p) {
    return ((((c >> 11) & 0x1F) * p / 100) << 11)
         | ((((c >> 5) & 0x3F) * p / 100) << 5)
         | (((c & 0x1F) * p / 100));
}

static uint16_t sc() {
    const String& s = pet.state;
    if (s == "thinking")   return rgb565(200, 175, 218);
    if (s == "working" || s == "typing") return rgb565(155, 200, 175);
    if (s == "building")   return rgb565(215, 182, 128);
    if (s == "juggling")   return rgb565(195, 165, 215);
    if (s == "attention")  return rgb565(222, 195, 120);
    if (s == "notification") return rgb565(212, 162, 162);
    if (s == "error")      return rgb565(205, 128, 128);
    if (s == "sweeping")   return rgb565(138, 190, 195);
    if (s == "sleeping")   return rgb565(142, 155, 188);
    return rgb565(218, 178, 128);
}
static const char* bigLabel() {
    const String& s = pet.state;
    if (s == "thinking")   return "THINKING";
    if (s == "working" || s == "typing") return "WORKING";
    if (s == "building")   return "BUILDING";
    if (s == "juggling")   return "JUGGLING";
    if (s == "attention")  return "ATTENTION";
    if (s == "notification") return "NOTIFY";
    if (s == "error")      return "ERROR";
    if (s == "sweeping")   return "SWEEPING";
    if (s == "sleeping")   return "SLEEPING";
    return "IDLE";
}

void displayInit() {
    tft.init(); tft.initDMA();
    tft.setRotation(CP_TFT_ROTATION);
    tft.setSwapBytes(true);
    pinMode(17, OUTPUT);
    digitalWrite(17, HIGH);
    tft.fillScreen(0x0000);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x4208, 0x0000);
    tft.drawString("VIBE PET", W / 2, H / 2, 2);
    tft.setTextDatum(TL_DATUM);
}
void displayBrightness(int b) {
    if (b < 10) b = 10;
    if (b > 255) b = 255;
    currentBrightness = b;
    digitalWrite(17, b > 0 ? HIGH : LOW);
}

static void drawTop() {
    tft.fillRect(0, 0, W, 28, 0x0000);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("VIBE PET", 8, 4, 1);

    if (isConnected && pet.agent.length() > 0) {
        String ag = pet.agent;
        ag.toUpperCase();
        if (ag.length() > 10) ag = ag.substring(0, 9) + ".";
        tft.setTextColor(dim(0xFFFF, 40), 0x0000);
        tft.drawString(ag.c_str(), W / 2, 4, 1);
    }

    tft.setTextDatum(TL_DATUM);

    // 连接指示器（用图形，不用字符）
    uint16_t dotColor = isConnected ? rgb565(145, 205, 175) : 0x4208;
    tft.fillCircle(W - 14, 10, 5, dotColor);
    if (!isConnected) {
        tft.fillCircle(W - 14, 10, 3, 0x0000); // 空心效果
    }
}

static String debounceState = "";
static unsigned long debounceTime = 0;
#define DB_MS 600

static void drawMain() {
    uint16_t gc = sc();

    // 消抖：状态快速变化时不刷新
    if (pet.state != debounceState) {
        debounceState = pet.state;
        debounceTime = millis();
    }
    bool stable = (millis() - debounceTime > DB_MS);
    bool stateChanged = (stable && (pet.state != sState || pet.agent != sAgent)) || isConnected != sConnected;
    bool outputChanged = (pet.output != sOutput);

    // 页面切换或状态变化 → 全部重绘
    if (dirty || stateChanged) {
        tft.fillScreen(0x0000);
        drawTop();

        // 大字 — 所有状态统一大小
        tft.setTextDatum(MC_DATUM);
        if (isConnected) {
            tft.setTextSize(5);
            tft.setTextColor(gc, 0x0000);
            tft.drawString(bigLabel(), W / 2, 72, 1);
            tft.setTextSize(1);
        } else {
            tft.setTextSize(4);
            tft.setTextColor(dim(0xFFFF, 25), 0x0000);
            tft.drawString("DISCONNECTED", W / 2, 72, 1);
            tft.setTextSize(1);
        }

        sState = pet.state; sAgent = pet.agent; sConnected = isConnected;
        dirty = false;
        outputChanged = true;  // 强制刷新底部
    }

    // 底部信息（状态变化或输出变化时刷新）
    if (outputChanged) {
        tft.fillRect(0, 155, W, 85, 0x0000);
        if (isConnected) {
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(2);
            tft.setTextColor(dim(0xFFFF, 16), 0x0000);
            String out = pet.output;
            if (out.length() > 28) out = out.substring(0, 25) + "...";
            tft.drawString(out.c_str(), W / 2, 175, 1);
            tft.setTextSize(1);
        }
        sOutput = pet.output;
    }

    tft.setTextDatum(TL_DATUM);
}

static void drawDetail() {
    uint16_t gc = sc();
    bool stateChanged = (pet.state != sState || pet.output != sOutput || pet.agent != sAgent || isConnected != sConnected);
    if (!dirty && !stateChanged) return;
    dirty = false;

    tft.fillScreen(0x0000);
    int y = 30;
    tft.setTextDatum(TL_DATUM);

    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("AGENT", 24, y, 1);
    tft.setTextSize(3);
    tft.setTextColor(dim(0xFFFF, 90), 0x0000);
    tft.drawString(pet.agent, 24, y + 22, 1);

    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("STATUS", 24, y + 60, 1);
    tft.setTextSize(3);
    tft.setTextColor(gc, 0x0000);
    tft.drawString(bigLabel(), 24, y + 82, 1);

    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("PERSONA", 24, y + 120, 1);
    tft.setTextSize(3);
    tft.setTextColor(dim(0xFFFF, 70), 0x0000);
    tft.drawString(pet.personaName, 24, y + 142, 1);

    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("OUTPUT", 24, y + 180, 1);
    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 55), 0x0000);
    String o = pet.output.length() > 0 ? pet.output : "(none)";
    int16_t oy = y + 204;
    while (o.length() > 0 && oy < H - 20) {
        int cut = 0;
        for (size_t i = 0, px = 0; i < o.length() && px < 260; i++, cut++) {
            px += ((o[i] & 0x80) != 0) ? 22 : 14;
            if ((o[i] & 0x80) != 0) i++;
        }
        tft.drawString(o.substring(0, cut), 24, oy, 1);
        o = o.substring(cut); oy += 18;
    }
    tft.setTextSize(1);

    tft.setTextDatum(TR_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(dim(0xFFFF, 35), 0x0000);
    struct tm ti;
    if (getLocalTime(&ti)) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        tft.drawString(buf, W - 16, y + 2, 1);
    }
    tft.setTextSize(1);
    sState = pet.state; sOutput = pet.output;
    sAgent = pet.agent; sConnected = isConnected;
    tft.setTextDatum(TL_DATUM);
}

void displayRender() {
    if (millis() - lastFrame < 30) return;
    lastFrame = millis(); fi++;

    if (page != lastPage) { lastPage = page; dirty = true; }
    (page == 0) ? drawMain() : drawDetail();
}

void displayShowPage(int p) { page = p; }

void displayShowConnecting() {
    tft.fillScreen(0x0000);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim(0xFFFF, 35), 0x0000);
    tft.drawString("VIBE PET", W / 2, H / 2 - 12, 2);
    tft.setTextColor(dim(0xFFFF, 22), 0x0000);
    tft.drawString("Connecting", W / 2, H / 2 + 14, 1);
    tft.setTextDatum(TL_DATUM);
}

void displayShowConfigPortal(const char* ip) {
    tft.fillScreen(0x0000);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim(0xFFFF, 70), 0x0000);
    tft.drawString("SETUP", W / 2, 40, 4);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("Connect: " WIFI_AP_SSID, W / 2, 90, 2);
    tft.setTextColor(dim(0xFFFF, 30), 0x0000);
    tft.drawString(WIFI_AP_PASS, W / 2, 110, 1);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString("Browser:", W / 2, 145, 2);
    tft.setTextColor(dim(0xFFFF, 35), 0x0000);
    tft.drawString(ip, W / 2, 165, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayShowError(const char* msg) {
    tft.fillScreen(0x0000);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim(rgb565(205, 130, 130), 70), 0x0000);
    tft.drawString("ERROR", W / 2, H / 2 - 20, 4);
    tft.setTextColor(dim(0xFFFF, 45), 0x0000);
    tft.drawString(msg, W / 2, H / 2 + 15, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayFillScreen(uint16_t c) { tft.fillScreen(c); }

void displayShowIP(const char* ip) {
    tft.fillScreen(0x0000);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim(0xFFFF, 60), 0x0000);
    tft.drawString("VIBE PET", W / 2, 55, 2);
    tft.setTextColor(rgb565(155, 200, 175), 0x0000);
    tft.drawString(ip, W / 2, 100, 4);
    tft.setTextColor(dim(0xFFFF, 30), 0x0000);
    tft.drawString("POST /api/state", W / 2, 140, 2);
    tft.setTextDatum(TL_DATUM);
}
