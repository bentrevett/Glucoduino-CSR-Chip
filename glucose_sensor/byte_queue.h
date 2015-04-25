/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      byte_queue.h
 *
 *  DESCRIPTION
 *      Interface to circular buffer implementation.
 *
 ******************************************************************************/

#ifndef __BYTE_QUEUE_H__
#define __BYTE_QUEUE_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
 
#include <types.h>          /* Commonly used type definitions */

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQSafeQueueBytes
 *
 *  DESCRIPTION
 *      Queue the supplied data if there is sufficient space available.
 *      If there is not enough space FALSE is returned instead.
 *
 * PARAMETERS
 *      p_data [in]     Pointer to the data to be queued
 *      len    [in]     Number of bytes of data to be queued
 *
 * RETURNS
 *      TRUE if the data is queued successfully
 *      FALSE if there is not enough space in the buffer
 *----------------------------------------------------------------------------*/
extern bool BQSafeQueueBytes(const uint8 *p_data, uint16 len);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQForceQueueBytes
 *
 *  DESCRIPTION
 *      Queue the supplied data. If there is not enough space then data at the
 *      head of the queue is overwritten and the head of the queue moved to
 *      the end of the new data.
 *
 * PARAMETERS
 *      p_data [in]     Pointer to the data to be queued
 *      len    [in]     Number of bytes of data to be queued
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void BQForceQueueBytes(const uint8 *p_data, uint16 len);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQGetBufferCapacity
 *
 *  DESCRIPTION
 *      Return the total size of the buffer.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Total buffer size in bytes
 *----------------------------------------------------------------------------*/
extern uint16 BQGetBufferCapacity(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQGetDataSize
 *
 *  DESCRIPTION
 *      Return the amount of data currently in the queue.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Size of data currently stored in the queue in bytes.
 *----------------------------------------------------------------------------*/
extern uint16 BQGetDataSize(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQGetAvailableSize
 *
 *  DESCRIPTION
 *      Return the amount of free space available in the buffer.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Size of free space available in the buffer in bytes.
 *----------------------------------------------------------------------------*/
extern uint16 BQGetAvailableSize(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQClearBuffer
 *
 *  DESCRIPTION
 *      Clear buffer contents leaving the queue empty.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void BQClearBuffer(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQPopBytes
 *
 *  DESCRIPTION
 *      Extract up to the specified number of bytes from the queue increasing
 *      the available size by the number of bytes extracted. If not enough data
 *      is held in the queue then the function returns immediately with
 *      whatever data is available.
 *
 * PARAMETERS
 *      p_data [out]    Pointer to a buffer to store the extracted data in
 *      len    [in]     Number of bytes of data to be extracted
 *
 * RETURNS
 *      Number of bytes actually extracted, may be fewer than requested if not
 *      enough data is available.
 *----------------------------------------------------------------------------*/
extern uint16 BQPopBytes(uint8 *p_data, uint16 len);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQPeekBytes
 *
 *  DESCRIPTION
 *      Peek up to the specified number of bytes from the queue, without
 *      modifying the buffer. If not enough data is held in the queue then
 *      the function returns immediately with whatever data is available.
 *
 * PARAMETERS
 *      p_data [out]    Pointer to a buffer to store the peeked data in
 *      len    [in]     Number of bytes of data to be peeked
 *
 * RETURNS
 *      Number of bytes actually peeked, may be fewer than requested if not
 *      enough data is available.
 *----------------------------------------------------------------------------*/
extern uint16 BQPeekBytes(uint8 *p_data, uint16 len);

/*----------------------------------------------------------------------------*
 *  NAME
 *      BQCommitLastPeek
 *
 *  DESCRIPTION
 *      Remove from the queue the data that was returned in the last call to
 *      BQPeekBytes.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void BQCommitLastPeek(void);

#endif /* __BYTE_QUEUE_H__ */
