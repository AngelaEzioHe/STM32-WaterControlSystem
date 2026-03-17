#ifndef __LCD_H
#define __LCD_H

#include "stm32f10x.h"

// 常用颜色定义
#define WHITE   0xFFFF
#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F

// API 接口声明
void LCD_Init(void);
void LCD_Clear(uint16_t Color);
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc);
void LCD_ShowString(uint16_t x, uint16_t y, const char *p, uint16_t fc, uint16_t bc);
void LCD_ShowNum(uint16_t x, uint16_t y, uint16_t num, uint8_t len, uint16_t fc, uint16_t bc);

#endif