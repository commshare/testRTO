
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPBandwidthTracker.h
Description: Uses Karns Algorithm to measure exact round trip times(RTT). This also
             tracks the current window size based on input from the caller.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __RTP_BANDWIDTH_TRACKER_H__
#define __RTP_BANDWIDTH_TRACKER_H__

#include "OSHeaders.h"

class RTPBandwidthTracker
{
    public:

        RTPBandwidthTracker(Bool16 inUseSlowStart)
         :  fRunningAverageMSecs(0),
            fRunningMeanDevationMSecs(0),
            fCurRetransmitTimeout( kMinRetransmitIntervalMSecs ),/* 先设为600ms */
            fUnadjustedRTO( kMinRetransmitIntervalMSecs ),/* 先设为600ms */
            fCongestionWindow(kMaximumSegmentSize),/* 先设置为1466,一个RTP Packet的大小 */
            fSlowStartThreshold(0),/* 设置参见RTPBandwidthTracker::SetWindowSize(),在1466字节和client窗口大小之间 */
            fSlowStartByteCount(0),
            fClientWindow(0),/* 注意初始值为0, used in RTPBandwidthTracker::SetWindowSize() */
            fBytesInList(0), /* RTPStream中发送但未得到确认的字节数,暂设为0 */
            fAckTimeout(kMinAckTimeout),   /* 先初始化为20ms */
            fUseSlowStart(inUseSlowStart), /* 由入参来设置是否使用慢启动算法? */
            fMaxCongestionWindowSize(0),
            fMinCongestionWindowSize(1000000),
            fMaxRTO(0),
            fMinRTO(24000),
            fTotalCongestionWindowSize(0),
            fTotalRTO(0),
            fNumStatsSamples(0)
        {}
        
        ~RTPBandwidthTracker() {}
        
        // Initialization - give the client's window size.
        void SetWindowSize(SInt32 clientWindowSize);
 
        // Before sending new data, let the tracker know
        // how much data you are sending so it can adjust the window size by EmptyWindow.
        void FillWindow(UInt32 inNumBytes)      { fBytesInList += inNumBytes; fIsRetransmitting = false; }        
        
        // When data is acked, let the tracker know how much
        // data was acked so it can adjust the window
        void EmptyWindow(UInt32 inNumBytes, Bool16 updateBytesInList = true);
        
        // When retransmitting a packet, call this function so
        // the tracker can adjust the window sizes and back off.
        void AdjustWindowForRetransmit();

		// Each RTT sample you get, let the tracker know what it is so it can keep a good running average.
		/* 对给定的入参样本,用Karn算法计算RTO */
		void AddToRTTEstimate( SInt32 rttSampleMSecs );
        
        // ACCESSORS
		/* 准备好接收Client Ack了吗? */
        const Bool16 ReadyForAckProcessing()    { return (fClientWindow > 0 && fCongestionWindow > 0); } // see RTPBandwidthTracker::EmptyWindow for requirements
        /* 是否需要流控?当RTPStream中发送但未得到确认的字节数超过阻塞窗的大小时,采用流控 */
		const Bool16 IsFlowControlled()         { return ( (SInt32)fBytesInList >= fCongestionWindow ); }

        const SInt32 ClientWindowSize()         { return fClientWindow; }
        const UInt32 BytesInList()              { return fBytesInList; }
        const SInt32 CongestionWindow()         { return fCongestionWindow; }
        const SInt32 SlowStartThreshold()       { return fSlowStartThreshold; }

        const SInt32 RunningAverageMSecs()      { return fRunningAverageMSecs / 8; }  // fRunningAverageMSecs is stored scaled up 8x
        const SInt32 RunningMeanDevationMSecs() { return fRunningMeanDevationMSecs/ 4; } // fRunningMeanDevationMSecs is stored scaled up 4x

		/* 获取当前restransmit timeout */
        const SInt32 CurRetransmitTimeout()     { return fCurRetransmitTimeout; }
        
		/* 获取当前传输比特率 */
		const SInt32 GetCurrentBandwidthInBps()
            { return (fUnadjustedRTO > 0) ? (fCongestionWindow * 1000) / fUnadjustedRTO : 0; }

		/* 获取Client的ack timeout,在20--100之间 */
        inline const UInt32 RecommendedClientAckTimeout() { return fAckTimeout; }

		/* 利用电影当前Bitrate,和用karn算法得到的RTO值,来计算等待当前Ack到来的时间fAckTimeout,并确保它在20到100之间 */
        void UpdateAckTimeout(UInt32 bitsSentInInterval, SInt64 intervalLengthInMsec);

		/* 对congestion Window和RTO估计值fUnadjustedRTO,分别统计它们的最大最小值,并累计它们的总和 */ 
        void UpdateStats();/* needed by RTPSession::run() */

        // Stats access, 在UpdateStats()中设置这些统计量

		/* 获取最大/最小/平均congestion Windows大小 */
        SInt32   GetMaxCongestionWindowSize()    { return fMaxCongestionWindowSize; }
        SInt32   GetMinCongestionWindowSize()    { return fMinCongestionWindowSize; }
        SInt32   GetAvgCongestionWindowSize()    { return (SInt32)(fTotalCongestionWindowSize / (SInt64)fNumStatsSamples); }

        /* 获取最大/最小/平均RTO(Retransmit Timeout) */
        SInt32   GetMaxRTO()                     { return fMaxRTO; }
        SInt32   GetMinRTO()                     { return fMinRTO; }
        SInt32   GetAvgRTO()                     { return (SInt32)(fTotalRTO / (SInt64)fNumStatsSamples); }
        
        enum
        {
			/* 最大(TCP数据段)报文长度,它是一个RTP包的长度 */
            kMaximumSegmentSize = 1466,  // enet - just a guess!
            
            // Our algorithm for telling the client what the ack timeout
            // is currently not too sophisticated. This could probably be made better.
            // 我们的算法要告诉Client ack timeout值,我们的算法不是太复杂,或许能做得更好些
			// During slow start algorithm, we just use 20, and afterwards, just use 100
			/* Ack的超时范围(ms) */
            kMinAckTimeout = 20,
            kMaxAckTimeout = 100
        };
        
    private:
  
        // For computing the round-trip estimate using Karn's algorithm,参见RTPBandwidthTracker::AddToRTTEstimate()
        SInt32  fRunningAverageMSecs;     //被平滑的RTT，用于估算下一次的RTO
        SInt32  fRunningMeanDevationMSecs;//均值偏差，用于估算下一次RTO

		/************* 这两个量在本类中极其重要!! *****************************/
        SInt32  fCurRetransmitTimeout;//当前估算的重传超时RTO,设置参见RTPBandwidthTracker::AddToRTTEstimate(),注意该量非常重要!!	 
        SInt32  fUnadjustedRTO;       //未经合理范围调整过的RTO,与上fCurRetransmitTimeout值做对比,它不一定在600ms-24000ms范围内
		Bool16  fIsRetransmitting;    // are we in the re-transmit 'state' ( started resending, but have yet to send 'new' data) //是否正处于重传状态?参见RTPBandwidthTracker::AdjustWindowForRetransmit()
        
        // Tracking our window sizes
        SInt64  fLastCongestionAdjust;  //上次拥塞调整时间,参见RTPBandwidthTracker::AdjustWindowForRetransmit()
        SInt32  fCongestionWindow;      // implentation of VJ congestion avoidance  //当前服务器端拥塞窗口大小
        SInt32  fSlowStartThreshold;    // point at which we stop adding to the window for each ack, and add to the window for each window full of acks // 慢启动门限
		SInt32  fSlowStartByteCount;    // counts window a full of acks when past ss(slow start) thresh //?? 超过慢启动阈值后,满窗口报文计数,参见RTPBandwidthTracker::AdjustWindowForRetransmit()
		Bool16  fUseSlowStart;          //使用慢启动算法ss(slow start) algorithm吗?

		
        /*********** NOTE:这个量非常重要!! ***************/
        SInt32  fClientWindow;  // max window size based on client UDP buffer,Client窗口大小,也就是Client端接收缓存大小	
        UInt32  fBytesInList;   // how many unacked bytes on this stream,RTPStream中发送但未得到确认的字节数,参见RTPBandwidthTracker::EmptyWindow()
		/*********** NOTE:这个量非常重要!! ***************/

		/* used in RTCPSRPacket::SetAckTimeout(),RTPBandwidthTracker::UpdateAckTimeout() */
        UInt32  fAckTimeout; //Client Ack客户端应答超时时间(等待多长时间,当前Ack会到来?),参见上面的常数kMinAckTimeout,kMaxAckTimeout
        
        // Stats
		/* 在UpdateStats()中设置这些统计量 */
        SInt32  fMaxCongestionWindowSize;   //当前cwnd（fClientWindow）的最大值
        SInt32  fMinCongestionWindowSize;   //当前cwnd（fClientWindow）的最小值
		SInt64  fTotalCongestionWindowSize; //总cwnd大小，为统计生命周期内cwnd平均大小所用
        SInt32  fMaxRTO;   //当前RTO（fUnadjustedRTO）的最大值
        SInt32  fMinRTO;   //当前RTO（fUnadjustedRTO）的最小值  
        SInt64  fTotalRTO; //RTO总量，为统计生命周期内RTO的平均大小
        SInt32  fNumStatsSamples; //检测cwnd的次数，为统计生命周期内cwnd平均大小所用
        
		/* RTO范围(单位是ms),参见RTPBandwidthTracker::AddToRTTEstimate() */
        enum
        {
            kMinRetransmitIntervalMSecs = 600,
            kMaxRetransmitIntervalMSecs = 24000
        };
};

#endif // __RTP_BANDWIDTH_TRACKER_H__
