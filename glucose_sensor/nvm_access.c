/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      nvm_access.c
 *
 *  DESCRIPTION
 *      This file defines routines which the application uses to access
 *      NVM.
 *
 *  NOTES
 *
 ******************************************************************************/
/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <nvm.h>
#include <i2c.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "nvm_access.h"


/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Disable
 *
 *  DESCRIPTION
 *      This function is used to perform things necessary to save power on NVM
 *      once the read/write operations are done.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void Nvm_Disable(void)
{
    NvmDisable();
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Read
 *
 *  DESCRIPTION
 *      Read words from the NVM Store after preparing the NVM to be readable.
 *
 *      Read words starting at the word offset and store them in the supplied
 *      buffer.
 *
 *      \param buffer  The buffer to read words into.
 *      \param length  The number of words to read.
 *      \param offset  The word offset within the NVM Store to read from.
 *
 *  RETURNS/MODIFIES
 *  Status of operation.
 *
 *---------------------------------------------------------------------------*/

extern void Nvm_Read(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;
    /* NvmRead automatically enables the NVM before reading */
    result = NvmRead(buffer, length, offset);
    /* Disable NVM after reading/writing */
    Nvm_Disable();

    /* If NvmRead fails, report panic */
    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_read);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Write
 *
 *  DESCRIPTION
 *      Write words to the NVM store after powering it up.
 *
 *      Write words from the supplied buffer into the NVM Store, starting at the
 *      given word offset.
 *
 *      \param buffer  The buffer to write.
 *      \param length  The number of words to write.
 *      \param offset  The word offset within the NVM Store to write to.
 *
 *  RETURNS/MODIFIES
 *      Status of operation.
 *
 *----------------------------------------------------------------------------*/

extern void Nvm_Write(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;
    /* NvmWrite automatically enables the NVM before writing */
    result = NvmWrite(buffer, length, offset);
    /* Disable NVM after reading/writing */
    Nvm_Disable();

    /* If NvmWrite fails, report panic */
    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_write);
    }
}
