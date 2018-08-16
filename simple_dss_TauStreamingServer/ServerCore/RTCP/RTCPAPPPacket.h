
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

    RTCPCompressedQTSSPacket(Bool16 debug = false); //�μ�RTPStream::PrintPacket()
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
    char*           mDumpArray; //���ڵ���������ַ�����,1024�ֽڳ�,�μ�RTCPCompressedQTSSPacket::ParseAndStore()
    StrPtrLenDel    mDumpArrayStrDeleter; 
    Bool16 fDebug;
    UInt8* fRTCPAPPDataBuffer;  //points into fReceiverPacketBuffer,ָ��App���ݻ�����ʼ��ַ,�μ�RTCPCompressedQTSSPacket::ParseCompressedQTSSPacket()

    void ParseAndStore(); //����Client���͹����İ�,�������������ݳ�Ա��ֵ
    
    UInt32 fReceiverBitRate;                //�����߱�����
    UInt16 fAverageLateMilliseconds;        //ƽ����ʱ(ms)
    UInt16 fPercentPacketsLost;             //�����ٷֱ�
    UInt16 fAverageBufferDelayMilliseconds; //ƽ��������ʱ(ms)
    Bool16 fIsGettingBetter;                //�����������ڱ����?
    Bool16 fIsGettingWorse;                 //�����������ڱ����?
    UInt32 fNumEyes;                        //������
    UInt32 fNumEyesActive;                  //��Ծ�Ĺ�����
    UInt32 fNumEyesPaused;                  //��ͣ�Ĺ�����
    UInt32 fOverbufferWindowSize;           //Client�����С,����RTCPqtssPacket����û�е�
    
    //Proposed - are these there yet?
    UInt32 fTotalPacketsReceived;           //�յ����ܰ���
    UInt16 fTotalPacketsDropped;            //�������ܰ���
    UInt16 fTotalPacketsLost;               //��ʧ���ܰ���,����RTCPqtssPacket����û�е�
    UInt16 fClientBufferFill;               //�ͻ��˻��������
    UInt16 fFrameRate;                      //֡��
    UInt16 fExpectedFrameRate;              //������֡��
    UInt16 fAudioDryCount;                  //��Ƶ����
    
	//RTCPAPPDataBuffer�еľ������ݵ�����
    enum
    {
        kAppNameOffset          = 0, //four App identifier               //All are UInt32
        kReportSourceIDOffset   = 4,  //SSRC for this report
        kAppPacketVersionOffset = 8,
        kAppPacketVersionMask   = 0xFFFF0000UL,//ռǰ���ֽ�
        kAppPacketVersionShift  = 16, //����ƽ��16��bit�õ�
        kAppPacketLengthOffset  = 8,
        kAppPacketLengthMask    = 0x0000FFFFUL,//ռ�����ֽ�
        kQTSSDataOffset         = 12,
    
        //Individual item offsets/masks �������ƫ�Ƽ�����
        kQTSSItemTypeOffset    = 0,    //SSRC for this report
        kQTSSItemTypeMask      = 0xFFFF0000UL,//ռǰ���ֽ�
        kQTSSItemTypeShift     = 16,          //����ƽ��16��bit�õ�
        kQTSSItemVersionOffset = 0,
        kQTSSItemVersionMask    = 0x0000FF00UL,//ռ�������ֽ�
        kQTSSItemVersionShift  = 8,           //����ƽ��8��bit�õ�
        kQTSSItemLengthOffset  = 0,
        kQTSSItemLengthMask    = 0x000000FFUL,//ռ���ĸ��ֽ�
        kQTSSItemDataOffset    = 4,
    
        kSupportedCompressedQTSSVersion = 0 //����App���İ汾��
    };
    
    //version we support currently
};


/****************  RTCPCompressedQTSSPacket inlines *******************************/

//�õ�RTCPCompressedQTSSPacket��ReportSourceID
inline UInt32 RTCPCompressedQTSSPacket::GetReportSourceID()
{
 return (UInt32) ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kReportSourceIDOffset]) ;
}

//�õ�RTCPCompressedQTSSPacket��AppPacketVersion
inline UInt16 RTCPCompressedQTSSPacket::GetAppPacketVersion()
{
 return (UInt16) ( (ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kAppPacketVersionOffset]) & kAppPacketVersionMask) >> kAppPacketVersionShift );
}

//�õ�RTCPCompressedQTSSPacket��AppPacketName
inline FourCharCode RTCPCompressedQTSSPacket::GetAppPacketName()
{
 return (UInt32) ntohl(*(UInt32*)&fRTCPAPPDataBuffer[kAppNameOffset]) ;
}

//�õ�RTCPCompressedQTSSPacket��AppPacketLength
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
