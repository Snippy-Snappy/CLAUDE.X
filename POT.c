/*
 * File:   POT.c
 * Author: Traffic Light Controller
 * 
 * Potentiometer implementation for variable time periods
 */

#include <xc.h>
#include <avr/io.h>
#include "POT.h"

// Setup ADC for potentiometer on PC0
void setup_POT(void) {
    // Configure ADC
    ADMUX = (1 << REFS0);    // AVcc reference, ADC0 (PC0)
    
    // Enable ADC, set prescaler to 128 for 125kHz ADC clock @ 16MHz
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    
    // Disable digital input on PC0
    DIDR0 |= (1 << ADC0D);
    
    // Start first conversion
    ADCSRA |= (1 << ADSC);
}

// Read potentiometer value (blocking)
uint16_t read_POT(void) {
    // Start conversion
    ADCSRA |= (1 << ADSC);
    
    // Wait for conversion to complete
    while (ADCSRA & (1 << ADSC));
    
    // Return 10-bit result
    return ADC;
}

// Convert ADC reading to time period in milliseconds
// ADC value 0-1023 maps to 50ms-1000ms
uint32_t get_time_period_ms(void) {
    uint16_t adc_value = read_POT();
    
    // Map 0-1023 to 50-1000ms
    // Formula: time_ms = 50 + (adc_value * 950) / 1023
    uint32_t time_period = 50 + ((uint32_t)adc_value * 950) / 1023;
    
    return time_period;
}