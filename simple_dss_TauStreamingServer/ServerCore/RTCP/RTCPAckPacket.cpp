
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPAckPacket.cpp
Description: Represents a RTCP Ack packet objects sent by client,and we use it
             to caculate the RTT/RTO in karn's algorithm.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTCPAckPacket.h"
#include "RTCPPacket.h"
#include "MyAssert.h"
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include <stdio.h>



//����ָ���İ����ݺ��䳤��,�����������Ƿ���ack��qtak��,�䳤���Ƿ�Ϸ�?�����Ϸ�ʱ,ֱ�ӷ���;����,�������ݳ�ԱfRTCPAckBuffer��fAckMaskSize
Bool16 RTCPAckPacket::ParseAckPacket(UInt8* inPacketBuffer, UInt32 inPacketLen)
{
    fRTCPAckBuffer = inPacketBuffer;//��ȡAck�Ļ�����ʼָ��

    // ���������<20�ֽ�,�������Ͳ���ack��qtak��,��������false
    // Check whether this is an ack packet or not.
    if ((inPacketLen < kAckMaskOffset) || (!this->IsAckPacketType()))
        return false;
    
	// ��UInt32�ȴ�ɢΪ�ַ�,����4�ֽڵ�RTCPͷ��Ӻ�,����ΪUInt32
    Assert(inPacketLen == (UInt32)((this->GetPacketLength() * 4)) + RTCPPacket::kRTCPHeaderSizeInBytes);
    fAckMaskSize = inPacketLen - kAckMaskOffset; //���Ack����Maskλ����ֽ���(����4���ֽ�)
    return true;
}

//�Ȼ��APP������,���ж��Ƿ���ack��qtak��?�Ƿ���true,���򷵻�false
Bool16 RTCPAckPacket::IsAckPacketType()
{
    // While we are moving to a new type, check for both, ���APP������
    UInt32 theAppType = ntohl(*(UInt32*)&fRTCPAckBuffer[kAppPacketTypeOffset]);
    
//  if ( theAppType == kOldAckPacketType ) qtss_printf("ack\n"); 
//  if ( theAppType == kAckPacketType ) qtss_printf("qtack\n");
                                                        
    return this->IsAckType(theAppType);
}

//����Ļ�ϴ�ӡ��Ack��������,ע��Mask������ϵ
void   RTCPAckPacket::Dump()
{
    UInt16 theSeqNum = this->GetAckSeqNum();
    UInt16 thePacketLen = this->GetPacketLength(); //����RTCP��ͷ1���ֽ�
    UInt32 theAckMaskSizeInBits = this->GetAckMaskSizeInBits(); //�õ�Maskλ���bit��
    
    char name[5];
    name[4] = 0; //�ַ�����ĩβ��0�ض�
    
    ::memcpy(name, &fRTCPAckBuffer[kAppPacketTypeOffset],4); //��ȡRTCP�İ�������

    UInt16 numBufferBytes = (UInt16) ( (7 * theAckMaskSizeInBits) + 1 );
    char *maskBytesBuffer = NEW char[numBufferBytes];
    OSCharArrayDeleter deleter(maskBytesBuffer);
    maskBytesBuffer[0] = 0; //�ַ�������β����0
    maskBytesBuffer[numBufferBytes -1] = 0;

	//���������к������䵽Mask�Ļ���
    for (UInt32 maskCount = 0; maskCount < theAckMaskSizeInBits; maskCount++)
    {
        if (this->IsNthBitEnabled(maskCount))
        {
            qtss_sprintf(&maskBytesBuffer[::strlen(maskBytesBuffer)],"%lu, ", theSeqNum + 1 + maskCount);
        }
    }
    Assert(::strlen(maskBytesBuffer) < numBufferBytes);
    qtss_printf(" H_name=%s H_seq=%u H_len=%u mask_size=%lu seq_nums_bit_set=%s\n",
                  name, theSeqNum,thePacketLen,theAckMaskSizeInBits, maskBytesBuffer);

}

