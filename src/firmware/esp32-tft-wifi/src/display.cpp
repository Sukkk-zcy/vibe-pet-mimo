#include "display.h"
#include "config.h"
#include "state.h"
#include <TFT_eSPI.h>
#include <time.h>

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite animBuf = TFT_eSprite(&tft);
static int currentPage = 0;
static int displayPage = -1;
static unsigned long lastFrameTime = 0;
static uint8_t frameIndex = 0;
static String curState = "";
static String curOutput = "";
static String curAgent = "";
static bool staticDirty = true;
static int16_t animBufX = 0;
static int16_t lastTimeMin = -1;
static int16_t animBufY = 0;

#define BG        0x0000
#define PANEL     0x0821
#define WHITE     0xFFFF
#define GRAY      0x7BCF
#define DIM       0x4208

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t dim(uint16_t c, uint8_t p) {
    uint8_t r = ((c >> 11) & 0x1F) * p / 100;
    uint8_t g = ((c >> 5) & 0x3F) * p / 100;
    uint8_t b = (c & 0x1F) * p / 100;
    return (r << 11) | (g << 5) | b;
}

void displayInit() {
    tft.init();
    tft.initDMA();
    tft.setRotation(CP_TFT_ROTATION);
    tft.setSwapBytes(true);
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    tft.fillScreen(BG);
    animBuf.createSprite(100, 100);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(DIM, BG);
    tft.drawString("VIBE PET", tft.width() / 2, tft.height() / 2, 4);
    tft.setTextDatum(TL_DATUM);
}

void displayBrightness(int b) {
    if (b < BRIGHTNESS_MIN) b = BRIGHTNESS_MIN;
    if (b > BRIGHTNESS_MAX) b = BRIGHTNESS_MAX;
    currentBrightness = b;
    digitalWrite(TFT_BACKLIGHT_PIN, b > 0 ? HIGH : LOW);
}

static uint16_t stateColor() {
    const String& s = pet.state;
    if (s == "thinking")   return 0xC600;
    if (s == "working" || s == "typing") return 0x0726;
    if (s == "building")   return rgb565(220, 100, 40);
    if (s == "juggling")   return rgb565(130, 100, 220);
    if (s == "attention")  return 0xFBE0;
    if (s == "notification") return rgb565(220, 60, 60);
    if (s == "error")      return 0xE800;
    if (s == "sweeping")   return rgb565(40, 160, 180);
    if (s == "sleeping")   return DIM;
    return 0x2D92;
}

static const char* stateLabel() {
    const String& s = pet.state;
    if (s == "thinking")   return "Thinking";
    if (s == "working" || s == "typing") return "Working";
    if (s == "building")   return "Building";
    if (s == "juggling")   return "Juggling";
    if (s == "attention")  return "Done";
    if (s == "notification") return "Notify";
    if (s == "error")      return "Error";
    if (s == "sweeping")   return "Cleaning";
    if (s == "sleeping")   return "Sleeping";
    return "Idle";
}

static void drawCat(int16_t cx, int16_t cy) {
    uint16_t ac = stateColor();
    const String& s = pet.state;
    uint16_t fur = rgb565(160, 110, 50);
    uint16_t belly = rgb565(220, 185, 130);
    uint16_t inner = rgb565(200, 140, 80);

    animBufX = cx - 50;
    animBufY = cy - 48;
    animBuf.fillSprite(BG);

    int hx = 50, hy = 38;
    int bodyBob = 0;
    int earTilt = 0;
    int legPhase = frameIndex % 16;
    int tailPhase = frameIndex % 32;
    float tailAngle = sin(tailPhase * 3.14159 / 16.0) * 20;
    int armL = 0, armR = 0;

    if (s == "idle") {
        bodyBob = sin(frameIndex * 3.14159 / 20.0) * 3;
        tailAngle = sin(frameIndex * 3.14159 / 30.0) * 15;
    } else if (s == "thinking") {
        bodyBob = -3;
        earTilt = (frameIndex % 8 < 4) ? -2 : 2;
        armR = -8;
        tailAngle = 25;
    } else if (s == "working" || s == "typing" || s == "building") {
        bodyBob = (legPhase < 8) ? -4 : 2;
        earTilt = (frameIndex % 2) ? 2 : -2;
        armL = (frameIndex % 4 < 2) ? -6 : 4;
        armR = (frameIndex % 4 < 2) ? 4 : -6;
        tailAngle = sin(tailPhase * 3.14159 / 8.0) * 30;
    } else if (s == "juggling") {
        bodyBob = (frameIndex % 3) - 1;
        earTilt = (frameIndex % 4 < 2) ? 3 : -3;
        tailAngle = 20;
    } else if (s == "attention") {
        bodyBob = -5;
        earTilt = -3;
        tailAngle = 35;
    } else if (s == "notification") {
        bodyBob = (frameIndex % 4 < 2) ? -3 : 0;
        earTilt = -4;
    } else if (s == "error") {
        bodyBob = (frameIndex % 2) ? 3 : -3;
    } else if (s == "sleeping") {
        bodyBob = 6;
        tailAngle = 5;
    } else if (s == "sweeping") {
        bodyBob = (frameIndex % 4 < 2) ? -2 : 2;
        armR = (frameIndex % 4 < 2) ? -5 : 5;
    }

    int earY = hy - 18 + bodyBob;
    animBuf.fillCircle(hx - 14, earY + earTilt, 10, fur);
    animBuf.fillCircle(hx - 14, earY + earTilt, 5, inner);
    animBuf.fillCircle(hx + 14, earY - earTilt, 10, fur);
    animBuf.fillCircle(hx + 14, earY - earTilt, 5, inner);

    int headY = hy + bodyBob;
    animBuf.fillCircle(hx, headY, 18, fur);
    animBuf.fillCircle(hx, headY + 4, 13, belly);

    int eyeL = hx - 7, eyeR = hx + 7, eyeY = headY + 1;

    if (s == "sleeping") {
        animBuf.drawLine(eyeL - 4, eyeY + 1, eyeL + 4, eyeY - 1, fur);
        animBuf.drawLine(eyeR - 4, eyeY - 1, eyeR + 4, eyeY + 1, fur);
    } else if (s == "error") {
        animBuf.drawLine(eyeL - 4, eyeY - 4, eyeL + 4, eyeY + 4, 0xF800);
        animBuf.drawLine(eyeL + 4, eyeY - 4, eyeL - 4, eyeY + 4, 0xF800);
        animBuf.drawLine(eyeR - 4, eyeY - 4, eyeR + 4, eyeY + 4, 0xF800);
        animBuf.drawLine(eyeR + 4, eyeY - 4, eyeR - 4, eyeY + 4, 0xF800);
    } else {
        int lx = 0;
        if (s == "thinking") lx = (frameIndex % 6 < 3) ? -3 : 3;
        animBuf.fillCircle(eyeL + lx, eyeY, 5, WHITE);
        animBuf.fillCircle(eyeR + lx, eyeY, 5, WHITE);
        animBuf.fillCircle(eyeL + lx + 1, eyeY - 1, 3, fur);
        animBuf.fillCircle(eyeR + lx + 1, eyeY - 1, 3, fur);
        animBuf.fillCircle(eyeL + lx + 2, eyeY - 2, 1, WHITE);
        animBuf.fillCircle(eyeR + lx + 2, eyeY - 2, 1, WHITE);
    }

    animBuf.fillCircle(hx, headY + 8, 3, rgb565(220, 120, 100));

    int my = headY + 13;
    if (s == "error") {
        animBuf.drawLine(hx - 4, my + 3, hx, my, 0xF800);
        animBuf.drawLine(hx, my, hx + 4, my + 3, 0xF800);
    } else if (s == "attention" || s == "notification") {
        animBuf.fillCircle(hx, my + 2, 3, fur);
    } else {
        animBuf.drawLine(hx, my, hx - 4, my + 3, fur);
        animBuf.drawLine(hx, my, hx + 4, my + 3, fur);
    }

    int wOff = (s == "thinking") ? (frameIndex % 6 < 3 ? -2 : 2) : 0;
    animBuf.drawLine(hx - 18, headY + 7, hx - 28, headY + 4 + wOff, dim(ac, 50));
    animBuf.drawLine(hx - 18, headY + 9, hx - 28, headY + 9 + wOff, dim(ac, 50));
    animBuf.drawLine(hx + 18, headY + 7, hx + 28, headY + 4 - wOff, dim(ac, 50));
    animBuf.drawLine(hx + 18, headY + 9, hx + 28, headY + 9 - wOff, dim(ac, 50));

    int bodyTop = headY + 18;
    animBuf.fillRoundRect(hx - 13, bodyTop, 26, 18, 6, belly);
    animBuf.drawRoundRect(hx - 13, bodyTop, 26, 18, 6, fur);

    animBuf.drawLine(hx - 13, bodyTop + 4, hx - 20, bodyTop + 10 + armL, fur);
    animBuf.fillCircle(hx - 21, bodyTop + 11 + armL, 3, fur);
    animBuf.drawLine(hx + 13, bodyTop + 4, hx + 20, bodyTop + 10 + armR, fur);
    animBuf.fillCircle(hx + 21, bodyTop + 11 + armR, 3, fur);

    int footY = bodyTop + 16;
    int legAnim1 = 0, legAnim2 = 0;
    if (s == "working" || s == "typing" || s == "building") {
        legAnim1 = (legPhase < 8) ? 3 : -3;
        legAnim2 = (legPhase < 8) ? -3 : 3;
    } else if (s == "attention") {
        legAnim1 = -2;
        legAnim2 = -2;
    }
    animBuf.fillEllipse(hx - 7, footY + 4 + legAnim1, 6, 4, fur);
    animBuf.fillEllipse(hx + 7, footY + 4 + legAnim2, 6, 4, fur);

    int tailX = hx + 13;
    int tailBaseY = bodyTop + 10;
    int tailMidX = tailX + 10;
    int tailMidY = tailBaseY - 10;
    int tailEndX = tailMidX + 8;
    int tailEndY = tailMidY - 6 + (int)(tailAngle * 0.4);
    animBuf.drawLine(tailX, tailBaseY, tailMidX, tailMidY, fur);
    animBuf.drawLine(tailMidX, tailMidY, tailEndX, tailEndY, fur);
    animBuf.fillCircle(tailEndX + 3, tailEndY - 3, 3, fur);

    if (s == "sleeping") {
        int zo = (frameIndex / 3) % 4;
        animBuf.setTextDatum(TL_DATUM);
        animBuf.setTextColor(dim(ac, 70), BG);
        animBuf.drawString("z", hx + 22, headY - 8 - zo * 4, 2);
        animBuf.drawString("z", hx + 30, headY - 18 - zo * 4, 1);
        animBuf.setTextDatum(TL_DATUM);
    } else if (s == "notification") {
        int p = (frameIndex / 2) % 3;
        animBuf.drawLine(hx, headY - 20 + earTilt, hx, headY - 28 - p + earTilt, ac);
        animBuf.fillCircle(hx, headY - 30 - p + earTilt, 3, ac);
    } else if (s == "juggling") {
        uint16_t cols[] = {0xF800, 0x07E0, 0x001F};
        for (int i = 0; i < 3; i++) {
            int a = (frameIndex * 6 + i * 120) % 360;
            float r = a * 3.14159 / 180.0;
            animBuf.fillCircle(hx + (int)(cos(r) * 22), headY - 16 + (int)(sin(r) * 10), 4, cols[i]);
        }
    } else if (s == "sweeping") {
        int sw = (frameIndex * 8) % 50 - 25;
        animBuf.drawLine(hx + 21, bodyTop + 10 + armR, hx + 21 + sw, bodyTop + 22, dim(ac, 50));
        animBuf.drawLine(hx + 21 + sw - 4, bodyTop + 22, hx + 21 + sw + 4, bodyTop + 22, dim(ac, 40));
    }

    animBuf.pushSprite(animBufX, animBufY);
}

static void drawMainPage() {
    int16_t w = tft.width();
    int16_t h = tft.height();
    int16_t animCx = w / 2;
    int16_t animCy = h / 2 - 10;

    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent);

    if (staticDirty || changed) {
        tft.fillScreen(BG);

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(DIM, BG);
        tft.drawString("VIBE PET", 10, 6, 2);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(isConnected ? 0x0726 : DIM, BG);
        tft.drawString(isConnected ? "LIVE" : "OFF", w - 10, 6, 2);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(stateColor(), BG);
        tft.drawString(stateLabel(), w / 2, h - 50, 4);

        tft.setTextColor(GRAY, BG);
        tft.drawString(pet.agent, w / 2, h - 28, 2);

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(DIM, BG);
        String f = isConnected ? pet.output : "Waiting...";
        if (f.length() > 42) f = f.substring(0, 39) + "...";
        tft.drawString(f, 10, h - 10, 1);

        curState = pet.state;
        curOutput = pet.output;
        curAgent = pet.agent;
        staticDirty = false;
    }

    struct tm ti;
    if (getLocalTime(&ti)) {
        int16_t curMin = ti.tm_hour * 60 + ti.tm_min;
        if (curMin != lastTimeMin) {
            tft.fillRoundRect(w / 2 - 30, 2, 60, 14, 3, BG);
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(GRAY, BG);
            tft.drawString(buf, w / 2, 8, 2);
            tft.setTextDatum(TL_DATUM);
            lastTimeMin = curMin;
        }
    }

    drawCat(animCx, animCy);
}

static void drawDetailPage() {
    int16_t w = tft.width();
    int16_t h = tft.height();
    uint16_t ac = stateColor();

    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent);

    if (staticDirty || changed) {
        tft.fillScreen(BG);
        tft.setTextDatum(TL_DATUM);

        tft.setTextColor(DIM, BG);
        tft.drawString("AGENT", 16, 10, 1);
        tft.setTextColor(WHITE, BG);
        tft.drawString(pet.agent, 16, 24, 2);

        tft.setTextColor(DIM, BG);
        tft.drawString("STATUS", 16, 50, 1);
        tft.setTextColor(ac, BG);
        tft.drawString(stateLabel(), 16, 64, 2);

        tft.setTextColor(DIM, BG);
        tft.drawString("PERSONA", 16, 90, 1);
        tft.setTextColor(rgb565(183, 126, 83), BG);
        tft.drawString(pet.personaName, 16, 104, 2);

        tft.drawFastHLine(16, 128, w - 32, PANEL);

        tft.setTextColor(DIM, BG);
        tft.drawString("OUTPUT", 16, 136, 1);
        tft.setTextColor(GRAY, BG);
        String o = pet.output.length() > 0 ? pet.output : "---";
        int16_t y = 152;
        while (o.length() > 0 && y < h - 16) {
            String ln = o.substring(0, 32);
            tft.drawString(ln, 16, y, 1);
            o = o.substring(ln.length());
            y += 12;
        }

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(DIM, BG);
        struct tm ti;
        if (getLocalTime(&ti)) {
            char buf[6];
            snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            tft.drawString(buf, w - 16, 10, 2);
        }
        tft.setTextDatum(TL_DATUM);

        curState = pet.state;
        curOutput = pet.output;
        curAgent = pet.agent;
        staticDirty = false;
    }
}

void displayRender() {
    unsigned long now = millis();
    if (now - lastFrameTime < 50) return;
    lastFrameTime = now;
    frameIndex++;

    if (currentPage != displayPage) {
        displayPage = currentPage;
        staticDirty = true;
        curState = "";
        curOutput = "";
        curAgent = "";
    }

    switch (currentPage) {
        case 0: drawMainPage(); break;
        case 1: drawDetailPage(); break;
        default: currentPage = 0; drawMainPage(); break;
    }
}

void displayShowPage(int page) {
    currentPage = page;
}

void displayShowConnecting() {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(DIM, BG);
    tft.drawString("Connecting...", tft.width() / 2, tft.height() / 2, 4);
    tft.setTextDatum(TL_DATUM);
}

void displayShowConfigPortal(const char* ip) {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(rgb565(233, 69, 96), BG);
    tft.drawString("SETUP", tft.width() / 2, 40, 4);
    tft.setTextColor(WHITE, BG);
    tft.drawString("Connect to WiFi:", tft.width() / 2, 85, 2);
    tft.setTextColor(0x0726, BG);
    tft.drawString(WIFI_AP_SSID, tft.width() / 2, 110, 2);
    tft.setTextColor(WHITE, BG);
    tft.drawString("Open browser:", tft.width() / 2, 145, 2);
    tft.setTextColor(DIM, BG);
    tft.drawString(ip, tft.width() / 2, 170, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayShowError(const char* msg) {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xE800, BG);
    tft.drawString("ERROR", tft.width() / 2, tft.height() / 2 - 20, 4);
    tft.setTextColor(WHITE, BG);
    tft.drawString(msg, tft.width() / 2, tft.height() / 2 + 20, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayFillScreen(uint16_t color) {
    tft.fillScreen(color);
}
