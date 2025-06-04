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
void setupTimer1() {
    /*
     * we want to send a square wave out OC1A (Port B Pin 1)
     * We have a 16MHz crystal, so the main clock is 16MHz.
     * We want to generate tones about the 1kHz range.
     *
     * We will initially divide the clock by 8 (CS12/CS11/CS10 = 010)
     * which gives us a 2MHz clock. The timer divides this signal by
     * up to 65536 which gives us the final range of frequencies: 15Hz to 1MHz
     * which covers the range we desire.
     */
    DDRB |= _BV(1);         // set PORT B Pin 1 as an output
    PORTB &= ~_BV(1);       // set output to low
    /*
     * We will toggle Port B Pin 1 on timer expiry, clock /8, WGM=4 (CTC mode)
     */
    TCCR1A = 0b01000000;    // COM1A0 = 1
    TCCR1B = 0b00001000;    // noise = 0, clock is currently stopped.
    TCCR1C = 0b00000000;    // no force output
    /*
     * We set the frequency on OCR1A. When the counter matches this value the
     * output pin will toggle (1->0, 0->1). In this example we want the note
     * frequency to be 440Hz ('A'). This means the output has to toggle at
     * 880Hz. The clock is 2000000Hz, so 2000000/880 = 2273.
     *
     * We can do the same calculation for whatever frequency we want.
     */
}