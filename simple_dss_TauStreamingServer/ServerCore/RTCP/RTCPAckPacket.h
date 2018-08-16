
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPAckPacket.h
Description: Represents a RTCP Ack packet objects sent by client,and we use it
             to caculate the RTT/RTO in karn's algorithm.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _RTCPACKPACKET_H_
#define _RTCPACKPACKET_H_

#include <stdlib.h>
#include <netinet/in.h>
#include "OSHeaders.h"
#include "SafeStdLib.h"



class RTCPAckPacket
{
    public:

        // This class is not derived from RTCPPacket as a performance optimization.
        // Instead, it is assumed that the RTCP packet validation has already been done.
        // 该类不是RTCP包的派生类, 但它假设RTCP包的合法性已经得到验证,数据成员赋值参见RTCPAckPacket::ParseAckPacket()
        RTCPAckPacket() : fRTCPAckBuffer(NULL), fAckMaskSize(0) {}
        virtual ~RTCPAckPacket() {}
        
        // Returns true if this is an Ack packet, false otherwise.
        // Assumes that inPacketBuffer is a pointer to a valid RTCP packet header.
        Bool16 ParseAckPacket(UInt8* inPacketBuffer, UInt32 inPacketLen);

        inline UInt16 GetAckSeqNum();
		inline UInt16 GetPacketLength();

		//下面两个函数很重要,要认真揣摩!
        inline UInt32 GetAckMaskSizeInBits() { return fAckMaskSize * 8; }
        inline Bool16 IsNthBitEnabled(UInt32 inBitNumber);
        
        void   Dump();
    private:
    
        UInt8* fRTCPAckBuffer; //Ack包的缓存指针,参见RTCPAckPacket::ParseAckPacket()
        UInt32 fAckMaskSize;   //Ack包的Mask位域的大小

        Bool16 IsAckPacketType();
        
        enum
        {
            kAckPacketType          = FOUR_CHARS_TO_INT('q', 't', 'a', 'k'), // 'qtak'  documented Apple reliable UDP packet type
            kOldAckPacketType       = FOUR_CHARS_TO_INT('a', 'c', 'k', ' '), // 'ack ' required by QT 5 and earlier
            kAppPacketTypeOffset    = 8,
            kAckSeqNumOffset        = 16,
            kAckMaskOffset          = 20,
            kPacketLengthMask       = 0x0000FFFFUL,
        };
        

        inline Bool16 IsAckType(UInt32 theAppType) {    return ( (theAppType == kOldAckPacketType) || (theAppType == kAckPacketType) );}
};


Bool16 RTCPAckPacket::IsNthBitEnabled(UInt32 inBitNumber)
{
    // Don't need to do endian conversion because we're dealing with 8-bit numbers
    UInt8 bitMask = 128;
    return *(fRTCPAckBuffer + kAckMaskOffset + (inBitNumber >> 3)) & (bitMask >>= inBitNumber & 7);
}

//得到Ack包的序列号
UInt16 RTCPAckPacket::GetAckSeqNum()
{
    return (UInt16) (ntohl(*(UInt32*)&fRTCPAckBuffer[kAckSeqNumOffset]));
}

//得到Ack包的长度(它位于该包的第3和4个字节,不包含4个字符的RTCP包头),参见RTCPAckPacket::ParseAckPacket()
inline UInt16 RTCPAckPacket::GetPacketLength()
{
    return (UInt16) ( ntohl(*(UInt32*)fRTCPAckBuffer) & kPacketLengthMask);
}




/*
6.6 Ack Packet format

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |   PT=APP=204  |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)  = 'qtak'               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Reserved             |          Seq num              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Mask...                                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   
 */

#endif //_RTCPAPPPACKET_H_
