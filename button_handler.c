/*
 * File:   button_handler.c
 * Author: Traffic Light Controller
 * 
 * Button interrupt handling
 */

#include <xc.h>
#include "SPI.h"
#include "Sensors.h"

// External declaration
extern volatile enum ON button_int;

// Function to handle button press interrupts
void buttonPressed(void) {
    uint8_t bp;
    
    // Clear interrupt flag
    button_int = FALSE;
    
    // Read Port A from expander (S0, S1)
    bp = SPI_Read_Command(0x12);
    
    // Check S0 - Dam Street (bit 0)
    if ((bp & S0) == 0) {
        Sensor_Manager(0);
        return;
    }
    
    // Check S1 - Park Road West (bit 4)
    if ((bp & S1) == 0) {
        Sensor_Manager(1);
        return;
    }
    
    // Read Port B from expander (S2, S3)
    bp = SPI_Read_Command(0x13);
    
    // Check S2 - Park Road West Turn (bit 0)
    if ((bp & S2) == 0) {
        Sensor_Manager(2);
        return;
    }
    
    // Check S3 - Park Road East (bit 4)
    if ((bp & S3) == 0) {
        Sensor_Manager(3);
        return;
    }
}

// Function to write LED states to port expander
/*
 * File:   utils.c
 * Author: Traffic Light Controller
 * 
 * Utility functions for string manipulation without stdio.h
 */

#include <stdint.h>

// Convert a number to string with fixed width and leading zeros
void uint_to_string(uint32_t num, char* str, uint8_t width) {
    uint8_t i;
    
    // Fill with zeros first
    for (i = 0; i < width; i++) {
        str[i] = '0';
    }
    str[width] = '\0';
    
    // Convert number to string from right to left
    i = width - 1;
    while (num > 0 && i < width) {
        str[i] = '0' + (num % 10);
        num /= 10;
        if (i == 0) break;
        i--;
    }
}

// Simple string copy
void string_copy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Simple string length
uint8_t string_length(const char* str) {
    uint8_t len = 0;
    while (*str++) {
        len++;
    }
    return len;
}