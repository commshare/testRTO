
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


//����һ�������RTCP�İ�ͷ����,ռ4���ֽ�
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

	//����ļ�����������RTCP�İ�ͷ�е�λ����������
    inline int    GetVersion();
    inline Bool16 GetHasPadding();
    inline int    GetReportCount();
    inline UInt8  GetPacketType();
    inline UInt16 GetPacketLength();    //in 32-bit words
    inline UInt32 GetPacketSSRC();

	//�õ�RTCP��ͷ(ǰ�����ֽ�)
    inline SInt16 GetHeader();

	/* �õ���������ʼ��ַ */
    UInt8* GetPacketBuffer() { return fReceiverPacketBuffer; }
    
    Bool16 IsValidPacket();
    
    virtual void Dump();

    enum
    {
        kRTCPPacketSizeInBytes = 8,     //All are UInt32s
        kRTCPHeaderSizeInBytes = 4      //4���ַ�/�ֽڵİ�ͷ
    };
        
protected:
    
    UInt8* fReceiverPacketBuffer; /* ���շ������ַ */
    
    enum
    {
        kVersionOffset        = 0,
        kVersionMask          = 0xC0000000UL,//Version����
        kVersionShift         = 30,          //��һ��32bit��������ƽ��30bit�õ�
        kHasPaddingOffset     = 0,           //��RTCP�ĵ�0���ֽں���
        kHasPaddingMask       = 0x20000000UL,//Paddingλ������
        kReportCountOffset    = 0,
        kReportCountMask      = 0x1F000000UL,//RC����
        kReportCountShift     = 24,          //��һ��32bit��������ƽ��24bit�õ�
        kPacketTypeOffset     = 0,
        kPacketTypeMask       = 0x00FF0000UL,//PT����
        kPacketTypeShift      = 16,          //��һ��32bit��������ƽ��16bit�õ�
        kPacketLengthOffset   = 0,
        kPacketLengthMask     = 0x0000FFFFUL,//length����
        kPacketSourceIDOffset = 4,           //��RTCP�ĵ�4���ֽں���,packet sender SSRC
        kPacketSourceIDSize   = 4,           //sender SSRC��ʶ��ռ4���ֽ�,�μ������RTCPReceiverPacket��
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


//������һ��RTCP RR����ȥRTCP��ͷ�Ĳ���, ����ReportBlock����,��ϸ������ReportBlock�ڵĲ���
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
    inline int RecordOffset(int inReportNum);//ע����������ǳ���Ҫ,����������Ҫ��!

    UInt8* fRTCPReceiverReportArray;    //points into fReceiverPacketBuffer,ָ��ReportBlock����,�μ�RTCPReceiverPacket::ParseReceiverReport()

    enum
    {
        kReportBlockOffsetSizeInBytes = 24,          //ReportBlockռ6��UInt32,All are UInt32s
        kReportBlockOffset            = kPacketSourceIDOffset + kPacketSourceIDSize,//ReportBlock������RTCP���е�ƫ����8���ֽ�  
        kReportSourceIDOffset         = 0,           //SSRC for this report
        kFractionLostOffset           = 4,
        kFractionLostMask             = 0xFF000000UL,//ռ1���ֽ�
        kFractionLostShift            = 24,          //��һ��32bit��������ƽ��24bit�õ�
        kTotalLostPacketsOffset       = 4,           //��һ��32bit��������ƽ��4bit�õ�
        kTotalLostPacketsMask         = 0x00FFFFFFUL,//ռ3���ֽ�
        kHighestSeqNumReceivedOffset  = 8,           //��һ��ReportBlock�ڵĵ�8���ֽں�
        kJitterOffset                 = 12,          //��һ��ReportBlock�ڵĵ�12���ֽں�
        kLastSenderReportOffset       = 16,          //��һ��ReportBlock�ڵĵ�16���ֽں�
        kLastSenderReportDelayOffset  = 20           //��һ��ReportBlock�ڵĵ�20���ֽں�
    };

};

/******************************  RTCPPacket  inlines ***************************/

//��ȡRTCP�������ֽ�,ת���ֽ����,ȡ���ֽڵ�ǰ������,������30���صõ�,��RTCP���İ汾��
inline int RTCPPacket::GetVersion()
{
    UInt32* theVersionPtr = (UInt32*)&fReceiverPacketBuffer[kVersionOffset];
    UInt32 theVersion = ntohl(*theVersionPtr); 
    return (int) ((theVersion  & kVersionMask) >> kVersionShift);
}

//��ȡpaddingλ��ֵ
inline Bool16 RTCPPacket::GetHasPadding()
{
    UInt32* theHasPaddingPtr = (UInt32*)&fReceiverPacketBuffer[kHasPaddingOffset];
    UInt32 theHasPadding = ntohl(*theHasPaddingPtr);
    return (Bool16) (theHasPadding & kHasPaddingMask);
}

//��ȡRC��(ReportBlock�ĸ���)��ֵ
inline int RTCPPacket::GetReportCount()
{
    UInt32* theReportCountPtr = (UInt32*)&fReceiverPacketBuffer[kReportCountOffset];
    UInt32 theReportCount = ntohl(*theReportCountPtr);
    return (int) ((theReportCount & kReportCountMask) >> kReportCountShift);
}

//��ȡRTCP��������
inline UInt8 RTCPPacket::GetPacketType()
{
    UInt32* thePacketTypePtr = (UInt32*)&fReceiverPacketBuffer[kPacketTypeOffset];
    UInt32 thePacketType = ntohl(*thePacketTypePtr);
    return (UInt8) ((thePacketType & kPacketTypeMask) >> kPacketTypeShift);
}

//��ȡRTCP���İ�����(����ͷ4���ֽ�)
inline UInt16 RTCPPacket::GetPacketLength()
{
    return (UInt16) ( ntohl(*(UInt32*)&fReceiverPacketBuffer[kPacketLengthOffset]) & kPacketLengthMask);
}

//��ȡRTCP��ͬ��Դ
inline UInt32 RTCPPacket::GetPacketSSRC()
{
    return (UInt32) ntohl(*(UInt32*)&fReceiverPacketBuffer[kPacketSourceIDOffset]) ;
}

//��ȡRTCP��ͷ�����ֽ�
inline SInt16 RTCPPacket::GetHeader(){ return (SInt16) ntohs(*(SInt16*)&fReceiverPacketBuffer[0]) ;}

/*************************  RTCPReceiverPacket  inlines *******************************/

//����ָ�����-1��ReportBlock��RTCP RRʵ���е�ƫ��
inline int RTCPReceiverPacket::RecordOffset(int inReportNum) 
{
    return inReportNum*kReportBlockOffsetSizeInBytes;
}   

//�õ���ReportBlock��SSRC
inline UInt32 RTCPReceiverPacket::GetReportSourceID(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kReportSourceIDOffset]) ;
}

//�õ���ReportBlock��FractionLost
inline UInt8 RTCPReceiverPacket::GetFractionLostPackets(int inReportNum)
{
    return (UInt8) ( (ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kFractionLostOffset]) & kFractionLostMask) >> kFractionLostShift );
}

//�õ���ReportBlock���ܶ�����
inline UInt32 RTCPReceiverPacket::GetTotalLostPackets(int inReportNum)
{
    return (ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kTotalLostPacketsOffset]) & kTotalLostPacketsMask );
}

//�õ���ReportBlock���յ������Seq Number
inline UInt32 RTCPReceiverPacket::GetHighestSeqNumReceived(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kHighestSeqNumReceivedOffset]) ;
}

//�õ���ReportBlock�Ĵ���Jitter
inline UInt32 RTCPReceiverPacket::GetJitter(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kJitterOffset]) ;
}

//�õ���ReportBlock����һ��SenderReportTime
inline UInt32 RTCPReceiverPacket::GetLastSenderReportTime(int inReportNum)
{
    return (UInt32) ntohl(*(UInt32*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum)+kLastSenderReportOffset]) ;
}

//�õ���ReportBlock����һ��SenderReport���ӳ�
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
