/*
 * File:   sensor_manager.c
 * Author: Traffic Light Controller
 * 
 * Enhanced sensor management with debouncing
 */

#include <xc.h>
#include <stdint.h>
#include "Sensors.h"
#include "SPI.h"
#include "abs_clock.h"
#include "sensor_manager.h"

// Sensor debounce time in milliseconds
#define DEBOUNCE_TIME_MS 50

// Sensor state tracking
sensor_state_t sensors = {0, 0, 0, 0};

// Debounce tracking

typedef struct {
    uint8_t state;
    uint32_t last_change;
} debounce_t;

static debounce_t sensor_debounce[6] = {
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
};

// Check if a sensor is currently pressed (with debouncing)

static uint8_t is_sensor_pressed(uint8_t sensor_num) {
    uint8_t current_state = 0;

    switch (sensor_num) {
        case 0: // S0 - Dam Street (Port A, bit 0)
            current_state = !(SPI_Read_Command(0x12) & S0);
            break;
        case 1: // S1 - Park Road West (Port A, bit 4)
            current_state = !(SPI_Read_Command(0x12) & S1);
            break;
        case 2: // S2 - Park Road West Turn (Port B, bit 0)
            current_state = !(SPI_Read_Command(0x13) & S2);
            break;
        case 3: // S3 - Park Road East (Port B, bit 4)
            current_state = !(SPI_Read_Command(0x13) & S3);
            break;
        case 4: // S4 - Railway Street (PB0)
            current_state = !(PINB & (1 << 0));
            break;
        case 5: // S5 - Bus sensor (PD6) - for extension
            current_state = !(PIND & (1 << 6));
            break;
    }

    return current_state;
}

// Update sensor states with debouncing

void update_sensor_states(void) {
    uint32_t now = millis();

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t current = is_sensor_pressed(i);

        // Check if state has changed
        if (current != sensor_debounce[i].state) {
            // State changed, check if debounce time has passed
            if ((now - sensor_debounce[i].last_change) >= DEBOUNCE_TIME_MS) {
                sensor_debounce[i].state = current;
                sensor_debounce[i].last_change = now;
                if (current) {

                    switch (i) {
                        case 0: // Dam Street
                            DMS.on = TRUE;
                            break;
                        case 1: // Park Road West
                            PRWS.on = TRUE;
                            break;
                        case 2: // Park Road West Turn
                            PRWT.on = TRUE;
                            break;
                        case 3: // Park Road East
                            PRES.on = TRUE;
                            break;
                        case 4: // Railway Street
                            RWS.on = TRUE;
                            break;
                    }


                    // Update triggered state on rising edge
                    if (!(sensors.handled & (1 << i))) {
                        sensors.triggered |= (1 << i);
                    }
                }
            }

        } else {
            // State stable, reset debounce timer
            sensor_debounce[i].last_change = now;

            // Keep triggered state active while button is held


            // Clear states when button is released
            if (!current) {
                switch (i) {
                    case 0: DMS.on = FALSE;
                        break;
                    case 1: PRWS.on = FALSE;
                        break;
                    case 2: PRWT.on = FALSE;
                        break;
                    case 3: PRES.on = FALSE;
                        break;
                    case 4: RWS.on = FALSE;
                        break;
                }
                if (!sensor_needs_handling(i)) {
                    sensors.triggered &= ~(1 << i);
                }
            }
        }
    }
}
    // Mark a sensor as handled

    void mark_sensor_handled(uint8_t sensor_num) {
        if (sensor_num < 6) {
            sensors.handled |= (1 << sensor_num);

            // Clear triggered state once handled
            if (!(sensor_debounce[sensor_num].state)) {
                sensors.triggered &= ~(1 << sensor_num);
            }
        }
    }

    // Check if a sensor needs handling

    uint8_t sensor_needs_handling(uint8_t sensor_num) {
        if (sensor_num >= 6) return 0;

        return (sensors.triggered & (1 << sensor_num)) &&
                !(sensors.handled & (1 << sensor_num));
    }

    // Clear all sensor states

    void clear_all_sensors(void) {
        sensors.triggered = 0;
        sensors.handled = 0;
        sensors.current = 0;
        sensors.previous = 0;
    }



    // Mark sensors as handled when their phase starts

    void mark_phase_sensors_handled(enum STATE phase) {
        switch (phase) {
            case Default:
                mark_sensor_handled(1); // S1 - Park Road West
                mark_sensor_handled(3); // S3 - Park Road East
                break;
            case ParkRdWestTurn:
                mark_sensor_handled(2); // S2 - Park Road West Turn
                break;
            case RailwayStThrough:
                mark_sensor_handled(4); // S4 - Railway Street
                break;
            case DamStThrough:
                mark_sensor_handled(0); // S0 - Dam Street
                break;
            case Hazard:
                // Don't clear sensors in hazard mode
                break;
        }
    }