/*
 * File:   Sensors.c
 * Author: Traffic Light Controller
 * 
 * Implements state machine and light control logic
 */

#include "Sensors.h"
#include <avr/io.h>
#include "abs_clock.h"
#include "sensor_manager.h"
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


// Check if it's time to change light color
static void update_light_timing(light *lt, phase_timing_t *timing, uint32_t now) {
    switch (lt->colour) {
        case GREEN:
            // Track elapsed time periods
            timing->current_periods = (now - timing->green_start) / time_period_ms;
            
            // Check minimum time
            if (timing->current_periods < timing->min_periods) {
                return;  // Still in minimum time
            }
            // Check maximum time
            if (timing->current_periods >= timing->max_periods) {
                lt->colour = YELLOW;
                timing->yellow_start = now;
                return;
            }

            // Between min and max - check if we should yield
            if (!lt->on) {
                // No more demand for this light
                lt->colour = YELLOW;
                timing->yellow_start = now;
            }
            break;
            
        case YELLOW:
            if ((now - timing->yellow_start) >= (2 * time_period_ms)) {
                lt->colour = RED;
                lt->phaseDone = TRUE;
                timing->red_start = now;
            }
            break;
            
        case RED:
            // RED state is maintained until state machine changes it
            break;
            
        case OFF:
            // OFF is only used in hazard mode
            break;
    }
}

// Determine next state based on sensor inputs
static enum STATE get_next_state(void) {
     //Don't change state while any light is yellow
    if (PRWS.colour == YELLOW || PRES.colour == YELLOW || 
        PRWT.colour == YELLOW || RWS.colour == YELLOW || 
        DMS.colour == YELLOW) {
        return state;
    }
    
    // Check if current phase is truly done (all red with 2 second delay)
    uint8_t all_red = TRUE;
    uint32_t now = millis();
    
    switch (state) {
        case Default:
            if (PRWS.colour != RED || PRES.colour != RED) {
                all_red = FALSE;
            } else if ((now - timing_prws.red_start) < (2 * time_period_ms)) {
                all_red = FALSE;
            }
            break;
            
        case ParkRdWestTurn:
            if (PRWT.colour != RED) {
                all_red = FALSE;
            } else if ((now - timing_prwt.red_start) < (2 * time_period_ms)) {
                all_red = FALSE;
            }
            break;
            
        case RailwayStThrough:
            if (RWS.colour != RED) {
                all_red = FALSE;
            } else if ((now - timing_rws.red_start) < (2 * time_period_ms)) {
                all_red = FALSE;
            }
            break;
            
        case DamStThrough:
            if (DMS.colour != RED) {
                all_red = FALSE;
            } else if ((now - timing_dms.red_start) < (2 * time_period_ms)) {
                all_red = FALSE;
            }
            break;
    }
    
    if (!all_red) {
        return state;  // Not ready to transition
    }
    
    // Priority order: Current phase completion -> Dam Street -> Railway -> Turn -> Default
    
    // If we're not in default and no other demands, go to default
    if (state != Default && !sensor_needs_handling(dms) && !sensor_needs_handling(rws) && !sensor_needs_handling(prwt)) {
        return Default;
    }
    
    // Check demands in priority order
    if (sensor_needs_handling(dms) && state != DamStThrough) {
        return DamStThrough;
    } else if (sensor_needs_handling(rws) && state != RailwayStThrough) {
        return RailwayStThrough;
    } else if (sensor_needs_handling(prwt) && state != ParkRdWestTurn) {
        return ParkRdWestTurn;
    } else if (state != Default) {
        // No demands, return to default
        return Default;
    }
    
    // Stay in current state
    return state;
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
                stop_hazard_buzzer();
            } else {
                PRWS.colour = YELLOW;
                PRES.colour = YELLOW;
                PRWT.colour = YELLOW;
                RWS.colour = YELLOW;
                DMS.colour = YELLOW;
                start_hazard_buzzer();
            }
            hazard_toggle_time = now;
        }
        return;
    }
    
    // Normal operation
    enum STATE next_state = get_next_state();

    // Handle state transitions
    if (next_state != state) {
        // Mark sensors as handled for new phase
        enum STATE old_state = state;
        state = next_state;

        

        
        // Start new phase lights
        switch (state) {
            case Default:
                PRWS.colour = GREEN;
                PRES.colour = GREEN;
                timing_prws.green_start = now;
                timing_prws.current_periods = 0;
                PRWS.phaseDone = FALSE;
                PRES.phaseDone = FALSE;
                break;
                
            case ParkRdWestTurn:
                PRWT.colour = GREEN;
                timing_prwt.green_start = now;
                timing_prwt.current_periods = 0;
                PRWT.phaseDone = FALSE;
                
                // Allow Park Road straight to continue if coming from Default
                if (old_state == Default && PRWS.colour == GREEN) {
                    // Keep PRWS green
                } else {
                    PRWS.colour = GREEN;
                    PRWS.phaseDone = FALSE;
                }
                break;
                
            case RailwayStThrough:
                RWS.colour = GREEN;
                timing_rws.green_start = now;
                timing_rws.current_periods = 0;
                RWS.phaseDone = FALSE;
                break;
                
            case DamStThrough:
                DMS.colour = GREEN;
                timing_dms.green_start = now;
                timing_dms.current_periods = 0;
                DMS.phaseDone = FALSE;
                break;
        }
    }
    
    // Update light timings based on current state
    switch (state) {
        case Default:
            // Unlimited time if no other demands
            if (!sensor_needs_handling(prwt) && !sensor_needs_handling(rws) && !sensor_needs_handling(dms)) {
                break;  
            }             
                update_light_timing(&PRWS, &timing_prws, now);
                update_light_timing(&PRES, &timing_prws, now);
                
            // Check if we need to yield to higher priority
            if ((sensor_needs_handling(rws) || sensor_needs_handling(dms)) && 
                (now - timing_prws.green_start) >= (timing_prws.min_periods * time_period_ms)) {

                if (PRWS.colour == GREEN ) {
                    PRWS.colour = YELLOW;
                    timing_prws.yellow_start = now;
                }
                if (PRES.colour == GREEN) {
                    PRES.colour = YELLOW;
                    timing_prws.yellow_start = now;
                }
            }
            break;
            
        case ParkRdWestTurn:
            update_light_timing(&PRWT, &timing_prwt, now);

            // Only update PRWS timing if it's not GREEN (don't disturb it if it's already green)
            if (PRWS.colour != GREEN) {
                update_light_timing(&PRWS, &timing_prws, now);
            }

            // Yield to higher priority (Dam or Railway)
            if ((sensor_needs_handling(rws) || sensor_needs_handling(dms)) && PRWT.colour == GREEN &&
                    (now - timing_prwt.green_start) >= (timing_prwt.min_periods * time_period_ms)) {
                PRWT.colour = YELLOW;
                timing_prwt.yellow_start = now;
            }
            break;
            
        case RailwayStThrough:
            update_light_timing(&RWS, &timing_rws, now);
            
            // Yield to Dam Street (highest priority)
            if (sensor_needs_handling(dms) && RWS.colour == GREEN && 
                (now - timing_rws.green_start) >= (timing_rws.min_periods * time_period_ms)) {
                RWS.colour = YELLOW;
                timing_rws.yellow_start = now;
            }
            break;
            
        case DamStThrough:
            update_light_timing(&DMS, &timing_dms, now);
            // Dam Street has highest priority, no need to yield
            break;
    }
}