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
#include <sys/stat.h> /* ����struct stat */
#include <sys/time.h>
#include <errno.h>
#include <math.h>

#include "OS.h"
#include "OSThread.h"
#include "OSFileSource.h"
#include "MyAssert.h"
#include "StringParser.h"



/********* ע����Щstatic�����ĸ�ֵ��﷽�� ***********/

/* ע��������Щ��ʼֵ */
double  OS::sDivisor = 0;
double  OS::sMicroDivisor = 0;
SInt64  OS::sMsecSince1970 = 0;
SInt64  OS::sMsecSince1900 = 0;
SInt64  OS::sInitialMsec = 0;/* ����OS::Initialize()Ҫ��,��OS::Milliseconds()������,��ֵֻ����һ�� */
SInt64  OS::sWrapTime = 0;
SInt64  OS::sCompareWrap = 0;
SInt64  OS::sLastTimeMilli = 0;/* ��¼��һ�εļ�ʱʱ�� */
OSMutex OS::sStdLibOSMutex;/* ������,��Ҫ */

#if DEBUG 
#include "OSMutex.h"
#include "OSMemory.h"
/* ������OS::Initialize()�ж��� */
static OSMutex* sLastMillisMutex = NULL;
#endif

/*********** �ǳ���Ҫ:��static variables����ʼ�� **********/
/* need in RunServer::StartServer(),�Ա�Ҫ��static���ݳ�Ա��ʼ�� */
/* �ú�����Ҫ������sInitialMsec��ֵ!����ֵ����,���˳� */
void OS::Initialize()
{
	/* ע���ֵֻ�ܸ�һ��,������ */
    Assert (sInitialMsec == 0);  // do only once
    if (sInitialMsec != 0) return;

    //setup t0 value for msec since 1900

    //t.tv_sec is number of seconds since Jan 1, 1970. Convert to seconds since 1900
	/* ����1900-1-1��1970-1-1������,ע��ÿ4��һ������(Ҫ���1��),70������17������(17*4=68) */
    SInt64 the1900Sec = (SInt64) (24 * 60 * 60) * (SInt64) ((70 * 365) + 17) ;
	/* ��1900-1-1��1970-1-1������ת��Ϊ���� */
    sMsecSince1900 = the1900Sec * 1000;
    
	/* �ӵ�4�ֽ��Ƶ���4�ֽ� */
    sWrapTime = (SInt64) 0x00000001 << 32;
	/* �ӵ�4�ֽ��Ƶ���4�ֽ� */
    sCompareWrap = (SInt64) 0xffffffff << 32;
	/* ������仯�����϶� */
    sLastTimeMilli = 0;
    
	/*********** �ǳ���Ҫ:��static variables����ʼ�� **********/
	/* ����ǰʱ��(����������ʱ��)���������̬����,ֻ�ڴ˴�����һ�� */
    sInitialMsec = OS::Milliseconds(); //Milliseconds uses sInitialMsec so this assignment is valid only once.

    /* ������1970��1��1��0���߹�������.��ע�����ֵֻ�ڴ˴�����һ��,���ǳ�ʼ��ʱ��ʱ��,�Ժ󲻻�ı� */
    sMsecSince1970 = ::time(NULL);  // POSIX time always returns seconds since 1970
    sMsecSince1970 *= 1000;         // Convert to msec(����)
    
#if DEBUG 
	/*************** NOTE!! ***********************/
	/* ע���������ʼ��ΪNULL,ע��˴�������Mutex���� */
    sLastMillisMutex = NEW OSMutex();
#endif
}

/* ע�����漸����:curTimeMilli��sLastTimeMilli,sInitialMsec,�ú���ֻ��OS::Initialize()�е���һ�� */
/* ��ȡ��������ǰʱ��,�������ϴε�ʱ��sLastTimeMilli(��λ����ms),�������ķ���ֵ��sInitialMsec */
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

/* ����������ü���Ƶ�� */
/* ��ȡ����������������������ʱ��(microseconds ΢��,�����֮һ��),Ҫ�õ�sInitialMsec */
SInt64 OS::Microseconds()
{
    struct timeval t;
    struct timezone tz;
	/* ��õ�ǰʱ���ʱ����Ϣ */
    int theErr = ::gettimeofday(&t, &tz);
    Assert(theErr == 0);

    SInt64 curTime;
    curTime = t.tv_sec;
    curTime *= 1000000;     // sec -> usec //* ����ת��Ϊ΢��(1000000) */
    curTime += t.tv_usec;/* �ۼƵ�ǰʱ��(΢��) */

    return curTime - (sInitialMsec * 1000);
}

/* ��ȡ��ǰʱ����GMT��Сʱ? */
SInt32 OS::GetGMTOffset()
{
    struct timeval  tv;
    struct timezone tz;

	/* ��õ�ǰʱ���ʱ����Ϣ */
    int err = ::gettimeofday(&tv, &tz);
    if (err != 0)
        return 0;
        
    return ((tz.tz_minuteswest / 60) * -1);//return hours before or after GMT
}

/* ��SInt64�����������������ֽ���ת��Ϊ�����ֽ���,��htonl(),htons()���ƹ� */
SInt64  OS::HostToNetworkSInt64(SInt64 hostOrdered)
{
#if BIGENDIAN
	/* ע�������ֽ�����BIGENDIAN */
    return hostOrdered;
#else
    return (SInt64) (  (UInt64)  (hostOrdered << 56) | (UInt64)  (((UInt64) 0x00ff0000 << 32) & (hostOrdered << 40))
        | (UInt64)  ( ((UInt64)  0x0000ff00 << 32) & (hostOrdered << 24)) | (UInt64)  (((UInt64)  0x000000ff << 32) & (hostOrdered << 8))
        | (UInt64)  ( ((UInt64)  0x00ff0000 << 8) & (hostOrdered >> 8)) | (UInt64)     ((UInt64)  0x00ff0000 & (hostOrdered >> 24))
        | (UInt64)  (  (UInt64)  0x0000ff00 & (hostOrdered >> 40)) | (UInt64)  ((UInt64)  0x00ff & (hostOrdered >> 56)) );
#endif
}

/* ����ǡ�෴,��SInt64�����������������ֽ���ת��Ϊ�����ֽ���,��ntohl(),ntohs()���ƹ� */
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


/* ��ȡָ��·�����ļ���Ϣ,��û��,���½�һ���ļ�·�� */
OS_Error OS::MakeDir(char *inPath)
{
	/* �����ļ�ͳ����Ϣ�Ľṹ�� */
    struct stat theStatBuffer;
	/* ����ȡָ��·�����ļ���Ϣ���� */
    if (::stat(inPath, &theStatBuffer) == -1)
    {
        //this directory doesn't exist, so let's try to create it
        if (::mkdir(inPath, S_IRWXU) == -1)
			/* ���ؾ���Ĵ�����Ϣ */
            return (OS_Error)OSThread::GetErrno();
    }
    else if (!S_ISDIR(theStatBuffer.st_mode))
        return EEXIST;//there is a file at this point in the path!

    //directory exists
    return OS_NoErr;
}

/* ����OS::MakeDir()������ָ�����ļ�Ŀ¼ */
OS_Error OS::RecursiveMakeDir(char *inPath)
{
    Assert(inPath != NULL);
    
    //iterate through the path, replacing '/' with '\0' as we go
	/* ����λ��·���ַ���ָ�������·�� */
    char *thePathTraverser = inPath;
    
    //skip over the first / in the path.
	/* ������'/'������ */
    if (*thePathTraverser == kPathDelimiterChar)
        thePathTraverser++;
    
	/* ��ָ���ַ��ǿ�ʱ */
    while (*thePathTraverser != '\0')
    {
		/* ������·���ָ���'/'ʱ */
        if (*thePathTraverser == kPathDelimiterChar)
        {
            //we've found a filename divider. Now that we have a complete
            //filename, see if this partial path exists.
            
            //make the partial path into a C string
            *thePathTraverser = '\0';
			/* ��ָ�����ļ�·�������ļ�Ŀ¼ */
            OS_Error theErr = MakeDir(inPath);
            //there is a directory here. Just continue in our traversal
			/* ���ļ��ָ������ȥ */
            *thePathTraverser = kPathDelimiterChar;

			/* ������Ŀ¼����,�ͷ��ظô��� */
            if (theErr != OS_NoErr)
                return theErr;
        }
        thePathTraverser++;
    }
    
    //need to create the last directory in the path
	/* �������һ���ļ���·��Ŀ¼ */
    return MakeDir(inPath);
}

/* just for MacOSX */
Bool16 OS::ThreadSafe()
{
	return true;
}

/* ͨ������ϵͳ��Ϣ��ô��������ĸ��� */
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
