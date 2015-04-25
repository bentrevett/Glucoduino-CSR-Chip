/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      dev_info_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for device info service
 *
 *  NOTES
 *
 ******************************************************************************/
#ifndef __DEVICE_INFO_UUIDS_H__
#define __DEVICE_INFO_UUIDS_H__

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser which
 * creates .c and .h files from .db file doesn't understand brackets
 * and will raise syntax errors.
 */


/* Vendor Id source */
#define VENDOR_ID_SRC_BT                                                  0x01
#define VENDOR_ID_SRC_USB                                                 0x02

/* Vendor Id */
#define VENDOR_ID                                                         0x000A
#define PRODUCT_ID                                                        0x014C
#define PRODUCT_VER                                                       0x0100

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/ 
 * Pages/ServiceViewer.aspx?u=org.bluetooth.service.generic_access.xml 
 */

/* Device information service */
#define UUID_DEVICE_INFO_SERVICE                                          0x180A

/* System ID UUID */
#define UUID_DEVICE_INFO_SYSTEM_ID                                        0x2A23
/* Model number UUID */
#define UUID_DEVICE_INFO_MODEL_NUMBER                                     0x2A24
/* Serial number UUID */
#define UUID_DEVICE_INFO_SERIAL_NUMBER                                    0x2A25
/* Hardware revision UUID */
#define UUID_DEVICE_INFO_HARDWARE_REVISION                                0x2A27
/* Firmware revision UUID */
#define UUID_DEVICE_INFO_FIRMWARE_REVISION                                0x2A26
/* Software revision UUID */
#define UUID_DEVICE_INFO_SOFTWARE_REVISION                                0x2A28
/* Manufacturer name UUID */
#define UUID_DEVICE_INFO_MANUFACTURER_NAME                                0x2A29
/* PnP ID UUID */
#define UUID_DEVICE_INFO_PNP_ID                                           0x2A50


#if defined(CSR101x_A05)
#define HARDWARE_REVISION "CSR101x A05"
#elif defined(CSR100x)
#define HARDWARE_REVISION "CSR100x A04"
#else
#define HARDWARE_REVISION "Unknown"
#endif

#endif /* __DEVICE_INFO_UUIDS_H__ */
