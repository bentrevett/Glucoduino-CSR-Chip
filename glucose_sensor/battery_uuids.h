/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      battery_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for battery service
 *
 ******************************************************************************/

#ifndef __BATTERY_UUIDS_H__
#define __BATTERY_UUIDS_H__

/* Brackets should not be used around the value of a macro. The parser which 
 * creates .c and .h files from .db file doesn't understand brackets and will
 * raise syntax errors. 
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/ 
 * Pages/ServiceViewer.aspx?u=org.bluetooth.service.generic_access.xml 
 */

/* Battery service UUID */
#define BATTERY_SERVICE_UUID                                              0x180f

/* Battery level UUID */
#define BATTERY_LEVEL_UUID                                                0x2a19

#endif /* __BATTERY_UUIDS_H__ */