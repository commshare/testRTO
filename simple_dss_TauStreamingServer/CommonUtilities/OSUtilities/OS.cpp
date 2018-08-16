/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OS.cpp
Description: Provide OS utility functions. Memory allocation, time-dealing, directory-creating, 
             transform between host byte order and net byte order etc.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <stdlib.h>
#include "SafeStdLib.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* 关于struct stat */
#include <sys/time.h>
#include <errno.h>
#include <math.h>

#include "OS.h"
#include "OSThread.h"
#include "OSFileSource.h"
#include "MyAssert.h"
#include "StringParser.h"



/********* 注意这些static变量的赋值表达方法 ***********/

/* 注意下面这些初始值 */
double  OS::sDivisor = 0;
double  OS::sMicroDivisor = 0;
SInt64  OS::sMsecSince1970 = 0;
SInt64  OS::sMsecSince1900 = 0;
SInt64  OS::sInitialMsec = 0;/* 下面OS::Initialize()要用,在OS::Milliseconds()中设置,该值只设置一次 */
SInt64  OS::sWrapTime = 0;
SInt64  OS::sCompareWrap = 0;
SInt64  OS::sLastTimeMilli = 0;/* 记录上一次的计时时间 */
OSMutex OS::sStdLibOSMutex;/* 互斥锁,重要 */

#if DEBUG 
#include "OSMutex.h"
#include "OSMemory.h"
/* 在下面OS::Initialize()中定义 */
static OSMutex* sLastMillisMutex = NULL;
#endif

/*********** 非常重要:对static variables作初始化 **********/
/* need in RunServer::StartServer(),对必要的static数据成员初始化 */
/* 该函数主要是配置sInitialMsec的值!若该值已配,就退出 */
void OS::Initialize()
{
	/* 注意该值只能赋一次,见下面 */
    Assert (sInitialMsec == 0);  // do only once
    if (sInitialMsec != 0) return;

    //setup t0 value for msec since 1900

    //t.tv_sec is number of seconds since Jan 1, 1970. Convert to seconds since 1900
	/* 设置1900-1-1到1970-1-1的秒数,注意每4年一个闰年(要多加1天),70年中有17个闰年(17*4=68) */
    SInt64 the1900Sec = (SInt64) (24 * 60 * 60) * (SInt64) ((70 * 365) + 17) ;
	/* 将1900-1-1到1970-1-1的秒数转化为毫秒 */
    sMsecSince1900 = the1900Sec * 1000;
    
	/* 从低4字节移到高4字节 */
    sWrapTime = (SInt64) 0x00000001 << 32;
	/* 从低4字节移到高4字节 */
    sCompareWrap = (SInt64) 0xffffffff << 32;
	/* 这个量变化次数较多 */
    sLastTimeMilli = 0;
    
	/*********** 非常重要:对static variables作初始化 **********/
	/* 将当前时间(服务器启动时间)赋给这个静态变量,只在此处定义一次 */
    sInitialMsec = OS::Milliseconds(); //Milliseconds uses sInitialMsec so this assignment is valid only once.

    /* 返回自1970年1月1日0点走过的秒数.请注意这个值只在此处定义一次,且是初始化时的时间,以后不会改变 */
    sMsecSince1970 = ::time(NULL);  // POSIX time always returns seconds since 1970
    sMsecSince1970 *= 1000;         // Convert to msec(毫秒)
    
#if DEBUG 
	/*************** NOTE!! ***********************/
	/* 注意在上面初始化为NULL,注意此处创造了Mutex对象 */
    sLastMillisMutex = NEW OSMutex();
#endif
}

/* 注意下面几个量:curTimeMilli和sLastTimeMilli,sInitialMsec,该函数只在OS::Initialize()中调用一次 */
/* 获取服务器当前时间,并设置上次的时间sLastTimeMilli(单位都是ms),而函数的返回值是sInitialMsec */
SInt64 OS::Milliseconds()
{
    struct timeval t;
    struct timezone tz;
    int theErr = ::gettimeofday(&t, &tz);
    Assert(theErr == 0);

    SInt64 curTime;
    curTime = t.tv_sec;
    curTime *= 1000;                // sec -> msec
    curTime += t.tv_usec / 1000;    // usec -> msec

    return (curTime - sInitialMsec) + sMsecSince1970;
}

/* 这个函数运用极其频繁 */
/* 获取服务器开机以来所经过的时间(microseconds 微秒,百万分之一秒),要用到sInitialMsec */
SInt64 OS::Microseconds()
{
    struct timeval t;
    struct timezone tz;
	/* 获得当前时间和时区信息 */
    int theErr = ::gettimeofday(&t, &tz);
    Assert(theErr == 0);

    SInt64 curTime;
    curTime = t.tv_sec;
    curTime *= 1000000;     // sec -> usec //* 将秒转换为微秒(1000000) */
    curTime += t.tv_usec;/* 累计当前时间(微秒) */

    return curTime - (sInitialMsec * 1000);
}

/* 获取当前时区与GMT相差几小时? */
SInt32 OS::GetGMTOffset()
{
    struct timeval  tv;
    struct timezone tz;

	/* 获得当前时间和时区信息 */
    int err = ::gettimeofday(&tv, &tz);
    if (err != 0)
        return 0;
        
    return ((tz.tz_minuteswest / 60) * -1);//return hours before or after GMT
}

/* 将SInt64的数据由主机主机字节序转换为网络字节序,是htonl(),htons()的推广 */
SInt64  OS::HostToNetworkSInt64(SInt64 hostOrdered)
{
#if BIGENDIAN
	/* 注意网络字节序是BIGENDIAN */
    return hostOrdered;
#else
    return (SInt64) (  (UInt64)  (hostOrdered << 56) | (UInt64)  (((UInt64) 0x00ff0000 << 32) & (hostOrdered << 40))
        | (UInt64)  ( ((UInt64)  0x0000ff00 << 32) & (hostOrdered << 24)) | (UInt64)  (((UInt64)  0x000000ff << 32) & (hostOrdered << 8))
        | (UInt64)  ( ((UInt64)  0x00ff0000 << 8) & (hostOrdered >> 8)) | (UInt64)     ((UInt64)  0x00ff0000 & (hostOrdered >> 24))
        | (UInt64)  (  (UInt64)  0x0000ff00 & (hostOrdered >> 40)) | (UInt64)  ((UInt64)  0x00ff & (hostOrdered >> 56)) );
#endif
}

/* 如上恰相反,将SInt64的数据由网络主机字节序转换为主机字节序,是ntohl(),ntohs()的推广 */
SInt64  OS::NetworkToHostSInt64(SInt64 networkOrdered)
{
#if BIGENDIAN
    return networkOrdered;
#else
    return (SInt64) (  (UInt64)  (networkOrdered << 56) | (UInt64)  (((UInt64) 0x00ff0000 << 32) & (networkOrdered << 40))
        | (UInt64)  ( ((UInt64)  0x0000ff00 << 32) & (networkOrdered << 24)) | (UInt64)  (((UInt64)  0x000000ff << 32) & (networkOrdered << 8))
        | (UInt64)  ( ((UInt64)  0x00ff0000 << 8) & (networkOrdered >> 8)) | (UInt64)     ((UInt64)  0x00ff0000 & (networkOrdered >> 24))
        | (UInt64)  (  (UInt64)  0x0000ff00 & (networkOrdered >> 40)) | (UInt64)  ((UInt64)  0x00ff & (networkOrdered >> 56)) );
#endif
}


/* 获取指定路径的文件信息,若没有,就新建一个文件路径 */
OS_Error OS::MakeDir(char *inPath)
{
	/* 包含文件统计信息的结构体 */
    struct stat theStatBuffer;
	/* 若获取指定路径的文件信息出错 */
    if (::stat(inPath, &theStatBuffer) == -1)
    {
        //this directory doesn't exist, so let's try to create it
        if (::mkdir(inPath, S_IRWXU) == -1)
			/* 返回具体的错误信息 */
            return (OS_Error)OSThread::GetErrno();
    }
    else if (!S_ISDIR(theStatBuffer.st_mode))
        return EEXIST;//there is a file at this point in the path!

    //directory exists
    return OS_NoErr;
}

/* 利用OS::MakeDir()逐步生成指定的文件目录 */
OS_Error OS::RecursiveMakeDir(char *inPath)
{
    Assert(inPath != NULL);
    
    //iterate through the path, replacing '/' with '\0' as we go
	/* 从入参获得路径字符串指针给遍历路径 */
    char *thePathTraverser = inPath;
    
    //skip over the first / in the path.
	/* 当遇到'/'就跳过 */
    if (*thePathTraverser == kPathDelimiterChar)
        thePathTraverser++;
    
	/* 当指向字符非空时 */
    while (*thePathTraverser != '\0')
    {
		/* 当遇到路径分隔符'/'时 */
        if (*thePathTraverser == kPathDelimiterChar)
        {
            //we've found a filename divider. Now that we have a complete
            //filename, see if this partial path exists.
            
            //make the partial path into a C string
            *thePathTraverser = '\0';
			/* 在指定的文件路径生成文件目录 */
            OS_Error theErr = MakeDir(inPath);
            //there is a directory here. Just continue in our traversal
			/* 将文件分割符换回去 */
            *thePathTraverser = kPathDelimiterChar;

			/* 若创建目录出错,就返回该错误 */
            if (theErr != OS_NoErr)
                return theErr;
        }
        thePathTraverser++;
    }
    
    //need to create the last directory in the path
	/* 生成最后一个文件的路径目录 */
    return MakeDir(inPath);
}

/* just for MacOSX */
Bool16 OS::ThreadSafe()
{
	return true;
}

/* 通过分析系统信息获得处理器核心个数 */
UInt32  OS::GetNumProcessors()
{ 
    char cpuBuffer[8192] = "";
    StrPtrLen cpuInfoBuf(cpuBuffer, sizeof(cpuBuffer));
    FILE    *cpuFile = ::fopen( "/proc/cpuinfo", "r" );
    if (cpuFile)
    {   
		cpuInfoBuf.Len = ::fread(cpuInfoBuf.Ptr, sizeof(char),  cpuInfoBuf.Len, cpuFile);
        ::fclose(cpuFile);
    }
    
    StringParser cpuInfoFileParser(&cpuInfoBuf);
    StrPtrLen line;
    StrPtrLen word;
    UInt32 numCPUs = 0;
    
    while( cpuInfoFileParser.GetDataRemaining() != 0 ) 
    {
        cpuInfoFileParser.GetThruEOL(&line);    // Read each line   
        StringParser lineParser(&line);
        lineParser.ConsumeWhitespace();         //skip over leading whitespace

        if (lineParser.GetDataRemaining() == 0) // must be an empty line
            continue;

        lineParser.ConsumeUntilWhitespace(&word);
               
        if ( word.Equal("processor") ) // found a processor as first word in line
        {   numCPUs ++; 
        }
    }
    
    if (numCPUs == 0)
        numCPUs = 1;
        
    return numCPUs;

    return 1;
}


//CISCO provided fix for integer + fractional fixed64.
SInt64 OS::TimeMilli_To_Fixed64Secs(SInt64 inMilliseconds)
{
       SInt64 result = inMilliseconds / 1000;  // The result is in lower bits.
       result <<= 32;  // shift it to higher 32 bits
       // Take the remainder (rem = inMilliseconds%1000) and multiply by
       // 2**32, divide by 1000, effectively this gives (rem/1000) as a
       // binary fraction.
       double p = ldexp((double)(inMilliseconds%1000), +32) / 1000.;
       UInt32 frac = (UInt32)p;
       result |= frac;
       return result;
}
