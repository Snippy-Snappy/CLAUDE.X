/*
 * File:   LCD.h
 * Author: Traffic Light Controller
 * 
 * LCD display header
 */

#ifndef LCD_H
#define LCD_H
#include <xc.h> // include processor files - each processor file is guarded.  
#include <stdint.h>
#include "Sensors.h"

// Function prototypes
void lcd_init(void);
void lcd_clear(void);
void lcd_goto(uint8_t col, uint8_t row);
void lcd_puts(const char* str);
void lcd_putc(char c);
void lcd_update_display(void);
char get_color_char(enum COLOUR color);

#endif /* LCD_H */