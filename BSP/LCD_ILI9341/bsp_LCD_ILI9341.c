#include "bsp_LCD_ILI9341.h"
#include "FONT.H"



/*****************************************************************************
 ** ȫ����Ч    ����������
****************************************************************************/
xLCD_TypeDef xLCD = {0};                                    // ����LCD��Ҫ����





/*****************************************************************************
 ** ������Ч    ����������
****************************************************************************/
#define LCD_BL_ON    LCD_BL_GPIO-> BSRR = LCD_BL_PIN;       // �������ţ��øߵ�ƽ
#define LCD_BL_OFF   LCD_BL_GPIO-> BSRR = LCD_BL_PIN << 16; // �������ţ��õ͵�ƽ


static void setCursor(uint16_t Xpos, uint16_t Ypos);        // ���ù��

volatile typedef struct                                     // LCD��ַ�ṹ��
{
    uint16_t LCD_REG;
    uint16_t LCD_RAM;
} LCD_TypeDef;
// ʹ��NOR/SRAM�� Bank1.sector1,��ַλHADDR[27,26]=11 A6��Ϊ��������������
// ע������ʱSTM32�ڲ�������һλ����! 111 1110=0X7E
#define LCD       ((LCD_TypeDef *) 0x6001FFFE)              // (0x60000000 | 0x0001FFFE)




// us��ʱ
static void delay_us(volatile uint32_t times)   // ����һ��us��ʱ������������ֲʱ���ⲿ�ļ�����; ����������������ʱʹ�ã������Ǿ�׼��ʱ;
{
    times = times * 20;
    while (--times)
        __ASM volatile ("nop");
}



/******************************************************************************
 * ��  ���� delay_ms
 * ��  �ܣ� ms ��ʱ����
 * ��  ע�� 1��ϵͳʱ��168MHz
 *          2���򹴣�Options/ c++ / One ELF Section per Function
            3�������Ż�����Level 3(-O3)
 * ��  ���� uint32_t  ms  ����ֵ
 * ����ֵ�� ��
 ******************************************************************************/
static volatile uint32_t ulTimesMS;         // ʹ��volatile��������ֹ�������������Ż�
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
        ulTimesMS--;                        // �����ⲿ��������ֹ��ѭ�����������Ż���
}



// ���Ĵ���;
uint16_t  readReg(uint16_t  LCD_Reg)
{
    LCD->LCD_REG = LCD_Reg;                 // д��Ҫ���ļĴ������
    delay_us(5);
    return LCD->LCD_RAM;                    // ���ض�����ֵ
}



// BGRת��RGBֵ; ����ֵ��RGB��ʽ����ɫֵ
uint16_t  LCD_BGR2RGB(uint16_t  c)
{
    uint16_t   r, g, b, rgb;
    b = (c >> 0) & 0x1f;
    g = (c >> 5) & 0x3f;
    r = (c >> 11) & 0x1f;
    rgb = (b << 11) + (g << 5) + (r << 0);
    return (rgb);
}



// ��ȡ��ĳ�����ɫֵ; x,y:���ꡢ����ֵ:�˵����ɫ
uint16_t  LCD_ReadPoint(uint16_t  x, uint16_t  y)
{
    uint16_t  r = 0, g = 0, b = 0;
    if (x >= xLCD.width || y >= xLCD.height)return 0; // �����˷�Χ,ֱ�ӷ���
    setCursor(x, y);
    LCD->LCD_REG = 0X2E;                              // ���Ͷ�GRAMָ��

    r = LCD->LCD_RAM;                                 // dummy Read

    delay_us(20);
    r = LCD->LCD_RAM;                                 // ʵ��������ɫ

    delay_us(20);
    b = LCD->LCD_RAM;
    g = r & 0XFF;                                     // ��һ�ζ�ȡ����RG��ֵ,R��ǰ,G�ں�,��ռ8λ
    g <<= 8;

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}



//LCD������ʾ
void LCD_DisplayOn(void)
{
    LCD->LCD_REG = 0X29;  // ������ʾ
}



//LCD�ر���ʾ
void LCD_DisplayOff(void)
{
    LCD->LCD_REG = 0X28;  // �ر���ʾ
}



//���ù��λ��; Xpos:�����ꡢYpos:������
static void setCursor(uint16_t  Xpos, uint16_t  Ypos)
{
    LCD->LCD_REG = 0X2A;
    LCD->LCD_RAM = Xpos >> 8;
    LCD->LCD_RAM = Xpos & 0XFF;
    LCD->LCD_REG = 0X2B;
    LCD->LCD_RAM = Ypos >> 8;
    LCD->LCD_RAM = Ypos & 0XFF;
}



/******************************************************************
 * �������� LCD_DrawPoint
 * ��  �ܣ� ���㺯��
 * ��  ���� x,y:    ����
 *          _color: �˵����ɫ
 * ��  ע��
 *****************************************************************/
void LCD_DrawPoint(uint16_t  x, uint16_t  y, uint16_t _color)
{
    // ���迼���Ƿ����÷�Χ����
    // if( x < xLCD.width && y < xLCD.height)

    LCD->LCD_REG = 0X2A;        // ����x����
    LCD->LCD_RAM = x >> 8;
    LCD->LCD_RAM = x & 0XFF;
    LCD->LCD_REG = 0X2B;        // ����y����
    LCD->LCD_RAM = y >> 8;
    LCD->LCD_RAM = y & 0XFF;
    LCD->LCD_REG = 0X2C;        // ��ʼдGRAM
    LCD->LCD_RAM = _color;
}



/******************************************************************
 * �������� LCD_Init
 * ��  �ܣ� ��ʼ��LCD����������оƬILI9341
 * ��  ����
 * ��  ע��
 *****************************************************************/

void LCD_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStruct = {0};                 // GPIO���Ź������ýṹ��

    xLCD.FlagInit = 0;                                       // LCD��ʼ���ɹ���־

    // ʹ��GPIO�˿�
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN ;  // ʹ��GPIOA��B��C��D��ʱ��

#ifdef USE_HAL_DRIVER                                        // HAL�� ����
    /// ��ʼ������-����
    GPIO_InitStruct.Pin   = LCD_BL_PIN;                      // ���ű��
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;             // ���Ź���ģʽ���������
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                     // �ڲ�������������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // ���ŷ�ת���ʣ����
    HAL_GPIO_Init(LCD_BL_GPIO, &GPIO_InitStruct);            // ��ʼ��
    // ͨ������ GPIOD����
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                  // ���Ź���ģʽ�������������
    GPIO_InitStruct.Pull = GPIO_PULLUP;                      // �ڲ�������������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // ���ŷ�ת���ʣ����
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;              // ���Ź��ܣ�FSMC
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);                  // ��ʼ��
    // ͨ������ GPIOE����
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 ;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                  // ���Ź���ģʽ�������������
    GPIO_InitStruct.Pull = GPIO_PULLUP;                      // �ڲ�������������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // ���ŷ�ת���ʣ����
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;              // ���Ź��ܣ�FSMC
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);                  // ��ʼ��
#endif

#ifdef USE_STDPERIPH_DRIVER                                  // ��׼��
    // ��ʼ������-����
    GPIO_InitStruct.GPIO_Pin = LCD_BL_PIN;                   // ����
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;               // ��ͨ���ģʽ
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;              // �������
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;           // 50MHz
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;                // ����
    GPIO_Init(LCD_BL_GPIO, &GPIO_InitStruct);                // ��ʼ�� ,�������,���Ʊ���
    // ��ʼ������-GPIOD����
    GPIO_InitStruct.GPIO_Pin   = (3 << 0) | (3 << 4) | (7 << 8) | (3 << 14); // PD0,1,4,5,8,9,10,14,15 AF OUT
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF;               // �������
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;              // �������
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;          // 100MHz
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_UP;               // ����
    GPIO_Init(GPIOD, &GPIO_InitStruct);                      // ��ʼ��
    // ��ʼ������-GPIOE����
    GPIO_InitStruct.GPIO_Pin = (0X1FF << 7);                 // PE7~15,AF OUT
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;                // �������
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;              // �������
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;          // 100MHz
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;                // ����
    GPIO_Init(GPIOE, &GPIO_InitStruct);                      // ��ʼ��
    // ��ʼ������-RS_PD11
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;                  // RS
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;                // �������
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;              // �������
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;          // 100MHz
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;                // ����
    GPIO_Init(GPIOD, &GPIO_InitStruct);                      // ��ʼ��
    // ��ʼ������-NE1_PD7
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;                   // PD7, FSMC_NE1
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;                // �������
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;              // �������
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;          // 100MHz
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;                // ����
    GPIO_Init(GPIOD, &GPIO_InitStruct);                      // ��ʼ��
    // �������ŵĹ���
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF_FSMC);  // D2
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_FSMC);  // D3
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource4, GPIO_AF_FSMC);  // NOE_RD
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_FSMC);  // NWE_WE
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_FSMC);  // D13
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_FSMC);  // D14
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF_FSMC); // D15
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_FSMC); // D0
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_FSMC); // D1
    // �������ŵĹ���
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource7, GPIO_AF_FSMC);  //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8, GPIO_AF_FSMC);  //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_FSMC);  //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF_FSMC); //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_FSMC); //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, GPIO_AF_FSMC); //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_FSMC); //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_FSMC); //
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, GPIO_AF_FSMC); //
    // �������ŵĹ���
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource11, GPIO_AF_FSMC); // RS
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource7, GPIO_AF_FSMC);  // CS
#endif

    // ʹ��FSMCʱ��
    RCC->AHB3ENR |= RCC_AHB3ENR_FSMCEN;                      // ʹ������FSMC��ʱ��

    // ÿ��BANK������1~4, Ҫ����3���Ĵ���
    // ����1:BTCR0��1��BWTR0;
    // ����1:BTCR2��3��BWTR1;
    // ����2��BTCR4��5��BWTR2;
    // ����4��BTCR6��7��BWTR3;
    FSMC_Bank1->BTCR[0]   = 0X00000000;
    FSMC_Bank1->BTCR[0 + 1] = 0X00000000;
    FSMC_Bank1E->BWTR[0]  = 0X00000000;

    //����BCR�Ĵ���    ʹ���첽ģʽ
    FSMC_Bank1->BTCR[0] |= 0x01 << 12;        // �洢��дʹ��
    FSMC_Bank1->BTCR[0] |= 0x01 << 14;        // ��дʹ�ò�ͬ��ʱ��
    FSMC_Bank1->BTCR[0] |= 0x01 << 4;         // �洢�����ݿ���Ϊ16bit

    //��ʱ����ƼĴ���
    FSMC_Bank1->BTCR[0 + 1] |= 0x00 << 28;    // ģʽA
    FSMC_Bank1->BTCR[0 + 1] |= 0X0F << 0;     // ��ַ����ʱ��(ADDSET)Ϊ15��HCLK 1/168M=6ns*15=90ns
    FSMC_Bank1->BTCR[0 + 1] |= 0x3C << 8;     // ���ݱ���ʱ��(DATAST)Ϊ60��HCLK    =6*60=360ns
    //дʱ����ƼĴ���
    FSMC_Bank1E->BWTR[0] |= 0x00 << 28;       // ģʽA
    FSMC_Bank1E->BWTR[0] |= 0x09 << 0;        // ��ַ����ʱ��(ADDSET)Ϊ9��HCLK=54ns
    FSMC_Bank1E->BWTR[0] |= 0x08 << 8;        // ���ݱ���ʱ��(DATAST)Ϊ6ns*9��HCLK=54ns

    //ʹ��BANK1������1
    FSMC_Bank1->BTCR[0] |= 0x01;              // ʹ��BANK1������1

    delay_ms(50);                             // delay 50 ms
    LCD->LCD_REG = 0x0000;                    // д��Ҫд�ļĴ������
    LCD->LCD_RAM = 0x0000;                    // д������
    delay_ms(50);                             // delay 50 ms
    xLCD.id = readReg(0x0000);

    LCD->LCD_REG = 0XD3;                      // ����9341 ID�Ķ�ȡ
    xLCD.id = LCD->LCD_RAM;                   // dummy read
    xLCD.id = LCD->LCD_RAM;                   // ����0X00
    xLCD.id = LCD->LCD_RAM;                   // ��ȡ93
    xLCD.id <<= 8;
    xLCD.id |= LCD->LCD_RAM;                  // ��ȡ41
    printf("��ʾ�����               оƬID: %x\r\n", xLCD.id);  // ��ӡ��ʾ������оƬ��ID

    //��������дʱ����ƼĴ�����ʱ��
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 0);      // ��ַ����ʱ��(ADDSET)����
    FSMC_Bank1E->BWTR[0] |= 2 << 8;           // ���ݱ���ʱ��(DATAST)Ϊ6ns*3��HCLK=18ns
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 8);      // ���ݱ���ʱ������
    FSMC_Bank1E->BWTR[0] |= 3 << 0;           // ��ַ����ʱ��(ADDSET)Ϊ3��HCLK =18ns

    // ���Ĳ������ã������޸ģ������Ҳ�������
    LCD->LCD_REG = 0xCF;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0xC1;
    LCD->LCD_RAM = 0X30;
    LCD->LCD_REG = 0xED;
    LCD->LCD_RAM = 0x64;
    LCD->LCD_RAM = 0x03;
    LCD->LCD_RAM = 0X12;
    LCD->LCD_RAM = 0X81;
    LCD->LCD_REG = 0xE8;
    LCD->LCD_RAM = 0x85;
    LCD->LCD_RAM = 0x10;
    LCD->LCD_RAM = 0x7A;
    LCD->LCD_REG = 0xCB;
    LCD->LCD_RAM = 0x39;
    LCD->LCD_RAM = 0x2C;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x34;
    LCD->LCD_RAM = 0x02;
    LCD->LCD_REG = 0xF7;
    LCD->LCD_RAM = 0x20;
    LCD->LCD_REG = 0xEA;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0xC0;  // Power control
    LCD->LCD_RAM = 0x1B;  // VRH[5:0]
    LCD->LCD_REG = 0xC1;  // Power control
    LCD->LCD_RAM = 0x01;  // SAP[2:0];BT[3:0]
    LCD->LCD_REG = 0xC5;  // VCM control
    LCD->LCD_RAM = 0x30;  // 3F
    LCD->LCD_RAM = 0x30;  // 3C
    LCD->LCD_REG = 0xC7;  // VCM control2
    LCD->LCD_RAM = 0XB7;
    LCD->LCD_REG = 0x36;  // Memory Access Control
    LCD->LCD_RAM = 0x48;
    LCD->LCD_REG = 0x3A;
    LCD->LCD_RAM = 0x55;
    LCD->LCD_REG = 0xB1;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x1A;
    LCD->LCD_REG = 0xB6;  // Display Function Control
    LCD->LCD_RAM = 0x0A;
    LCD->LCD_RAM = 0xA2;
    LCD->LCD_REG = 0xF2;  // 3Gamma Function Disable
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0x26;  // Gamma curve selected
    LCD->LCD_RAM = 0x01;
    LCD->LCD_REG = 0xE0;  // Set Gamma
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x2A;
    LCD->LCD_RAM = 0x28;
    LCD->LCD_RAM = 0x08;
    LCD->LCD_RAM = 0x0E;
    LCD->LCD_RAM = 0x08;
    LCD->LCD_RAM = 0x54;
    LCD->LCD_RAM = 0XA9;
    LCD->LCD_RAM = 0x43;
    LCD->LCD_RAM = 0x0A;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0XE1;   // Set Gamma
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x15;
    LCD->LCD_RAM = 0x17;
    LCD->LCD_RAM = 0x07;
    LCD->LCD_RAM = 0x11;
    LCD->LCD_RAM = 0x06;
    LCD->LCD_RAM = 0x2B;
    LCD->LCD_RAM = 0x56;
    LCD->LCD_RAM = 0x3C;
    LCD->LCD_RAM = 0x05;
    LCD->LCD_RAM = 0x10;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x3F;
    LCD->LCD_RAM = 0x3F;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_REG = 0x2B;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x01;
    LCD->LCD_RAM = 0x3f;
    LCD->LCD_REG = 0x2A;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0xef;
    LCD->LCD_REG = 0x11;  // �˳�˯��ģʽ
    delay_ms(120);
    LCD->LCD_REG = 0x29;  // ����ʾ

    LCD_SetDir(0);        // ������ʾ�ķ���
    LCD_Fill(0, 0, xLCD.width, xLCD.height, BLACK);
    LCD_BL_ON;            // ��LCD����
    xLCD.FlagInit = 1;
}



/******************************************************************
 * �������� LCD_SetDir
 * ��  �ܣ� ������ʾ����
 * ��  ���� uint8_t dir     0-������1-����
 * ��  ע�� ���ʹ�ô�������ÿ�θ�������󣬶���Ҫ����У׼
 *          �����ļĴ�������ֵ�� 0-��������3-��������5-������, 6-������; ע�⣺���ʹ�ô�������ÿ�θ�������󣬶���Ҫ����У׼
 * ��  �أ� ��
 *****************************************************************/
void LCD_SetDir(uint8_t dir)
{
    uint16_t  regval = 0;
    uint16_t  temp;

    if (dir == 1)
        dir = 6;

    if (dir == 0 || dir == 3)         // ����
    {
        xLCD.dir = 0;
        xLCD.width = LCD_WIDTH;
        xLCD.height = LCD_HEIGHT;
    }
    else                              // ����
    {
        xLCD.dir = 1;
        xLCD.width = LCD_HEIGHT;
        xLCD.height = LCD_WIDTH;
    }

    if (dir == 0) regval |= (0 << 7) | (0 << 6) | (0 << 5); // ������,���ϵ���
    if (dir == 3) regval |= (1 << 7) | (1 << 6) | (0 << 5); // ���ҵ���,���µ���
    if (dir == 5) regval |= (0 << 7) | (1 << 6) | (1 << 5); // ���ϵ���,���ҵ���
    if (dir == 6) regval |= (1 << 7) | (0 << 6) | (1 << 5); // ���µ���,������

    regval |= 0X08;
    LCD->LCD_REG = 0X36;               // д��Ҫд�ļĴ������
    LCD->LCD_RAM = regval;             // д������

    if (regval & 0X20)
    {
        if (xLCD.width < xLCD.height)  // ����X,Y
        {
            temp = xLCD.width;
            xLCD.width = xLCD.height;
            xLCD.height = temp;
        }
    }
    else
    {
        if (xLCD.width > xLCD.height)  // ����X,Y
        {
            temp = xLCD.width;
            xLCD.width = xLCD.height;
            xLCD.height = temp;
        }
    }

    LCD->LCD_REG = 0X2A;
    LCD->LCD_RAM = 0;
    LCD->LCD_RAM = 0;
    LCD->LCD_RAM = (xLCD.width - 1) >> 8;
    LCD->LCD_RAM = (xLCD.width - 1) & 0XFF;
    LCD->LCD_REG = 0X2B;
    LCD->LCD_RAM = 0;
    LCD->LCD_RAM = 0;
    LCD->LCD_RAM = (xLCD.height - 1) >> 8;
    LCD->LCD_RAM = (xLCD.height - 1) & 0XFF;
}



/******************************************************************
 * �������� LCD_Fill
 * ��  �ܣ� ��ָ����������䵥����ɫ
 * ��  ���� uint16_t sx     ���Ͻ�X����
 *          uint16_t sy     ���Ͻ�Y����
 *          uint16_t ex     ���½�X����
 *          uint16_t ey     ���½�Y����
 *          uint16_t color  ��ɫֵ
 * ��  �أ� ��
 *****************************************************************/
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint16_t  xlen = 0;
    xlen = ex - sx + 1;
    for (uint16_t i = sy; i <= ey; i++)
    {
        setCursor(sx, i);                    // ���ù��λ��
        LCD->LCD_REG = 0X2C;                 // ��ʼдGRAM
        for (uint16_t j = 0; j < xlen; j++)
            LCD->LCD_RAM = color;            // ��ʾ��ɫ
    }
}



/******************************************************************
 * �������� LCD_Line
 * ��  �ܣ� ��ֱ��
 * ��  ���� uint16_t x1     ���X����
 *          uint16_t y1     ���Y����
 *          uint16_t x2     �յ�X����
 *          uint16_t y2     �յ�Y����
 *          uint16_t color  ��ɫֵ
 * ��  ע��
 *****************************************************************/
void LCD_Line(uint16_t  x1, uint16_t  y1, uint16_t  x2, uint16_t  y2, uint16_t _color)
{
    uint16_t  t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x2 - x1;                         // ������������
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0)incx = 1;                  // ���õ�������
    else if (delta_x == 0)incx = 0;            // ��ֱ��
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0)incy = 1;
    else if (delta_y == 0)incy = 0;            // ˮƽ��
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    if (delta_x > delta_y)distance = delta_x;  // ѡȡ��������������
    else distance = delta_y;
    for (t = 0; t <= distance + 1; t++)        // �������
    {
        LCD_DrawPoint(uRow, uCol, _color);     // ����
        xerr += delta_x ;
        yerr += delta_y ;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}



/******************************************************************
 * �������� LCD_Circle
 * ��  �ܣ� ��ָ��λ�û�Բ
 * ��  ���� uint16_t Xpos     X����
 *          uint16_t Ypos     ���Y����
 *          uint16_t Radius   �뾶
 *          uint16_t _color   ��ɫ
 * ��  ע��
 *****************************************************************/
void LCD_Circle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius, uint16_t _color)
{
    int16_t mx = Xpos, my = Ypos, x = 0, y = Radius;
    int16_t d = 1 - Radius;
    while (y > x)
    {
        LCD_DrawPoint(x + mx, y + my, _color);
        LCD_DrawPoint(-x + mx, y + my, _color);
        LCD_DrawPoint(-x + mx, -y + my, _color);
        LCD_DrawPoint(x + mx, -y + my, _color);
        LCD_DrawPoint(y + mx, x + my, _color);
        LCD_DrawPoint(-y + mx, x + my, _color);
        LCD_DrawPoint(y + mx, -x + my, _color);
        LCD_DrawPoint(-y + mx, -x + my, _color);
        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}



/******************************************************************
 * �������� drawAscii
 * ��  �ܣ� ��ָ��λ����ʾһ���ַ�
 * ��  ���� uint16_t x,y     ��ʼ����
 *          uint8_t  num     Ҫ��ʾ���ַ�:" "--->"~"
 *          uint8_t  size    �����С 12/16/24/32
 *          uint32_t bColor  ������ɫ
 *          uint32_t fColor  ������ɫ
 * ��  ע�� �ο�ԭ�Ӹ��Ұ�����Ĵ�����޸�
 *****************************************************************/
void drawAscii(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint32_t bColor, uint32_t fColor)
{
    static uint8_t temp;
    static uint8_t csize;
    static uint16_t y0;

    y0 = y;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);   // �õ�����һ���ַ���Ӧ������ռ���ֽ���
    num = num - ' ';                                          // �õ�ƫ�ƺ��ֵ��ASCII�ֿ��Ǵӿո�ʼȡģ������-' '���Ƕ�Ӧ�ַ����ֿ⣩
    for (uint8_t t = 0; t < csize; t++)
    {
        if (size == 12)         temp = aFontASCII12[num][t];  // ����1206����
        else if (size == 16)    temp = aFontASCII16[num][t];  // ����1608����
        else if (size == 24)    temp = aFontASCII24[num][t];  // ����2412����
        else if (size == 32)    temp = aFontASCII32[num][t];  // ����3216����
        else                    return;                       // û����ģ

        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80) LCD_DrawPoint(x, y, fColor);     // ���� ����
            else             LCD_DrawPoint(x, y, bColor);     // ���� ����

            temp <<= 1;
            y++;
            if (y >= xLCD.height)    return;                  // ������Ļ�߶�(��)
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= xLCD.width) return;                  // ������Ļ����(��)
                break;
            }
        }
    }
}



/******************************************************************
 * �������� drawGBK
 * ��  �ܣ� ��ָ��λ����ʾһ���ַ�
 * ��  ���� uint16_t  x,y     ��ʼ����
 *          uint8_t * font    ����
 *          uint8_t   size    �����С 12/16/24/32
 *          uint32_t  bColor  ������ɫ
 *          uint32_t  fColor  ������ɫ
 * ��  ע��
 *****************************************************************/
void drawGBK(uint16_t x, uint16_t y, uint8_t *font, uint8_t size, uint32_t bColor, uint32_t fColor)
{
    static uint8_t temp;
    static uint16_t y0;
    static uint8_t GBKData[128];
    static uint8_t csize;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);         // ����һ�������ֿ������ռ���ֽ���
    W25Q128_ReadFontData(font, size, GBKData);                  // ��ȡ������ģ�ĵ�������

    y0 = y;
    for (uint8_t t = 0; t < csize; t++)
    {
        temp = GBKData[t];                                      // �õ�GBK��������
        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)   LCD_DrawPoint(x, y, fColor);     // �������е㣬�ͻ���ɫ
            else               LCD_DrawPoint(x, y, bColor);     // �������޵㣬�ͻ���ɫ
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}



/******************************************************************************
 * ��  ���� LCD_String
 * ��  �ܣ� ��LCD����ʾ�ַ���(֧��Ӣ�ġ�����)
 * ��  ���� Ӣ�ģ���ģ���ݱ�����font.h�������ʹ���һ�𱣴���оƬ�ڲ�Flash
 *          ���֣���ģ�������ⲿFlash�У��������ֿ���W25Q128��
 *                ħŮ��������W25Q128����¼����4���ֺŴ�С��ģ����
 * ��  ���� uint16_t   x       �������Ͻ�X����
 *          uint16_t   y       �������Ͻ�y����
 *          char*      pFont   Ҫ��ʾ���ַ�������
 *          uint8_t    size    �ֺŴ�С��12 16 24 32
 *          uint32_t   fColor  ������ɫ
 *          uint32_t   bColor  ������ɫ
 * ����ֵ:  ��
 * ��  ע�� ����޸�_2020��05��1����
 ******************************************************************************/
void LCD_String(uint16_t x, uint16_t y, char *pFont, uint8_t size, uint32_t fColor, uint32_t bColor)
{
    if (xLCD .FlagInit == 0) return;

    uint16_t xStart = x;

    if (size != 12 && size != 16 && size != 24 && size != 32)      // �����С����
        size = 24;

    while (*pFont != 0)                                            // ������ȡ�ַ������ݣ�ֱ��'\0'ʱֹͣ
    {
        if (x > (xLCD.width - size))                               // ��λ���жϣ����������ĩ���Ͱѹ�껻��
        {
            x = xStart;
            y = y + size;
        }
        if (y > (xLCD.height - size))                              // ��λ���жϣ����������ĩ���ͷ��أ��������
            return;

        if (*pFont < 127)                                          // ASCII�ַ�
        {
            drawAscii(x, y, *pFont, size, bColor, fColor);
            pFont++;
            x += size / 2;
        }
        else                                                       // ������ʾ
        {
            drawGBK(x, y, (uint8_t *)pFont, size, bColor, fColor); // ��Ҫ: ����õĲ���ħŮ��������ֿ�, ��Ҫ�޸Ļ�ע��������һ��, �����Ͳ�Ӱ��ASCIIӢ���ַ������
            pFont = pFont + 2;                                     // ��һ��Ҫ��ʾ���������ڴ��е�λ��
            x = x + size;                                          // ��һ��Ҫ��ʾ����������Ļ�ϵ�Xλ��
        }
    }
}



/******************************************************************
 * �������� LCD_Image
 * ��  �ܣ� ��ָ�����������ָ��ͼƬ����
 * ��  ע�� ͼƬ������font.h�ļ���.ֻ�ʺ�����ͼƬ����
 *          Image2Lcdת����ˮƽɨ�裬16λ���ɫ, ��λ��ǰ(����������������ɫ�����෴)
 * ��  ���� uint16_t x,y     ���Ͻ���ʼ����
 *          uint16_t width   ͼƬ����
 *          uint16_t height  ͼƬ�߶�
 *          uint8_t* image   ���ݻ����ַ
 * ��  �أ� ��
 *****************************************************************/
void LCD_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image)
{
    for (uint16_t i = 0; i < height; i++)           // һ��һ�е���ʾ
    {
        LCD->LCD_REG = 0X2A;                        // ����x����
        LCD->LCD_RAM = x >> 8;                      // X����ĸ�8λ
        LCD->LCD_RAM = x ;                          // X����ĵ�8λ����Ϊָ���ֵֻ�ǵ�8λ��Ч�����Ե�Ч�� x1 & 0XFF
        LCD->LCD_REG = 0X2B;                        // ����y����
        LCD->LCD_RAM = (y + i) >> 8;                // Y����ĸ�8λ
        LCD->LCD_RAM = y + i;                       // Y����ĵ�8λ����Ϊָ���ֵֻ�ǵ�8λ��Ч�����Ե�Ч�� Y & 0XFF
        LCD->LCD_REG = 0X2C;                        // ��ʼдGRAM
        for (uint16_t j = 0; j < width; j++)        // һ���У������£�������ش���
        {
            LCD->LCD_RAM = image[1] << 8 | *image;  // д��16λ��ɫ����
            image += 2;                             // ����ָ����������ֽ�
        }
    }
}



/******************************************************************
 * �������� LCD_DispFlush
 * ��  �ܣ� ��ָ�����������ָ������
 * ��  ע�� ��������������ͼƬ������䡢16λ����λ��ǰ(�������ͼƬ��ʾ�����෴);
 *          ��������������LVGL��ֲ�ĺ�����disp_flush()������Ч�ؿ���ˢ��
 * ��  ���� uint16_t   x        ���Ͻ���ʼX����
 *          uint16_t   y        ���Ͻ���ʼY����
 *          uint16_t   width    ���ȣ�ÿ���ж��ٸ�16λ����; ��������ΪͼƬ�Ŀ�
 *          uint16_t   height   �߶ȣ�ÿ���ж��ٸ�16λ����; ��������ΪͼƬ�ĸ�
 *          uint16_t  *pData    ���ݵ�ַ
 * ��  �أ� ��
 *****************************************************************/
void LCD_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pData)
{
    for (uint16_t nowY = y; nowY <= height; nowY++)       // ������ʾ
    {
        LCD->LCD_REG = 0X2A;                              // ��������X�����ָ��
        LCD->LCD_RAM = x >> 8;                            // X����ĸ�8λ
        LCD->LCD_RAM = x ;                                // X����ĵ�8λ����Ϊָ���ֵֻ�ǵ�8λ��Ч�����Ե�Ч�� x & 0XFF
        LCD->LCD_REG = 0X2B;                              // ��������Y�����ָ��
        LCD->LCD_RAM = nowY >> 8;                         // Y����ĸ�8λ
        LCD->LCD_RAM = nowY ;                             // Y����ĵ�8λ����Ϊָ���ֵֻ�ǵ�8λ��Ч�����Ե�Ч�� Y & 0XFF
        LCD->LCD_REG = 0X2C;                              // ָ���ʼд��GRAM
        for (uint16_t nowX = x; nowX <= width; nowX++)    // һ���У��������������
        {
            LCD->LCD_RAM = *pData++;                      // д��ÿ�����16λ��ɫ����, RGB565ֵ
        }
    }
}



/******************************************************************
 * �������� LCD_ShowChinese
 * ��  �ܣ� ��ʾ����ȡģ�ĺ���,
 *          �ֿ�������font�ļ��У�ֻ�ʺ��������̶ֹ����
 * ȡ  ģ�� ʹ��������PCtoLCD2018 (��ʾ���ļ������ѱ���������ͼ��)
 *          1��ѡ���ֿ�
 *          2���������ã�����+����ʽ+����+C51��ʽ; 16������; �򹴣������ļ�+�����ʽ+���ո�ʽ; ����ֵ(�ֿ�x�ָ�/8); ����ֵ(80)
 *          3������Ҫ���ɵ��ı������������ģ
 *          4�����Ƶ�font.h�ļ��Ķ�Ӧ����β��; ע�����ʽ�ϵĴ���;
 * ��  ���� uint16_t  x         ����x
 *          uint16_t  y         ����y
 *          uint8_t   size      �ֺ�; ע�⣬Ҫ������ֺŵ�����, ���û�У����½�һ�������޸����溯��
 *          uint8_t   index     ��ģ�������е����
 *          uint32_t  fColor    ������ɫ
 *          uint32_t  bColor    ������ɫ
 * ��  ��:  ��
 *****************************************************************/
void LCD_ShowChinese(uint8_t x, uint8_t y, uint8_t size, uint8_t index, uint32_t fColor, uint32_t bColor)
{
    uint8_t m, temp;
    uint8_t x0 = x, y0 = y;
    uint16_t size3 = (size / 8 + ((size % 8) ? 1 : 0)) * size; // �õ�����һ���ַ���Ӧ������ռ���ֽ���

    for (uint16_t i = 0; i < size3; i++)
    {
        if (size == 12)
        {
            temp = aFontChinese12[index][i];   // ����12*12����
        }
        else if (size == 16)
        {
            temp = aFontChinese16[index][i];   // ����16*16����
        }
        else if (size == 24)
        {
            temp = aFontChinese24[index][i];   // ����24*24����
        }
        else if (size == 32)
        {
            temp = aFontChinese32[index][i];   // ����32*32����
        }
        else
        {
            temp = aFontChinese12[index][i];   // ����ǷǷ����Σ������12*12����
        }
        for (m = 0; m < 8; m++)
        {
            if (temp & 0x01)
                LCD_DrawPoint(x, y, fColor);
            else
                LCD_DrawPoint(x, y, bColor);
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size)
        {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
}



/******************************************************************
 * �������� LCD_Cross
 * ��  �ܣ� ��ָ�����ϻ���ʮ���ߣ�����У׼������
 * ��  ���� uint16_t x  ��   ʮ���ߵ����ĵ�����x
 *          uint16_t y  ��   ʮ���ߵ����ĵ�����x
 *          uint16_t len     ʮ���ߵ����س���
 *          uint32_t fColor  ��ɫ
 * �����أ� ��
 * ��  ע��
 *****************************************************************/
void LCD_Cross(uint16_t x, uint16_t y, uint16_t len, uint32_t fColor)
{
    uint16_t temp = len / 2;

    LCD_Line(x - temp, y, x + temp, y, fColor);
    LCD_Line(x, y - temp, x, y + temp, fColor);
}



/******************************************************************
 * �������� LCD_GUI
 * ��  �ܣ� ���԰����豸�����LCD��ʾ����
 * ��  ����
 * �����أ�
 * ��  ע��
 *****************************************************************/
void LCD_GUI(void)
{
    char strTemp[30];

    // ȫ������-����
    LCD_Fill(0, 0,  xLCD.width,  xLCD.height, BLACK);

    LCD_String(8,  0, "STM32F407VET6������", 24, WHITE, BLACK);
    LCD_String(72, 28, "�豸�����", 16, GREY, BLACK);

    // �߿�
    LCD_Line(0, 49,  0, 329, GREY);                         // ����
    LCD_Line(119, 70, 119, 329, GREY);                      // ����
    LCD_Line(239, 49, 239, 329, GREY);                      // ����

    LCD_Fill(0, 49, 239, 70, WHITE);
    LCD_String(6, 52, "�����豸", 16, BLACK, WHITE);
    LCD_String(125, 52, "WiFi����ͨ��", 16, BLACK, WHITE);

    LCD_Fill(119, 125, 239, 145, WHITE);
    LCD_String(125, 127, "CANͨ��", 16, BLACK, WHITE);

    LCD_Fill(119, 205, 239, 225, WHITE);
    LCD_String(125, 207, "RS485ͨ��", 16, BLACK, WHITE);

    // �ײ�״̬��-����
    LCD_Fill(0, 287, 239, 329, WHITE);                     // �װ�
    LCD_Line(0, 303, 239, 303, BLACK);
    LCD_Line(119, 287, 119, 329, BLACK);
    LCD_Line(119, 49, 119, 70, BLACK);                     // ����
    // �ײ�״̬��-����
    LCD_String(6, 290, "�ڲ��¶�", 12, BLACK, WHITE);      // �ڲ��¶�
    LCD_String(6, 306, "��������", 12, BLACK, WHITE);      // ��������
    LCD_String(125, 290, "��������", 12, BLACK, WHITE);    // ��������
    LCD_String(125, 306, "����ʱ��", 12, BLACK, WHITE);    // ����ʱ��
    sprintf(strTemp, "��%d��", xW25Q128 .StartupTimes);
    LCD_String(68, 306, strTemp, 12, BLUE, WHITE);

    uint16_t y = 74;
    // UASRT1
//    LCD_String(6, y, "UART1����",  12, YELLOW, BLACK);
//    if (xUSART1.InitFlag == 1)
//    {
//        LCD_String(90, y, "���", 12, GREEN, BLACK);
//    }
//    else
//    {
//        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
//    }
    y = y + 15;
    // SystemClock
    LCD_String(6, y, "ϵͳʱ��",   12, YELLOW, BLACK);
    sprintf(strTemp, "%d", SystemCoreClock / 1000000);
    LCD_String(77, y, strTemp,       12, GREEN, BLACK);
    LCD_String(96, y, "MHz", 12, GREEN, BLACK);
    y = y + 15;
    // LEDָʾ��
    LCD_String(6, y, "LEDָʾ��",  12, YELLOW, BLACK);
    LCD_String(90, y, "���", 12, GREEN, BLACK);
    y = y + 15;
    // �����ж�
    LCD_String(6, y, "�����ж�",   12, YELLOW, BLACK);
    LCD_String(90, y, "���", 12, GREEN, BLACK);
    y = y + 15;
    // FLASH�洢��
    LCD_String(6, y, "FLASH�洢",  12, YELLOW, BLACK);
    if (xW25Q128.FlagInit  == 1)
    {
        LCD_String(71, y, xW25Q128.type, 12, GREEN, BLACK);
    }
    else
    {
        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    }
    y = y + 15;
    // �����ֿ�
    LCD_String(6, y, "�����ֿ�",   12, YELLOW, BLACK);
    if (xW25Q128 .FlagGBKStorage == 1)
    {
        LCD_String(90, y, "����", 12, GREEN, BLACK);
    }
    else
    {
        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    }
    y = y + 15;
    // ��ʾ��
    LCD_String(6, y, "��ʾ��оƬ", 12, YELLOW, BLACK);
    sprintf(strTemp, "%X", xLCD.id);
    if (xLCD.FlagInit  == 1)
    {
        LCD_String(90, y, strTemp, 12, GREEN, BLACK);
    }
    else
    {
        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    }
    y = y + 15;
}

