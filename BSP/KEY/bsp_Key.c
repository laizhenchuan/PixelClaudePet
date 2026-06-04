#include "bsp_key.h"






void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                    // 作用：配置引脚工作模式

    // 使能KEY_1所用引脚端口时钟；使用端口判断的方法使能时钟, 以使代码移植更方便
    if (KEY_1_GPIO == GPIOA)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    if (KEY_1_GPIO == GPIOB)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    if (KEY_1_GPIO == GPIOC)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    if (KEY_1_GPIO == GPIOD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    if (KEY_1_GPIO == GPIOE)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    if (KEY_1_GPIO == GPIOF)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    if (KEY_1_GPIO == GPIOG)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
    // 使能KEY_2所用引脚端口时钟；使用端口判断的方法使能时钟, 以使代码移植更方便
    if (KEY_2_GPIO == GPIOA)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    if (KEY_2_GPIO == GPIOB)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    if (KEY_2_GPIO == GPIOC)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    if (KEY_2_GPIO == GPIOD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    if (KEY_2_GPIO == GPIOE)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    if (KEY_2_GPIO == GPIOF)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    if (KEY_2_GPIO == GPIOG)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
    // 使能KEY_3所用引脚端口时钟；使用端口判断的方法使能时钟, 以使代码移植更方便
    if (KEY_3_GPIO == GPIOA)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    if (KEY_3_GPIO == GPIOB)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    if (KEY_3_GPIO == GPIOC)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    if (KEY_3_GPIO == GPIOD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    if (KEY_3_GPIO == GPIOE)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    if (KEY_3_GPIO == GPIOF)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    if (KEY_3_GPIO == GPIOG)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

    // 配置KEY_1引脚: PA0, 闲时下拉，按下置高电平
    GPIO_InitStructure.GPIO_Pin   = KEY_1_PIN;         // 选择要控制的引脚编号; 此处使用了宏定义，以方便移植修改
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;      // 引脚模式：输入模式
    GPIO_InitStructure.GPIO_PuPd  = KEY_1_PUPD;        // 上下拉状态：即在按键闲时，引脚所处电平状态
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;   // 引脚速率：2MHz
    GPIO_Init(KEY_1_GPIO, &GPIO_InitStructure);        // 调用库函数，使用上面的配置初始化GPIO
    // 配置KEY_2引脚: PA1, 闲时上拉，按下置低电平
    GPIO_InitStructure.GPIO_Pin   = KEY_2_PIN;         // 选择要控制的引脚编号; 此处使用了宏定义，以方便移植修改
    GPIO_InitStructure.GPIO_PuPd  = KEY_2_PUPD;        // 上下拉状态：即在按键闲时，引脚所处电平状态
    GPIO_Init(KEY_2_GPIO, &GPIO_InitStructure);        // 调用库函数，使用上面的配置初始化GPIO
    // 配置KEY_3引脚: PA4, 闲时上拉，按下置低电平
    GPIO_InitStructure.GPIO_Pin   = KEY_3_PIN;         // 选择要控制的引脚编号; 此处使用了宏定义，以方便移植修改
    GPIO_InitStructure.GPIO_PuPd  = KEY_3_PUPD;        // 上下拉状态：即在按键闲时，引脚所处电平状态
    GPIO_Init(KEY_3_GPIO, &GPIO_InitStructure);        // 调用库函数，使用上面的配置初始化GPIO
}



/******************************************************************************
 * 函  数： Key_Scan
 * 功  能： 扫描按键状态
 * 参  数： GPIOx：       GPIO端口
 *          PINx :        Pin引脚编号
 *          targetStatus: 目标电平(按下时期待得到的电平）
 *
 * 返回值： 1: 按下
 *          0：无动作
 *
 * 说  明： 1: 本函数检测方式为“while不断扫描”，发现按键按下且等待按键松开，为一次有效按下动作;
 *          2：本方法建议只用示例使用，实际项目中，建议使用中断方法检测;
 ******************************************************************************/
uint8_t Key_Scan(GPIO_TypeDef *GPIOx, uint16_t PINx, uint8_t targetStatus)
{
    if (GPIO_ReadInputDataBit(GPIOx, PINx) == targetStatus)
    {
        while (GPIO_ReadInputDataBit(GPIOx, PINx) == targetStatus);
        return 1;     // 如果检测到按下状态，和期待的目标电平相同，就返回：1
    }
    return 0;         // 如果检测到按下状态，和期待的目标电平不相同，就返回：0
}




