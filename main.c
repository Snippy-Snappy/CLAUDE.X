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
#include "button_handler.h"
// Note: LCD functions will need to be implemented without stdio.h

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
/*
 * File:   button_handler.c
 * Author: Traffic Light Controller
 * 
 * Button interrupt handling
 */

// External declaration
extern volatile enum ON button_int;

// Sensor states for debouncing
typedef struct {
    uint8_t current;
    uint8_t previous;
    uint8_t triggered;
    uint8_t handled;
} sensor_state_t;

sensor_state_t sensors = {0, 0, 0, 0};

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
    // Read hazard input directly
    if (!(PIND & (1<<3))) {
        HAZARD = TRUE;
    } else if (HAZARD && ((millis() - hazard_start_time) > 10000)) {
        HAZARD = FALSE;
    }
    
    // Read sensors from port expander if interrupt occurred
    if (button_int) {
        button_int = FALSE;
        
        // Read Port A sensors (S0, S1)
        uint8_t portA = SPI_Read_Command(0x12);
        // S0 - Dam Street (bit 0)
        if (!(portA & S0)) {
            sensors.triggered |= (1<<0);
        }
        // S1 - Park Road West (bit 4)
        if (!(portA & S1)) {
            sensors.triggered |= (1<<1);
        }
        
        // Read Port B sensors (S2, S3)
        uint8_t portB = SPI_Read_Command(0x13);
        // S2 - Park Road West Turn (bit 0)
        if (!(portB & S2)) {
            sensors.triggered |= (1<<2);
        }
        // S3 - Park Road East (bit 4)
        if (!(portB & S3)) {
            sensors.triggered |= (1<<3);
        }
    }
    
    // Check S4 - Railway Street (PB0)
    if (!(PINB & (1<<0))) {
        sensors.triggered |= (1<<4);
    }
    
    // Update sensor states in light structures
    if (sensors.triggered & (1<<0) && !(sensors.handled & (1<<0))) {
        DMS.on = TRUE;
    }
    if (sensors.triggered & (1<<1) && !(sensors.handled & (1<<1))) {
        PRWS.on = TRUE;
    }
    if (sensors.triggered & (1<<2) && !(sensors.handled & (1<<2))) {
        PRWT.on = TRUE;
    }
    if (sensors.triggered & (1<<3) && !(sensors.handled & (1<<3))) {
        PRES.on = TRUE;
    }
    if (sensors.triggered & (1<<4) && !(sensors.handled & (1<<4))) {
        RWS.on = TRUE;
    }
}

void update_lcd(void) {
    char lcd_line1[17];
    char lcd_line2[17];
    uint8_t i;
    
    // Line 1: Sensor states and time counter
    // Format: "XXXXXX 00000"
    for (i = 0; i < 6; i++) {
        if (i == 5) {  // S6 is hazard
            lcd_line1[i] = HAZARD ? 'X' : '_';
        } else if (sensors.triggered & (1<<i)) {
            lcd_line1[i] = (sensors.handled & (1<<i)) ? '_' : 'X';
        } else {
            lcd_line1[i] = '_';
        }
    }
    lcd_line1[6] = ' ';
    
    // Time counter (5 digits)
    uint_to_string(time_counter % 100000, &lcd_line1[7], 5);
    lcd_line1[12] = '\0';
    
    // Line 2: Phase and state info
    char dir = ' ';
    char phase1[4] = "   ";
    char phase2[4] = "   ";
    char col1 = ' ';
    char col2 = ' ';
    
    if (state == Hazard) {
        string_copy(phase1, "HZD");
    } else {
        switch (state) {
            case Default:
                string_copy(phase1, "PRT");
                col1 = (PRWS.colour == GREEN) ? 'G' : 
                       (PRWS.colour == YELLOW) ? 'Y' : 
                       (PRWS.colour == RED) ? 'R' : '_';
                // Check if only one direction
                if (PRWS.colour == GREEN && PRES.colour != GREEN) {
                    dir = 'W';
                } else if (PRES.colour == GREEN && PRWS.colour != GREEN) {
                    dir = 'E';
                }
                break;
                
            case ParkRdWestTurn:
                string_copy(phase1, "PRT");
                string_copy(phase2, "PWT");
                col1 = (PRWS.colour == GREEN) ? 'G' : 
                       (PRWS.colour == YELLOW) ? 'Y' : 
                       (PRWS.colour == RED) ? 'R' : '_';
                col2 = (PRWT.colour == GREEN) ? 'G' : 
                       (PRWT.colour == YELLOW) ? 'Y' : 
                       (PRWT.colour == RED) ? 'R' : '_';
                break;
                
            case RailwayStThrough:
                string_copy(phase1, "RST");
                col1 = (RWS.colour == GREEN) ? 'G' : 
                       (RWS.colour == YELLOW) ? 'Y' : 
                       (RWS.colour == RED) ? 'R' : '_';
                break;
                
            case DamStThrough:
                string_copy(phase1, "DST");
                col1 = (DMS.colour == GREEN) ? 'G' : 
                       (DMS.colour == YELLOW) ? 'Y' : 
                       (DMS.colour == RED) ? 'R' : '_';
                break;
        }
    }
    
    // Build line 2
    lcd_line2[0] = dir;
    lcd_line2[1] = phase1[0];
    lcd_line2[2] = phase1[1];
    lcd_line2[3] = phase1[2];
    lcd_line2[4] = col1;
    lcd_line2[5] = phase2[0];
    lcd_line2[6] = phase2[1];
    lcd_line2[7] = phase2[2];
    lcd_line2[8] = col2;
    lcd_line2[9] = '\0';
    
    // Send to LCD (you need to implement these functions)
//     lcd_clear();
//     lcd_goto(0, 0);
//     lcd_puts(lcd_line1);
//     lcd_goto(0, 1);
//     lcd_puts(lcd_line2);
}

int main(void) {
    // Initialize hardware
    setup_hardware();
    setupTimer2();
    setup_SPI();
    setup_PortExpander();
    setup_I2C();
    // lcd_init();  // You need to implement this
    
    // Initialize states
    HAZARD = TRUE;
    state = Hazard;
    hazard_start_time = millis();
    
    // Initialize all lights to YELLOW for hazard
    PRWS.colour = YELLOW;
    PRES.colour = YELLOW;
    PRWT.colour = YELLOW;
    RWS.colour = YELLOW;
    DMS.colour = YELLOW;
    uint32_t lights;
    // Enable interrupts
    sei();
    uint32_t last_state_update = 0;
    uint32_t last_light_update = 0;
    
    while (1) {
        uint32_t now = millis();
        
        // Read sensors
        read_sensors();
        
        // Handle button interrupt
        if (button_int) {
            buttonPressed();
        }
        
        // Update state machine every 100ms
        if ((now - last_state_update) >= 100) {
            State_Manager();
            last_state_update = now;
        }
        
        // Update lights every 10ms
        if ((now - last_light_update) >= 10) {
            lights = get_Lights();
            write_LEDs(lights);
            lights = lights >> 16;
            PORTC = (lights & 0x0f);
            last_light_update = now;
        }
        
        // Update LCD every 100ms
        if ((now - last_lcd_update) >= 100) {
            update_lcd();
            last_lcd_update = now;
            
            // Increment time counter every time period
            static uint32_t last_time_increment = 0;
            if ((now - last_time_increment) >= time_period_ms) {
                time_counter++;
                last_time_increment = now;
            }
        }
        
        // Check for transition out of hazard
        if (HAZARD && !(PIND & (1<<3))) {
            hazard_start_time = now;
        } else if (HAZARD && (PIND & (1<<3)) && ((now - hazard_start_time) >= 3000)) {
            // Exit hazard mode
            HAZARD = FALSE;
            state = Default;
            
            // Set initial state for normal operation
            PRWS.colour = GREEN;
            PRES.colour = GREEN;
            PRWT.colour = RED;
            RWS.colour = RED;
            DMS.colour = RED;
            timing_prws.green_start = now;

            // Reset all phase done flags
            PRWS.phaseDone = TRUE;
            PRES.phaseDone = TRUE;
            PRWT.phaseDone = TRUE;
            RWS.phaseDone = TRUE;
            DMS.phaseDone = TRUE;
        }
    }
    
    return 0;
}