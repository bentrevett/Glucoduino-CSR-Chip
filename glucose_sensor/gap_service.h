/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      gap_service.h
 *
 *  DESCRIPTION
 *      Header definitions for GAP service
 *
 ******************************************************************************/

#ifndef __GAP_SERVICE_H__
#define __GAP_SERVICE_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function initializes the GAP service data. */
extern void GapDataInit(void);

/* This function handles the read access indication for GAP service 
 * characteristic.
 */
extern void GapHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles the write access indication for GAP service 
 * characteristic.
 */
extern void GapHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function reads GAP service data from NVM. */
extern void GapReadDataFromNVM(uint16 *p_offset);

/* This function initializes and writes data on NVM. */
extern void GapInitWriteDataToNVM(uint16 *p_offset);

/* This function checks if the handle passed fall in GAP service handle range
 * or not.
 */
extern bool GapCheckHandleRange(uint16 handle);

/* This function reads a global variable for device name and length. */
extern uint8 *GapGetNameAndLength(uint16 *p_name_length);

#endif /* __GAP_SERVICE_H__ */
