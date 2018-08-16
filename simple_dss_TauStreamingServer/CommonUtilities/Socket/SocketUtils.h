
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD
FileName:	 SocketUtils.h
Description: Implemention some static routines for dealing with networking.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#ifndef __SOCKETUTILS_H__
#define __SOCKETUTILS_H__


#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include "ev.h"
#include "OSHeaders.h"
#include "MyAssert.h"
#include "StrPtrLen.h"
#include "OSMutex.h"



class SocketUtils
{
    public:

        // Call initialize before using any socket functions.
        // (pass true for lookupDNSName if you want the hostname
        // looked up via DNS during initialization -- %%sfu)
		/* 创建数据报类型的Socket,获取它的IP Address List;创建指定大小的IPAddrInfoArray,并配置成以IP address,
		IP address string,DNS为分量的结构体IPAddrInfo的数组.有可能的话,互换第一个和第二个分量的位置 */
        static void Initialize(Bool16 lookupDNSName = true);
        
        //static utility routines
        static Bool16   IsMulticastIPAddr(UInt32 inAddress);
        static Bool16   IsLocalIPAddr(UInt32 inAddress);

        //This function converts an integer IP address to a dotted-decimal string.
        //This function is NOT THREAD SAFE!!!
        static void ConvertAddrToString(const struct in_addr& theAddr, StrPtrLen* outAddr);
        
        // This function converts a dotted-decimal string IP address to a UInt32
        static UInt32 ConvertStringToAddr(const char* inAddr);
        
        //You can get at all the IP addrs and DNS names on this machine this way
        static UInt32       GetNumIPAddrs() { return sNumIPAddrs; }
        static inline UInt32        GetIPAddr(UInt32 inAddrIndex);
        static inline StrPtrLen*    GetIPAddrStr(UInt32 inAddrIndex);
        static inline StrPtrLen*    GetDNSNameStr(UInt32 inDNSIndex);
            
    private:

        //Utility function used by Initialize
        static Bool16 IncrementIfReqIter(char** inIfReqIter, ifreq* ifr);

        //For storing relevent information about each IP interface
        struct IPAddrInfo
        {
            UInt32      fIPAddr;
            StrPtrLen   fIPAddrStr;
            StrPtrLen   fDNSNameStr;
        };
        
		/* 该数组配置参见SocketUtils::Initialize() */
        static IPAddrInfo*              sIPAddrInfoArray;
        static UInt32                   sNumIPAddrs;
        static OSMutex                  sMutex;
};

inline UInt32   SocketUtils::GetIPAddr(UInt32 inAddrIndex)
{
    Assert(sIPAddrInfoArray != NULL);
    Assert(inAddrIndex < sNumIPAddrs);
    return sIPAddrInfoArray[inAddrIndex].fIPAddr;
}

inline StrPtrLen*   SocketUtils::GetIPAddrStr(UInt32 inAddrIndex)
{
    Assert(sIPAddrInfoArray != NULL);
    Assert(inAddrIndex < sNumIPAddrs);
    return &sIPAddrInfoArray[inAddrIndex].fIPAddrStr;
}

inline StrPtrLen*   SocketUtils::GetDNSNameStr(UInt32 inDNSIndex)
{
    Assert(sIPAddrInfoArray != NULL);
    Assert(inDNSIndex < sNumIPAddrs);
    return &sIPAddrInfoArray[inDNSIndex].fDNSNameStr;
}

#endif // __SOCKETUTILS_H__

