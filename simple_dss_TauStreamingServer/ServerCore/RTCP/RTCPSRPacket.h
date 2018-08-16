
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




//������Server�˷��͵�RTCP compound packet�Ľṹ, ����RFC 3550�е�SR��
class RTCPSRPacket
{
    public:
    
        enum
        {
            kSRPacketType  = 200,    //UInt32
            kByePacketType = 203
        };

        RTCPSRPacket(); //��������������,ʮ����Ҫ
        ~RTCPSRPacket() {}
        
        // ACCESSORS
        // FOR SR
        void*   GetSRPacket()       { return &fSenderReportBuffer[0]; }//�õ�SR���ϰ�����ʼ��ַ
        UInt32  GetSRPacketLen()    { return fSenderReportWithServerInfoSize; }//�õ�SR���ϰ��ĳ���,����BYE������
        UInt32  GetSRWithByePacketLen() { return fSenderReportWithServerInfoSize + kByeSizeInBytes; }//�õ�SR���ϰ��ĳ���,����BYE������
        // FOR SERVER INFO APP PACKET
        void*   GetServerInfoPacket() { return &fSenderReportBuffer[fSenderReportSize]; }//�õ�SERVER INFO APP�����ڴ���ʼ��ַ
        UInt32  GetServerInfoPacketLen() { return kServerInfoSizeInBytes; } //�õ�SERVER INFO APP���ĳ���

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

        //RTCP support requires generating unique CNames(Canonical End-Point Identifier SDES Item �淶������) for each session.
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
            kSenderReportSizeInBytes = 36, //SR��+2��32bit
            kServerInfoSizeInBytes = 28,   //SERVER INFO APP��
            kByeSizeInBytes = 8            //BYE���ĳ���
        };
        char        fSenderReportBuffer[kSenderReportSizeInBytes + kMaxCNameLen + kServerInfoSizeInBytes + kByeSizeInBytes]; //SR���Ļ���,��132�ֽ�
        UInt32      fSenderReportSize; //����SR��+SDES���ĳ���
        UInt32      fSenderReportWithServerInfoSize; //����SR��+SDES��+SERVER INFO APP���ĳ���+4�ֽ�(AckTimeout)

};

//����SR/SDES/SERVER INFO/BYE�е�SSRC
inline void RTCPSRPacket::SetSSRC(UInt32 inSSRC)
{
    // Set SSRC in SR
    ((UInt32*)&fSenderReportBuffer)[1] = htonl(inSSRC);
    
    // Set SSRC in SDES
    ((UInt32*)&fSenderReportBuffer)[8] = htonl(inSSRC);
    
    // Set SSRC in SERVER INFO
    Assert((fSenderReportSize & 3) == 0);//ȷ��fSenderReportSize��4�ı���
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 1] = htonl(inSSRC);

    // Set SSRC in BYE
    Assert((fSenderReportWithServerInfoSize & 3) == 0);//ȷ��fSenderReportSize��4�ı���
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) + 1] = htonl(inSSRC);
}

// Set Client SSRC in SERVER INFO
inline void RTCPSRPacket::SetClientSSRC(UInt32 inClientSSRC)
{  
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 3] = htonl(inClientSSRC);   
}

//����NTPʱ���,�μ�RTPStream::PrintRTCPSenderReport()
inline void RTCPSRPacket::SetNTPTimestamp(SInt64 inNTPTimestamp)
{
#if ALLOW_NON_WORD_ALIGN_ACCESS
    ((SInt64*)&fSenderReportBuffer)[1] = OS::HostToNetworkSInt64(inNTPTimestamp);
#else
    SInt64 temp = OS::HostToNetworkSInt64(inNTPTimestamp);
    ::memcpy(&((SInt64*)&fSenderReportBuffer)[1], &temp, sizeof(temp));
#endif
}

//����RTPʱ���
inline void RTCPSRPacket::SetRTPTimestamp(UInt32 inRTPTimestamp)
{
    ((UInt32*)&fSenderReportBuffer)[4] = htonl(inRTPTimestamp);
}

//����PacketCount
inline void RTCPSRPacket::SetPacketCount(UInt32 inPacketCount)
{
    ((UInt32*)&fSenderReportBuffer)[5] = htonl(inPacketCount);
}

//����ByteCount
inline void RTCPSRPacket::SetByteCount(UInt32 inByteCount)
{
    ((UInt32*)&fSenderReportBuffer)[6] = htonl(inByteCount);
}   

//����SERVER INFO PACKET�е�AckTimeout
inline void RTCPSRPacket::SetAckTimeout(UInt32 inAckTimeoutInMsec)
{
    ((UInt32*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) - 1] = htonl(inAckTimeoutInMsec);
}

#endif //__RTCP_SR_PACKET__
