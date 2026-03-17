#include "stm32f10x.h" // 包含 STM32 标准库核心头文件
#include "lcd.h"       // 引入屏幕驱动头文件

// 简单的软件延时函数
void Delay_ms(uint32_t ms) {
    uint32_t i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 8000; j++); 
    }
}

// 硬件底层初始化配置
void Hardware_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    // 1. 开启时钟：开启 GPIOA、GPIOB 以及 ADC1 的时钟总线
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1, ENABLE);

    // 2. 配置 PA0 为模拟输入模式 (读取水位传感器)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 PB0 (继电器控制) 和 PB1 (蜂鸣器控制) 为推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 初始化安全状态：继电器断开，蜂鸣器安静(低电平触发)
    GPIO_ResetBits(GPIOB, GPIO_Pin_0); 
    GPIO_SetBits(GPIOB, GPIO_Pin_1);

    // 4. 配置 ADC1
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; 
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; 
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 5. 开启 ADC1 并执行内部硬件校准
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

// ==================== 核心主业务逻辑 ====================
int main(void) {
    uint16_t water_level = 0;
    uint16_t threshold = 2000; 

    Hardware_Init(); // 底层硬件初始化
    LCD_Init();      // 屏幕初始化
    
    // 清屏为白色，并显示静态的 UI 框架 (黑字白底)
    LCD_Clear(WHITE); 
    LCD_ShowString(10, 10, "SMART POOL SYS", BLACK, WHITE);
    LCD_ShowString(10, 30, "LEVEL: ", BLACK, WHITE);
    LCD_ShowString(10, 50, "STATUS: ", BLACK, WHITE);

    while(1) {
        water_level = Get_Water_Level(); // 读取水位

        // 实时刷新屏幕上的水位数值 (蓝字白底)
        LCD_ShowNum(60, 30, water_level, 4, BLUE, WHITE); 

        if(water_level > threshold) {
            // 【事件触发：水位过高】
            GPIO_SetBits(GPIOB, GPIO_Pin_0);   // 抽水
            GPIO_ResetBits(GPIOB, GPIO_Pin_1); // 报警
            
            // 屏幕显示报警状态 (红字白底)
            LCD_ShowString(60, 50, "PUMP ON ", RED, WHITE); 
        } else {
            // 【事件触发：水位正常】
            GPIO_ResetBits(GPIOB, GPIO_Pin_0); // 停抽
            GPIO_SetBits(GPIOB, GPIO_Pin_1);   // 停报
            
            // 屏幕显示安全状态 (绿字白底)
            LCD_ShowString(60, 50, "NORMAL  ", GREEN, WHITE); 
        }

        Delay_ms(500); 
    }
}