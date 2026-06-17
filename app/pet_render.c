#include "pet_render.h"
#include "pet_command.h"
#include <string.h>
#include <stdio.h>

static uint8_t  g_anim_tick = 0;
static char     g_result_msg[40] = "";
static uint16_t g_result_timer = 0;

/* --- Cat stage colors --- */
static uint16_t CatColor(void)
{
    switch (g_pet.stage) {
        case STAGE_BABY:    return CAT_BABY_COLOR;
        case STAGE_GROWING: return CAT_GROW_COLOR;
        case STAGE_MATURE:  return CAT_MATURE_COLOR;
        case STAGE_SECRET:  return CAT_SECRET_COLOR;
        default:            return CAT_BABY_COLOR;
    }
}

static uint16_t Lighten(uint16_t c)
{
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5)  & 0x3F;
    uint8_t b5 = c & 0x1F;
    r5 = (r5 < 27) ? r5 + 5 : 31;
    g6 = (g6 < 54) ? g6 + 10 : 63;
    b5 = (b5 < 27) ? b5 + 5 : 31;
    return ((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5;
}

static uint16_t Darken(uint16_t c, uint8_t amt)
{
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5)  & 0x3F;
    uint8_t b5 = c & 0x1F;
    r5 = (r5 > amt) ? r5 - amt : 0;
    g6 = (g6 > amt*2) ? g6 - amt*2 : 0;
    b5 = (b5 > amt) ? b5 - amt : 0;
    return ((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5;
}

/* Blend two colors by ratio (0-255, where 0 = all c1, 255 = all c2) */
static uint16_t Blend(uint16_t c1, uint16_t c2, uint8_t ratio)
{
    uint8_t r = (((c1 >> 11) * (255 - ratio) + (c2 >> 11) * ratio) / 255);
    uint8_t g = ((((c1 >> 5) & 0x3F) * (255 - ratio) + ((c2 >> 5) & 0x3F) * ratio) / 255);
    uint8_t b = (((c1 & 0x1F) * (255 - ratio) + (c2 & 0x1F) * ratio) / 255);
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

/* --- Progress bar --- */
static void DrawBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    uint8_t pct, uint16_t fg, uint16_t bg)
{
    LCD_Fill(x, y, x + w - 1, y + h - 1, bg);
    uint16_t fw = (uint16_t)((uint32_t)w * pct / 100);
    if (fw > 1) LCD_Fill(x + 1, y + 1, x + fw - 1, y + h - 2, fg);
}

/* --- Emoji mood --- */
static void DrawMoodIcon(uint16_t x, uint16_t y, PetMood mood)
{
    const char *emoji;
    uint16_t color;
    switch (mood) {
        case MOOD_HAPPY:  emoji = ":3"; color = YELLOW;  break;
        case MOOD_SAD:    emoji = ":("; color = LIGHTBLUE; break;
        case MOOD_HUNGRY: emoji = ":P"; color = GRED;    break;
        case MOOD_SLEEPY: emoji = "-_-";color = LGRAY;   break;
        case MOOD_ANGRY:  emoji = ">_<";color = RED;     break;
        case MOOD_SICK:   emoji = "x_x";color = MAGENTA; break;
        default:          emoji = ":3"; color = YELLOW;  break;
    }
    LCD_String(x, y, (char*)emoji, 16, color, BLACK);
}

/* ================================================================
 * CAT FACE — round head, triangle ears, big eyes, whiskers
 * ================================================================ */
static void DrawCat(uint16_t cx, uint16_t cy, uint16_t scale,
                     uint16_t color, uint8_t frame)
{
    uint16_t lighter  = Lighten(color);
    uint16_t darker   = Darken(color, 2);
    uint16_t darkest  = Darken(color, 5);

    /* Bob animation */
    uint8_t  f4  = frame % 4;
    int16_t  bob = (f4 < 2) ? 0 : 2 * scale;

    /* --- Head (big round circle) --- */
    uint16_t head_r = 35 * scale;  /* face radius */
    uint16_t head_y = cy + bob;

    /* Head fill */
    LCD_Fill(cx - head_r, head_y - head_r, cx + head_r, head_y + head_r, color);

    /* Head outline */
    LCD_Circle(cx, head_y, head_r, CAT_OUTLINE);

    /* Head highlight (lighter patch top-left) */
    uint16_t hl_r = head_r / 2;
    LCD_Fill(cx - head_r/2 - hl_r/2, head_y - head_r/2,
             cx - head_r/2 + hl_r/2, head_y, lighter);
    LCD_Circle(cx - head_r/2, head_y - head_r/4, hl_r/2, lighter);

    /* --- Ears (triangles on top) --- */
    uint16_t ear_w  = 14 * scale;  /* ear base half-width */
    uint16_t ear_h  = 22 * scale;  /* ear height */
    uint16_t ear_dx = head_r * 7 / 10; /* distance from center to ear base */
    uint16_t ear_y  = head_y - head_r + 4 * scale;

    for (uint8_t side = 0; side < 2; side++) {
        int16_t dir = (side == 0) ? -1 : 1;
        uint16_t ex = cx + dir * ear_dx;

        /* Outer ear (triangle) */
        LCD_Fill(ex - ear_w, ear_y, ex + ear_w, ear_y + ear_h, color);
        LCD_Line(ex, ear_y - 2*scale, ex + ear_w, ear_y + ear_h, color);
        LCD_Line(ex, ear_y - 2*scale, ex - ear_w, ear_y + ear_h, color);
        LCD_Fill(ex - ear_w + 1, ear_y + 1, ex + ear_w - 1, ear_y + ear_h - 1, color);

        /* Inner ear pink triangle */
        uint16_t in_w  = ear_w * 2 / 3;
        uint16_t in_h  = ear_h * 2 / 3;
        LCD_Fill(ex - in_w, ear_y + ear_h - in_h, ex + in_w, ear_y + ear_h, CAT_EAR_INNER);
        LCD_Fill(ex - in_w + 1, ear_y + ear_h - in_h + 1, ex + in_w - 1, ear_y + ear_h - 1, CAT_EAR_INNER);

        /* Ear outline */
        LCD_Line(ex, ear_y - 2*scale, ex + ear_w, ear_y + ear_h, CAT_OUTLINE);
        LCD_Line(ex, ear_y - 2*scale, ex - ear_w, ear_y + ear_h, CAT_OUTLINE);
    }

    /* --- Eyes --- */
    uint16_t eye_spacing = 12 * scale;
    uint16_t eye_y = head_y - 3 * scale;
    uint16_t eye_r = 8 * scale;
    uint16_t lx = cx - eye_spacing;
    uint16_t rx = cx + eye_spacing;

    /* Blink */
    uint8_t blink = (frame % 18 >= 16);

    if (g_pet.current_mood == MOOD_SLEEPY || blink) {
        /* Closed = line */
        LCD_Line(lx - eye_r, eye_y, lx + eye_r, eye_y, CAT_OUTLINE);
        LCD_Line(rx - eye_r, eye_y, rx + eye_r, eye_y, CAT_OUTLINE);
    } else if (g_pet.current_mood == MOOD_ANGRY) {
        /* \\ // */
        LCD_Line(lx - eye_r, eye_y - eye_r, lx + eye_r, eye_y + eye_r, CAT_OUTLINE);
        LCD_Line(rx - eye_r, eye_y + eye_r, rx + eye_r, eye_y - eye_r, CAT_OUTLINE);
    } else {
        /* Round eyes with green iris and slit pupil */
        /* White */
        LCD_Fill(lx - eye_r, eye_y - eye_r, lx + eye_r, eye_y + eye_r, CAT_EYE_WHITE);
        LCD_Fill(rx - eye_r, eye_y - eye_r, rx + eye_r, eye_y + eye_r, CAT_EYE_WHITE);
        /* Outline */
        LCD_Circle(lx, eye_y, eye_r, CAT_OUTLINE);
        LCD_Circle(rx, eye_y, eye_r, CAT_OUTLINE);
        /* Iris (green circle) */
        uint16_t iris_r = eye_r * 2 / 3;
        LCD_Fill(lx - iris_r, eye_y - iris_r, lx + iris_r, eye_y + iris_r, CAT_EYE_GREEN);
        LCD_Fill(rx - iris_r, eye_y - iris_r, rx + iris_r, eye_y + iris_r, CAT_EYE_GREEN);
        /* Pupil (vertical slit in center) */
        uint16_t slit_h = iris_r;
        uint16_t slit_w = scale > 1 ? 2 : 1;
        LCD_Fill(lx - slit_w, eye_y - slit_h, lx + slit_w, eye_y + slit_h, CAT_EYE_COLOR);
        LCD_Fill(rx - slit_w, eye_y - slit_h, rx + slit_w, eye_y + slit_h, CAT_EYE_COLOR);
        /* Eye shine */
        uint16_t sr = scale;
        if (sr < 1) sr = 1;
        LCD_DrawPoint(lx - iris_r + 2, eye_y - iris_r + 2, CAT_EYE_WHITE);
        LCD_DrawPoint(lx - iris_r + 3, eye_y - iris_r + 2, CAT_EYE_WHITE);
        LCD_DrawPoint(rx - iris_r + 2, eye_y - iris_r + 2, CAT_EYE_WHITE);
        LCD_DrawPoint(rx - iris_r + 3, eye_y - iris_r + 2, CAT_EYE_WHITE);
    }

    /* --- Nose (small triangle) --- */
    uint16_t nose_y = head_y + 6 * scale;
    uint16_t nose_w = 4 * scale;
    LCD_Fill(cx - nose_w, nose_y, cx + nose_w, nose_y + 3*scale, CAT_NOSE_COLOR);

    /* --- Mouth (two curved lines from nose) --- */
    uint16_t mouth_y1 = nose_y + 3 * scale;
    uint16_t mouth_w  = 7 * scale;
    uint16_t mouth_h  = 5 * scale;

    if (g_pet.current_mood == MOOD_HAPPY) {
        /* Wide grin */
        LCD_Line(cx - mouth_w, mouth_y1 + mouth_h, cx, mouth_y1, CAT_OUTLINE);
        LCD_Line(cx, mouth_y1, cx + mouth_w, mouth_y1 + mouth_h, CAT_OUTLINE);
        /* Tiny open mouth */
        LCD_Fill(cx - 3*scale, mouth_y1 + 1, cx + 3*scale, mouth_y1 + mouth_h, CAT_NOSE_COLOR);
    } else if (g_pet.current_mood == MOOD_SAD) {
        LCD_Line(cx - mouth_w, mouth_y1, cx, mouth_y1 + mouth_h, CAT_OUTLINE);
        LCD_Line(cx, mouth_y1 + mouth_h, cx + mouth_w, mouth_y1, CAT_OUTLINE);
    } else if (g_pet.current_mood == MOOD_HUNGRY) {
        LCD_Fill(cx - 5*scale, mouth_y1, cx + 5*scale, mouth_y1 + 6*scale, CAT_EYE_COLOR);
        /* Tongue */
        LCD_Fill(cx - 2*scale, mouth_y1 + 2*scale, cx + 2*scale, mouth_y1 + 5*scale, RED);
    } else {
        /* Default: gentle ^_^ */
        LCD_Line(cx - mouth_w, mouth_y1 + mouth_h, cx, mouth_y1, CAT_OUTLINE);
        LCD_Line(cx, mouth_y1, cx + mouth_w, mouth_y1 + mouth_h, CAT_OUTLINE);
    }

    /* --- Whiskers --- */
    uint16_t whisk_y1 = nose_y + 1*scale;
    uint16_t whisk_y2 = nose_y + 2*scale;
    uint16_t whisk_len = 18 * scale;

    for (uint8_t w = 0; w < 2; w++) {
        uint16_t wsx, wsy;
        if (w == 0) { wsx = cx - head_r + 5*scale; wsy = whisk_y1; }
        else        { wsx = cx - head_r + 5*scale; wsy = whisk_y2; }
        LCD_Line(wsx, wsy, wsx - whisk_len, wsy - 3*scale, CAT_WHISKER_COLOR);
        LCD_Line(wsx, wsy, wsx - whisk_len, wsy + 2*scale, CAT_WHISKER_COLOR);

        uint16_t rex = cx + head_r - 5*scale;
        LCD_Line(rex, whisk_y1, rex + whisk_len, whisk_y1 - 3*scale, CAT_WHISKER_COLOR);
        LCD_Line(rex, whisk_y1, rex + whisk_len, whisk_y1 + 2*scale, CAT_WHISKER_COLOR);
        LCD_Line(rex, whisk_y2, rex + whisk_len, whisk_y2 - 3*scale, CAT_WHISKER_COLOR);
        LCD_Line(rex, whisk_y2, rex + whisk_len, whisk_y2 + 2*scale, CAT_WHISKER_COLOR);
    }

    /* --- Stage features --- */
    if (g_pet.stage >= STAGE_GROWING) {
        /* Collar / bow tie */
        uint16_t bow_y = head_y + head_r - 8*scale;
        uint16_t bow_w = 8 * scale;
        /* Left bow */
        LCD_Fill(cx - bow_w - 3*scale, bow_y - 2*scale, cx - 1, bow_y + 2*scale, RED);
        LCD_Fill(cx + 1, bow_y - 2*scale, cx + bow_w + 3*scale, bow_y + 2*scale, RED);
        /* Center */
        LCD_Fill(cx - 1, bow_y - 3*scale, cx + 1, bow_y + 3*scale, YELLOW);
    }

    if (g_pet.stage >= STAGE_MATURE) {
        /* Crown */
        uint16_t crown_y = ear_y - ear_h - 6*scale;
        uint16_t crown_w = ear_dx + ear_w;
        uint16_t crown_h = 8 * scale;

        /* Crown base */
        LCD_Fill(cx - crown_w, crown_y, cx + crown_w, crown_y + crown_h, CAT_CROWN_COLOR);
        LCD_Line(cx - crown_w, crown_y, cx + crown_w, crown_y + crown_h, CAT_OUTLINE);
        LCD_Line(cx - crown_w, crown_y, cx + crown_w, crown_y, CAT_OUTLINE);
        LCD_Line(cx - crown_w, crown_y + crown_h, cx + crown_w, crown_y + crown_h, CAT_OUTLINE);

        /* Crown spikes */
        for (uint8_t i = 0; i < 3; i++) {
            uint16_t sx = cx - crown_w + (i + 1) * crown_w / 2;
            LCD_Fill(sx - 3*scale, crown_y - crown_h, sx + 3*scale, crown_y, CAT_CROWN_COLOR);
            LCD_Line(sx, crown_y - crown_h, sx + 3*scale, crown_y, CAT_OUTLINE);
            LCD_Line(sx, crown_y - crown_h, sx - 3*scale, crown_y, CAT_OUTLINE);

            /* Jewel on top */
            LCD_Fill(sx - 1, crown_y - crown_h, sx + 1, crown_y - crown_h + 1, RED);
        }
    }

    if (g_pet.stage == STAGE_SECRET) {
        /* Space cat — stars around head */
        uint16_t star_radius = head_r + 15 * scale;

        /* Star shape: small cross + diagonal cross */
        for (uint8_t s = 0; s < 6; s++) {
            int16_t angle = s * 60 + frame * 3;
            /* Simple: calculate x,y on circle */
            int16_t sx, sy;
            /* Hmm, let me use approximate positions */
            uint16_t offsets[6][2] = {
                {cx, head_y - star_radius},
                {cx + star_radius * 7/10, head_y - star_radius * 7/10},
                {cx + star_radius * 7/10, head_y + star_radius * 7/10},
                {cx, head_y + star_radius - 10*scale},
                {cx - star_radius * 7/10, head_y + star_radius * 7/10},
                {cx - star_radius * 7/10, head_y - star_radius * 7/10}
            };

            uint16_t sx2 = offsets[s][0];
            uint16_t sy2 = offsets[s][1];

            /* Draw small cross star */
            uint8_t ss = (s % 3 == 0) ? 3 : 2;
            LCD_Line(sx2 - ss*scale, sy2, sx2 + ss*scale, sy2, CAT_STAR_COLOR);
            LCD_Line(sx2, sy2 - ss*scale, sx2, sy2 + ss*scale, CAT_STAR_COLOR);
        }
    }

    /* --- Sickness indicator --- */
    if (g_pet.is_sick) {
        /* X eyes override */
        uint16_t sx_r = 5 * scale;
        /* Left X */
        LCD_Line(lx - sx_r, eye_y - sx_r, lx + sx_r, eye_y + sx_r, RED);
        LCD_Line(lx + sx_r, eye_y - sx_r, lx - sx_r, eye_y + sx_r, RED);
        /* Right X */
        LCD_Line(rx - sx_r, eye_y - sx_r, rx + sx_r, eye_y + sx_r, RED);
        LCD_Line(rx + sx_r, eye_y - sx_r, rx - sx_r, eye_y + sx_r, RED);
    }
}

/* ================================================================
 * CLAUDE CHARACTER — round friendly face, orange/warm palette
 * frame: animation tick, mood: ClaudeStatus for expression
 * ================================================================ */
void Render_ClaudeCharacter(uint16_t cx, uint16_t cy, uint8_t frame, ClaudeStatus mood)
{
    /* Body: flat rectangular block, head+body fused, coral orange */
    uint16_t bw = 40;
    uint16_t bh = 28;
    uint16_t body = 0xFC68;   /* coral orange */
    uint16_t light = 0xFDAB;  /* highlight */
    uint16_t dark = 0xEAA4;   /* outline */

    LCD_Fill(cx - 60, cy - 55, cx + 60, cy + 55, BLACK);

    /* Float animation */
    int8_t bob = 0;
    uint8_t f60 = frame % 60;
    if (f60 < 15)      bob = f60 / 5;
    else if (f60 < 30) bob = 3 - (f60 - 15) / 5;
    else if (f60 < 45) bob = -(f60 - 30) / 5;
    else               bob = -3 + (f60 - 45) / 5;
    uint16_t by = cy + bob;

    /* Side arms */
    uint16_t aw = 10, ah = 14, ay = by - 5;
    LCD_Fill(cx - bw - aw, ay, cx - bw - 1, ay + ah, body);
    LCD_Fill(cx + bw + 1, ay, cx + bw + aw, ay + ah, body);

    /* Main body block */
    LCD_Fill(cx - bw, by - bh, cx + bw, by + bh, body);
    /* Slightly rounded top corners */
    LCD_Fill(cx - bw + 3, by - bh, cx + bw - 3, by - bh + 3, body);
    /* Outline */
    LCD_Line(cx - bw + 3, by - bh, cx + bw - 3, by - bh, dark);
    LCD_Line(cx - bw, by - bh + 3, cx - bw, by + bh, dark);
    LCD_Line(cx + bw, by - bh + 3, cx + bw, by + bh, dark);
    LCD_Line(cx - bw + 3, by + bh, cx + bw - 3, by + bh, dark);
    /* Top highlight bar */
    LCD_Fill(cx - bw + 5, by - bh + 2, cx + bw - 5, by - bh + 5, light);

    /* 4 short legs at bottom */
    uint16_t lw = 6, lh = 16, ly = by + bh;
    uint16_t legs[4] = {cx - bw + 3, cx - bw + 3 + lw + 7,
                        cx + bw - 3 - lw - 7 - lw, cx + bw - 3 - lw};
    uint8_t i;
    for (i = 0; i < 4; i++)
        LCD_Fill(legs[i], ly, legs[i] + lw, ly + lh, body);

    /* Eyes: two black rectangles, no mouth */
    uint16_t ew = 7, eh = 13, ey = by - 8, esp = 18;
    uint8_t blink = (frame % 30 >= 27);

    if (mood == CLAUDE_DONE) {
        LCD_Fill(cx - esp - ew - 1, ey, cx - esp + 1, ey + eh, CLAUDE_EYE_PUPIL);
        LCD_Fill(cx + esp - 1, ey, cx + esp + ew + 1, ey + eh, CLAUDE_EYE_PUPIL);
    } else if (mood == CLAUDE_THINKING) {
        LCD_Fill(cx - esp - ew, ey - 3, cx - esp, ey + eh - 6, CLAUDE_EYE_PUPIL);
        LCD_Fill(cx + esp, ey - 3, cx + esp + ew, ey + eh - 6, CLAUDE_EYE_PUPIL);
    } else if (mood == CLAUDE_WAITING) {
        LCD_Fill(cx - esp - ew, ey, cx - esp, ey + eh, CLAUDE_EYE_PUPIL);
        LCD_Fill(cx + esp, ey + 2, cx + esp + ew, ey + eh - 2, CLAUDE_EYE_PUPIL);
    } else if (blink) {
        LCD_Fill(cx - esp - ew, ey + eh/2 - 1, cx - esp, ey + eh/2 + 1, CLAUDE_EYE_PUPIL);
        LCD_Fill(cx + esp, ey + eh/2 - 1, cx + esp + ew, ey + eh/2 + 1, CLAUDE_EYE_PUPIL);
    } else {
        LCD_Fill(cx - esp - ew, ey, cx - esp, ey + eh, CLAUDE_EYE_PUPIL);
        LCD_Fill(cx + esp, ey, cx + esp + ew, ey + eh, CLAUDE_EYE_PUPIL);
    }

    /* Effects */
    if (mood == CLAUDE_THINKING) {
        LCD_DrawPoint(cx + bw + 5, by - bh - 12, LIGHTBLUE);
        LCD_Fill(cx + bw + 8, by - bh - 20, cx + bw + 14, by - bh - 14, LIGHTBLUE);
    }
    if (mood == CLAUDE_DONE && (frame % 12 < 6)) {
        uint16_t sx = cx - 20 + (frame % 3) * 12;
        LCD_DrawPoint(sx, by - bh - 6, YELLOW);
        LCD_DrawPoint(sx + 5, by - bh - 9, YELLOW);
    }
}

void Render_PCStatusBar(void)
{
    char buf[32];
    uint16_t y = PC_STATUS_Y;
    uint16_t bar_x = PC_STATUS_BAR_X;
    uint16_t bar_w = PC_STATUS_BAR_W;
    uint16_t bar_h = PC_STATUS_BAR_H;
    uint16_t label_x = 5;

    /* Clear status bar region only (not whole screen) */
    LCD_Fill(0, 0, LCD_WIDTH - 1, PC_STATUS_Y + PC_STATUS_LINE_H * 3 + 12, BLACK);

    /* ---- CPU Line ---- */
    if (g_pc_data.cpu_temp > 0) {
        snprintf(buf, sizeof(buf), "CPU %.0fC", g_pc_data.cpu_temp);
    } else {
        snprintf(buf, sizeof(buf), "CPU --");
    }
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d%%", g_pc_data.cpu_usage);
    LCD_String(bar_x - 32, y, buf, 12, GREEN, BLACK);

    /* CPU bar — color shifts: green → yellow → red */
    uint16_t cpu_color = GREEN;
    if (g_pc_data.cpu_usage > 75) cpu_color = RED;
    else if (g_pc_data.cpu_usage > 50) cpu_color = YELLOW;
    DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.cpu_usage, cpu_color, DARKBLUE);

    /* ---- GPU Line ---- */
    y += PC_STATUS_LINE_H;
    if (g_pc_data.gpu_temp > 0) {
        snprintf(buf, sizeof(buf), "GPU %.0fC", g_pc_data.gpu_temp);
    } else if (g_pc_data.gpu_usage == 0xFF) {
        snprintf(buf, sizeof(buf), "GPU --");
    } else {
        snprintf(buf, sizeof(buf), "GPU n/a");  /* has usage but no temp sensor */
    }
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);

    if (g_pc_data.gpu_usage != 0xFF) {
        snprintf(buf, sizeof(buf), "%d%%", g_pc_data.gpu_usage);
        uint16_t gpu_color = GREEN;
        if (g_pc_data.gpu_usage > 75) gpu_color = RED;
        else if (g_pc_data.gpu_usage > 50) gpu_color = YELLOW;
        LCD_String(bar_x - 32, y, buf, 12, gpu_color, BLACK);
        DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.gpu_usage, gpu_color, DARKBLUE);
    } else {
        LCD_String(bar_x - 32, y, "--", 12, LGRAY, BLACK);
    }

    /* ---- MEM Line ---- */
    y += PC_STATUS_LINE_H;
    snprintf(buf, sizeof(buf), "MEM");
    LCD_String(label_x, y, buf, 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d%%", g_pc_data.mem_usage);
    LCD_String(bar_x - 32, y, buf, 12, CYAN, BLACK);
    DrawBar(bar_x, y + 2, bar_w, bar_h, g_pc_data.mem_usage, CYAN, DARKBLUE);

    /* ---- Separator line ---- */
    y += PC_STATUS_LINE_H + 4;
    LCD_Line(0, y, LCD_WIDTH - 1, y, DARKBLUE);
}

/* Display Chinese status using FONT.H indices (W25Q128 font lib not available) */
/* Indices in aFontChinese16: 5=就 6=绪 7=思 8=考 9=中 10=执 11=行 12=待 13=确 14=认 15=已 16=完 17=成 */
static void ShowStatusChinese(uint16_t x, uint16_t y, ClaudeStatus st, uint16_t color)
{
    LCD_Fill(0, y, LCD_WIDTH - 1, y + 22, BLACK);
    switch (st) {
    case CLAUDE_IDLE:
        LCD_ShowChinese(x, y, 16, 5, color, BLACK);   /* 就 */
        LCD_ShowChinese(x+18, y, 16, 6, color, BLACK); /* 绪 */
        break;
    case CLAUDE_THINKING:
        LCD_ShowChinese(x, y, 16, 7, color, BLACK);    /* 思 */
        LCD_ShowChinese(x+18, y, 16, 8, color, BLACK); /* 考 */
        LCD_ShowChinese(x+36, y, 16, 9, color, BLACK); /* 中 */
        break;
    case CLAUDE_EXECUTING:
        LCD_ShowChinese(x, y, 16, 10, color, BLACK);   /* 执 */
        LCD_ShowChinese(x+18, y, 16, 11, color, BLACK);/* 行 */
        LCD_ShowChinese(x+36, y, 16, 9, color, BLACK); /* 中 */
        break;
    case CLAUDE_WAITING:
        LCD_ShowChinese(x, y, 16, 12, color, BLACK);   /* 待 */
        LCD_ShowChinese(x+18, y, 16, 13, color, BLACK);/* 确 */
        LCD_ShowChinese(x+36, y, 16, 14, color, BLACK);/* 认 */
        break;
    case CLAUDE_DONE:
        LCD_ShowChinese(x, y, 16, 15, color, BLACK);   /* 已 */
        LCD_ShowChinese(x+18, y, 16, 16, color, BLACK);/* 完 */
        LCD_ShowChinese(x+36, y, 16, 17, color, BLACK);/* 成 */
        break;
    default:
        break;
    }
}

void Render_StatusText(const char *text, uint16_t color)
{
    uint16_t w = strlen(text) * 8;  /* ~8px per char at size 16 */
    uint16_t x = (LCD_WIDTH - w) / 2;
    LCD_Fill(0, STATUS_TEXT_Y, LCD_WIDTH - 1, STATUS_TEXT_Y + 22, BLACK);
    LCD_String(x, STATUS_TEXT_Y, (char*)text, 16, color, BLACK);
}

uint8_t g_display_page = 0;  /* 0=PC monitor, 1=Calendar, 2=Pomodoro */

/* === Pomodoro Timer State === */
#define POMO_WORK_SEC   (25 * 60)
#define POMO_BREAK_SEC  (5 * 60)
uint16_t g_pomo_sec = POMO_WORK_SEC;
uint8_t  g_pomo_running = 0;
uint8_t  g_pomo_is_work = 1;
uint8_t  g_pomo_count = 0;
uint32_t g_pomo_last_tick = 0;

/* Days in each month */
static uint8_t days_in_month(uint8_t y, uint8_t m)
{
    static const uint8_t d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m != 2) return d[m-1];
    /* Leap year check */
    if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) return 29;
    return 28;
}

void Render_PomodoroPage(void)
{
    char buf[20];
    uint16_t mins, secs;
    uint16_t cy;

    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);

    /* === Animated tomato icon (top-left) === */
    {
        extern volatile uint32_t g_sys_tick_ms;
        uint8_t f = (g_sys_tick_ms / 100) % 60;
        /* Tomato body (red circle) */
        uint16_t tx = 30, ty = 30;
        LCD_Circle(tx, ty, 12, RED);
        LCD_Fill(tx - 12, ty - 10, tx + 12, ty + 8, RED);
        /* Green leaf on top */
        LCD_Fill(tx - 2, ty - 19, tx + 2, ty - 12, GREEN);
        LCD_Fill(tx - 6, ty - 17, tx + 6, ty - 14, GREEN);
        /* Highlight */
        LCD_Circle(tx - 4, ty - 3, 3, WHITE);
        /* Subtle pulsing when running */
        if (g_pomo_running && (f < 10)) {
            LCD_Circle(tx, ty, 14, RED);
        }
    }

    /* Title */
    cy = 20;
    LCD_String(55, cy, "Pomodoro", 16, WHITE, BLACK);

    /* Work/Break label */
    cy = 50;
    snprintf(buf, sizeof(buf), "%s #%d",
             g_pomo_is_work ? "WORK" : "BREAK", g_pomo_count);
    uint16_t tx = (LCD_WIDTH - strlen(buf) * 12) / 2;
    uint16_t clr = g_pomo_is_work ? RED : GREEN;
    LCD_String(tx, cy, buf, 24, clr, BLACK);

    /* Big countdown */
    cy = 110;
    mins = g_pomo_sec / 60;
    secs = g_pomo_sec % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
    tx = (LCD_WIDTH - 5 * 16) / 2;  /* 5 chars * ~16px at size 32 */
    LCD_String(tx, cy, buf, 32, WHITE, BLACK);

    /* Animated dots (bouncing when running) */
    {
        extern volatile uint32_t g_sys_tick_ms;
        uint8_t f = (g_sys_tick_ms / 150) % 12;
        uint16_t dx = 80;
        for (uint8_t i = 0; i < 4; i++) {
            uint16_t dy = cy - 8;
            if (g_pomo_running) {
                /* Bounce: rise and fall */
                uint8_t phase = (f + i * 3) % 12;
                if (phase < 6) dy -= phase * 2;
                else dy -= (11 - phase) * 2;
            }
            uint16_t dc = g_pomo_running ? YELLOW : LGRAY;
            LCD_Fill(dx + i * 20, dy, dx + i * 20 + 6, dy + 6, dc);
        }
    }

    /* Progress bar */
    cy = 170;
    uint16_t total = g_pomo_is_work ? POMO_WORK_SEC : POMO_BREAK_SEC;
    uint8_t pct = (uint8_t)((uint32_t)(total - g_pomo_sec) * 100 / total);
    DrawBar(30, cy, 180, 12, pct, g_pomo_is_work ? RED : GREEN, DARKBLUE);

    /* Status */
    cy = 200;
    LCD_String(60, cy, g_pomo_running ? "Running" : "Paused", 16,
               g_pomo_running ? GREEN : YELLOW, BLACK);

    /* Key hints */
    cy = 260;
    LCD_String(10, cy, "K1:", 12, WHITE, BLACK);
    LCD_String(40, cy, g_pomo_running ? "Pause" : "Start", 12, GREEN, BLACK);
    LCD_String(100, cy, "K2:Reset", 12, WHITE, BLACK);
    LCD_String(10, cy + 20, "K3:Next", 12, LGRAY, BLACK);

    /* Bottom clock */
    {
        const char *t = g_pc_data.time_str;
        if (t[0] == '\0') t = "--:--:--";
        uint16_t tx = (LCD_WIDTH - strlen(t) * 12) / 2;
        LCD_String(tx, 290, (char*)t, 24, WHITE, BLACK);
    }
}

/* ================================================================
 * LYRICS PAGE — shows now-playing song info
 * ================================================================ */
void Render_LyricsPage(void)
{
    char buf[64];
    uint16_t cy;

    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);

    /* Music icon (simple note) */
    {
        uint16_t nx = 100, ny = 25;
        LCD_Circle(nx, ny, 8, CYAN);
        LCD_Fill(nx - 3, ny + 5, nx + 3, ny + 18, CYAN);
        LCD_Fill(nx - 8, ny + 10, nx + 8, ny + 16, CYAN);
    }

    /* Title - Now Playing */
    cy = 60;
    LCD_String(50, cy, "Now Playing", 16, LIGHTBLUE, BLACK);

    /* Song title */
    cy = 100;
    if (g_pc_data.song[0] != '\0') {
        uint16_t tx = (LCD_WIDTH - strlen(g_pc_data.song) * 8) / 2;
        if (tx > LCD_WIDTH) tx = 5;
        LCD_String(tx, cy, g_pc_data.song, 16, WHITE, BLACK);
    } else {
        LCD_String(50, cy, "No media playing", 12, LGRAY, BLACK);
    }

    /* Artist */
    cy = 130;
    if (g_pc_data.artist[0] != '\0') {
        uint16_t tx = (LCD_WIDTH - strlen(g_pc_data.artist) * 8) / 2;
        if (tx > LCD_WIDTH) tx = 5;
        LCD_String(tx, cy, g_pc_data.artist, 12, CYAN, BLACK);
    }

    /* Divider */
    LCD_Line(10, 165, LCD_WIDTH - 10, 165, DARKBLUE);

    /* Lyrics text (auto-scroll) */
    if (g_pc_data.lyrics[0] != '\0') {
        extern volatile uint32_t g_sys_tick_ms;
        /* Count lines in lyrics */
        uint16_t total_lines = 1;
        const char *lp = g_pc_data.lyrics;
        while (*lp) { if (*lp++ == '\n') total_lines++; }
        /* Scroll: shift every 4 seconds */
        uint8_t scroll_offset = (uint8_t)((g_sys_tick_ms / 4000) % total_lines);
        /* Find start of current line */
        lp = g_pc_data.lyrics;
        for (uint8_t s = 0; s < scroll_offset && *lp; s++) {
            while (*lp && *lp != '\n') lp++;
            if (*lp == '\n') lp++;
        }
        /* Show 3 lines */
        uint16_t ly = 175;
        for (uint8_t li = 0; li < 3 && *lp; li++) {
            char line[33] = {0};
            uint8_t ln = 0;
            while (*lp && *lp != '\n' && ln < 30) line[ln++] = *lp++;
            if (*lp == '\n') lp++;
            LCD_String(5, ly, line, 12, WHITE, BLACK);
            ly += 16;
        }
    } else {
        LCD_String(30, 180, "No lyrics available", 12, LGRAY, BLACK);
    }

    /* Animated equalizer bars */
    {
        extern volatile uint32_t g_sys_tick_ms;
        cy = 185;
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t f = (g_sys_tick_ms / 80 + i * 5) % 16;
            uint8_t h = (f < 8) ? (f * 3 + 6) : ((15 - f) * 3 + 6);
            LCD_Fill(60 + i * 26, cy + 20 - h, 60 + i * 26 + 12, cy + 20, GREEN);
        }
    }

    /* Controls */
    cy = 230;
    LCD_String(30, cy, "Prev", 12, WHITE, BLACK);
    LCD_String(95, cy, "Play", 12, WHITE, BLACK);
    LCD_String(160, cy, "Next", 12, WHITE, BLACK);

    /* Bottom clock */
    {
        const char *t = g_pc_data.time_str;
        if (t[0] == '\0') t = "--:--:--";
        uint16_t tx = (LCD_WIDTH - strlen(t) * 12) / 2;
        LCD_String(tx, 270, (char*)t, 20, WHITE, BLACK);
    }

    LCD_String(5, LCD_HEIGHT - 15, "K3:Next", 12, LGRAY, BLACK);
}

void Render_CalendarPage(void)
{
    char buf[20];
    uint16_t cx, cy, i;
    const char *wd[] = {"Mo","Tu","We","Th","Fr","Sa","Su"};
    const char *wd_full[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};

    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);

    /* === Top bar: date + weekday (y=5 to y=35) === */
    cy = 8;
    LCD_Fill(0, 0, LCD_WIDTH - 1, 38, DARKBLUE);
    if (g_pc_data.date_str[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s  %s", g_pc_data.date_str,
                 wd_full[g_pc_data.weekday % 7]);
    } else {
        snprintf(buf, sizeof(buf), "----/--/--  ---");
    }
    cx = (LCD_WIDTH - strlen(buf) * 8) / 2;
    LCD_String(cx, cy + 5, buf, 16, WHITE, DARKBLUE);

    /* === DHT11 row === */
    cy = 42;
    if (g_pc_data.dht11_temp != 0xFF && g_pc_data.dht11_humi != 0xFF) {
        snprintf(buf, sizeof(buf), "T:%d.%dC  H:%d.%d%%",
                 g_pc_data.dht11_temp, 0, g_pc_data.dht11_humi, 0);
    } else {
        snprintf(buf, sizeof(buf), "T:--.-C  H:--.-%%");
    }
    cx = (LCD_WIDTH - strlen(buf) * 8) / 2;
    LCD_String(cx, cy, buf, 12, GREEN, BLACK);

    /* === Weekday headers === */
    cy = 62;
    for (i = 0; i < 7; i++) {
        cx = 16 + i * 30;
        uint16_t clr = (i >= 5) ? LIGHTBLUE : WHITE;
        LCD_String(cx, cy, (char*)wd[i], 12, clr, BLACK);
    }
    LCD_Line(0, cy + 16, LCD_WIDTH - 1, cy + 16, DARKBLUE);

    /* === Build calendar data === */
    uint8_t year = 26, month = 6, today = 1;
    if (g_pc_data.date_str[4] == '-' && g_pc_data.date_str[7] == '-') {
        month = (g_pc_data.date_str[5]-'0')*10 + (g_pc_data.date_str[6]-'0');
        today  = (g_pc_data.date_str[8]-'0')*10 + (g_pc_data.date_str[9]-'0');
    }
    uint8_t days = days_in_month(year, month);

    uint8_t first_wday;
    /* Zeller: compute weekday of 1st day (0=Mon..6=Sun) */
    {
        uint8_t mz = month, yz = year;
        if (mz < 3) { mz += 12; yz--; }
        uint8_t K = yz, J = 20;
        uint8_t h = (uint8_t)((1 + (13*(mz+1))/5 + K + K/4 + J/4 + 5*J) % 7);
        first_wday = (h + 5) % 7;
    }

    /* === Calendar grid === */
    uint8_t col = first_wday, row = 0, d;
    uint16_t cell_y0 = 82, cell_h = 24, cell_w = 30;

    for (d = 1; d <= days; d++) {
        cx = 10 + col * cell_w;
        cy = cell_y0 + row * cell_h;
        snprintf(buf, sizeof(buf), "%2d", d);

        if (d == today) {
            LCD_Fill(cx - 2, cy - 1, cx + cell_w - 4, cy + cell_h - 2, DARKBLUE);
            LCD_String(cx, cy, buf, 16, YELLOW, DARKBLUE);
        } else {
            uint16_t c = (col >= 5) ? LIGHTBLUE : WHITE;
            LCD_String(cx, cy, buf, 12, c, BLACK);
        }
        if (++col >= 7) { col = 0; row++; }
    }

    /* === Bottom: time === */
    cy = 270;
    const char *t = g_pc_data.time_str;
    if (t[0] == '\0') t = "--:--:--";
    cx = (LCD_WIDTH - strlen(t) * 12) / 2;
    LCD_String(cx, cy, (char*)t, 24, WHITE, BLACK);

    /* Page hint */
    LCD_String(5, LCD_HEIGHT - 15, "[K3] PC", 12, LGRAY, BLACK);
}

void Render_PCMode(void)
{
    /* Top status bars */
    Render_PCStatusBar();

    /* Claude character */
    extern volatile uint32_t g_sys_tick_ms;
    uint8_t frame = (uint8_t)((g_sys_tick_ms / 100) % 256);
    Render_ClaudeCharacter(CLAUDE_CX, CLAUDE_CY, frame, g_pc_data.claude);

    /* Status text — English for now, Chinese needs PCtoLCD font conversion */
    const char *label = PC_Monitor_GetStatusLabel();
    uint16_t label_color = WHITE;
    switch (g_pc_data.claude) {
        case CLAUDE_THINKING:  label_color = LIGHTBLUE; break;
        case CLAUDE_EXECUTING: label_color = GREEN;     break;
        case CLAUDE_WAITING:   label_color = YELLOW;    break;
        case CLAUDE_DONE:      label_color = GREEN;     break;
        default:               label_color = WHITE;     break;
    }
    Render_StatusText(label, label_color);

    /* Bottom info bar — show time + connection dot */
    LCD_Fill(0, PC_INFO_BAR_Y, LCD_WIDTH - 1, LCD_HEIGHT - 1, DARKBLUE);
    LCD_Line(0, PC_INFO_BAR_Y, LCD_WIDTH - 1, PC_INFO_BAR_Y, LIGHTBLUE);

    /* Clock — centered, always display */
    {
        const char *t = g_pc_data.time_str;
        if (t[0] == '\0') t = "--:--:--";
        uint16_t tlen = strlen(t);
        uint16_t tx = (LCD_WIDTH - (int)tlen * 8) / 2;
        LCD_String(tx, PC_INFO_BAR_Y + 10, (char*)t, 16, WHITE, DARKBLUE);
    }

    /* Connection dot */
    if (g_pc_data.is_connected && g_pc_data.error_count == 0) {
        LCD_Fill(225, PC_INFO_BAR_Y + 3, 234, PC_INFO_BAR_Y + 12, GREEN);
    } else {
        LCD_Fill(225, PC_INFO_BAR_Y + 3, 234, PC_INFO_BAR_Y + 12, RED);
    }
    /* Page hint */
    LCD_String(5, PC_INFO_BAR_Y + 8, "K3:Page", 12, LGRAY, DARKBLUE);
}

/* ================================================================
 * PUBLIC API
 * ================================================================ */
void Render_Init(void)
{
    LCD_Init();
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);
}

void Render_UpdateStatusBar(void)
{
    char buf[24];

    LCD_Fill(0, STATUS_TOP_Y, LCD_WIDTH - 1,
             STATUS_TOP_Y + STATUS_BAR_H * 2 + STATUS_SPACING + 20, BLACK);

    uint16_t y1 = STATUS_TOP_Y;
    uint16_t y2 = STATUS_TOP_Y + STATUS_BAR_H + STATUS_SPACING;

    /* Row 1 */
    LCD_String(STAT_LEFT_X, y1, "Hunger", 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d", g_pet.hunger);
    LCD_String(STAT_LEFT_X + 100, y1, buf, 12, GREEN, BLACK);
    DrawBar(STAT_LEFT_X, y1 + 13, 114, 6, g_pet.hunger, GREEN, DARKBLUE);

    LCD_String(STAT_RIGHT_X, y1, "Energy", 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d", g_pet.energy);
    LCD_String(STAT_RIGHT_X + 100, y1, buf, 12, BLUE, BLACK);
    DrawBar(STAT_RIGHT_X, y1 + 13, 106, 6, g_pet.energy, BLUE, DARKBLUE);

    /* Row 2 */
    LCD_String(STAT_LEFT_X, y2, "Mood", 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d", g_pet.mood);
    LCD_String(STAT_LEFT_X + 100, y2, buf, 12, YELLOW, BLACK);
    DrawBar(STAT_LEFT_X, y2 + 13, 114, 6, g_pet.mood, YELLOW, DARKBLUE);

    LCD_String(STAT_RIGHT_X, y2, "Clean", 12, WHITE, BLACK);
    snprintf(buf, sizeof(buf), "%d", g_pet.hygiene);
    LCD_String(STAT_RIGHT_X + 100, y2, buf, 12, CYAN, BLACK);
    DrawBar(STAT_RIGHT_X, y2 + 13, 106, 6, g_pet.hygiene, CYAN, DARKBLUE);

    LCD_Line(0, y2 + 24, LCD_WIDTH - 1, y2 + 24, DARKBLUE);
}

void Render_DrawPet(uint8_t anim_frame)
{
    uint16_t cx = LCD_WIDTH / 2;
    uint16_t cy = PET_AREA_Y + PET_AREA_H / 2 - 10;

    /* Map pet mood to Claude status for expression */
    ClaudeStatus mood;
    if (g_pet.is_sick)      mood = CLAUDE_IDLE;
    else if (g_pet.current_mood == MOOD_HAPPY)  mood = CLAUDE_DONE;
    else if (g_pet.current_mood == MOOD_SAD)    mood = CLAUDE_WAITING;
    else if (g_pet.current_mood == MOOD_ANGRY)  mood = CLAUDE_THINKING;
    else                                        mood = CLAUDE_EXECUTING;

    LCD_Fill(0, PET_AREA_Y, LCD_WIDTH - 1, PET_AREA_Y + PET_AREA_H, BLACK);
    Render_ClaudeCharacter(cx, cy, anim_frame, mood);
}

void Render_ShowCommandResult(const char *msg)
{
    strncpy(g_result_msg, msg, sizeof(g_result_msg) - 1);
    g_result_msg[sizeof(g_result_msg) - 1] = '\0';
    g_result_timer = 25;
}

void Render_DrawInfoBar(void)
{
    LCD_Fill(0, INFO_BAR_Y, LCD_WIDTH - 1, LCD_HEIGHT - 1, DARKBLUE);
    LCD_Line(0, INFO_BAR_Y, LCD_WIDTH - 1, INFO_BAR_Y, LIGHTBLUE);

    const char *stage_names[] = {"Kitten", "Cat", "King", "???"};
    char buf[48];

    snprintf(buf, sizeof(buf), "Lv.%lu %s  EXP:%lu",
             (unsigned long)(g_pet.experience / 10),
             stage_names[g_pet.stage],
             (unsigned long)g_pet.experience);
    LCD_String(5, INFO_BAR_Y + 4, buf, 12, WHITE, DARKBLUE);

    if (g_result_timer > 0) {
        LCD_String(5, INFO_BAR_Y + 22, g_result_msg, 12, GREEN, DARKBLUE);
        g_result_timer--;
    } else {
        LCD_String(5, INFO_BAR_Y + 22, g_pet.name, 12, YELLOW, DARKBLUE);
    }

    DrawMoodIcon(150, INFO_BAR_Y + 4, g_pet.current_mood);

    if (Cmd_IsCooldown()) {
        snprintf(buf, sizeof(buf), "CD:%d", (int)Cmd_GetCooldownSec());
        LCD_String(190, INFO_BAR_Y + 22, buf, 12, RED, DARKBLUE);
    }
}

void Render_DrawAll(void)
{
    if (g_display_page == 0) {
        Render_PCMode();
    } else if (g_display_page == 1) {
        Render_CalendarPage();
    } else if (g_display_page == 2) {
        Render_PomodoroPage();
    } else {
        Render_LyricsPage();
    }
    g_anim_tick++;
    if (g_anim_tick >= 200) g_anim_tick = 0;
}
