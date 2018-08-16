
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



//输入指定的包数据和其长度,解析其类型是否是ack或qtak包,其长度是否合法?当不合法时,直接返回;否则,配置数据成员fRTCPAckBuffer和fAckMaskSize
Bool16 RTCPAckPacket::ParseAckPacket(UInt8* inPacketBuffer, UInt32 inPacketLen)
{
    fRTCPAckBuffer = inPacketBuffer;//获取Ack的缓存起始指针

    // 假如包长度<20字节,或者类型不是ack或qtak包,立即返回false
    // Check whether this is an ack packet or not.
    if ((inPacketLen < kAckMaskOffset) || (!this->IsAckPacketType()))
        return false;
    
	// 将UInt32先打散为字符,在与4字节的RTCP头相加后,换算为UInt32
    Assert(inPacketLen == (UInt32)((this->GetPacketLength() * 4)) + RTCPPacket::kRTCPHeaderSizeInBytes);
    fAckMaskSize = inPacketLen - kAckMaskOffset; //获得Ack包的Mask位域的字节数(至少4个字节)
    return true;
}

//先获得APP包类型,再判断是否是ack或qtak包?是返回true,否则返回false
Bool16 RTCPAckPacket::IsAckPacketType()
{
    // While we are moving to a new type, check for both, 检查APP包类型
    UInt32 theAppType = ntohl(*(UInt32*)&fRTCPAckBuffer[kAppPacketTypeOffset]);
    
//  if ( theAppType == kOldAckPacketType ) qtss_printf("ack\n"); 
//  if ( theAppType == kAckPacketType ) qtss_printf("qtack\n");
                                                        
    return this->IsAckType(theAppType);
}

//在屏幕上打印出Ack包的内容,注意Mask的填充关系
void   RTCPAckPacket::Dump()
{
    UInt16 theSeqNum = this->GetAckSeqNum();
    UInt16 thePacketLen = this->GetPacketLength(); //不含RTCP包头1个字节
    UInt32 theAckMaskSizeInBits = this->GetAckMaskSizeInBits(); //得到Mask位域的bit数
    
    char name[5];
    name[4] = 0; //字符数组末尾以0截断
    
    ::memcpy(name, &fRTCPAckBuffer[kAppPacketTypeOffset],4); //获取RTCP的包类型名

    UInt16 numBufferBytes = (UInt16) ( (7 * theAckMaskSizeInBits) + 1 );
    char *maskBytesBuffer = NEW char[numBufferBytes];
    OSCharArrayDeleter deleter(maskBytesBuffer);
    maskBytesBuffer[0] = 0; //字符数组首尾都是0
    maskBytesBuffer[numBufferBytes -1] = 0;

	//将包的序列号逐个填充到Mask的缓存
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

