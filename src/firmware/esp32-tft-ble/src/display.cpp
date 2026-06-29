#include "display.h"
#include "config.h"
#include "state.h"
#include <TFT_eSPI.h>
#include <time.h>

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite robotBuf = TFT_eSprite(&tft);
static int currentPage = 0;
static int displayPage = -1;
static unsigned long lastFrameTime = 0;
static uint8_t frameIndex = 0;
static String curState = "";
static String curOutput = "";
static String curAgent = "";
static bool staticDirty = true;
static int16_t lastTimeMin = -1;
static bool curConnected = false;

#define BG        0x0000
#define PANEL     0x0821
#define WHITE     0xFFFF
#define GRAY      0x7BCF
#define DIM       0x4208
#define CYAN      rgb565(0, 255, 255)
#define MAGENTA   rgb565(255, 0, 255)
#define RED       rgb565(255, 50, 50)
#define YELLOW    rgb565(255, 215, 0)
#define BODY      rgb565(30, 30, 50)
#define BODY_HI   rgb565(50, 50, 70)
#define NEON_G    rgb565(0, 220, 120)
#define NEON_P    rgb565(180, 0, 255)

#define SPRITE_W  140
#define SPRITE_H  130
#define CX 70
#define CY 80

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t dim(uint16_t c, uint8_t p) {
    uint8_t r = ((c >> 11) & 0x1F) * p / 100;
    uint8_t g = ((c >> 5) & 0x3F) * p / 100;
    uint8_t b = (c & 0x1F) * p / 100;
    return (r << 11) | (g << 5) | b;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void displayInit() {
    tft.init();
    tft.initDMA();
    tft.setRotation(CP_TFT_ROTATION);
    tft.setSwapBytes(true);
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    tft.fillScreen(BG);
    robotBuf.createSprite(SPRITE_W, SPRITE_H);
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
    if (s == "thinking")   return CYAN;
    if (s == "working" || s == "typing") return NEON_G;
    if (s == "building")   return rgb565(255, 140, 0);
    if (s == "juggling")   return NEON_P;
    if (s == "attention")  return YELLOW;
    if (s == "notification") return MAGENTA;
    if (s == "error")      return RED;
    if (s == "sweeping")   return rgb565(0, 180, 200);
    if (s == "sleeping")   return rgb565(60, 60, 100);
    return CYAN;
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

static void drawRobotSprite() {
    uint16_t ac = stateColor();
    const String& s = pet.state;
    robotBuf.fillSprite(BG);

    int bob = 0;
    int armL = 0, armR = 0;
    int eyeOff = 0;
    int visorH = 10;
    float pulse = (sin(frameIndex * 0.15) + 1.0) * 0.5;
    uint16_t antCol = dim(ac, 40 + (int)(pulse * 60));

    if (s == "idle") {
        bob = (int)(sin(frameIndex * 0.08) * 5);
    } else if (s == "thinking") {
        bob = -2;
        eyeOff = (frameIndex % 20 < 6) ? -7 : (frameIndex % 20 < 12) ? 7 : 0;
        armL = -5;
    } else if (s == "working" || s == "typing" || s == "building") {
        bob = (frameIndex % 6 < 3) ? -7 : 3;
        armL = (frameIndex % 4 < 2) ? -12 : 8;
        armR = (frameIndex % 4 < 2) ? 8 : -12;
    } else if (s == "juggling") {
        bob = (frameIndex % 6 < 3) ? -5 : 5;
        armL = -4; armR = -4;
    } else if (s == "attention" || s == "notification") {
        bob = -8;
        armL = -8; armR = -8;
        pulse = 1.0; antCol = ac;
    } else if (s == "error") {
        bob = (frameIndex % 3 < 2) ? 8 : -8;
    } else if (s == "sleeping") {
        bob = 10;
        visorH = 3;
        pulse = 0.15;
        antCol = dim(CYAN, 12);
    } else if (s == "sweeping") {
        bob = (frameIndex % 6 < 3) ? -3 : 3;
        armR = (frameIndex % 4 < 2) ? -14 : 14;
    }

    int bx = CX - 28;
    int by = CY - 28 + bob;

    robotBuf.fillRoundRect(bx, by, 56, 38, 4, BODY);
    robotBuf.drawRoundRect(bx, by, 56, 38, 4, dim(ac, 50));

    int hw = 44, hh = 22;
    int headX = CX - hw / 2;
    int headY = by - hh + 2;
    headY = clamp(headY, 2, 80);
    robotBuf.fillRoundRect(headX, headY, hw, hh, 4, BODY_HI);
    robotBuf.drawRoundRect(headX, headY, hw, hh, 4, dim(ac, 60));

    int vy = headY + 6;
    int vw = 32;
    int vx = CX - vw / 2;

    if (s == "sleeping") {
        robotBuf.fillRoundRect(vx, vy + 3, vw, 3, 2, dim(ac, 25));
    } else if (s == "error") {
        uint16_t ef = (frameIndex % 4 < 2) ? RED : dim(RED, 30);
        robotBuf.fillRoundRect(vx, vy, vw, visorH, 3, dim(ef, 20));
        robotBuf.drawRoundRect(vx, vy, vw, visorH, 3, ef);
        robotBuf.drawLine(vx + 5, vy + 2, vx + vw - 5, vy + visorH - 2, RED);
        robotBuf.drawLine(vx + vw - 5, vy + 2, vx + 5, vy + visorH - 2, RED);
    } else {
        robotBuf.fillRoundRect(vx, vy, vw, visorH, 3, dim(ac, 12));
        robotBuf.drawRoundRect(vx, vy, vw, visorH, 3, dim(ac, 55));
        int ew = 10, eg = 5;
        int elx = CX - eg - ew / 2 + eyeOff;
        int erx = CX + eg - ew / 2 + eyeOff;
        bool blink = (frameIndex % 80 > 74);
        int eh = blink ? 2 : 7;
        robotBuf.fillRoundRect(elx, vy + (visorH - eh) / 2, ew, eh, 2, ac);
        robotBuf.fillRoundRect(erx, vy + (visorH - eh) / 2, ew, eh, 2, ac);
        robotBuf.fillCircle(elx + ew / 2, vy + visorH / 2, 2, WHITE);
        robotBuf.fillCircle(erx + ew / 2, vy + visorH / 2, 2, WHITE);
    }

    int antBase = clamp(headY - 1, 1, 60);
    robotBuf.drawLine(CX, antBase, CX, clamp(antBase - 8, 1, antBase), dim(ac, 40));
    robotBuf.fillCircle(CX, clamp(antBase - 10, 1, antBase), 3, antCol);
    robotBuf.fillCircle(CX, clamp(antBase - 10, 1, antBase), 1, WHITE);

    int aw = 6, ah = 18;
    int armY = by + 6;
    int aLx = bx - aw - 1;
    int aRx = bx + 56 + 1;
    robotBuf.fillRoundRect(aLx, armY + armL, aw, ah, 3, BODY_HI);
    robotBuf.drawRoundRect(aLx, armY + armL, aw, ah, 3, dim(ac, 35));
    robotBuf.fillRoundRect(aRx, armY + armR, aw, ah, 3, BODY_HI);
    robotBuf.drawRoundRect(aRx, armY + armR, aw, ah, 3, dim(ac, 35));
    robotBuf.fillCircle(aLx + aw / 2, clamp(armY + armL + ah + 2, 0, SPRITE_H - 1), 3, BODY_HI);
    robotBuf.fillCircle(aRx + aw / 2, clamp(armY + armR + ah + 2, 0, SPRITE_H - 1), 3, BODY_HI);

    int lw = 10, lh = 12;
    int legY = by + 38;
    robotBuf.fillRoundRect(CX - 14, legY, lw, lh, 3, BODY_HI);
    robotBuf.drawRoundRect(CX - 14, legY, lw, lh, 3, dim(ac, 25));
    robotBuf.fillRoundRect(CX + 4, legY, lw, lh, 3, BODY_HI);
    robotBuf.drawRoundRect(CX + 4, legY, lw, lh, 3, dim(ac, 25));

    for (int i = 0; i < 3; i++) {
        uint16_t lc = dim(ac, 25 + (int)(pulse * 35 * ((i + 1) / 3.0)));
        robotBuf.fillCircle(CX - 8 + i * 8, clamp(by + 8, 0, SPRITE_H - 1), 2, lc);
    }

    if (s == "sleeping") {
        int zo = (frameIndex / 5) % 4;
        robotBuf.setTextDatum(TL_DATUM);
        robotBuf.setTextColor(dim(CYAN, 40), BG);
        robotBuf.drawString("z", CX + 24, clamp(headY - 2 - zo * 6, 1, SPRITE_H - 1), 2);
        robotBuf.drawString("z", CX + 32, clamp(headY - 12 - zo * 6, 1, SPRITE_H - 1), 1);
    } else if (s == "notification") {
        int p = (frameIndex / 3) % 4;
        for (int i = 0; i <= p; i++) {
            robotBuf.drawCircle(CX, headY + hh / 2, 6 + i * 5, dim(MAGENTA, 50 - i * 12));
        }
    } else if (s == "juggling") {
        uint16_t cols[] = {CYAN, MAGENTA, YELLOW};
        for (int i = 0; i < 3; i++) {
            int a = (frameIndex * 10 + i * 120) % 360;
            float r = a * 3.14159 / 180.0;
            int jx = clamp(CX + (int)(cos(r) * 24), 4, SPRITE_W - 4);
            int jy = clamp(headY + hh / 2 + (int)(sin(r) * 12), 4, SPRITE_H - 4);
            robotBuf.fillCircle(jx, jy, 4, cols[i]);
            robotBuf.fillCircle(jx - 1, jy - 1, 1, WHITE);
        }
    } else if (s == "sweeping") {
        int sw = (frameIndex * 8) % 40 - 20;
        int ax = clamp(aRx + aw / 2 + sw, 0, SPRITE_W - 1);
        int ay = clamp(armY + armR + ah + 4, 0, SPRITE_H - 10);
        robotBuf.drawLine(ax, ay, ax, ay + 14, dim(ac, 50));
        robotBuf.drawLine(ax - 6, ay + 14, ax + 6, ay + 14, dim(ac, 40));
    } else if (s == "error") {
        for (int i = 0; i < 4; i++) {
            int sx = clamp(bx + 6 + (frameIndex * 6 + i * 13) % 44, 0, SPRITE_W - 5);
            int sy = clamp(by + 6 + (frameIndex * 4 + i * 11) % 28, 0, SPRITE_H - 5);
            robotBuf.drawLine(sx, sy, sx + 3, sy + 3, RED);
            robotBuf.drawLine(sx + 3, sy, sx, sy + 3, RED);
        }
    } else if (s == "attention") {
        int p = (frameIndex / 2) % 4;
        for (int i = 0; i <= p; i++) {
            robotBuf.drawCircle(CX, headY + hh / 2, 26 + i * 5, dim(YELLOW, 50 - i * 12));
        }
    } else if (s == "thinking") {
        for (int i = 0; i < 3; i++) {
            int ox = CX + 26 + i * 7;
            int oy = clamp(headY - 2 - i * 8 - (frameIndex % 16), 2, SPRITE_H - 2);
            robotBuf.fillCircle(ox, oy, 3 - i, dim(CYAN, 40 - i * 12));
        }
    }

    int pushX = tft.width() / 2 - SPRITE_W / 2;
    int pushY = 48;
    robotBuf.pushSprite(pushX, pushY);
}

static void drawStaticUI() {
    int16_t w = tft.width();
    int16_t h = tft.height();
    tft.fillScreen(BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(DIM, BG);
    tft.drawString("VIBE PET", 10, 6, 2);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(isConnected ? 0x0726 : DIM, BG);
    tft.drawString(isConnected ? "BLE" : "OFF", w - 10, 6, 2);
    struct tm ti;
    if (getLocalTime(&ti)) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(GRAY, BG);
        tft.drawString(buf, w / 2, 8, 2);
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(stateColor(), BG);
    tft.drawString(stateLabel(), w / 2, h - 48, 4);
    tft.setTextColor(GRAY, BG);
    tft.drawString(pet.agent, w / 2, h - 26, 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(DIM, BG);
    String f = isConnected ? pet.output : "Waiting...";
    if (f.length() > 42) f = f.substring(0, 39) + "...";
    tft.drawString(f, 10, h - 8, 1);
}

static void drawTopBar() {
    int16_t w = tft.width();
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(DIM, BG);
    tft.drawString("VIBE PET", 10, 6, 2);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(isConnected ? 0x0726 : DIM, BG);
    tft.drawString(isConnected ? "BLE" : "OFF", w - 10, 6, 2);
}

static void drawBottomText() {
    int16_t w = tft.width();
    int16_t h = tft.height();
    tft.fillRect(0, h - 60, w, 60, BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(stateColor(), BG);
    tft.drawString(stateLabel(), w / 2, h - 48, 4);
    tft.setTextColor(GRAY, BG);
    tft.drawString(pet.agent, w / 2, h - 26, 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(DIM, BG);
    String f = isConnected ? pet.output : "Waiting...";
    if (f.length() > 42) f = f.substring(0, 39) + "...";
    tft.drawString(f, 10, h - 8, 1);
}

static void drawMainPage() {
    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent || isConnected != curConnected);
    if (staticDirty) {
        drawStaticUI();
        curState = pet.state;
        curOutput = pet.output;
        curAgent = pet.agent;
        curConnected = isConnected;
        staticDirty = false;
    } else if (changed) {
        drawTopBar();
        drawBottomText();
        curState = pet.state;
        curOutput = pet.output;
        curAgent = pet.agent;
        curConnected = isConnected;
    }
    drawRobotSprite();
}

static void drawDetailPage() {
    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent || isConnected != curConnected);
    if (!staticDirty && !changed) return;

    int16_t w = tft.width();
    int16_t h = tft.height();
    uint16_t ac = stateColor();
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
    tft.setTextColor(WHITE, BG);
    String o = pet.output.length() > 0 ? pet.output : "(no output)";
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
    curState = pet.state;
    curOutput = pet.output;
    curAgent = pet.agent;
    curConnected = isConnected;
    staticDirty = false;
}

void displayRender() {
    unsigned long now = millis();
    if (now - lastFrameTime < 80) return;
    lastFrameTime = now;
    frameIndex++;
    if (currentPage != displayPage) {
        displayPage = currentPage;
        staticDirty = true;
        curState = "";
        curOutput = "";
        curAgent = "";
        curConnected = !isConnected;
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

void displayFillScreen(uint16_t color) {
    tft.fillScreen(color);
}
