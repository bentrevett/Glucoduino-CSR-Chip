/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      byte_queue.c
 *
 *  DESCRIPTION
 *      Circular buffer implementation.
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
 
#include <mem.h>            /* Memory library */

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "byte_queue.h"     /* Interface to this source file */

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Intended buffer size in number of bytes */
#define BUFFER_SIZE 256

/* Largest amount of data that can be stored in the buffer */
#define BUFFER_LEN (BUFFER_SIZE - 1)

/* Length of data currently held in queue */
#define QUEUE_LENGTH \
       ((g_tail >= g_head) ? g_tail - g_head : BUFFER_SIZE - g_head + g_tail)
           
/* Amount of free space left in queue (= BUFFER_LEN - QUEUE_LENGTH) */
#define QUEUE_FREE \
       ((g_tail >= g_head) ? BUFFER_LEN - g_tail + g_head : g_head - g_tail - 1)

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Circular buffer */
static uint8 g_queue[BUFFER_SIZE];

/* Pointer to head of queue (next byte to be read out) */
static uint16 g_head = 0;

/* Pointer to head of queue after committing most recent peek */
static uint16 g_peek = 0;

/* Pointer to tail of queue (next byte to be inserted) */
static uint16 g_tail = 0;

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

/* Append the supplied data to the queue */
static void copyIntoBuffer(const uint8 *p_data, uint16 len);

/* Read up to the requested number of bytes out of the queue */
static uint16 peekBuffer(uint8 *p_data, uint16 len);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/
 
/*----------------------------------------------------------------------------*
 *  NAME
 *      copyIntoBuffer
 *
 *  DESCRIPTION
 *      Copy a given number of bytes in to the buffer. Assumes there is enough
 *      space available in the buffer. If not, the existing data will be
 *      overwritten to accommodate the new data.
 *
 *      At the end of the function g_head points to the oldest queue entry and
 *      g_tail the next insertion point.
 *
 * PARAMETERS
 *      p_data [in]     Pointer to the data to be copied
 *      len    [in]     Number of bytes of data to be copied
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
static void copyIntoBuffer(const uint8 *p_data, uint16 len)
{
    /* Sanity check */
    if ((len == 0) || (p_data == NULL))
        return;
    
    /* No point copying more data into the queue than the queue can hold */
    if (len > BUFFER_LEN)
    {
        /* Advance input pointer to the last BUFFER_LEN bytes */
        p_data += len - BUFFER_LEN;
        
        /* Adjust len */
        len = BUFFER_LEN;
    }
    
    /* Check whether the queue will overflow */
    if (len > QUEUE_FREE)
    {
        /* Advance g_head to point to the oldest item, after the overflow */
        g_head += len - QUEUE_FREE;
        
        /* If this goes past the end of the buffer, wrap around */
        if (g_head >= BUFFER_SIZE)
            g_head -= BUFFER_SIZE;
        
        /* Update g_peek similarly */
        g_peek = g_head;
    }
    
    /* Check whether we're going past the end of the buffer */
    if (g_tail + len >= BUFFER_SIZE)
    {
        /* Calculate how much space there is till the end of the buffer */
        const uint16 available = BUFFER_SIZE - g_tail;
        
        /* Copy data into the queue up to end of buffer */
        MemCopy(&g_queue[g_tail], p_data, available);
        
        /* Update g_tail */
        g_tail = len - available;
        
        /* Copy data from start of buffer */
        MemCopy(g_queue, p_data + available, g_tail);
    }
    else
    {
        /* Append data to tail of the queue */
        MemCopy(&g_queue[g_tail], p_data, len);
        
        /* Update g_tail */
        g_tail += len;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      peekBuffer
 *
 *  DESCRIPTION
 *      Read a given number of bytes from the buffer without removing any data.
 *      If more data is requested than is available, then only the available
 *      data is read.
 *
 * PARAMETERS
 *      p_data [in]     Pointer to buffer to store read data in
 *      len    [in]     Number of bytes of data to peek
 *
 * RETURNS
 *      Number of bytes of data peeked.
 *----------------------------------------------------------------------------*/
static uint16 peekBuffer(uint8 *p_data, uint16 len)
{
    uint16 peeked = len;    /* Number of bytes of data peeked */
    
    /* Sanity check */
    if ((len == 0) || (p_data == NULL))
        return 0;
    
    /* Cannot peek more data than is available */
    if (peeked > QUEUE_LENGTH)
        peeked = QUEUE_LENGTH;
    
    /* Check whether we're going past the end of the buffer */
    if (g_head + peeked >= BUFFER_SIZE)
    {
        /* Calculate how much space there is till the end of the buffer */
        const uint16 available = BUFFER_SIZE - g_head;
        
        /* Copy data up to end of buffer */
        MemCopy(p_data, &g_queue[g_head], available);
        
        /* Update g_peek */
        g_peek = peeked - available;
        
        /* Copy data from start of buffer */
        MemCopy(p_data + available, g_queue, g_peek);
    }
    else
    {
        /* Peek data starting from the head of the queue */
        MemCopy(p_data, &g_queue[g_head], peeked);
        
        /* Update g_peek */
        g_peek = g_head + peeked;
    }
    
    return peeked;
}

/*============================================================================*
 *  Public Function Implementations
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
 *      FALSE if there is not enough space in the queue
 *----------------------------------------------------------------------------*/
bool BQSafeQueueBytes(const uint8 *p_data, uint16 len)
{
    /* Check whether there is enough space available in the buffer */
    bool ret_val = (QUEUE_FREE >= len);
    
    /* If so, copy the data into the buffer */
    if (ret_val)
        copyIntoBuffer(p_data, len);
    
    return ret_val;
}

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
void BQForceQueueBytes(const uint8 *p_data, uint16 len)
{
    /* Copy data into the buffer whether or not space is available */
    copyIntoBuffer(p_data, len);
}

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
uint16 BQGetBufferCapacity(void)
{
    return BUFFER_LEN;
}

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
uint16 BQGetDataSize(void)
{
    return QUEUE_LENGTH;
}

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
uint16 BQGetAvailableSize(void)
{
    return QUEUE_FREE;
}

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
void BQClearBuffer(void)
{
    /* Reset queue pointers */
    g_head = g_peek = g_tail = 0;
}

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
uint16 BQPopBytes(uint8 *p_data, uint16 len)
{
    /* Copy the data into the return buffer */
    uint16 peeked = peekBuffer(p_data, len);
    
    /* Remove the peeked data from the queue */
    BQCommitLastPeek();
    
    /* Return number of bytes peeked */
    return peeked;
}

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
uint16 BQPeekBytes(uint8 *p_data, uint16 len)
{
    /* Peek into the buffer */
    return peekBuffer(p_data, len);
}

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
void BQCommitLastPeek(void)
{
    /* Update g_head to point to current g_peek location */
    g_head = g_peek;
}
