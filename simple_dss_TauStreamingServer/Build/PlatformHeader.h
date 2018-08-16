
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 PlatformHeader.h
Description: about debug macro using on Linux platform.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


// Build flags. How do you want your server built?
#define DEBUG 1
#define ASSERT 1
#define MEMORY_DEBUGGING  0 /* 20091030taoyxmodified*/ //enable this to turn on really fancy debugging of memory leaks, etc...
#define QTFILE_MEMORY_DEBUGGING 0 //QuickTime file memory debugging

#define PLATFORM_SERVER_BIN_NAME "DarwinStreamingServer"
#define PLATFORM_SERVER_TEXT_NAME "Darwin Streaming Server"


#include <endian.h>
#if __BYTE_ORDER == BIG_ENDIAN
    #define BIGENDIAN      1
#else
    #define BIGENDIAN      0
#endif

#define USE_ATOMICLIB 0
#define MACOSXEVENTQUEUE 0
#define __PTHREADS__    1
#define __PTHREADS_MUTEXES__    1
#define ALLOW_NON_WORD_ALIGN_ACCESS 1
#define USE_THREAD      0 //Flag used in QTProxy
#define THREADING_IS_COOPERATIVE        0 
#define USE_THR_YIELD   0
#define kPlatformNameString     "Linux" //²»Í¬µã
#define EXPORT
#define _REENTRANT 1

