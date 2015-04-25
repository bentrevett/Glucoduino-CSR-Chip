/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_service_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for glucose service
 *
 *NOTES
 *
 ******************************************************************************/

#ifndef __GLUCOSE_SERVICE_UUID_H__
#define __GLUCOSE_SERVICE_UUID_H__

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Macro expansions have been kept out of brackets because DB generator is 
 * unable to parse Macros expansions enclosed in brackets 
 */
 
/* For UUID values, refer http://developer.bluetooth.org/gatt/services/ 
 * Pages/ServiceViewer.aspx?u=org.bluetooth.service.generic_access.xml 
 */

/* UUID for glucose primary service */
#define UUID_GLUCOSE_SERVICE                                             0x1808

/* UUID for glucose measurement characteristic */
#define UUID_GLUCOSE_MEASUREMENT                                         0x2A18

/* UUID for glucose measurement context characteristic */
#define UUID_GLUCOSE_MEASUREMENT_CONTEXT                                 0x2A34

/* UUID for glucose feature characteristic */
#define UUID_GLUCOSE_FEATURE                                             0x2A51

/* UUID for record access control point */
#define UUID_RECORD_ACCESS_CONTROL_POINT                                 0x2A52


/* Macros for glucose feature characteristic */
#define LOW_BATTERY_DETECTION                                            0x0001
#define SENSOR_MALFUNCTION_DETECTION                                     0x0002
#define SENSOR_SAMPLE_SIZE_SUPPORT                                       0x0004
#define STRIP_INSERTION_ERROR_DETECTION                                  0x0008
#define STRIP_TYPE_ERROR_DETECTION                                       0x0010
#define RESULT_HIGH_LOW_DETECTION                                        0x0020
#define TEMPERATURE_HIGH_LOW_DETECTION                                   0x0040
#define SENSOR_READ_INTERRUPT_DETECTION                                  0x0080
#define GENERAL_DEVICE_FAULT_SUPPORT                                     0x0100
#define TIME_FAULT_SUPPORT                                               0x0200
#define MULTIPLE_BOND_SUPPORT                                            0x0400


#define GLUCOSE_FEATURE_VALUE                                            0x03FF
/* Currently OR operation is not working in .db files. 
 * That is why we have directly used the value. 
 * It has been calculated from the following OR operation
 * 
 * GLUCOSE_FEATURE_VALUE : [LOW_BATTERY_DETECTION | 
 *             SENSOR_MALFUNCTION_DETECTION | 
 *             SENSOR_SAMPLE_SIZE_SUPPORT | 
 *             STRIP_INSERTION_ERROR_DETECTION | 
 *             STRIP_TYPE_ERROR_DETECTION | 
 *             RESULT_HIGH_LOW_DETECTION | 
 *             TEMPERATURE_HIGH_LOW_DETECTION | 
 *             SENSOR_READ_INTERRUPT_DETECTION |
 *             GENERAL_DEVICE_FAULT_SUPPORT | 
 *             TIME_FAULT_SUPPORT] 
 */

#endif /* __GLUCOSE_SERVICE_UUID_H__ */
