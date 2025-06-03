//
// Created by joeyj on 7/05/2025.
//

#include "abs_clock.h"
#include <avr/interrupt.h>
#include <avr/io.h>
uint32_t   clock_count = 0;

ISR(TIMER2_COMPA_vect) {
    clock_count++;
}

uint32_t millis() {
    /*
     * Return the current clock_count value.
     * We temporarily disable interrupts to ensure the clock_count value
     * doesn't change while we are reading them.
     * 
     * We then restore the original SREG, which contains the I flag.
     */
    register uint32_t count;
    register char cSREG;
    
    cSREG = SREG;
    cli();
    count = clock_count;
    SREG = cSREG;
    return count;
}

void setupTimer2() {
    /*
     * Timer 2 is setup as a millisecond interrupting timer.
     * This generates a regular millisecond interrupt,
     * which we can use internally to create a counter.
     * 
     * This counter is the basis of all internal timing.
     * 
     * The main clock is 16MHz, we need a 1kHz interrupt, so
     * the counter needs to count 16000 clock pulses and then interrupt.
     * 
     * The counter can count to 256, with prescalers of 8, 32, 64, 128, 256 or 1024
     * 
     * Using a prescaler of 128 we need a count of 125.
     * Resulting in 124 being the OCR2A number
     * We want the prescaler to be as large as possible,
     * as it uses less power then the main counter.
     */
    TCCR2B = 0;             // turn off counter to configure
    TCNT2 = 0;              // set current count to zero
    OCR2A = 124;            // 125 * 128 * 1000 = 16000000
    TIFR2 = 0b00000111;     // clear all existing interrupts
    TIMSK2 = 0b00000010;    // enable interrupt on OCRA
    ASSR = 0;               // no async counting
    TCCR2A = 0b00000010;    // No I/O, mode = 2 (CTC)
    TCCR2B = 0b00000101;    // clock/128, mode = 2 (CTC), start counter
}
