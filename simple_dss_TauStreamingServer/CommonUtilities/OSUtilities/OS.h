
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OS.h
Description: Provide OS utility functions. Memory allocation, time-dealing, directory-creating, 
             transform between host byte order and net byte order etc.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _OS_H_
#define _OS_H_


#include "OSHeaders.h"
#include "OSMutex.h"
#include <string.h>

class OS
{
    public:
    
        //call this before calling anything else
        static void Initialize();

        static SInt32 Min(SInt32 a, SInt32 b)   { if (a < b) return a; return b; } //C语言中常见的取最小值
        
        // Milliseconds always returns milliseconds(毫秒) since Jan 1, 1970 GMT.
        // This basically makes it the same as a POSIX time_t value, except
        // in msec, not seconds. To convert to a time_t, divide by 1000.
        static SInt64   Milliseconds(); //毫秒定义

        static SInt64   Microseconds(); //微秒定义
        
        // Some processors (MIPS, Sparc) cannot handle non word aligned memory
        // accesses. So, we need to provide functions to safely get at non-word
        // aligned memory.
		/* 得到非word对齐内存 */
        static inline UInt32    GetUInt32FromMemory(UInt32* inP); //函数声明,具体定义见最下面

        //because the OS doesn't seem to have these functions
		/* 网络字节序的相互转化,是原来16bit和32bit version的推广 */
        static SInt64   HostToNetworkSInt64(SInt64 hostOrdered);
        static SInt64   NetworkToHostSInt64(SInt64 networkOrdered);
        
		/* 处理时间值问题 */
		static SInt64	TimeMilli_To_Fixed64Secs(SInt64 inMilliseconds); //new CISCO provided implementation
        //disable: calculates integer value only                { return (SInt64) ( (Float64) inMilliseconds / 1000) * ((SInt64) 1 << 32 ) ; }
		
		static SInt64	TimeMilli_To_1900Fixed64Secs(SInt64 inMilliseconds)
						{ return TimeMilli_To_Fixed64Secs(sMsecSince1900) + TimeMilli_To_Fixed64Secs(inMilliseconds); }

		static SInt64	TimeMilli_To_UnixTimeMilli(SInt64 inMilliseconds)
						{ return inMilliseconds; }

		static time_t	TimeMilli_To_UnixTimeSecs(SInt64 inMilliseconds)
						{ return (time_t)  ( (SInt64) TimeMilli_To_UnixTimeMilli(inMilliseconds) / (SInt64) 1000); }
		
		static time_t 	UnixTime_Secs(void) // Seconds since 1970
						{ return TimeMilli_To_UnixTimeSecs(Milliseconds()); }

        static time_t   Time1900Fixed64Secs_To_UnixTimeSecs(SInt64 in1900Fixed64Secs)
                        { return (time_t)( (SInt64)  ((SInt64)  ( in1900Fixed64Secs - TimeMilli_To_Fixed64Secs(sMsecSince1900) ) /  ((SInt64) 1 << 32)  ) ); }
                            
        static SInt64   Time1900Fixed64Secs_To_TimeMilli(SInt64 in1900Fixed64Secs)
                        { return   ( (SInt64) ( (Float64) ((SInt64) in1900Fixed64Secs - (SInt64) TimeMilli_To_Fixed64Secs(sMsecSince1900) ) / (Float64)  ((SInt64) 1 << 32) ) * 1000) ; }
 
        // Returns the offset in hours between local time and GMT (or UTC) time.
		/* 获取本地时区和GMT或UTC相差几小时? */
        static SInt32   GetGMTOffset();
                            
        //Both these functions return QTSS_NoErr, QTSS_FileExists, or POSIX errorcode
		// create directory

        //Makes whatever directories in this path that don't exist yet 
		/* 生成指定路径上任何不存在的目录 */
        static OS_Error RecursiveMakeDir(char *inPath);
        //Makes the directory at the end of this path
        static OS_Error MakeDir(char *inPath);
        
        // Discovery of how many processors are on this machine
		/* 得到处理器个数 */
        static UInt32   GetNumProcessors();
        
        // CPU Load
		/* 得到CPU当前负载百分比 */
        static Float32  GetCurrentCPULoadPercent();
        
        // Mutex for StdLib calls
		/* SafeStdLib.h/cpp会用到该mutex */
         static OSMutex* GetStdLibMutex()  { return &sStdLibOSMutex; }

        static SInt64   InitialMSec()       { return sInitialMsec; } //见上行定义

		/* 服务器开始多长时间? */
        static Float32  StartTimeMilli_Float() { return (Float32) ( (Float64) ( (SInt64) OS::Milliseconds() - (SInt64) OS::InitialMSec()) / (Float64) 1000.0 ); }
        static SInt64   StartTimeMilli_Int()      { return (OS::Milliseconds() - OS::InitialMSec()); }

		static Bool16 	ThreadSafe();

   private:
    
        static double sDivisor;
        static double sMicroDivisor;
		/* 1900-1-1到1970-1-1的毫秒数*/
        static SInt64 sMsecSince1900;
		/* 返回自1970年1月1日0点走过的秒数,用::time(NULL)生成的POSIX time,再将秒转化为毫秒 */
        static SInt64 sMsecSince1970;
        static SInt64 sInitialMsec;
        static SInt32 sMemoryErr;
        static void SetDivisor();
        static SInt64 sWrapTime;
        static SInt64 sCompareWrap;
        static SInt64 sLastTimeMilli;

		/* used in SafeStdLib.h/cpp */
        static OSMutex sStdLibOSMutex;
};


// used in RTCPCompressedQTSSPacket::ParseAndStore()
/* 注意: inline函数必须与函数声明在一起定义,参见谭浩强C++书 */
/* 当允许non-word align时,直接返回整型指针,否则先转换为字符型指针再复制得到整型值 */
inline UInt32   OS::GetUInt32FromMemory(UInt32* inP) //函数声明见上面
{
#if ALLOW_NON_WORD_ALIGN_ACCESS
    return *inP;
#else
    char* tempPtr = (char*)inP;
    UInt32 temp = 0;
    ::memcpy(&temp, tempPtr, sizeof(UInt32));
    return temp;
#endif
}


#endif
