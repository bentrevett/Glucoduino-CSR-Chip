/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 *  Part of CSR uEnergy SDK 2.2.2
 *  Application version 2.2.2.0
 *
 *  FILE
 *      uartio.c
 *
 *  DESCRIPTION
 *      UART IO implementation.
 *
 ******************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <uart.h>           /* Functions to interface with the chip's UART */

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "uartio.h"         /* Header file to this source file */
#include "byte_queue.h"     /* Byte queue API */
#include "glucose_sensor.h"
#include <string.h>
#include <time.h> 



#define READ_SERIAL_NO 0
#define READ_RECORD_NO 1
#define READ_RECORDS 2
#define SEND_ACK 3
#define CRC_SEED 0xFFFF

/*============================================================================*
 *  Private Data
 *============================================================================*/
 
 /* The application is required to create two buffers, one for receive, the
  * other for transmit. The buffers need to meet the alignment requirements
  * of the hardware. See the macro definition in uart.h for more details.
  */

/* Create 64-byte receive buffer for UART data */
UART_DECLARE_BUFFER(rx_buffer, UART_BUF_SIZE_BYTES_64);

/* Create 64-byte transmit buffer for UART data */
UART_DECLARE_BUFFER(tx_buffer, UART_BUF_SIZE_BYTES_64);

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

/* UART receive callback to receive serial commands */
static uint16 uartRxDataCallback(void   *p_rx_buffer,
                                 uint16  length,
                                 uint16 *p_req_data_length);

/* UART transmit callback when a UART transmission has finished */
static void uartTxDataCallback(void);

/* Transmit waiting data over UART */
static void sendPendingData(void);

/* Transmit the sleep state over UART */
static void protocolHandler(void);
static TIME_UNIX_CONV timeMeter;
static uint8 rxflag=0;
static uint16 recordNo=0;
//static uint16 recordToRead=0x0000;
static uint8 status=0;
static bool error=FALSE;
//static bool finishedReading=FALSE;
static bool ackFlag=TRUE;
//static uint8 serialNoArrayCRC[15];
//static uint8 serialNoArray[9];
//static uint8 noOfRecordsArrayCRC[8];
//static uint8 noOfRecordsArray[2];
//static uint8 recordsArrayCRC[14];
static uint8 buffer[9];
//static uint8 recordsArray[8];
static void readSerialNo(void);
static void sendAck(bool odd);
static void readNoOfRecords(void);
static void readRecords(/*uint16 startRecord,*/uint16 noOfRecords );
void AddGlucoseMeasData(uint16 result);
void calcDate(TIME_UNIX_CONV  *tm,uint32 meterEpoch);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      uartRxDataCallback
 *
 *  DESCRIPTION
 *      This is an internal callback function (of type uart_data_in_fn) that
 *      will be called by the UART driver when any data is received over UART.
 *      See DebugInit in the Firmware Library documentation for details.
 *
 * PARAMETERS
 *      p_rx_buffer [in]   Pointer to the receive buffer (uint8 if 'unpacked'
 *                         or uint16 if 'packed' depending on the chosen UART
 *                         data mode - this application uses 'unpacked')
 *
 *      length [in]        Number of bytes ('unpacked') or words ('packed')
 *                         received
 *
 *      p_additional_req_data_length [out]
 *                         Number of additional bytes ('unpacked') or words
 *                         ('packed') this application wishes to receive
 *
 * RETURNS
 *      The number of bytes ('unpacked') or words ('packed') that have been
 *      processed out of the available data.
 *----------------------------------------------------------------------------*/
static uint16 uartRxDataCallback(void   *p_rx_buffer,
                                 uint16  length,
                                 uint16 *p_additional_req_data_length)
{


    int i=0;
    if(length>0)
    {
            for(i=0;i<9;i++){
                buffer[i]=(char)*((char *)p_rx_buffer+i);
            }
    
    if(buffer[0]==0x61 && buffer[1]==0x62 && buffer[2]==0x63){
        uint16 result=(uint16)(buffer[4]|(buffer[3]<<8));
        uint32 dateTimeL=(uint32)(buffer[8]|(buffer[7]<<8));
        uint32 dateTimeH=(uint32)(buffer[6]|(buffer[5]<<8));
               
                 
                uint32 dateTime=(uint32)((dateTimeL|dateTimeH<<16));
                calcDate(&timeMeter,dateTime);
                
const uint8  message_len = (sizeof(buffer))/sizeof(uint8);
  
    /* Add opening message to byte queue */
    BQForceQueueBytes(buffer, message_len);    
    sendPendingData();
    TimeDelayUSec(50000);                 
                
                
                AddGlucoseMeasData(result);
    }
    }       
        
/*
    if ( length > 0 )
    {
     
       
        switch(status){
            uint8 crc[2];
            case READ_SERIAL_NO:
            {
                if(rxflag>5 && rxflag<21){
                    serialNoArrayCRC[rxflag-6]=(char)*(char *)p_rx_buffer;
                if (rxflag>10 && rxflag<20){                   
                    serialNoArray[rxflag-11]=(char)*(char *)p_rx_buffer;
                 
                }
                }
                if(rxflag>=21){
                crc[rxflag-21]=(char)*(char *)p_rx_buffer;
                
            }
            if(rxflag>=22)
            {
            uint16 crcrtn=crc_calculate_crc(CRC_SEED,serialNoArrayCRC,sizeof(serialNoArrayCRC)/sizeof(uint8));
           
            if(crc[0]==(crcrtn<<8)>>8 && crc[1]==(uint8)crcrtn>>8) 
            {  
                error=FALSE;
              }
            else{
                error=TRUE;
            }
        }
            } 
            break;
            case READ_RECORD_NO:
            {
             if(rxflag>5 && rxflag<14){
                    noOfRecordsArrayCRC[rxflag-6]=(char)*(char *)p_rx_buffer;
                  if(rxflag>10 && rxflag<13){
                      noOfRecordsArray[rxflag-11]=(char)*(char *)p_rx_buffer;
                      
                  
                  }
                }
            if(rxflag>=14){
                crc[rxflag-14]=(char)*(char *)p_rx_buffer;
                
            }
            if(rxflag>=15)
            {
            uint16 crcrtn=crc_calculate_crc(CRC_SEED,noOfRecordsArrayCRC,sizeof(noOfRecordsArrayCRC)/sizeof(uint8));
           
            if(crc[0]==(crcrtn<<8)>>8 && crc[1]==(uint8)crcrtn>>8) 
            {  
                recordNo=(uint16)(noOfRecordsArray[0]|(noOfRecordsArray[1]<<8));//uint16 of the no of records
                error=FALSE;
              
            }
            else{
                error=TRUE;
            }
        }
        }
            break;
            case READ_RECORDS:
            {
             if(rxflag>5 && rxflag<20){
                    recordsArrayCRC[rxflag-6]=(char)*(char *)p_rx_buffer;
                    if(rxflag>10 && rxflag<19){
                        recordsArray[rxflag-11]=(char)*(char *)p_rx_buffer;
                    
                }
                }
            if(rxflag>=20){
                crc[rxflag-20]=(uint8)*(uint8 *)p_rx_buffer;
            }
            if(rxflag>=21){
            uint16 crcrtn=crc_calculate_crc(CRC_SEED,recordsArrayCRC,sizeof(recordsArrayCRC)/sizeof(uint8));
           
            if(crc[0]==(crcrtn<<8)>>8 && crc[1]==(uint8)crcrtn>>8) 
            {  
                error=FALSE;
                uint16 result=(uint16)(recordsArray[4]|(recordsArray[5]<<8));
                uint32 dateTimeL=(uint32)(recordsArray[0]|(recordsArray[1]<<8));
                uint32 dateTimeH=(uint32)(recordsArray[2]|(recordsArray[3]<<8));
                
                 
                uint32 dateTime=(uint32)((dateTimeL|dateTimeH<<16));
                calcDate(&timeMeter,dateTime);
                
                uint16 result=(uint16)(recordsArray[4]|(recordsArray[5]<<8));
                AddGlucoseMeasData(result);
                
                ackFlag=!ackFlag;
                sendAck(ackFlag);
                
                
                recordToRead++; 
              
                if(recordToRead>=recordNo){
                    finishedReading=TRUE;
                }
                if(!finishedReading){
              
                
            }
                
              
            }
            else{
                error=TRUE;
            }
       }
        }
            break;
            
                    
        }
     
        
         rxflag+=1;
       
    }*/
    
   
    
    /* Inform the UART driver that we'd like to receive another byte when it
     * becomes available
     */
    *p_additional_req_data_length = (uint16)1;
    
    /* Return the number of bytes that have been processed */
    return length;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      uartTxDataCallback
 *
 *  DESCRIPTION
 *      This is an internal callback function (of type uart_data_out_fn) that
 *      will be called by the UART driver when data transmission over the UART
 *      is finished. See DebugInit in the Firmware Library documentation for
 *      details.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
static void uartTxDataCallback(void)
{
    /* Send any pending data waiting to be sent */
    sendPendingData();
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      sendPendingData
 *
 *  DESCRIPTION
 *      Send buffered data over UART that was waiting to be sent. Perform some
 *      translation to ensured characters are properly displayed.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
static void sendPendingData(void)
{
    /* Loop until the byte queue is empty */
    while (BQGetDataSize() > 0)
    {
        uint8 byte = '\0';
        
        /* Read the next byte in the queue */
        if (BQPeekBytes(&byte, 1) > 0)
        {
            bool ok_to_commit = FALSE;
            
            /* Check if Enter key was pressed */
            if (byte == '\r')
            {
                /* Echo carriage return and newline */
                const uint8 data[] = {byte, '\n'};
                
                ok_to_commit = UartWrite(data, sizeof(data)/sizeof(uint8));
            }
            else if (byte == '\b')
            /* If backspace key was pressed */
            {
                /* Issue backspace, overwrite previous character on the
                 * terminal, then issue another backspace
                 */
                const uint8 data[] = {byte, ' ', byte};
                
                ok_to_commit = UartWrite(data, sizeof(data)/sizeof(uint8));
            }
            else
            {
                /* Echo the character */
                ok_to_commit = UartWrite(&byte, 1);
            }

            if (ok_to_commit)
            {
                /* Now that UART driver has accepted this data
                 * remove the data from the buffer
                 */
                BQCommitLastPeek();
            }
            else
            {
                /* If UART doesn't have enough space available to accommodate
                 * this data, postpone sending data and leave it in the buffer
                 * to try again later.
                 */
                break;
            }
        }
        else
        {
            /* Couldn't read data for some reason. Postpone sending data and
             * try again later.
            */
            break;
        }
    }
}



/*----------------------------------------------------------------------------*
 *  NAME
 *      protocolHandler
 *
 *  DESCRIPTION
 *      Translate the sleep state code and transmit over UART.
 *
 * PARAMETERS
 *      state [in]     Sleep state code to translate and transmit
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
static void protocolHandler()
{

    readSerialNo();
      
    if(!error){
  /*   TimeDelayUSec(50000);*/
         sendAck(TRUE);
     }
   /* TimeDelayUSec(50000);*/
    readNoOfRecords();
   if(!error){
   /* TimeDelayUSec(50000);*/
    sendAck(TRUE);
}
 /*TimeDelayUSec(50000); */
    uint16 recordCounter;
  for (recordCounter=0;recordCounter<recordNo&&(!error);recordCounter++) {
   readRecords(recordCounter);
   sendAck(recordCounter%2);
}
    
}
void printForDebug(char string[]){
    
    
    const uint8  message_len = (sizeof(string))/sizeof(uint8);
    BQForceQueueBytes((uint8 *)string, message_len);
    sendPendingData();
}

static void readSerialNo(void){
    rxflag=0;
    
    uint8 message[]= {0x02, 0x12, 0x00, 0x05, 0x0b, 0x02, 0x00, 0x00, 0x00, 0x00, 0x84, 0x6a, 0xe8, 0x73, 0x00, 0x03, 0x9b, 0xea}; 
    const uint8  message_len = (sizeof(message))/sizeof(uint8);
  
    /* Add opening message to byte queue */
    BQForceQueueBytes(message, message_len);
    status=READ_SERIAL_NO;
    /* Add translated sleep state to byte queue */
/*    BQForceQueueBytes(p_state_msg, state_msg_len);*/
    
    /* Send byte queue over UART */
    sendPendingData();
    TimeDelayUSec(50000); 
    
    }
static void readNoOfRecords(void){
    rxflag=0;
    uint8 message[]= {0x02, 0x0A, 0x00, 0x05, 0x1F, 0xF5, 0x01, 0x03, 0x38, 0xAA};
    const uint8  message_len = (sizeof(message))/sizeof(uint8);
  
    /* Add opening message to byte queue */
    BQForceQueueBytes(message, message_len);
    status=READ_RECORD_NO;
  
    
    /* Send byte queue over UART */
    sendPendingData();
    TimeDelayUSec(50000); 
    TimeDelayUSec(50000); 
    
    }
static void readRecords(/*uint16 startRecord,*/uint16 noOfRecords ){
   rxflag=0;
    uint16 recordCounter=noOfRecords;
  
    
    uint8 rL=(uint8)(recordCounter);
    uint8 rH=(uint8)(recordCounter>>8);
    
    uint8 link=ackFlag?0x03:0x00;
    uint8 messageCRC[]= {0x02,0x0A,link,0x05,0x1F, rL, rH, 0x03};    
    uint16 crcrtn=crc_calculate_crc(CRC_SEED,messageCRC,sizeof(messageCRC)/sizeof(uint8));
    uint8 message[]= {0x02,0x0A,link,0x05,0x1F, rL, rH, 0x03, crcrtn, crcrtn>>8};
    const uint8  message_len = (sizeof(message))/sizeof(uint8);
  
    /* Add opening message to byte queue */
    BQForceQueueBytes(message, message_len);
    status=READ_RECORDS;
   
    
    /* Send byte queue over UART */
        sendPendingData();
        TimeDelayUSec(50000); 
        TimeDelayUSec(50000);
/*}*/
    
    }
static void sendAck(bool odd)
{
    uint8 message[]= {0x02, 0x06, 0x07, 0x03, 0xFC, 0x72};/*odd*/
    if(!odd){
            message[2]=0x04;
            message[4]=0xAF;
            message[5]=0x27;/*even*/
        }

        const uint8  message_len = (sizeof(message))/sizeof(uint8);
        BQForceQueueBytes(message, message_len);
        status=SEND_ACK;
   
    
    /* Send byte queue over UART */
    sendPendingData();
    
    TimeDelayUSec(50000); 
    TimeDelayUSec(50000); 
}
void uartHandle(void)
{
    /* Initialise UART and configure with default baud rate and port
     * configuration
     */
    UartInit(uartRxDataCallback,
             uartTxDataCallback,
             rx_buffer, UART_BUF_SIZE_BYTES_128,
             tx_buffer, UART_BUF_SIZE_BYTES_64,
             uart_data_unpacked);
    /*Set the baud rate and configuration*/   
    UartConfig(0x0028,0x00);
    
    /* Enable UART */
    UartEnable(TRUE);

    /* UART receive threshold is set to 1 byte, so that every single byte
     * received will trigger the receiver callback */
    UartRead(9, 0);

    

    /* Display the last sleep state */
    protocolHandler();

    /* Construct welcome message */
   /* const uint8 message[] = "\r\nType something: ";*/
    
    /* Add message to the byte queue */
   /* BQForceQueueBytes(OneTouchEasySN, (sizeof(OneTouchEasySN)-1)/sizeof(uint8));*/
    
    /* Transmit the byte queue over UART */
   /* sendPendingData();*/
}
void AddGlucoseMeasData(uint16 result)
{
  
    uint8 mFlag = 0;
    uint8 cFlag = 0;
    uint8 mData[MAX_LEN_MEAS_OPTIONAL_FIELDS];
    uint8 cData[MAX_LEN_CONTEXT_OPTIONAL_FIELDS];
    uint16 mLen = 0;
    uint16 cLen = 0;
    uint16 random_val = 0;

    /* Fill glucose measurement data */
    mFlag = TIME_OFFSET_PRESENT |
            GLUCOSE_CONC_UNIT_MMOL_PER_LITRE|
            GLUCOSE_CONC_TYPE_SAMPLE_LOCATION_PRESENT |
            SENSOR_STATUS_ANNUNCIATION_PRESENT; /* Unit - mg/dl */

    /* Add time offset */
    mData[mLen ++] = LE8_L(255);
    mData[mLen ++] = LE8_H(255);

    /* Glucose concentration - two Octets - bit 2 of flag tells the unit of
     * glucose concentration. If bit2 is set, It means units will be mol/L and
     * if bit2 is not set units will be in Kg/L
     */
   /* random_val = (TimeGet16() % 29) + GLUCOSE_MEAS_FASTING_NORMAL_MIN;*/

    /* We are setting glucose concentation in [70-99]mg/dl range.
     * Since glucose concentration uses SFLOAT format, So exponent here will
     * be -5 (calculated by converting Kg/L to mg/dl.
     */
   /* random_val |= 0xb000;*/ /* Signed -5 = 0xb(2's complement), exponent gives the
                           * 4 MSBs of concentration.
                           */
   /* reading=reading/18;*/
    mData[mLen ++] = LE8_L(result);
    mData[mLen ++] = LE8_H(result);

    /* Type - sample location - one octet */
    mData[mLen ++] = TYPE_CAPILLARY_WHOLE_BLOOD | LOCATION_FINGER;

    /* Sensor Annunciation octet is normally set to Zero as there was no 
     * irregularity while the reading was being taken but just to have better
     * use case coverage, we are setting random error once in
     * GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH measurements.
     */
     if(result == 1)
    {
        uint8 random_error = TimeGet16() % 256;
        mData[mLen ++] = LE8_L(random_error);
    }
    else
    {
        mData[mLen ++] = LE8_L(0);
    }
    mData[mLen ++] = LE8_H(0);

    /* If some PTS test case is running which requires glucose context to be in
     * every record/last record/first record and project keyr file has been 
     * configured accordingly, then following boolean variable will be TRUE and
     * glucose context will be generated for every record.
     * OR
     * Add context information once in every 
     * GLUCOSE_CONTEXT_REPEAT_CYCLE_LENGTH measurements.
     */
   /* if(g_pts_generate_context_every_record == TRUE || num == 2)*/
    if(TRUE)
    {
        /* Fill glucose context information data */
        mFlag |= CONTEXT_INFORMATION_PRESENT;

        cFlag = EXTENDED_FLAGS_PRESENT |
                CARBOHYDRATE_FIELD_PRESENT |
                MEAL_FIELD_PRESENT |
                TESTER_HEALTH_FIELD_PRESENT |
                EXERCISE_FIELD_PRESENT |
                MEDICATION_FIELD_PRESENT |
                MEDICATION_IN_MILLILITRES |
                HBA1C_FIELD_PRESENT;

        /* Extended flag */
        cData[cLen ++] = 0;

        /* Carbohydrate Id */
        cData[cLen ++] = BREAKFAST;
        /* Carbohydrate value is being randomly chosen in range [0-99]grams
         * Default unit of carbohydrate value is Kg. This range has been chosen
         * keeping PTS testcases in mind.
         */
        random_val = (TimeGet16() % 99);
        /* To convert Kg into grams, exponent will be -3.*/
        random_val |= 0xd000; /* Signed -3 = 0xd(2's complement), exponent gives
                               * the 4 MSBs of concentration.
                               */

        cData[cLen ++] = LE8_L(random_val); /* Range - 0 to 2000 */
        cData[cLen ++] = LE8_H(random_val);

        /* Meal field */
        cData[cLen ++] = AFTER_MEAL;

        /* Tester health field */
        cData[cLen ++] = SELF | NO_HEALTH_ISSUES;

        /* Excercise duration and intensity */
        cData[cLen ++] = LE8_L(3600); /* In seconds - 1 Hour */
        cData[cLen ++] = LE8_H(3600);
        cData[cLen ++] = 40; /* Intensity - 0 to 100 %*/

        /* Medication value is being randomly chosen in range [0-99]
         * millilitres. This range has been chosen
         * keeping PTS testcases in mind.
         */
        random_val = (TimeGet16() % 99);
        /* To convert L into ml , exponent will be -3.*/
        random_val |= 0xd000; /* Signed -3 = 0xd(2's complement), exponent
                               *  gives the 4 MSBs of concentration.
                               */

        /* Medication Id */
        cData[cLen ++] = SHORT_ACTING_INSULIN;
        cData[cLen ++] = LE8_L(random_val); /* Range - 0 to 2000 mg/ml */
        cData[cLen ++] = LE8_H(random_val);

        /* HbA1c */
        cData[cLen ++] = LE8_L(10); /* Range - 0 to 100 % */
        cData[cLen ++] = LE8_H(10);
    }

    AddGlucoseMeasurementToQueue(mFlag, mData, mLen,
                                 cFlag, cData, cLen, &timeMeter);
}
void calcDate(TIME_UNIX_CONV  *tm,uint32 meterEpoch)
{
  uint32 seconds, minutes, hours, days, year, month;
  uint32 dayOfWeek;
  seconds = meterEpoch;

  /* calculate minutes */
  minutes  = seconds / 60;
  seconds -= minutes * 60;
  /* calculate hours */
  hours    = minutes / 60;
  minutes -= hours   * 60;
  /* calculate days */
  days     = hours   / 24;
  hours   -= days    * 24;

  /* Unix time starts in 1970 on a Thursday */
  year      = 1970;
  dayOfWeek = 4;

  while(1)
  {
    bool     leapYear   = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16 daysInYear = leapYear ? 366 : 365;
    if (days >= daysInYear)
    {
      dayOfWeek += leapYear ? 2 : 1;
      days      -= daysInYear;
      if (dayOfWeek >= 7)
        dayOfWeek -= 7;
      ++year;
    }
    else
    {
      tm->tm_yday = days;
      dayOfWeek  += days;
      dayOfWeek  %= 7;

      /* calculate the month and day */
      static const uint8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(month = 0; month < 12; ++month)
      {
        uint8 dim = daysInMonth[month];

        /* add a day to feburary if this is a leap year */
        if (month == 1 && leapYear)
          ++dim;

        if (days >= dim)
          days -= dim;
        else
          break;
      }
      break;
    }
  }
    
  tm->tm_sec  = seconds%60;
  tm->tm_min  = minutes%3600;
  tm->tm_hour = hours%216000;
  tm->tm_mday = (days + 1);
  tm->tm_mon  = month+1;
  tm->tm_year = year;
  tm->tm_wday = dayOfWeek;
  /*
  uint8 message[]= {0x99, tm->tm_sec, tm->tm_min, tm->tm_hour,tm->tm_mon,tm->tm_mday,tm->tm_year,tm->tm_year>>8,0x99}; 
    const uint8  message_len = (sizeof(message))/sizeof(uint8);
  
   
    BQForceQueueBytes(message, message_len);
    sendPendingData();
    */
}
