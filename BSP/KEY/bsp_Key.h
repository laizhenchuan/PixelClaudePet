#ifndef _BSP_KEY_H
#define _BSP_KEY_H
#include "stm32f4xx.h"



/*****************************************************************************
 ** 移植配置区
****************************************************************************/
// KEY_1_WKUP, 闲时下拉，按下时被置高电平
#define KEY_1_GPIO                GPIOA              // 引脚所用端口
#define KEY_1_PIN                 GPIO_Pin_0         // 引脚编号
#define KEY_1_PUPD                GPIO_PuPd_DOWN     // 闲时应置电平状态
// KEY_2, 闲时上拉，按下时被置低电平
#define KEY_2_GPIO                GPIOA              // 引脚所用端口
#define KEY_2_PIN                 GPIO_Pin_1         // 引脚编号
#define KEY_2_PUPD                GPIO_PuPd_UP       // 闲时应置电平状态
// KEY_2, 闲时上拉，按下时被置低电平
#define KEY_3_GPIO                GPIOA              // 引脚所用端口
#define KEY_3_PIN                 GPIO_Pin_4         // 引脚编号
#define KEY_3_PUPD                GPIO_PuPd_UP       // 闲时应置电平状态
   


/*****************************************************************************
 ** 声明全局函数
****************************************************************************/
void    Key_Init(void);  // 使用h文件中的参数，初始化引脚
uint8_t Key_Scan(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t targetStatus);  // 引脚端口、引脚编号、期待电平

#endif

