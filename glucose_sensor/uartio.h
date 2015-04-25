/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      uartio.h
 *
 *  DESCRIPTION
 *      UART IO header
 *
 ******************************************************************************/

#ifndef __UARTIO_H__
#define __UARTIO_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
 
#include <types.h>          /* Commonly used type definitions */
#include <sys_events.h>     /* System event definitions and declarations */
#include <sleep.h>          /* Control the device sleep states */
#include "Calc_CRC.h"



typedef struct
{
  uint8 tm_sec;
  uint8 tm_min;
  uint8 tm_hour;
  uint8 tm_mday;
  uint8 tm_mon;
  uint8 tm_yday;
  uint16 tm_year;
  uint8 tm_wday;
}TIME_UNIX_CONV;

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      Start
 *
 *  DESCRIPTION
 *      Run the startup routine.
 *
 * PARAMETERS
 *      last_sleep_state [in]   Last sleep state
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void uartHandle(void);
/*----------------------------------------------------------------------------*
 *  NAME
 *      ProcessSystemEvent
 *
 *  DESCRIPTION
 *      Prints the system event meaning on to UART.
 *
 * PARAMETERS
 *      id   [in]   System event ID
 *      pData [in]  Event data
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
/*void ProcessSystemEvent(sys_event_id id, void *pData);*/
extern void printForDebug(char *string);

#endif /* __UARTIO_H__ */