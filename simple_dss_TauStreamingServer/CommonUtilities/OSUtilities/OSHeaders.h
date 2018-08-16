
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSHeaders.h
Description: Define Operating system platform-specific constants and typedefs,just for Linux.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef OSHeaders_H
#define OSHeaders_H

#include <limits.h>
#include <sys/types.h>

#define kSInt16_Max USHRT_MAX
#define kUInt16_Max USHRT_MAX

#define kSInt32_Max LONG_MAX
#define kUInt32_Max ULONG_MAX

#define kSInt64_Max LONG_LONG_MAX
#define kUInt64_Max ULONG_LONG_MAX


#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif



/* Platform-specific components, just for Linux */
    

#define _64BITARG_ "q"
#define kEOLString "\n"
#define kPathDelimiterString "/"
#define kPathDelimiterChar '/'
#define kPartialPathBeginsWithDelimiter 0

#define QT_TIME_TO_LOCAL_TIME   (-2082844800)
#define QT_PATH_SEPARATOR       '/'

typedef unsigned int        PointerSizedInt;
typedef unsigned char       UInt8;
typedef signed char         SInt8;
typedef unsigned short      UInt16;
typedef signed short        SInt16;
typedef unsigned long       UInt32;
typedef signed long         SInt32;
typedef signed long long    SInt64;
typedef unsigned long long  UInt64;
typedef float               Float32;
typedef double              Float64;
typedef UInt16              Bool16;
typedef UInt8               Bool8;

typedef unsigned long       FourCharCode;
typedef FourCharCode        OSType;

#ifdef  FOUR_CHARS_TO_INT
#error Conflicting Macro "FOUR_CHARS_TO_INT"
#endif

#define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

#ifdef  TW0_CHARS_TO_INT
#error Conflicting Macro "TW0_CHARS_TO_INT"
#endif
    
#define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )


typedef SInt32 OS_Error; 

enum
{
    OS_NoErr = (OS_Error) 0,
    OS_BadURLFormat = (OS_Error) -100,
    OS_NotEnoughSpace = (OS_Error) -101
};


#endif /* OSHeaders_H */
