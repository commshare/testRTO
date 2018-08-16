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



//����RTCP��ͷ�Ƿ�Ϸ�?
//returns true if successful, false otherwise
Bool16 RTCPPacket::ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen)
{
	//����RTCP��ͷ<8���ֽ�,�Ƿ�,ֱ�ӷ���false
    if (inPacketLen < kRTCPPacketSizeInBytes)
        return false;
    fReceiverPacketBuffer = inPacketBuffer;

    //the length of this packet can be no less than the advertised length (which is
    //in 32-bit words, so we must multiply) plus the size of the header (4 bytes)
	//�������<�ð�ָ�����ֽ���+��ͷ8�ֽ�,�Ƿ�,ֱ�ӷ���false
    if (inPacketLen < (UInt32)((this->GetPacketLength() * 4) + kRTCPHeaderSizeInBytes))
        return false;
    
    //do some basic validation on the packet
	//���粻��ָ���İ汾��,�Ƿ�,ֱ�ӷ���false
    if (this->GetVersion() != kSupportedRTCPVersion)
        return false;
        
    return true;
}

//����Ļ�ϴ�ӡ��RTCP��ͷ��������Ϣ
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
//��ӡһ��RR����������Ϣ(����RTCP��ͷ+����ReportBlock)
void RTCPReceiverPacket::Dump()//Override
{
    RTCPPacket::Dump(); //�ȴ�ӡRTCP��ͷ��������Ϣ
    
	//�������ӡÿ��ReportBlock��������Ϣ
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
	//���Ƚ���RTCP��ͷ�Ƿ�Ϸ�?
    Bool16 ok = this->ParsePacket(inPacketBuffer, inPacketLength);
    if (!ok)
        return false;
    
	//�õ�ReportBlock����λ��
    fRTCPReceiverReportArray = inPacketBuffer + kRTCPPacketSizeInBytes;
    
    //this is the maximum number of reports there could possibly be
	//�õ�ReportBlock�ĸ���
    int theNumReports = (inPacketLength - kRTCPPacketSizeInBytes) / kReportBlockOffsetSizeInBytes;

    //if the number of receiver reports is greater than the theoretical limit, return an error.
	//�������õ���ReportBlock�ĸ���<RC���ֵ,��RR���Ƿ�,����false
    if (this->GetReportCount() > theNumReports)
        return false;
        
    return true;
}

//�õ�����ReportBlock�Ķ����ٷֱȵ�ƽ����
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

//�õ�����ReportBlock�����綶����ƽ����
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

//�õ�����ReportBlock���ܶ�������ƽ����
UInt32 RTCPReceiverPacket::GetCumulativeTotalLostPackets()
{
    UInt32 totalLostPackets = 0;
    for (short i = 0; i < this->GetReportCount(); i++)
    {
        totalLostPackets += this->GetTotalLostPackets(i);
    }
    
    return totalLostPackets;
}







