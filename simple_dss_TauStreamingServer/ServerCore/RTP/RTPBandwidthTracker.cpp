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



/* used in RTPSession::Play(),入参由点播影片的平均比特率和Windows threshold决定,一般为24/48/64kbytes */
/* 在开始发送一个RTPStream数据流之前,根据入参,需要对Client Window,congestion Window和fSlowStartThreshold赋初始值 */
void RTPBandwidthTracker::SetWindowSize( SInt32 clientWindowSize )
{
    // Currently we only allow this info to be set once
	// 当前仅允许这些窗口大小只被赋值一次,假如Client端接收缓存已被设置,立即返回
    if (fClientWindow > 0)
        return;
        
    // call SetWindowSize once the clients buffer size is known
    // since this occurs before the stream starts to send
    //一旦知道Client buffer大小,就要调用RTPBandwidthTracker::SetWindowSize()设置rwnd大小    
    fClientWindow = clientWindowSize;
    fLastCongestionAdjust = 0;/* 上一次拥塞控制时间 */

	/* 因为RTPBandwidthTracker类含在RTPPacketResender类中 */
#if RTP_PACKET_RESENDER_DEBUGGING   
    //� test to see what happens w/o slow start at beginning
    //if ( initSlowStart )
    //  qtss_printf( "ack list initializing with slow start.\n" );
    //else
    //  qtss_printf( "ack list initializing at full speed.\n" );
#endif
     
	/* 假如使用慢启动算法而非全速启动,这由生成该RTPBandwidthTracker实例的入参决定 */
    if ( fUseSlowStart )
    {
		/* 设置慢启动阈值大小,最小为24*3/4=18Kbytes */
        fSlowStartThreshold = clientWindowSize * 3 / 4;
        
		//这里与TCP的慢启动算法有区别，由于在网络延时严重或码流大的情况下，获得对方的确认常常需要很长的时间，
		//因此这里稍作改变，让cwnd为rwnd的一半，而不是像TCP那样只是一个报文段的长度。
        //
        // This is a change to the standard TCP slow start algorithm(标准TCP慢启动算法). What
        // we found was that on high bitrate high latency networks (a DSL connection, perhaps),
        // it took just too long for the ACKs to come in and for the window size to
        // grow enough. So we cheat a bit.
        fCongestionWindow = clientWindowSize / 2;
        //fCongestionWindow = kMaximumSegmentSize;
    }
	//如果不使用慢启动算法,而是full speed全速启动，设置ssthresh和cwnd为rwnd大小，发送的数据只要不超过rwnd大小就可以了。
    else
    {   
        fSlowStartThreshold = clientWindowSize;
        fCongestionWindow = clientWindowSize;
    }
    
	//最后作一下保护,保证ssthresh不小于一个MSS(1466个字节),一般来说,这不可能达到
    if ( fSlowStartThreshold < kMaximumSegmentSize )
        fSlowStartThreshold = kMaximumSegmentSize;
}

/******************************** 注意:该函数超级重要,在RTPPacketResender.cpp中多次引用!! *****************************************/

/* 根据新的被Ack(确认)的字节数，按照慢启动中一定的拥塞控制算法调整fCongestionWindow,并且在第二个入参允许的情况下,更新当前未得到确认的字节数fBytesInList。
分别在处理play请求、收到包确认Ack、数据包被放弃重发(参见RTPPacketResender::AckPacket())，三种情况下被调用。 */
void RTPBandwidthTracker::EmptyWindow( UInt32 bytesIncreased, Bool16 updateBytesInList )
{   
	/* 假如没有被Ack的字节,直接返回 */
    if (bytesIncreased == 0)
        return;
     
	/* 确保两方窗口(congestion Window,Client Window)都存在 */
    Assert(fClientWindow > 0 && fCongestionWindow > 0);
    
	//一般情况下bytesIncreased(常常指新得到确认的字节数)小于当前窗口中未得到确认的字节数fBytesInList，
	//一旦出现大于的情况，就设置bytesIncreased为fBytesInList(截断部分忽略)。
    if(fBytesInList < bytesIncreased)
        bytesIncreased = fBytesInList;
     
	//如果"更新当前窗口中未得到确认的字节数"的标志为真，那么更新fBytesInList,即现在没有得到Ack的字节数减少了
    if (updateBytesInList)
        fBytesInList -= bytesIncreased;

    // this assert hits
	/* 确保没Ack的数据低于,比如24+2=26Kbytes */
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0
    
    // update the congestion window by the number of bytes just acknowledged.
    //当congestion Window(cwd)超过慢启动门限时,使用拥塞控制,根据新得到确认的字节数按照一定的算法更新服务器的阻塞窗cwnd       
    if ( fCongestionWindow >= fSlowStartThreshold )
    {
        // when we hit the slow start threshold, only increase the window for each window full of acks.
		//如果cwnd大小超过了ssthresh,进入拥塞避免阶段,按照karn算法，该表达式应是: fCongestionWindow += bytesIncreased / fCongestionWindow;
		//这是由于在高码流且网络延时严重的情况下,要收到对方的确认需要较长时间,因此对该算法作一个调整,以避免受cwnd大小限制而降低了发送的速率。
		/***************  注意该算法!  *************************/
        fCongestionWindow += bytesIncreased * bytesIncreased / fCongestionWindow;
		/***************  注意该算法!  *************************/
    }
    else
        // This is a change to the standard TCP slow start algorithm. What
        // we found was that on high bitrate high latency networks (a DSL connection, perhaps),
        // it took just too long for the ACKs to come in and for the window size to grow enough. So we cheat a bit.
        //较之标准的TCP慢启动算法,略有不同
		//如果cwnd大小还在ssthresh以下，不使用拥塞控制，那么跟karn算法一样，增加cwnd,用已Ack(确认)的字节数。
        fCongestionWindow += bytesIncreased;

    
	/* 由于cwd有不断增大的可能,就要始终确保cwd不超过Client Window */
    if ( fCongestionWindow > fClientWindow )
        fCongestionWindow = fClientWindow;
    
//  qtss_printf("Window = %d, %d left\n", fCongestionWindow, fCongestionWindow-fBytesInList);
}

/* 每隔250ms,按照一定的算法,重新调整ssthresh和cwnd的大小(都不能小于1466个字节)，在每次循环重发数据包(参见RTPPacketResender::ResendDueEntries())后，即超时发生的时候被调用。 */
void RTPBandwidthTracker::AdjustWindowForRetransmit()
{
    // this assert hits 确保发送但没收到确认Ack的数据低于指定范围
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0

    // slow start says that we should reduce the new ss threshold to 1/2
    // of where started getting errors ( the current congestion window size )        
    // so, we get a burst of re-transmit because our RTO was mis-estimated
    // it doesn't seem like we should lower the threshold for each one.
    // it seems like we should just lower it when we first enter the re-transmit "state" 
//  if ( !fIsRetransmitting )
//      fSlowStartThreshold = fCongestionWindow/2;
    
    // make sure that it is at least 1 packet
	// 保证ssthresh不小于一个(RTP)数据包的长度1466字节
    if ( fSlowStartThreshold < kMaximumSegmentSize )
        fSlowStartThreshold = kMaximumSegmentSize;

	//再次启动满窗口报文计数
    // start the full window segemnt counter over again.
    fSlowStartByteCount = 0;
    
	/* 获取当前时间 */
    SInt64 theTime = OS::Milliseconds();

	/* TCP慢启动算法中，将ssthresh设为当前cwnd的一半，而将cwnd的大小设为一个数据包的长度，但这里不这么做，
	在需要重发的情况下，隔250ms重新设置一次ssthresh和cwnd大小。常数设置类似RTPBandwidthTracker::SetWindowSize() */
    if (theTime - fLastCongestionAdjust > 250)
    {
		/* 注意这两个常数设置类似RTPBandwidthTracker::SetWindowSize(),仅把Client Window改为congest window */
        fSlowStartThreshold = fCongestionWindow * 3 / 4;
        fCongestionWindow = fCongestionWindow / 2;	
        fLastCongestionAdjust = theTime;/* 及时更新上次拥塞控制时间  */
    }

/*
    if ( fSlowStartThreshold < fCongestionWindow )
        fCongestionWindow = fSlowStartThreshold/2;
    else
        fCongestionWindow = fCongestionWindow /2;
*/  
      
	//如上表达式所示，如果网络状况不好，cwnd会越来越小，因此这里保证cwnd大小不小于一个RTP数据包大小。
    if ( fCongestionWindow < kMaximumSegmentSize )
        fCongestionWindow = kMaximumSegmentSize;

    // qtss_printf("Congestion window now %d\n", fCongestionWindow);
	/* 现在可以重传了 */
    fIsRetransmitting = true;
}

//用入参来更新fRunningAverageMSecs,fRunningMeanDevationMSecs,并运用它们来更新fUnadjustedRT和fCurRetransmitTimeout,并确保后者在600-2400ms内.
//在RTPPacketResender::AckPacket()/ResendDueEntries()中,采用karn算法对当前RTO(RetransmitTimeout)进行估计,将得到的fCurRetransmitTimeout作为当前包是否重发的时间依据。
/* 在两种情况下，需要作估计RTO运算：一是当服务器收到一个确认数据包，且该包是一次发送成功，就将该包从
发送到收到确认的时间间隔作为一个RTO估计的样本（参见RTPPacketResender::AckPacket（））；二是某数据包第一次被重发后，
到了超时发送时间时还没收到Ack包,将上次估计的RTO乘以1.5，作为下个RTO估计的样本（参见RTPPacketResender::ResendDueEntries()）。 */
void RTPBandwidthTracker::AddToRTTEstimate( SInt32 rttSampleMSecs )
{
//  qtss_printf("%d ", rttSampleMSecs);
//  static int count = 0;
//  if ((count++ % 10) == 0) qtss_printf("\n");
    
    // this assert hits
    Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0

	/* 当avg是0时,用入参的2**3=8倍来更新它 */
    if ( fRunningAverageMSecs == 0 )
        fRunningAverageMSecs = rttSampleMSecs * 8;  // init avg to cur sample, scaled by 2**3,注意RunningAverageMSecs()中会除8 

	/* 得出入参测量结果与当前RTT估计器之差 */
    SInt32 delta = rttSampleMSecs - fRunningAverageMSecs / 8; // scale average back to get cur delta from sample
    
    // add 1/8 the delta back to the smooth running average
    fRunningAverageMSecs = fRunningAverageMSecs + delta; // same as: rt avg = rt avg + delta / 8, but scaled
    
	/* 确保delta是非负值 */
    if ( delta < 0 )
        delta = -1*delta;   // absolute value(绝对值) 
    
/* 
    fRunningMeanDevationMSecs is kept scaled by 4,so this is the same as
    fRunningMeanDevationMSecs = fRunningMeanDevationMSecs + ( |delta| - fRunningMeanDevationMSecs ) /4;
*/
    
	//计算均值偏差
    fRunningMeanDevationMSecs += delta - fRunningMeanDevationMSecs / 4;
    
    //得到RTO的估计值
	/********************** 注意该算法非常重要!! ******************************/
	/* RTPBandwidthTracker::UpdateAckTimeout()中用到第一个量 */
    fUnadjustedRTO = fCurRetransmitTimeout = fRunningAverageMSecs / 8 + fRunningMeanDevationMSecs;
	/********************** 注意该算法非常重要!! ******************************/
    
    // rto should not be too low..
	//保证估计出的RTO值不小于RTO下限600ms
    if ( fCurRetransmitTimeout < kMinRetransmitIntervalMSecs )  
        fCurRetransmitTimeout = kMinRetransmitIntervalMSecs;
    
    // or too high...
	// 保证估计出的RTO值不大于RTO上限24000ms
    if ( fCurRetransmitTimeout > kMaxRetransmitIntervalMSecs )
        fCurRetransmitTimeout = kMaxRetransmitIntervalMSecs;
//  qtss_printf("CurTimeout == %d\n", fCurRetransmitTimeout);
}

/* used in RTPSession::Run() */
/* 对congestion Window和RTO估计值fUnadjustedRTO,分别更新它们的最大最小值,并累计它们的总和 */
void RTPBandwidthTracker::UpdateStats()
{
	/* 检测cwnd的次数增加1 */
    fNumStatsSamples++;
    
	/* 调整最大和最小 congestion Windows大小 */
    if (fMaxCongestionWindowSize < fCongestionWindow)
        fMaxCongestionWindowSize = fCongestionWindow;
    if (fMinCongestionWindowSize > fCongestionWindow)
        fMinCongestionWindowSize = fCongestionWindow;
     
	/* 记录fUnadjustedRTO的最大最小值 */
    if (fMaxRTO < fUnadjustedRTO)
        fMaxRTO = fUnadjustedRTO;
    if (fMinRTO > fUnadjustedRTO)
        fMinRTO = fUnadjustedRTO;

	/* 累计congestion Windows的总大小 */
    fTotalCongestionWindowSize += fCongestionWindow;
	/* 累计fUnadjustedRTO的总大小 */
    fTotalRTO += fUnadjustedRTO;
}

/* 利用电影当前Bitrate,和用karn算法得到的RTO值,来计算等待当前Ack到来的时间fAckTimeout,并确保它在20到100之间 */
void RTPBandwidthTracker::UpdateAckTimeout(UInt32 bitsSentInInterval, SInt64 intervalLengthInMsec)
{
    // First figure out how long it will take us to fill up our window, based on the movie's current bit rate
	/* 按照电影当前Bitrate,计算需要多长毫秒能填满congestion Window */
    UInt32 unadjustedTimeout = 0;
    if (bitsSentInInterval > 0)
        unadjustedTimeout = (UInt32) ((intervalLengthInMsec * fCongestionWindow) / bitsSentInInterval);

    // If we wait that long, that's too long because we need to actually wait for the ack to arrive.
    // So, subtract 1/2 the rto - the last ack timeout
    UInt32 rto = (UInt32)fUnadjustedRTO;
    if (rto < fAckTimeout)
        rto = fAckTimeout;

	/* 以下用RTPBandwidthTracker::AddToRTTEstimate()中计算的fUnadjustedRTO来计算调整adjustment */
    UInt32 adjustment = (rto - fAckTimeout) / 2;
    //qtss_printf("UnadjustedTimeout = %lu. rto: %ld. Last ack timeout: %lu. Adjustment = %lu.", unadjustedTimeout, fUnadjustedRTO, fAckTimeout, adjustment);
    if (adjustment > unadjustedTimeout)
        adjustment = unadjustedTimeout;

	/*************** 非常重要! *******************/
    /* 用预计等待时间-调整时间差,来计算fAckTimeout */
    fAckTimeout = unadjustedTimeout - adjustment;
	/*************** 非常重要! *******************/
    
    //qtss_printf("AckTimeout: %lu\n",fAckTimeout);

	/* 调整fAckTimeout在20到100之间 */
    if (fAckTimeout > kMaxAckTimeout)
        fAckTimeout = kMaxAckTimeout;
    else if (fAckTimeout < kMinAckTimeout)
        fAckTimeout = kMinAckTimeout;
}
