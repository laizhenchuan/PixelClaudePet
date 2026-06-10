#ifndef __BSP_DHT11_H
#define __BSP_DHT11_H
#include "stm32f4xx.h"

/* DHT11 connected to PE3 */
#define DHT11_GPIO    GPIOE
#define DHT11_PIN     GPIO_Pin_3
#define DHT11_RCC     RCC_AHB1Periph_GPIOE

uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi);

#endif
