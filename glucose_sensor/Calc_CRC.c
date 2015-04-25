#include "Calc_CRC.h"




unsigned short crc_calculate_crc (unsigned short initial_crc, const unsigned char *buffer, unsigned short length)
{    
    unsigned short index = 0;
    unsigned short crc = initial_crc; 
    
    if (buffer != NULL){ 
        for (index = 0; index < length; index++){ 
            crc = (unsigned short)((unsigned char)(crc >> 8) | (unsigned short)(crc << 8)); 
            crc ^= buffer [index];     
            crc ^= (unsigned char)(crc & 0xff) >> 4;       
            crc ^= (unsigned short)((unsigned short)(crc << 8) << 4);   
            crc ^= (unsigned short)((unsigned short)((crc & 0xff) << 4) << 1);   
            }    
        }  
    return (crc); } 