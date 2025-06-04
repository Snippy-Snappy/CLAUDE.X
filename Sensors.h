// Sensors.h - Improved version
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

// Define bool type for XC8
#define bool uint8_t
#define true 1
#define false 0

#define dms 0 
#define prws 1
#define prwt 2 
#define pres 3 
#define rws 4 

// States for the traffic controller
enum STATE {
    Hazard,
    Default,        // Park Road Through
    ParkRdWestTurn,
    RailwayStThrough,
    DamStThrough
};

// Light colors
enum COLOUR {
    OFF,
    RED,
    YELLOW,
    GREEN
};

// On/Off state
enum ON {
    FALSE = 0,
    TRUE = 1
};

// Light structure
typedef struct {
    enum COLOUR colour;
    enum ON on;         // Sensor triggered
    enum ON phaseDone;  // Phase completed
} light;

// Timing variables for each phase
typedef struct {
    uint32_t red_start;
    uint32_t green_start;
    uint32_t yellow_start;
    uint8_t min_periods;
    uint8_t max_periods;
    uint8_t current_periods;
} phase_timing_t;


// External variables
extern enum STATE state;
extern light PRWS, PRES, PRWT, RWS, DMS;
extern phase_timing_t timing_prws, timing_prwt, timing_rws, timing_dms;
extern enum ON HAZARD;
extern volatile uint32_t time_period_ms;  // Time period in milliseconds

// Function prototypes
void Sensor_Manager(int sensor);
void State_Manager(void);
void Colour_Manager(void);
uint32_t get_Lights(void);
void setup_sensors(void);

#define S0      0x01
#define DSG     0x02
#define DSY     0x04
#define DSR     0x08

#define S1      0x10
#define PRWG    0x20
#define PRWY    0x40
#define PRWR    0x80

#define S2      0x01
#define PRTG    0x0200
#define PRTY    0x0400
#define PRTR    0x0800

#define S3      0x10
#define PREG    0x2000
#define PREY    0x4000
#define PRER    0x8000


#define S4      0x01
#define RSG     0x02
#define RSY     0x04
#define RSR     0x08

#endif // SENSORS_H