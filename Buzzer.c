//
// Created by joeyj on 1/06/2025.
//

#include "Buzzer.h"
#include <xc.h>

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
    OCR1A = 2273;           // 440Hz = Middle A
}