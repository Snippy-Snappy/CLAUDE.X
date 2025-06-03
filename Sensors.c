/*
 * File:   Sensors.c
 * Author: Traffic Light Controller
 * 
 * Implements state machine and light control logic
 */

#include "Sensors.h"
#include <avr/io.h>
#include "abs_clock.h"

// Global state variables
enum STATE state = Hazard;
enum ON HAZARD = TRUE;

// Light structures
light PRWS = {YELLOW, FALSE, TRUE};  // Park Road West Straight
light PRES = {YELLOW, FALSE, TRUE};  // Park Road East Straight
light PRWT = {YELLOW, FALSE, TRUE};  // Park Road West Turn
light RWS = {YELLOW, FALSE, TRUE};   // Railway Street
light DMS = {YELLOW, FALSE, TRUE};   // Dam Street



// Timing configurations
phase_timing_t timing_prws = {0, 0, 0, 4, 6, 0};  // Park Road West/East
phase_timing_t timing_prwt = {0, 0, 0, 2, 4, 0};  // Park Road Turn
phase_timing_t timing_rws = {0, 0, 0, 2, 3, 0};   // Railway Street
phase_timing_t timing_dms = {0, 0, 0, 2, 4, 0};   // Dam Street

// Hazard timing
uint32_t hazard_toggle_time = 0;

// Get the combined light states for output
uint32_t get_Lights(void) {
    uint32_t lights = 0;
    uint16_t lights_low = 0;
    uint16_t lights_high = 0;
    
    // Dam Street lights (GPIOA)
    switch (DMS.colour) {
        case RED:    lights_low |= DSR; break;
        case YELLOW: lights_low |= DSY; break;
        case GREEN:  lights_low |= DSG; break;
        default:     break;
    }
    
    // Park Road West lights (GPIOA)
    switch (PRWS.colour) {
        case RED:    lights_low |= PRWR; break;
        case YELLOW: lights_low |= PRWY; break;
        case GREEN:  lights_low |= PRWG; break;
        default:     break;
    }
    
    // Park Road Turn lights (GPIOB)
    switch (PRWT.colour) {
        case RED:    lights_low |= PRTR; break;
        case YELLOW: lights_low |= PRTY; break;
        case GREEN:  lights_low |= PRTG; break;
        default:     break;
    }
    
    // Park Road East lights (GPIOB)
    switch (PRES.colour) {
        case RED:    lights_low |= PRER; break;
        case YELLOW: lights_low |= PREY; break;
        case GREEN:  lights_low |= PREG; break;
        default:     break;
    }
    
    // Railway Street lights (Port C - handled separately)
    switch (RWS.colour) {
        case RED:    lights_high |= RSR; break;
        case YELLOW: lights_high |= RSY; break;
        case GREEN:  lights_high |= RSG; break;
        default:     break;
    }
    lights |= lights_high;
    lights = lights << 16;
    lights |= lights_low;
    return lights;
}

// Handle sensor triggers
void Sensor_Manager(int sensor) {
    switch (sensor) {
        case 0: DMS.on = TRUE; break;   // Dam Street
        case 1: PRWS.on = TRUE; break;  // Park Road West
        case 2: PRWT.on = TRUE; break;  // Park Road West Turn
        case 3: PRES.on = TRUE; break;  // Park Road East
        case 4: RWS.on = TRUE; break;   // Railway Street
        case 6: HAZARD = TRUE; break;   // Hazard
    }
}

// Check if it's time to change light color
static void update_light_timing(light *lt, phase_timing_t *timing, uint32_t now) {
    switch (lt->colour) {
        case GREEN:
            // Check minimum time
            if ((now - timing->green_start) < (timing->min_periods * time_period_ms)) {
                return;
            }
            
            // Check maximum time or if we need to yield
            if ((now - timing->green_start) >= (timing->max_periods * time_period_ms)) {
                lt->colour = YELLOW;
                timing->yellow_start = now;
            } else if (!lt->on && (now - timing->green_start) >= (timing->min_periods * time_period_ms)) {
                // No more demand and minimum time met
                lt->colour = YELLOW;
                timing->yellow_start = now;
            }
            break;
            
        case YELLOW:
            if ((now - timing->yellow_start) >= (2 * time_period_ms)) {
                lt->colour = RED;
                lt->phaseDone = TRUE;
                lt->on = FALSE;  // Clear demand
                timing->last_change = now;
            }
            break;
            
        case RED:
            // Transition to green handled by state manager
            if ((now - timing->last_change) >= (2 * time_period_ms)) {
                lt->phaseDone = FALSE;
            }
            break;
    }
}

// Determine next state based on sensor inputs
static enum STATE get_next_state(void) {
    // Priority order: Dam Street > Railway Street > Park Road Turn
    
    // Check if current phase is done
    uint8_t current_done = FALSE;
    switch (state) {
        case Default:
            current_done = (PRWS.colour != GREEN && PRES.colour != GREEN);
            break;
        case ParkRdWestTurn:
            current_done = (PRWT.phaseDone);
            break;
        case RailwayStThrough:
            current_done = (RWS.phaseDone);
            break;
        case DamStThrough:
            current_done = (DMS.phaseDone);
            break;
    }
    
    if (!current_done) {
        return state;  // Stay in current state
    }
    
    // All lights should be red before transitioning
    if (PRWS.phaseDone && PRES.phaseDone) {
        // Check demands in priority order
        if (DMS.on) {
            return DamStThrough;
        } else if (RWS.on) {
            return RailwayStThrough;
        } else if (PRWT.on) {
            return ParkRdWestTurn;
        }
    }
    
    // Default to Park Road Through
    return Default;
}

// Main state machine
void State_Manager(void) {
    uint32_t now = millis();
    
    if (HAZARD) {
        state = Hazard;
        
        // Flash yellow lights every second
        if ((now - hazard_toggle_time) >= 1000) {
            if (PRWS.colour == YELLOW) {
                PRWS.colour = OFF;
                PRES.colour = OFF;
                PRWT.colour = OFF;
                RWS.colour = OFF;
                DMS.colour = OFF;
            } else {
                PRWS.colour = YELLOW;
                PRES.colour = YELLOW;
                PRWT.colour = YELLOW;
                RWS.colour = YELLOW;
                DMS.colour = YELLOW;
            }
            hazard_toggle_time = now;
        }
        return;
    }
    
    // Normal operation
    enum STATE next_state = get_next_state();
    
    // Handle state transitions
    if (next_state != state && PRWS.phaseDone && PRES.phaseDone) {
        state = next_state;
        
        // Start new phase
        switch (state) {
            case Default:
                PRWS.colour = GREEN;
                PRES.colour = GREEN;
                timing_prws.green_start = now;
                PRWS.phaseDone = FALSE;
                PRES.phaseDone = FALSE;
                break;
                
            case ParkRdWestTurn:
                PRWT.colour = GREEN;
                timing_prwt.green_start = now;
                PRWT.phaseDone = FALSE;
                // Keep Park Road straight green if possible
                if (PRWS.colour == GREEN) {
                    // Keep it green
                } else {
                    PRWS.colour = GREEN;
                    timing_prws.green_start = now;
                    PRWS.phaseDone = FALSE;
                }
                break;
                
            case RailwayStThrough:
                RWS.colour = GREEN;
                timing_rws.green_start = now;
                RWS.phaseDone = FALSE;
                break;
                
            case DamStThrough:
                DMS.colour = GREEN;
                timing_dms.green_start = now;
                DMS.phaseDone = FALSE;
                break;
        }
    }
    
    // Update light timings based on current state
    switch (state) {
        case Default:
            if (~(PRWT.on || RWS.on || DMS.on)){
                timing_prws.max_periods = 0xffff;
            }else {
                timing_prws.max_periods = 6;

            }
                update_light_timing(&PRWS, &timing_prws, now);
                update_light_timing(&PRES, &timing_prws, now);

            // Check if we need to stop one direction
            if ((PRWT.on || RWS.on || DMS.on) && !PRWS.on && PRWS.colour == GREEN) {
                PRWS.colour = YELLOW;
                timing_prws.yellow_start = now;
            }
            if ((PRWT.on || RWS.on || DMS.on) && !PRES.on && PRES.colour == GREEN) {
                PRES.colour = YELLOW;
                timing_prws.yellow_start = now;
            }
            break;
            
        case ParkRdWestTurn:
            update_light_timing(&PRWT, &timing_prwt, now);
            update_light_timing(&PRWS, &timing_prws, now);
            
            // If higher priority demand, start ending this phase
            if ((DMS.on || RWS.on) && PRWT.colour == GREEN && 
                (now - timing_prwt.green_start) >= (timing_prwt.min_periods * time_period_ms)) {
                PRWT.colour = YELLOW;
                timing_prwt.yellow_start = now;
            }
            break;
            
        case RailwayStThrough:
            update_light_timing(&RWS, &timing_rws, now);
            
            // If higher priority demand, start ending this phase
            if (DMS.on && RWS.colour == GREEN && 
                (now - timing_rws.green_start) >= (timing_rws.min_periods * time_period_ms)) {
                RWS.colour = YELLOW;
                timing_rws.yellow_start = now;
            }
            break;
            
        case DamStThrough:
            update_light_timing(&DMS, &timing_dms, now);
            break;
    }
}