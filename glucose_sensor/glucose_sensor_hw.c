/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor_hw.c
 *
 *  DESCRIPTION
 *      This file defines all the function which interact with hardware
 *
 *  NOTES
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <main.h>
#include <pio_ctrlr.h>
#include <pio.h>
#include <timer.h>


/*============================================================================*
 *  Local Header File
 *============================================================================*/
#include "glucose_sensor_hw.h"
#include "glucose_sensor.h"
#include "glucose_sensor_gatt.h"

/*============================================================================*
 *  Private Data
 *============================================================================*/
/* Glucose Sensor application hardware data structure */
APP_HW_DATA_T g_app_hw_data;


/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

#ifdef ENABLE_BUZZER
/* This function handles the buzzer timer expiry. */
static void appBuzzerTimerHandler(timer_id tid);
#endif /* ENABLE_BUZZER*/

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

#ifdef ENABLE_BUZZER

/*----------------------------------------------------------------------------*
 *  NAME
 *      appBuzzerTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to stop the Buzzer at the expiry of 
 *      timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void appBuzzerTimerHandler(timer_id tid)
{
    uint32 beep_timer = SHORT_BEEP_TIMER_VALUE;

    g_app_hw_data.buzzer_tid = TIMER_INVALID;

    switch(g_app_hw_data.beep_type)
    {
        case beep_short: /* FALLTHROUGH */
        case beep_long:
        {
            g_app_hw_data.beep_type = beep_off;

            /* Disable buzzer */
            PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);
        }
        break;
        case beep_twice:
        {
            if(g_app_hw_data.beep_count == 0)
            {
                /* First beep sounded. Start the silent gap*/
                g_app_hw_data.beep_count = 1;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                /* Time gap between two beeps */
                beep_timer = BEEP_GAP_TIMER_VALUE;
            }
            else if(g_app_hw_data.beep_count == 1)
            {
                g_app_hw_data.beep_count = 2;

                /* Enable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

                /* Start beep */
                beep_timer = SHORT_BEEP_TIMER_VALUE;
            }
            else
            {
                /* Two beeps have been sounded. Stop buzzer now*/
                g_app_hw_data.beep_count = 0;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                g_app_hw_data.beep_type = beep_off;
            }
        }
        break;
        case beep_thrice:
        {
            if(g_app_hw_data.beep_count == 0 ||
               g_app_hw_data.beep_count == 2)
            {
                /* First beep sounded. Start the silent gap*/
                g_app_hw_data.beep_count++;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                /* Time gap between two beeps */
                beep_timer = BEEP_GAP_TIMER_VALUE;
            }
            else if(g_app_hw_data.beep_count == 1 ||
                    g_app_hw_data.beep_count == 3)
            {
                g_app_hw_data.beep_count++;

                /* Enable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

                beep_timer = SHORT_BEEP_TIMER_VALUE;
            }
            else
            {
                /* Two beeps have been sounded. Stop buzzer now*/
                g_app_hw_data.beep_count = 0;

                /* Disable buzzer */
                PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);

                g_app_hw_data.beep_type = beep_off;
            }
        }
        break;
        default:
        {
            /* No such beep type */
            ReportPanic(app_panic_unexpected_beep_type);
            /* Though break statement will not be executed after panic but this
             * has been kept to avoid any confusion for default case.
             */
        }
        break;
    }

    if(g_app_hw_data.beep_type != beep_off)
    {
        /* start the timer */
        g_app_hw_data.buzzer_tid = TimerCreate(beep_timer, TRUE, 
                                               appBuzzerTimerHandler);
    }
}

#endif /* ENABLE_BUZZER*/


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/



/*----------------------------------------------------------------------------*
 *  NAME
 *      SoundBuzzer
 *
 *  DESCRIPTION
 *      Function for sounding beeps.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void SoundBuzzer(buzzer_beep_type beep_type)
{
#ifdef ENABLE_BUZZER
    uint32 beep_timer = SHORT_BEEP_TIMER_VALUE;

    PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);
    TimerDelete(g_app_hw_data.buzzer_tid);
    g_app_hw_data.buzzer_tid = TIMER_INVALID;
    g_app_hw_data.beep_count = 0;

    /* Store the beep type in some global variable. It will used on timer expiry
     * to check the type of beeps being sounded.
     */
    g_app_hw_data.beep_type = beep_type;

    switch(g_app_hw_data.beep_type)
    {
        case beep_off:
        {
            /* Don't do anything */
        }
        break;

        case beep_short:
            /* FALLTHROUGH */
        case beep_twice: /* Two short beeps will be sounded */
            /* FALLTHROUGH */
        case beep_thrice: /* Three short beeps will be sounded */
        {
            /* One short beep will be sounded */
            beep_timer = SHORT_BEEP_TIMER_VALUE;
        }
        break;

        case beep_long:
        {
            /* One long beep will be sounded */
            beep_timer = LONG_BEEP_TIMER_VALUE;
        }
        break;

        default:
        {
            /* No such beep type defined */
            ReportPanic(app_panic_unexpected_beep_type);
            /* Though break statement will not be executed after panic but this
             * has been kept to avoid any confusion for default case.
             */
        }
        break;
    }

    if(g_app_hw_data.beep_type != beep_off)
    {
        /* Initialize beep count to zero */
        g_app_hw_data.beep_count = 0;

        /* Enable buzzer */
        PioEnablePWM(BUZZER_PWM_INDEX_0, TRUE);

        TimerDelete(g_app_hw_data.buzzer_tid);
        g_app_hw_data.buzzer_tid = TimerCreate(beep_timer, TRUE, 
                                               appBuzzerTimerHandler);
    }
#endif /* ENABLE_BUZZER */
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      InitGSHardware  -  intialise app hardware
 *
 *  DESCRIPTION
 *      This function is called upon a power reset to initialize the PIOs
 *      and configure their initial states.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void InitGSHardware(void)
{
    /* Don't wakeup on UART RX line */
    SleepWakeOnUartRX(TRUE);

    /* Setup PIOs
     * PIO3 - Buzzer - BUZZER_PIO
     * PIO4 - LED 1 - LED_PIO
     * PIO11 - Button - BUTTON_PIO
     */

    PioSetModes(PIO_BIT_MASK(BUTTON_PIO), pio_mode_user);
    PioSetDir(BUTTON_PIO, PIO_DIRECTION_INPUT); /* input */
    PioSetPullModes(PIO_BIT_MASK(BUTTON_PIO), pio_mode_strong_pull_up); 
    /* Setup button on PIO11 */
    PioSetEventMask(PIO_BIT_MASK(BUTTON_PIO), pio_event_mode_both);
    
#ifdef ENABLE_BUZZER
    PioSetModes(PIO_BIT_MASK(BUZZER_PIO), pio_mode_pwm0);
#endif /* ENABLE_BUZZER */


#ifdef ENABLE_LEDBLINK
    /* PWM is being used for LED glowing.*/
    PioSetModes(PIO_BIT_MASK(LED_PIO), pio_mode_pwm1);
    PioSetDir(LED_PIO, PIO_DIRECTION_OUTPUT);   /* output */
    PioSet(LED_PIO, FALSE);     /* set low */

    /* Advertising parameters are being configured for PWM right now. When
     * application moves to connection state, we change PWM parameters to
     * the ones for Connection
     */
    
    PioConfigPWM(LED_PWM_INDEX_1, pio_pwm_mode_push_pull, DULL_LED_ON_TIME_ADV, 
          DULL_LED_OFF_TIME_ADV, DULL_LED_HOLD_TIME_ADV, BRIGHT_LED_ON_TIME_ADV,
          BRIGHT_LED_OFF_TIME_ADV, BRIGHT_LED_HOLD_TIME_ADV, LED_RAMP_RATE);

    PioEnablePWM(LED_PWM_INDEX_1, FALSE);
#endif /* ENABLE_LEDBLINK */


#ifdef ENABLE_BUZZER
    /* Configure the buzzer on PIO3 */
    PioConfigPWM(BUZZER_PWM_INDEX_0, pio_pwm_mode_push_pull, DULL_BUZZ_ON_TIME,
                 DULL_BUZZ_OFF_TIME, DULL_BUZZ_HOLD_TIME, BRIGHT_BUZZ_ON_TIME,
                 BRIGHT_BUZZ_OFF_TIME, BRIGHT_BUZZ_HOLD_TIME, BUZZ_RAMP_RATE);


    PioEnablePWM(BUZZER_PWM_INDEX_0, FALSE);
#endif /* ENABLE_BUZZER */

    /* Change the I2C pull mode to pull down*/
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      FormulateNAddGlucoseMeasData
 *
 *  DESCRIPTION
 *      This function formulates and adds Glucose measurement data to the
 *      glucose measurement queue.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void FormulateNAddGlucoseMeasData(uint8 num)
{
    uint8 mFlag = 0;
    uint8 cFlag = 0;
    uint8 mData[MAX_LEN_MEAS_OPTIONAL_FIELDS];
    uint8 cData[MAX_LEN_CONTEXT_OPTIONAL_FIELDS];
    uint16 mLen = 0;
    uint16 cLen = 0;
    uint16 random_val = 0;

    /* Fill glucose measurement data */
    mFlag = TIME_OFFSET_PRESENT |
            GLUCOSE_CONC_UNIT_MMOL_PER_LITRE|
            GLUCOSE_CONC_TYPE_SAMPLE_LOCATION_PRESENT |
            SENSOR_STATUS_ANNUNCIATION_PRESENT; /* Unit - mg/dl */

    /* Add time offset */
    mData[mLen ++] = LE8_L(255);
    mData[mLen ++] = LE8_H(255);

    /* Glucose concentration - two Octets - bit 2 of flag tells the unit of
     * glucose concentration. If bit2 is set, It means units will be mol/L and
     * if bit2 is not set units will be in Kg/L
     */
    random_val = (TimeGet16() % 29) + GLUCOSE_MEAS_FASTING_NORMAL_MIN;

    /* We are setting glucose concentation in [70-99]mg/dl range.
     * Since glucose concentration uses SFLOAT format, So exponent here will
     * be -5 (calculated by converting Kg/L to mg/dl.
     */
    random_val |= 0xb000; /* Signed -5 = 0xb(2's complement), exponent gives the
                           * 4 MSBs of concentration.
                           */

    mData[mLen ++] = LE8_L(random_val);
    mData[mLen ++] = LE8_H(random_val);

    /* Type - sample location - one octet */
    mData[mLen ++] = TYPE_CAPILLARY_WHOLE_BLOOD | LOCATION_FINGER;

    /* Sensor Annunciation octet is normally set to Zero as there was no 
     * irregularity while the reading was being taken but just to have better
     * use case coverage, we are setting random error once in
     * GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH measurements.
     */
     if(num == 1)
    {
        uint8 random_error = TimeGet16() % 256;
        mData[mLen ++] = LE8_L(random_error);
    }
    else
    {
        mData[mLen ++] = LE8_L(0);
    }
    mData[mLen ++] = LE8_H(0);

    /* If some PTS test case is running which requires glucose context to be in
     * every record/last record/first record and project keyr file has been 
     * configured accordingly, then following boolean variable will be TRUE and
     * glucose context will be generated for every record.
     * OR
     * Add context information once in every 
     * GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH measurements.
     */
   /* if(g_pts_generate_context_every_record == TRUE || num == 2)*/
    if(TRUE)
    {
        /* Fill glucose context information data */
        mFlag |= CONTEXT_INFORMATION_PRESENT;

        cFlag = EXTENDED_FLAGS_PRESENT |
                CARBOHYDRATE_FIELD_PRESENT |
                MEAL_FIELD_PRESENT |
                TESTER_HEALTH_FIELD_PRESENT |
                EXERCISE_FIELD_PRESENT |
                MEDICATION_FIELD_PRESENT |
                MEDICATION_IN_MILLILITRES |
                HBA1C_FIELD_PRESENT;

        /* Extended flag */
        cData[cLen ++] = 0;

        /* Carbohydrate Id */
        cData[cLen ++] = BREAKFAST;
        /* Carbohydrate value is being randomly chosen in range [0-99]grams
         * Default unit of carbohydrate value is Kg. This range has been chosen
         * keeping PTS testcases in mind.
         */
        random_val = (TimeGet16() % 99);
        /* To convert Kg into grams, exponent will be -3.*/
        random_val |= 0xd000; /* Signed -3 = 0xd(2's complement), exponent gives
                               * the 4 MSBs of concentration.
                               */

        cData[cLen ++] = LE8_L(random_val); /* Range - 0 to 2000 */
        cData[cLen ++] = LE8_H(random_val);

        /* Meal field */
        cData[cLen ++] = AFTER_MEAL;

        /* Tester health field */
        cData[cLen ++] = SELF | NO_HEALTH_ISSUES;

        /* Excercise duration and intensity */
        cData[cLen ++] = LE8_L(3600); /* In seconds - 1 Hour */
        cData[cLen ++] = LE8_H(3600);
        cData[cLen ++] = 40; /* Intensity - 0 to 100 %*/

        /* Medication value is being randomly chosen in range [0-99]
         * millilitres. This range has been chosen
         * keeping PTS testcases in mind.
         */
        random_val = (TimeGet16() % 99);
        /* To convert L into ml , exponent will be -3.*/
        random_val |= 0xd000; /* Signed -3 = 0xd(2's complement), exponent
                               *  gives the 4 MSBs of concentration.
                               */

        /* Medication Id */
        cData[cLen ++] = SHORT_ACTING_INSULIN;
        cData[cLen ++] = LE8_L(random_val); /* Range - 0 to 2000 mg/ml */
        cData[cLen ++] = LE8_H(random_val);

        /* HbA1c */
        cData[cLen ++] = LE8_L(10); /* Range - 0 to 100 % */
        cData[cLen ++] = LE8_H(10);
    }

  /*  AddGlucoseMeasurementToQueue(mFlag, mData, mLen,
                                 cFlag, cData, cLen);*/
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      AppHwDataInit
 *
 *  DESCRIPTION
 *      This function initializes the application hardware data
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void AppHwDataInit(void)
{
    /* Delete the button press timer. */
    TimerDelete(g_app_hw_data.button_press_tid);
    g_app_hw_data.button_press_tid = TIMER_INVALID;

    SetIndication(stop_ind);
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      SetIndication
 *
 *  DESCRIPTION
 *      This function indicated the app state through LED blinking
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void SetIndication(app_indication state)
{
#ifdef ENABLE_LEDBLINK
    if(state == stop_ind)
    {
        /*Stop LED glowing */
        PioEnablePWM(LED_PWM_INDEX_1, FALSE);

        /* Reconfigure LED to pio_mode_user. This reconfiguration has been done
         * because When PWM is disabled, LED pio value remains same as it was at
         * the exact time of disabling. So if LED was on, it may remain ON even
         * after PWM disabling. So it is better to reconfigure it to user mode.
         * It will reconfigured to PWM mode while enabling.
         */
        PioSetModes(PIO_BIT_MASK(LED_PIO), pio_mode_user);
        PioSet(LED_PIO, FALSE);
    }
    else
    {
        if(state == advertising_ind)
        {
            /* Fast Blinking for advertising */
            PioConfigPWM(LED_PWM_INDEX_1, pio_pwm_mode_push_pull, 
                DULL_LED_ON_TIME_ADV, DULL_LED_OFF_TIME_ADV,
                DULL_LED_HOLD_TIME_ADV, BRIGHT_LED_ON_TIME_ADV,
                BRIGHT_LED_OFF_TIME_ADV, BRIGHT_LED_HOLD_TIME_ADV, 
                LED_RAMP_RATE);
        }
        else if(state == connected_ind)
        {
            /* slow blinking for connected state */
            PioConfigPWM(LED_PWM_INDEX_1, pio_pwm_mode_push_pull, 
                DULL_LED_ON_TIME_CONN, DULL_LED_OFF_TIME_CONN, 
                DULL_LED_HOLD_TIME_CONN, BRIGHT_LED_ON_TIME_CONN,
                BRIGHT_LED_OFF_TIME_CONN, BRIGHT_LED_HOLD_TIME_CONN, 
                LED_RAMP_RATE);
        }

        PioSetModes(PIO_BIT_MASK(LED_PIO), pio_mode_pwm1);
        /*Start LED glowing */
        PioEnablePWM(LED_PWM_INDEX_1, TRUE);
        PioSet(LED_PIO, TRUE);
    }
#endif /* ENABLE_LEDBLINK */
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      HandlePIOChangedEvent
 *
 *  DESCRIPTION
 *      This function handles PIO Changed event
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HandlePIOChangedEvent(uint32 pio_changed)
{

    if(pio_changed & PIO_BIT_MASK(BUTTON_PIO))
    {
        /* PIO changed */
        uint32 pios = PioGets();

        if(!(pios & PIO_BIT_MASK(BUTTON_PIO)))
        {
            /* This event comes when a button is pressed */

            /* Start a timer for LONG_BUTTON_PRESS_TIMER seconds. If timer expi-
             * res before we receive a button release event then it was a long -
             * press and if we receive a button release pio changed event, it -
             * means it was a short press.
             */
            TimerDelete(g_app_hw_data.button_press_tid);

            g_app_hw_data.button_press_tid = TimerCreate(
                                           EXTRA_LONG_BUTTON_PRESS_TIMER,
                                           TRUE,
                                           HandleExtraLongButtonPress);
        }
        else
        {
            /* This event comes when a button is released */
            if(g_app_hw_data.button_press_tid != TIMER_INVALID)
            {
                /* Timer was already running. This means it was a short button 
                 * press.
                 */
                TimerDelete(g_app_hw_data.button_press_tid);
                g_app_hw_data.button_press_tid = TIMER_INVALID;

                HandleShortButtonPress();
            }
        }
    }

}


