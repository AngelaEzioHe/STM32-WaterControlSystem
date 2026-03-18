#include "stm32f10x.h" // 包含 STM32 标准库核心头文件
#include "lcd.h"       // 引入屏幕驱动头文件

// ==================== 1. 新增蓝牙所需的头文件和全局变量 ====================
#include "stm32f10x_usart.h" 
#include "misc.h"

typedef enum {
    MODE_AUTO = 0,   // 自动模式
    MODE_MANUAL = 1  // 手动模式
} SystemMode;

// 使用 volatile 修饰，防止中断中被优化
volatile SystemMode current_mode = MODE_AUTO; 
volatile uint8_t manual_pump_state = 0; // 0: 关泵, 1: 开泵


// ==================== 保持你原有的底层配置不变 ====================
// 简单的软件延时函数
void Delay_ms(uint32_t ms) {
    uint32_t i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 8000; j++); 
    }
}

// 硬件底层初始化配置 (PA0, PB0, PB1 及 ADC)
void Hardware_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1, ENABLE);

    // PA0: 水位传感器
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PB0(继电器), PB1(蜂鸣器)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, GPIO_Pin_0); 
    GPIO_SetBits(GPIOB, GPIO_Pin_1);

    // ADC1 配置
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; 
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; 
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1)); 
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));      
}

// 获取水位传感器 ADC 数值
uint16_t Get_Water_Level(void) {
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}


// ==================== 2. 新增蓝牙串口(USART3)配置和中断 ====================
void USART3_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 开启 USART3 (在APB1总线上) 和 GPIOB (在APB2总线上) 的时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 配置 USART3_TX (PB10)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 配置 USART3_RX (PB11)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 串口参数：9600波特率
    USART_InitStructure.USART_BaudRate = 9600; 
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    // 配置接收中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 开启接收中断并使能串口
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART3, ENABLE);
}

void USART3_IRQHandler(void) {
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        uint8_t receive_data = USART_ReceiveData(USART3);
        
        // 在屏幕上打印收到的数据（方便调试）
        LCD_ShowChar(60, 90, receive_data, RED, WHITE);
        LCD_ShowNum(80, 90, receive_data, 3, BLUE, WHITE);
       
        // 指令解析
        if (receive_data == 'A') {         
            current_mode = MODE_AUTO;
        } else if (receive_data == '1') {  
            current_mode = MODE_MANUAL;
            manual_pump_state = 1;
        } else if (receive_data == '0') {  
            current_mode = MODE_MANUAL;
            manual_pump_state = 0;
        }
        
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}


// ==================== 3. 主函数修改为双模式逻辑 ====================
int main(void) {
    uint16_t water_level = 0;
    uint16_t threshold = 2000; 

    // 【新增】配置中断优先级分组（必须加在初始化最前面）
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Hardware_Init(); // 底层硬件初始化
    USART3_Config(); // 【新增】初始化蓝牙模块
    LCD_Init();      // 屏幕初始化
    
    // UI 初始化
    LCD_Clear(WHITE); 
    LCD_ShowString(10, 10, "SMART POOL SYS", BLACK, WHITE);
    LCD_ShowString(10, 30, "LEVEL: ", BLACK, WHITE);
    LCD_ShowString(10, 50, "STATUS: ", BLACK, WHITE);
    LCD_ShowString(10, 70, "MODE: ", BLACK, WHITE); // 【新增】
    LCD_ShowString(10, 90, "RECV: ", BLACK, WHITE); // 【新增】

    while(1) {
        water_level = Get_Water_Level(); // 读取水位
        LCD_ShowNum(60, 30, water_level, 4, BLUE, WHITE); 

        // ------------------ 模式分离控制逻辑 ------------------
        if (current_mode == MODE_AUTO) {
            // 【当前：自动模式】
            LCD_ShowString(60, 70, "AUTO  ", BLACK, WHITE);
            
            if(water_level > threshold) {
                GPIO_SetBits(GPIOB, GPIO_Pin_0);   // 抽水
                GPIO_ResetBits(GPIOB, GPIO_Pin_1); // 报警
                LCD_ShowString(60, 50, "PUMP ON ", RED, WHITE); 
            } else {
                GPIO_ResetBits(GPIOB, GPIO_Pin_0); // 停抽
                GPIO_SetBits(GPIOB, GPIO_Pin_1);   // 停报
                LCD_ShowString(60, 50, "NORMAL  ", GREEN, WHITE); 
            }
            
        } else if (current_mode == MODE_MANUAL) {
            // 【当前：手动模式 (由蓝牙接管)】
            LCD_ShowString(60, 70, "MANUAL", BLACK, WHITE);
            GPIO_SetBits(GPIOB, GPIO_Pin_1); // 蜂鸣器安静(停报)
            
            if (manual_pump_state == 1) {
                GPIO_SetBits(GPIOB, GPIO_Pin_0); // 强制开泵
                LCD_ShowString(60, 50, "PUMP ON*", RED, WHITE); 
            } else {
                GPIO_ResetBits(GPIOB, GPIO_Pin_0); // 强制停抽
                LCD_ShowString(60, 50, "NORMAL *", GREEN, WHITE); 
            }
        }

        Delay_ms(500); // 刷新率控制
    }
}