/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor_hw.h 
 *
 *  DESCRIPTION
 *      Header definitions
 *
 *  NOTES
 *
 ******************************************************************************/
#ifndef __GLUCOSE_SENSOR_HW_H__
#define __GLUCOSE_SENSOR_HW_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <timer.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "app_gatt.h"
#include "nvm_access.h"

/*============================================================================*
 *  Public Definitions
 *============================================================================*/
/* All the LED blinking and buzzer code has been put under these compiler flags
 * Disable these flags at the time of current consumption measurement 
 */
#define ENABLE_BUZZER
/*#define ENABLE_LEDBLINK*/

#ifdef ENABLE_BUZZER
/* TIMER for Buzzer */
#define SHORT_BEEP_TIMER_VALUE                   (100* MILLISECOND)
#define LONG_BEEP_TIMER_VALUE                    (500* MILLISECOND)
#define BEEP_GAP_TIMER_VALUE                     (25* MILLISECOND)
#endif /* ENABLE_BUZZER */

#define PIO_DIRECTION_INPUT                      (FALSE)
#define PIO_DIRECTION_OUTPUT                     (TRUE)

#define GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH      (3)
/*============================================================================*
 *  Public Data
 *============================================================================*/

/* Setup PIOs */
#define BUZZER_PIO                               (3)
#define LED_PIO                                  (4)
#define BUTTON_PIO                               (11)

#define PIO_BIT_MASK(pio)                        (0x01UL << (pio))

#ifdef ENABLE_LEDBLINK
#define LED_PWM_INDEX_1                          (1)
#define DULL_LED_ON_TIME_ADV                     (2)
#define DULL_LED_OFF_TIME_ADV                    (20)
#define DULL_LED_HOLD_TIME_ADV                   (10)

#define BRIGHT_LED_OFF_TIME_ADV                  (30)
#define BRIGHT_LED_ON_TIME_ADV                   (10)
#define BRIGHT_LED_HOLD_TIME_ADV                 (10)

#define LED_RAMP_RATE                            (0x33)


#define DULL_LED_ON_TIME_CONN                    (2)
#define DULL_LED_OFF_TIME_CONN                   (20)
#define DULL_LED_HOLD_TIME_CONN                  (70)

#define BRIGHT_LED_OFF_TIME_CONN                 (30)
#define BRIGHT_LED_ON_TIME_CONN                  (10)
#define BRIGHT_LED_HOLD_TIME_CONN                (70)
#endif /* ENABLE_LEDBLINK */

#ifdef ENABLE_BUZZER
/* The index (0-3) of the PWM unit to be configured */
#define BUZZER_PWM_INDEX_0                       (0)

/* PWM parameters for Buzzer */

/* Dull on. off and hold times */
#define DULL_BUZZ_ON_TIME                        (2)    /* 60us */
#define DULL_BUZZ_OFF_TIME                       (15)   /* 450us */
#define DULL_BUZZ_HOLD_TIME                      (0)

/* Bright on, off and hold times */
#define BRIGHT_BUZZ_ON_TIME                      (2)    /* 60us */
#define BRIGHT_BUZZ_OFF_TIME                     (15)   /* 450us */
#define BRIGHT_BUZZ_HOLD_TIME                    (0)    /* 0us */

#define BUZZ_RAMP_RATE                           (0xFF)
#endif /* ENABLE_BUZZER */


/* Extra long button press timer */
#define EXTRA_LONG_BUTTON_PRESS_TIMER            (4*SECOND)
                                     
/*============================================================================*
 *  Public Data Types
 *============================================================================*/


/* data type for different type of buzzer beeps */
typedef enum
{
    beep_off = 0,                   /* No beeps */

    beep_short,                     /* Short beep */

    beep_long,                      /* Long beep */

    beep_twice,                     /* Two beeps */

    beep_thrice                     /* Three beeps */
}buzzer_beep_type;



typedef struct
{
#ifdef ENABLE_BUZZER
    /* Buzzer timer id */
    timer_id                                  buzzer_tid;
#endif /* ENABLE_BUZZER */

    /* Timer for button press */
    timer_id                                  button_press_tid;

#ifdef ENABLE_BUZZER
    /* Variable for storing beep type.*/
    buzzer_beep_type                          beep_type;

    /* Variable for keeping track of beep counts. This variable will be 
     * initialized to 0 on beep start and will incremented for each beep on and 
     * off
     */
    uint16                                    beep_count;
#endif /* ENABLE_BUZZER */
}APP_HW_DATA_T;


typedef enum 
{
    stop_ind = 0,                   /* Stop indications */

    advertising_ind,                /* Advertising  state */

    connected_ind                   /* connected state */

} app_indication;

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

extern APP_HW_DATA_T g_app_hw_data;


/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function initializes the glucose sensor hardware.*/
extern void InitGSHardware(void);

/* This function formulates and adds the glucose measurement data in measurement
 * queue.
 */
extern void FormulateNAddGlucoseMeasData(uint8 num);

/* This function initializes the application hardware data. */
extern void AppHwDataInit(void);

/* This function indicates different application states throught LED glowing. */
extern void SetIndication(app_indication state);

/* This fucntion handles the PIO changes event. */
extern void HandlePIOChangedEvent(uint32 pio_changed);

/* This function sounds the buzzer. */
extern void SoundBuzzer(buzzer_beep_type beep_type);

#endif /*__GLUCOSE_SENSOR_HW_H__*/

