#include "display.h"
#include "config.h"
#include "state.h"
#include <TFT_eSPI.h>
#include <time.h>

// ─── TFT 对象 ────────────────────────────────────────────
static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite sprite = TFT_eSprite(&tft);

static int currentPage = 0;
static int displayPage = -1;
static unsigned long lastFrameTime = 0;
static uint8_t frameIndex = 0;
static int16_t lastTimeMin = -1;

static String curState = "";
static String curOutput = "";
static String curAgent = "";
static bool curConnected = false;
static bool staticDirty = true;

// ─── 尺寸常量 ────────────────────────────────────────────
#define W      320
#define H      240
#define TOP_H  22
#define BTM_H  24
#define PAN_W  65          // 左侧/右侧面板宽度
#define MAIN_X (PAN_W)     // 主区域起始 x = 65
#define MAIN_W (W - PAN_W * 2)  // 主区域宽度 = 190
#define MAIN_Y TOP_H
#define MAIN_H (H - TOP_H - BTM_H)  // 主区域高度 = 194

// ─── 调色板 ──────────────────────────────────────────────
#define BG       0x0000
#define PANEL    0x0821
#define WHITE    0xFFFF
#define GRAY     0x7BCF
#define DIM      0x4208
#define GHOST_BODY   rgb565(215, 228, 245)
#define GHOST_HI     rgb565(235, 242, 252)
#define GHOST_SHAD   rgb565(175, 195, 218)
#define BLUSH        rgb565(255, 175, 175)
#define EYE_W        rgb565(245, 245, 255)
#define PUPIL        rgb565(40,  50,  80)

// ─── 工具函数 ────────────────────────────────────────────
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t dim(uint16_t c, uint8_t pct) {
    uint8_t r = ((c >> 11) & 0x1F) * pct / 100;
    uint8_t g = ((c >> 5) & 0x3F) * pct / 100;
    uint8_t b = (c & 0x1F) * pct / 100;
    return (r << 11) | (g << 5) | b;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ─── 状态颜色 ────────────────────────────────────────────
static uint16_t stateColor() {
    const String& s = pet.state;
    if (s == "thinking")   return rgb565(255, 210, 50);   // 暖黄
    if (s == "working" || s == "typing") return rgb565(0, 220, 120);  // 霓虹绿
    if (s == "building")   return rgb565(255, 140, 40);   // 橙
    if (s == "juggling")   return rgb565(180, 60, 255);   // 紫
    if (s == "attention")  return rgb565(255, 200, 40);   // 金黄
    if (s == "notification") return rgb565(255, 60, 180); // 品红
    if (s == "error")      return rgb565(240, 50, 50);    // 红
    if (s == "sweeping")   return rgb565(40, 200, 220);   // 青
    if (s == "sleeping")   return rgb565(90, 100, 160);   // 暗紫
    return rgb565(0, 200, 220);  // 青蓝（idle）
}

static const char* stateLabel() {
    const String& s = pet.state;
    if (s == "thinking")   return "THINK";
    if (s == "working" || s == "typing") return "WORK";
    if (s == "building")   return "BUILD";
    if (s == "juggling")   return "JUGGLE";
    if (s == "attention")  return "DONE";
    if (s == "notification") return "NOTIFY";
    if (s == "error")      return "ERROR";
    if (s == "sweeping")   return "SWEEP";
    if (s == "sleeping")   return "SLEEP";
    return "IDLE";
}

// ─── 初始化 ──────────────────────────────────────────────
void displayInit() {
    tft.init();
    tft.initDMA();
    tft.setRotation(CP_TFT_ROTATION);
    tft.setSwapBytes(true);
    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    tft.fillScreen(BG);
    sprite.createSprite(140, 155);  // 幽灵角色画布
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(DIM, BG);
    tft.drawString("VIBE PET", W / 2, H / 2, 4);
    tft.setTextDatum(TL_DATUM);
}

void displayBrightness(int b) {
    if (b < BRIGHTNESS_MIN) b = BRIGHTNESS_MIN;
    if (b > BRIGHTNESS_MAX) b = BRIGHTNESS_MAX;
    currentBrightness = b;
    digitalWrite(TFT_BACKLIGHT_PIN, b > 0 ? HIGH : LOW);
}

// ════════════════════════════════════════════════════════════
//  幽灵角色绘制
// ════════════════════════════════════════════════════════════

// 在 sprite (140x155) 中绘制幽灵，cx/cy 是精灵内坐标
static void drawGhost(int16_t cx, int16_t cy, uint16_t ac, const String& s, uint8_t fi) {
    sprite.fillSprite(BG);

    float bob = 0;
    float pulse = (sin(fi * 0.15) + 1.0) * 0.5;  // 0~1 呼吸

    // ── 浮动偏移 ──
    if (s == "idle" || s == "thinking" || s == "working" || s == "typing" || s == "building") {
        bob = sin(fi * 0.08) * 6;
    } else if (s == "attention" || s == "notification") {
        bob = -8 + sin(fi * 0.15) * 3;
    } else if (s == "error") {
        bob = (fi % 4 < 2) ? 8 : -8;
    } else if (s == "juggling") {
        bob = sin(fi * 0.2) * 5;
    } else if (s == "sleeping") {
        bob = 10 + sin(fi * 0.05) * 2;
    } else if (s == "sweeping") {
        bob = sin(fi * 0.12) * 4;
    }

    int16_t gy = cy + (int16_t)bob;

    // ── 1. 辉光光晕 ──
    uint16_t auraCol = dim(ac, 12 + (int)(pulse * 15));
    for (int r = 75; r >= 48; r -= 4) {
        sprite.drawCircle(cx, gy - 5, r, auraCol);
    }

    // ── 2. 幽灵身体 ──
    // 主体：上半身圆形过渡到下半身椭圆
    sprite.fillCircle(cx, gy - 20, 36, GHOST_BODY);          // 头部
    sprite.fillEllipse(cx, gy + 15, 44, 50, GHOST_BODY);     // 身体

    // 高光（左上侧）
    sprite.fillCircle(cx - 18, gy - 28, 12, GHOST_HI);
    sprite.fillCircle(cx - 22, gy + 2, 10, GHOST_HI);

    // 底部波浪 (5 个小圆)
    int wavY = gy + 58;
    for (int i = 0; i < 5; i++) {
        int wx = cx - 32 + i * 16;
        int wy = wavY + (int)(sin(i * 1.2 + fi * 0.1) * 3);
        sprite.fillCircle(wx, wy, 10, GHOST_BODY);
    }
    // 底层阴影波浪
    for (int i = 0; i < 5; i++) {
        int wx = cx - 32 + i * 16;
        int wy = gy + 64 + (int)(sin(i * 1.2 + fi * 0.1) * 3);
        sprite.fillCircle(wx, wy, 8, GHOST_SHAD);
    }

    // ── 3. 手臂 ──
    int armBob = (int)(sin(fi * 0.12) * 3);
    if (s == "working" || s == "typing" || s == "building") {
        // 快速交替摆动
        int alt = (fi % 6 < 3) ? 6 : -6;
        sprite.fillEllipse(cx - 46, gy + 8 + alt, 7, 10, GHOST_BODY);
        sprite.fillEllipse(cx + 46, gy + 8 - alt, 7, 10, GHOST_BODY);
    } else if (s == "thinking") {
        sprite.fillEllipse(cx - 46, gy + 8, 7, 10, GHOST_BODY);      // 左臂正常
        sprite.fillEllipse(cx + 46, gy - 2 + armBob, 7, 10, GHOST_BODY);  // 右臂抬起来托腮
    } else if (s == "sweeping") {
        int sw = (fi * 6) % 30 - 15;
        sprite.fillEllipse(cx - 46 + sw, gy + 10, 7, 10, GHOST_BODY);   // 左臂扫动
        sprite.fillEllipse(cx + 46, gy + 8, 7, 10, GHOST_BODY);
    } else if (s == "attention" || s == "notification") {
        sprite.fillEllipse(cx - 46, gy - 2 + armBob, 7, 10, GHOST_BODY);  // 举起来
        sprite.fillEllipse(cx + 46, gy - 2 - armBob, 7, 10, GHOST_BODY);
    } else {
        sprite.fillEllipse(cx - 46, gy + 8 + armBob, 7, 10, GHOST_BODY);
        sprite.fillEllipse(cx + 46, gy + 8 - armBob, 7, 10, GHOST_BODY);
    }

    // ── 4. 眼睛 ──
    int eyeY = gy - 18;
    bool blink = (fi % 100 > 92);

    if (s == "sleeping") {
        // 闭合眼（弧线）
        sprite.drawLine(cx - 12, eyeY + 4, cx - 4, eyeY - 1, PUPIL);
        sprite.drawLine(cx - 4, eyeY - 1, cx + 4, eyeY - 1, PUPIL);
        sprite.drawLine(cx + 4, eyeY - 1, cx + 12, eyeY + 4, PUPIL);
    } else if (s == "error") {
        // X 眼
        uint16_t ec = (fi % 4 < 2) ? rgb565(255, 50, 50) : dim(rgb565(255, 50, 50), 40);
        sprite.drawLine(cx - 14, eyeY - 6, cx - 4, eyeY + 4, ec);
        sprite.drawLine(cx - 4, eyeY - 6, cx - 14, eyeY + 4, ec);
        sprite.drawLine(cx + 4, eyeY - 6, cx + 14, eyeY + 4, ec);
        sprite.drawLine(cx + 14, eyeY - 6, cx + 4, eyeY + 4, ec);
    } else {
        // 正常/表情眼
        int eyeOffX = 0;
        int eyeOffY = 0;

        if (s == "thinking") {
            // 眼睛左右看 + 向上看
            eyeOffX = (fi % 30 < 8) ? -5 : (fi % 30 < 16) ? 5 : (fi % 30 < 22) ? -2 : 0;
            eyeOffY = -2;
        } else if (s == "attention") {
            eyeOffX = 0;
            eyeOffY = -3;
        } else if (s == "notification") {
            eyeOffX = 0;
            eyeOffY = -4;
        }

        // 眼白
        if (blink) {
            // 眨眼：画细线
            sprite.drawLine(cx - 13 + eyeOffX, eyeY + 2, cx - 3 + eyeOffX, eyeY + 2, GHOST_SHAD);
            sprite.drawLine(cx + 3 - eyeOffX, eyeY + 2, cx + 13 - eyeOffX, eyeY + 2, GHOST_SHAD);
        } else {
            sprite.fillCircle(cx - 8 + eyeOffX, eyeY + eyeOffY, 9, EYE_W);
            sprite.fillCircle(cx + 8 - eyeOffX, eyeY + eyeOffY, 9, EYE_W);

            // 瞳孔（跟随视线偏移）
            sprite.fillCircle(cx - 8 + eyeOffX / 2, eyeY + 1 + eyeOffY, 5, PUPIL);
            sprite.fillCircle(cx + 8 - eyeOffX / 2, eyeY + 1 + eyeOffY, 5, PUPIL);

            // 高光
            uint16_t hl = rgb565(255, 255, 255);
            sprite.fillCircle(cx - 10 + eyeOffX / 2, eyeY - 1 + eyeOffY, 2, hl);
            sprite.fillCircle(cx + 6 - eyeOffX / 2, eyeY - 1 + eyeOffY, 2, hl);
        }
    }

    // ── 5. 腮红 ──
    if (s != "error" && s != "sleeping") {
        sprite.fillCircle(cx - 18, gy - 6, 5, BLUSH);
        sprite.fillCircle(cx + 18, gy - 6, 5, BLUSH);
    }

    // ── 6. 嘴 ──
    if (s == "attention" || s == "juggling") {
        // 笑脸
        sprite.drawCircle(cx, gy - 2, 4, PUPIL);
        sprite.fillCircle(cx, gy + 2, 3, BG);
        sprite.drawLine(cx - 3, gy + 1, cx + 3, gy + 1, PUPIL);
    }

    // ── 7. 各状态特效 ──
    if (s == "thinking") {
        // 头顶冒泡泡
        for (int i = 0; i < 3; i++) {
            int bubY = gy - 40 - i * 10 - (fi * 2 + i * 15) % 30;
            sprite.fillCircle(cx + 20 + i * 8, clamp(bubY, 2, gy - 20), 3 - i, dim(ac, 50 - i * 12));
        }
    } else if (s == "working" || s == "typing" || s == "building") {
        // 侧边运动线
        for (int i = 0; i < 3; i++) {
            int ly = gy - 10 + i * 12 + (fi * 3 + i * 8) % 20;
            int lx = cx - 50 + (i % 2) * 4;
            sprite.drawLine(lx, ly, lx + 8, ly, dim(ac, 40 + i * 10));
        }
    } else if (s == "juggling") {
        // 抛接彩球
        uint16_t cols[] = {rgb565(255, 80, 80), rgb565(80, 255, 80), rgb565(80, 80, 255)};
        for (int i = 0; i < 3; i++) {
            int a = (fi * 10 + i * 120) % 360;
            float r = a * 3.14159 / 180.0;
            int jx = clamp(cx + (int)(cos(r) * 30), 10, 130);
            int jy = clamp(gy - 35 + (int)(sin(r) * 18), 5, 140);
            sprite.fillCircle(jx, jy, 5, cols[i]);
            sprite.fillCircle(jx - 1, jy - 1, 2, WHITE);
        }
    } else if (s == "notification") {
        // 脉冲波纹
        int ri = (fi / 3) % 4;
        for (int i = 0; i <= ri; i++) {
            sprite.drawCircle(cx, gy - 8, 40 + i * 6, dim(ac, 50 - i * 12));
        }
    } else if (s == "attention") {
        // 闪光星星
        for (int i = 0; i < 3; i++) {
            int sx = cx - 30 + i * 30 + (fi * 2) % 15;
            int sy = gy - 45 - i * 5 + (fi * 3) % 20;
            sprite.drawLine(sx - 3, sy, sx + 3, sy, dim(ac, 60));
            sprite.drawLine(sx, sy - 3, sx, sy + 3, dim(ac, 60));
        }
    } else if (s == "sleeping") {
        // 浮动的 Z
        int zo = (fi / 4) % 5;
        sprite.setTextDatum(TL_DATUM);
        sprite.setTextColor(dim(rgb565(150, 160, 220), 40), BG);
        sprite.drawString("z", cx + 30, gy - 20 - zo * 6, 2);
        sprite.drawString("z", cx + 40, gy - 32 - zo * 6, 1);
        sprite.setTextDatum(TL_DATUM);
    } else if (s == "sweeping") {
        // 扫帚形状
        int sw = (fi * 5) % 40 - 20;
        int sx = cx + 48;
        int sy = gy + 12;
        sprite.drawLine(sx, sy, sx + sw, sy + 16, dim(ac, 50));
        sprite.drawLine(sx + sw - 5, sy + 16, sx + sw + 5, sy + 16, dim(ac, 40));
    } else if (s == "error") {
        // 错误火花
        for (int i = 0; i < 3; i++) {
            int ex = cx - 20 + (fi * 8 + i * 25) % 40;
            int ey = gy - 10 + (fi * 6 + i * 20) % 30;
            sprite.drawLine(ex, ey, ex + 4, ey + 4, rgb565(255, 100, 50));
            sprite.drawLine(ex + 4, ey, ex, ey + 4, rgb565(255, 100, 50));
        }
    }

    // ── 8. 浮动装饰粒子 ──
    if (s != "error" && s != "sleeping") {
        for (int i = 0; i < 4; i++) {
            int px = 10 + (i * 37 + fi * 3) % 120;
            int py = 5 + (i * 23 + fi * 2) % 140;
            sprite.fillCircle(px, py, 1, dim(ac, 20 + (int)(pulse * 15)));
        }
    }

    sprite.pushSprite(MAIN_X + (MAIN_W - 140) / 2, MAIN_Y + (MAIN_H - 155) / 2);
}

// ════════════════════════════════════════════════════════════
//  左侧面板
// ════════════════════════════════════════════════════════════
static void drawLeftPanel(uint16_t ac) {
    int16_t px = 0;
    int16_t py = MAIN_Y;

    // 底色
    tft.fillRect(px, py, PAN_W, MAIN_H, BG);

    // 竖线装饰
    tft.drawLine(px + 8, py + 8, px + 8, py + MAIN_H - 8, dim(ac, 25));
    tft.drawLine(px + 10, py + 8, px + 10, py + MAIN_H - 8, dim(ac, 12));

    // 状态名（竖排，每个字一行）
    const char* label = stateLabel();
    int ly = py + 20;
    tft.setTextDatum(TC_DATUM);
    for (int i = 0; label[i] != '\0'; i++) {
        tft.setTextColor(dim(ac, 70), BG);
        tft.drawChar(label[i], px + 22, ly, 2);
        ly += 18;
    }
    tft.setTextDatum(TL_DATUM);

    // 状态色指示条（竖条，脉动高度）
    float p = (sin(frameIndex * 0.1) + 1.0) * 0.5;
    int barH = 30 + (int)(p * 20);
    int barY = py + MAIN_H - 50;
    tft.fillRoundRect(px + 6, barY - barH + 20, 6, barH, 2, ac);

    // 底部装饰点
    for (int i = 0; i < 3; i++) {
        int dotY = barY + 28 + i * 8;
        tft.fillCircle(px + 9, dotY, 2, dim(ac, 30 + i * 10));
    }
}

// ════════════════════════════════════════════════════════════
//  右侧面板
// ════════════════════════════════════════════════════════════
static void drawRightPanel(uint16_t ac) {
    int16_t px = W - PAN_W;
    int16_t py = MAIN_Y;

    tft.fillRect(px, py, PAN_W, MAIN_H, BG);

    // 竖线装饰
    tft.drawLine(px + PAN_W - 8, py + 8, px + PAN_W - 8, py + MAIN_H - 8, dim(ac, 25));

    // Agent 名
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim(ac, 80), BG);
    String agent = pet.agent.length() > 0 ? pet.agent : "N/A";
    if (agent.length() > 8) agent = agent.substring(0, 7) + ".";
    tft.drawString(agent, px + PAN_W / 2, py + 25, 2);

    // 分隔线
    tft.drawLine(px + 8, py + 38, px + PAN_W - 8, py + 38, dim(ac, 20));

    // Persona 名
    tft.setTextColor(dim(WHITE, 60), BG);
    String pName = pet.personaName.length() > 0 ? pet.personaName : "---";
    if (pName.length() > 6) pName = pName.substring(0, 5) + ".";
    tft.drawString(pName, px + PAN_W / 2, py + 55, 2);

    tft.drawLine(px + 8, py + 68, px + PAN_W - 8, py + 68, dim(ac, 20));

    // 连接状态指示灯
    int dotY = py + 88;
    uint16_t dotCol = isConnected ? rgb565(0, 220, 120) : DIM;
    tft.fillCircle(px + PAN_W / 2, dotY, 5, dotCol);
    tft.setTextColor(dim(WHITE, 50), BG);
    tft.drawString(isConnected ? "LIVE" : "OFF", px + PAN_W / 2, dotY + 14, 1);

    // 底部动画装饰（跳动小点）
    for (int i = 0; i < 3; i++) {
        float bp = (sin(frameIndex * 0.12 + i * 2.0) + 1.0) * 0.5;
        int by = py + MAIN_H - 20 - (int)(bp * 15);
        tft.fillCircle(px + 12 + i * 20, by, 3, dim(ac, 30 + (int)(bp * 30)));
    }

    tft.setTextDatum(TL_DATUM);
}

// ════════════════════════════════════════════════════════════
//  顶栏
// ════════════════════════════════════════════════════════════
static void drawTopBar(uint16_t ac) {
    tft.fillRect(0, 0, W, TOP_H, BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(dim(WHITE, 70), BG);
    tft.drawString("VIBE PET [WiFi]", 10, 5, 2);

    // 时间（中）
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(GRAY, BG);
    struct tm ti;
    if (getLocalTime(&ti)) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        int16_t curMin = ti.tm_hour * 60 + ti.tm_min;
        if (curMin != lastTimeMin) {
            tft.fillRoundRect(W / 2 - 28, 3, 56, 16, 4, BG);
            tft.drawString(buf, W / 2, 10, 2);
            lastTimeMin = curMin;
        }
    }

    // 连接状态（右）
    tft.setTextDatum(TR_DATUM);
    uint16_t connCol = isConnected ? rgb565(0, 220, 120) : DIM;
    tft.setTextColor(connCol, BG);
    tft.drawString(isConnected ? "● LIVE" : "○ OFF", W - 10, 5, 2);

    // 底部分隔线
    tft.drawLine(0, TOP_H - 1, W, TOP_H - 1, dim(ac, 15));
    tft.setTextDatum(TL_DATUM);
}

// ════════════════════════════════════════════════════════════
//  底栏
// ════════════════════════════════════════════════════════════
static void drawBottomBar(uint16_t ac) {
    int16_t by = H - BTM_H;
    tft.fillRect(0, by, W, BTM_H, BG);
    tft.drawLine(0, by, W, by, dim(ac, 15));

    // 左侧：状态标签
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(ac, BG);
    String label = isConnected ? String(stateLabel()) : "WAITING";
    tft.drawString(label.c_str(), 8, by + 5, 1);

    // 右侧：输出文字
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(dim(WHITE, 50), BG);
    String out = isConnected ? pet.output : "No connection...";
    if (out.length() > 36) out = out.substring(0, 33) + "...";
    tft.drawString(out.c_str(), W - 8, by + 5, 1);

    tft.setTextDatum(TL_DATUM);
}

// ════════════════════════════════════════════════════════════
//  页面渲染
// ════════════════════════════════════════════════════════════

static void drawMainPage() {
    uint16_t ac = stateColor();
    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent || isConnected != curConnected);

    if (staticDirty || changed) {
        drawTopBar(ac);
        drawLeftPanel(ac);
        drawRightPanel(ac);
        drawBottomBar(ac);
        curState = pet.state;
        curOutput = pet.output;
        curAgent = pet.agent;
        curConnected = isConnected;
        staticDirty = false;
    }

    // 幽灵角色动画（每帧重绘）
    int16_t spriteCx = 70;    // 140/2
    int16_t spriteCy = 82;    // 155/2 + 5
    drawGhost(spriteCx, spriteCy, ac, pet.state, frameIndex);
}

static void drawDetailPage() {
    uint16_t ac = stateColor();
    bool changed = (pet.state != curState || pet.output != curOutput || pet.agent != curAgent || isConnected != curConnected);

    if (!staticDirty && !changed) return;

    tft.fillScreen(BG);

    int16_t cx = W / 2, cy = 20;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(dim(WHITE, 60), BG);
    tft.drawString("=== AGENT ===", 16, cy, 1);
    tft.setTextColor(WHITE, BG);
    tft.drawString(pet.agent, 16, cy + 16, 2);

    tft.setTextColor(dim(WHITE, 60), BG);
    tft.drawString("=== STATUS ===", 16, cy + 44, 1);
    tft.setTextColor(ac, BG);
    tft.drawString(stateLabel(), 16, cy + 60, 2);

    tft.setTextColor(dim(WHITE, 60), BG);
    tft.drawString("=== PERSONA ===", 16, cy + 88, 1);
    tft.setTextColor(rgb565(183, 126, 83), BG);
    tft.drawString(pet.personaName, 16, cy + 104, 2);

    tft.drawFastHLine(16, cy + 130, W - 32, dim(ac, 20));

    tft.setTextColor(dim(WHITE, 60), BG);
    tft.drawString("=== OUTPUT ===", 16, cy + 138, 1);
    tft.setTextColor(dim(WHITE, 70), BG);
    String o = pet.output.length() > 0 ? pet.output : "(none)";
    int16_t y = cy + 156;
    int16_t maxY = H - 30;
    // 智能折行
    while (o.length() > 0 && y < maxY) {
        // 检测中文字符宽度
        int cut = 0, px = 0;
        for (size_t i = 0; i < o.length() && px < 240; i++) {
            if ((o[i] & 0x80) != 0) { px += 18; i++; }  // 中文字符宽估
            else px += 10;
            cut = i + 1;
        }
        String ln = o.substring(0, cut);
        tft.drawString(ln, 16, y, 1);
        o = o.substring(cut);
        y += 12;
    }

    // 右上角时间
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(GRAY, BG);
    struct tm ti;
    if (getLocalTime(&ti)) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        tft.drawString(buf, W - 16, cy + 2, 2);
    }

    tft.setTextDatum(TL_DATUM);
    curState = pet.state;
    curOutput = pet.output;
    curAgent = pet.agent;
    curConnected = isConnected;
    staticDirty = false;
}

// ════════════════════════════════════════════════════════════
//  公开接口
// ════════════════════════════════════════════════════════════

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
    tft.drawString("VIBE PET", W / 2, H / 2 - 20, 4);
    tft.setTextColor(GRAY, BG);
    tft.drawString("Connecting...", W / 2, H / 2 + 15, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayShowConfigPortal(const char* ip) {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(rgb565(233, 69, 96), BG);
    tft.drawString("SETUP", W / 2, 40, 4);
    tft.setTextColor(WHITE, BG);
    tft.drawString("Connect to WiFi:", W / 2, 85, 2);
    tft.setTextColor(rgb565(0, 220, 120), BG);
    tft.drawString(WIFI_AP_SSID, W / 2, 110, 2);
    tft.drawString(WIFI_AP_PASS, W / 2, 128, 1);
    tft.setTextColor(WHITE, BG);
    tft.drawString("Open browser:", W / 2, 155, 2);
    tft.setTextColor(DIM, BG);
    tft.drawString(ip, W / 2, 175, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayShowError(const char* msg) {
    tft.fillScreen(BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(rgb565(240, 50, 50), BG);
    tft.drawString("ERROR", W / 2, H / 2 - 20, 4);
    tft.setTextColor(WHITE, BG);
    tft.drawString(msg, W / 2, H / 2 + 15, 2);
    tft.setTextDatum(TL_DATUM);
}

void displayFillScreen(uint16_t color) {
    tft.fillScreen(color);
}
