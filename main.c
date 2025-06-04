/*
 * File:   main.c
 * Author: Traffic Light Controller
 * 
 * ELEC3042 Major Project - Traffic Light Controller
 */

#include <xc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include "abs_clock.h"
#include "SPI.h"
#include "Sensors.h"
#include "POT.h"
#include "I2C.h"
#include "sensor_manager.h"
#include "LCD.h"

// Debug macros
#define debug   (PORTD &= 0x00)
#define debug1  (PORTD ^= 0b10000000)
#define debug2  (PORTD ^= 0b01000000)
#define debug3  (PORTD ^= 0b00100000)
#define debug4  (PORTD ^= 0b00010000)

// Function prototypes
void buttonPressed(void);
void write_LEDs(uint16_t leds);
void uint_to_string(uint32_t num, char* str, uint8_t width);
void string_copy(char* dest, const char* src);

// Global variables
volatile enum ON button_int = FALSE;
volatile uint32_t time_period_ms = 1000;  // Default 1 second
volatile uint32_t time_counter = 0;       // For LCD display
uint32_t hazard_start_time = 0;
uint32_t last_lcd_update = 0;


// External declaration
extern volatile enum ON button_int;

// ISR for button interrupts
ISR(INT0_vect) {
    button_int = TRUE;
}

ISR(PCINT0_vect) {
    button_int = TRUE;
}

// ADC ISR for potentiometer reading
ISR(ADC_vect) {
    uint16_t adc_value = ADC;
    // Map ADC value (0-1023) to time period (50-1000 ms)
    time_period_ms = 50 + ((uint32_t)adc_value * 950) / 1023;
}

void setup_hardware(void) {
    // Port B setup
    DDRB |= 0b00000010;   // PB1 speaker output
    DDRB &= 0b11111110;   // PB0 S4 input
    PORTB |= 0b00000001;  // Enable pull-up on S4
    
    // Port C setup
    DDRC |= 0b00001110;   // PC1-3 as outputs for LEDs
    PORTC &= ~0b00001110; // Start with LEDs off
    
    // Port D setup
    DDRD |= 0b11110000;   // PD4-7 as outputs for debug LEDs
    DDRD &= ~0b00001000;  // PD3 as input for hazard
    PORTD &= ~0b11110000; // Turn off debug LEDs
    PORTD |= 0b00001000;  // Enable pull-up on hazard input
    
    // Setup interrupts
    PCMSK0 |= 0b00000001;  // Enable PCINT0 for S4
    PCICR |= 0b00000001;   // Enable PCINT0 interrupt
    
    // INT0 setup for SPI interrupt
    EICRA = _BV(ISC01);    // Falling edge of INT0
    EIMSK = _BV(INT0);
    EIFR = _BV(INTF0);
    
    // ADC setup for potentiometer
    ADMUX = 0x40;          // AVcc reference, ADC0 (PC0)
    ADCSRA = 0xEF;         // Enable ADC, auto-trigger, interrupt, prescaler 128
    ADCSRB = 0x00;         // Free running mode
}

void read_sensors(void) {
    // Update sensor states with debouncing
    update_sensor_states();
    
    // Check hazard input directly (PD3)
    if (!(PIND & (1<<3))) {
        if (!HAZARD) {
            HAZARD = TRUE;
            hazard_start_time = millis();
        }
    } else if (HAZARD && ((millis() - hazard_start_time) > 10000)) {
        HAZARD = FALSE;
    }
}

void update_lcd(void) {
    lcd_update_display();
}
void write_LEDs(uint16_t leds) {
    // Write to GPIOA (lower 8 bits)
    SPI_Send_Command(0x14, (leds & 0xFF));
    
    // Write to GPIOB (upper 8 bits)
    SPI_Send_Command(0x15, ((leds >> 8) & 0xFF));
}
int main(void) {
    // Initialize hardware
    setup_hardware();
    setupTimer2();
    setupTimer1();

    setup_SPI();
    setup_PortExpander();
    setup_I2C();
    lcd_init();  // Now properly implemented
    
    // Initialize states
    HAZARD = TRUE;
    state = Hazard;
    hazard_start_time = millis();
    clear_all_sensors();
    
    // Initialize all lights to OFF for hazard (will flash)
    PRWS.colour = OFF;
    PRES.colour = OFF;
    PRWT.colour = OFF;
    RWS.colour = OFF;
    DMS.colour = OFF;
    
    // Enable interrupts
    sei();
        while ((millis()) < 2000) {
            ;
    }
    uint32_t last_state_update = 0;
    uint32_t last_light_update = 0;
    uint32_t last_sensor_read = 0;
    uint32_t last_time_increment = 0;
    lcd_clear();
    while (1) {
        uint32_t now = millis();
        if (button_int) {
            read_sensors();
        }
        // Check for transition out of hazard
        if (HAZARD && !(PIND & (1<<3))) {
            hazard_start_time = now;
        } else if (HAZARD && (PIND & (1<<3)) && ((now - hazard_start_time) >= 10000)) {
            // Exit hazard mode
            HAZARD = FALSE;
            state = Default;
            
            // Clear all sensor states
            clear_all_sensors();
            
            // Set initial state for normal operation
            PRWS.colour = GREEN;
            PRES.colour = GREEN;
            PRWT.colour = RED;
            RWS.colour = RED;
            DMS.colour = RED;
            
            timing_prws.green_start = now;
            timing_prws.red_start = now;
            timing_prwt.red_start = now;
            timing_rws.red_start = now;
            timing_dms.red_start = now;
            
            // Reset all demands
            PRWS.on = FALSE;
            PRES.on = FALSE;
            PRWT.on = FALSE;
            RWS.on = FALSE;
            DMS.on = FALSE;
            
            // Reset phase done flags
            PRWS.phaseDone = FALSE;
            PRES.phaseDone = FALSE;
            PRWT.phaseDone = TRUE;
            RWS.phaseDone = TRUE;
            DMS.phaseDone = TRUE;
            
            // Mark default sensors as handled
            mark_phase_sensors_handled(Default);
        }
    
        // Read sensors every 10ms
        if ((now - last_sensor_read) >= 10) {
            read_sensors();
            last_sensor_read = now;
        }
        
        // Update state machine every 100ms
        if ((now - last_state_update) >= 100) {
            State_Manager();
            last_state_update = now;
           // clear_all_sensors();

        }
        
        // Update lights every 20ms
        if ((now - last_light_update) >= 20) {
            uint32_t lights = get_Lights();
            write_LEDs(lights);
            lights = lights >> 16;
            PORTC = (lights & 0x0f);
            last_light_update = now;
        }
        
        // Update LCD every 200ms
        if ((now - last_lcd_update) >= 200) {
            update_lcd();
            last_lcd_update = now;
        }
        
        // Increment time counter every time period
        if ((now - last_time_increment) >= time_period_ms) {
            time_counter++;
            if (time_counter > 99999) {
                time_counter = 0;
            }
            last_time_increment = now;
        }
        
        
    }
    return 0;
}