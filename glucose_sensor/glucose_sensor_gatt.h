/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_sensor_gatt.h
 *
 *  DESCRIPTION
 *      Header file for Glucose Sensor GATT-related routines
 *
 ******************************************************************************/
#ifndef __GLUCOSE_SENSOR_GATT_H__
#define __GLUCOSE_SENSOR_GATT_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <gap_app_if.h>
#include <gatt.h>
#include <gatt_uuid.h>
#include <gatt_prim.h>


/*============================================================================*
 *  Public Definitions
 *============================================================================*/
/* Since the glucose sensor application uses limted discoverable mode, so 
 * advertising timers will be of 30 seconds both in fast as well as slow 
 * advertising mode.
 */
#define FAST_CONNECTION_ADVERT_TIMEOUT_VALUE      (30 * SECOND)
#define SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE      (30 * SECOND)

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function starts the advertisements. */
extern void GattStartAdverts(bool fast_connection);

/* This function stops advertisements. */
extern void GattStopAdverts(void);

/* This function handles access indicaton for the attributes being handled by
 * the application.
.*/
extern void GattHandleAccessInd(GATT_ACCESS_IND_T *p_ind);

/* This function prepares the list of 16-bit UUID services to be added to 
 * Advertisement data.
 */
extern uint16 GattGetSupported16BitUUIDServiceList(uint8 *p_service_uuid_ad);

/* This function checks if the address is resolvable random or not. */
extern bool GattIsAddressResolvableRandom(TYPED_BD_ADDR_T *addr);

/* This function triggers fast advertisements. */
extern void GattTriggerFastAdverts(void);

#endif /* __GLUCOSE_SENSOR_GATT_H__ */

