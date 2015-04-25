/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      app_gatt.h
 *
 *  DESCRIPTION
 *      Header definitions for common application attributes
 *
 *  NOTES
 *
 ******************************************************************************/

#ifndef __APP_GATT_H__
#define __APP_GATT_H__


/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <gatt.h>
#include <gatt_uuid.h>
#include <gatt_prim.h>
#include <nvm.h>
#include <mem.h>
#include <buf_utils.h>
#include <panic.h>


/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Invalid UCID indicating we are not currently connected */
#define GATT_INVALID_UCID                        (0xFFFF)

/* Invalid attribute Handle */
#define INVALID_ATT_HANDLE                       (0x0000)

/* AD type for Appearance */
#define AD_TYPE_APPEARANCE                       (0x19)


/* Maximum number of words in central device IRK */
#define MAX_WORDS_IRK                            (8)

/*Number of IRKs that application can store */
#define MAX_NUMBER_IRK_STORED                    (1)

/* Extract low order byte of 16-bit UUID */
#define LE8_L(x)                                 ((x) & 0xff)

/* Extract high order byte of 16-bit UUID */
#define LE8_H(x)                                 (((x) >> 8) & 0xff)


/* Maximum Length of device name prefixed with AD type complete local name 
 * after taking into consideration other elements added to advertisement. 
 * The advertising packet can receive upto a maximum of 31 octets
 * from application 
 */
#define DEVICE_NAME_MAX_LENGTH                   (20)

/* This constant is used in defining  some arrays so it
 * should always be large enough to hold the advertisement data.
 */
#define MAX_ADV_DATA_LEN                         (31)


#define GAP_CONN_PARAM_TIMEOUT                   (30 * SECOND)


/* CS KEY Index for PTS */
/* Application have eight CS keys for its use. Index for these keys : [0-7]
 * First CS key will be used for PTS test cases which require application
 * behaviour different from our current behaviour.
 * For such PTS test cases, we have implemented some work arounds which will 
 * get enabled by setting user CSkey at following index
 */
#define PTS_CS_KEY_INDEX                         (0x0000)


/* bit0 of CSkey will be used for abort test cases which require one second
 * gap between two glucose measurement notifications.
 */
#define PTS_ABORT_CS_KEY_MASK                    (0x0001)


/* Bit1 will be used for generating context in every record */
#define PTS_GENERATE_CONTEXT_EVERY_RECORD_MASK   (0x0002)

/* Timer value for remote device to re-encrypt the link using old keys */
#define BONDING_CHANCE_TIMER                     (30*SECOND)

/*============================================================================*
 *  Public Data Types
 *============================================================================*/

/* GATT client characteristic configuration value [Ref GATT spec, 3.3.3.3]*/
typedef enum
{
    gatt_client_config_none         = 0x0000,
    gatt_client_config_notification = 0x0001,
    gatt_client_config_indication   = 0x0002,
    gatt_client_config_reserved     = 0xFFF4

} gatt_client_config;

/*  Application defined panic codes */
typedef enum
{
    /* Failure while setting advertisement parameters */
    app_panic_set_advert_params,

    /* Failure while setting advertisement data */
    app_panic_set_advert_data,
    
    /* Failure while setting scan response data */
    app_panic_set_scan_rsp_data,

    /* Failure while establishing connection */
    app_panic_connection_est,

    /* Failure while registering GATT DB with firmware */
    app_panic_db_registration,

    /* Failure while reading NVM */
    app_panic_nvm_read,

    /* Failure while writing NVM */
    app_panic_nvm_write,

    /* Failure while reading Tx Power Level */
    app_panic_read_tx_pwr_level,

    /* Failure while deleting device from whitelist */
    app_panic_delete_whitelist,

    /* Failure while adding device to whitelist */
    app_panic_add_whitelist,

    /* Failure while triggering connection parameter update procedure */
    app_panic_con_param_update,

    /* Event received in an unexpected application state */
    app_panic_invalid_state,

    /* Unexpected beep type */
    app_panic_unexpected_beep_type
}app_panic_code;

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

/* These boolean variabled will be used for enabling PTS test case specific
 * code.
 */
extern bool g_pts_generate_context_every_record;
extern bool g_pts_abort_test;



/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

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
/* This function reports panic and resets the chip. */
extern void ReportPanic(app_panic_code panic_code);

/* This function checks if application bonded or not. */
extern bool AppIsDeviceBonded(void);

/* This function resets the idle timer. */
extern void ResetIdleTimer(void);

/* This function deletes the idle timer. */
extern void DeleteIdleTimer(void);

/* This function returns the application connection UCID. */
extern uint16 GetAppConnectedUcid(void);

/* This function checks if the link is encrypted or not. */
extern bool AppIsLinkEncrypted(void);
#endif /* __APP_GATT_H__ */
