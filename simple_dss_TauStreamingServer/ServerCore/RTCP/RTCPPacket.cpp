/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPPacket.cpp
Description: Represents a common RTCP Packet header object.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "RTCPPacket.h"
#include "RTCPAckPacket.h"
#include "OS.h"
#include <stdio.h>



//解析RTCP包头是否合法?
//returns true if successful, false otherwise
Bool16 RTCPPacket::ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen)
{
	//假如RTCP包头<8个字节,非法,直接返回false
    if (inPacketLen < kRTCPPacketSizeInBytes)
        return false;
    fReceiverPacketBuffer = inPacketBuffer;

    //the length of this packet can be no less than the advertised length (which is
    //in 32-bit words, so we must multiply) plus the size of the header (4 bytes)
	//假如入参<该包指定的字节数+包头8字节,非法,直接返回false
    if (inPacketLen < (UInt32)((this->GetPacketLength() * 4) + kRTCPHeaderSizeInBytes))
        return false;
    
    //do some basic validation on the packet
	//假如不是指定的版本号,非法,直接返回false
    if (this->GetVersion() != kSupportedRTCPVersion)
        return false;
        
    return true;
}

//在屏幕上打印该RTCP包头的所有信息
void RTCPPacket::Dump()
{  
	qtss_printf( "H_vers=%d, H_pad=%d, H_rprt_count=%d, H_type=%d, H_length=%d, H_ssrc=%ld\n",
		this->GetVersion(),
		(int)this->GetHasPadding(),
		this->GetReportCount(),
		(int)this->GetPacketType(),
		(int)this->GetPacketLength(),
		this->GetPacketSSRC() );
}


/* see RTPStream::ProcessIncomingRTCPPacket()  */
//打印一个RR包的所有信息(包括RTCP包头+若干ReportBlock)
void RTCPReceiverPacket::Dump()//Override
{
    RTCPPacket::Dump(); //先打印RTCP包头的所有信息
    
	//再逐个打印每个ReportBlock的所有信息
    for (int i = 0;i<this->GetReportCount(); i++)
    {
        qtss_printf( "   [%d] H_ssrc=%lu, H_frac_lost=%d, H_tot_lost=%lu, H_high_seq=%lu H_jit=%lu, H_last_sr_time=%lu, H_last_sr_delay=%lu \n",
                             i,
                             this->GetReportSourceID(i),
                             this->GetFractionLostPackets(i),
                             this->GetTotalLostPackets(i),
                             this->GetHighestSeqNumReceived(i),
                             this->GetJitter(i),
                             this->GetLastSenderReportTime(i),
                             this->GetLastSenderReportDelay(i) );
    }
}


Bool16 RTCPReceiverPacket::ParseReceiverReport(UInt8* inPacketBuffer, UInt32 inPacketLength)
{
	//首先解析RTCP包头是否合法?
    Bool16 ok = this->ParsePacket(inPacketBuffer, inPacketLength);
    if (!ok)
        return false;
    
	//得到ReportBlock数组位置
    fRTCPReceiverReportArray = inPacketBuffer + kRTCPPacketSizeInBytes;
    
    //this is the maximum number of reports there could possibly be
	//得到ReportBlock的个数
    int theNumReports = (inPacketLength - kRTCPPacketSizeInBytes) / kReportBlockOffsetSizeInBytes;

    //if the number of receiver reports is greater than the theoretical limit, return an error.
	//假如计算得到的ReportBlock的个数<RC域的值,该RR包非法,返回false
    if (this->GetReportCount() > theNumReports)
        return false;
        
    return true;
}

//得到所有ReportBlock的丢包百分比的平均数
UInt32 RTCPReceiverPacket::GetCumulativeFractionLostPackets()
{
    float avgFractionLost = 0;
    for (short i = 0; i < this->GetReportCount(); i++)
    {
        avgFractionLost += this->GetFractionLostPackets(i);
        avgFractionLost /= (i+1);
    }
    
    return (UInt32)avgFractionLost;
}

//得到所有ReportBlock的网络抖动的平均数
UInt32 RTCPReceiverPacket::GetCumulativeJitter()
{
    float avgJitter = 0;
    for (short i = 0; i < this->GetReportCount(); i++)
    {
        avgJitter += this->GetJitter(i);
        avgJitter /= (i + 1);
    }
    
    return (UInt32)avgJitter;
}

//得到所有ReportBlock的总丢包数的平均数
UInt32 RTCPReceiverPacket::GetCumulativeTotalLostPackets()
{
    UInt32 totalLostPackets = 0;
    for (short i = 0; i < this->GetReportCount(); i++)
    {
        totalLostPackets += this->GetTotalLostPackets(i);
    }
    
    return totalLostPackets;
}







