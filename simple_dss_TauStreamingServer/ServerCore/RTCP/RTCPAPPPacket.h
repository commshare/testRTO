
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPAPPPacket.h
Description: define a RTCPCompressedQTSSPacket class which extends the RTCP APP 
             packet in the RFC 3550.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _RTCPAPPPACKET_H_
#define _RTCPAPPPACKET_H_

#include "RTCPPacket.h"
#include "StrPtrLen.h"


/****** RTCPCompressedQTSSPacket is the packet type that the client actually sends ******/
class RTCPCompressedQTSSPacket : public RTCPPacket
{
public:

    RTCPCompressedQTSSPacket(Bool16 debug = false); //参见RTPStream::PrintPacket()
    virtual ~RTCPCompressedQTSSPacket() {}
    
    //Call this before any accessor method. Returns true if successful, false otherwise
    Bool16 ParseCompressedQTSSPacket(UInt8* inPacketBuffer, UInt32 inPacketLength);

	// the following 4 are used in RTCPCompressedQTSSPacket::ParseAndStore()
    inline UInt32 GetReportSourceID();
    inline UInt16 GetAppPacketVersion();
    inline UInt16 GetAppPacketLength(); //In 'UInt32's
    inline FourCharCode GetAppPacketName();
    
	//access
    inline UInt32 GetReceiverBitRate() {return fReceiverBitRate;}
    inline UInt16 GetAverageLateMilliseconds()  {return fAverageLateMilliseconds;}
    inline UInt16 GetPercentPacketsLost()   {return fPercentPacketsLost;}
    inline UInt16 GetAverageBufferDelayMilliseconds()   {return fAverageBufferDelayMilliseconds;}
    inline Bool16 GetIsGettingBetter()  {return fIsGettingBetter;}
    inline Bool16 GetIsGettingWorse()   {return fIsGettingWorse;}
    inline UInt32 GetNumEyes()  {return fNumEyes;}
    inline UInt32 GetNumEyesActive()    {return fNumEyesActive;}
    inline UInt32 GetNumEyesPaused()    {return fNumEyesPaused;}
    inline UInt32 GetOverbufferWindowSize() {return fOverbufferWindowSize;}
    
    //Proposed - are these there yet?
    inline UInt32 GetTotalPacketReceived()  {return fTotalPacketsReceived;}
    inline UInt16 GetTotalPacketsDropped()  {return fTotalPacketsDropped;}
    inline UInt16 GetTotalPacketsLost() {return fTotalPacketsLost;}
    inline UInt16 GetClientBufferFill() {return fClientBufferFill;}
    inline UInt16 GetFrameRate()    {return fFrameRate;}
    inline UInt16 GetExpectedFrameRate()    {return fExpectedFrameRate;}
    inline UInt16 GetAudioDryCount()    {return fAudioDryCount;}
    
    virtual void  Dump(); //Override
    inline UInt8* GetRTCPAPPDataBuffer()    {return fRTCPAPPDataBuffer;}

private:
    char*           mDumpArray; //用于调试输出的字符数组,1024字节长,参见RTCPCompressedQTSSPacket::ParseAndStore()
    StrPtrLenDel    mDumpArrayStrDeleter; 
    Bool16 fDebug;
    UInt8* fRTCPAPPDataBuffer;  //points into fReceiverPacketBuffer,指向App数据缓存起始地址,参见RTCPCompressedQTSSPacket::ParseCompressedQTSSPacket()

    void ParseAndStore(); //解析Client发送过来的包,并设置下面数据成员的值
    
    UInt32 fReceiverBitRate;                //接收者比特率
    UInt16 fAverageLateMilliseconds;        //平均延时(ms)
    UInt16 fPercentPacketsLost;             //丢包百分比
    UInt16 fAverageBufferDelayMilliseconds; //平均缓冲延时(ms)
    Bool16 fIsGettingBetter;                //播放质量正在变好吗?
    Bool16 fIsGettingWorse;                 //播放质量正在变差吗?
    UInt32 fNumEyes;                        //观众数
    UInt32 fNumEyesActive;                  //活跃的观众数
    UInt32 fNumEyesPaused;                  //暂停的观众数
    UInt32 fOverbufferWindowSize;           //Client缓存大小,这是RTCPqtssPacket类所没有的
    
    //Proposed - are these there yet?
    UInt32 fTotalPacketsReceived;           //收到的总包数
    UInt16 fTotalPacketsDropped;            //丢弃的总包数
    UInt16 fTotalPacketsLost;               //丢失的总包数,这是RTCPqtssPacket类所没有的
    UInt16 fClientBufferFill;               //客户端缓存填充数
    UInt16 fFrameRate;                      //帧率
    UInt16 fExpectedFrameRate;              //期望的帧率
    UInt16 fAudioDryCount;                  //音频丢包
    
	//RTCPAPPDataBuffer中的具体内容的排列
    enum
    {
        kAppNameOffset          = 0, //four App identifier               //All are UInt32
        kReportSourceIDOffset   = 4,  //SSRC for this report
        kAppPacketVersionOffset = 8,
        kAppPacketVersionMask   = 0xFFFF0000UL,//占前两字节
        kAppPacketVersionShift  = 16, //向右平移16个bit得到
        kAppPacketLengthOffset  = 8,
        kAppPacketLengthMask    = 0x0000FFFFUL,//占后两字节
        kQTSSDataOffset         = 12,
    
        //Individual item offsets/masks 单个项的偏移及掩码
        kQTSSItemTypeOffset    = 0,    //SSRC for this report
        kQTSSItemTypeMask      = 0xFFFF0000UL,//占前两字节
        kQTSSItemTypeShift     = 16,          //向右平移16个bit得到
        kQTSSItemVersionOffset = 0,
        kQTSSItemVersionMask    = 0x0000FF00UL,//占第三个字节
        kQTSSItemVersionShift  = 8,           //向右平移8个bit得到
        kQTSSItemLengthOffset  = 0,
        kQTSSItemLengthMask    = 0x000000FFUL,//占第四个字节
        kQTSSItemDataOffset    = 4,
    
        kSupportedCompressedQTSSVersion = 0 //该类App包的版本号
    };
    
    //version we support currently
};


/****************  RTCPCompressedQTSSPacket inlines *******************************/

//得到RTCPCompressedQTSSPacket中ReportSourceID
inline UInt32 RTCPCompressedQTSSPacket::GetReportSourceID()
{
 return (UInt32) ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kReportSourceIDOffset]) ;
}

//得到RTCPCompressedQTSSPacket中AppPacketVersion
inline UInt16 RTCPCompressedQTSSPacket::GetAppPacketVersion()
{
 return (UInt16) ( (ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kAppPacketVersionOffset]) & kAppPacketVersionMask) >> kAppPacketVersionShift );
}

//得到RTCPCompressedQTSSPacket中AppPacketName
inline FourCharCode RTCPCompressedQTSSPacket::GetAppPacketName()
{
 return (UInt32) ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kAppNameOffset]) ;
}

//得到RTCPCompressedQTSSPacket中AppPacketLength
inline UInt16 RTCPCompressedQTSSPacket::GetAppPacketLength()
{
    return (UInt16) (ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kAppPacketLengthOffset]) & kAppPacketLengthMask);
}


/*
6.6 APP: Application-defined RTCP packet

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |   PT=APP=204  |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   application-dependent data                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   
 */

#endif //_RTCPAPPPACKET_H_
