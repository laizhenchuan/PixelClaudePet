#ifndef __PET_RENDER_H
#define __PET_RENDER_H

#include "bsp_LCD_ILI9341.h"
#include "pet_core.h"
#include "pc_monitor.h"  /* For ClaudeStatus enum */

/* Layout */
#define STATUS_TOP_Y        15
#define STATUS_BAR_H        20
#define STATUS_SPACING       5
#define PET_AREA_Y          65
#define PET_AREA_H          210
#define INFO_BAR_Y          278
#define INFO_BAR_H          42
#define STAT_LEFT_X          8
#define STAT_RIGHT_X        128

/* ---- PC Monitor Mode Layout ---- */
/* Screen is 240x320 portrait; LCD_WIDTH=240, LCD_HEIGHT=320 */

/* PC status bar area (top section) */
#define PC_STATUS_Y         5
#define PC_STATUS_LINE_H    18   /* Height per status line */
#define PC_STATUS_BAR_W     100  /* Width of progress bar */
#define PC_STATUS_BAR_X     130  /* X start of progress bar */
#define PC_STATUS_BAR_H     8    /* Height of progress bar */

/* Claude character area (middle section) */
#define CLAUDE_AREA_Y       75
#define CLAUDE_AREA_H       155   /* Height for Claude face + status text */
#define CLAUDE_CX           120  /* Center X of Claude face */
#define CLAUDE_CY           150  /* Center Y of Claude face */

/* Status text area */
#define STATUS_TEXT_Y       230

/* Bottom info bar */
#define PC_INFO_BAR_Y       280
#define PC_INFO_BAR_H       40

/* Claude character colors (orange/warm palette) */
#define CLAUDE_BODY_COLOR   0xFD08  /* Warm orange */
#define CLAUDE_LIGHT_COLOR  0xFE95  /* Light orange highlight */
#define CLAUDE_DARK_COLOR   0xEAA4  /* Darker orange shade */
#define CLAUDE_OUTLINE      0x4228  /* Dark brown outline */
#define CLAUDE_EYE_WHITE    0xFFFF  /* White */
#define CLAUDE_EYE_PUPIL    0x0000  /* Black pupil */
#define CLAUDE_MOUTH_COLOR  0x4228  /* Dark brown mouth */

/* ---- PC Mode Render API ---- */
void Render_PCMode(void);              /* Full PC mode screen render */
void Render_PCStatusBar(void);         /* Top status bars (CPU/GPU/MEM) */
void Render_ClaudeCharacter(uint16_t cx, uint16_t cy, uint8_t frame, ClaudeStatus mood);  /* Claude face */
void Render_StatusText(const char *text, uint16_t color);  /* Bottom status text */

/* Cat colors by stage */
#define CAT_BABY_COLOR      0xFC08  /* Orange/ginger */
#define CAT_BABY2_COLOR     0xFD68  /* Light orange */
#define CAT_GROW_COLOR      0x8410  /* Gray */
#define CAT_MATURE_COLOR    0xFF40  /* Golden */
#define CAT_SECRET_COLOR    0xA0DF  /* Purple/space */
#define CAT_NOSE_COLOR      0xF9A7  /* Pink */
#define CAT_EAR_INNER       0xFAC7  /* Pink inner ear */
#define CAT_EYE_COLOR       0x0000  /* Black */
#define CAT_EYE_GREEN       0x07E0  /* Green eyes */
#define CAT_EYE_WHITE       0xFFFF  /* White */
#define CAT_WHISKER_COLOR   0xFFFF  /* White whiskers */
#define CAT_OUTLINE         0x0000  /* Black outline */
#define CAT_CROWN_COLOR     0xFFE0  /* Yellow crown */
#define CAT_STAR_COLOR       0xFFFF /* White stars */

void Render_Init(void);
void Render_DrawAll(void);
void Render_UpdateStatusBar(void);
void Render_DrawPet(uint8_t anim_frame);
void Render_ShowCommandResult(const char *msg);
void Render_DrawInfoBar(void);

#endif
