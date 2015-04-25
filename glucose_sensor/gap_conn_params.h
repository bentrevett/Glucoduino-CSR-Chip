/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 * FILE
 *      gap_conn_params.h
 *
 * DESCRIPTION
 *      MACROs for connection parameter values
 *
 ******************************************************************************/

#ifndef __GAP_CONN_PARAMS_H__
#define __GAP_CONN_PARAMS_H__

/*============================================================================*
 *  Public Definitions
 *============================================================================*/


/* Advertising parameters, time is expressed in microseconds and the firmware
 * will round this down to the nearest slot. Acceptable range is 20ms to 10.24s
 * and the minimum must be no larger than the maximum. This value needs to be 
 * modified at later stage as decided GPA for specific profile.
 *
 * For Glucose sensors, to enable fast connections though the recommended 
 * range is between 20 ms to 30 ms, but it has been observed that it is way too 
 * energy expensive for glucose sensors running on coin cell battery. So, we
 * have decided to use 60 ms as the fast connection advertisement interval. For 
 * reduced power connections, the recommended range is between 1s to 2.5 s. 
 * Vendors will need to tune these values as per their requirements.
 */
#define FC_ADVERTISING_INTERVAL_MIN    (60 * MILLISECOND)
#define FC_ADVERTISING_INTERVAL_MAX    (60 * MILLISECOND)
    
#define RP_ADVERTISING_INTERVAL_MIN    (1280 * MILLISECOND)
#define RP_ADVERTISING_INTERVAL_MAX    (1280 * MILLISECOND)


#ifndef NO_IDLE_TIMEOUT
/* Idle timer value in connected state. At the expiry of this timer, the 
 * glucose sensor will disconnect itself from the Host.
 */
#define CONNECTED_IDLE_TIMEOUT_VALUE   (30 * MINUTE)
#endif /* NO_IDLE_TIMEOUT */

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand  brackets 
 * and will raise syntax errors.
 */
/* Preferred connection parameters should be within the range specified by the 
 * Bluetooth specification.
 */
/* Minimum and maximum connection interval in number of frames */
#define PREFERRED_MAX_CON_INTERVAL          0x0190 /* 500 ms */
#define PREFERRED_MIN_CON_INTERVAL          0x0190 /* 500 ms */

/* Slave latency in number of connection intervals */
#define PREFERRED_SLAVE_LATENCY             0x0004 /* 4 conn_intervals */

/* Supervision timeout (ms) = PREFERRED_SUPERVISION_TIMEOUT * 10 ms */
#define PREFERRED_SUPERVISION_TIMEOUT       0x03e8 /* 10 seconds */


/* Max num of connection parameter update that we send in one connection*/
#define MAX_NUM_CONN_PARAM_UPDATE_REQS 2

#endif /* __GAP_CONN_PARAMS_H__ */
