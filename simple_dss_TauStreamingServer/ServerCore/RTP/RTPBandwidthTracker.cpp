/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPBandwidthTracker.cpp
Description: Uses Karns Algorithm to measure exact round trip times(RTT). This also
             tracks the current window size based on input from the caller.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTPBandwidthTracker.h"
#include "MyAssert.h"
#include "OS.h"



/* used in RTPSession::Play(),����ɵ㲥ӰƬ��ƽ�������ʺ�Windows threshold����,һ��Ϊ24/48/64kbytes */
/* �ڿ�ʼ����һ��RTPStream������֮ǰ,�������,��Ҫ��Client Window,congestion Window��fSlowStartThreshold����ʼֵ */
void RTPBandwidthTracker::SetWindowSize( SInt32 clientWindowSize )
{
    // Currently we only allow this info to be set once
	// ��ǰ��������Щ���ڴ�Сֻ����ֵһ��,����Client�˽��ջ����ѱ�����,��������
    if (fClientWindow > 0)
        return;
        
    // call SetWindowSize once the clients buffer size is known
    // since this occurs before the stream starts to send
    //һ��֪��Client buffer��С,��Ҫ����RTPBandwidthTracker::SetWindowSize()����rwnd��С    
    fClientWindow = clientWindowSize;
    fLastCongestionAdjust = 0;/* ��һ��ӵ������ʱ�� */

	/* ��ΪRTPBandwidthTracker�ຬ��RTPPacketResender���� */
#if RTP_PACKET_RESENDER_DEBUGGING   
    //� test to see what happens w/o slow start at beginning
    //if ( initSlowStart )
    //  qtss_printf( "ack list initializing with slow start.\n" );
    //else
    //  qtss_printf( "ack list initializing at full speed.\n" );
#endif
     
	/* ����ʹ���������㷨����ȫ������,�������ɸ�RTPBandwidthTrackerʵ������ξ��� */
    if ( fUseSlowStart )
    {
		/* ������������ֵ��С,��СΪ24*3/4=18Kbytes */
        fSlowStartThreshold = clientWindowSize * 3 / 4;
        
		//������TCP���������㷨������������������ʱ���ػ������������£���öԷ���ȷ�ϳ�����Ҫ�ܳ���ʱ�䣬
		//������������ı䣬��cwndΪrwnd��һ�룬��������TCP����ֻ��һ�����Ķεĳ��ȡ�
        //
        // This is a change to the standard TCP slow start algorithm(��׼TCP�������㷨). What
        // we found was that on high bitrate high latency networks (a DSL connection, perhaps),
        // it took just too long for the ACKs to come in and for the window size to
        // grow enough. So we cheat a bit.
        fCongestionWindow = clientWindowSize / 2;
        //fCongestionWindow = kMaximumSegmentSize;
    }
	//�����ʹ���������㷨,����full speedȫ������������ssthresh��cwndΪrwnd��С�����͵�����ֻҪ������rwnd��С�Ϳ����ˡ�
    else
    {   
        fSlowStartThreshold = clientWindowSize;
        fCongestionWindow = clientWindowSize;
    }
    
	//�����һ�±���,��֤ssthresh��С��һ��MSS(1466���ֽ�),һ����˵,�ⲻ���ܴﵽ
    if ( fSlowStartThreshold < kMaximumSegmentSize )
        fSlowStartThreshold = kMaximumSegmentSize;
}

/******************************** ע��:�ú���������Ҫ,��RTPPacketResender.cpp�ж������!! *****************************************/

/* �����µı�Ack(ȷ��)���ֽ�����������������һ����ӵ�������㷨����fCongestionWindow,�����ڵڶ����������������,���µ�ǰδ�õ�ȷ�ϵ��ֽ���fBytesInList��
�ֱ��ڴ���play�����յ���ȷ��Ack�����ݰ��������ط�(�μ�RTPPacketResender::AckPacket())����������±����á� */
void RTPBandwidthTracker::EmptyWindow( UInt32 bytesIncreased, Bool16 updateBytesInList )
{   
	/* ����û�б�Ack���ֽ�,ֱ�ӷ��� */
    if (bytesIncreased == 0)
        return;
     
	/* ȷ����������(congestion Window,Client Window)������ */
    Assert(fClientWindow > 0 && fCongestionWindow > 0);
    
	//һ�������bytesIncreased(����ָ�µõ�ȷ�ϵ��ֽ���)С�ڵ�ǰ������δ�õ�ȷ�ϵ��ֽ���fBytesInList��
	//һ�����ִ��ڵ������������bytesIncreasedΪfBytesInList(�ضϲ��ֺ���)��
    if(fBytesInList < bytesIncreased)
        bytesIncreased = fBytesInList;
     
	//���"���µ�ǰ������δ�õ�ȷ�ϵ��ֽ���"�ı�־Ϊ�棬��ô����fBytesInList,������û�еõ�Ack���ֽ���������
    if (updateBytesInList)
        fBytesInList -= bytesIncreased;

    // this assert hits
	/* ȷ��ûAck�����ݵ���,����24+2=26Kbytes */
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0
    
    // update the congestion window by the number of bytes just acknowledged.
    //��congestion Window(cwd)��������������ʱ,ʹ��ӵ������,�����µõ�ȷ�ϵ��ֽ�������һ�����㷨���·�������������cwnd       
    if ( fCongestionWindow >= fSlowStartThreshold )
    {
        // when we hit the slow start threshold, only increase the window for each window full of acks.
		//���cwnd��С������ssthresh,����ӵ������׶�,����karn�㷨���ñ��ʽӦ��: fCongestionWindow += bytesIncreased / fCongestionWindow;
		//���������ڸ�������������ʱ���ص������,Ҫ�յ��Է���ȷ����Ҫ�ϳ�ʱ��,��˶Ը��㷨��һ������,�Ա�����cwnd��С���ƶ������˷��͵����ʡ�
		/***************  ע����㷨!  *************************/
        fCongestionWindow += bytesIncreased * bytesIncreased / fCongestionWindow;
		/***************  ע����㷨!  *************************/
    }
    else
        // This is a change to the standard TCP slow start algorithm. What
        // we found was that on high bitrate high latency networks (a DSL connection, perhaps),
        // it took just too long for the ACKs to come in and for the window size to grow enough. So we cheat a bit.
        //��֮��׼��TCP�������㷨,���в�ͬ
		//���cwnd��С����ssthresh���£���ʹ��ӵ�����ƣ���ô��karn�㷨һ��������cwnd,����Ack(ȷ��)���ֽ�����
        fCongestionWindow += bytesIncreased;

    
	/* ����cwd�в�������Ŀ���,��Ҫʼ��ȷ��cwd������Client Window */
    if ( fCongestionWindow > fClientWindow )
        fCongestionWindow = fClientWindow;
    
//  qtss_printf("Window = %d, %d left\n", fCongestionWindow, fCongestionWindow-fBytesInList);
}

/* ÿ��250ms,����һ�����㷨,���µ���ssthresh��cwnd�Ĵ�С(������С��1466���ֽ�)����ÿ��ѭ���ط����ݰ�(�μ�RTPPacketResender::ResendDueEntries())�󣬼���ʱ������ʱ�򱻵��á� */
void RTPBandwidthTracker::AdjustWindowForRetransmit()
{
    // this assert hits ȷ�����͵�û�յ�ȷ��Ack�����ݵ���ָ����Χ
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0

    // slow start says that we should reduce the new ss threshold to 1/2
    // of where started getting errors ( the current congestion window size )        
    // so, we get a burst of re-transmit because our RTO was mis-estimated
    // it doesn't seem like we should lower the threshold for each one.
    // it seems like we should just lower it when we first enter the re-transmit "state" 
//  if ( !fIsRetransmitting )
//      fSlowStartThreshold = fCongestionWindow/2;
    
    // make sure that it is at least 1 packet
	// ��֤ssthresh��С��һ��(RTP)���ݰ��ĳ���1466�ֽ�
    if ( fSlowStartThreshold < kMaximumSegmentSize )
        fSlowStartThreshold = kMaximumSegmentSize;

	//�ٴ����������ڱ��ļ���
    // start the full window segemnt counter over again.
    fSlowStartByteCount = 0;
    
	/* ��ȡ��ǰʱ�� */
    SInt64 theTime = OS::Milliseconds();

	/* TCP�������㷨�У���ssthresh��Ϊ��ǰcwnd��һ�룬����cwnd�Ĵ�С��Ϊһ�����ݰ��ĳ��ȣ������ﲻ��ô����
	����Ҫ�ط�������£���250ms��������һ��ssthresh��cwnd��С��������������RTPBandwidthTracker::SetWindowSize() */
    if (theTime - fLastCongestionAdjust > 250)
    {
		/* ע��������������������RTPBandwidthTracker::SetWindowSize(),����Client Window��Ϊcongest window */
        fSlowStartThreshold = fCongestionWindow * 3 / 4;
        fCongestionWindow = fCongestionWindow / 2;	
        fLastCongestionAdjust = theTime;/* ��ʱ�����ϴ�ӵ������ʱ��  */
    }

/*
    if ( fSlowStartThreshold < fCongestionWindow )
        fCongestionWindow = fSlowStartThreshold/2;
    else
        fCongestionWindow = fCongestionWindow /2;
*/  
      
	//���ϱ��ʽ��ʾ���������״�����ã�cwnd��Խ��ԽС��������ﱣ֤cwnd��С��С��һ��RTP���ݰ���С��
    if ( fCongestionWindow < kMaximumSegmentSize )
        fCongestionWindow = kMaximumSegmentSize;

    // qtss_printf("Congestion window now %d\n", fCongestionWindow);
	/* ���ڿ����ش��� */
    fIsRetransmitting = true;
}

//�����������fRunningAverageMSecs,fRunningMeanDevationMSecs,����������������fUnadjustedRT��fCurRetransmitTimeout,��ȷ��������600-2400ms��.
//��RTPPacketResender::AckPacket()/ResendDueEntries()��,����karn�㷨�Ե�ǰRTO(RetransmitTimeout)���й���,���õ���fCurRetransmitTimeout��Ϊ��ǰ���Ƿ��ط���ʱ�����ݡ�
/* ����������£���Ҫ������RTO���㣺һ�ǵ��������յ�һ��ȷ�����ݰ����Ҹð���һ�η��ͳɹ����ͽ��ð���
���͵��յ�ȷ�ϵ�ʱ������Ϊһ��RTO���Ƶ��������μ�RTPPacketResender::AckPacket������������ĳ���ݰ���һ�α��ط���
���˳�ʱ����ʱ��ʱ��û�յ�Ack��,���ϴι��Ƶ�RTO����1.5����Ϊ�¸�RTO���Ƶ��������μ�RTPPacketResender::ResendDueEntries()���� */
void RTPBandwidthTracker::AddToRTTEstimate( SInt32 rttSampleMSecs )
{
//  qtss_printf("%d ", rttSampleMSecs);
//  static int count = 0;
//  if ((count++ % 10) == 0) qtss_printf("\n");
    
    // this assert hits
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0

	/* ��avg��0ʱ,����ε�2**3=8���������� */
    if ( fRunningAverageMSecs == 0 )
        fRunningAverageMSecs = rttSampleMSecs * 8;  // init avg to cur sample, scaled by 2**3,ע��RunningAverageMSecs()�л��8 

	/* �ó���β�������뵱ǰRTT������֮�� */
    SInt32 delta = rttSampleMSecs - fRunningAverageMSecs / 8; // scale average back to get cur delta from sample
    
    // add 1/8 the delta back to the smooth running average
    fRunningAverageMSecs = fRunningAverageMSecs + delta; // same as: rt avg = rt avg + delta / 8, but scaled
    
	/* ȷ��delta�ǷǸ�ֵ */
    if ( delta < 0 )
        delta = -1*delta;   // absolute value(����ֵ) 
    
/* 
    fRunningMeanDevationMSecs is kept scaled by 4,so this is the same as
    fRunningMeanDevationMSecs = fRunningMeanDevationMSecs + ( |delta| - fRunningMeanDevationMSecs ) /4;
*/
    
	//�����ֵƫ��
    fRunningMeanDevationMSecs += delta - fRunningMeanDevationMSecs / 4;
    
    //�õ�RTO�Ĺ���ֵ
	/********************** ע����㷨�ǳ���Ҫ!! ******************************/
	/* RTPBandwidthTracker::UpdateAckTimeout()���õ���һ���� */
    fUnadjustedRTO = fCurRetransmitTimeout = fRunningAverageMSecs / 8 + fRunningMeanDevationMSecs;
	/********************** ע����㷨�ǳ���Ҫ!! ******************************/
    
    // rto should not be too low..
	//��֤���Ƴ���RTOֵ��С��RTO����600ms
    if ( fCurRetransmitTimeout < kMinRetransmitIntervalMSecs )  
        fCurRetransmitTimeout = kMinRetransmitIntervalMSecs;
    
    // or too high...
	// ��֤���Ƴ���RTOֵ������RTO����24000ms
    if ( fCurRetransmitTimeout > kMaxRetransmitIntervalMSecs )
        fCurRetransmitTimeout = kMaxRetransmitIntervalMSecs;
//  qtss_printf("CurTimeout == %d\n", fCurRetransmitTimeout);
}

/* used in RTPSession::Run() */
/* ��congestion Window��RTO����ֵfUnadjustedRTO,�ֱ�������ǵ������Сֵ,���ۼ����ǵ��ܺ� */
void RTPBandwidthTracker::UpdateStats()
{
	/* ���cwnd�Ĵ�������1 */
    fNumStatsSamples++;
    
	/* ����������С congestion Windows��С */
    if (fMaxCongestionWindowSize < fCongestionWindow)
        fMaxCongestionWindowSize = fCongestionWindow;
    if (fMinCongestionWindowSize > fCongestionWindow)
        fMinCongestionWindowSize = fCongestionWindow;
     
	/* ��¼fUnadjustedRTO�������Сֵ */
    if (fMaxRTO < fUnadjustedRTO)
        fMaxRTO = fUnadjustedRTO;
    if (fMinRTO > fUnadjustedRTO)
        fMinRTO = fUnadjustedRTO;

	/* �ۼ�congestion Windows���ܴ�С */
    fTotalCongestionWindowSize += fCongestionWindow;
	/* �ۼ�fUnadjustedRTO���ܴ�С */
    fTotalRTO += fUnadjustedRTO;
}

/* ���õ�Ӱ��ǰBitrate,����karn�㷨�õ���RTOֵ,������ȴ���ǰAck������ʱ��fAckTimeout,��ȷ������20��100֮�� */
void RTPBandwidthTracker::UpdateAckTimeout(UInt32 bitsSentInInterval, SInt64 intervalLengthInMsec)
{
    // First figure out how long it will take us to fill up our window, based on the movie's current bit rate
	/* ���յ�Ӱ��ǰBitrate,������Ҫ�೤����������congestion Window */
    UInt32 unadjustedTimeout = 0;
    if (bitsSentInInterval > 0)
        unadjustedTimeout = (UInt32) ((intervalLengthInMsec * fCongestionWindow) / bitsSentInInterval);

    // If we wait that long, that's too long because we need to actually wait for the ack to arrive.
    // So, subtract 1/2 the rto - the last ack timeout
    UInt32 rto = (UInt32)fUnadjustedRTO;
    if (rto < fAckTimeout)
        rto = fAckTimeout;

	/* ������RTPBandwidthTracker::AddToRTTEstimate()�м����fUnadjustedRTO���������adjustment */
    UInt32 adjustment = (rto - fAckTimeout) / 2;
    //qtss_printf("UnadjustedTimeout = %lu. rto: %ld. Last ack timeout: %lu. Adjustment = %lu.", unadjustedTimeout, fUnadjustedRTO, fAckTimeout, adjustment);
    if (adjustment > unadjustedTimeout)
        adjustment = unadjustedTimeout;

	/*************** �ǳ���Ҫ! *******************/
    /* ��Ԥ�Ƶȴ�ʱ��-����ʱ���,������fAckTimeout */
    fAckTimeout = unadjustedTimeout - adjustment;
	/*************** �ǳ���Ҫ! *******************/
    
    //qtss_printf("AckTimeout: %lu\n",fAckTimeout);

	/* ����fAckTimeout��20��100֮�� */
    if (fAckTimeout > kMaxAckTimeout)
        fAckTimeout = kMaxAckTimeout;
    else if (fAckTimeout < kMinAckTimeout)
        fAckTimeout = kMinAckTimeout;
}
