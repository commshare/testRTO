
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSDataConverter.h
Description: Utility routines for converting to and from QTSS_AttrDataTypes and text
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSS.h"

class QTSSDataConverter
{
    public:
    
        
        // This function converts a type string, eg, "UInt32" to the enum, qtssAttrDataTypeUInt32
        static QTSS_AttrDataType TypeStringToType( char* inTypeString);
        
        
        // This function does the opposite conversion
        static char*    TypeToTypeString( QTSS_AttrDataType inType);
        
        
        // This function converts a text-formatted value of a certain type
        // to its type. Returns: QTSS_NotEnoughSpace if the buffer provided
        // is not big enough.
        
        // String must be NULL-terminated(一定是NULL结尾的).
        // If output value is a string, it will not be NULL-terminated(不一定是NULL结尾的)
        static QTSS_Error   StringToValue(char* inValueAsString,
                                          QTSS_AttrDataType inType,
                                          void* ioBuffer,
                                          UInt32* ioBufSize);
        
        // If value is a string, doesn't have to be NULL-terminated(不一定是NULL结尾的).
        // Output string will be NULL terminated.
        static char* ValueToString(void* inValue,
                                   const UInt32 inValueLen,
                                   const QTSS_AttrDataType inType);
        

        // Takes a pointer to buffer and converts to hex in high to low order
        static char* ConvertBytesToCHexString( void* inValue, const UInt32 inValueLen);
        
        // Takes a string of hex values and converts to bytes in high to low order
        static QTSS_Error ConvertCHexStringToBytes( char* inValueAsString,
                                                    void* ioBuffer,
                                                    UInt32* ioBufSize);
};

