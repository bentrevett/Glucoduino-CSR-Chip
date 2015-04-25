/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      battery_service.c
 *
 *  DESCRIPTION
 *      This file defines routines for using Battery service.
 *
 *  NOTES
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <gatt.h>
#include <gatt_prim.h>
#include <battery.h>
#include <buf_utils.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "app_gatt.h"
#include "battery_service.h"
#include "nvm_access.h"
#include "app_gatt_db.h"

/*============================================================================*
 *  Private Data Types
 *============================================================================*/
/* Battery service data type */
typedef struct
{
    /* Battery level */
    uint8   level;

    /* Client configuration for battery Level characteristic */
    gatt_client_config level_client_config;

    /* NVM offset at which battery data is stored */
    uint16 nvm_offset;

} BATT_DATA_T;


/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Battery service data instance */
BATT_DATA_T g_batt_data;


/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Battery level full in percentage */
#define BATTERY_LEVEL_FULL                            (100)

/* Battery critical level in percentage */
#define BATTERY_CRITICAL_LEVEL                        (10)

/* Battery minimum and maximum voltages in mV */
#define BATTERY_FULL_BATTERY_VOLTAGE                  (3000)          /* 3.0V */
#define BATTERY_FLAT_BATTERY_VOLTAGE                  (1800)          /* 1.8V */

/* Number of words of NVM memory used by battery service */
#define BATTERY_SERVICE_NVM_MEMORY_WORDS              (1)

/* The offset of data being stored in NVM for battery service. This offset is 
 * added to Battery service offset to NVM region (see GET_BATTERY_NVM_OFFSET()) 
 * to get the absolute offset at which this data is stored in NVM
 */
#define BATTERY_NVM_LEVEL_CLIENT_CONFIG_OFFSET        (0)

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static uint8 readBatteryLevel(void);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      readBatteryLevel
 *
 *  DESCRIPTION
 *      This function reads the battery level 
 *
 *  RETURNS/MODIFIES
 *      uint8 - Battery Level.
 *
 *----------------------------------------------------------------------------*/
static uint8 readBatteryLevel(void)
{
    uint32 bat_voltage;
    uint32 bat_level;

    /* Read battery voltage and level it with minimum voltage */
    bat_voltage = BatteryReadVoltage();

    /* Level the read battery voltage to the minimum value */
    if(bat_voltage < BATTERY_FLAT_BATTERY_VOLTAGE)
    {
        bat_voltage = BATTERY_FLAT_BATTERY_VOLTAGE;
    }

    bat_voltage -= BATTERY_FLAT_BATTERY_VOLTAGE;
    
    /* Get battery level in percent */
    bat_level = (bat_voltage * 100) / (BATTERY_FULL_BATTERY_VOLTAGE - 
                                                  BATTERY_FLAT_BATTERY_VOLTAGE);

    /* Check the precision errors */
    if(bat_level > 100)
        bat_level = 100;

    return (uint8)bat_level;
    
}

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialize Battery service data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryDataInit(void)
{
    if(!AppIsDeviceBonded())
    {
        /* Initialize battery level characteristic client configuration 
         * only if device is not bonded
         */
        g_batt_data.level_client_config = gatt_client_config_none;
    }

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryInitChipReset
 *
 *  DESCRIPTION
 *      This function is used to initialize Battery service data 
 *      structure at chip reset
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryInitChipReset(void)
{

    /* Initialise battery level to 0 percent so that the battery level 
     * notification (if configured) is sent when the value is read for 
     * the first time after power cycle.
     */
    g_batt_data.level = 0;

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles read operation on battery service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  value[2];
    uint8 *p_value = NULL;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {

        case HANDLE_BATT_LEVEL:
        {
            /* Reading battery level */
            length = 1; /* One Octet */

            g_batt_data.level = readBatteryLevel();

            value[0] = g_batt_data.level;
        }
        break;

        case HANDLE_BATT_LEVEL_C_CFG:
        {
            /* Reading the battery level client configuration descriptor */
            length = 2; /* Two octets */
            p_value = value;
            BufWriteUint16((uint8 **)&p_value, g_batt_data.level_client_config);
        }
        break;

        default:
        {
            /* No more IRQ characteristics */
            rc = gatt_status_read_not_permitted;
        }
        break;

    }

    /* Send access response */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, value);

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on Battery service attributes 
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

extern void BatteryHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint8 *p_value = p_ind->value;
    uint16 client_config;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {
        case HANDLE_BATT_LEVEL_C_CFG:
        {
            client_config = BufReadUint16(&p_value);

            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                g_batt_data.level_client_config = client_config;

                /* Write batery level client configuration to NVM if the 
                 * device is bonded.
                 */
                if(AppIsDeviceBonded())
                {
                     Nvm_Write((uint16 *)&client_config,
                              sizeof(client_config),
                              g_batt_data.nvm_offset + 
                              BATTERY_NVM_LEVEL_CLIENT_CONFIG_OFFSET);
                }
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return error as only notifications are supported */
                rc = gatt_status_app_mask;
            }
        }
        break;

        default:
        {
            rc = gatt_status_write_not_permitted;
        }
        break;
    }

    /* Send ACCESS RESPONSE */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);

    /* Send an update as soon as notifications are configured */
    if(g_batt_data.level_client_config == gatt_client_config_notification)
    {
        /* Reset current battery level to an invalid value so that it
         * triggers notifications on reading the current battery level
         */
        g_batt_data.level = 0xFF; /* 0 to 100: Valid value range */
        BatteryUpdateLevel(p_ind->cid);
    }

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BattUpdateLevel
 *
 *  DESCRIPTION
 *      This function is to monitor the battery level and trigger notifications
 *      (if configured) to the connected host.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryUpdateLevel(uint16 ucid)
{
    uint8 old_vbat;
    uint8 cur_bat_level;

    /* Read the battery level and status */
    cur_bat_level = readBatteryLevel();

    old_vbat = (g_batt_data.level);

    /* check if the current and old battery level are same or not. Notify 
     * battery level only if they are different.
     */
    if(old_vbat != cur_bat_level)
    {

        if((ucid != GATT_INVALID_UCID) && AppIsLinkEncrypted() &&
          (g_batt_data.level_client_config == gatt_client_config_notification))
        {

            /* Update battery level characteristic in database */
            g_batt_data.level = cur_bat_level;

            GattCharValueNotification(ucid, 
                                      HANDLE_BATT_LEVEL, 
                                      1, &cur_bat_level);
        }
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read Battery service data store in NVM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryReadDataFromNVM(bool bonded, uint16 *p_offset)
{
    g_batt_data.nvm_offset = *p_offset;

    /* Read NVM only if devices are bonded */
    if(bonded)
    {

        /* Read battery level client configuration */
        Nvm_Read((uint16 *)&g_batt_data.level_client_config,
                   sizeof(g_batt_data.level_client_config),
                   *p_offset + 
                   BATTERY_NVM_LEVEL_CLIENT_CONFIG_OFFSET);

    }

    /* Increment the offset by the number of words of NVM memory required 
     * by Battery service 
     */
    *p_offset += BATTERY_SERVICE_NVM_MEMORY_WORDS;

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the Battery 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/

extern bool BatteryCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_BATTERY_SERVICE) &&
            (handle <= HANDLE_BATTERY_SERVICE_END))
            ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      BatteryBondingNotify
 *
 *  DESCRIPTION
 *      This function is used by application to notify bonding status to 
 *      Battery service
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void BatteryBondingNotify(bool bond_status)
{
    uint16 batt_client_config = g_batt_data.level_client_config;
    uint16 batt_offset;

    /* Write data to NVM if bond is established */
    if(bond_status)
    {
        /* Write to NVM the client configuration value of battery level 
         * that was configured prior to bonding 
         */
             
        batt_offset =  g_batt_data.nvm_offset + 
                                  BATTERY_NVM_LEVEL_CLIENT_CONFIG_OFFSET;
        Nvm_Write((uint16 *)&batt_client_config, sizeof(batt_client_config),
                                                              batt_offset);
    }

}
