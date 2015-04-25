/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor_gatt.c
 *
 *  DESCRIPTION
 *      Implementation of the Glucose Sensor GATT-related routines
 *
 *  NOTES
 *
 ******************************************************************************/
/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <ls_app_if.h>
#include <gap_app_if.h>
#include <timer.h>

/*============================================================================*
 *  Local Header File
 *============================================================================*/
#include "glucose_sensor_gatt.h"
#include "glucose_service_uuids.h"
#include "gap_service.h"
#include "glucose_service.h"
#include "glucose_sensor.h"
#include "app_gatt_db.h"
#include "dev_info_uuids.h"
#include "battery_service.h"
#include "gap_uuids.h"
#include "appearance.h"
#include "dev_info_service.h"
#include "app_gatt.h"
#include "gap_conn_params.h"

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void gattSetAdvertParams(gap_mode_connect connect_mode);
static void gattAdvertTimerHandler(timer_id tid);
static void gattHandleAccessRead(GATT_ACCESS_IND_T *p_ind);
static void gattHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);
static void gattAddDeviceNameToAdvData(uint16 adv_data_len, 
                                       uint16 scan_data_len);

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Invalid Tx power value */
#define TX_POWER_GLUCOSE_SENSOR_VALUE         (0)

 /* Length of Tx power prefixed with 'Tx Power' AD Type */
#define TX_POWER_VALUE_LENGTH                 (2)

/* Acceptable shortened device name length that can be sent in advertisement
 * data 
 */
#define SHORTENED_DEV_NAME_LEN                (8)


/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      gattAddDeviceNameToAdvData
 *
 *  DESCRIPTION
 *      This function is used to add device name to advertisement or scan 
 *      response data
 *      It follows below steps:
 *      a. Try to add complete device name to the advertisement packet
 *      b. Try to add complete device name to the scan response packet
 *      c. Try to add shortened device name to the advertisement packet
 *      d. Try to add shortened (max possible) device name to the scan response
 *          packet
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void gattAddDeviceNameToAdvData(uint16 adv_data_len,
                                       uint16 scan_data_len)
{

    uint8 *p_device_name = NULL;
    uint16 device_name_adtype_len;

    /* Read device name along with AD type and its length */
    p_device_name = GapGetNameAndLength(&device_name_adtype_len);

    /* Increment device_name_length by one to account for length field
     * which will be added by the GAP layer. 
     */

    /* Check if complete device name can fit in remaining advData space */
    if((device_name_adtype_len + 1) <= (MAX_ADV_DATA_LEN - adv_data_len))
    {
        /* Add shortened device name to advertisement data */
        p_device_name[0] = AD_TYPE_LOCAL_NAME_COMPLETE;
        
        /* Add complete device name to advertisement Data */
        if (LsStoreAdvScanData(device_name_adtype_len , p_device_name, 
                      ad_src_advertise) != ls_err_none)
        {
            /* control should never come here  */
            ReportPanic(app_panic_set_advert_data);
        }

    }
    /* Check if complete device name can fit in scan response message */
    else if((device_name_adtype_len + 1) <= (MAX_ADV_DATA_LEN - scan_data_len)) 
    {
        /* Add complete device name to scan response data */
        if (LsStoreAdvScanData(device_name_adtype_len , p_device_name, 
                      ad_src_scan_rsp) != ls_err_none)
        {
            /* control should never come here  */
            ReportPanic(app_panic_set_scan_rsp_data);
        }

    }
    /* Check if shortened device name can fit in remaining advData space */
    else if((MAX_ADV_DATA_LEN - adv_data_len) >=
            (SHORTENED_DEV_NAME_LEN + 2)) /* Added 2 for length and AD type
                                           * added by GAP layer
                                           */
    {
        /* Add shortened device name to advertisement data */
        p_device_name[0] = AD_TYPE_LOCAL_NAME_SHORT;

       if (LsStoreAdvScanData(SHORTENED_DEV_NAME_LEN , p_device_name, 
                      ad_src_advertise) != ls_err_none)
        {
            /* control should never come here  */
            ReportPanic(app_panic_set_advert_data);
        }

    }
    else /* Add device name to remaining Scan response data space */
    {
        /* Add as much as can be stored in scan response data */
        p_device_name[0] = AD_TYPE_LOCAL_NAME_SHORT;

       if (LsStoreAdvScanData(MAX_ADV_DATA_LEN - scan_data_len, 
                                    p_device_name, 
                                    ad_src_scan_rsp) != ls_err_none)
        {
            /* control should never come here  */
            ReportPanic(app_panic_set_scan_rsp_data);
        }

    }

}



/*----------------------------------------------------------------------------*
 *  NAME
 *      gattSetAdvertParams
 *
 *  DESCRIPTION
 *      This function is used to set advertisement parameters
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void gattSetAdvertParams(bool fast_connection)
{
    uint8 advert_data[MAX_ADV_DATA_LEN];
    uint16 length;
    uint32 adv_interval_min = RP_ADVERTISING_INTERVAL_MIN;
    uint32 adv_interval_max = RP_ADVERTISING_INTERVAL_MAX;

    int8 tx_power_level; /* Unsigned value */

    /* Tx power level value prefixed with 'Tx Power' AD Type */
    uint8 device_tx_power[TX_POWER_VALUE_LENGTH] = {
                AD_TYPE_TX_POWER
                };

    uint8 device_appearance[ATTR_LEN_DEVICE_APPEARANCE + 1] = {
                AD_TYPE_APPEARANCE,
                LE8_L(APPEARANCE_GLUCOSE_SENSOR_VALUE),
                LE8_H(APPEARANCE_GLUCOSE_SENSOR_VALUE)
                };

    /* A variable to keep track of the data added to AdvData. The limit is 
     * MAX_ADV_DATA_LEN.
     * GAP layer will add AD Flags to AdvData which is 3 bytes. Refer 
     * BT Spec 4.0, Vol 3, Part C,
     * Sec 11.1.3.
     */
    uint16 length_adv_data = 3;
    uint16 length_scan_data = 0;


    if(fast_connection)
    {
        adv_interval_min = FC_ADVERTISING_INTERVAL_MIN;
        adv_interval_max = FC_ADVERTISING_INTERVAL_MAX;
    }

    /* Put the glucose sensor in limited discoverable mode as specified in 
     * the Glucose profile specification.
     */
    if((GapSetMode(gap_role_peripheral, gap_mode_discover_limited,
                        gap_mode_connect_undirected, 
                        gap_mode_bond_yes,
                        gap_mode_security_unauthenticate) != ls_err_none) ||
       (GapSetAdvInterval(adv_interval_min, adv_interval_max) 
                        != ls_err_none))
    {
        /*Some error has occurred */
        ReportPanic(app_panic_set_advert_params);
    }

    /* Reset existing advertising data */
    if((LsStoreAdvScanData(0, NULL, ad_src_advertise) != ls_err_none) ||
        (LsStoreAdvScanData(0, NULL, ad_src_scan_rsp) != ls_err_none))
    {
        /*Some error has occurred */
        ReportPanic(app_panic_set_advert_data);
    }


    /* Setup ADVERTISEMENT DATA */

    /* Add UUID list of the services supported by the device */
    length = GattGetSupported16BitUUIDServiceList(advert_data);

    /* One added for length field, which will be added to Adv Data by 
     * GAP layer 
     */
    length_adv_data += (length + 1);

    /* One added for Length field, which will be added to Adv Data by GAP 
     * layer
     */
    length_adv_data += (sizeof(device_appearance) + 1);

    if ((LsStoreAdvScanData(length, advert_data, 
                        ad_src_advertise) != ls_err_none) ||
        (LsStoreAdvScanData(ATTR_LEN_DEVICE_APPEARANCE + 1, 
        device_appearance, ad_src_advertise) != ls_err_none))
    {
        /*Some error has occurred */
        ReportPanic(app_panic_set_advert_data);
    }



    /* Read tx power of the chip */
    if(LsReadTransmitPowerLevel(&tx_power_level) != ls_err_none)
    {
        /* Reading tx power failed */
        ReportPanic(app_panic_read_tx_pwr_level);
    }

    /* Add the read tx power level to device_tx_power 
     * Tx power level value is of 1 byte 
     */
    device_tx_power[TX_POWER_VALUE_LENGTH - 1] = (uint8 )tx_power_level;

    /* One added for length field, which will be added to Adv Data by GAP 
     * layer
     */
     length_scan_data += TX_POWER_VALUE_LENGTH + 1;

    /* Add tx power value of device to the scan response data */
    if (LsStoreAdvScanData(TX_POWER_VALUE_LENGTH, device_tx_power, 
                          ad_src_scan_rsp) != ls_err_none)
    {
        /*Some error has occurred */
        ReportPanic(app_panic_set_scan_rsp_data);
    }

    gattAddDeviceNameToAdvData(length_adv_data, length_scan_data);

}



/*----------------------------------------------------------------------------*
 *  NAME
 *      gattAdvertTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to stop on-going advertisements at the expiry of 
 *      DISCOVERABLE or RECONNECTION timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
static void gattAdvertTimerHandler(timer_id tid)
{
    /* Based upon the timer id, stop on-going advertisements */
    if(g_gs_data.app_tid == tid)
    {
        if(g_gs_data.state == app_fast_advertising)
        {
            /* Advertisement timer for reduced power connections */
            g_gs_data.advert_timer_value = 
                                SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE;
        }

        /* Stop on-going advertisements */
        GattStopAdverts();

        g_gs_data.app_tid = TIMER_INVALID;
    } /* Else ignore timer expiry, could be because of 
       * some race condition 
       */
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      gattHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on attributes (as received in 
 *      GATT_ACCESS_IND message) maintained by the application and respond 
 *      with the GATT_ACCESS_RSP message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void gattHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{

    /* Check all the services that support attribute 'Read' operation handled
     * by application
     */

    if(GapCheckHandleRange(p_ind->handle))
    {
        GapHandleAccessRead(p_ind);
    }
    else if(GlucoseCheckHandleRange(p_ind->handle))
    {
        GlucoseHandleAccessRead(p_ind);
    }
    else if(BatteryCheckHandleRange(p_ind->handle))
    {
        BatteryHandleAccessRead(p_ind);
    }
    else if(DeviceInfoCheckHandleRange(p_ind->handle))
    {
        DeviceInfoHandleAccessRead(p_ind);
    }
    else
    {
        /* Handle does not belong to these services. Send an error response */
        GattAccessRsp(p_ind->cid, p_ind->handle, 
                      gatt_status_read_not_permitted,
                      0, NULL);
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      gattHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on attributes (as received in 
 *      GATT_ACCESS_IND message) maintained by the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void gattHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{

    /* Check all the services that support attribute 'Write' operation handled
     * by application
     */

    if(GapCheckHandleRange(p_ind->handle))
    {
        GapHandleAccessWrite(p_ind);
    }
    else if(GlucoseCheckHandleRange(p_ind->handle))
    {
        GlucoseHandleAccessWrite(p_ind);
    }
    else if(BatteryCheckHandleRange(p_ind->handle))
    {
        BatteryHandleAccessWrite(p_ind);
    }
    else
    {
        /* Handle does not belong to these services. Send an error response */
        GattAccessRsp(p_ind->cid, p_ind->handle, 
                      gatt_status_write_not_permitted,
                      0, NULL);
    }

}


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattStartAdverts
 *
 *  DESCRIPTION
 *      This function is used to start undirected advertisements and moves
 *      to ADVERTISING state.
 *
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
extern void GattStartAdverts(bool fast_connection)
{
    uint16 connect_flags = L2CAP_CONNECTION_SLAVE_UNDIRECTED | 
                          L2CAP_OWN_ADDR_TYPE_PUBLIC | 
                          L2CAP_PEER_ADDR_TYPE_PUBLIC;

    /* Set UCID to INVALID_UCID */
    g_gs_data.st_ucid = GATT_INVALID_UCID;

    /* Set advertisement parameters */
    gattSetAdvertParams(fast_connection);

    /* If we are bonded and bonded device is not using resolvable random 
     * address, we will always use white list, set the controller's 
     * advertising filter policy to "process scan and connection requests only
     * from devices in the White List"
     */
    if(g_gs_data.bonded && 
        (!GattIsAddressResolvableRandom(&g_gs_data.bonded_bd_addr)))
    {
        connect_flags = L2CAP_CONNECTION_SLAVE_WHITELIST |
                       L2CAP_OWN_ADDR_TYPE_PUBLIC | 
                       L2CAP_PEER_ADDR_TYPE_PUBLIC;
    }

    /* Start GATT connection in Slave role */
    GattConnectReq(NULL, connect_flags);

    /* Start advertisement timer */
    if(g_gs_data.advert_timer_value)
    {
        TimerDelete(g_gs_data.app_tid);

        /* Start advertisement timer  */
        g_gs_data.app_tid = TimerCreate(g_gs_data.advert_timer_value, TRUE, 
                                        gattAdvertTimerHandler);
    }

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattStopAdverts
 *
 *  DESCRIPTION
 *      This function is used to stop on-going advertisements.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GattStopAdverts(void)
{
    GattCancelConnectReq();
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      GattHandleAccessInd
 *
 *  DESCRIPTION
 *      This function handles GATT_ACCESS_IND message for attributes maintained
 *      by the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GattHandleAccessInd(GATT_ACCESS_IND_T *p_ind)
{

    if(p_ind->flags == (ATT_ACCESS_WRITE | 
                        ATT_ACCESS_PERMISSION | 
                        ATT_ACCESS_WRITE_COMPLETE))
    {
        gattHandleAccessWrite(p_ind);
    }
    else if(p_ind->flags == (ATT_ACCESS_READ | 
                             ATT_ACCESS_PERMISSION))
    {
        gattHandleAccessRead(p_ind);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattGetSupported16BitUUIDServiceList
 *
 *  DESCRIPTION
 *      This function prepares the list of supported 16-bit service UUIDs to be 
 *      added to Advertisement data. It also adds the relevant AD Type to the 
 *      starting of AD array.
 *
 *  RETURNS/MODIFIES
 *      Return the size AD Service UUID data.
 *
 *----------------------------------------------------------------------------*/
extern uint16 GattGetSupported16BitUUIDServiceList(uint8 *p_service_uuid_ad)
{
    uint8   i = 0;

    /* Add 16-bit UUID for supported main service  */
    p_service_uuid_ad[i++] = AD_TYPE_SERVICE_UUID_16BIT_LIST;

    p_service_uuid_ad[i++] = LE8_L(UUID_GLUCOSE_SERVICE);
    p_service_uuid_ad[i++] = LE8_H(UUID_GLUCOSE_SERVICE);

    return ((uint16)i);

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattIsAddressResolvableRandom
 *
 *  DESCRIPTION
 *      This function checks if the address is resolvable random or not.
 *
 *  RETURNS/MODIFIES
 *      Boolean - True (Resolvable Random Address) /
 *                     False (Not a Resolvable Random Address)
 *
 *----------------------------------------------------------------------------*/

extern bool GattIsAddressResolvableRandom(TYPED_BD_ADDR_T *addr)
{
    if (addr->type != L2CA_RANDOM_ADDR_TYPE || 
        (addr->addr.nap & BD_ADDR_NAP_RANDOM_TYPE_MASK)
                                      != BD_ADDR_NAP_RANDOM_TYPE_RESOLVABLE)
    {
        /* This isn't a resolvable private address... */
        return FALSE;
    }
    return TRUE;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      GattTriggerFastAdverts
 *
 *  DESCRIPTION
 *      This function is used to start advertisements for fast connection 
 *      parameters
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/
extern void GattTriggerFastAdverts(void)
{

    g_gs_data.advert_timer_value = FAST_CONNECTION_ADVERT_TIMEOUT_VALUE;

    /* Trigger fast advertising */
    GattStartAdverts(TRUE);
}

