#include "pet_command.h"
#include "pet_core.h"
#include "pet_render.h"
#include "pet_save.h"
#include "bsp_UART.h"
#include <string.h>
#include <stdio.h>
#include "pc_monitor.h"

#define CMD_BUF_SIZE    256
#define COOLDOWN_TICKS  150  /* 15 seconds at 100ms/tick */

static char g_cmd_buf[CMD_BUF_SIZE];
static uint8_t g_cmd_len = 0;
static uint16_t g_cooldown = 0;
static uint8_t g_in_json = 0;  /* 1 = current line starts with '{' */

void Cmd_Init(void)
{
    UART1_Init(115200);
    memset(g_cmd_buf, 0, CMD_BUF_SIZE);
    g_cmd_len = 0;
    g_cooldown = 0;

    UART1_SendString("\r\n================================\r\n");
    UART1_SendString("  Pixel Claude Pet v2.0\r\n");
    UART1_SendString("  Type 'help' for commands\r\n");
    UART1_SendString("================================\r\n");
}

static void Cmd_Execute(const char *cmd)
{
    char response[80];

    if (g_cooldown > 0) {
        snprintf(response, sizeof(response), "[CD] Wait %d sec", (g_cooldown + 9) / 10);
        UART1_SendString("%s\r\n", response);
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        UART1_SendString("\r\n--- Commands ---\r\n");
        UART1_SendString(" feed    - Feed +%d hunger\r\n", PET_FEED_AMOUNT);
        UART1_SendString(" play    - Play +%d mood, -%d energy\r\n", PET_PLAY_MOOD_AMT, PET_PLAY_ENERGY_COST);
        UART1_SendString(" clean   - Clean +%d hygiene\r\n", PET_CLEAN_AMOUNT);
        UART1_SendString(" sleep   - Sleep +%d energy\r\n", PET_SLEEP_ENERGY_AMT);
        UART1_SendString(" status  - Show all stats\r\n");
        UART1_SendString(" name X  - Rename pet\r\n");
        UART1_SendString(" info    - Pet info card\r\n");
        UART1_SendString(" save    - Force save\r\n");
        UART1_SendString(" pc      - PC monitor mode\r\n");
        UART1_SendString(" pet     - Pet game mode\r\n");
        UART1_SendString(" help    - This list\r\n");
        UART1_SendString("-----------------\r\n");
        return;
    }

    if (strcmp(cmd, "status") == 0) {
        char buf[512];
        Pet_GetStatusString(buf, sizeof(buf));
        UART1_SendString("%s", buf);
        return;
    }

    if (strcmp(cmd, "feed") == 0) {
        Pet_Feed();
        snprintf(response, sizeof(response), "[OK] Fed! Hunger +%d -> %d",
                 PET_FEED_AMOUNT, g_pet.hunger);
        UART1_SendString("%s\r\n", response);
        Render_ShowCommandResult("Fed!");
        g_cooldown = COOLDOWN_TICKS;
        Pet_Save();
        return;
    }

    if (strcmp(cmd, "play") == 0) {
        Pet_Play();
        snprintf(response, sizeof(response), "[OK] Played! Mood +%d -> %d, Energy -%d",
                 PET_PLAY_MOOD_AMT, g_pet.mood, PET_PLAY_ENERGY_COST);
        UART1_SendString("%s\r\n", response);
        Render_ShowCommandResult("Played!");
        g_cooldown = COOLDOWN_TICKS;
        Pet_Save();
        return;
    }

    if (strcmp(cmd, "clean") == 0) {
        Pet_Clean();
        snprintf(response, sizeof(response), "[OK] Cleaned! Hygiene +%d -> %d",
                 PET_CLEAN_AMOUNT, g_pet.hygiene);
        UART1_SendString("%s\r\n", response);
        Render_ShowCommandResult("Cleaned!");
        g_cooldown = COOLDOWN_TICKS;
        Pet_Save();
        return;
    }

    if (strcmp(cmd, "sleep") == 0) {
        Pet_Sleep();
        snprintf(response, sizeof(response), "[OK] Slept! Energy +%d -> %d",
                 PET_SLEEP_ENERGY_AMT, g_pet.energy);
        UART1_SendString("%s\r\n", response);
        Render_ShowCommandResult("Slept!");
        g_cooldown = COOLDOWN_TICKS;
        Pet_Save();
        return;
    }

    if (strncmp(cmd, "name ", 5) == 0) {
        const char *new_name = cmd + 5;
        if (strlen(new_name) > 0 && strlen(new_name) < 12) {
            strcpy(g_pet.name, new_name);
            snprintf(response, sizeof(response), "[OK] Name changed to '%s'", g_pet.name);
            UART1_SendString("%s\r\n", response);
            Pet_Save();
        } else {
            UART1_SendString("[ERR] Name must be 1-11 chars\r\n");
        }
        return;
    }

    if (strcmp(cmd, "info") == 0) {
        const char *stage_names[] = {"Kitten", "Cat", "King Cat", "Space Cat"};
        const char *mood_names[] = {"Happy", "Sad", "Hungry", "Sleepy", "Angry", "Sick"};
        const char *stage_desc[] = {
            "A tiny kitten with big curious eyes!",
            "A sleek cat with a dashing bow tie!",
            "A regal king cat with a golden crown!",
            "A cosmic cat surrounded by stars!"
        };
        UART1_SendString("\r\n=== %s the %s Slime ===\r\n", g_pet.name, stage_names[g_pet.stage]);
        UART1_SendString(" Lv: %lu | Mood: %s | Age: %lus\r\n",
                         (unsigned long)(g_pet.experience / 10),
                         mood_names[g_pet.current_mood],
                         (unsigned long)g_pet.age_seconds);
        UART1_SendString(" %s\r\n", stage_desc[g_pet.stage]);
        if (g_pet.is_sick) {
            UART1_SendString(" STATUS: SICK - Care needed!\r\n");
        }
        UART1_SendString("=========================\r\n");
        return;
    }

    if (strcmp(cmd, "save") == 0) {
        if (Pet_Save()) {
            UART1_SendString("[OK] Progress saved!\r\n");
        } else {
            UART1_SendString("[ERR] Save failed!\r\n");
        }
        return;
    }

    if (strcmp(cmd, "pc") == 0) {
        g_pet.is_pc_mode = 1;
        UART1_SendString("[OK] Switched to PC Monitor mode\r\n");
        return;
    }

    if (strcmp(cmd, "pet") == 0) {
        g_pet.is_pc_mode = 0;
        UART1_SendString("[OK] Switched to Pet mode\r\n");
        return;
    }

    /* Unknown command */
    UART1_SendString("[?] Unknown. Type 'help'\r\n");
}

void Cmd_Process(void)
{
    /* Cooldown tick */
    if (g_cooldown > 0) {
        g_cooldown--;
    }

    /* Check for received data */
    uint16_t rx_num = UART1_GetRxNum();
    if (rx_num == 0) return;

    uint8_t *rx_data = UART1_GetRxData();

    for (uint16_t i = 0; i < rx_num; i++) {
        char c = rx_data[i];

        if (c == '\r' || c == '\n') {
            if (g_cmd_len > 0) {
                g_cmd_buf[g_cmd_len] = '\0';

                if (g_in_json) {
                    /* Process JSON line silently */
                    int8_t result = PC_Monitor_Parse(g_cmd_buf, g_cmd_len);
                    if (result == 1) {
                        extern volatile uint32_t g_sys_tick_ms;
                        g_pc_data.last_update_ms = g_sys_tick_ms;
                    }
                    g_in_json = 0;
                } else {
                    /* Regular command — echo newline then execute */
                    UART1_SendString("\r\n");
                    Cmd_Execute(g_cmd_buf);
                }
                g_cmd_len = 0;
            }
        } else if (c == '\b' || c == 0x7F) {
            if (g_cmd_len > 0) {
                g_cmd_len--;
                if (!g_in_json) {
                    UART1_SendString("\b \b");
                }
            }
        } else if (g_cmd_len < CMD_BUF_SIZE - 1 && c >= ' ') {
            if (g_cmd_len == 0 && c == '{') {
                g_in_json = 1;
            }
            g_cmd_buf[g_cmd_len++] = c;
            if (!g_in_json) {
                /* Echo human commands */
                UART1_SendData((uint8_t *)&c, 1);
            }
        }
    }

    UART1_ClearRx();
}

uint8_t Cmd_IsCooldown(void)
{
    return (g_cooldown > 0) ? 1 : 0;
}

uint16_t Cmd_GetCooldownSec(void)
{
    return (g_cooldown + 9) / 10;
}
