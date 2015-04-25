/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor.h 
 *
 *  DESCRIPTION
 *      Header definitions for application
 *
*******************************************************************************/
#ifndef __GLUCOSE_SENSOR_H__
#define __GLUCOSE_SENSOR_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <bluetooth.h>
#include <bt_event_types.h>
#include <timer.h>
#include <panic.h>
#include <config_store.h>

/*============================================================================*
 *  Local Header File
 *============================================================================*/
#include "glucose_service.h"
#include "app_gatt.h"

/*============================================================================*
 *  Public Data Types
 *============================================================================*/


typedef enum app_state_tag
{
    /* Initial State */
    app_init = 0,

    /* Fast undirected advertisements configured */
    app_fast_advertising,

    /* Slow undirected advertisements configured */
    app_slow_advertising,

    /* Glucose Sensor has established connection to the host but Host has not 
     * subscribed for glucose measurement notifications yet
     */
    app_connected_not_subscribed,

    /* Glucose sensor is connected to host and host has subscribed the 
     * notifications of glucose measurements.
     */
    app_connected_and_subscribed,

    /* Enters when disconnect is initiated by the device */
    app_disconnecting,

    /* Enters when connection has been removed */
    app_idle
} app_state;

/* Structure defined for Central device IRK */
typedef struct
{
    uint16                              irk[MAX_WORDS_IRK];

} CENTRAL_DEVICE_IRK_T;


typedef struct
{
    app_state                                state;

    /* Value for which advertisement timer needs to be started. 
     *
     * For bonded devices, the timer is initially started for 10 seconds to 
     * enable fast connection by bonded device to the sensor. If bonded device 
     * doesn't connect within this time, another 20 seconds timer is started 
     * to enable fast connections from any collector device in the vicinity. 
     * This is then followed by reduced power advertisements.
     *
     * For non-bonded devices, the timer is initially started for 30 seconds 
     * to enable fast connections from any collector device in the vicinity.
     * This is then followed by reduced power advertisements.
     */
    uint32                                      advert_timer_value;

    /* Store timer id in 'UNDIRECTED ADVERTS', and 
     * 'CONNECTED' states.
     */
    timer_id                                    app_tid;

    /* TYPED_BD_ADDR_T of the host to which device is connected */
    TYPED_BD_ADDR_T                             con_bd_addr;

    /* Track the UCID as Clients connect and disconnect */
    uint16                                      st_ucid;

    /* Boolean flag to indicated whether the device is bonded */
    bool                                        bonded;

    /* TYPED_BD_ADDR_T of the host to which app is bonded. App
     * can only bond to one collector 
     */
    TYPED_BD_ADDR_T                             bonded_bd_addr;

    /* Boolean flag to indicate whether encryption is enabled with the bonded 
     * host
     */
    bool                                        encrypt_enabled;
    
    /* Timer to hold the time elapsed after the last 
     * L2CAP_CONNECTION_PARAMETER_UPDATE Request failed.
     */
    timer_id                                    conn_param_update_tid;

    /* Diversifier associated with the LTK of the bonded device */
    uint16                                      diversifier;

    /*Central Private Address Resolution IRK  Will only be used when
     *central device used resolvable random address. 
     */
    CENTRAL_DEVICE_IRK_T                        central_device_irk;

    /* variable to keep track of number of connection parameter update 
     * request made 
     */
    uint8                                       num_conn_update_req;

    /* Boolean flag set to indicate pairing button press */
    bool                                        pairing_remove_button_pressed;

    /* This timer will be used if the application is already bonded to the 
     * remote host address but the remote device wanted to rebond which we had 
     * declined. In this scenario, we give ample time to the remote device to 
     * encrypt the link using old keys. If remote device doesn't encrypt the 
     * link, we will disconnect the link on this timer expiry.
     */
    timer_id                                    bonding_reattempt_tid;

    /* Varibale to store the current connection interval being used. */
    uint16                                      conn_interval;

    /* Variable to store the current slave latency. */
    uint16                                      conn_latency;

    /*Variable to store the current connection timeout value. */
    uint16                                      conn_timeout;
} APP_DATA_T;

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/
extern APP_DATA_T g_gs_data;


/*============================================================================*
 *  Public definitions
 *============================================================================*/

/* Magic value to check the sanity of NVM region used by the application */
#define NVM_SANITY_MAGIC               (0xAB01)

/* NVM offset for NVM sanity word */
#define NVM_OFFSET_SANITY_WORD         (0)

/* NVM offset for bonded flag */
#define NVM_OFFSET_BONDED_FLAG         (NVM_OFFSET_SANITY_WORD + 1)

/* NVM offset for bonded device bluetooth address */
#define NVM_OFFSET_BONDED_ADDR         (NVM_OFFSET_BONDED_FLAG + \
                                        sizeof(g_gs_data.bonded))

/* NVM offset for diversifier */
#define NVM_OFFSET_SM_DIV              (NVM_OFFSET_BONDED_ADDR + \
                                        sizeof(g_gs_data.bonded_bd_addr))

/* NVM offset for IRK */
#define NVM_OFFSET_SM_IRK              (NVM_OFFSET_SM_DIV + \
                                        sizeof(g_gs_data.diversifier))

/* Number of words of NVM used by application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define NVM_MAX_APP_MEMORY_WORDS       (NVM_OFFSET_SM_IRK + \
                                        MAX_WORDS_IRK)



/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function sets the application state to new state. */
extern void SetAppState(app_state new_state);

/* This function handles the extra long button press. */
extern void HandleExtraLongButtonPress(timer_id tid);

/* This function handles the short button press. */
extern void HandleShortButtonPress(void);
#endif /* __GLUCOSE_SENSOR_H__ */
