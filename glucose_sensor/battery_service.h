/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      battery_service.h
 *
 *  DESCRIPTION
 *      Header definitions for Battery service
 *
 ******************************************************************************/

#ifndef __BATT_SERVICE_H__
#define __BATT_SERVICE_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <bt_event_types.h>

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function initializes the battery service data. */
extern void BatteryDataInit(void);

/* This function initializes the battery data structure on chip reset. */
extern void BatteryInitChipReset(void);

/* This function handles the read access indication for battery service. */
extern void BatteryHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles the write access indication for battery service. */
extern void BatteryHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function updates the remote device about the current battery level. */
extern void BatteryUpdateLevel(uint16 ucid);

/* This function reads the battery service data from NVM */
extern void BatteryReadDataFromNVM(bool bonded, uint16 *p_offset);

/* This function checks if the passed handles falls in battery service handle 
 * range or not.
 */
extern bool BatteryCheckHandleRange(uint16 handle);

/* Application calls this function to notify battery service of the bonding 
 * status.
 */
extern void BatteryBondingNotify(bool bond_status);

#endif /* __BATT_SERVICE_H__ */
