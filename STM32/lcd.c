#include "lcd.h"
#include "font.h"

// 软件 SPI 引脚宏定义 (绑定我们规划的引脚)
#define LCD_SCL_Clr() GPIO_ResetBits(GPIOB, GPIO_Pin_4)
#define LCD_SCL_Set() GPIO_SetBits(GPIOB, GPIO_Pin_4)
#define LCD_SDA_Clr() GPIO_ResetBits(GPIOB, GPIO_Pin_5)
#define LCD_SDA_Set() GPIO_SetBits(GPIOB, GPIO_Pin_5)
#define LCD_RES_Clr() GPIO_ResetBits(GPIOB, GPIO_Pin_6)
#define LCD_RES_Set() GPIO_SetBits(GPIOB, GPIO_Pin_6)
#define LCD_DC_Clr()  GPIO_ResetBits(GPIOB, GPIO_Pin_7)
#define LCD_DC_Set()  GPIO_SetBits(GPIOB, GPIO_Pin_7)
#define LCD_CS_Clr()  GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define LCD_CS_Set()  GPIO_SetBits(GPIOB, GPIO_Pin_8)

// 简单的微秒级延时
static void LCD_Delay(volatile uint32_t i) {
    while(i--);
}

// 模拟 SPI 向屏幕发送 1 字节数据
static void LCD_Writ_Bus(uint8_t dat) {
    uint8_t i;
    for(i=0; i<8; i++) {
        LCD_SCL_Clr();
        if(dat & 0x80) LCD_SDA_Set();
        else LCD_SDA_Clr();
        LCD_SCL_Set();
        dat <<= 1;
    }
}

// 写命令
static void LCD_WR_REG(uint8_t dat) {
    LCD_CS_Clr();
    LCD_DC_Clr();
    LCD_Writ_Bus(dat);
    LCD_CS_Set();
}

// 写数据
static void LCD_WR_DATA8(uint8_t dat) {
    LCD_CS_Clr();
    LCD_DC_Set();
    LCD_Writ_Bus(dat);
    LCD_CS_Set();
}

// 写 16 位颜色数据
static void LCD_WR_DATA(uint16_t dat) {
    LCD_CS_Clr();
    LCD_DC_Set();
    LCD_Writ_Bus(dat >> 8);
    LCD_Writ_Bus(dat);
    LCD_CS_Set();
}

// 设置写入坐标范围
static void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    LCD_WR_REG(0x2a); // 列地址设置
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(x1+2); 
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(x2+2);
    
    LCD_WR_REG(0x2b); // 行地址设置
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(y1+1); 
    LCD_WR_DATA8(0x00); LCD_WR_DATA8(y2+1);
    
    LCD_WR_REG(0x2c); // 准备写入显存
}

// 初始化屏幕
void LCD_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 1. 开启 GPIOB 和 AFIO (复用功能) 的时钟 
    // (因为全在 GPIOB，GPIOA 其实可以不开了，如果别的代码没用到的话)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    
    // 2. 核心操作：禁用 JTAG，保留 SWD。成功释放 PB4！
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // 3. 批量初始化 PB4, PB5, PB6, PB7, PB8, PB9 为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 4. 点亮背光：将 PB9 (BLK) 拉高，确保屏幕背光开启
    GPIO_SetBits(GPIOB, GPIO_Pin_9);

    // 硬件复位
    LCD_RES_Clr();
    LCD_Delay(2000000);
    LCD_RES_Set();
    LCD_Delay(2000000);

    // 回归极简初始化序列 (这套序列你之前验证过100%能出画面)
    LCD_WR_REG(0x11); // 退出睡眠
    LCD_Delay(1200000);

    LCD_WR_REG(0xB1); 
    LCD_WR_DATA8(0x05); LCD_WR_DATA8(0x3C); LCD_WR_DATA8(0x3C); // 帧率

    // ? 唯一关键的一句话：0x08 解决水平镜像，排针朝下拿！
    LCD_WR_REG(0x36); LCD_WR_DATA8(0x08); 

    LCD_WR_REG(0x3A); LCD_WR_DATA8(0x05); // 16-bit颜色
    
    // 如果画面显示出来了，但是底色发黑/发紫，就把这里的 0x20 改成 0x21
    LCD_WR_REG(0x20); 

    LCD_WR_REG(0x29); // 开启显示
}

// 清屏函数
void LCD_Clear(uint16_t Color) {
    uint16_t i, j;
    LCD_Address_Set(0, 0, 127, 127);
    for(i=0; i<128; i++) {
        for(j=0; j<128; j++) {
            LCD_WR_DATA(Color);
        }
    }
}

// 显示单个字符
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc) {
    uint8_t temp, t, m;
    uint16_t i;
    num = num - ' '; // 得到在字体数组中的偏移量
    LCD_Address_Set(x, y, x+7, y+15);
    for(i=0; i<16; i++) {
        temp = asc2_1608[num][i];
        for(m=0; m<8; m++) {
            if(temp & 0x80) LCD_WR_DATA(fc);
            else LCD_WR_DATA(bc);
            temp <<= 1;
        }
    }
}

// 显示字符串
void LCD_ShowString(uint16_t x, uint16_t y, const char *p, uint16_t fc, uint16_t bc) {
    while(*p != '\0') {
        if(x > 120) { x = 0; y += 16; }
        if(y > 112) { y = x = 0; LCD_Clear(bc); }
        LCD_ShowChar(x, y, *p, fc, bc);
        x += 8;
        p++;
    }
}

// 提取数字的每一位
static uint32_t mypow(uint8_t m, uint8_t n) {
    uint32_t result = 1;	 
    while(n--) result *= m;
    return result;
}

// 显示数字
void LCD_ShowNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc) {
    uint8_t t, temp;
    uint8_t enshow = 0;
    for(t=0; t<len; t++) {
        temp = (num / mypow(10, len-t-1)) % 10;
        if(enshow == 0 && t < (len-1)) {
            if(temp == 0) {
                LCD_ShowChar(x+8*t, y, ' ', fc, bc);
                continue;
            } else enshow = 1; 
        }
        LCD_ShowChar(x+8*t, y, temp+'0', fc, bc); 
    }
}