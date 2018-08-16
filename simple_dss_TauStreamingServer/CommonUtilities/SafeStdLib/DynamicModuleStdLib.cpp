
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 DynamicModuleStdLib.cpp
Description: Thread safe std lib calls for internal modules and apps.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/


#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "SafeStdLib.h"
#include "OSMutex.h"
#include "OS.h"
#include "QTSS.h"


#ifndef USE_DEFAULT_STD_LIB

#define VSNprintf vsnprintf


int qtss_printf(char *fmt,  ...)
{
    if (fmt == NULL)
        return 1;

    QTSS_LockStdLib();
    va_list args;
    va_start(args,fmt);
    int result =  ::vprintf(fmt, args);
    va_end(args);
    QTSS_UnlockStdLib();
    
    return result;
}

int qtss_sprintf(char *buffer, const char *fmt,  ...)
{
    if (buffer == NULL)
        return -1;

    QTSS_LockStdLib();
    va_list args;
    va_start(args,fmt);
    int result =  ::vsprintf(buffer, fmt, args);
    va_end(args);
    QTSS_UnlockStdLib();
    
    return result;
}

int qtss_fprintf(FILE *file, const char *fmt,  ...)
{
    if (file == NULL)
        return -1;

    QTSS_LockStdLib();
    va_list args;
    va_start(args,fmt);
    int result =  ::vfprintf(file, fmt, args);
    va_end(args);
    QTSS_UnlockStdLib();
    
    return result;
}

int  qtss_snprintf(char *str, size_t size, const char *fmt, ...)
{
    if (str == NULL)
        return -1;

    QTSS_LockStdLib();
    va_list args;
    va_start(args,fmt);
    int result =  ::VSNprintf(str, size, fmt, args);
    va_end(args);
    QTSS_UnlockStdLib();
    
    return result;
}

size_t qtss_strftime(char *buf, size_t maxsize, const char *format, const struct tm *timeptr)
{
    if (buf == NULL)
        return 0;

    QTSS_LockStdLib();
    size_t result = ::strftime(buf, maxsize, format, timeptr); 
    QTSS_UnlockStdLib();
    
    return result;
}

#endif //USE_DEFAULT_STD_LIB

char *qtss_strerror(int errnum, char* buffer, int buffLen)
{
    QTSS_LockStdLib();
	(void) ::strncpy( buffer, ::strerror(errnum), buffLen);
	buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
    QTSS_UnlockStdLib();

    return buffer;
}

char *qtss_ctime(const time_t *timep, char* buffer, int buffLen)
{
    QTSS_LockStdLib();
    (void) ::strncpy( buffer, ::ctime(timep), buffLen);
    buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
    QTSS_UnlockStdLib();

    return buffer;
}

char *qtss_asctime(const struct tm *timeptr, char* buffer, int buffLen)
{
    QTSS_LockStdLib();
    (void) ::strncpy( buffer, ::asctime(timeptr), buffLen);
    buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
    QTSS_UnlockStdLib();

    return buffer;
}

struct tm *qtss_gmtime(const time_t *timep, struct tm *result)
{
   QTSS_LockStdLib();
    struct tm *time_result = ::gmtime(timep);
    *result = *time_result;
    QTSS_UnlockStdLib();
    
    return result;
}

struct tm *qtss_localtime(const time_t *timep, struct tm *result)
{
    QTSS_LockStdLib();
    struct tm *time_result = ::localtime(timep);
    *result = *time_result;
    QTSS_UnlockStdLib();
    
    return result;
}

