#include "bsp_dht11.h"

/* DWT-based microsecond delay (168MHz) */
static void dht11_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

static void dht11_pin_out(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin  = DHT11_PIN;
    g.GPIO_Mode = GPIO_Mode_OUT;
    g.GPIO_OType = GPIO_OType_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(DHT11_GPIO, &g);
}

static void dht11_pin_in(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin  = DHT11_PIN;
    g.GPIO_Mode = GPIO_Mode_IN;
    g.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(DHT11_GPIO, &g);
}

static uint8_t dht11_read_byte(void)
{
    uint8_t b = 0;
    uint8_t i;
    for (i = 0; i < 8; i++) {
        /* Wait for pin to go high (start of data bit) */
        uint32_t timeout = 1000;
        while (!GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN)) {
            if (--timeout == 0) return 0;
        }
        /* Measure high time: >40us = 1, <40us = 0 */
        dht11_delay_us(30);
        b <<= 1;
        if (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN))
            b |= 1;
        /* Wait for pin to go low */
        timeout = 1000;
        while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN)) {
            if (--timeout == 0) return 0;
        }
    }
    return b;
}

uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5] = {0};
    uint8_t i;

    /* Enable DWT cycle counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Enable GPIO clock */
    RCC_AHB1PeriphClockCmd(DHT11_RCC, ENABLE);

    /* Start signal: pull low 18ms, then high 30us, then release */
    dht11_pin_out();
    GPIO_ResetBits(DHT11_GPIO, DHT11_PIN);
    dht11_delay_us(18000);
    GPIO_SetBits(DHT11_GPIO, DHT11_PIN);
    dht11_delay_us(30);
    dht11_pin_in();

    /* Wait for DHT11 response: low ~80us */
    uint32_t timeout = 5000;
    while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN)) {
        if (--timeout == 0) return 0;
    }
    timeout = 5000;
    while (!GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN)) {
        if (--timeout == 0) return 0;
    }
    timeout = 5000;
    while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN)) {
        if (--timeout == 0) return 0;
    }

    /* Read 5 bytes */
    for (i = 0; i < 5; i++)
        buf[i] = dht11_read_byte();

    /* Verify checksum */
    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
        return 0;

    *humi = buf[0];
    *temp = buf[2];
    return 1;
}
