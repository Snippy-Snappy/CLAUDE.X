/*
 * File:   LCD.c
 * Author: Traffic Light Controller
 * 
 * LCD display functions for traffic light controller
 */

#include <xc.h>
#include <stdint.h>
#include "I2C.h"
#include "LCD.h"
#include "Sensors.h"
#include "abs_clock.h"
#include "sensor_manager.h"

// External variables
extern sensor_state_t sensors;
extern volatile uint32_t time_counter;
extern enum STATE state;
extern enum ON HAZARD;
extern light PRWS, PRES, PRWT, RWS, DMS;
        uint8_t LCD_ADDR = 0x27; // current time for main loop

static uint8_t lcd_initialized = 0;

// Initialize LCD
void lcd_init(void) {
    // Small delay for LCD power-up
    for (volatile uint16_t i = 0; i < 10000; i++);
        if (setup_LCD(LCD_ADDR) == -1) {
        LCD_ADDR = 0x3f;
        if (setup_LCD(LCD_ADDR) == -1) {
            LCD_ADDR = 0x20;
            if (setup_LCD(LCD_ADDR) == -1) {
                while(1) {
                    PORTB ^= 0x20;  // flash the led as we don't know.
                }
            }
        }
    }
    if (setup_LCD(LCD_ADDR) == 0) {
        lcd_initialized = 1;
        LCD_clear(LCD_ADDR);
        
        // Display startup message
        LCD_Position(LCD_ADDR, 0x00);
        LCD_Write(LCD_ADDR, "Traffic Control", 15);
        LCD_Position(LCD_ADDR, 0x40);
        LCD_Write(LCD_ADDR, "Initializing...", 15);
    }
}

// Clear LCD
void lcd_clear(void) {
    if (lcd_initialized) {
        LCD_clear(LCD_ADDR);
    }
}

// Set cursor position (row: 0-1, col: 0-15)
void lcd_goto(uint8_t col, uint8_t row) {
    if (lcd_initialized) {
        uint8_t pos = (row == 0) ? col : (0x40 + col);
        LCD_Position(LCD_ADDR, pos);
    }
}

// Write string to LCD
void lcd_puts(const char* str) {
    if (!lcd_initialized) return;
    
    uint8_t len = 0;
    while (str[len] && len < 16) {
        len++;
    }
    LCD_Write(LCD_ADDR, (char*)str, len);
}

// Write single character
void lcd_putc(char c) {
    if (lcd_initialized) {
        LCD_Write_Chr(LCD_ADDR, c);
    }
}

// Update LCD display with system status
void lcd_update_display(void) {
    if (!lcd_initialized) return;
    
    char line1[17];
    char line2[17];
    
    // Build Line 1: Sensor states and time counter
    // Format: "SSSSSS TTTTT"
    for (uint8_t i = 0; i < 6; i++) {
        if (i == 5) {  // S6 is hazard
            line1[i] = HAZARD ? 'X' : '_';
        } else {
            // Show X if triggered but not handled, _ if handled
            if (sensors.triggered & (1<<i)) {
                line1[i] = (sensors.handled & (1<<i)) ? '_' : 'X';
            } else {
                line1[i] = '_';
            }
        }
    }
    line1[6] = ' ';
    line1[7] = ' ';
    line1[8] = ' ';
    line1[9] = ' ';
    line1[10] = ' ';
    
    // Add time counter (5 digits)
    uint32_t display_time = time_counter % 100000;
    line1[11] = '0' + (display_time / 10000);
    line1[12] = '0' + ((display_time / 1000) % 10);
    line1[13] = '0' + ((display_time / 100) % 10);
    line1[14] = '0' + ((display_time / 10) % 10);
    line1[15] = '0' + (display_time % 10);
    line1[16] = '\0';
    
    // Build Line 2: Phase and color info
    // Format: "DPPPCLLLC"
    char dir = ' ';
    const char* phase1 = "   ";
    const char* phase2 = "   ";
    char col1 = ' ';
    char col2 = ' ';
    
    if (state == Hazard) {
        phase1 = "HZD";
        col1 = ' ';
    } else {
        switch (state) {
            case Default:
                phase1 = "PRT";
                col1 = get_color_char(PRWS.colour);
                
                // Direction indicator
                if (PRWS.colour == GREEN && PRES.colour != GREEN) {
                    dir = 'W';
                } else if (PRES.colour == GREEN && PRWS.colour != GREEN) {
                    dir = 'E';
                }
                break;
                
            case ParkRdWestTurn:
                dir = 'W';
                phase1 = "PRT";
                phase2 = "PWT";
                col1 = get_color_char(PRWS.colour);
                col2 = get_color_char(PRWT.colour);
                break;
                
            case RailwayStThrough:
                phase1 = "RST";
                col1 = get_color_char(RWS.colour);
                break;
                
            case DamStThrough:
                phase1 = "DST";
                col1 = get_color_char(DMS.colour);
                break;
        }
    }
    
    // Format line 2
    line2[0] = dir;
    line2[1] = phase1[0];
    line2[2] = phase1[1];
    line2[3] = phase1[2];
    line2[4] = col1;
    line2[5] = phase2[0];
    line2[6] = phase2[1];
    line2[7] = phase2[2];
    line2[8] = col2;
    line2[9] = '\0';
    
    // Write to LCD
    lcd_goto(0, 0);
    lcd_puts(line1);
    lcd_goto(0, 1);
    lcd_puts(line2);
    lcd_goto(17, 1);
}

// Helper function to get color character
char get_color_char(enum COLOUR color) {
    switch (color) {
        case GREEN:  return 'G';
        case YELLOW: return 'Y';
        case RED:    return 'R';
        case OFF:    return '_';
        default:     return ' ';
    }
}