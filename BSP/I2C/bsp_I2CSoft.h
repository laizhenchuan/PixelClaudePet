#ifndef __I2C_MONI_H
#define __I2C_MONI_H
/***********************************************************************************************************************************
 **【淘宝链接】  魔女开发板    https://demoboard.taobao.com
 ***********************************************************************************************************************************
 **【文件名称】  bsp_I2CSoft.h
 **【功能描述】  模拟IIC时序
 **              定义引脚、定义全局结构体、声明全局函数
 **
 **【适用平台】  STM32F407 + 标准库v1.8 + keil5
 **
 **【移植说明】  引脚修改：在bsp_I2CSoft.h文件中修改，以方便I2C总线复用、代码复用
 **              器件地址：在各设备文件中修改
 **
 **【更新记录】  2023-11-10  优化多字节读写代码
 **              2021-05-03  完善文件格式、注释格式
 **              2020-03-05  创建
 **
***********************************************************************************************************************************/
#include "stm32f4xx.h"
#include <inttypes.h>



/*****************************************************************************
 * 移植修改区
 * 用户修改下面引脚配置，即可改变SCL和SDA所用引脚
 * 本驱动文件使用模拟I2C，所用引脚无需具有硬件I2C功能
****************************************************************************/
#define  I2C_SCL_GPIO        GPIOB             // SCL引脚的GPIO端口
#define  I2C_SCL_PIN         GPIO_Pin_6        // SCL引脚编号
                                               
#define  I2C_SDA_GPIO        GPIOB             // SDA引脚的GPIO端口
#define  I2C_SDA_PIN         GPIO_Pin_7        // SDA引脚编号





/*****************************************************************************
 * 全局函数 声明
****************************************************************************/
void     I2CSoft_Init(void);                   // 初始化所用引脚
void     I2CSoft_Start(void);                  // 开始信号
void     I2CSoft_Stop(void);                   // 停止信号
uint8_t  I2CSoft_WaitAck(void);                // 获取从机回答信号：0-应答、1-无应答
void     I2CSoft_Ack(void);                    // 产生回答信号
void     I2CSoft_NAck(void);                   // 产生不回答信号
uint8_t  I2CSoft_ReadByte(void);               // 读取1个字节
void     I2CSoft_SendByte(uint8_t _uData);     // 写入1个字节
uint8_t  I2CSoft_CheckDevice(uint8_t _uAddr);  // 检查从机是否有回应：0-检测到设备回应，1-未检测到设备回应
uint8_t  I2CSoft_WriteBytes(uint8_t _uSlaveAddr, uint8_t _uDataAddr, uint8_t *_puDataBuf, uint16_t _usNum);  // 写入多个字节; 从机地址，数据地址，缓存地址，字节数量
uint8_t  I2CSoft_ReadBytes (uint8_t _uSlaveAddr, uint8_t _uDataAddr, uint8_t *_puDataBuf, uint16_t _usNum);  // 读取多个字节; 从机地址，数据地址，缓存地址，字节数量



#endif

