/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_service.h
 *
 *  DESCRIPTION
 *      Header definitions for Glucose service
 *
 *  NOTES
 *
  *****************************************************************************/
#ifndef __GLUCOSE_SERVICE_H__
#define __GLUCOSE_SERVICE_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "app_gatt.h"
#include "uartio.h"
/*============================================================================*
 *  Public Definitions
 *============================================================================*/

#define MAX_LEN_MEAS_FIELDS                         (17)
#define MAX_LEN_MEAS_OPTIONAL_FIELDS                (7)

#define MAX_LEN_CONTEXT_FIELDS                      (17)
#define MAX_LEN_CONTEXT_OPTIONAL_FIELDS             (14)

#define MAX_NUMBER_GLUCOSE_MEASUREMENTS             (0x64)
#define MAX_NUMBER_GLUCOSE_CONTEXT                  (0x64)

/* Bit masks for glucose measurement flag byte */
#define TIME_OFFSET_PRESENT                         (0x01)
#define GLUCOSE_CONC_TYPE_SAMPLE_LOCATION_PRESENT   (0x02)

/* Otherwise the unit of glucose concentration is mg/dL. */
#define GLUCOSE_CONC_UNIT_MMOL_PER_LITRE            (0x04)
#define SENSOR_STATUS_ANNUNCIATION_PRESENT          (0x08)
#define CONTEXT_INFORMATION_PRESENT                 (0x10)

/* Sample location values and type values will be ORed to give one byte of 
 * type-sample location. Most significant nibble is for sample location value. 
 * So assign its values to the most significant nibble 
 */

/* Sample type values */
#define TYPE_CAPILLARY_WHOLE_BLOOD                  (0x01)
#define TYPE_CAPILLARY_PLASMA                       (0x02)
#define TYPE_VENOUS_WHOLE_BLOOD                     (0x03)
#define TYPE_VENOUS_PLASMA                          (0x04)
#define TYPE_ARTERIAL_WHOLE_BLOOD                   (0x05)
#define TYPE_ARTERIAL_PLASMA                        (0x06)
#define TYPE_UNDETERMINED_WHOLE_BLOOD               (0x07)
#define TYPE_UNDETERMINED_PLASMA                    (0x08)
#define TYPE_INTERSTITIAL_FLUID                     (0x09)
#define TYPE_CONTROL_SOLUTION                       (0x0a)
    
/* Sample location values */
#define LOCATION_FINGER                             (0x10)
#define LOCATION_ALTERNATE_SITE_TEST                (0x20)
#define LOCATION_EARLOBE                            (0x30)
#define LOCATION_CONTROL_SOLUTION                   (0x40)
#define LOCATION_NOT_AVAILABLE                      (0xFO)

/* Bit masks for sensor status annunciation field */
#define DEVICE_BATTERY_LOW                          (0x0001)
#define SENSOR_MALFUNCTION                          (0x0002)
#define SAMPLE_SIZE_INSUFFICIENT                    (0x0004)
#define STRIP_INSERTION_ERROR                       (0x0008)
#define STRIP_TYPE_INCORRECT                        (0x0010)
#define SENSOR_RESULT_TOO_HIGH                      (0x0020)
#define SENSOR_RESULT_TOO_LOW                       (0x0040)
#define SENSOR_TEMPERATURE_TOO_HIGH                 (0x0080)
#define SENSOR_TEMPERATURE_TOO_LOW                  (0x0100)
#define SENSOR_READ_INTERRUPTED                     (0x0200)
#define GENERAL_DEVICE_FAULT                        (0x0400)
#define TIME_FAULT                                  (0x0800)

/* Flag values for glucose measurement Context characteristic */
#define CARBOHYDRATE_FIELD_PRESENT                  (0x01)
#define MEAL_FIELD_PRESENT                          (0x02)
#define TESTER_HEALTH_FIELD_PRESENT                 (0x04)
#define EXERCISE_FIELD_PRESENT                      (0x08)
#define MEDICATION_FIELD_PRESENT                    (0x10)
#define MEDICATION_IN_MILLILITRES                   (0x20) 
                                 /* Else the medication unit is milligrams */

#define HBA1C_FIELD_PRESENT                         (0x40)
#define EXTENDED_FLAGS_PRESENT                      (0x80)

/* Carbohydrate ID values */
#define BREAKFAST                                   (0x01)
#define LUNCH                                       (0x02)
#define DINNER                                      (0x03)
#define SNACK                                       (0x04)
#define DRINKS                                      (0x05)
#define SUPPER                                      (0x06)
#define BRUNCH                                      (0x07)

/* Meal values */
#define BEFORE_MEAL                                 (0x01)
#define AFTER_MEAL                                  (0x02)
#define FASTING                                     (0x03)
#define CASUAL                                      (0x04)

/* Tester values */
#define SELF                                        (0x01)
#define HEALTH_CARE_PROFESSIONAL                    (0x02)
#define LAB_TEST                                    (0x03)
#define TESTER_VALUE_NOT_AVAILABLE                  (0x0F)

/* Health values */
/* Tester and health values will be ORed to give one byte of Tester-Health.
 * Most significant nibble is for health value. So assign its values to the most
 * significant nibble 
 */
#define MINOR_HEALTH_ISSUES                         (0x10)
#define MAJOR_HEALTH_ISSUES                         (0x20)
#define DURING_MENSES                               (0x30)
#define UNDER_STRESS                                (0x40)
#define NO_HEALTH_ISSUES                            (0x50)
#define HEALTH_VALUE_NOT_AVAILABLE                  (0xFO)

#define EXERCISE_DURATION_OVERRUN                   (0xFFFF)

/* Medication ID values */
#define RAPID_ACTING_INSULIN                        (0x01)
#define SHORT_ACTING_INSULIN                        (0x02)
#define INTERMEDIATE_ACTING_INSULIN                 (0x03)
#define LONG_ACTING_INSULIN                         (0x04)
#define PRE_MIXED_INSULIN                           (0x05)

/* Normal glucose concentration values */
#define GLUCOSE_MEAS_FASTING_NORMAL_MIN             (70)
#define GLUCOSE_MEAS_FASTING_NORMAL_MAX             (100)


/* Opcodes of record access control point operations */
#define REPORT_STORED_RECORDS                       (0x01)
#define DELETE_STORED_RECORDS                       (0x02)
#define ABORT_OPERATION                             (0x03)
#define REPORT_NUMBER_OF_STORED_RECORDS             (0x04)
#define NUMBER_OF_STORED_RECORDS_RESPONSE           (0x05)
#define RESPONSE_CODE                               (0x06)

/* Operators of record access control point operations */
#define OPERATOR_NULL                               (0x00)
#define ALL_RECORDS                                 (0x01)
#define LESS_THAN_OR_EQUAL_TO                       (0x02)
#define GREATER_THAN_OR_EQUAL_TO                    (0x03)
#define WITHIN_RANGE_OF                             (0x04)
#define FIRST_RECORD                                (0x05)
#define LAST_RECORD                                 (0x06)
#define OPERATOR_RFU_START                          (0x07)
#define OPERATOR_RFU_END                            (0xFF)

/* Operand filter type value */
#define SEQUENCE_NUMBER                             (0x01)
#define USER_FACING_TIME                            (0x02)

/* Response codes associated with opcode "RESPONSE CODE" */
#define RESPONSE_CODE_SUCCESS                       (0x01)
#define OPCODE_NOT_SUPPORTED                        (0x02)
#define INVALID_OPERATOR                            (0x03)
#define OPERATOR_NOT_SUPPORTED                      (0x04)
#define INVALID_OPERAND                             (0x05)
#define NO_RECORDS_FOUND                            (0x06)
#define ABORT_UNSUCCESSFUL                          (0x07)
#define PROCEDURE_NOT_COMPLETED                     (0x08)
#define FILTER_TYPE_NOT_SUPPORTED                   (0x09)

/* Error code definitions from glucose service spec */
#define PROCEDURE_ALREADY_IN_PROGRESS               (0x80| gatt_status_app_mask)
#define CLIENT_CHAR_CONFIG_DESC_IMPROPER_CONFIGURED (0x81| gatt_status_app_mask)

/* Macros for NVM access */
#define NVM_GLUCOSE_SEQ_NUM                         (0)
#define NVM_MEASUREMENT_CLIENT_CONFIG_OFFSET        (1)
#define NVM_CONTEXT_CLIENT_CONFIG_OFFSET            (2)
#define NVM_RACP_CLIENT_CONFIG_OFFSET               (3)

#define GLUCOSE_SERVICE_NVM_MEMORY_WORDS            (4)

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* This function intializes the Glucose Service data */
extern void GlucoseDataInit(void);

/* This function initializes the Glucose Service structure on chip reset.*/
extern void GlucoseInitChipReset(void);

/* This function checks if Glucose data is pending for transmission or not.*/
extern bool IsGlucoseDataPending(void);

/* This function adds Glucose measurement data to the measurement queue. */
extern void AddGlucoseMeasurementToQueue(
                uint8 meas_flag,uint8 *meas_data,uint16 meas_len,
                uint8 context_flag, uint8 *context_data, uint16 context_len,TIME_UNIX_CONV *tm);

/* This function handles the read access requests on Glucose Service 
 * characteristics.
 */
extern void GlucoseHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles the write access requests on Glucose Service 
 * characteristics.
 */
extern void GlucoseHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function read the Glucose service data from NVM */
extern void GlucoseReadDataFromNVM(bool bonded, uint16 *p_offset);

/* This function handles the radio events for Tx data..
 */
extern void GlucoseHandleSignalLsRadioEventInd(uint16 ucid);

/* This fucntin handles the confirmation signal for the notification sent.
 */
extern void GlucoseHandleSignalGattCharValNotCfm(GATT_CHAR_VAL_IND_CFM_T 
                                                                *p_event_data);

/* This function checks if the parameter received falls under the Glucose 
 * Service handle range or not.
 */
extern bool GlucoseCheckHandleRange(uint16 handle);

/* This function intializes the Sequence number for Glucose measurement and 
 * stores it in NVM.
 */
extern void GlucoseSeqNumInit(uint16 offset);

/* This function is used by application to notify bonding status to Glucose 
 * Service.
 */
extern void GlucoseBondingNotify(bool bond_Status);

#endif /* __GLUCOSE_SERVICE_H__ */
