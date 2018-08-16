/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 InternalStdLib.cpp
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

#include "OS.h"
#include "OSMutex.h"
#include "OSMemory.h"
#include "SafeStdLib.h"


static UInt64 sTotalChars=0;
static UInt32 sMaxTotalCharsInK = 100 * 1000;//100MB default
static int    sMaxFileSizeReached = 0;
unsigned long qtss_getmaxprintfcharsinK()
{
    OSMutexLocker locker(OS::GetStdLibMutex());
    return sMaxTotalCharsInK;
}

void qtss_setmaxprintfcharsinK(unsigned long newMaxCharsInK)
{
    OSMutexLocker locker(OS::GetStdLibMutex());
    sMaxTotalCharsInK = newMaxCharsInK;
}

int qtss_maxprintf(const char *fmt,  ...)
{
    if (fmt == NULL)
        return -1;

    OSMutexLocker locker(OS::GetStdLibMutex());
    
    if (sTotalChars > ( (UInt64) sMaxTotalCharsInK * 1024) )
    {	
    	if (sMaxFileSizeReached == 0)
            printf ("\nReached maximum configured output limit = %luK\n", sMaxTotalCharsInK);

    	sMaxFileSizeReached = 1; 

    	return -1;
    }	
    sMaxFileSizeReached = 0; // in case maximum changes
    
    va_list args;
    va_start(args,fmt);
    int result = ::vprintf(fmt, args);
    sTotalChars += result;
    va_end(args);
    
    return result;
}



#ifndef USE_DEFAULT_STD_LIB


#define VSNprintf vsnprintf


int qtss_printf(const char *fmt,  ...)
{
    if (fmt == NULL)
        return -1;

    OSMutexLocker locker(OS::GetStdLibMutex());
    va_list args;
    va_start(args,fmt);
    int result = ::vprintf(fmt, args);
    va_end(args);
    
    return result;
}

int qtss_sprintf(char *buffer, const char *fmt,  ...)
{
    if (buffer == NULL)
        return -1;
        
    OSMutexLocker locker(OS::GetStdLibMutex());
    va_list args;
    va_start(args,fmt);
    int result = ::vsprintf(buffer, fmt, args);
    va_end(args);
    
    return result;
}

int qtss_fprintf(FILE *file, const char *fmt,  ...)
{
    if (file == NULL)
        return -1;
        
    OSMutexLocker locker(OS::GetStdLibMutex());
    va_list args;
    va_start(args,fmt);
    int result = ::vfprintf(file, fmt, args);
    va_end(args);
    
    return result;
}

int  qtss_snprintf(char *str, size_t size, const char *fmt, ...)
{
    if (str == NULL)
        return -1;

    OSMutexLocker locker(OS::GetStdLibMutex());
    va_list args;
    va_start(args,fmt);
    int result = ::VSNprintf(str, size, fmt, args);
    va_end(args);
    
    return result;
}

size_t qtss_strftime(char *buf, size_t maxsize, const char *format, const struct tm *timeptr)
{
    if (buf == NULL)
        return 0;

    OSMutexLocker locker(OS::GetStdLibMutex());
    return ::strftime(buf, maxsize, format, timeptr); 

}


#endif //USE_DEFAULT_STD_LIB


/* 获取入参errnum指定的错误原因的描述字符串,并存入指定长度的缓存中 */
char *qtss_strerror(int errnum, char* buffer, int buffLen)
{
    OSMutexLocker locker(OS::GetStdLibMutex());
	/* strerror()用来依参数errnum的错误代码来查询其错误原因的描述字符串并将字符串指针返回 */
 	(void) ::strncpy( buffer, ::strerror(errnum), buffLen); 
	buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
 
    return buffer;
}

/* 将参数timep所指的时间和日期以字符串格式表示,并存放在指定长度的缓存中 */
char *qtss_ctime(const time_t *timep, char* buffer, int buffLen)
{
    OSMutexLocker locker(OS::GetStdLibMutex());
	/* ctime()将参数timep所指的time_t结构中的信息转换成真实世界所使用的时间日期表示方法，然后将结果以字符串形态返回 */
    ::strncpy( buffer, ::ctime(timep), buffLen);//don't use terminator
	buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
    
	return buffer;
}

/* 将参数timeptr所指的tm结构中的时间和日期以字符串格式表示,并存放在指定长度的缓存中 */
char *qtss_asctime(const struct tm *timeptr, char* buffer, int buffLen)
{
    OSMutexLocker locker(OS::GetStdLibMutex());
	/* asctime()将参数timeptr所指的tm结构中的信息转换成真实世界所使用的时间日期表示方法，然后将结果以字符串形态返回 */
    ::strncpy( buffer, ::asctime(timeptr), buffLen);
    buffer[buffLen -1] = 0;  //make sure it is null terminated even if truncated.
    
    return buffer;
}

/* 依据给定的时间(第一个参数),输出GMT指定格式的时间结构(同时也保存在第二个参数里) */
struct tm *qtss_gmtime(const time_t *timep, struct tm *result)
{
    OSMutexLocker locker(OS::GetStdLibMutex());
	/* 将指定时间转换成GTM/UTC时间(没有时区转换) */
    struct tm *time_result = ::gmtime(timep);
    *result = *time_result;
    
    return result;
}

/* 依据给定的时间(第一个参数),输出本地指定格式的时间结构(同时也保存在第二个参数里) */
struct tm *qtss_localtime(const time_t *timep, struct tm *result)
{
   OSMutexLocker locker(OS::GetStdLibMutex());
    /* 用标准Linux函数localtime()将指定时间转换成local time */
    struct tm *time_result = ::localtime(timep);
    *result = *time_result;
    
    return result;
}



