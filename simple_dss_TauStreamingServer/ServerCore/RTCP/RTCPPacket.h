
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPPacket.h
Description: Represents a common RTCP Packet header object.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#define DEBUG_RTCP_PACKETS 1


#ifndef _RTCPPACKET_H_
#define _RTCPPACKET_H_

#include <stdlib.h>
#include "OSHeaders.h"
#include "SafeStdLib.h"
#ifndef __Win32__
#include <sys/types.h>
#include <netinet/in.h>
#endif


//描述一个抽象的RTCP的包头的类,占4个字节
class RTCPPacket 
{
public:

    // Packet types
    enum
    {
        kReceiverPacketType     = 201,  //UInt32 //receiver report
        kSDESPacketType         = 202,  //UInt32 //source description
        kAPPPacketType          = 204   //UInt32 //application-defined
    };
    

    RTCPPacket() : fReceiverPacketBuffer(NULL) {}
    virtual ~RTCPPacket() {}

    //Call this before any accessor method. Returns true if successful, false otherwise
    Bool16 ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen);

	//下面的几个函数按照RTCP的包头中的位域依次排列
    inline int    GetVersion();
    inline Bool16 GetHasPadding();
    inline int    GetReportCount();
    inline UInt8  GetPacketType();
    inline UInt16 GetPacketLength();    //in 32-bit words
    inline UInt32 GetPacketSSRC();

	//得到RTCP包头(前两个字节)
    inline SInt16 GetHeader();

	/* 得到包缓存起始地址 */
    UInt8* GetPacketBuffer() { return fReceiverPacketBuffer; }
    
    Bool16 IsValidPacket();
    
    virtual void Dump();

    enum
    {
        kRTCPPacketSizeInBytes = 8,     //All are UInt32s
        kRTCPHeaderSizeInBytes = 4      //4个字符/字节的包头
    };
        
protected:
    
    UInt8* fReceiverPacketBuffer; /* 接收方缓存地址 */
    
    enum
    {
        kVersionOffset        = 0,
        kVersionMask          = 0xC0000000UL,//Version掩码
        kVersionShift         = 30,          //将一个32bit整数向右平移30bit得到
        kHasPaddingOffset     = 0,           //在RTCP的第0个字节后面
        kHasPaddingMask       = 0x20000000UL,//Padding位的掩码
        kReportCountOffset    = 0,
        kReportCountMask      = 0x1F000000UL,//RC掩码
        kReportCountShift     = 24,          //将一个32bit整数向右平移24bit得到
        kPacketTypeOffset     = 0,
        kPacketTypeMask       = 0x00FF0000UL,//PT掩码
        kPacketTypeShift      = 16,          //将一个32bit整数向右平移16bit得到
        kPacketLengthOffset   = 0,
        kPacketLengthMask     = 0x0000FFFFUL,//length掩码
        kPacketSourceIDOffset = 4,           //在RTCP的第4个字节后面,packet sender SSRC
        kPacketSourceIDSize   = 4,           //sender SSRC标识符占4个字节,参见下面的RTCPReceiverPacket类
        kSupportedRTCPVersion = 2            //just used in RTCPPacket::ParsePacket()
    };

};




class SourceDescriptionPacket : public RTCPPacket

{

public:
    
    SourceDescriptionPacket() : RTCPPacket() {}
    
    Bool16 ParseSourceDescription(UInt8* inPacketBuffer, UInt32 inPacketLength)
                            { return ParsePacket(inPacketBuffer, inPacketLength); }

private:    
};


//描述了一个RTCP RR包除去RTCP包头的部分, 它是ReportBlock数组,详细分析了ReportBlock内的部分
class RTCPReceiverPacket  : public RTCPPacket
{
public:

    RTCPReceiverPacket() : RTCPPacket(), fRTCPReceiverReportArray(NULL) {}

    //Call this before any accessor method. Returns true if successful, false otherwise
    Bool16 ParseReceiverReport(UInt8* inPacketBuffer, UInt32 inPacketLength);

    inline UInt32 GetReportSourceID(int inReportNum); 
    inline UInt32 GetHighestSeqNumReceived(int inReportNum);
    inline UInt32 GetJitter(int inReportNum);
    inline UInt32 GetLastSenderReportTime(int inReportNum);
    inline UInt32 GetLastSenderReportDelay(int inReportNum);    //expressed in units of 1/65536 seconds

	UInt8  GetFractionLostPackets(int inReportNum);
	UInt32 GetTotalLostPackets(int inReportNum);
    UInt32 GetCumulativeFractionLostPackets();
    UInt32 GetCumulativeTotalLostPackets();
    UInt32 GetCumulativeJitter();

    Bool16 IsValidPacket();
    
    virtual void Dump(); //Override RTCPPacket object
    
protected:
    inline int RecordOffset(int inReportNum);//注意这个函数非常重要,其它函数都要用!

    UInt8* fRTCPReceiverReportArray;    //points into fReceiverPacketBuffer,指向ReportBlock数组,参见RTCPReceiverPacket::ParseReceiverReport()

    enum
    {
        kReportBlockOffsetSizeInBytes = 24,          //ReportBlock占6个UInt32,All are UInt32s
        kReportBlockOffset            = kPacketSourceIDOffset + kPacketSourceIDSize,//ReportBlock在整个RTCP包中的偏移是8个字节  
        kReportSourceIDOffset         = 0,           //SSRC for this report
        kFractionLostOffset           = 4,
        kFractionLostMask             = 0xFF000000UL,//占1个字节
        kFractionLostShift            = 24,          //将一个32bit整数向右平移24bit得到
        kTotalLostPacketsOffset       = 4,           //将一个32bit整数向左平移4bit得到
        kTotalLostPacketsMask         = 0x00FFFFFFUL,//占3个字节
        kHighestSeqNumReceivedOffset  = 8,           //在一个ReportBlock内的第8个字节后
        kJitterOffset                 = 12,          //在一个ReportBlock内的第12个字节后
        kLastSenderReportOffset       = 16,          //在一个ReportBlock内的第16个字节后
        kLastSenderReportDelayOffset  = 20           //在一个ReportBlock内的第20个字节后
    };

};

/******************************  RTCPPacket  inlines ***************************/

//先取RTCP包的首字节,转换字节序后,取该字节的前两比特,再右移30比特得到,该RTCP包的版本号
inline int RTCPPacket::GetVersion()
{
    UInt32* theVersionPtr = (UInt32*)&fReceiverPacketBuffer[kVersionOffset];
    UInt32 theVersion = ntohl(*theVersionPtr); 
    return (int) ((theVersion  & kVersionMask) >> kVersionShift);
}

//获取padding位的值
inline Bool16 RTCPPacket::GetHasPadding()
{
    UInt32* theHasPaddingPtr = (UInt32*)&fReceiverPacketBuffer[kHasPaddingOffset];
    UInt32 theHasPadding = ntohl(*theHasPaddingPtr);
    return (Bool16) (theHasPadding & kHasPaddingMask);
}

//获取RC域(ReportBlock的个数)的值
inline int RTCPPacket::GetReportCount()
{
    UInt32* theReportCountPtr = (UInt32*)&fReceiverPacketBuffer[kReportCountOffset];
    UInt32 theReportCount = ntohl(*theReportCountPtr);
    return (int) ((theReportCount & kReportCountMask) >> kReportCountShift);
}

//获取RTCP包的类型
inline UInt8 RTCPPacket::GetPacketType()
{
    UInt32* thePacketTypePtr = (UInt32*)&fReceiverPacketBuffer[kPacketTypeOffset];
    UInt32 thePacketType = ntohl(*thePacketTypePtr);
    return (UInt8) ((thePacketType & kPacketTypeMask) >> kPacketTypeShift);
}

//获取RTCP包的包长度(不含头4个字节)
inline UInt16 RTCPPacket::GetPacketLength()
{
    return (UInt16) ( ntohl(*(UInt32*)&fReceiverPacketBuffer[kPacketLengthOffset]) & kPacketLengthMask);
}

//获取RTCP的同步源
inline UInt32 RTCPPacket::GetPacketSSRC()
{
    return (UInt32) ntohl(*(UInt32*)&fReceiverPacketBuffer[kPacketSourceIDOffset]) ;
}

//获取RTCP的头两个字节
inline SInt16 RTCPPacket::GetHeader(){ return (SInt16) ntohs(*(SInt16*)&fReceiverPacketBuffer[0]) ;}

/*************************  RTCPReceiverPacket  inlines *******************************/

//返回指定编号-1的ReportBlock在RTCP RR实例中的偏移
inline int RTCPReceiverPacket::RecordOffset(int inReportNum) 
{
    return inReportNum*kReportBlockOffsetSizeInBytes;
}   

//得到该ReportBlock的SSRC
inline UInt32 RTCPReceiverPacket::GetReportSourceID(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kReportSourceIDOffset]) ;
}

//得到该ReportBlock的FractionLost
inline UInt8 RTCPReceiverPacket::GetFractionLostPackets(int inReportNum)
{
    return (UInt8) ( (ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kFractionLostOffset]) & kFractionLostMask) >> kFractionLostShift );
}

//得到该ReportBlock的总丢包数
inline UInt32 RTCPReceiverPacket::GetTotalLostPackets(int inReportNum)
{
    return (ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kTotalLostPacketsOffset]) & kTotalLostPacketsMask );
}

//得到该ReportBlock的收到的最大Seq Number
inline UInt32 RTCPReceiverPacket::GetHighestSeqNumReceived(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kHighestSeqNumReceivedOffset]) ;
}

//得到该ReportBlock的传送Jitter
inline UInt32 RTCPReceiverPacket::GetJitter(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kJitterOffset]) ;
}

//得到该ReportBlock的上一个SenderReportTime
inline UInt32 RTCPReceiverPacket::GetLastSenderReportTime(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kLastSenderReportOffset]) ;
}

//得到该ReportBlock的上一个SenderReport的延迟
inline UInt32 RTCPReceiverPacket::GetLastSenderReportDelay(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kLastSenderReportDelayOffset]) ;
}


/*
SDES: Source Description
------------------------
0             1               2               3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P| SC    | PT=SDES=202   |              length             | header 
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                        SSRC/CSRC_1                            | chunk 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        SDES items                             |
|                            ...                                |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                        SSRC/CSRC_2                            | chunk 2 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        SDES items                             |
|                            ...                                |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

Receiver Report
---------------
 0                   1                   2                   3
 0 0 0 1 1 1 1 1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|    RC   |   PT=RR=201   |             length            | header
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     SSRC of packet sender                     |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_1 (SSRC of first source)                 | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
| fraction lost |       cumulative number of packets lost       |   1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           extended highest sequence number received           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      interarrival jitter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         last SR (LSR)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   delay since last SR (DLSR)                  |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_2 (SSRC of second source)                | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
:                               ...                             :   2
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                  profile-specific extensions                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


*/

#endif //_RTCPPACKET_H_
