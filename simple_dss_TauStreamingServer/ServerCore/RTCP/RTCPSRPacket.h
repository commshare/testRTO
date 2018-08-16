
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPSRPacket.h
Description: implement a compound RTCP packet including SR,SDES,BYE and Server info packets
             which sent by the Server side.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#ifndef __RTCP_SR_PACKET__
#define __RTCP_SR_PACKET__

#include "OSHeaders.h"
#include "OS.h"
#include "MyAssert.h"

#include <netinet/in.h> //definition of htonl




//描述了Server端发送的RTCP compound packet的结构, 并非RFC 3550中的SR包
class RTCPSRPacket
{
    public:
    
        enum
        {
            kSRPacketType  = 200,    //UInt32
            kByePacketType = 203
        };

        RTCPSRPacket(); //认真分析这个函数,十分重要
        ~RTCPSRPacket() {}
        
        // ACCESSORS
        // FOR SR
        void*   GetSRPacket()       { return &fSenderReportBuffer[0]; }//得到SR复合包的起始地址
        UInt32  GetSRPacketLen()    { return fSenderReportWithServerInfoSize; }//得到SR复合包的长度,不含BYE包部分
        UInt32  GetSRWithByePacketLen() { return fSenderReportWithServerInfoSize + kByeSizeInBytes; }//得到SR复合包的长度,包含BYE包部分
        // FOR SERVER INFO APP PACKET
        void*   GetServerInfoPacket() { return &fSenderReportBuffer[fSenderReportSize]; }//得到SERVER INFO APP包的内存起始地址
        UInt32  GetServerInfoPacketLen() { return kServerInfoSizeInBytes; } //得到SERVER INFO APP包的长度

        // MODIFIERS
        
        // FOR SR
        inline void SetSSRC(UInt32 inSSRC);
        inline void SetClientSSRC(UInt32 inClientSSRC);

        inline void SetNTPTimestamp(SInt64 inNTPTimestamp);
        inline void SetRTPTimestamp(UInt32 inRTPTimestamp);
        
        inline void SetPacketCount(UInt32 inPacketCount);
        inline void SetByteCount(UInt32 inByteCount);
        
        // FOR SERVER INFO APP PACKET
        inline void SetAckTimeout(UInt32 inAckTimeoutInMsec);

        //RTCP support requires generating unique CNames(Canonical End-Point Identifier SDES Item 规范化名称) for each session.
        //This function generates a proper cName and returns its length. The buffer
        //passed in must be at least kMaxCNameLen
        enum
        {
            kMaxCNameLen = 60   //Uint32
        };
        static UInt32           GetACName(char* ioCNameBuffer);

    private:
    
        enum
        {
            kSenderReportSizeInBytes = 36, //SR包+2个32bit
            kServerInfoSizeInBytes = 28,   //SERVER INFO APP包
            kByeSizeInBytes = 8            //BYE包的长度
        };
        char        fSenderReportBuffer[kSenderReportSizeInBytes + kMaxCNameLen + kServerInfoSizeInBytes + kByeSizeInBytes]; //SR包的缓存,共132字节
        UInt32      fSenderReportSize; //包含SR包+SDES包的长度
        UInt32      fSenderReportWithServerInfoSize; //包含SR包+SDES包+SERVER INFO APP包的长度+4字节(AckTimeout)

};

//设置SR/SDES/SERVER INFO/BYE中的SSRC
inline void RTCPSRPacket::SetSSRC(UInt32 inSSRC)
{
    // Set SSRC in SR
    ((UInt32*)&fSenderReportBuffer)[1] = htonl(inSSRC);
    
    // Set SSRC in SDES
    ((UInt32*)&fSenderReportBuffer)[8] = htonl(inSSRC);
    
    // Set SSRC in SERVER INFO
    Assert((fSenderReportSize & 3) == 0);//确保fSenderReportSize是4的倍数
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 1] = htonl(inSSRC);

    // Set SSRC in BYE
    Assert((fSenderReportWithServerInfoSize & 3) == 0);//确保fSenderReportSize是4的倍数
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) + 1] = htonl(inSSRC);
}

// Set Client SSRC in SERVER INFO
inline void RTCPSRPacket::SetClientSSRC(UInt32 inClientSSRC)
{  
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 3] = htonl(inClientSSRC);   
}

//设置NTP时间戳,参见RTPStream::PrintRTCPSenderReport()
inline void RTCPSRPacket::SetNTPTimestamp(SInt64 inNTPTimestamp)
{
#if ALLOW_NON_WORD_ALIGN_ACCESS
    ((SInt64*)&fSenderReportBuffer)[1] = OS::HostToNetworkSInt64(inNTPTimestamp);
#else
    SInt64 temp = OS::HostToNetworkSInt64(inNTPTimestamp);
    ::memcpy(&((SInt64*)&fSenderReportBuffer)[1], &temp, sizeof(temp));
#endif
}

//设置RTP时间戳
inline void RTCPSRPacket::SetRTPTimestamp(UInt32 inRTPTimestamp)
{
    ((UInt32*)&fSenderReportBuffer)[4] = htonl(inRTPTimestamp);
}

//设置PacketCount
inline void RTCPSRPacket::SetPacketCount(UInt32 inPacketCount)
{
    ((UInt32*)&fSenderReportBuffer)[5] = htonl(inPacketCount);
}

//设置ByteCount
inline void RTCPSRPacket::SetByteCount(UInt32 inByteCount)
{
    ((UInt32*)&fSenderReportBuffer)[6] = htonl(inByteCount);
}   

//设置SERVER INFO PACKET中的AckTimeout
inline void RTCPSRPacket::SetAckTimeout(UInt32 inAckTimeoutInMsec)
{
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) - 1] = htonl(inAckTimeoutInMsec);
}

#endif //__RTCP_SR_PACKET__
