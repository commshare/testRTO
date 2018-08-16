
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

        static SInt32 Min(SInt32 a, SInt32 b)   { if (a < b) return a; return b; } //C�����г�����ȡ��Сֵ
        
        // Milliseconds always returns milliseconds(����) since Jan 1, 1970 GMT.
        // This basically makes it the same as a POSIX time_t value, except
        // in msec, not seconds. To convert to a time_t, divide by 1000.
        static SInt64   Milliseconds(); //���붨��

        static SInt64   Microseconds(); //΢�붨��
        
        // Some processors (MIPS, Sparc) cannot handle non word aligned memory
        // accesses. So, we need to provide functions to safely get at non-word
        // aligned memory.
		/* �õ���word�����ڴ� */
        static inline UInt32    GetUInt32FromMemory(UInt32* inP); //��������,���嶨���������

        //because the OS doesn't seem to have these functions
		/* �����ֽ�����໥ת��,��ԭ��16bit��32bit version���ƹ� */
        static SInt64   HostToNetworkSInt64(SInt64 hostOrdered);
        static SInt64   NetworkToHostSInt64(SInt64 networkOrdered);
        
		/* ����ʱ��ֵ���� */
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
		/* ��ȡ����ʱ����GMT��UTC��Сʱ? */
        static SInt32   GetGMTOffset();
                            
        //Both these functions return QTSS_NoErr, QTSS_FileExists, or POSIX errorcode
		// create directory

        //Makes whatever directories in this path that don't exist yet 
		/* ����ָ��·�����κβ����ڵ�Ŀ¼ */
        static OS_Error RecursiveMakeDir(char *inPath);
        //Makes the directory at the end of this path
        static OS_Error MakeDir(char *inPath);
        
        // Discovery of how many processors are on this machine
		/* �õ����������� */
        static UInt32   GetNumProcessors();
        
        // CPU Load
		/* �õ�CPU��ǰ���ذٷֱ� */
        static Float32  GetCurrentCPULoadPercent();
        
        // Mutex for StdLib calls
		/* SafeStdLib.h/cpp���õ���mutex */
         static OSMutex* GetStdLibMutex()  { return &sStdLibOSMutex; }

        static SInt64   InitialMSec()       { return sInitialMsec; } //�����ж���

		/* ��������ʼ�೤ʱ��? */
        static Float32  StartTimeMilli_Float() { return (Float32) ( (Float64) ( (SInt64) OS::Milliseconds() - (SInt64) OS::InitialMSec()) / (Float64) 1000.0 ); }
        static SInt64   StartTimeMilli_Int()      { return (OS::Milliseconds() - OS::InitialMSec()); }

		static Bool16 	ThreadSafe();

   private:
    
        static double sDivisor;
        static double sMicroDivisor;
		/* 1900-1-1��1970-1-1�ĺ�����*/
        static SInt64 sMsecSince1900;
		/* ������1970��1��1��0���߹�������,��::time(NULL)���ɵ�POSIX time,�ٽ���ת��Ϊ���� */
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
/* ע��: inline���������뺯��������һ����,�μ�̷��ǿC++�� */
/* ������non-word alignʱ,ֱ�ӷ�������ָ��,������ת��Ϊ�ַ���ָ���ٸ��Ƶõ�����ֵ */
inline UInt32   OS::GetUInt32FromMemory(UInt32* inP) //��������������
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
