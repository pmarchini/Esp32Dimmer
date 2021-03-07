// RobotDyn porting of RBDDimmer library from Arduino framework to esp-idf
//
// author : pmarchini
// mail   : pietro.marchini94@gmail.com 

#ifndef RBDDIMMER_H
#define RBDDIMMER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "freertos/task.h"
#include "math.h"
#include "esp_log.h"

#define ALL_DIMMERS 50

/*ISR debug defines*/
#define ISR_DEBUG_ON 1
#define ISR_DEBUG_OFF 0
/*Activate/Deactivate isr debug*/
#define DEBUG_ISR_DIMMER ISR_DEBUG_OFF
/*If timer is too fast can lead to core 0 panic*/
#define DEBUG_ISR_TIMER ISR_DEBUG_OFF



static const uint8_t powerBuf[] = {
    100, 99, 98, 97, 96, 95, 94, 93, 92, 91,
    90, 89, 88, 87, 86, 85, 84, 83, 82, 81,
    80, 79, 78, 77, 76, 75, 74, 73, 72, 71,
    70, 69, 68, 67, 66, 65, 64, 63, 62, 61,
    60, 59, 58, 57, 56, 55, 54, 53, 52, 51,
    50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
    40, 39, 38, 37, 36, 35, 34, 33, 32, 31,
    30, 29, 28, 27, 26, 25, 24, 23, 22, 21,
    20, 19, 18, 17, 16, 15, 14, 13, 12, 11,
    10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

#define ESP_INTR_FLAG_DEFAULT 0

typedef enum
{
    NORMAL_MODE = 0,
    TOGGLE_MODE = 1
} DIMMER_MODE_typedef;

typedef enum
{
    OFF = false,
    ON = true
} ON_OFF_typedef;

typedef struct dimmer
{
    int current_num;
    int timer_num;
    bool toggle_state;
    int tog_current;
    int steps;
    uint16_t pulse_begin;
    int dimmer_pin;
    int tog_max;
    int tog_min;
    int zc_pin;
} dimmertyp;

dimmertyp *createDimmer(gpio_num_t user_dimmer_pin, gpio_num_t zc_dimmer_pin);
void begin(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE, ON_OFF_typedef ON_OFF, int FREQ);
void setPower(dimmertyp *ptr, int power);
int getPower(dimmertyp *ptr);
void setState(dimmertyp *ptr, ON_OFF_typedef ON_OFF);
bool getState(dimmertyp *ptr);
void changeState(dimmertyp *ptr);
void setMode(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE);
DIMMER_MODE_typedef getMode(dimmertyp *ptr);
void toggleSettings(dimmertyp *ptr, int minValue, int maxValue);
void port_init(dimmertyp *ptr);
void config_timer(int freq);
void ext_int_init(dimmertyp *ptr);

static void IRAM_ATTR isr_ext(void* arg);
static void IRAM_ATTR onTimerISR(void* arg);


#endif
