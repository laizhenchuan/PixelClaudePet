/***********************************************************************************************************************************
 ** �������д��  ħŮ�������Ŷ�
 ** ����    ����  ħŮ������
 ** ���������ӡ�  https://demoboard.taobao.com
 ***********************************************************************************************************************************
 ** ���ļ����ơ�  bsp_UART.c
 **
 ** ���ļ����ܡ�  ��UART��GPIO���á�ͨ��Э�����á��ж����ã������ܺ���ʵ��
 **
 ** ������ƽ̨��  STM32F407 + keil5 + ��׼��\HAL��
 **
************************************************************************************************************************************/
#include "bsp_UART.h"           // ͷ�ļ�




/*****************************************************************************
 ** �������ر���
****************************************************************************/
typedef struct
{
    uint16_t  usRxNum;            // ��һ֡���ݣ����յ����ٸ��ֽ�����
    uint8_t  *puRxData;           // ��һ֡���ݣ����ݻ���; ��ŵ��ǿ����жϺ󣬴���ʱ���ջ��渴�ƹ������������ݣ����ǽ��չ����еĲ���������;

    uint8_t  *puTxFiFoData;       // ���ͻ����������ζ���; Ϊ�˷��������Ķ���û�з�װ�ɶ��к���
    uint16_t  usTxFiFoData ;      // ���λ������Ķ�ͷ
    uint16_t  usTxFiFoTail ;      // ���λ������Ķ�β
} xUSATR_TypeDef;





/******************************************************************************
 * ��  ���� delay_ms
 * ��  �ܣ� ms ��ʱ����
 * ��  ע�� 1��ϵͳʱ��168MHz
 *          2���򹴣�Options/ c++ / One ELF Section per Function
            3�������Ż�����Level 3(-O3)
 * ��  ���� uint32_t  ms  ����ֵ
 * ����ֵ�� ��
 ******************************************************************************/
static volatile uint32_t ulTimesMS;    // ʹ��volatile��������ֹ�������������Ż�
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
        ulTimesMS--;                   // �����ⲿ��������ֹ��ѭ�����������Ż���
}





//////////////////////////////////////////////////////////////   UART-1   ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART1_EN

static xUSATR_TypeDef  xUART1 = { 0 };                      // ���� UART1 ���շ��ṹ��
static uint8_t uaUART1RxData[UART1_RX_BUF_SIZE];            // ���� UART1 �Ľ��ջ���
static uint8_t uaUART1TxFiFoData[UART1_TX_BUF_SIZE];        // ���� UART1 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART1_Init
 * ��  �ܣ� ��ʼ��USART1��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PA10��RX-PA11
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t  ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART1_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;                       // ʹ�����裺UART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                        // ʹ��GPIO��GPIOA
    // �رմ���
    USART1 -> CR1  =   0;                                       // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX����
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;                 // ���ű�ţ�TX_PA9
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX����
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;                // ���ű�ţ�RX_PA10
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);   // ����PA9���ù��ܣ� USART1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);  // ����PA10���ù��ܣ�USART1
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_9 | GPIO_PIN_10;           // ���� TX-PA9��RX-PA10
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;                // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 1);                    // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(USART1_IRQn);                            // ʹ���ж���
#endif

    // ���㲨���ʲ���
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 2) / (ulBaudrate * 16);    // �����ʹ�ʽ����; USART1������APB2, ʱ��Ϊϵͳʱ�ӵ�2��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    USART1 -> BRR  = mantissa << 4 | fraction;                  // ���ò�����
    // ����ͨ�Ų�������
    USART1 -> CR1 |=   0x01 << 2;                               // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    USART1 -> CR1 |=   0x01 << 3;                               // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    USART1 -> CR1 &= ~(0x01 << 12);                             // ����λ[12]��0=8λ��1=9λ
    USART1 -> CR2  =   0;                                       // ������0
    USART1 -> CR2 &=  ~(0x03 << 12);                            // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    USART1 -> CR3  =   0;                                       // ������0
    USART1 -> CR3 &= ~(0x01 << 6);                              // DMA����[6]: 0=��ֹ��1=ʹ��
    USART1 -> CR3 &= ~(0x01 << 7);                              // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    USART1->CR1 &= ~(0x01 << 7);                                // �رշ����ж�
    USART1->CR1 |= 0x01 << 5;                                   // ʹ�ܽ����ж�: ���ջ������ǿ�
    USART1->CR1 |= 0x01 << 4;                                   // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    USART1->SR   = ~(0x00F0);                                   // �����ж�
    // �򿪴���
    USART1 -> CR1 |= 0x01 << 13;                                // ʹ��UART��ʼ����
    // ����������
    xUART1.puRxData = uaUART1RxData;                            // �������ջ������ĵ�ַ
    xUART1.puTxFiFoData = uaUART1TxFiFoData;                    // �������ͻ������ĵ�ַ
    // �����ʾ
    printf("\r\r\r===========  STM32F407VE ���� ��ʼ������ ===========\r");                   // �������������
    SystemCoreClockUpdate();                                                                  // ����һ��ϵͳ����Ƶ�ʱ���
    printf("ϵͳʱ��Ƶ��             %d MHz\r", SystemCoreClock / 1000000);                   // �������������
    printf("UART1 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate); // �������������
}

/******************************************************************************
 * ��  ���� USART1_IRQHandler
 * ��  �ܣ� USART1�Ľ����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 * ��  ע�� ���������������ж��¼�ʱ����Ӳ�����á�
 *          ���ʹ�ñ��ļ����룬�ڹ����ļ��������ط���Ҫע��ͬ�������������ͻ��
******************************************************************************/
void USART1_IRQHandler(void)
{
    static uint16_t cnt = 0;                                         // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART1_RX_BUF_SIZE];                       // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUSART.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((USART1->SR & 1 << 7) && (USART1->CR1 & 1 << 7))             // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        USART1->DR = xUART1.puTxFiFoData[xUART1.usTxFiFoTail++];     // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART1.usTxFiFoTail == UART1_TX_BUF_SIZE)                // �������ָ�뵽��β���������±�ǵ�0
            xUART1.usTxFiFoTail = 0;
        if (xUART1.usTxFiFoTail == xUART1.usTxFiFoData)
            USART1->CR1 &= ~(0x01 << 7);                             // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (USART1->SR & (0x01 << 5))                                    // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART1_RX_BUF_SIZE))//||(xUART1.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART1��֡���������ѳ������ջ����С\r!");
            USART1->DR;                                              // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = USART1->DR ;                                 // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ��
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (USART1->SR & (0x01 << 4))                                    // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART1.usRxNum  = 0;                                         // �ѽ��յ��������ֽ�����0
        memcpy(xUART1.puRxData, rxTemp, UART1_RX_BUF_SIZE);          // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART1.usRxNum  = cnt;                                       // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                     // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART1_RX_BUF_SIZE);                        // �������ݻ������飬����; ׼����һ�εĽ���
        USART1 ->SR;
        USART1 ->DR;                                                 // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}

/******************************************************************************
 * ��  ���� UART1_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t*  puData   �跢�����ݵĵ�ַ
 *          uint16_t  usNum    ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART1_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // �����ݷ��뻷�λ�����
    {
        xUART1.puTxFiFoData[xUART1.usTxFiFoData++] = puData[i];  // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART1.usTxFiFoData == UART1_TX_BUF_SIZE)            // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART1.usTxFiFoData = 0;
    }                                                            // Ϊ�˷����Ķ����⣬����û�аѴ˲��ַ�װ�ɶ��к������������з�װ

    if ((USART1->CR1 & 1 << 7) == 0)                             // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        USART1->CR1 |= 1 << 7;                                   // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART1_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART1_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // ����һ������, ���������������0
    va_list ap;                                             // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                 // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                  // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                             // ��տɱ�����б�
    UART1_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART1_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART1_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART1_ClearRx();                                              // ��0
    UART1_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART1_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART1_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART1_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}


/******************************************************************************
 * ��  ���� UART1_SendStringForDMA
 * ��  �ܣ� UARTͨ��DMA�������ݣ�ʡ��ռ���жϵ�ʱ��
 *         ���ʺϳ������ַ������ֽ����ǳ��࣬
 *         ���� �� �ϡ�1:ֻ�ʺϷ����ַ��������ʺϷ��Ϳ��ܺ�0����ֵ������; 2-ʱ����Ҫ�㹻
 * ��  ���� char strintTemp  Ҫ���͵��ַ����׵�ַ
 * ����ֵ�� ��
 * ��  ע:  ������Ϊ���������������û��ο���Ϊ�˷�����ֲ�����ļ����ⲻ��ʹ�ñ�������
 ******************************************************************************/
#if 0
void UART1_SendStringForDMA(char *stringTemp)
{
    static uint8_t Flag_DmaTxInit = 0;                // ���ڱ���Ƿ�������DMA����
    uint32_t   num = 0;                               // ���͵�������ע�ⷢ�͵ĵ�λ���Ǳ���8λ��
    char *t = stringTemp ;                            // ������ϼ��㷢�͵�����

    while (*t++ != 0)  num++;                         // ����Ҫ���͵���Ŀ���ⲽ�ȽϺ�ʱ�����Է���ÿ��6���ֽڣ�����1us����λ��8λ

    while (DMA1_Channel4->CNDTR > 0);                 // ��Ҫ�����DMA���ڽ����ϴη��ͣ��͵ȴ�; �ý�����ж����־��F4������ô�鷳���������EN�Զ�����
    if (Flag_DmaTxInit == 0)                          // �Ƿ��ѽ��й�USAART_TX��DMA��������
    {
        Flag_DmaTxInit  = 1;                          // ���ñ�ǣ��´ε��ñ������Ͳ��ٽ���������
        USART1 ->CR3   |= 1 << 7;                     // ʹ��DMA����
        RCC->AHBENR    |= 1 << 0;                     // ����DMA1ʱ��  [0]DMA1   [1]DMA2

        DMA1_Channel4->CCR   = 0;                     // ʧ�ܣ� ��0�����Ĵ���, DMA����ʧ�ܲ�������
        DMA1_Channel4->CNDTR = num;                   // ����������
        DMA1_Channel4->CMAR  = (uint32_t)stringTemp;  // �洢����ַ
        DMA1_Channel4->CPAR  = (uint32_t)&USART1->DR; // �����ַ

        DMA1_Channel4->CCR |= 1 << 4;                 // ���ݴ��䷽��   0:�������   1:�Ӵ洢����
        DMA1_Channel4->CCR |= 0 << 5;                 // ѭ��ģʽ       0:��ѭ��     1��ѭ��
        DMA1_Channel4->CCR |= 0 << 6;                 // �����ַ������ģʽ
        DMA1_Channel4->CCR |= 1 << 7;                 // �洢������ģʽ
        DMA1_Channel4->CCR |= 0 << 8;                 // �������ݿ���Ϊ8λ
        DMA1_Channel4->CCR |= 0 << 10;                // �洢�����ݿ���8λ
        DMA1_Channel4->CCR |= 0 << 12;                // �е����ȼ�
        DMA1_Channel4->CCR |= 0 << 14;                // �Ǵ洢�����洢��ģʽ
    }
    DMA1_Channel4->CCR  &= ~((uint32_t)(1 << 0));     // ʧ�ܣ�DMA����ʧ�ܲ�������
    DMA1_Channel4->CNDTR = num;                       // ����������
    DMA1_Channel4->CMAR  = (uint32_t)stringTemp;      // �洢����ַ
    DMA1_Channel4->CCR  |= 1 << 0;                    // ����DMA����
}
#endif

/******************************************************************************
 * ��  ���� UART1_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART1_GetRxNum(void)
{
    return xUART1.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART1_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� �����ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART1_GetRxData(void)
{
    return xUART1.puRxData ;
}

/******************************************************************************
 * ��  ���� UART1_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART1_ClearRx(void)
{
    xUART1.usRxNum = 0 ;
}
#endif  // endif UART1_EN





//////////////////////////////////////////////////////////////   UART-2   ///////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART2_EN

static xUSATR_TypeDef  xUART2 = { 0 };                      // ���� UART2 ���շ��ṹ��
static uint8_t uaUART2RxData[UART2_RX_BUF_SIZE];            // ���� UART2 �Ľ��ջ���
static uint8_t uaUART2TxFiFoData[UART2_TX_BUF_SIZE];        // ���� UART2 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART2_Init
 * ��  �ܣ� ��ʼ��USART2��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PA2��RX-PA3
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t  ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART2_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;                       // ʹ�����裺UART2
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                        // ʹ��GPIO��GPIOA
    // �رմ���
    USART2 -> CR1  =   0;                                       // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX_PA2
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;                 // ���ű�ţ�TX_PA2
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX_PA3
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;                 // ���ű�ţ�RX_PA3
    GPIO_Init(GPIOA, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);   // �������Ÿ��ù��ܣ�USART2
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);   // �������Ÿ��ù��ܣ�USART2
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_2 | GPIO_PIN_3;            // ���� TX-PA2��RX-PA3
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;                // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 1);                    // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(USART2_IRQn);                            // ʹ���ж���
#endif

    // ���㲨���ʲ���
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // �����ʹ�ʽ����; USART2������APB1, ʱ��Ϊϵͳʱ�ӵ�4��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    USART2 -> BRR  = mantissa << 4 | fraction;                  // ���ò�����
    // ����ͨ�Ų�������
    USART2 -> CR1 |=   0x01 << 2;                               // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    USART2 -> CR1 |=   0x01 << 3;                               // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    USART2 -> CR1 &= ~(0x01 << 12);                             // ����λ[12]��0=8λ��1=9λ
    USART2 -> CR2  =   0;                                       // ������0
    USART2 -> CR2 &=  ~(0x03 << 12);                            // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    USART2 -> CR3  =   0;                                       // ������0
    USART2 -> CR3 &= ~(0x01 << 6);                              // DMA����[6]: 0=��ֹ��1=ʹ��
    USART2 -> CR3 &= ~(0x01 << 7);                              // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    USART2->CR1 &= ~(0x01 << 7);                                // �رշ����ж�
    USART2->CR1 |= 0x01 << 5;                                   // ʹ�ܽ����ж�: ���ջ������ǿ�
    USART2->CR1 |= 0x01 << 4;                                   // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    USART2->SR   = ~(0x00F0);                                   // �����ж�
    // ����USART2
    USART2 -> CR1 |= 0x01 << 13;                                // ʹ��UART��ʼ����
    // ����������
    xUART2.puRxData = uaUART2RxData;                            // ��ȡ���ջ������ĵ�ַ
    xUART2.puTxFiFoData = uaUART2TxFiFoData;                    // ��ȡ���ͻ������ĵ�ַ
    // �����ʾ
    printf("UART2 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate);
}

/******************************************************************************
 * ��  ���� USART2_IRQHandler
 * ��  �ܣ� USART2�Ľ����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 * ��  ע�� ���������������ж��¼�ʱ����Ӳ�����á�
 *          ���ʹ�ñ��ļ����룬�ڹ����ļ��������ط���Ҫע��ͬ�������������ͻ��
 ******************************************************************************/
void USART2_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART2_RX_BUF_SIZE];                      // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUARTx.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((USART2->SR & 1 << 7) && (USART2->CR1 & 1 << 7))            // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        USART2->DR = xUART2.puTxFiFoData[xUART2.usTxFiFoTail++];    // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART2.usTxFiFoTail == UART2_TX_BUF_SIZE)               // �������ָ�뵽��β���������±�ǵ�0
            xUART2.usTxFiFoTail = 0;
        if (xUART2.usTxFiFoTail == xUART2.usTxFiFoData)
            USART2->CR1 &= ~(1 << 7);                               // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (USART2->SR & (1 << 5))                                      // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART2_RX_BUF_SIZE))//||xUART2.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART2��֡���������ѳ������ջ����С\r!");
            USART2->DR;                                             // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = USART2->DR ;                                // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ��
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (USART2->SR & (1 << 4))                                      // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART2.usRxNum  = 0;                                        // �ѽ��յ��������ֽ�����0
        memcpy(xUART2.puRxData, rxTemp, UART2_RX_BUF_SIZE);         // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART2.usRxNum  = cnt;                                      // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                    // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART2_RX_BUF_SIZE);                       // �������ݻ������飬����; ׼����һ�εĽ���
        USART2 ->SR;
        USART2 ->DR;                                                // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}

/******************************************************************************
 * ��  ���� UART2_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t* puData     �跢�����ݵĵ�ַ
 *          uint8_t  usNum      ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART2_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // �����ݷ��뻷�λ�����
    {
        xUART2.puTxFiFoData[xUART2.usTxFiFoData++] = puData[i];  // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART2.usTxFiFoData == UART2_TX_BUF_SIZE)            // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART2.usTxFiFoData = 0;
    }

    if ((USART2->CR1 & 1 << 7) == 0)                             // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        USART2->CR1 |= 1 << 7;                                   // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART2_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART2_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // ����һ������, ���������������0
    va_list ap;                                             // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                 // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                  // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                             // ��տɱ�����б�
    UART2_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART2_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART2_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART2_ClearRx();                                              // ��0
    UART2_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART2_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART2_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART2_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}

/******************************************************************************
 * ��  ���� UART2_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART2_GetRxNum(void)
{
    return xUART2.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART2_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� ���ݵĵ�ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART2_GetRxData(void)
{
    return xUART2.puRxData ;
}

/******************************************************************************
 * ��  ���� UART2_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART2_ClearRx(void)
{
    xUART2.usRxNum = 0 ;
}
#endif  // endif UART2_EN





//////////////////////////////////////////////////////////////   USART-3   //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART3_EN

static xUSATR_TypeDef  xUART3 = { 0 };                      // ���� UART3 ���շ��ṹ��
static uint8_t uaUart3RxData[UART3_RX_BUF_SIZE];            // ���� UART3 �Ľ��ջ���
static uint8_t uaUart3TxFiFoData[UART3_TX_BUF_SIZE];        // ���� UART3 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART3_Init
 * ��  �ܣ� ��ʼ��USART3��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PB10��RX-PB11
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART3_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;                       // ʹ�����裺UART3
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;                        // ʹ��GPIO��GPIOB
    // �رմ���
    USART3 -> CR1  =   0;                                       // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX����
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;                // ���ű�ţ�TX_PB10
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOB, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;                // ���ű�ţ�RX_PB11
    GPIO_Init(GPIOB, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);  // �������Ÿ��ù���
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);  // �������Ÿ��ù���
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_10 | GPIO_PIN_11;          // ���� TX-PB10��RX-PB11
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;                // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 1);                    // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(USART3_IRQn);                            // ʹ���ж���
#endif

    // ���㲨���ʲ���
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // �����ʹ�ʽ����; USART3������APB1, ʱ��Ϊϵͳʱ�ӵ�4��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    USART3 -> BRR  = mantissa << 4 | fraction;                  // ���ò�����
    // ����ͨ�Ų�������
    USART3 -> CR1 |=   0x01 << 2;                               // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    USART3 -> CR1 |=   0x01 << 3;                               // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    USART3 -> CR1 &= ~(0x01 << 12);                             // ����λ[12]��0=8λ��1=9λ
    USART3 -> CR2  =   0;                                       // ������0
    USART3 -> CR2 &=  ~(0x03 << 12);                            // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    USART3 -> CR3  =   0;                                       // ������0
    USART3 -> CR3 &= ~(0x01 << 6);                              // DMA����[6]: 0=��ֹ��1=ʹ��
    USART3 -> CR3 &= ~(0x01 << 7);                              // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    USART3->CR1 &= ~(0x01 << 7);                                // �رշ����ж�
    USART3->CR1 |= 0x01 << 5;                                   // ʹ�ܽ����ж�: ���ջ������ǿ�
    USART3->CR1 |= 0x01 << 4;                                   // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    USART3->SR   = ~(0x00F0);                                   // �����ж�
    // �򿪴���
    USART3 -> CR1 |= 0x01 << 13;                                // ʹ��UART��ʼ����
    // ����������
    xUART3.puRxData = uaUart3RxData;                            // ��ȡ���ջ������ĵ�ַ
    xUART3.puTxFiFoData = uaUart3TxFiFoData;                    // ��ȡ���ͻ������ĵ�ַ
    // �����ʾ
    printf("UART3 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate);
}

/******************************************************************************
 * ��  ���� USART3_IRQHandler
 * ��  �ܣ� USART3�Ľ����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 * ��  ע�� ���������������ж��¼�ʱ����Ӳ�����á�
 *          ���ʹ�ñ��ļ����룬�ڹ����ļ��������ط���Ҫע��ͬ�������������ͻ��
 ******************************************************************************/
void USART3_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART3_RX_BUF_SIZE];                      // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUARTx.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((USART3->SR & 1 << 7) && (USART3->CR1 & 1 << 7))            // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        USART3->DR = xUART3.puTxFiFoData[xUART3.usTxFiFoTail++];    // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART3.usTxFiFoTail == UART3_TX_BUF_SIZE)               // �������ָ�뵽��β���������±�ǵ�0
            xUART3.usTxFiFoTail = 0;
        if (xUART3.usTxFiFoTail == xUART3.usTxFiFoData)
            USART3->CR1 &= ~(1 << 7);                               // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (USART3->SR & (1 << 5))                                      // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART3_RX_BUF_SIZE))//||xUART3.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART3��֡���������ѳ������ջ����С\r!");
            USART3->DR;                                             // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = USART3->DR ;                                // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (USART3->SR & (1 << 4))                                      // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART3.usRxNum  = 0;                                        // �ѽ��յ��������ֽ�����0
        memcpy(xUART3.puRxData, rxTemp, UART3_RX_BUF_SIZE);         // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART3.usRxNum  = cnt;                                      // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                    // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART3_RX_BUF_SIZE);                       // �������ݻ������飬����; ׼����һ�εĽ���
        USART3 ->SR;
        USART3 ->DR;                                                // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}

/******************************************************************************
 * ��  ���� UART3_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t* puData   �跢�����ݵĵ�ַ
 *          uint8_t  usNum      ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART3_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                           // �����ݷ��뻷�λ�����
    {
        xUART3.puTxFiFoData[xUART3.usTxFiFoData++] = puData[i];    // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART3.usTxFiFoData == UART3_TX_BUF_SIZE)              // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART3.usTxFiFoData = 0;
    }

    if ((USART3->CR1 & 1 << 7) == 0)                               // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        USART3->CR1 |= 1 << 7;                                     // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART3_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART3_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                                // ����һ������, ���������������0
    va_list ap;                                              // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                  // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                   // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                              // ��տɱ�����б�
    UART3_SendData((uint8_t *)mBuffer, strlen(mBuffer));     // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART3_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART3_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART3_ClearRx();                                              // ��0
    UART3_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART3_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART3_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART3_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}

/******************************************************************************
 * ��  ���� UART3_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART3_GetRxNum(void)
{
    return xUART3.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART3_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� ���ݵĵ�ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART3_GetRxData(void)
{
    return xUART3.puRxData ;
}

/******************************************************************************
 * ��  ���� UART3_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART3_ClearRx(void)
{
    xUART3.usRxNum = 0 ;
}
#endif  // endif UART3_EN





//////////////////////////////////////////////////////////////   UART-4   //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART4_EN

static xUSATR_TypeDef  xUART4 = { 0 };                      // ���� UART4 ���շ��ṹ��
static uint8_t uaUart4RxData[UART4_RX_BUF_SIZE];            // ���� UART4 �Ľ��ջ���
static uint8_t uaUart4TxFiFoData[UART4_TX_BUF_SIZE];        // ���� UART4 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART4_Init
 * ��  �ܣ� ��ʼ��UART4��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PC10��RX-PC11
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART4_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB1ENR |= RCC_APB1ENR_UART4EN;                        // ʹ�����裺UART4
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;                        // ʹ��GPIO��GPIOC
    // �رմ���
    UART4 -> CR1  =   0;                                        // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX����
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;                // ���ű�ţ�TX_PC10
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOC, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX_PA3
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;                // ���ű�ţ�RX_PC11
    GPIO_Init(GPIOC, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_UART4);   // �������Ÿ��ù��ܣ�UART4
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4);   // �������Ÿ��ù��ܣ�UART4
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = UART4_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_10 | GPIO_PIN_11;          // ���� TX-PC10��RX-PC11
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;                 // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(UART4_IRQn, 1, 1);                     // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(UART4_IRQn);                             // ʹ���ж���
#endif

    // ���㲨���ʲ���
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // �����ʹ�ʽ����; UART4������APB1, ʱ��Ϊϵͳʱ�ӵ�4��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    UART4 -> BRR  = mantissa << 4 | fraction;                   // ���ò�����
    // ����ͨ�Ų�������
    UART4 -> CR1 |=   0x01 << 2;                                // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    UART4 -> CR1 |=   0x01 << 3;                                // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    UART4 -> CR1 &= ~(0x01 << 12);                              // ����λ[12]��0=8λ��1=9λ
    UART4 -> CR2  =   0;                                        // ������0
    UART4 -> CR2 &=  ~(0x03 << 12);                             // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    UART4 -> CR3  =   0;                                        // ������0
    UART4 -> CR3 &= ~(0x01 << 6);                               // DMA����[6]: 0=��ֹ��1=ʹ��
    UART4 -> CR3 &= ~(0x01 << 7);                               // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    UART4->CR1 &= ~(0x01 << 7);                                 // �رշ����ж�
    UART4->CR1 |= 0x01 << 5;                                    // ʹ�ܽ����ж�: ���ջ������ǿ�
    UART4->CR1 |= 0x01 << 4;                                    // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    UART4->SR   = ~(0x00F0);                                    // �����ж�
    // �򿪴���
    UART4 -> CR1 |= 0x01 << 13;                                 // ʹ��UART��ʼ����
    // ����������
    xUART4.puRxData = uaUart4RxData;                            // ��ȡ���ջ������ĵ�ַ
    xUART4.puTxFiFoData = uaUart4TxFiFoData;                    // ��ȡ���ͻ������ĵ�ַ
    // �����ʾ
    printf("UART4 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate);
}

/******************************************************************************
 * ��  ���� UART4_IRQHandler
 * ��  �ܣ� UART4���жϴ�������
 *          �����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 * ��  ע�� ���������������ж��¼�ʱ����Ӳ�����á�
 *          ���ʹ�ñ��ļ����룬�ڹ����ļ��������ط���Ҫע��ͬ�������������ͻ��
 ******************************************************************************/
void UART4_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART4_RX_BUF_SIZE];                      // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUARTx.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((UART4->SR & 1 << 7) && (UART4->CR1 & 1 << 7))              // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        UART4->DR = xUART4.puTxFiFoData[xUART4.usTxFiFoTail++];     // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART4.usTxFiFoTail == UART4_TX_BUF_SIZE)               // �������ָ�뵽��β���������±�ǵ�0
            xUART4.usTxFiFoTail = 0;
        if (xUART4.usTxFiFoTail == xUART4.usTxFiFoData)
            UART4->CR1 &= ~(1 << 7);                                // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (UART4->SR & (1 << 5))                                       // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART4_RX_BUF_SIZE))//||xUART4.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART4��֡���������ѳ������ջ����С\r!");
            UART4->DR;                                              // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = UART4->DR ;                                 // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (UART4->SR & (1 << 4))                                       // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART4.usRxNum  = 0;                                        // �ѽ��յ��������ֽ�����0
        memcpy(xUART4.puRxData, rxTemp, UART4_RX_BUF_SIZE);         // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART4.usRxNum  = cnt;                                      // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                    // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART4_RX_BUF_SIZE);                       // �������ݻ������飬����; ׼����һ�εĽ���
        UART4 ->SR;
        UART4 ->DR;                                                 // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}

/******************************************************************************
 * ��  ���� UART4_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t* puData   �跢�����ݵĵ�ַ
 *          uint8_t  usNum    ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART4_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // �����ݷ��뻷�λ�����
    {
        xUART4.puTxFiFoData[xUART4.usTxFiFoData++] = puData[i];  // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART4.usTxFiFoData == UART4_TX_BUF_SIZE)            // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART4.usTxFiFoData = 0;
    }

    if ((UART4->CR1 & 1 << 7) == 0)                              // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        UART4->CR1 |= 1 << 7;                                    // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART4_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART4_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                                // ����һ������, ���������������0
    va_list ap;                                              // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                  // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                   // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                              // ��տɱ�����б�
    UART4_SendData((uint8_t *)mBuffer, strlen(mBuffer));     // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART4_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART4_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART4_ClearRx();                                              // ��0
    UART4_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART4_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART4_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART4_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}

/******************************************************************************
 * ��  ���� UART4_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART4_GetRxNum(void)
{
    return xUART4.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART4_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� ���ݵĵ�ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART4_GetRxData(void)
{
    return xUART4.puRxData ;
}

/******************************************************************************
 * ��  ���� UART4_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART4_ClearRx(void)
{
    xUART4.usRxNum = 0 ;
}
#endif  // endif UART4_EN




//////////////////////////////////////////////////////////////   UART-5   //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART5_EN

static xUSATR_TypeDef  xUART5 = { 0 };                      // ���� UART5 ���շ��ṹ��
static uint8_t uaUart5RxData[UART5_RX_BUF_SIZE];            // ���� UART5 �Ľ��ջ���
static uint8_t uaUart5TxFiFoData[UART5_TX_BUF_SIZE];        // ���� UART5 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART5_Init
 * ��  �ܣ� ��ʼ��UART5��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PC12��RX-PD2
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART5_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB1ENR |= RCC_APB1ENR_UART5EN;                        // ʹ�����裺UART5
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;                        // ʹ��GPIO��GPIOC
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;                        // ʹ��GPIO��GPIOD
    // �رմ���
    UART5 -> CR1  =   0;                                        // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX����
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    // ����TX_PA2
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;                // ���ű�ţ�TX_PC12
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOC, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX_PA3
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;                 // ���ű�ţ�RX_PD2
    GPIO_Init(GPIOD, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_UART5);   // �������Ÿ��ù��ܣ�UART5
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_UART5);    // �������Ÿ��ù��ܣ�UART5
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = UART5_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_12 ;                       // ���� TX-PC12
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_NOPULL;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;                 // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                     // ��ʼ������

    GPIO_InitStruct.Pin   = GPIO_PIN_2 ;                        // ���� RX-PD2
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;                 // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(UART5_IRQn, 0, 0);                     // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(UART5_IRQn);                             // ʹ���ж���
#endif

    // ���㲨���ʲ���
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 4) / (ulBaudrate * 16);    // �����ʹ�ʽ����; UART5������APB1, ʱ��Ϊϵͳʱ�ӵ�4��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    UART5 -> BRR  = mantissa << 4 | fraction;                   // ���ò�����
    // ����ͨ�Ų�������
    UART5 -> CR1 |=   0x01 << 2;                                // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    UART5 -> CR1 |=   0x01 << 3;                                // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    UART5 -> CR1 &= ~(0x01 << 12);                              // ����λ[12]��0=8λ��1=9λ
    UART5 -> CR2  =   0;                                        // ������0
    UART5 -> CR2 &=  ~(0x03 << 12);                             // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    UART5 -> CR3  =   0;                                        // ������0
    UART5 -> CR3 &= ~(0x01 << 6);                               // DMA����[6]: 0=��ֹ��1=ʹ��
    UART5 -> CR3 &= ~(0x01 << 7);                               // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    UART5->CR1 &= ~(0x01 << 7);                                 // �رշ����ж�
    UART5->CR1 |= 0x01 << 5;                                    // ʹ�ܽ����ж�: ���ջ������ǿ�
    UART5->CR1 |= 0x01 << 4;                                    // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    UART5->SR   = ~(0x00F0);                                    // �����ж�
    // �򿪴���
    UART5 -> CR1 |= 0x01 << 13;                                 // ʹ��UART��ʼ����
    // ����������
    xUART5.puRxData = uaUart5RxData;                            // ��ȡ���ջ������ĵ�ַ
    xUART5.puTxFiFoData = uaUart5TxFiFoData;                    // ��ȡ���ͻ������ĵ�ַ
    // �����ʾ
    printf("UART5 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate);
}

/******************************************************************************
 * ��  ���� UART5_IRQHandler
 * ��  �ܣ� UART5�Ľ����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 * ��  ע�� ���������������ж��¼�ʱ����Ӳ�����á�
 *          ���ʹ�ñ��ļ����룬�ڹ����ļ��������ط���Ҫע��ͬ�������������ͻ��
 ******************************************************************************/
void UART5_IRQHandler(void)
{
    static uint16_t cnt = 0;                                        // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART5_RX_BUF_SIZE];                      // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUARTx.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((UART5->SR & 1 << 7) && (UART5->CR1 & 1 << 7))              // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        UART5->DR = xUART5.puTxFiFoData[xUART5.usTxFiFoTail++];     // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART5.usTxFiFoTail == UART5_TX_BUF_SIZE)               // �������ָ�뵽��β���������±�ǵ�0
            xUART5.usTxFiFoTail = 0;
        if (xUART5.usTxFiFoTail == xUART5.usTxFiFoData)
            UART5->CR1 &= ~(1 << 7);                                // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (UART5->SR & (1 << 5))                                       // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART5_RX_BUF_SIZE))//||xUART5.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART5��֡���������ѳ������ջ����С\r!");
            UART5->DR;                                              // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = UART5->DR ;                                 // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (UART5->SR & (1 << 4))                                       // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART5.usRxNum  = 0;                                        // �ѽ��յ��������ֽ�����0
        memcpy(xUART5.puRxData, rxTemp, UART5_RX_BUF_SIZE);         // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART5.usRxNum  = cnt;                                      // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                    // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART5_RX_BUF_SIZE);                       // �������ݻ������飬����; ׼����һ�εĽ���
        UART5 ->SR;
        UART5 ->DR;                                                 // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}

/******************************************************************************
 * ��  ���� UART5_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t* pudata     �跢�����ݵĵ�ַ
 *          uint8_t  usNum      ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART5_SendData(uint8_t *pudata, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                         // �����ݷ��뻷�λ�����
    {
        xUART5.puTxFiFoData[xUART5.usTxFiFoData++] = pudata[i];  // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART5.usTxFiFoData == UART5_TX_BUF_SIZE)            // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART5.usTxFiFoData = 0;
    }

    if ((UART5->CR1 & 1 << 7) == 0)                              // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        UART5->CR1 |= 1 << 7;                                    // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART5_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART5_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // ����һ������, ���������������0
    va_list ap;                                             // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                 // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                  // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                             // ��տɱ�����б�
    UART5_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART5_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART5_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART5_ClearRx();                                              // ��0
    UART5_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART5_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART5_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART5_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}

/******************************************************************************
 * ��  ���� UART5_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART5_GetRxNum(void)
{
    return xUART5.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART5_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� ���ݵĵ�ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART5_GetRxData(void)
{
    return xUART5.puRxData ;
}

/******************************************************************************
 * ��  ���� UART5_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART5_ClearRx(void)
{
    xUART5.usRxNum = 0 ;
}
#endif  // endif UART5_EN




//////////////////////////////////////////////////////////////   USART-6   //////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if UART6_EN

static xUSATR_TypeDef  xUART6 = { 0 };                      // ���� UART6 ���շ��ṹ��
static uint8_t uaUart6RxData[UART6_RX_BUF_SIZE];            // ���� UART6 �Ľ��ջ���
static uint8_t uaUart6TxFiFoData[UART6_TX_BUF_SIZE];        // ���� UART6 �ķ��ͻ���

/******************************************************************************
 * ��  ���� UART6_Init
 * ��  �ܣ� ��ʼ��USART6��ͨ�����š�Э��������ж����ȼ�
 *          ���ţ�TX-PC6��RX-PC7
 *          Э�飺������-None-8-1
 *          ���ͣ������ж�
 *          ���գ�����+�����ж�
 *
 * ��  ���� uint32_t ulBaudrate  ͨ�Ų�����
 * ����ֵ�� ��
 ******************************************************************************/
void UART6_Init(uint32_t ulBaudrate)
{
    // ʹ�����ʱ��
    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;                       // ʹ�����裺UART6
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;                        // ʹ��GPIO��GPIOC
    // �رմ���
    USART6 -> CR1  =   0;                                       // �رմ��ڣ���������

#ifdef USE_STDPERIPH_DRIVER                                     // ��׼�� ����
    // ����TX����
    GPIO_InitTypeDef  GPIO_InitStructure = {0};                 // GPIO ��ʼ���ṹ��
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;                 // ���ű�ţ�TX_PC6
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;               // ���ŷ���: ���ù���
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;              // ���ģʽ������
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;               // ������������
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;           // ����ٶȣ�50MHz
    GPIO_Init(GPIOC, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // ����RX
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;                 // ���ű�ţ�RX_PC7
    GPIO_Init(GPIOC, &GPIO_InitStructure);                      // ��ʼ�������������������µ�оƬ�Ĵ���
    // �������ŵľ��帴�ù���
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_USART6);   // ����PC6���ù��ܣ�USART6
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_USART6);   // ����PC7���ù��ܣ�USART6
    // �ж�����
    NVIC_InitTypeDef  NVIC_InitStructure = {0};                 // �ж����ȼ����ýṹ��
    NVIC_InitStructure .NVIC_IRQChannel = USART6_IRQn;
    NVIC_InitStructure .NVIC_IRQChannelPreemptionPriority = 1;  // ��ռ���ȼ�
    NVIC_InitStructure .NVIC_IRQChannelSubPriority = 1;         // �����ȼ�
    NVIC_InitStructure .NVIC_IRQChannelCmd = ENABLE;            // IRQͨ��ʹ��
    NVIC_Init(&NVIC_InitStructure);
#endif

#ifdef USE_HAL_DRIVER                                           // HAL�� ����
    // GPIO���Ź���ģʽ����
    GPIO_InitTypeDef    GPIO_InitStruct = {0};                  // ������ʼ��Ҫ�õ��Ľṹ��
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;            // ���� TX-PC6��RX-PC7
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;                    // ����ģʽ
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ������
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // ��������
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;                // ���Ÿ��ù���
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                     // ��ʼ�����Ź���ģʽ
    // �ж���ѡ������
    HAL_NVIC_SetPriority(USART6_IRQn, 1, 1);                    // �����ж��ߵ����ȼ�
    HAL_NVIC_EnableIRQ(USART6_IRQn);                            // ʹ���ж���
#endif

    // ���㲨����
    float    temp;
    uint16_t mantissa, fraction;
    SystemCoreClockUpdate();                                    // ����ϵͳ����Ƶ��ȫ��ֵ; ����SystemCoreClock( )���ڱ�׼�⡢HAL��ͨ��
    temp = (float)(SystemCoreClock / 2) / (ulBaudrate * 16);    // �����ʹ�ʽ����; USART6������APB2, ʱ��Ϊϵͳʱ�ӵ�2��Ƶ; ȫ�ֱ���SystemCoreClock���ڱ�׼�⡢HAL��ͨ��;
    mantissa = temp;				                            // ��������
    fraction = (temp - mantissa) * 16;                          // С������
    USART6 -> BRR  = mantissa << 4 | fraction;                  // ���ò�����
    // ͨ�Ų�������
    USART6 -> CR1 |=   0x01 << 2;                               // ����ʹ��[2]: 0=ʧ�ܡ�1=ʹ��
    USART6 -> CR1 |=   0x01 << 3;                               // ����ʹ��[3]��0=ʧ�ܡ�1=ʹ��
    USART6 -> CR1 &= ~(0x01 << 12);                             // ����λ[12]��0=8λ��1=9λ
    USART6 -> CR2  =   0;                                       // ������0
    USART6 -> CR2 &=  ~(0x03 << 12);                            // ֹͣλ[13:12]��00b=1��ֹͣλ��01b=0.5��ֹͣλ��10b=2��ֹͣλ��11b=1.5��ֹͣλ
    USART6 -> CR3  =   0;                                       // ������0
    USART6 -> CR3 &= ~(0x01 << 6);                              // DMA����[6]: 0=��ֹ��1=ʹ��
    USART6 -> CR3 &= ~(0x01 << 7);                              // DMA����[7]: 0=��ֹ��1=ʹ��
    // �����ж�����
    USART6->CR1 &= ~(0x01 << 7);                                // �رշ����ж�
    USART6->CR1 |= 0x01 << 5;                                   // ʹ�ܽ����ж�: ���ջ������ǿ�
    USART6->CR1 |= 0x01 << 4;                                   // ʹ�ܿ����жϣ�����1�ֽ�ʱ��û�յ�������
    USART6->SR   = ~(0x00F0);                                   // �����ж�
    // �򿪴���
    USART6 -> CR1 |= 0x01 << 13;                                // ʹ��UART��ʼ����
    // ����������
    xUART6.puRxData = uaUart6RxData;                            // ��ȡ���ջ������ĵ�ַ
    xUART6.puTxFiFoData = uaUart6TxFiFoData;                    // ��ȡ���ͻ������ĵ�ַ
    // �����ʾ
    printf("UART6 ��ʼ������         %d-None-8-1; ����ɳ�ʼ�����á��շ�����\r", ulBaudrate);
}

/******************************************************************************
 * ��  ���� USART6_IRQHandler
 * ��  �ܣ� USART6�Ľ����жϡ������жϡ������ж�
 * ��  ���� ��
 * ����ֵ�� ��
 *
******************************************************************************/
void USART6_IRQHandler(void)
{
    static uint16_t cnt = 0;                                         // �����ֽ����ۼƣ�ÿһ֡�����ѽ��յ����ֽ���
    static uint8_t  rxTemp[UART6_RX_BUF_SIZE];                       // �������ݻ������飺ÿ�½��գ����ֽڣ���˳���ŵ������һ֡������(���������ж�), ��ת�浽ȫ�ֱ�����xUARTx.puRxData[xx]�У�

    // �����жϣ����ڰѻ��λ�������ݣ����ֽڷ���
    if ((USART6->SR & 1 << 7) && (USART6->CR1 & 1 << 7))             // ���TXE(�������ݼĴ�����)��TXEIE(���ͻ��������ж�ʹ��)
    {
        USART6->DR = xUART6.puTxFiFoData[xUART6.usTxFiFoTail++];     // ��Ҫ���͵��ֽڣ�����USART�ķ��ͼĴ���
        if (xUART6.usTxFiFoTail == UART6_TX_BUF_SIZE)                // �������ָ�뵽��β���������±�ǵ�0
            xUART6.usTxFiFoTail = 0;
        if (xUART6.usTxFiFoTail == xUART6.usTxFiFoData)
            USART6->CR1 &= ~(1 << 7);                                // �ѷ�����ɣ��رշ��ͻ����������ж� TXEIE
        return;
    }

    // �����жϣ���������ֽڽ��գ���ŵ���ʱ����
    if (USART6->SR & (1 << 5))                                       // ���RXNE(�����ݼĴ����ǿձ�־λ); RXNE�ж�������������DRʱ�Զ�������
    {
        if ((cnt >= UART6_RX_BUF_SIZE))//||(xUART1.ReceivedFlag==1   // �ж�1: ��ǰ֡�ѽ��յ���������������(������), Ϊ�������������������յ�������ֱ��������
        {
            // �ж�2: ���֮ǰ���պõ����ݰ���û�������ͷ��������ݣ�����������֡���ܸ��Ǿ�����֡��ֱ��������֡��������ȱ�㣺���ݴ�������ڴ����ٶ�ʱ��������ô����������������ڵ���
            printf("���棺UART6��֡���������ѳ������ջ����С\r!");
            USART6->DR;                                              // ��ȡ���ݼĴ��������ݣ��������森��Ҫ���ã���DRʱ�Զ����������жϱ�־��
            return;
        }
        rxTemp[cnt++] = USART6->DR ;                                 // �����յ����ֽ����ݣ�˳���ŵ�RXTemp�����У�ע�⣺��ȡDRʱ�Զ������ж�λ
        return;
    }

    // �����жϣ������ж�һ֡���ݽ������������ݵ��ⲿ����
    if (USART6->SR & (1 << 4))                                       // ���IDLE(�����жϱ�־λ); IDLE�жϱ�־�����������������㣬USART1 ->SR;  USART1 ->DR;
    {
        xUART6.usRxNum  = 0;                                         // �ѽ��յ��������ֽ�����0
        memcpy(xUART6.puRxData, rxTemp, UART6_RX_BUF_SIZE);          // �ѱ�֡���յ������ݣ����뵽�ṹ��������ԱxUARTx.puRxData��, �ȴ�����; ע�⣺���Ƶ����������飬����0ֵ���Է����ַ������ʱβ����0���ַ���������
        xUART6.usRxNum  = cnt;                                       // �ѽ��յ����ֽ��������뵽�ṹ�����xUARTx.usRxNum�У�
        cnt = 0;                                                     // �����ֽ����ۼ���������; ׼����һ�εĽ���
        memset(rxTemp, 0, UART6_RX_BUF_SIZE);                        // �������ݻ������飬����; ׼����һ�εĽ���
        USART6 ->SR;
        USART6 ->DR;                                                 // ����IDLE�жϱ�־λ!! �������㣬˳���ܴ�!!
        return;
    }

    return;
}


/******************************************************************************
 * ��  ���� UART6_SendData
 * ��  �ܣ� UARTͨ���жϷ�������
 *         ���ʺϳ������������ɷ��͸������ݣ����������ַ�������int,char
 *         ���� �� �ϡ�ע��h�ļ���������ķ���������С��ע������ѹ�뻺�������ٶ��봮�ڷ����ٶȵĳ�ͻ
 * ��  ���� uint8_t  *puData   �跢�����ݵĵ�ַ
 *          uint8_t   usNum    ���͵��ֽ��� ������������h�ļ������õķ��ͻ�������С�궨��
 * ����ֵ�� ��
 ******************************************************************************/
void UART6_SendData(uint8_t *puData, uint16_t usNum)
{
    for (uint16_t i = 0; i < usNum; i++)                           // �����ݷ��뻷�λ�����
    {
        xUART6.puTxFiFoData[xUART6.usTxFiFoData++] = puData[i];    // ���ֽڷŵ�����������λ�ã�Ȼ��ָ�����
        if (xUART6.usTxFiFoData == UART6_TX_BUF_SIZE)              // ���ָ��λ�õ��ﻺ���������ֵ�����0
            xUART6.usTxFiFoData = 0;
    }

    if ((USART6->CR1 & 1 << 7) == 0)                               // ���USART�Ĵ����ķ��ͻ����������ж�(TXEIE)�Ƿ��Ѵ�
        USART6->CR1 |= 1 << 7;                                     // ��TXEIE�ж�
}

/******************************************************************************
 * ��  ���� UART6_SendString
 * ��  �ܣ� �����ַ���
 *          �÷���ο�printf����ʾ���е�չʾ
 *          ע�⣬������ֽ���Ϊ512-1���ַ������ں������޸�����
 * ��  ���� const char *pcString, ...   (��ͬprintf���÷�)
 * ����ֵ�� ��
 ******************************************************************************/
void UART6_SendString(const char *pcString, ...)
{
    char mBuffer[512] = {0};;                               // ����һ������, ���������������0
    va_list ap;                                             // �½�һ���ɱ�����б�
    va_start(ap, pcString);                                 // �б�ָ���һ���ɱ����
    vsnprintf(mBuffer, 512, pcString, ap);                  // �����в���������ʽ�����������; ����2�������Ʒ��͵�����ֽ���������ﵽ���ޣ���ֻ��������ֵ-1; ���1�ֽ��Զ���'\0'
    va_end(ap);                                             // ��տɱ�����б�
    UART6_SendData((uint8_t *)mBuffer, strlen(mBuffer));    // ���ֽڴ�Ż��λ��壬�Ŷ�׼������
}

/******************************************************************************
 * ��    ���� UART6_SendAT
 * ��    �ܣ� ����AT����, ���ȴ�������Ϣ
 * ��    ���� char     *pcString      ATָ���ַ���
 *            char     *pcAckString   �ڴ���ָ�����Ϣ�ַ���
 *            uint16_t  usTimeOut     ���������ȴ���ʱ�䣬����
 *
 * �� �� ֵ�� 0-ִ��ʧ�ܡ�1-ִ������
 ******************************************************************************/
uint8_t UART6_SendAT(char *pcAT, char *pcAckString, uint16_t usTimeOutMs)
{
    UART6_ClearRx();                                              // ��0
    UART6_SendString(pcAT);                                       // ����ATָ���ַ���
    
    while (usTimeOutMs--)                                         // �ж��Ƿ���ʱ(����ֻ���򵥵�ѭ���жϴ���������
    {
        if (UART6_GetRxNum())                                     // �ж��Ƿ���յ�����
        {
            UART6_ClearRx();                                      // ��0�����ֽ���; ע�⣺���յ����������� ����û�б���0��
            if (strstr((char *)UART6_GetRxData(), pcAckString))   // �жϷ����������Ƿ����ڴ����ַ�
                return 1;                                         // ���أ�0-��ʱû�з��ء�1-���������ڴ�ֵ
        }
        delay_ms(1);                                              // ��ʱ; ���ڳ�ʱ�˳���������������
    }
    return 0;                                                     // ���أ�0-��ʱ�������쳣��1-���������ڴ�ֵ
}

/******************************************************************************
 * ��  ���� UART6_GetRxNum
 * ��  �ܣ� ��ȡ����һ֡���ݵ��ֽ���
 * ��  ���� ��
 * ����ֵ�� 0=û�н��յ����ݣ���0=��һ֡���ݵ��ֽ���
 ******************************************************************************/
uint16_t UART6_GetRxNum(void)
{
    return xUART6.usRxNum ;
}

/******************************************************************************
 * ��  ���� UART6_GetRxData
 * ��  �ܣ� ��ȡ����һ֡���� (���ݵĵ�ַ��
 * ��  ���� ��
 * ����ֵ�� ���ݵĵ�ַ(uint8_t*)
 ******************************************************************************/
uint8_t *UART6_GetRxData(void)
{
    return xUART6.puRxData ;
}

/******************************************************************************
 * ��  ���� UART6_ClearRx
 * ��  �ܣ� �������һ֡���ݵĻ���
 *          ��Ҫ����0�ֽ�������Ϊ���������жϽ��յı�׼
 * ��  ���� ��
 * ����ֵ�� ��
 ******************************************************************************/
void UART6_ClearRx(void)
{
    xUART6.usRxNum = 0 ;
}
#endif  // endif UART6_EN





/////////////////////////////////////////////////////////////  ��������   /////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************
 * ��  ���� showData
 * ��  �ܣ� ��printf�����͵����������ϣ�����۲�
 * ��  ���� uint8_t  *rxData   ���ݵ�ַ
 *          uint16_t  rxNum    �ֽ���
 * ����ֵ�� ��
 ******************************************************************************/
void showData(uint8_t *puRxData, uint16_t usRxNum)
{
    printf("�ֽ����� %d \r", usRxNum);                   // ��ʾ�ֽ���
    printf("ASCII ��ʾ����: %s\r", (char *)puRxData);    // ��ʾ���ݣ���ASCII��ʽ��ʾ�������ַ����ķ�ʽ��ʾ
    printf("16������ʾ����: ");                          // ��ʾ���ݣ���16���Ʒ�ʽ����ʾÿһ���ֽڵ�ֵ
    while (usRxNum--)                                    // ����ֽ��жϣ�ֻҪ��Ϊ'\0', �ͼ���
        printf("0x%X ", *puRxData++);                    // ��ʽ��
    printf("\r\r");                                      // ��ʾ����
}





//////////////////////////////////////////////////////////////  printf   //////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/******************************************************************************
 * ����˵���� printf����֧�ִ���
 *           ���ر�ע�⡿�������´���, ʹ��printf����ʱ, ������Ҫ��use MicroLIB
 * ��    ע��
 * �����£� 2024��06��07��
 ******************************************************************************/
__ASM(".global __use_no_semihosting");

struct __FILE
{
    int handle;
};                                         // ��׼����Ҫ��֧�ֺ���

FILE __stdout;                             // FILE ��stdio.h�ļ�
void _sys_exit(int x)
{
    x = x;                                 // ����_sys_exit()�Ա���ʹ�ð�����ģʽ
}

int fputc(int ch, FILE *f)                 // �ض���fputc������ʹprintf���������fputc�����UART
{
    UART1_SendData((uint8_t *)&ch, 1);     // ʹ�ö���+�жϷ�ʽ��������; ������ʽ1�����ȴ���ʱ����Ҫ������д�õĺ��������λ���
    return ch;
}

