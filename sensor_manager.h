/*
 * File:   sensor_manager.h
 * Author: Traffic Light Controller
 * 
 * Sensor management header
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include "Sensors.h"

// Sensor state structure
typedef struct {
    uint8_t current;
    uint8_t previous;
    uint8_t triggered;
    uint8_t handled;
} sensor_state_t;

// External sensor state
extern sensor_state_t sensors;

// Function prototypes
void update_sensor_states(void);
void mark_sensor_handled(uint8_t sensor_num);
uint8_t sensor_needs_handling(uint8_t sensor_num);
void clear_all_sensors(void);
void mark_phase_sensors_handled(enum STATE phase);

#endif /* SENSOR_MANAGER_H */