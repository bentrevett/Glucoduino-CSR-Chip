/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor.c
 *
 *  DESCRIPTION
 *      This file defines a simple Glucose Sensor application.
 *
 * NOTES
 *
 ******************************************************************************/
/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <string.h>
#include <main.h>
#include <pio_ctrlr.h>
#include <pio.h>
#include <gatt.h>
#include <gatt_prim.h>
#include <ls_app_if.h>
#include <timer.h>
#include <security.h>



/*============================================================================*
 *  Local Header File
 *============================================================================*/
#include "glucose_sensor.h"
#include "glucose_service.h"
#include "gap_service.h"
#include "app_gatt.h"
#include "glucose_sensor_gatt.h"
#include "app_gatt_db.h"
#include "glucose_sensor_hw.h"
#include "battery_service.h"
#include "nvm_access.h"
#include "gap_conn_params.h"
#include "uartio.h"
#include "byte_queue.h"


/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
/* This function initializes the glucose sensor data. */
static void gsDataInit(void);

/* This function reads the persistent store. */
static void readPersistentStore(void);

#ifndef NO_IDLE_TIMEOUT
/* This function handles the idle timer expiry. */
static void gsIdleTimerHandler(timer_id tid);
#endif /*NO_IDLE_TIMEOUT */

/* This function is called to request remote master to update the connection 
 * parameters.
 */
static void requestConnParamUpdate(timer_id tid);

/* This function handles the signal LM_EV_CONNECTION_COMPLETE */
static void handleSignalLmEvConnectionComplete(
                                     LM_EV_CONNECTION_COMPLETE_T *p_event_data);

/* This function handles the connection confirmation from firmware. */
static void handleSignalGattConnectCFM(GATT_CONNECT_CFM_T *p_event_data);

/* This function handles the cancel connect confirmation signal from firmware.*/
static void handleSignalGattCancelConnectCFM(void);

/* This function handles the access indication received from firmware. */
static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *p_event_data);

/* This function handles the disconnect complete signal from firmware. */
static void handleSignalLmDisconnectComplete(
                HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data);

/* This function handles the encryption change signal. */
static void handleSignalLMEncryptionChange(LM_EVENT_T *p_event_data);

/* This function handles the Diversifier approve request signal from firmware.*/
static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data);

/* This function handles the SM keys indication received. */
static void handleSignalSmKeysInd(SM_KEYS_IND_T *p_event_data);

/* This function handles the signal SM_PAIRING_AUTH_IND */
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data);

/* This function handles the SM pairing complete indication. */
static void handleSignalSmSimplePairingCompleteInd(
                            SM_SIMPLE_PAIRING_COMPLETE_IND_T *p_event_data);

/* This function handles the connection update confirmation signal received for 
 * the request made by application.
 */
static void handleSignalLsConnUpdateSignalCfm(
                                LS_CONNECTION_PARAM_UPDATE_CFM_T *p_event_data);

/* This function handles the event LM_EV_CONNECTION_UPDATE */
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data);

/* This function handles the connection parameter update indication received. */
static void handleSignalLsConnParamUpdateInd(
                    LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data);

/* This function handles the bonding chance timer expiry. */
static void handleBondingChanceTimerExpiry(timer_id tid);

/* This function gets called when application exits the app_init state and
 * initializes application data. 
 */
static void appInitExit(void);

/*============================================================================*
 *  Private Definitions
 *============================================================================*/


/******** TIMERS ********/

/* Maximum number of timers 
 * Two timers have been kept for normal application operation,
 * one timer has been kept for buzzer sounding.
 * One timer will get used for PTS timer which we run to maintain 1 second gap
 * between two glucose measurement sending over the air.
 * This timer will get used only when PTS is running those test cases which
 * require application to keep sending glucose measurements for a long time 
 */
#define MAX_APP_TIMERS                           (5)

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Declare space for application timers. */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_APP_TIMERS];


/*============================================================================*
 *  Public Data
 *============================================================================*/

/* Glucose sensor application data structure */
APP_DATA_T g_gs_data;

/* PTS test case - 
 *      TC_CN_BV_06_C
 *      TC_CN_BV_07_C
 *      TC_CN_BV_08_C
 *      TC_CN_BV_09_C
 *      TC_CN_BV_10_C
 *      TC_CN_BV_13_C
 *
 * These test cases expect last(most recent) generated glucose
 * record to contain context information but our current implementation 
 * generates context once in GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH records.
 * If we enable this flag, it will generate context in every record.
 *
 * Enabling this flag will be controlled through user CS keys
 */
bool g_pts_generate_context_every_record = FALSE;

/* PTS Test case : Following PTS testcases were failing because these test cases
 * want glucose sensor to keep sending glucose measurement notifications for a 
 * few seconds but we don't have much space for storing glucose measurements. So
 * just to increase the time of glucose measurement transmission, we will send 
 * notifications after a gap of 1 second 
 * Enabling this flag will introduce one sec gap between two glucose measuremen
 * notifcations.
 *
 * Enabling this flag will be controlled through user CS keys
 *
 * Following PTS testcases require this flag to be enabled
 *      TC_SPA_BV_01_C
 *      TC_SPE_BI_07_C
 */
bool g_pts_abort_test = FALSE;

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      gsDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialize glucose sensor application data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void gsDataInit(void)
{
    TimerDelete(g_gs_data.app_tid);
    g_gs_data.app_tid = TIMER_INVALID;
    TimerDelete(g_gs_data.conn_param_update_tid);
    g_gs_data.conn_param_update_tid = TIMER_INVALID;

    /* Delete the bonding chance timer */
    TimerDelete(g_gs_data.bonding_reattempt_tid);
    g_gs_data.bonding_reattempt_tid = TIMER_INVALID;

    g_gs_data.st_ucid = GATT_INVALID_UCID;

    g_gs_data.encrypt_enabled = FALSE;

    g_gs_data.pairing_remove_button_pressed = FALSE;

    g_gs_data.advert_timer_value = 0;

    /* Reset the connection parameter variables. */
    g_gs_data.conn_interval = 0;
    g_gs_data.conn_latency = 0;
    g_gs_data.conn_timeout = 0;
    

    AppHwDataInit();

    /* Battery service data initialization */
    BatteryDataInit();

    /* Glucose service data initialization */
    GlucoseDataInit();

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      readPersistentStore
 *
 *  DESCRIPTION
 *      This function is used to initialize and read NVM data
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void readPersistentStore(void)
{
    /* NVM offset for supported services */
    uint16 nvm_offset = NVM_MAX_APP_MEMORY_WORDS;
    uint16 nvm_sanity = 0xffff;

    /* Read persistent storage to know if the device was last bonded 
     * to another device 
     */

    /* If the device was bonded, trigger fast undirected advertisements by
     * setting the white list for bonded host. If the device was not bonded,
     * trigger undirected advertisements for any host to connect.
     */

    Nvm_Read(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

    if(nvm_sanity == NVM_SANITY_MAGIC)
    {
        /* Read bonded flag from NVM */
        Nvm_Read((uint16 *)&g_gs_data.bonded,sizeof(g_gs_data.bonded),
                    NVM_OFFSET_BONDED_FLAG);

        if(g_gs_data.bonded)
        {

            /* Bonded host typed BD address will only be stored if bonded flag
             * is set to TRUE. Read last bonded device address.
             */
            Nvm_Read((uint16 *)&g_gs_data.bonded_bd_addr, 
                       sizeof(TYPED_BD_ADDR_T),
                       NVM_OFFSET_BONDED_ADDR);
        }

        else /* Case when we have only written NVM_SANITY_MAGIC to NVM but 
              * didn't get bonded to any host in the last powered session
              */
        {
            g_gs_data.bonded = FALSE;
        }

        /* Read the diversifier associated with the presently bonded/last 
         * bonded device.
         */
        Nvm_Read(&g_gs_data.diversifier, 
                 sizeof(g_gs_data.diversifier),
                 NVM_OFFSET_SM_DIV);

        /* If device is bonded and bonded address is resolvable then read the 
         * bonded device's IRK
         */
        if(g_gs_data.bonded && 
                GattIsAddressResolvableRandom(&g_gs_data.bonded_bd_addr))
        {
            Nvm_Read((uint16 *)g_gs_data.central_device_irk.irk, 
                                MAX_WORDS_IRK,
                                NVM_OFFSET_SM_IRK);
        }

        /* Read device name and length from NVM */
        GapReadDataFromNVM(&nvm_offset);

        /* Read glucose service data from NVM if the devices are bonded and  
         * update the offset with the number of word of NVM required by 
         * this service
         */
        GlucoseReadDataFromNVM(g_gs_data.bonded, &nvm_offset);

        /* Read battery service data from NVM if the devices are bonded and  
         * update the offset with the number of word of NVM required by 
         * this service
         */
        BatteryReadDataFromNVM(g_gs_data.bonded, &nvm_offset);

    }
    else /* NVM sanity check failed means either the device is being brought up 
          * for the first time or memory has got corrupted in which case 
          * discard the data and start fresh.
          */
    {

        nvm_sanity = NVM_SANITY_MAGIC;

        /* Write NVM Sanity word to the NVM */
        Nvm_Write(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

        /* The device will not be bonded as it is coming up for the first time
         */
        g_gs_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16 *)&g_gs_data.bonded, sizeof(g_gs_data.bonded), 
                             NVM_OFFSET_BONDED_FLAG);

        /* When the application is coming up for the first time after flashing 
         * the image to it, it will not have bonded to any device. So, no LTK 
         * will be associated with it. Hence, set the diversifier to 0.
         */
        g_gs_data.diversifier = 0;

        /* Write the same to NVM. */
        Nvm_Write(&g_gs_data.diversifier, 
                  sizeof(g_gs_data.diversifier),
                  NVM_OFFSET_SM_DIV);

        /* Write device name and length to NVM for the first time. */
        GapInitWriteDataToNVM(&nvm_offset);

        /* Device is being brought up first time. Initialize sequence number
         * and store it to NVM
         */
        GlucoseSeqNumInit(nvm_offset);

        /* Since Device is being brought up first time. So it won't be in
         * bonded state. Following function call will only initialize the nvm 
         * offset of glucose service.and battery service
         */
        GlucoseReadDataFromNVM(g_gs_data.bonded, &nvm_offset);

        BatteryReadDataFromNVM(g_gs_data.bonded, &nvm_offset);

    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      gsIdleTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to handle IDLE timer expiry in connected states.
 *      At the expiry of this timer, application shall disconnect with the 
 *      host.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
#ifndef NO_IDLE_TIMEOUT
static void gsIdleTimerHandler(timer_id tid)
{
    /* Trigger Disconnect and move to CON_DISCONNECTING state */

    if((tid == g_gs_data.app_tid) &&
        (g_gs_data.state == app_connected_not_subscribed ||
                g_gs_data.state == app_connected_and_subscribed))
    {
        g_gs_data.app_tid = TIMER_INVALID;

        SetAppState(app_disconnecting);

    } /* Else ignore the timer */

}
#endif  /*NO_IDLE_TIMEOUT*/


/*----------------------------------------------------------------------------*
 *  NAME
 *      requestConnParamUpdate
 *
 *  DESCRIPTION
 *      This function is used to send L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST
 *      to the remote device when an earlier sent request had failed.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void requestConnParamUpdate(timer_id tid)
{
    ble_con_params app_pref_conn_param;

    if(g_gs_data.conn_param_update_tid == tid)
    {
        g_gs_data.conn_param_update_tid= TIMER_INVALID;
        app_pref_conn_param.con_max_interval = PREFERRED_MAX_CON_INTERVAL;
        app_pref_conn_param.con_min_interval = PREFERRED_MIN_CON_INTERVAL;
        app_pref_conn_param.con_slave_latency = PREFERRED_SLAVE_LATENCY;
        app_pref_conn_param.con_super_timeout = PREFERRED_SUPERVISION_TIMEOUT;

        if(LsConnectionParamUpdateReq(&(g_gs_data.con_bd_addr), 
                                     &app_pref_conn_param) != ls_err_none)
        {
            /* Connection parameter update request should not have failed.
             * Report panic 
             */
            ReportPanic(app_panic_con_param_update);
        }
        g_gs_data.num_conn_update_req++;
    }
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      handleSignalLmEvConnectionComplete
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_COMPLETE.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLmEvConnectionComplete(
                                     LM_EV_CONNECTION_COMPLETE_T *p_event_data)
{
    /* Store the connection parameters. */
    g_gs_data.conn_interval = p_event_data->data.conn_interval;
    g_gs_data.conn_latency = p_event_data->data.conn_latency;
    g_gs_data.conn_timeout = p_event_data->data.supervision_timeout;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattConnectCFM
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CONNECT_CFM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattConnectCFM(GATT_CONNECT_CFM_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_fast_advertising: /* FALLTHROUGH */
        case app_slow_advertising:
        {
            if(p_event_data->result == sys_status_success)
            {

                g_gs_data.con_bd_addr = p_event_data->bd_addr;

                /* Store received UCID */
                g_gs_data.st_ucid = p_event_data->cid;

                SetAppState(app_connected_not_subscribed);

                if(g_gs_data.bonded && 
                    GattIsAddressResolvableRandom(&g_gs_data.bonded_bd_addr) &&
                    (SMPrivacyMatchAddress(&g_gs_data.con_bd_addr,
                                           g_gs_data.central_device_irk.irk,
                                           MAX_NUMBER_IRK_STORED, 
                                           MAX_WORDS_IRK) < 0))
                {
                    /* Application was bonded to a remote device with
                     * resolvable random address and application has failed to
                     * resolve the remote device address to which we just
                     * connected So disconnect and start advertising again
                     */

                    /* Disconnect if we are connected */
                    SetAppState(app_disconnecting);

                }
                else
                {
                    /*Cancel application and start IDLE timer */
                    ResetIdleTimer();

                    /* Initiate slave security request if the remote host 
                     * supports security feature. This is added for this device 
                     * to work against legacy hosts that don't support security
                     */

                    /* Security supported by the remote host */
                    if(!GattIsAddressResolvableRandom(&g_gs_data.con_bd_addr))
                    {
                        SMRequestSecurityLevel(&g_gs_data.con_bd_addr);
                    }

                }

            }
            else
            {

                /* Connection failed. Restart advertising. */

                /* Reset the application data */
                gsDataInit();

                /* Start fast advertisements. */
                GattTriggerFastAdverts();

                /* Indicate advertising to user */
                SetIndication(advertising_ind);

                /* Indicate advertising mode by sounding two beeps */
                SoundBuzzer(beep_twice);
            }
        }
        break;
        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCancelConnectCFM
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CANCEL_CONNECT_CFM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattCancelConnectCFM(void)
{
    if(g_gs_data.pairing_remove_button_pressed)
    {
        /* Case when user performed an extra long button press for removing
         * pairing.
         */
        g_gs_data.pairing_remove_button_pressed = FALSE;

        /* Reset and clear the whitelist */
        LsResetWhiteList();

        /* Trigger fast advertisements */
        if(g_gs_data.state == app_fast_advertising)
        {
            GattTriggerFastAdverts();
        }
        else
        {
            SetAppState(app_fast_advertising);
        }
    }
    else
    {
        /* Handle as per state */
        switch(g_gs_data.state)
        {
            case app_fast_advertising:
            {
                SetAppState(app_slow_advertising);
            }
            break;
            case app_slow_advertising:
            {
                SetAppState(app_idle);
            }
            break;
            default:
            {
                /* Control should never come here */
                ReportPanic(app_panic_invalid_state);
            }
            break;
        }
    }
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAccessInd
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ACCESS_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *p_event_data)
{
    uint16 client_config;
    /* Received when host tries to access a characteristic value or desc for 
     * which flag FLAG_IRQ was set in application att database.
     */

    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            GattHandleAccessInd(p_event_data);

            /*Setting the state machine */
            if((p_event_data->flags & ATT_ACCESS_WRITE) &&
                (p_event_data->handle == 
                            HANDLE_GLUCOSE_MEASUREMENT_CLIENT_CONFIG)
                )
            {
                client_config = BufReadUint16(
                                   &((GATT_ACCESS_IND_T *)p_event_data)->value);
                if(client_config == gatt_client_config_notification)
                {
                    SetAppState(app_connected_and_subscribed);
                }
                else if (client_config == gatt_client_config_none)
                {
                    SetAppState(app_connected_not_subscribed);
                }
            }
        }
        break;
        default :
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
    
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmDisconnectComplete
 *
 *  DESCRIPTION
 *      This function handles LM Disconnect Complete event which is received
 *      at the completion of disconnect procedure triggered either by the 
 *      device or remote host or because of link loss 
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalLmDisconnectComplete(
                HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data)
{

    /* Delete the bonding chance timer */
    TimerDelete(g_gs_data.bonding_reattempt_tid);
    g_gs_data.bonding_reattempt_tid = TIMER_INVALID;

    /* Since there is no connection anymore, reset the connection parameter 
     * variables. 
     */
    g_gs_data.conn_interval = 0;
    g_gs_data.conn_latency = 0;
    g_gs_data.conn_timeout = 0;

    /* LM_EV_DISCONNECT_COMPLETE event can have following disconnect 
     * reasons:
     *
     * HCI_ERROR_CONN_TIMEOUT - Link Loss case
     * HCI_ERROR_CONN_TERM_LOCAL_HOST - Disconnect triggered by device
     * HCI_ERROR_OETC_* - Other end (i.e., remote host) terminated connection
     */
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
            SetAppState(app_idle);
            /* FALLTHROUGH */
        case app_disconnecting:
        {
            /* Link Loss Case */
            if(IsGlucoseDataPending() ||
               (p_event_data->reason == HCI_ERROR_CONN_TIMEOUT))
            {
                /* Start fast advertisement for vendor specific time.*/
                SetAppState(app_fast_advertising);
            }
            else if(p_event_data->reason == HCI_ERROR_CONN_TERM_LOCAL_HOST)
            {
                /* It is possible to receive LM_EV_DISCONNECT_COMPLETE 
                 * event in app_state_connected state at the expiry of 
                 * lower layers ATT/SMP timer leading to disconnect
                 */
                if(g_gs_data.bonded)
                {
                    SetAppState(app_idle);
                }
                else
                {
                    SetAppState(app_fast_advertising);
                }
            }
        }
        break;

        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLMEncryptionChange
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_ENCRYPTION_CHANGE
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLMEncryptionChange(LM_EVENT_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {

            HCI_EV_DATA_ENCRYPTION_CHANGE_T *pEvDataEncryptChange = 
                                        &p_event_data->enc_change.data;

            if(pEvDataEncryptChange->status == HCI_SUCCESS)
            {
                g_gs_data.encrypt_enabled = pEvDataEncryptChange->enc_enable;

                /* Delete the bonding chance timer */
                TimerDelete(g_gs_data.bonding_reattempt_tid);
                g_gs_data.bonding_reattempt_tid = TIMER_INVALID;
            }

            /* If the current connection parameters being used don't comply with
             * the application's preferred connection parameters and the timer 
             * is not running and , start timer to trigger Connection Parameter 
             * Update procedure
             */
            if((g_gs_data.conn_param_update_tid == TIMER_INVALID) &&
                    (g_gs_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                     g_gs_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
                     || g_gs_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
                    )
              )
            {
                /* Set the num of connection update attempts to zero */
                g_gs_data.num_conn_update_req = 0;

                /* Start timer to trigger Connection Parameter Update procedure 
                 */
                g_gs_data.conn_param_update_tid = 
                                        TimerCreate(GAP_CONN_PARAM_TIMEOUT,
                                                TRUE, requestConnParamUpdate);
            } /* Else at the expiry of timer Connection parameter 
               * update procedure will get triggered
               */

            /* Update battery status at every encryption instance. It may not 
             * be worth updating timer more often, but again it will primarily 
             * depend upon application requirements 
             */
            BatteryUpdateLevel(g_gs_data.st_ucid);
        }
        break;
        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmDivApproveInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_DIV_APPROVE_IND.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_gs_data.state)
    {
        /* Request for approval from application comes only when pairing is not
         * in progress
         */
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            sm_div_verdict approve_div = SM_DIV_REVOKED;
            
            /* Check whether the application is still bonded (bonded flag gets
             * reset upon 'connect' button press by the user). Then check 
             * whether the diversifier is the same as the one stored by the 
             * application
             */
            if(g_gs_data.bonded)
            {
                if(g_gs_data.diversifier == p_event_data->div)
                {
                    approve_div = SM_DIV_APPROVED;
                }
            }

            SMDivApproval(p_event_data->cid, approve_div);
        }
        break;

        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;

    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmKeysInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_KEYS_IND and copies IRK from it
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmKeysInd(SM_KEYS_IND_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            /* Store the diversifier which will be used for accepting/rejecting
             * the encryption requests.
             */
            g_gs_data.diversifier = (p_event_data->keys)->div;

            /* Write the new diversifier to NVM */
            Nvm_Write(&g_gs_data.diversifier,
                      sizeof(g_gs_data.diversifier), 
                      NVM_OFFSET_SM_DIV);

            /* Store IRK if the connected host is using random resolvable 
             * address. IRK is used afterwards to validate the identity of 
             * connected host 
             */
            if(GattIsAddressResolvableRandom(&g_gs_data.con_bd_addr)) 
            {
                MemCopy(g_gs_data.central_device_irk.irk, 
                                    (p_event_data->keys)->irk,
                                    MAX_WORDS_IRK);

                /* If bonded device address is resolvable random
                 * then store IRK to NVM 
                 */
                Nvm_Write(g_gs_data.central_device_irk.irk, 
                          MAX_WORDS_IRK, 
                          NVM_OFFSET_SM_IRK);
            }
        }
        break;
        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
}


/*---------------------------------------------------------------------------
--*
 *  NAME
 *      handleSignalSmPairingAuthInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_PAIRING_AUTH_IND. This message will
 *      only be received when the peer device is initiating 'Just Works' 
 *      pairing.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 
*----------------------------------------------------------------------------*/
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed:  /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            /* Authorise the pairing request if the application is NOT bonded */
            if(!g_gs_data.bonded)
            {
                SMPairingAuthRsp(p_event_data->data, TRUE);
            }
            else /* Otherwise Reject the pairing request */
            {
                SMPairingAuthRsp(p_event_data->data, FALSE);
            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmSimplePairingCompleteInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmSimplePairingCompleteInd(
                            SM_SIMPLE_PAIRING_COMPLETE_IND_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
                case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            if(p_event_data->status == sys_status_success)
            {
              /*  DebugWriteString("Paring completed");                */
                g_gs_data.bonded = TRUE;
                g_gs_data.bonded_bd_addr = p_event_data->bd_addr;

                /* Store bonded host typed bd address to NVM */

                /* Write one word bonded flag */
                Nvm_Write((uint16 *)&g_gs_data.bonded, sizeof(g_gs_data.bonded),
                                     NVM_OFFSET_BONDED_FLAG);

                /* Write typed bd address of bonded host */
                Nvm_Write((uint16 *)&g_gs_data.bonded_bd_addr,
                          sizeof(TYPED_BD_ADDR_T), NVM_OFFSET_BONDED_ADDR);

                if(!GattIsAddressResolvableRandom(&g_gs_data.bonded_bd_addr))
                {
                    /* White list is configured with the Bonded host address */
                    if(LsAddWhiteListDevice(&g_gs_data.bonded_bd_addr) != 
                                        ls_err_none)
                    {
                        ReportPanic(app_panic_add_whitelist);
                    }
                }

                /* If the devices are bonded then send notification to all 
                 * registered services for the same so that they can store
                 * required data to NVM.
                 */
                GlucoseBondingNotify(g_gs_data.bonded);

                BatteryBondingNotify(g_gs_data.bonded);
            }
            else
            {
                /* Pairing has failed.
                 * 1. If pairing has failed due to repeated attempts, the 
                 *    application should immediately disconnect the link.
                 * 2. The application was bonded and pairing has failed.
                 *    Since the application was using whitelist, so the remote 
                 *    device has same address as our bonded device address.
                 *    The remote connected device may be a genuine one but 
                 *    instead of using old keys, wanted to use new keys. We 
                 *    don't allow bonding again if we are already bonded but we
                 *    will give some time to the connected device to encrypt the
                 *    link using the old keys. if the remote device encrypts the
                 *    link in that time, it's good. Otherwise we will disconnect
                 *    the link.
                 */
                 if(p_event_data->status == sm_status_repeated_attempts)
                 {
                    SetAppState(app_disconnecting);
                 }
                 else if(g_gs_data.bonded)
                 {
                    g_gs_data.encrypt_enabled = FALSE;
                    g_gs_data.bonding_reattempt_tid = TimerCreate(
                                           BONDING_CHANCE_TIMER,
                                           TRUE, 
                                           handleBondingChanceTimerExpiry);
                 }
            }
        }
        break;
        default:
        {
            /* Firmware may send this signal after disconnection. So don't 
             * panic but ignore this signal.
             */
        }
        break;
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnUpdateSignalCfm
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_UPDATE_SIGNALLING_RSP
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLsConnUpdateSignalCfm(
                                 LS_CONNECTION_PARAM_UPDATE_CFM_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed: /* FALLTHROUGH */
        case app_connected_and_subscribed:
        {
            /* Received in response to the L2CAP_CONNECTION_PARAMETER_UPDATE 
             * request sent from the slave after encryption is enabled. If the
             * request has failed, the device should again send same request
             * only after Tgap(conn_param_timeout). Refer Bluetooth 4.0
             * spec Vol 3 Part C, Section 9.3.9 and profile spec.
             */
            if (((p_event_data->status) != ls_err_none)
                && (g_gs_data.num_conn_update_req < 
                                MAX_NUM_CONN_PARAM_UPDATE_REQS))
            {
                /* Delete timer if running */
                TimerDelete(g_gs_data.conn_param_update_tid);
                
                g_gs_data.conn_param_update_tid= TimerCreate(
                                           GAP_CONN_PARAM_TIMEOUT,
                                           TRUE, requestConnParamUpdate);
            }
        }
        break;
        default:
        {
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        }
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmConnectionUpdate
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_UPDATE.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data)
{
    switch(g_gs_data.state)
    {
        case app_connected_not_subscribed:
        case app_connected_and_subscribed:
        case app_disconnecting:
        {
            /* Store the new connection parameters. */
            g_gs_data.conn_interval = p_event_data->data.conn_interval;
            g_gs_data.conn_latency = p_event_data->data.conn_latency;
            g_gs_data.conn_timeout = p_event_data->data.supervision_timeout;
        }
        break;

        default:
            /* Connection parameter update indication received in unexpected
             * application state.
             */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLsConnParamUpdateInd(
                                 LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data)
{

    /* The application has already received the new connection parameters while 
     * handling the event LM_EV_CONNECTION_UPDATE. Check if new parameters 
     * comply with application preferred parameters. If not, application shall 
     * trigger Connection parameter update procedure 
     */
    if(g_gs_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
       g_gs_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
       || g_gs_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
      )
    {
        /* Delete timer if running */
        TimerDelete(g_gs_data.conn_param_update_tid);
        /* Set the num of connection update attempts to zero */
        g_gs_data.num_conn_update_req = 0;

        /* Start timer to trigger Connection Parameter Update procedure */
        g_gs_data.conn_param_update_tid = TimerCreate(GAP_CONN_PARAM_TIMEOUT,
                                     TRUE, requestConnParamUpdate);
    }

 }

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleBondingChanceTimerExpiry
 *
 *  DESCRIPTION
 *      This function is handle the expiry of bonding chance timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 
*----------------------------------------------------------------------------*/
static void handleBondingChanceTimerExpiry(timer_id tid)
{
    if(g_gs_data.bonding_reattempt_tid== tid)
    {
        g_gs_data.bonding_reattempt_tid= TIMER_INVALID;
        /* The bonding chance timer has expired. This means the remote has not
         * encrypted the link using old keys. Disconnect the link.
         */
        SetAppState(app_disconnecting);
    }/* Else it may be due to some race condition. Ignore it. */
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      appInitExit
 *
 *  DESCRIPTION
 *      This function is called upon exiting from app_init state.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void appInitExit(void)
{
    if(g_gs_data.bonded && 
       (!GattIsAddressResolvableRandom(&g_gs_data.bonded_bd_addr)))
    {
        /* If the device is bonded, configure white list with the bonded 
         * host address 
         */

        if(LsAddWhiteListDevice(&g_gs_data.bonded_bd_addr) != 
                                        ls_err_none)
        {
            ReportPanic(app_panic_add_whitelist);
        }
    }
}


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      ReportPanic
 *
 *  DESCRIPTION
 *      This function raises the panic
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void ReportPanic(app_panic_code panic_code)
{
    /* If we want any debug prints, we can put them here */
    Panic(panic_code);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      SetAppState
 *
 *  DESCRIPTION
 *      This function is used to set the state of the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void SetAppState(app_state new_state)
{
    app_state old_state = g_gs_data.state;

    /* Check if the new state to be set is not the same as the present state
     * of the application. 
     */
    if (old_state != new_state)
    {
        /* Handle exiting old state */
        switch (old_state)
        {
            case app_init:
            {
                appInitExit();
            }
            break;

            case app_idle:
            break;

            case app_disconnecting:
            {
                /* Common things to do whenever application exits
                 * app_disconnecting state.
                 */
                gsDataInit();
            }
            break;

            case app_fast_advertising:/* FALLTHROUGH */
            case app_slow_advertising:
            {
                /* Common things to do whenever application exits
                 * APP_*_ADVERTISING state.
                 */
                /* Cancel advertisement timer */
                TimerDelete(g_gs_data.app_tid);
                g_gs_data.app_tid = TIMER_INVALID;
            }
            break;

            case app_connected_not_subscribed: /* FALLTHROUGH */
            case app_connected_and_subscribed:
                /* Nothing to do */
            break;

            default:
                
                /* Nothing to do */
            break;
        }

        /* Set new state */
        g_gs_data.state = new_state;

        /* Handle entering new state */
        switch (new_state)
        {
            case app_fast_advertising:
            {
                /* Start advertising and indicate this to user */
                GattTriggerFastAdverts();

                /* Indicate advertising to user */
                SetIndication(advertising_ind);

                /* Indicate advertising mode by sounding two beeps */
                SoundBuzzer(beep_twice);
            }
            break;

            case app_slow_advertising:
            {
                GattStartAdverts(FALSE);

                /* Indicate advertising to user */
                SetIndication(advertising_ind);
            }
            break;

            case app_init: /* FALLTHROUGH */
            case app_idle:
            {
                /* Stop the indication which we have started for 
                 * advertising/Connection 
                 */
                gsDataInit();

                /* Indicate this state LED */
                SetIndication(stop_ind);

                /* Sound long beep to indicate non connectable mode*/
                SoundBuzzer(beep_long);
            }
            break;

            case app_connected_not_subscribed: /* FALLTHROUGH */
            case app_connected_and_subscribed:
            {
                /* Indicate connected state to user */
                SetIndication(connected_ind);
            }
            break;


            case app_disconnecting:
            {
                GattDisconnectReq(g_gs_data.st_ucid);
            }
            break;


            default:
            break;
        }
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleShortButtonPress
 *
 *  DESCRIPTION
 *      This function is for handling the short Button press
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void HandleShortButtonPress(void)
{
    /*Button was pressed */
   /* static uint16 number = 0;*/

    /* Formulate and store glucose measurement. Measurements are always 
     * fetched by collector once the notifications are configured for
     * Glucose Measurement and Glucose Context Information 
     */
   /* number++;

    FormulateNAddGlucoseMeasData(number % 
                        GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH);

    number++;

    FormulateNAddGlucoseMeasData(number % 
                        GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH);

number++;

    FormulateNAddGlucoseMeasData(number % 
                        GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH);  */  

    /*sound Buzzer */
    SoundBuzzer(beep_short);

    switch(g_gs_data.state)
    {
        case app_init:
            /* FALLTHROUGH */
        case app_idle:
        {
            /* Start advertising */
            SetAppState(app_fast_advertising);
        }
        break;
        default:
            /* Nothing to do */
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleExtraLongButtonPress
 *
 *  DESCRIPTION
 *      This function contains handling of extra long button press, which
 *      signifies pairing / bonding removal
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HandleExtraLongButtonPress(timer_id tid)
{
    if(tid == g_app_hw_data.button_press_tid)
    {
        /* Re-initialise timer id */
        g_app_hw_data.button_press_tid = TIMER_INVALID;

        /* Sound three beeps to indicate pairing removal to user */
        SoundBuzzer(beep_thrice);

        /* Remove bonding information*/

        /* The device will no more be bonded */
        g_gs_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_gs_data.bonded, 
                  sizeof(g_gs_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);


        switch(g_gs_data.state)
        {
            case app_connected_not_subscribed: /* FALLTHROUGH */
            case app_connected_and_subscribed:
            {
                /* Disconnect with the connected host before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                SetAppState(app_disconnecting);

                /* Reset and clear the whitelist */
                LsResetWhiteList();
            }
            break;

            case app_fast_advertising: /* FALLTHROUGH */
            case app_slow_advertising:
            {
                g_gs_data.pairing_remove_button_pressed = TRUE;

                /* Delete the advertising timer */
                TimerDelete(g_gs_data.app_tid);
                g_gs_data.app_tid = TIMER_INVALID;

                /* Stop advertisements first as it may be making use of white 
                 * list. Once advertisements are stopped, reset the whitelist
                 * and trigger advertisements again for any host to connect
                 */
                GattStopAdverts();
            }
            break;

            case app_disconnecting:
            {
                /* Disconnect procedure on-going, so just reset the whitelist 
                 * and wait for procedure to get completed before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                LsResetWhiteList();
            }
            break;

            default: /* app_state_init / app_state_idle handling */
            {
                /* Initialise application and services data related to 
                 * for bonding status
                 */
                gsDataInit();

                /* Reset and clear the whitelist */
                LsResetWhiteList();

                /* Start fast undirected advertisements */
                SetAppState(app_fast_advertising);
            }
            break;

        }
    } /* Else ignore timer */

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppIsDeviceBonded
 *
 *  DESCRIPTION
 *      This function returns the status whether the connected device is 
 *      bonded or not.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern bool AppIsDeviceBonded(void)
{
    return (g_gs_data.bonded);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppIsLinkEncrypted
 *
 *  DESCRIPTION
 *      This function returns the status whether the link is encrypted or not.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern bool AppIsLinkEncrypted(void)
{
    return (g_gs_data.encrypt_enabled);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GetAppConnectedUcid
 *
 *  DESCRIPTION
 *      This function returns ucid of the connection
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern uint16 GetAppConnectedUcid(void)
{
    return (g_gs_data.st_ucid);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      DeleteIdleTimer
 *
 *  DESCRIPTION
 *      This function is used to delete Idle timer.
 *
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void DeleteIdleTimer(void)
{
    /* Delete Idle timer */
    TimerDelete(g_gs_data.app_tid);

    g_gs_data.app_tid = TIMER_INVALID;

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      ResetIdleTimer
 *
 *  DESCRIPTION
 *      This function is used to reset Idle timer.
 *
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void ResetIdleTimer(void)
{
    /* Reset Idle timer */
    TimerDelete(g_gs_data.app_tid);

#ifndef NO_IDLE_TIMEOUT
    g_gs_data.app_tid = TimerCreate(CONNECTED_IDLE_TIMEOUT_VALUE, 
                                    TRUE, gsIdleTimerHandler);
#endif  /*NO_IDLE_TIMEOUT*/
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This function is called just after a power-on reset (including after
 *      a firmware panic).
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the reset() function.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppPowerOnReset(void)
{
    /* Configure the application constants */
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This function is called after a power-on reset (including after a
 *      firmware panic) or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after app_power_on_reset.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppInit(sleep_state LastSleepState)
{
    uint16 gatt_database_length = 0;
    uint16 *p_gatt_database_pointer = NULL;
    uint16 pts_cskey = 0;
/*    DebugInit(0,NULL,NULL);*/
    /* uint8 message[]= {'a','p','p','s','t','a','r','t','e','d','\n','\r'}; 
    const uint8  message_len = (sizeof(message))/sizeof(uint8);*/
  
    /* Add opening message to byte queue */
    /*BQForceQueueBytes(message, message_len);
    sendPendingData();*/
/*   DebugWriteString("app started\n\r");*/
    /* Initialize the application timers */
    TimerInit(MAX_APP_TIMERS, (void*)app_timers);

    /* Initialize GATT entity */
    GattInit();

    /* Install GATT Server support for the optional Write procedures */
    GattInstallServerWrite();

    /* Setting for No MITM - SM_IO_CAP_NO_INPUT_NO_OUTPUT
     * This is the default setting, So no need to do it again 
     */
    /* SMSetIOCapabilities(SM_IO_CAP_NO_INPUT_NO_OUTPUT); */

    /* Reset and clear the whitelist */
    LsResetWhiteList();

#ifdef NVM_TYPE_EEPROM
    /* Configure the NVM manager to use I2C EEPROM for NVM store */
    NvmConfigureI2cEeprom();
#elif NVM_TYPE_FLASH
    /* Configure the NVM Manager to use SPI flash for NVM store. */
    NvmConfigureSpiFlash();
#endif /* NVM_TYPE_EEPROM */

    Nvm_Disable();

    /* Battery Service Initialization on Chip reset */
    BatteryInitChipReset();

    /* Glucose Service Initialization on Chip reset */
    GlucoseInitChipReset();

    /* Initialize the gap data. Needs to be done before readPersistentStore */
    GapDataInit();

    /* Read persistent storage */
    readPersistentStore();

    /* Tell Security Manager module about the value it needs to initialize it's
     * diversifier to.
     */
    SMInit(g_gs_data.diversifier);

    /* Initialize Glucose sensor application data structure */
    gsDataInit();

    /* Initialize Hardware to set 8051 for PIOs scanning */
    InitGSHardware();

    /* Initialize Glucose Sensor state */
    g_gs_data.state = app_init;
g_pts_abort_test = TRUE;/*I added*/
    /* Read the project keyr file for user defined CS keys for PTS testcases */
    pts_cskey = CSReadUserKey(PTS_CS_KEY_INDEX);
    if(pts_cskey & PTS_ABORT_CS_KEY_MASK)
    {
        /* CS key has been set for ABORT one second time gap */
        g_pts_abort_test = TRUE;
    }

    if(pts_cskey & PTS_GENERATE_CONTEXT_EVERY_RECORD_MASK)
    {
        /* CS key has been set for generating context in every record. */
        g_pts_generate_context_every_record = TRUE;
    }

    /* Tell GATT about our database. We will get a GATT_ADD_DB_CFM event when
     * this has completed.
     */
    p_gatt_database_pointer = GattGetDatabase(&gatt_database_length);
    GattAddDatabaseReq(gatt_database_length, p_gatt_database_pointer);
    /*HandleShortButtonPress();*/
/*      char string[]="hello";*/
    /*printForDebug(string);*/
     uartHandle();
     HandleShortButtonPress();
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppProcessSystemEvent(sys_event_id id, void *data)
{
    switch(id)
    {
        case sys_event_battery_low:
        {
            /* Battery low event received - notify the connected host. If 
             * not connected, the battery level will get notified when 
             * device gets connected again
             */
            if(g_gs_data.state == app_connected_not_subscribed ||
               g_gs_data.state == app_connected_and_subscribed)
            {
                BatteryUpdateLevel(g_gs_data.st_ucid);
            }
        }
        break;

        case sys_event_pio_changed:
        {
            HandlePIOChangedEvent(((pio_changed_data*)data)->pio_cause);
        }
        break;

        default:
            /* Ignore anything else */
        break;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event is
 *      received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

bool AppProcessLmEvent(lm_event_code event_code, LM_EVENT_T *p_event_data)
{
    switch(event_code)
    {
        /* Below messages are received in app_init state */
        case GATT_ADD_DB_CFM:
        {
           /* DebugWriteString("GATT_ADD_DB_CFM\n\r");*/
            if(((GATT_ADD_DB_CFM_T *)p_event_data)->result != 
                                            sys_status_success)
            {
                /* Don't expect this to happen */
                ReportPanic(app_panic_db_registration);
            }
            else
            {
                SetAppState(app_idle);
            }
        }
        break;

        case LM_EV_CONNECTION_COMPLETE:
            /* Handle the LM connection complete event. */
        /*DebugWriteString("M_EV_CONNECTION_COMPLETE\n\r");*/
            handleSignalLmEvConnectionComplete(
                                     (LM_EV_CONNECTION_COMPLETE_T*)p_event_data);
        break;

        case GATT_CONNECT_CFM:
            handleSignalGattConnectCFM((GATT_CONNECT_CFM_T *) p_event_data);
           /*  DebugWriteString("GATT_CONNECT_CFM\n\r");*/
        break;
        
        case GATT_CANCEL_CONNECT_CFM:
            handleSignalGattCancelConnectCFM();
            /*DebugWriteString("GATT_CANCEL_CONNECT_CFM\n\r");*/
        break;

        /* Below messages are received in connected state */
        case GATT_ACCESS_IND: /* GATT Access Indication */
            handleSignalGattAccessInd((GATT_ACCESS_IND_T *)p_event_data);
           /* DebugWriteString("GATT_ACCESS_IND\n\r");*/
        break;

        case GATT_DISCONNECT_IND:
            /* Disconnect procedure triggered by remote host or due to 
             * link loss is considered complete on reception of 
             * LM_EV_DISCONNECT_COMPLETE event. So, it gets handled on 
             * reception of LM_EV_DISCONNECT_COMPLETE event.
             */
        /*DebugWriteString("GATT_DISCONNECT_IND\n\r");*/
        break;

        case GATT_DISCONNECT_CFM:
            /* Confirmation for the completion of GattDisconnectReq()
             * procedure is ignored as the procedure is considered complete 
             * on reception of LM_EV_DISCONNECT_COMPLETE event. So, it gets 
             * handled on reception of LM_EV_DISCONNECT_COMPLETE event.
             */
       /* DebugWriteString("GATT_DISCONNECT_CFM\n\r");*/
        break;

        case LM_EV_DISCONNECT_COMPLETE:
        {
            /* Disconnect procedures either triggered by application or remote
             * host or link loss case are considered completed on reception 
             * of LM_EV_DISCONNECT_COMPLETE event
             */
             handleSignalLmDisconnectComplete(
                    &((LM_EV_DISCONNECT_COMPLETE_T *)p_event_data)->data);
             /*DebugWriteString("LM_EV_DISCONNECT_COMPLETE\n\r");*/
        }
        break;

        case LM_EV_ENCRYPTION_CHANGE:
            handleSignalLMEncryptionChange(p_event_data);
           /* DebugWriteString("LM_EV_ENCRYPTION_CHANGE\n\r");*/
        break;

        case SM_DIV_APPROVE_IND:
            handleSignalSmDivApproveInd((SM_DIV_APPROVE_IND_T *)p_event_data);
           /* DebugWriteString("SM_DIV_APPROVE_IND\n\r");*/
        break;

        case SM_KEYS_IND:
            handleSignalSmKeysInd((SM_KEYS_IND_T *)p_event_data);
         /*   DebugWriteString("SM_KEYS_IND\n\r");*/
        break;

        case SM_PAIRING_AUTH_IND:
            /* Authorize or Reject the pairing request */
            handleSignalSmPairingAuthInd((SM_PAIRING_AUTH_IND_T*)p_event_data);
          /*  DebugWriteString("SM_PAIRING_AUTH_IND\n\r");*/
        break;

        case SM_SIMPLE_PAIRING_COMPLETE_IND:
            handleSignalSmSimplePairingCompleteInd(
                        (SM_SIMPLE_PAIRING_COMPLETE_IND_T *)p_event_data);
          /*  DebugWriteString("SM_SIMPLE_PAIRING_COMPLETE_IND\n\r");*/
        break;


        case LS_CONNECTION_PARAM_UPDATE_CFM:
            handleSignalLsConnUpdateSignalCfm(
                            (LS_CONNECTION_PARAM_UPDATE_CFM_T *)p_event_data);
          /*  DebugWriteString("LS_CONNECTION_PARAM_UPDATE_CFM\n\r");*/
        break;

        case LM_EV_CONNECTION_UPDATE:
            /* This event is sent by the controller on connection parameter 
             * update. 
             */
            handleSignalLmConnectionUpdate(
                            (LM_EV_CONNECTION_UPDATE_T*)p_event_data);
            /*DebugWriteString("LM_EV_CONNECTION_UPDATE\n\r");*/
        break;

        case LS_CONNECTION_PARAM_UPDATE_IND:
            handleSignalLsConnParamUpdateInd(
                            (LS_CONNECTION_PARAM_UPDATE_IND_T *)p_event_data);
           /* DebugWriteString("LS_CONNECTION_PARAM_UPDATE_IND\n\r");*/
        break;


        case LS_RADIO_EVENT_IND:
            GlucoseHandleSignalLsRadioEventInd(g_gs_data.st_ucid);
          /* DebugWriteString("LS_RADIO_EVENT_IND\n\r");*/
        break;

        case GATT_CHAR_VAL_NOT_CFM:
            GlucoseHandleSignalGattCharValNotCfm((GATT_CHAR_VAL_IND_CFM_T *)
                                                 p_event_data);
         /*   DebugWriteString("GATT_CHAR_VAL_NOT_CFM\n\r");*/
        break;
        
        case LM_EV_NUMBER_COMPLETED_PACKETS:
            /* Do nothing */
          /*  DebugWriteString("LM_EV_NUMBER_COMPLETED_PACKETS\n\r");*/
        break;

        default:
            /* Control should never come here */
           /* DebugWriteString("Default: Control should never come here \n\r");*/
        break;

    }

    return TRUE;
}


