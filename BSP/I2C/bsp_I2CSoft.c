/***********************************************************************************************************************************
**【淘宝链接】  魔女开发板    https://demoboard.taobao.com
***********************************************************************************************************************************
**【文件名称】  bsp_IICSoft.c
**
**【功能描述】  模拟IIC时序
**              定义功能函数
**
**【更新记录】  2023-11-10  优化多字节读写代码
**              2020-03-05  创建
**              2021-05-03  完善文件格式、注释格式
**
***********************************************************************************************************************************/
#include "bsp_I2CSoft.h"






/************************************************
 ** 宏定义_引脚电平控制简化、增加可代码可读性
 ***********************************************/
#define  I2C_SCL_1       ( I2C_SCL_GPIO->BSRR = I2C_SCL_PIN )        // SCL 置高
#define  I2C_SCL_0       ( I2C_SCL_GPIO->BSRR = I2C_SCL_PIN << 16 )  // SCL 置低
#define  I2C_SDA_1       ( I2C_SDA_GPIO->BSRR = I2C_SDA_PIN )        // SDA 置高
#define  I2C_SDA_0       ( I2C_SDA_GPIO->BSRR = I2C_SDA_PIN << 16 )  // SDA 置低
                                                            

#define  I2C_SDA_READ()  ( I2C_SDA_GPIO->IDR & I2C_SDA_PIN )         // 读SDA电平状态






/*****************************************************************************
 * 全局宏定义
 * 不用修改
****************************************************************************/
#define  I2C_WR              0                 // 写控制位
#define  I2C_RD              1                 // 读控制位





/*****************************************************************************
 ** 内部函数 声明
*****************************************************************************/
static void delay(void);





/********************************************************************************************************
 * 函  数：delay
 * 功  能：I2C总线位延迟，最快400KHz
 * 参  数：无
 * 返回值：无
 * 备  注：系统时钟168MHz, 下面参数20~250都能正常通信
********************************************************************************************************/
static void delay(void)
{
    uint8_t i;
    for (i = 0; i < 30; i++);
}



/********************************************************************************************************
 * 函  数：I2CSoft_Start
 * 功  能：向总线产生开始信号(SCL高电平期间，SDA由高向低跳变)
 * 参  数：无
 * 返回值：无
*********************************************************************************************************/
void I2CSoft_Start(void)
{
    I2C_SDA_1;
    delay();  //
    I2C_SCL_1;
    delay();
    I2C_SDA_0;
    delay();
    I2C_SCL_0;
    delay();
}



/********************************************************************************************************
 * 函  数：I2CSoft_Start
 * 功  能：向总线产生停止信号(在SCL高电平其间，SDA由低向高跳变)
 * 参  数：无
 * 返回值：无
********************************************************************************************************/
void I2CSoft_Stop(void)
{
    I2C_SDA_0;
    delay();   //
    I2C_SCL_1;
    delay();
    I2C_SDA_1;
    delay();   //
}



/********************************************************************************************************
 * 函  数：I2CSoft_WaitAck
 * 功  能：向总线产生一个时钟，读取从机的ACK应答信号
 * 参  数：无
 * 返回值：0-应答、1-无应答
 ********************************************************************************************************/
uint8_t I2CSoft_WaitAck(void)
{
    uint8_t ack;

    I2C_SDA_1;           // 释放SDA总线
    delay();
    I2C_SCL_1;           // SC = 1, 等待器件会返回ACK应答
    delay();
    if (I2C_SDA_READ())  // 读取SDA电平
    {
        ack = 1;         // 高电平，即无应答
    }
    else
    {
        ack = 0;         // 低电平，即应答
    }
    I2C_SCL_0;           // 结束应答时钟
    delay();
    return ack;          // 返回：0-应答、1-无应答
}



/********************************************************************************************************
 * 函  数：I2CSoft_Ack
 * 功  能：向总线产生一个ACK信号
 * 参  数：无
 * 返回值：无
********************************************************************************************************/
void I2CSoft_Ack(void)
{
    I2C_SDA_0;
    delay();
    I2C_SCL_1;
    delay();
    I2C_SCL_0;
    delay();
    I2C_SDA_1;
}



/*******************************************************************************************************
 * 函  数：I2CSoft_NAck
 * 功  能：向总线产生1个不应答NACK信号
 * 参  数：无
 * 返回值：无
*******************************************************************************************************/
void I2CSoft_NAck(void)
{
    I2C_SDA_1;
    delay();
    I2C_SCL_1;
    delay();
    I2C_SCL_0;
    delay();
}



/********************************************************************************************************
 * 函  数：I2CSoft_SendByte
 * 功  能：向总线发送8bit数据
 * 参  数：uint8_t _uData： 发送的字节
 * 返回值：无
********************************************************************************************************/
void I2CSoft_SendByte(uint8_t _uData)
{
    for (uint8_t i = 0; i < 8; i++)  // 由高位到低位，逐位产生电平
    {
        if (_uData & 0x80)
            I2C_SDA_1;
        else
            I2C_SDA_0;
        _uData <<= 1;                // 左移1位
        delay();
        I2C_SCL_1;
        delay();
        I2C_SCL_0;
        delay();
        if (i == 7)                  // 最后一位
        {
            I2C_SDA_1;               // 释放总线
        }
    }
}



/********************************************************************************************************
 * 函  数：I2CSoft_ReadByte
 * 功  能：向总线读取8bit数据
 * 参  数：无
 * 返回值：读到的数据
*********************************************************************************************************/
uint8_t I2CSoft_ReadByte(void)
{
    uint8_t data = 0;

    for (uint8_t i = 0; i < 8; i++)  // 由高位读起
    {
        data <<= 1;
        I2C_SCL_1;
        delay();
        if (I2C_SDA_READ())
        {
            data++;
        }
        I2C_SCL_0;
        delay();
    }
    return data;
}



/*********************************************************************************************************
 * 函  数：I2CSoft_CheckDevice
 * 功  能：检查I2C从机设备是否有响应
 * 参  数：uint8_t _uAddr  设备的I2C总线地址
 * 返回值：0-检测到设备回应，1-未检测到设备回应
********************************************************************************************************/
uint8_t I2CSoft_CheckDevice(uint8_t _uAddr)
{
    uint8_t ucAck;

    I2CSoft_Stop();                    // 给总线一个停止信号
    I2CSoft_Start();                   // 启动信号

    I2CSoft_SendByte(_uAddr | I2C_WR); // 发送7位从机地址+读写控制位(0-写、1-读)
    delay();
    ucAck = I2CSoft_WaitAck();         // 检测设备应答信号：0-应答、1-无应答

    I2CSoft_Stop();                    // 发送停止信号
    return ucAck;                      // 0-检测到设备回应，1-未检测到设备回应
}



/*****************************************************************************
 * 函  数：I2CSoft_Init
 * 功  能：初始化模拟IIC引脚
 * 参  数：无
 * 返回值：无
*****************************************************************************/
void I2CSoft_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 使能SCL引脚端口时钟
    if (I2C_SCL_GPIO == GPIOA)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    if (I2C_SCL_GPIO == GPIOB)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    if (I2C_SCL_GPIO == GPIOC)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    if (I2C_SCL_GPIO == GPIOD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    if (I2C_SCL_GPIO == GPIOE)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    if (I2C_SCL_GPIO == GPIOF)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    if (I2C_SCL_GPIO == GPIOG)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
    // 使能SDA引脚端口时钟
    if (I2C_SDA_GPIO == GPIOA)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    if (I2C_SDA_GPIO == GPIOB)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    if (I2C_SDA_GPIO == GPIOC)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    if (I2C_SDA_GPIO == GPIOD)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    if (I2C_SDA_GPIO == GPIOE)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    if (I2C_SDA_GPIO == GPIOF)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    if (I2C_SDA_GPIO == GPIOG)  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = I2C_SCL_PIN ;      // SCL引脚编号
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 引脚反转速率
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;     // 工作模式：输出
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;     // 输出模式：开漏
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;      // 上下拉：无
    GPIO_Init(I2C_SCL_GPIO, &GPIO_InitStructure);      // 初始化

    GPIO_InitStructure.GPIO_Pin   =  I2C_SDA_PIN;      // SDA引脚编号
    GPIO_Init(I2C_SCL_GPIO, &GPIO_InitStructure);      // 初始化

    I2CSoft_Stop();                                    // 给总线一个停止信号
    // I2CSoft_Start();                                // 启动信号
}



/*********************************************************************************************************
 * 函  数：I2CSoft_ReadBytes
 * 功  能：读取指定数量的数据
 * 参  数：uint8_t     _uSlaveAddr  设备地址
 *         uint8_t     _uDataAddr   数据地址
 *         uint8_t    *_puDataBuf   存放数据的缓存地址
 *         uint16_t    _usNum       要读取的字节数
 * 返回值：0-失败，1-成功
********************************************************************************************************/
uint8_t I2CSoft_ReadBytes(uint8_t _uSlaveAddr, uint8_t _uDataAddr, uint8_t *_puDataBuf, uint16_t _usNum)
{
    I2CSoft_Start();                                // 1-开始信号

    I2CSoft_SendByte(_uSlaveAddr | I2C_WR);         // 2-发送从机地址+写控制，注意：这里是写控制 0-写、1-读
    if (I2CSoft_WaitAck())                          // 等待ACK。注意：每发送一个字节，都要确认从机应答
        goto ReadBytes_ACK_Fail;                    // 从机无应答, 跳到失败处理(产生停止信号、返回0)

    I2CSoft_SendByte((uint8_t)_uDataAddr);          // 3-发送数据地址。注意：24C02只有256字节，只需发送1个数据地址值，如果24C04或以上就要发送两个数据地址值
    if (I2CSoft_WaitAck())                          // 等待ACK。注意：每发送一个字节，都要确认从机应答
        goto ReadBytes_ACK_Fail;                    // 从机无应答, 跳到失败处理(产生停止信号、返回0)

    I2CSoft_Start();                                // 4-再次产生开始信号，开始读数据处理

    I2CSoft_SendByte(_uSlaveAddr | I2C_RD);         // 5-再次发送从机地址+写控制，注意：这里是读控制
    if (I2CSoft_WaitAck())                          // 等待ACK。注意：每发送一个字节，都要确认从机应答
        goto ReadBytes_ACK_Fail;                    // 从机无应答, 跳到失败处理(产生停止信号、返回0)

    for (uint16_t i = 0; i < _usNum; i++)           // 6-开始读取需要的字节数, 注意：发送要读写的首地址就可以连续地读取
    {
        _puDataBuf[i] = I2CSoft_ReadByte();         // 读字节
        if (i != _usNum - 1)                        // 每读1个字节，需要发送Ack，最后1个字节发送Nack
            I2CSoft_Ack();                          // 向总线产生ACK信号
        else
            I2CSoft_NAck();                         // 最后1个字节, 产生NACK信号
    }

    I2CSoft_Stop();                                 // 向总线发出停止信号
    return 1;                                       // 读取成功，返回 1

ReadBytes_ACK_Fail:                                 // 从机无应答时的处理
    I2CSoft_Stop();                                 // 重要：向总线发送停止信号
    return 0;                                       // 返回 0
}



/*****************************************************************************
 * 函  数：I2CSoft_WriteBytes
 * 功  能：写入多个字节
 * 参  数：uint8_t     _uSlaveAddr  设备地址
 *         uint8_t     _uDataAddr   数据地址
 *         uint8_t    *_puDataBuf   存放数据的缓存地址
 *         uint16_t    _usNum       要读取的字节数
 * 返回值：0-失败，1-成功
 * 特别地：本函数，未经验证，不同的设备可能有所不同
*****************************************************************************/
uint8_t I2CSoft_WriteBytes(uint8_t _uSlaveAddr, uint8_t _uDataAddr, uint8_t *_puDataBuf, uint16_t _usNum)
{
    I2CSoft_Start();                        // 起始信号

    I2CSoft_SendByte(_uSlaveAddr | 0);      // 发送从机地址,写方向, 0写1读
    if (I2CSoft_WaitAck())                  // 等待ACK。注意：每发送一个字节，都要确认从机应答
        goto WriteBytes_ACK_Fail;           // 从机无应答, 跳到失败处理(产生停止信号、返回0)

    I2CSoft_SendByte(_uDataAddr);           // 发送数据地址
    if (I2CSoft_WaitAck())                  // 等待ACK。注意：每发送一个字节，都要确认从机应答
        goto WriteBytes_ACK_Fail;           // 从机无应答, 跳到失败处理(产生停止信号、返回0)

    for (uint16_t i = 0; i < _usNum; i++)
    {
        I2CSoft_SendByte(_puDataBuf[i]);    // 发送数据
        if (I2CSoft_WaitAck())              // 等待ACK。注意：每发送一个字节，都要确认从机应答
            goto WriteBytes_ACK_Fail;       // 从机无应答, 跳到失败处理(产生停止信号、返回0)
        delay();
    }

    I2CSoft_Stop();                         // 向总线发出停止信号
    return 1;                               // 读取成功，返回 1

WriteBytes_ACK_Fail:                        // 从机无应答时的处理
    I2CSoft_Stop();                         // 重要：向总线发送停止信号
    return 0;                               // 返回 0
}


