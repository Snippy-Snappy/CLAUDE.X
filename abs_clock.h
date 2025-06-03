//
// Created by joeyj on 7/05/2025.
//

#ifndef ABS_CLOCK_H
#define ABS_CLOCK_H
#include <xc.h> // include processor files - each processor file is guarded.  
extern uint32_t   clock_count;
uint32_t millis();
void setupTimer2();
#endif //ABS_CLOCK_H
