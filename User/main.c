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
#include "bsp_dht11.h"
#include "bsp_W25Q128.h"

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
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);

    Led_Init();
    Key_Init();
    W25Q128_Init();  /* Must init before LCD for Chinese font */
    LED_BLUE_ON;

    PC_Monitor_Init();
    Render_Init();
    Cmd_Init();

    /* Welcome */
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);
    LCD_String(40, 130, "Pixel Claude", 16, WHITE, BLACK);
    LCD_String(55, 170, "Serial Ready", 12, LIGHTBLUE, BLACK);
    Delay_ms(1500);

    /* Full clear before entering monitor mode */
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);
    Render_DrawAll();

    uint32_t last_render_ms = 0;
    uint32_t last_key_check_ms = 0;

    while (1)
    {
        Cmd_Process();

        /* Staleness check */
        if (g_sys_tick_ms - g_pc_data.last_update_ms > 3000) {
            g_pc_data.is_connected = 0;
        }

        /* Keys */
        if (g_sys_tick_ms - last_key_check_ms >= 50) {
            last_key_check_ms = g_sys_tick_ms;

            if (Key_Scan(KEY_1_GPIO, KEY_1_PIN, 1)) {
                UART1_SendString("KEY:PAUSE\r\n");
                Render_ShowCommandResult("Play/Pause");
            }
            if (Key_Scan(KEY_2_GPIO, KEY_2_PIN, 0)) {
                UART1_SendString("KEY:NEXT\r\n");
                Render_ShowCommandResult("Next Track");
            }
            if (Key_Scan(KEY_3_GPIO, KEY_3_PIN, 0)) {
                extern uint8_t g_display_page;
                g_display_page = !g_display_page;
                LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BLACK);
                if (g_display_page == 0)
                    Render_ShowCommandResult("PC Monitor");
                else
                    Render_ShowCommandResult("Calendar");
            }
        }

        /* DHT11 sensor (every 2s) */
        {
            static uint32_t last_dht11 = 0;
            if (g_sys_tick_ms - last_dht11 >= 2000) {
                last_dht11 = g_sys_tick_ms;
                uint8_t t, h;
                if (DHT11_Read(&t, &h)) {
                    g_pc_data.dht11_temp = t;
                    g_pc_data.dht11_humi = h;
                }
            }
        }

        /* Render (500ms) */
        if (g_sys_tick_ms - last_render_ms >= 500) {
            last_render_ms = g_sys_tick_ms;
            Render_DrawAll();
        }
    }
}
