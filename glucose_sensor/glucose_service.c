/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      glucose_service.c
 *
 *  DESCRIPTION
 *      This file defines routines for using glucose service.
 *
 *  NOTES
 *
 ******************************************************************************/
/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <bluetooth.h>
#include <bt_event_types.h>
#include <timer.h>
#include <ls_app_if.h>
/*============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "glucose_service.h"
#include "app_gatt_db.h"
#include "nvm_access.h"



/*============================================================================*
 *  Private Data Declaration
 *============================================================================*/

/* Application has to take care that it provides the same sequence number
 * to the glucose measurement and context which are related 
 * (i.e., which belong to the same patient record). 
 */
typedef struct _glucose_measurement
{
    uint16      sequence_number;

    uint16      meas_len;

    bool        deleted;

    /* All fields of glucose measurement characteristic have to be supplied 
     * by the application as an uint8 array, which can go upto maximum of 
     * 7 octets 
     */
    uint8       meas_data[MAX_LEN_MEAS_FIELDS];

}GLUCOSE_MEASUREMENT_T;


typedef struct _glucose_context
{
    uint16       sequence_number;
    uint16       context_len;
    uint8        context_data[MAX_LEN_CONTEXT_FIELDS];
}GLUCOSE_CONTEXT_T;


/* Circular queue for storing pending glucose measurement values */
typedef struct _cqueue_glucose_measurement
{
    /* Circular queue buffer */
    GLUCOSE_MEASUREMENT_T     gs_meas[MAX_NUMBER_GLUCOSE_MEASUREMENTS];

    GLUCOSE_CONTEXT_T         gs_contexts[MAX_NUMBER_GLUCOSE_CONTEXT];

    /* Starting index of circular queue carrying the oldest Glucose measurement 
     * value 
     */
    uint8                     start_idx;

    /* Out-standing measurements in the queue */
    uint8                     num;

} CQUEUE_GLUCOSE_MEASUREMENT_T;

typedef struct _glucose_meas_pending
{
    uint8               cqueue_idx[MAX_NUMBER_GLUCOSE_MEASUREMENTS];
    uint8               num;
    uint8               current;

} GLUCOSE_MEAS_PENDING_T;

typedef struct
{
    /* Circular queue for storing Glucose measurement values */
    CQUEUE_GLUCOSE_MEASUREMENT_T        gs_meas_queue;

    /* Measurements pending transmission to collector. These are the 
     * measurements that have qualified to be transmitted to
     * collector as per the RACP procedure executed by 
     * the collector.
     */
    GLUCOSE_MEAS_PENDING_T              meas_pending;

    /* Boolean flag set to transfer glucose measurements post connection with 
     * the host. 
     */
    bool                                data_pending;

    /* Glucose measurement client configuration */
    gatt_client_config                  meas_client_config;

    /* Glucose measurement context client configuration */
    gatt_client_config                  context_client_config;

    /* RACP client configuration */
    gatt_client_config                  racp_client_config;

    /* NVM offset at which data is stored */
    uint16                              nvm_offset;

    /* Boolean indicating a already in progress RACP procedure */
    bool                                racp_procedure_in_progress;

    /* Boolean flag indicating if ABORT operation is in progress */
    bool                                abort_racp_in_progress;

    uint16                              seq_num;

    /* Timer for PTS, it will be used to introduce one second gap between 
     * two measurement notifications.
     */
    timer_id                            pts_tid;

    /* Variable to store the last pending Glucose Measurement Record index. */
    uint8                               last_idx;

    /* Variable to store the handle of the last sent Glucose Measurement Record 
     * notification.
     */
    uint16                              last_handle;

    /* The following variable will help implementing the flow control mechanism 
     * in the Glucose Sensor application.
     * The Current Flow Control procedure is:
     * The application will keep pumping notifications to the firmware as long 
     * as the firmware returns success in the notification confirmation events,
     * As soon as the firmware returns a failure, the application will stop 
     * sending notifications on notification confirmation and will send them only 
     * on receving Radio Tx events.
     * The following variable, when set, will trigger the application to send 
     * notification only on Radio Tx events
     */
    bool                                has_notification_failed_before;

    /* The following variable will be used in implementing flow control in the 
     * application. The boolean variable will tell the application that it 
     * should resend the last notification on receiving Radio Tx event as the 
     * notification has failed as returned by the firmware
     */
    bool                                send_the_last_notification_again;

}GLUCOSE_SERVICE_DATA_T;

/*============================================================================*
 *  Private Data
 *============================================================================*/

GLUCOSE_SERVICE_DATA_T g_glucose_data;

/*============================================================================*
 *  Private Function Declarations
 *============================================================================*/

/* This function sends the first or last record to the collector. */
static void sendFirstOrLastMeasRecord(uint16 ucid, uint8 operator);

/* This function sends the Glucose measurements based on sequence number. */
static uint16 sendMeasBasedOnSeqNum(uint16 ucid, uint8 opcode, uint8 operator, 
                        uint16 min_seq_num, uint16 max_seq_num);

/* This function sends the GLucose Record access control point indication for 
 * number of stored records procedure.
 */
static void sendRACPNumOfStoredRecordsInd(uint16 ucid, uint16 num_records);

/* This function sends the Record access control point indication for read 
 * stored records procedure.
 */
static void sendRACPResponseInd(uint16 ucid, uint8 req_code, uint8 res_value);

/* This function handles the access request on RACP character. */
static void handleRACP(GATT_ACCESS_IND_T *p_ind);

/* This function deletes the measurement records from the queue based on 
 * sequence number.
 */
static void deleteMeasRecordsBasedOnSeqNum(uint8 operator, uint16 min_seq_num,
                                           uint16 max_seq_num);

/* This function removed holes from the measurement queue.*/
static void removeHolesFromMeasurementQueue(void);

/* This function sends the Glucose context of the current record. If there is 
 * no Glucose context present, it moves to the next record.
 */
static void sendMeasContextOrMoveToNextRecord(uint16 ucid);

/* This function send the measurement notifications. */
static void sendMeasNotifications(uint16 ucid);

/* This function is only for PTS test case. It sends glucose measurement 
 * notifications with a gap of 1 seconds between two notifications.
 */
static void ptsSendMeasNotifications(timer_id tid);

/*============================================================================*
 *         Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      deleteMeasRecordsBasedOnSeqNum
 *
 *  DESCRIPTION
 *      This function is used to delete glucose measurement records in range
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void deleteMeasRecordsBasedOnSeqNum(uint8 operator, uint16 min_seq_num,
                                           uint16 max_seq_num)
{
    uint8 idx = g_glucose_data.gs_meas_queue.start_idx;
    uint8 num_elements = g_glucose_data.gs_meas_queue.num;
    uint16 start_idx_seq_num;

    /* Go through the list of measurements and mark it for deletion if the
     * sequence number falls in range
     */
    while(num_elements)
    {
        start_idx_seq_num = g_glucose_data.gs_meas_queue
                                          .gs_meas[idx].sequence_number;

        switch(operator)
        {
            case WITHIN_RANGE_OF:
            {
                /* Operator and operand validation */
                if(start_idx_seq_num >= min_seq_num &&
                    start_idx_seq_num <= max_seq_num)
                {
                    /* Set the deleted flag in measurement data */
                    g_glucose_data.gs_meas_queue.gs_meas[idx].deleted = TRUE;
                }
            }
            break;
            case LESS_THAN_OR_EQUAL_TO:
            {
                /* min_seq_num won't get used in this case */
                if(start_idx_seq_num <= max_seq_num)
                {
                    /* Set the deleted flag in measurement data */
                    g_glucose_data.gs_meas_queue.gs_meas[idx].deleted = TRUE;
                }
            }
            break;
            case GREATER_THAN_OR_EQUAL_TO:
            {
                /*max_seq_num won't get used in this case */
                if(start_idx_seq_num >= min_seq_num)
                {
                    /* Set the deleted flag in measurement data */
                    g_glucose_data.gs_meas_queue.gs_meas[idx].deleted = TRUE;
                }
            }
            break;
            default:
                /* Control should not come here */
            break;
        }

        idx = (idx + 1) % MAX_NUMBER_GLUCOSE_MEASUREMENTS;

        num_elements--;
    }

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      removeHolesFromMeasurementQueue
 *
 *  DESCRIPTION
 *      This function is removes holes from the measurement queue 
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void removeHolesFromMeasurementQueue(void)
{
    uint8 index = g_glucose_data.gs_meas_queue.start_idx;
    uint8 num_of_elements = g_glucose_data.gs_meas_queue.num;

    /* This will be final  start index after hole removal */
    uint8 final_start = g_glucose_data.gs_meas_queue.start_idx;

    /* Temporary index for  iteration*/
    uint8 temp_index = g_glucose_data.gs_meas_queue.start_idx; 

    uint8 final_num_of_elements = 0; /* Will be final num of elements after
                                      * hole removal 
                                      */
    while(num_of_elements)
    {
        /* This loop body will be executed for all the elements in the array */
        if(g_glucose_data.gs_meas_queue.gs_meas[index].deleted == TRUE )
        {
            /* This glucose measurement has been deleted */
            /* don't do anything */
        }
        else
        {
            /* This measurement is present. Move it to first empty index
             * in the final array 
             */
            g_glucose_data.gs_meas_queue.gs_meas[temp_index] = 
                    g_glucose_data.gs_meas_queue.gs_meas[index];
            /* Move the corresponding glucose measurement context also
             * to right location. If context was not measured then also, this 
             * assignment won't harm.
             */
            g_glucose_data.gs_meas_queue.gs_contexts[temp_index] = 
                    g_glucose_data.gs_meas_queue.gs_contexts[index];

            /* Move index further */
            temp_index = (temp_index + 1) % MAX_NUMBER_GLUCOSE_MEASUREMENTS;

            /* Increase the final num of elements*/
            final_num_of_elements++;
        }
        index = (index + 1) % MAX_NUMBER_GLUCOSE_MEASUREMENTS;
        num_of_elements--;
    }

    if(final_num_of_elements != 0)
    {
        /* There are non-deleted measurements */
        g_glucose_data.gs_meas_queue.start_idx = final_start;
        
        g_glucose_data.gs_meas_queue.num = final_num_of_elements;
    }
    else
    {
        /* All measurements got deleted */
        g_glucose_data.data_pending = FALSE;

        g_glucose_data.gs_meas_queue.start_idx = 0;
        g_glucose_data.gs_meas_queue.num = 0;
    }
}
/*----------------------------------------------------------------------------*
 *  NAME
 *      sendFirstOrLastMeasRecord
 *
 *  DESCRIPTION
 *      This function is used to report First (Oldest) or Last (Latest) 
 *      measurement data to the collector.
 *      
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void sendFirstOrLastMeasRecord(uint16 ucid, uint8 operator)
{
    uint8 index;

    /* Initialise measurement pending tx data */
    g_glucose_data.meas_pending.num = 0;
    g_glucose_data.meas_pending.current = 0;

    /* Send data to collector */
     if(operator == FIRST_RECORD) /* Oldest record */
    {
        index = g_glucose_data.gs_meas_queue.start_idx;
    }
    else /* LAST_RECORD or most Recent Record */
    {
        index = 
              (g_glucose_data.gs_meas_queue.start_idx +
              g_glucose_data.gs_meas_queue.num - 1) % 
              MAX_NUMBER_GLUCOSE_MEASUREMENTS;
    }

    g_glucose_data.meas_pending.cqueue_idx[0] = index;

    g_glucose_data.meas_pending.num = 1;

}
/*----------------------------------------------------------------------------*
 *  NAME
 *      ptsSendMeasNotifications
 *
 *  DESCRIPTION
 *      This function will be used only for those PTS test cases which will be 
 *      writing ABORT on RACP and will send glucose measurement notifications
 *      upon expiry of a timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void ptsSendMeasNotifications(timer_id tid)
{
    /* Send glucose notification */
    if(tid == g_glucose_data.pts_tid)
    {
        sendMeasNotifications(GetAppConnectedUcid());
    }
    /* Else Ignore. This may be due to some race condition */
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      sendMeasContextOrMoveToNextRecord
 *
 *  DESCRIPTION
 *      If a Glucose Context is present for the current Glucose Record, the 
 *      application sends it otherwise it moves to the next record.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void sendMeasContextOrMoveToNextRecord(uint16 ucid)
{
    uint8 idx;
    /* Check if collector has not aborted the ongoing procedure */
    if(!g_glucose_data.abort_racp_in_progress &&
        g_glucose_data.racp_procedure_in_progress)
    {
        idx = g_glucose_data.
                          meas_pending.cqueue_idx[g_glucose_data.last_idx];

        /* If the application had sent a Glucose Measurement 
         * notification, if there is context information 
         * present for the same record, send it. Otherwise send 
         * the next Glucose measurement notification
         */
        if((g_glucose_data.last_handle == HANDLE_GLUCOSE_MEASUREMENT) &&
           (g_glucose_data.context_client_config == 
                                          gatt_client_config_notification) &&
           g_glucose_data.gs_meas_queue.gs_contexts[idx].context_len)
        {
            /* If context is present, Send that too */
            GattCharValueNotification(ucid, 
                    HANDLE_GLUCOSE_MEASUREMENT_CONTEXT,
                    g_glucose_data.gs_meas_queue.gs_contexts[idx].context_len,
                    g_glucose_data.gs_meas_queue.gs_contexts[idx].context_data);
                 /*DebugWriteString("sendMeasContextOrMoveToNextRecord");*/
            /* Reset the last stored handle.*/
            g_glucose_data.last_handle = HANDLE_GLUCOSE_MEASUREMENT_CONTEXT;
        }
        else
        {
            /* There is no context to be sent, move to the next record.*/
                
            /* Increment the pending data index.*/
            g_glucose_data.meas_pending.current++;
    
            if(g_pts_abort_test)
            {
                /* If PTS abort test case is running and project
                 * keyr file has been configured accordingly, 
                 * introduce one secong gap between glucose 
                 * records. Start a one second timer and when it
                 * expires, send the notification over the air.
                 */
                
                 TimerDelete(g_glucose_data.pts_tid);
                 g_glucose_data.pts_tid = TimerCreate(0.1*SECOND, 
                                                       TRUE,
                                                     ptsSendMeasNotifications);
            }
            else
            {
                sendMeasNotifications(ucid);
            }
        }
    }
    else if(g_glucose_data.abort_racp_in_progress)
    {
        /*Aborting the RACP procedure */
        if(g_pts_abort_test)
        {
            /* If PTS abort test case is running and project keyr 
             * file has been configured accordingly, delete the one 
             * second gap timer if it is running.
             */
            TimerDelete(g_glucose_data.pts_tid);
            g_glucose_data.pts_tid = TIMER_INVALID;
        }
        /* Re-initialise the measurement pending data  */
        g_glucose_data.meas_pending.current = 0;
        g_glucose_data.meas_pending.num = 0;
        g_glucose_data.abort_racp_in_progress = FALSE;
        g_glucose_data.racp_procedure_in_progress = FALSE;
        sendRACPResponseInd(ucid, ABORT_OPERATION, RESPONSE_CODE_SUCCESS);
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      sendMeasNotifications
 *
 *  DESCRIPTION
 *      This function sends glucose measurement notifications
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void sendMeasNotifications(uint16 ucid)
{
    /* One notification will be sent at a time. Then we will wait for the 
     * radio event for tx data and then second notification and so on.
     * If we don't have any more notification to be send after this, we will
     * reset measurement pending data and send complete indication
     */
    uint8 idx;
    uint8 response_val = RESPONSE_CODE_SUCCESS;

    /* If there is no pending Glucose Measurements to be trasmitted, Send the 
     * RACP procedure complete indication.
     */
    if((g_glucose_data.meas_pending.num == 0) ||
       (g_glucose_data.meas_pending.current >= g_glucose_data.meas_pending.num))
    {
        /* Reset Data. */
        g_glucose_data.meas_pending.current = 0;
        g_glucose_data.meas_pending.num = 0;
        /* Send RACP response indication */
        sendRACPResponseInd(ucid, REPORT_STORED_RECORDS, response_val);
    }
    else if(g_glucose_data.meas_pending.num)
    {
        if(g_glucose_data.meas_client_config == gatt_client_config_notification)
        {
            /* If notifications are enabled, Send notification*/
            idx = g_glucose_data.meas_pending.cqueue_idx
                        [g_glucose_data.meas_pending.current];


            GattCharValueNotification(ucid, HANDLE_GLUCOSE_MEASUREMENT, 
                          g_glucose_data.gs_meas_queue.gs_meas[idx].meas_len,
                          g_glucose_data.gs_meas_queue.gs_meas[idx].meas_data);

            g_glucose_data.last_idx = g_glucose_data.meas_pending.current;
            g_glucose_data.last_handle = HANDLE_GLUCOSE_MEASUREMENT;
        }
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      sendMeasBasedOnSeqNum
 *
 *  DESCRIPTION
 *      This function sends measurements using operand sequence number.
 *
 *  RETURNS/MODIFIES
 *      uint16 Number of records with matching criteria
 *
 *----------------------------------------------------------------------------*/
static uint16 sendMeasBasedOnSeqNum(uint16 ucid, uint8 opcode, uint8 operator, 
                                    uint16 min_seq_num, uint16 max_seq_num)
{
    uint8 idx = g_glucose_data.gs_meas_queue.start_idx;
    uint8 num_elements = g_glucose_data.gs_meas_queue.num;
    uint16 start_idx_seq_num;
    uint16 num_of_records = 0;

    /* Initialise measurement pending tx data */
    g_glucose_data.meas_pending.num = 0;
    g_glucose_data.meas_pending.current = 0;

    /* check every stores glucose measurement against the criteria */
    while(num_elements)
    {
        start_idx_seq_num = g_glucose_data.gs_meas_queue
                            .gs_meas[idx].sequence_number;

        /* Operator and operand validation */
        if(((operator == ALL_RECORDS) ||
            (operator == LESS_THAN_OR_EQUAL_TO && 
             start_idx_seq_num <= min_seq_num) ||
            (operator == GREATER_THAN_OR_EQUAL_TO && 
             start_idx_seq_num >= min_seq_num) ||
            (operator == WITHIN_RANGE_OF && 
             start_idx_seq_num >= min_seq_num &&
             start_idx_seq_num <= max_seq_num)) &&
             (g_glucose_data.gs_meas_queue.gs_meas[(idx)].deleted == FALSE))
        {

            /* Send data to collector */
            if(opcode == REPORT_STORED_RECORDS)
            {
                /* store the index in the pending measurement record */
                g_glucose_data.meas_pending.cqueue_idx[num_of_records] = idx;
            }

            num_of_records++;

        }

        idx = (idx + 1) % MAX_NUMBER_GLUCOSE_MEASUREMENTS;

        num_elements--;
    }

    if(opcode == REPORT_STORED_RECORDS)
    {
        /* if reporting of records has been requested then store the the 
         * count in number of pending records to be transmitted 
         */
        g_glucose_data.meas_pending.num = num_of_records;
    }

    return num_of_records;

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      sendRACPNumOfStoredRecordsInd
 *
 *  DESCRIPTION
 *      This function sends RACP number of stored records Indication to the 
 *      glucose collector.
 *
 *  RETURNS/MODIFIES
 *      Noting
 *
 *----------------------------------------------------------------------------*/
static void sendRACPNumOfStoredRecordsInd(uint16 ucid, uint16 num_records)
{
    if(g_glucose_data.racp_client_config == gatt_client_config_indication)
    {
        uint8 value[4];

        value[0] = NUMBER_OF_STORED_RECORDS_RESPONSE;
        value[1] = 0x00; /* NULL operator */
        value[2] = LE8_L(num_records);
        value[3] = LE8_H(num_records);

        GattCharValueIndication(ucid, HANDLE_RECORD_ACCESS_CONTROL_POINT,
                                4, value);
    }

    /* RACP procedure is complete after app sends RACP indication to remote side
     * Set RACP procedure in progress flag to FASLE 
     */
    g_glucose_data.racp_procedure_in_progress = FALSE;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      sendRACPResponseInd
 *
 *  DESCRIPTION
 *      This function sends RACP Response Indication to the Glucose Collector.
 *
 *  RETURNS/MODIFIES
 *      Noting
 *
 *----------------------------------------------------------------------------*/
static void sendRACPResponseInd(uint16 ucid, uint8 req_code, uint8 res_value)
{
    /* Disable radio events. */
    LsRadioEventNotification(ucid, radio_event_none);

    if(g_glucose_data.racp_client_config == gatt_client_config_indication)
    {
        uint8 value[4];

        value[0] = RESPONSE_CODE;
        value[1] = 0x00; /* NULL operator */
        value[2] = req_code;
        value[3] = res_value;

        GattCharValueIndication(ucid, HANDLE_RECORD_ACCESS_CONTROL_POINT,
                                4, value);
    }
    /* RACP procedure is complete after app sends RACP indication to remote side
     * Set RACP procedure in progress flag to FASLE 
     */
    g_glucose_data.racp_procedure_in_progress = FALSE;
    g_glucose_data.last_idx = -1;
    g_glucose_data.last_handle = INVALID_ATT_HANDLE;
    g_glucose_data.has_notification_failed_before = FALSE;
    g_glucose_data.send_the_last_notification_again = FALSE;

    /* Restart the idle timer. If collector does not execute any more RACP
     * procedure in next CONNECTED_IDLE_TIMEOUT_VALUE, we will disconnect
     */
    ResetIdleTimer();
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleDeleteStoredRecordsInfo
 *
 *  DESCRIPTION
 *      This function the deletion operation on stored records.
 *
 *  RETURNS/MODIFIES
 *      Noting
 *
 *---------------------------------------------------------------------------*/
static void handleDeleteStoredRecordsInfo(GATT_ACCESS_IND_T *p_ind)
{
    uint8 *p_value = p_ind->value;
    uint8 opcode = BufReadUint8(&p_value);
    uint8 operator = BufReadUint8(&p_value);
    uint8 response_val = RESPONSE_CODE_SUCCESS;
    uint8 filter_type;
    uint16 min_seq_num = 0;
    uint16 max_seq_num = 0;

    if(g_glucose_data.meas_pending.num)
    {
        /* There are pending measurements to be sent to collector.
         * control should not come here 
         */
        response_val = PROCEDURE_NOT_COMPLETED;
    }
    else
    {
        if(operator == ALL_RECORDS)
        {
            /* Deletion of all records has been requested 
             * Set number of stored measurements to zero and
             * starting index of measurements to zero
             */
            g_glucose_data.data_pending = FALSE;

            g_glucose_data.gs_meas_queue.start_idx = 0;
            g_glucose_data.gs_meas_queue.num = 0;
        }
        else if(operator == WITHIN_RANGE_OF)
        {
            /* Deletion of records with in some range has been requested */
            filter_type = BufReadUint8(&p_value);

            if(filter_type == SEQUENCE_NUMBER)
            {
                /*Sequence number supported */
                min_seq_num = BufReadUint16(&p_value);
                max_seq_num = BufReadUint16(&p_value);

                deleteMeasRecordsBasedOnSeqNum(operator, min_seq_num, max_seq_num);
            }
            else
            {
                /* Only sequence number filtering is supported */
                response_val = FILTER_TYPE_NOT_SUPPORTED;
            }
        }
        else if(operator == LESS_THAN_OR_EQUAL_TO)
        {
            /* Deletion of records less than or equal to a certain
             * value has been requested.
             */
            filter_type = BufReadUint8(&p_value);

            if(filter_type == SEQUENCE_NUMBER)
            {
                /*Sequence number supported */
                max_seq_num = BufReadUint16(&p_value);
                min_seq_num = 0;    /* Won't be used in this case */
                deleteMeasRecordsBasedOnSeqNum(operator, min_seq_num, 
                                               max_seq_num);
            }
            else
            {
                /* Only sequence number filtering is supported */
                response_val = FILTER_TYPE_NOT_SUPPORTED;
            }
        }
        else if(operator == GREATER_THAN_OR_EQUAL_TO)
        {
            /* deletion of records with filter greater than or equal to a
             * a certain value has been requested 
             */
              filter_type = BufReadUint8(&p_value);

            if(filter_type == SEQUENCE_NUMBER)
            {
                /*Sequence number Supported */
                min_seq_num  = BufReadUint16(&p_value);
                max_seq_num = 0;    /* Won't be used in this case */
                deleteMeasRecordsBasedOnSeqNum(operator, min_seq_num,
                                               max_seq_num);
            }
            else
            {
                /* Only sequence number filtering is supported */
                response_val = FILTER_TYPE_NOT_SUPPORTED;
            }
        }
        else if(operator == FIRST_RECORD )
        {
            if(g_glucose_data.gs_meas_queue.num == 1)
            {
                /*all Records has been deleted */
                g_glucose_data.data_pending = FALSE;

                g_glucose_data.gs_meas_queue.start_idx = 0;
                g_glucose_data.gs_meas_queue.num =0;
            }
            else if(g_glucose_data.gs_meas_queue.num != 0)
            {
                /* Set the deleted flag to TRUE and increment starting index
                 *  by 1 and decrease the number of elements 
                 */
                g_glucose_data.gs_meas_queue.gs_meas
                          [g_glucose_data.gs_meas_queue.start_idx].deleted 
                                                                    = TRUE;


                g_glucose_data.gs_meas_queue.start_idx = 
                        (g_glucose_data.gs_meas_queue.start_idx + 1) % 
                                            MAX_NUMBER_GLUCOSE_MEASUREMENTS;
                g_glucose_data.gs_meas_queue.num--;
            }

        }
        else if (operator == LAST_RECORD)
        {
            if(g_glucose_data.gs_meas_queue.num == 1)
            {
                /*all Records has been deleted */
                g_glucose_data.data_pending = FALSE;

                g_glucose_data.gs_meas_queue.start_idx = 0;
                g_glucose_data.gs_meas_queue.num =0;
            }
            else if(g_glucose_data.gs_meas_queue.num != 0)
            {
                /* Set the deleted flag to TRUE and 
                 * and decrease the number of elements 
                 */
                g_glucose_data.gs_meas_queue.num--;
            }
        }
        else
        {
            /* No other operator supported for deletion  */
            response_val = OPERATOR_NOT_SUPPORTED;
        }

    }

    removeHolesFromMeasurementQueue();
    /* Send RACP response indication */
    sendRACPResponseInd(p_ind->cid, opcode, response_val);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleReportStoredRecordsInfo
 *
 *  DESCRIPTION
 *      This function handles two RACP procedures
 *      1. REPORT_STORED_RECORDS
         2. REPORT_NUMBER_OF_STORED_RECORDS
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void handleReportStoredRecordsInfo(GATT_ACCESS_IND_T *p_ind)
{
    uint8 *p_value = p_ind->value;
    uint8 opcode = BufReadUint8(&p_value);
    uint8 operator = BufReadUint8(&p_value);
    uint8 size_param = p_ind->size_value;
    uint8 response_val = RESPONSE_CODE_SUCCESS;
    uint8 filter_type;
    uint16 min_seq_num = 0;
    uint16 max_seq_num = 0;
    uint16 num_records = 0;

    switch(operator)
    {
        case OPERATOR_NULL:
        {
            /* This operator is not valid for this opcode procedure */
            response_val = INVALID_OPERATOR;
        }
        break;
        
        case ALL_RECORDS:
        {
            if(size_param == 2)
            {
                /* Flush all records, but don't delete them */
                num_records = sendMeasBasedOnSeqNum(p_ind->cid, opcode, 
                                        ALL_RECORDS, 0, 0);
            }
            else
            {
                response_val = INVALID_OPERAND;
            }
        }
        break;

        case LESS_THAN_OR_EQUAL_TO:
            /* FALLTHROUGH */
        case GREATER_THAN_OR_EQUAL_TO:
            /* FALLTHROUGH */
        case WITHIN_RANGE_OF:
        {
            filter_type = BufReadUint8(&p_value);
            switch(filter_type)
            {
                case SEQUENCE_NUMBER:
                {
                    min_seq_num = BufReadUint16(&p_value);
                    max_seq_num = 0;

                    if(operator == WITHIN_RANGE_OF)
                    {
                        max_seq_num = BufReadUint16(&p_value);

                        if(min_seq_num > max_seq_num)
                        {
                            /* Send invalid operand error response */
                            response_val = INVALID_OPERAND;
                            break;
                        }
                    }
                    /* send measurement records based on sequence number */
                    num_records = sendMeasBasedOnSeqNum(p_ind->cid, opcode, 
                                          operator, min_seq_num, max_seq_num);
                }
                break;

                default:
                {
                    /* Operand not supported */
                    response_val = FILTER_TYPE_NOT_SUPPORTED;
                }
                break;
            }
        }
        break;
        
        case FIRST_RECORD: /* FALLTHROUGH */
        case LAST_RECORD:
        {
            /* First or last record has been requested */
            num_records = 0;
            if(g_glucose_data.gs_meas_queue.num)
            {
                /* Send oldest or latest record using the existing function */
                if(opcode == REPORT_STORED_RECORDS)
                    sendFirstOrLastMeasRecord(p_ind->cid, operator);

                num_records = 1;
            }

        }
        break;

        default:
        {
            /* No more operators */
            response_val = OPERATOR_NOT_SUPPORTED;
        }
        break;
    }


    /* Case when there are no other errors but no records are found 
     * for reporting
     */
    if((response_val == RESPONSE_CODE_SUCCESS) &&
       (num_records == 0) &&
       (opcode == REPORT_STORED_RECORDS))
    {
        /* Send no records found error response */
        response_val = NO_RECORDS_FOUND;
    }
    

    if(response_val == RESPONSE_CODE_SUCCESS)
    {
        if(opcode == REPORT_NUMBER_OF_STORED_RECORDS)
        {
            /* Send RACP number of Store records indication */
            sendRACPNumOfStoredRecordsInd(p_ind->cid, num_records);
        }
        else
        {
            /* The application is starting transmission of the Glucose 
             * measurements. Reset the flow control variables.
             */
            g_glucose_data.has_notification_failed_before = FALSE;
            g_glucose_data.send_the_last_notification_again = FALSE;
            sendMeasNotifications(p_ind->cid);
        }
    }
    else
    {
        /* Send RACP response indication */
        sendRACPResponseInd(p_ind->cid, opcode, response_val);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleRACP
 *
 *  DESCRIPTION
 *      This function handles a RACP procedure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void handleRACP(GATT_ACCESS_IND_T *p_ind)
{
    uint8 *p_value = p_ind->value;
    uint8 opcode = BufReadUint8(&p_value);
    uint8 operator = BufReadUint8(&p_value);
    uint8 resp_value = RESPONSE_CODE_SUCCESS;
    sys_status rc = sys_status_success;

    if(g_glucose_data.racp_procedure_in_progress &&
        (opcode != ABORT_OPERATION))
    {
        /* App is already processing a RACP procedure and 
         * new RACP procedure requested is not ABORT operation
         * So it will reject it 
         */
        /* As Gatt status codes are not unified with sys status type, therefore
         * each application error code should now be ORed with 
         * gatt_status_app_mask 
         */
        rc = PROCEDURE_ALREADY_IN_PROGRESS;
    }
    else if((g_glucose_data.racp_client_config != 
                            gatt_client_config_indication) ||
        ((opcode == REPORT_STORED_RECORDS) &&
        (g_glucose_data.meas_client_config != gatt_client_config_notification))
               )
    {
        /* IF RACP client configuration descriptor is not configured for 
         * indications or if opcode is REPORT_STORED_RECORDS but glucose 
         * measurement characteristic client configuration descriptor is not 
         * configured then return error 
         */
        rc = CLIENT_CHAR_CONFIG_DESC_IMPROPER_CONFIGURED;
    }

    /* Send ACCESS RESPONSE */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);
    if(rc == sys_status_success)
    {
        /* Glucose collector has initiated a RACP procedure.
         * Delete the idle timer which we had started earlier
         */
        DeleteIdleTimer();

        switch(opcode)
        {
            case REPORT_STORED_RECORDS: /* FALLTHROUGH */
            case REPORT_NUMBER_OF_STORED_RECORDS:
            {
                /* Set the RACP procedure already in progress flag and
                 * start processing the request 
                 */
                g_glucose_data.racp_procedure_in_progress = TRUE;
                handleReportStoredRecordsInfo(p_ind); 
            }
            break;

            case DELETE_STORED_RECORDS:
            {
                /* Set the RACP procedure already in progress flag and
                 * start processing the request 
                 */
                g_glucose_data.racp_procedure_in_progress = TRUE;
                handleDeleteStoredRecordsInfo(p_ind);
            }
            break;

            case ABORT_OPERATION: /* Not supported */
            {
                if(operator != OPERATOR_NULL)
                {
                    resp_value = INVALID_OPERATOR;
                }

                /* Set RACP procedure in progress flag as we don't want to
                 * any other RACP procedure to run when ABORT operation
                 * is going on.
                 */
                if(g_glucose_data.racp_procedure_in_progress && 
                    resp_value == RESPONSE_CODE_SUCCESS)
                {
                    g_glucose_data.abort_racp_in_progress = TRUE;
                }
                else
                {
                    g_glucose_data.abort_racp_in_progress = FALSE;
                    sendRACPResponseInd(p_ind->cid, opcode, resp_value);
                }
            }
            break;
            default:
            {
                sendRACPResponseInd(p_ind->cid, opcode, OPCODE_NOT_SUPPORTED);
                break;
            }
        }
    }
}


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialize Glucose service data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseDataInit(void)
{

    if(!AppIsDeviceBonded())
    {
        /* Initialize glucose measurement, Glucose context information and 
         * Glucose RACP characteristic client configuration only if device 
         * is not bonded
         */
        g_glucose_data.meas_client_config= gatt_client_config_none;
        g_glucose_data.context_client_config = gatt_client_config_none;
        g_glucose_data.racp_client_config = gatt_client_config_none;
    }

    /* Initialize measurement pending data */
    g_glucose_data.meas_pending.num = 0;
    g_glucose_data.meas_pending.current = 0;
    g_glucose_data.racp_procedure_in_progress = FALSE;
    g_glucose_data.abort_racp_in_progress = FALSE;
    g_glucose_data.last_idx = -1;
    g_glucose_data.last_handle = INVALID_ATT_HANDLE;

    if(g_pts_abort_test)
    {
        /* If PTS abort test case is running and keyr file has been configured
         * for this variable, delete this timer.
         */
        TimerDelete(g_glucose_data.pts_tid);
        g_glucose_data.pts_tid = TIMER_INVALID;
    }


}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseSeqNumInit
 *
 *  DESCRIPTION
 *      This function is used to initialise Glucose service sequence number 
 *      This function will only be called when NVM will be initialized at the
 *      time of first device bring up.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseSeqNumInit(uint16 offset)
{
    g_glucose_data.seq_num = 0;
    /* Write glucose service sequence number to NVM */
    Nvm_Write(&g_glucose_data.seq_num, sizeof(g_glucose_data.seq_num),
               offset);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseInitChipReset
 *
 *  DESCRIPTION
 *      This function is used to initialize Glucose service data 
 *      structure at chip reset
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseInitChipReset(void)
{
    uint16 i = 0;

    /* Initialise circular queue buffer */
    g_glucose_data.gs_meas_queue.start_idx = 0;
    g_glucose_data.gs_meas_queue.num =0;
    g_glucose_data.data_pending = FALSE;

    for(i=0; i<MAX_NUMBER_GLUCOSE_MEASUREMENTS; i++)
    {
        /* Set the deleted flag in measurement data */
        g_glucose_data.gs_meas_queue.gs_meas[i].deleted= TRUE;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IsGlucoseDataPending
 *
 *  DESCRIPTION
 *      This function is used to check if Application has data pending to be
 *      notified to the collector
 *
 *  RETURNS/MODIFIES
 *      Boolean TRUE if data pending
 *              FALSE.if no data pending
 *
 *----------------------------------------------------------------------------*/
extern bool IsGlucoseDataPending(void)
{
    return (g_glucose_data.data_pending == TRUE);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AddGlucoseMeasurementToQueue
 *
 *  DESCRIPTION
 *      This function is used to add glucose measurements  to circular queue.
 *      The glucose measurements will get notified to the collector once
 *      notifications are enabled by the remote client and he requests using 
 *      RACP procedures.
 *
 *  Arguments
 *  measurementFlag: flag field of Glucose measurement characteristic. 
 *  Different possible flag combinations are made available to the application
 *  from glucose_service.h.
 *
 *  measurementData: pointer to array containing the optional fields of 
 *  Glucose measurement characteristic as per the flags field.
 *  The array may contain glucose concentration, 
 *  type-sample location, sensor status annunciation. The different values 
 *  for these fields are made available to
 *  the application from glucose_service.h.
 *
 *  context_flag: flag field of glucose measurement context characteristic.
 *  Different possible flag 
 *  combinations are made available to the application from glucose_service.h.
 *
 *  context_data: pointer to array containing the optional fields of 
 *  Glucose measurement context characteristic as per the flags field.
 *  The array may contain extended flags field, Carbohydrate
 *  ID field, Carbohydrate value, Meal, Tester-Health, Exercise Duration, 
 *  Exercise Intensity, Medication ID, medication, HbA1c.
 *  The different values for these fields are made available 
 *  to the application from glucose_service.h.
 *
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void AddGlucoseMeasurementToQueue(
                uint8 meas_flag,uint8 *meas_data,uint16 meas_len,
                uint8 context_flag, uint8 *context_data, uint16 context_len, TIME_UNIX_CONV *tm)
{
    uint8 *temp_measurement_data = NULL;
    uint8 *temp_context_data = NULL;
    uint16 add_idx, data_len = 0;
    uint16 offset = g_glucose_data.nvm_offset + 
                                  NVM_GLUCOSE_SEQ_NUM;

    /* Increment sequence number */
    g_glucose_data.seq_num++;

    /* Write glucose service sequence number to NVM */
    Nvm_Write(&g_glucose_data.seq_num, sizeof(g_glucose_data.seq_num),
                                                                offset);

    /* Add new data to the end of circular queue. If max circular queue 
     * length has reached the oldest measurement will get overwritten 
     */
    add_idx = 
        (g_glucose_data.gs_meas_queue.start_idx +
            g_glucose_data.gs_meas_queue.num)% 
                                    MAX_NUMBER_GLUCOSE_MEASUREMENTS;

    /* ******* Fill glucose measurement data ******* */
    temp_measurement_data = g_glucose_data.gs_meas_queue
                                              .gs_meas[add_idx].meas_data;

    /* Add measurement flag */
    temp_measurement_data[data_len ++] = meas_flag;

    /* Add sequence number */
    g_glucose_data.gs_meas_queue
            .gs_meas[add_idx].sequence_number = g_glucose_data.seq_num;

    temp_measurement_data[data_len ++] = LE8_L(g_glucose_data.seq_num);
    temp_measurement_data[data_len ++] = LE8_H(g_glucose_data.seq_num);

    /* Add base time to glucose measurement data */
    temp_measurement_data[data_len ++] = LE8_L(tm->tm_year); /* Year */
    temp_measurement_data[data_len ++] = LE8_H(tm->tm_year);
    temp_measurement_data[data_len ++] = tm->tm_mon; /* Month 0 to 12 */
    temp_measurement_data[data_len ++] = tm->tm_mday; /* Day   1 to 31 */
    temp_measurement_data[data_len ++] = 
                tm->tm_hour; /* Hour  0 to 23 */
    temp_measurement_data[data_len ++] = 
                tm->tm_min; /* Min   0 to 59 */
    temp_measurement_data[data_len ++] = 
                tm->tm_sec; /* Secs  0 to 59 */

    /* Add optional data based upon the flag set */
    if(meas_len)
        MemCopy((uint8*)(temp_measurement_data + data_len), meas_data,
                    meas_len);

    g_glucose_data.gs_meas_queue.gs_meas[add_idx].meas_len = 
                                                    meas_len + data_len;

    /* Set the deleted flag in measurement data */
    g_glucose_data.gs_meas_queue.gs_meas[add_idx].deleted= FALSE;

    /* ******* Fill glucose context information data ******* */

    /* Reset 'dataLen' variable */
    data_len = 0;

    if(context_len)
    {
        temp_context_data = 
            g_glucose_data.gs_meas_queue.gs_contexts[add_idx].context_data;

        /* Add context information flag */
        temp_context_data[data_len ++] = context_flag;

        /* Add sequence number */
        g_glucose_data.gs_meas_queue.gs_contexts[add_idx].sequence_number = 
                                                g_glucose_data.seq_num;
 
        temp_context_data[data_len ++] = LE8_L(g_glucose_data.seq_num);
        temp_context_data[data_len ++] = LE8_H(g_glucose_data.seq_num);

        /* Add optional data based upon the flag set */
        if(context_len)
            MemCopy((uint8*)(temp_context_data + data_len), context_data,
                        context_len);

    }

    g_glucose_data.gs_meas_queue.gs_contexts[add_idx].context_len = 
                                                        context_len + data_len;

    if(g_glucose_data.gs_meas_queue.num
                                    < MAX_NUMBER_GLUCOSE_MEASUREMENTS)
    {
        g_glucose_data.gs_meas_queue.num++;
    }
    else /* Oldest glucose measurement overwritten, move the index */
    {
        g_glucose_data.gs_meas_queue.start_idx =
            (add_idx + 1) % MAX_NUMBER_GLUCOSE_MEASUREMENTS;
    }

    g_glucose_data.data_pending = TRUE;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on Glucose service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  *p_value = NULL, val[2];
    sys_status rc = sys_status_success;


    switch(p_ind->handle)
    {
        case HANDLE_GLUCOSE_MEASUREMENT_CLIENT_CONFIG:
        {
            /* Glucose measurement client configuration descriptor read has been
             * requested 
             */
            p_value = val;
            BufWriteUint16(&p_value, g_glucose_data.meas_client_config);
            length = 2;
        }
        break;

        case HANDLE_GLUCOSE_MEASUREMENT_CONTEXT_CLIENT_CONFIG:
        {
            /* Glucose measurement context client configuration descriptor
             * is being read 
             */
            p_value = val;
            BufWriteUint16(&p_value, g_glucose_data.context_client_config);
            length = 2;
        }
        break;

        case HANDLE_RACP_CLIENT_CONFIG:
        {
            /* RACP client configuration descriptor is being read */
            p_value = val;
            BufWriteUint16(&p_value, g_glucose_data.racp_client_config);
            length = 2;
        }
        break;

        default:
        {
            rc = gatt_status_read_not_permitted;
        }
        break;

    }
    
    /* Send back the access response */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, val);

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on Glucose service attributes 
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint16 client_config, offset;
    uint8 *p_value = p_ind->value;
    sys_status rc = sys_status_success;
    bool racpFlag = FALSE;

    switch(p_ind->handle)
    {
        case HANDLE_GLUCOSE_MEASUREMENT_CLIENT_CONFIG:
        {

            client_config = BufReadUint16(&p_value);

            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                g_glucose_data.meas_client_config= client_config;

                offset =  g_glucose_data.nvm_offset + 
                              NVM_MEASUREMENT_CLIENT_CONFIG_OFFSET;

               /* Write Glucose measurement characteristic client configuration
                * to NVM if the devices are bonded.
                */
                 if(AppIsDeviceBonded())
                 {
                     Nvm_Write((uint16 *)&client_config,
                              sizeof(client_config),
                              offset);
                 }
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return Error as only notifications are supported for 
                 * glucose measurement characteristic 
                 */

                rc = gatt_status_app_mask;
            }
        }
        break;

        case HANDLE_GLUCOSE_MEASUREMENT_CONTEXT_CLIENT_CONFIG:
        {
            
            client_config = BufReadUint16(&p_value);
            
            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                g_glucose_data.context_client_config = client_config;

                offset = g_glucose_data.nvm_offset + 
                              NVM_CONTEXT_CLIENT_CONFIG_OFFSET;

                /* Write glucose measurement context characteristic client 
                 * configuration to NVM if the devices are bonded.
                 */
                 if(AppIsDeviceBonded())
                 {
                     Nvm_Write((uint16 *)&client_config,
                              sizeof(client_config),
                              offset);
                 }
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return error as only Notifications are supported for 
                 * glucose measurement context characteristic 
                 */

                rc = gatt_status_app_mask;
            }
        }
        break;

        case HANDLE_RACP_CLIENT_CONFIG:
        {
            client_config = BufReadUint16(&p_value);
            
            if((client_config == gatt_client_config_indication) ||
               (client_config == gatt_client_config_none))
            {

                g_glucose_data.racp_client_config = client_config;

                offset = g_glucose_data.nvm_offset + 
                              NVM_RACP_CLIENT_CONFIG_OFFSET;

               /* Write glucose measurement context characteristic client 
                * configuration to NVM if the devices are bonded.
                */
                 if(AppIsDeviceBonded())
                 {
                     Nvm_Write((uint16 *)&client_config,
                              sizeof(client_config),
                              offset);
                 }
            }
            else
            {
                /* NOTIFICATION or RESERVED */

                /* Return error as only indications are supported for 
                 * record access control point characteristic 
                 */

                rc = gatt_status_app_mask;
            }
        }
        break;

        case HANDLE_RECORD_ACCESS_CONTROL_POINT:
        {
            racpFlag = TRUE;
        }
        break;

        default:
        {
            rc = gatt_status_write_not_permitted;
        }
        break;
    }

    /* If this write request is not for RACP control point, 
     * Send the response right now. If it is for RACP control point,
     * This will be handled in function handleRACP 
     */
    if(racpFlag != TRUE)
    {
        /* Send ACCESS RESPONSE */
        GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);
    }
    else
    {
        handleRACP(p_ind);
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read Glucose service specific data stored in 
 *      NVM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseReadDataFromNVM(bool bonded, uint16 *p_offset)
{
    g_glucose_data.nvm_offset = *p_offset;

    /* Read sequence number */
    Nvm_Read(&g_glucose_data.seq_num,
                   sizeof(g_glucose_data.seq_num),
                   g_glucose_data.nvm_offset + NVM_GLUCOSE_SEQ_NUM);
        
    /* Read NVM only if devices are bonded */
    if(bonded)
    {
        /* Read glucose measurement client configuration */
        Nvm_Read((uint16 *)&g_glucose_data.meas_client_config,
                   sizeof(g_glucose_data.meas_client_config),
                   g_glucose_data.nvm_offset + 
                   NVM_MEASUREMENT_CLIENT_CONFIG_OFFSET);

        /* Read glucose context information client configuration */
        Nvm_Read((uint16 *)&g_glucose_data.context_client_config,
                   sizeof(g_glucose_data.context_client_config),
                   g_glucose_data.nvm_offset + 
                   NVM_CONTEXT_CLIENT_CONFIG_OFFSET);

        /* Read glucose RACP client configuration */
        Nvm_Read((uint16 *)&g_glucose_data.racp_client_config,
                   sizeof(g_glucose_data.racp_client_config),
                   g_glucose_data.nvm_offset + 
                   NVM_RACP_CLIENT_CONFIG_OFFSET);
    }

    /* Increment the offset by the number of words of NVM memory required 
     * by service.
     */
    *p_offset += GLUCOSE_SERVICE_NVM_MEMORY_WORDS;

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseHandleSignalLsRadioEventInd
 *
 *  DESCRIPTION
 *      This function handles the radio event for Tx data..
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseHandleSignalLsRadioEventInd(uint16 ucid)
{
    uint8 idx;
    
    if(g_glucose_data.has_notification_failed_before)
    {
        if(!g_glucose_data.abort_racp_in_progress &&
            g_glucose_data.racp_procedure_in_progress &&
            g_glucose_data.send_the_last_notification_again)
        {
            /* The last notification sending had failed, send it again. */
            g_glucose_data.send_the_last_notification_again = FALSE;

            idx = g_glucose_data.meas_pending.cqueue_idx
                                                    [g_glucose_data.last_idx];
            if(g_glucose_data.last_handle == HANDLE_GLUCOSE_MEASUREMENT)
            {
                GattCharValueNotification(ucid, 
                            HANDLE_GLUCOSE_MEASUREMENT,
                            g_glucose_data.gs_meas_queue.gs_meas[idx].meas_len, 
                            g_glucose_data.gs_meas_queue.gs_meas[idx].meas_data);
            }
            else if(g_glucose_data.last_handle == 
                                            HANDLE_GLUCOSE_MEASUREMENT_CONTEXT)
            {
                GattCharValueNotification(ucid, 
                            HANDLE_GLUCOSE_MEASUREMENT_CONTEXT,
                            g_glucose_data.gs_meas_queue.
                                                  gs_contexts[idx].context_len, 
                            g_glucose_data.gs_meas_queue.
                                                 gs_contexts[idx].context_data);
            }
        }
        else
        {
            /* Send the next context or record. */
            sendMeasContextOrMoveToNextRecord(ucid);
        }
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseHandleSignalGattCharValNotCfm
 *
 *  DESCRIPTION
 *      This function handles the notification confirmation for the 
 *      notification sent.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseHandleSignalGattCharValNotCfm(GATT_CHAR_VAL_IND_CFM_T 
                                                                *p_event_data)
{
    uint16 ucid = p_event_data->cid;
    /* The application shall keep sending notifications to the chip firmware as 
     * long as the firmware has buffer, this will ensure good throughput. As 
     * soon as the firmware buffer gets full, the notification sending will fail 
     * and the firmware will return a failure in the corresponding notification 
     * confirmation. The application, at this point, shall apply flow control 
     * and shall send notifications only on receiving Radio Tx events. A radio
     * Tx event indicates a successful tranmission of data which means a buffer
     * would have got free, so the application can send one more notification. 
     * When the application is done with the notification sending, it shall 
     * disable the radio tx events and resume normal functioning.
     */

    if(!g_glucose_data.has_notification_failed_before)
    {
        /* No notification sending has failed so far, the application shall 
         * continue sending notification to ensure good throughput.
         */

        if(p_event_data->handle == HANDLE_GLUCOSE_MEASUREMENT ||
           p_event_data->handle == HANDLE_GLUCOSE_MEASUREMENT_CONTEXT)
        {
            /* If the firmware has returned a success in the notification 
             * confirmation, the application shall send the next notification.
             */
            if(p_event_data->result == sys_status_success)
            {
                sendMeasContextOrMoveToNextRecord(ucid);
            }
            else
            {
                /* Enable radio events on Tx data. */
                LsRadioEventNotification(p_event_data->cid, 
                                         radio_event_tx_data);
                g_glucose_data.has_notification_failed_before = TRUE;
                g_glucose_data.send_the_last_notification_again = TRUE;
            }

        }
    }
    /* The application need not do anything in the else part. It will keep 
     * sending notifcations based on Radio Tx events.
     */

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the Glucose 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/
extern bool GlucoseCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_GLUCOSE_SERVICE) &&
            (handle <= HANDLE_GLUCOSE_SERVICE_END))
            ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GlucoseBondingNotify
 *
 *  DESCRIPTION
 *      This function is used by application to notify bonding status to 
 *      Glucose service
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void GlucoseBondingNotify(bool bond_status)
{
    /* Write data to NVM if bond is established */
    if(bond_status)
    {
        /* Write to NVM the client configuration values of glucose measurement,
         * glucose context information and RACP
         */
        uint16 client_config = g_glucose_data.meas_client_config;
        uint16 offset = g_glucose_data.nvm_offset + 
                                  NVM_MEASUREMENT_CLIENT_CONFIG_OFFSET;

        Nvm_Write((uint16 *)&client_config, sizeof(client_config), offset);

        client_config = g_glucose_data.context_client_config;
        offset = g_glucose_data.nvm_offset + 
                                 NVM_CONTEXT_CLIENT_CONFIG_OFFSET;

        Nvm_Write((uint16 *)&client_config, sizeof(client_config), offset);

        client_config = g_glucose_data.racp_client_config;
        offset = g_glucose_data.nvm_offset + 
                                 NVM_RACP_CLIENT_CONFIG_OFFSET;

        Nvm_Write((uint16 *)&client_config, sizeof(client_config), offset);
    }
}

