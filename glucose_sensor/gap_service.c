/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      gap_service.c
 *
 *  DESCRIPTION
 *      This file defines routines for using GAP service.
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <gatt.h>
#include <gatt_prim.h>
#include <mem.h>
#include <string.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "app_gatt.h"
#include "gap_service.h"
#include "app_gatt_db.h"
#include "nvm_access.h"

/*============================================================================*
 *  Private Data Types
 *============================================================================*/

typedef struct
{
    /* Name length in Bytes */
    uint16  length;

    /* Pointer to hold device name used by the application */
    uint8   *p_dev_name;

    /* NVM offset at which GAP data is stored */
    uint16  nvm_offset;

} GAP_DATA_T;


/*============================================================================*
 *  Private Data
 *============================================================================*/

/* GAP service data structure */
GAP_DATA_T g_gap_data;

/* Default device name - Added two for storing AD type and null ('\0') */ 
uint8 g_device_name[DEVICE_NAME_MAX_LENGTH + 2] = {
    AD_TYPE_LOCAL_NAME_COMPLETE,
     'C', 'S', 'R', ' ', 'G', 'l', 'u','c','o', 's', 'e', ' ',
     'S', 'e', 'n', 's', 'o', 'r', '\0'};

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Number of words of NVM memory used by GAP service */

/* Add space for Device Name Length and Device Name */
#define GAP_SERVICE_NVM_MEMORY_WORDS  (1 + DEVICE_NAME_MAX_LENGTH)

/* The offset of data being stored in NVM for GAP service. This offset is 
 * added to GAP service offset to NVM region (see g_gap_data.nvm_offset) 
 * to get the absolute offset at which this data is stored in NVM
 */
#define GAP_NVM_DEVICE_LENGTH_OFFSET  (0)

#define GAP_NVM_DEVICE_NAME_OFFSET    (1)

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
/* This function writes device name to NVM. */
static void gapWriteDeviceNameToNvm(void);

/* This function updates the device name. */
static void updateDeviceName(uint16 length, uint8 *name);


/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      gapWriteDeviceNameToNvm
 *
 *  DESCRIPTION
 *      This function is used to write GAP device name length and device name
 *      to NVM 
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void gapWriteDeviceNameToNvm(void)
{
    /* Write device name length to NVM */
    Nvm_Write(&g_gap_data.length, sizeof(g_gap_data.length), 
              g_gap_data.nvm_offset + 
              GAP_NVM_DEVICE_LENGTH_OFFSET);

    /* Write device name to NVM 
     * Typecast of uint8 to uint16 or vice-versa shall not have any side 
     * affects as both types (uint8 and uint16) take one word memory on XAP
     */
    Nvm_Write((uint16*)g_gap_data.p_dev_name, g_gap_data.length, 
              g_gap_data.nvm_offset + 
              GAP_NVM_DEVICE_NAME_OFFSET);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      updateDeviceName
 *
 *  DESCRIPTION
 *      This function updates the device name and length in gap service.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void updateDeviceName(uint16 length, uint8 *name)
{
    uint8   *p_name = g_gap_data.p_dev_name;

    /* Update device name length to the maximum of DEVICE_NAME_MAX_LENGTH  */
    if(length < DEVICE_NAME_MAX_LENGTH)
        g_gap_data.length = length;
    else
        g_gap_data.length = DEVICE_NAME_MAX_LENGTH;

    MemCopy(p_name, name, g_gap_data.length);

    /* Null terminate the device name string */
    p_name[g_gap_data.length] = '\0';

    gapWriteDeviceNameToNvm();

}


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialize GAP service data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GapDataInit(void)
{
    /* Skip 1st byte to move over AD type field and point to device name */
    g_gap_data.p_dev_name = (g_device_name + 1);
    g_gap_data.length = StrLen((char *)g_gap_data.p_dev_name);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles read operation on GAP service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GapHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  *p_value = NULL;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {

        case HANDLE_DEVICE_NAME:
        {
            /* Reading the device name characteristic of gap service. */

            length = g_gap_data.length;
            p_value = g_gap_data.p_dev_name;
        }
        break;

        default:
        {
            rc = gatt_status_read_not_permitted;
        }
        break;

    }
    /* Reply to the access indication */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, p_value);

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on GAP service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GapHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {

        case HANDLE_DEVICE_NAME:
        {
            /* Host is updating device name of glucose sensor */
            updateDeviceName(p_ind->size_value, p_ind->value);
        }
        break;

        default:
        {
            rc = gatt_status_write_not_permitted;
        }
        break;

    }
    /* Reply to the access indication */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read GAP specific data store in NVM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GapReadDataFromNVM(uint16 *p_offset)
{

    g_gap_data.nvm_offset = *p_offset;

    /* Read Device Length */
    Nvm_Read(&g_gap_data.length, sizeof(g_gap_data.length), 
             *p_offset + 
             GAP_NVM_DEVICE_LENGTH_OFFSET);

    /* Read Device Name 
     * Typecast of uint8 to uint16 or vice-versa shall not have any side 
     * affects as both types (uint8 and uint16) take one word memory on XAP
     */
    Nvm_Read((uint16*)g_gap_data.p_dev_name, g_gap_data.length, 
             *p_offset + 
             GAP_NVM_DEVICE_NAME_OFFSET);

    /* Add NULL character to terminate the device name string */
    g_gap_data.p_dev_name[g_gap_data.length] = '\0';

    /* Increase NVM offset for maximum device name length. Add 1 for
     * 'device name length' field as well
     */
    *p_offset += DEVICE_NAME_MAX_LENGTH + 1;

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      GapInitWriteDataToNVM
 *
 *  DESCRIPTION
 *      This function is used to write GAP specific data to NVM for 
 *      the first time during application initialisation
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GapInitWriteDataToNVM(uint16 *p_offset)
{

    /* The NVM offset at which GAP service data will be stored */
    g_gap_data.nvm_offset = *p_offset;

    gapWriteDeviceNameToNvm();

    /* Increase NVM offset for maximum device name length. Add 1 for
     * 'device name length' field as well
     */
    *p_offset += DEVICE_NAME_MAX_LENGTH + 1;

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the GAP 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/

extern bool GapCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_GAP_SERVICE) &&
            (handle <= HANDLE_GAP_SERVICE_END))
            ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GapGetNameAndLength
 *
 *  DESCRIPTION
 *      This function is used to get the reference to the 'g_device_name' 
 *      array, which contains AD type and device name. This function also
 *      returns the AD Type and device name length.
 *
 *  RETURNS/MODIFIES
 *      Pointer to device name array.
 *
 *----------------------------------------------------------------------------*/

extern uint8 *GapGetNameAndLength(uint16 *p_name_length)
{
    *p_name_length = StrLen((char *)g_device_name);
    return (g_device_name);
}
