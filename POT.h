/*
 * File:   POT.h
 * Author: Traffic Light Controller
 * 
 * Potentiometer interface for variable time periods
 */

#ifndef POT_H
#define POT_H

#include <stdint.h>

// Function prototypes
void setup_POT(void);
uint16_t read_POT(void);
uint32_t get_time_period_ms(void);

#endif /* POT_H */