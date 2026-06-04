/**==================================================================================================================
 ** File: main.c
 ** Project: Pixel Slime Pet
 ** Platform: STM32F407VET6 + ILI9341 2.8" LCD
 **==================================================================================================================
 ** Description: Electronic pet (Tamagotchi-style) with serial console control
 **              - LCD shows pet animations and status
 **              - Serial commands via USART1 (115200-8N1)
 **              - Keys for quick actions (K1=feed, K2=play, K3=clean)
 **              - Saves progress to EEPROM (24C02)
 **==================================================================================================================
 ** Pin Map:
 **   LED Red:  PC5 (active low)
 **   LED Blue: PB2 (active low)
 **   KEY1:     PA0 (pulldown, press=low)
 **   KEY2:     PA1 (pullup, press=low)
 **   KEY3:     PA4 (pullup, press=low)
 **   USART1:   TX=PA9, RX=PA10 (onboard USB-TTL)
 **   I2C(24C02): SCL=PB6, SDA=PB7 (software I2C)
 **   LCD:      FSMC interface (see bsp_LCD_ILI9341)
 **==================================================================================================================*/
#include "stm32f4xx.h"
#include "bsp_LED.h"
#include "bsp_Key.h"
#include "bsp_UART.h"
#include "bsp_24C02.h"
#include "pet_core.h"
#include "pet_render.h"
#include "pet_command.h"
#include "pet_save.h"
#include "pc_monitor.h"

volatile uint32_t g_sys_tick_ms = 0;
volatile uint8_t  g_tick_100ms = 0;

void SysTick_Handler(void)
{
    g_sys_tick_ms++;
    /* 100ms tick for pet logic */
    static uint8_t div = 0;
    div++;
    if (div >= 100) {
        div = 0;
        g_tick_100ms = 1;
    }
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = g_sys_tick_ms;
    while ((g_sys_tick_ms - start) < ms);
}

int main(void)
{
    /* System init */
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);  /* 1ms tick */

    /* BSP init */
    Led_Init();
    Key_Init();

    /* Pet init */
    Pet_Init();

    /* PC Monitor init */
    PC_Monitor_Init();

    /* Try to load saved data */
    if (Pet_Save_Init()) {
        if (!Pet_Load()) {
            /* No valid save, start fresh */
            LED_RED_ON;
            Delay_ms(200);
            LED_RED_OFF;
        } else {
            /* Loaded OK - quick blue blink */
            LED_BLUE_ON;
            Delay_ms(200);
            LED_BLUE_OFF;
        }
    }

    /* LCD init */
    Render_Init();

    /* Serial console init */
    Cmd_Init();

    /* Welcome screen */
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", g_pet.name);
    LCD_String(60, 140, buf, 24, GREEN, BLACK);
    LCD_String(30, 180, "Pixel Claude Pet", 16, WHITE, BLACK);
    LCD_String(45, 210, "Serial Ready", 12, LIGHTBLUE, BLACK);
    Delay_ms(1500);

    /* Initial render */
    Render_DrawAll();

    uint32_t last_render_ms = 0;
    uint32_t last_save_ms = 0;
    uint32_t last_key_check_ms = 0;

    while (1)
    {
        /* --- 100ms tick for pet logic --- */
        if (g_tick_100ms) {
            g_tick_100ms = 0;
            Pet_Tick100ms();

            /* LED indication */
            if (g_pet.is_pc_mode) {
                /* PC mode: blue LED steady on */
                LED_RED_OFF;
                LED_BLUE_ON;
            } else if (g_pet.is_sick) {
                if (g_sys_tick_ms % 500 < 250) {
                    LED_RED_ON;
                } else {
                    LED_RED_OFF;
                }
                LED_BLUE_OFF;
            } else {
                LED_RED_OFF;
                LED_BLUE_OFF;
            }
        }

        /* --- Serial command processing --- */
        Cmd_Process();

        /* --- PC data staleness check (3s timeout) --- */
        if (g_sys_tick_ms - g_pc_data.last_update_ms > 3000) {
            g_pc_data.is_connected = 0;
        }

        /* --- Key handling (every 50ms) --- */
        if (g_sys_tick_ms - last_key_check_ms >= 50) {
            last_key_check_ms = g_sys_tick_ms;

            /* KEY1: Feed (pet) / Yes (PC mode) */
            if (Key_Scan(KEY_1_GPIO, KEY_1_PIN, 1)) {
                if (g_pet.is_pc_mode) {
                    UART1_SendString("KEY:YES\r\n");
                    Render_ShowCommandResult("Yes (K1)");
                } else if (!Cmd_IsCooldown()) {
                    Pet_Feed();
                    Render_ShowCommandResult("Fed! (K1)");
                    Pet_Save();
                }
            }
            /* KEY2: Play (pet) / No (PC mode) */
            if (Key_Scan(KEY_2_GPIO, KEY_2_PIN, 0)) {
                if (g_pet.is_pc_mode) {
                    UART1_SendString("KEY:NO\r\n");
                    Render_ShowCommandResult("No (K2)");
                } else if (!Cmd_IsCooldown()) {
                    Pet_Play();
                    Render_ShowCommandResult("Play! (K2)");
                    Pet_Save();
                }
            }
            /* KEY3: Clean */
            if (Key_Scan(KEY_3_GPIO, KEY_3_PIN, 0)) {
                if (!g_pet.is_pc_mode && !Cmd_IsCooldown()) {
                    Pet_Clean();
                    Render_ShowCommandResult("Clean! (K3)");
                    Pet_Save();
                }
            }
        }

        /* --- LCD refresh (every 200ms) --- */
        if (g_sys_tick_ms - last_render_ms >= 200) {
            last_render_ms = g_sys_tick_ms;
            Render_DrawAll();
        }

        /* --- Auto-save (every 5 seconds) --- */
        if (g_sys_tick_ms - last_save_ms >= 5000) {
            last_save_ms = g_sys_tick_ms;
            Pet_Save();
        }
    }
}
