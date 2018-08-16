DSS 代码分析【Reliable UDP之超时时间计算】
2017年11月22日 10:56:57
阅读数：72
Darwin流媒体服务中使用Karn算法来评估RTP传输超时时间，
具体实现在RTPBandwidthTracker::AddToRTTEstimate函数中，具体代码如下：

```$xslt
void RTPBandwidthTracker::AddToRTTEstimate(SInt32 rttSampleMSecs)
{
	//  qtss_printf("%d ", rttSampleMSecs);
	//  static int count = 0;
	//  if ((count++ % 10) == 0) qtss_printf("\n");
 
		// this assert hits
	Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0
 
	if (fRunningAverageMSecs == 0)
		fRunningAverageMSecs = rttSampleMSecs * 8;  // init avg to cur sample, scaled by 2**3 
 
	SInt32 delta = rttSampleMSecs - fRunningAverageMSecs / 8; // scale average back to get cur delta from sample
 
	// add 1/8 the delta back to the smooth running average
	fRunningAverageMSecs = fRunningAverageMSecs + delta; // same as: rt avg = rt avg + delta / 8, but scaled
 
	if (delta < 0)
		delta = -1 * delta;   // absolute value 
 
	/*
		fRunningMeanDevationMSecs is kept scaled by 4
		so this is the same as
		fRunningMeanDevationMSecs = fRunningMeanDevationMSecs + ( |delta| - fRunningMeanDevationMSecs ) /4;
	*/
 
	fRunningMeanDevationMSecs += delta - fRunningMeanDevationMSecs / 4;
 
 
	fUnadjustedRTO = fCurRetransmitTimeout = fRunningAverageMSecs / 8 + fRunningMeanDevationMSecs;
 
	// rto should not be too low..
	if (fCurRetransmitTimeout < kMinRetransmitIntervalMSecs)
		fCurRetransmitTimeout = kMinRetransmitIntervalMSecs;
 
	// or too high...
	if (fCurRetransmitTimeout > kMaxRetransmitIntervalMSecs)
		fCurRetransmitTimeout = kMaxRetransmitIntervalMSecs;
	//  qtss_printf("CurTimeout == %d\n", fCurRetransmitTimeout);
}
```


RTP传输超时时间是动态计算出来的具体如下：
1.收到Ack确认包时调用RTPPacketResender::AckPacket，该函数继续调用下面处理来调整超时时间，
用真实的RTT时间来计算超时时间。

if (theEntry->fNumResends == 0)//表示该包是第一次发送
{
	// add RTT sample...        
	// only use rtt from packets acked after their initial send, do not use
	// estimates gatherered from re-trasnmitted packets.
	//fRTTEstimator.AddToEstimate( theEntry->fPacketRTTDuration.DurationInMilliseconds() );
	fBandwidthTracker->AddToRTTEstimate((SInt32)(inCurTimeInMsec - theEntry->fAddedTime));
	//用收到确认包的时间减去该包的发送时间，就得到RTT(Round trip time)时间
}

2.包超时重传时调用 RTPPacketResender::ResendDueEntries，该函数继续调用下面处理来调整超时时间，
用上一次的超时时间*1.5来计算超时时间。
// ok -- lets try this.. add 1.5x of the INITIAL duration since the last send to the
 rto estimator
// since we won't get an ack on this packet
// this should keep us from exponentially increasing due o a one time increase
// in the actuall rtt, only AddToEstimate on the first resend ( assume that it's a dupe )
// if it's not a dupe, but rather an actual loss, the subseqnuent actuals wil bring down 
the average quickly
 
if (theEntry->fNumResends == 1)//判断是超时后第一次传输
    fBandwidthTracker->AddToRTTEstimate((SInt32)((theEntry->fOrigRetransTimeout * 3) / 2));
    //如果是超时后第一次传输,就使用上一次超时时间*1.5作为新的RTT时间



DSS 代码分析【Reliable UDP之拥塞控制】
2017年11月22日 11:57:39
阅读数：70
Darwin流媒体服务器中拥塞控制是在RTPBandwidthTracker类中定义并实现的。

1.在服务器启调用 RTPSession::Play给客户端播送音视频数据时，会调用RTPBandwidthTracker::SetWindowSize给拥塞窗口fCongestionWindow，fSlowStartThreshold设定初值。

void RTPBandwidthTracker::SetWindowSize(SInt32 clientWindowSize)
{
	//
	// Currently we only allow this info to be set once
	if (fClientWindow > 0)
		return;
 
	// call SetWindowSize once the clients buffer size is known
	// since this occurs before the stream starts to send
 
	fClientWindow = clientWindowSize;
	fLastCongestionAdjust = 0;
 
#if RTP_PACKET_RESENDER_DEBUGGING   
	//€ test to see what happens w/o slow start at beginning
	//if ( initSlowStart )
	//  qtss_printf( "ack list initializing with slow start.\n" );
	//else
	//  qtss_printf( "ack list initializing at full speed.\n" );
#endif
 
	if (fUseSlowStart)
	{
		fSlowStartThreshold = clientWindowSize * 3 / 4;
 
		//
		// This is a change to the standard TCP slow start algorithm. What
		// we found was that on high bitrate high latency networks (a DSL connection, perhaps),
		// it took just too long for the ACKs to come in and for the window size to
		// grow enough. So we cheat a bit.
		fCongestionWindow = clientWindowSize / 2;
		//fCongestionWindow = kMaximumSegmentSize;
	}
	else
	{
		fSlowStartThreshold = clientWindowSize;
		fCongestionWindow = clientWindowSize;
	}
 
	if (fSlowStartThreshold < kMaximumSegmentSize)
		fSlowStartThreshold = kMaximumSegmentSize;
}

2.RTPStream::ReliableRTPWrite在发送数据前会调用fResender.IsFlowControlled判断窗口是否已满，如果已满则延迟发包，代码如下
if (fResender.IsFlowControlled())
{
		err = QTSS_WouldBlock;
}else
{
 
		//
		// Assign a lifetime to the packet using the current delay of the packet and
		// the time until this packet becomes stale.
		fBytesSentThisInterval += inLen;
		fResender.AddPacket(inBuffer, inLen, (SInt32)(fDropAllPacketsForThisStreamDelay - curPacketDelay));
 
		(void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, inBuffer, inLen);
}

//fBytesInList是已经发送的数据大小，fCongestionWindow代表当前拥塞窗口的大小
const Bool16 IsFlowControlled() { return ((SInt32)fBytesInList >= fCongestionWindow); }

3.已送数据fBytesInList大小更新：
   3.1 RTPStream::ReliableRTPWrite调用RTPPacketResender::AddPacket发包时会将发送包的大小累加到fBytesInList上。

          

void RTPPacketResender::AddPacket(void * inRTPPacket, UInt32 packetSize, SInt32 ageLimit)
{
......
      fBandwidthTracker->FillWindow(packetSize);
......
}

void FillWindow(UInt32 inNumBytes)
{
    fBytesInList += inNumBytes; fIsRetransmitting = false;
}
   3.2 RTPPacketResender::AckPacket收到确认包时会调用fBandwidthTracker->EmptyWindow(theEntry->fPacketSize)，fBytesInList可用的大小变大。
         RTPPacketResender::ResendDueEntries()删除过期的包也会调用fBandwidthTracker->EmptyWindow；RTPPacketResender::RemovePacket在移除不被使用的包时，也

         会调用。

void RTPBandwidthTracker::EmptyWindow(UInt32 bytesIncreased, Bool16 updateBytesInList)
{
	if (bytesIncreased == 0)
		return;
 
	Assert(fClientWindow > 0 && fCongestionWindow > 0);
 
	if (fBytesInList < bytesIncreased)
		bytesIncreased = fBytesInList;
 
	if (updateBytesInList)
		fBytesInList -= bytesIncreased;
 
	// this assert hits
	Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0
 
	// update the congestion window by the number of bytes just acknowledged.
 
	if (fCongestionWindow >= fSlowStartThreshold)
	{
		// when we hit the slow start threshold, only increase the 
		// window for each window full of acks.
		fCongestionWindow += bytesIncreased * bytesIncreased / fCongestionWindow;
	}
	else
		//
		// This is a change to the standard TCP slow start algorithm. What
		// we found was that on high bitrate high latency networks (a DSL connection, perhaps),
		// it took just too long for the ACKs to come in and for the window size to
		// grow enough. So we cheat a bit.
		fCongestionWindow += bytesIncreased;
 
 
	if (fCongestionWindow > fClientWindow)
		fCongestionWindow = fClientWindow;
 
	//  qtss_printf("Window = %d, %d left\n", fCongestionWindow, fCongestionWindow-fBytesInList);
}
          
拥塞避免：为了防止fCongestionWindow增加过快而导致网络拥塞，所以需要设置一个慢开始门限fSlowStartThreshold状态变量。

当fCongestionWindow >= fSlowStartThreshold,使用拥塞控制算法，停用慢启动算法，fCongestionWindow缓慢的增长，比慢启动要慢的多；

当fCongestionWindow < fSlowStartThreshold,使用慢启动算法。



当需要重传时(调用RTPPacketResender::ResendDueEntries)，说明网络出现拥塞，就要把慢启动开始门限(fSlowStartThreshold)设置为设置为发送窗口的3/4，fCongestionWindow(拥塞窗口)设置为原来的1/2，然后在使用慢启动算法，这样做的目的能迅速的减少主机向网络中传输数据，使发生拥塞的路由器能够把队列中堆积的分组处理完毕，代码如下：

RTPPacketResender::ResendDueEntries--->RTPBandwidthTracker::AdjustWindowForRetransmit


void RTPBandwidthTracker::AdjustWindowForRetransmit()
{
	// this assert hits
	Assert(fBytesInList < ((UInt32)fClientWindow + 2000)); //mainly just to catch fBytesInList wrapping below 0
 
	// slow start says that we should reduce the new ss threshold to 1/2
	// of where started getting errors ( the current congestion window size )
 
	// so, we get a burst of re-tx becuase our RTO was mis-estimated
	// it doesn't seem like we should lower the threshold for each one.
	// it seems like we should just lower it when we first enter
	// the re-transmit "state" 
//  if ( !fIsRetransmitting )
//      fSlowStartThreshold = fCongestionWindow/2;
 
	// make sure that it is at least 1 packet
	if (fSlowStartThreshold < kMaximumSegmentSize)
		fSlowStartThreshold = kMaximumSegmentSize;
 
	// start the full window segemnt counter over again.
	fSlowStartByteCount = 0;
 
	// tcp resets to one (max segment size) mss, but i'm experimenting a bit
	// with not being so brutal.
 
	//curAckList->fCongestionWindow = kMaximumSegmentSize;
 
//  fCongestionWindow = kMaximumSegmentSize;
//  fCongestionWindow = fCongestionWindow / 2;  // half the congestion window size
	SInt64 theTime = OS::Milliseconds();
	if (theTime - fLastCongestionAdjust > 250) //调整周期为250
	{
		fSlowStartThreshold = fCongestionWindow * 3 / 4;   //慢开始门限设为拥塞窗口的3/4
		fCongestionWindow = fCongestionWindow / 2;         //拥塞窗口设为原来的1/2
		fLastCongestionAdjust = theTime;
	}
 
	/*
		if ( fSlowStartThreshold < fCongestionWindow )
			fCongestionWindow = fSlowStartThreshold/2;
		else
			fCongestionWindow = fCongestionWindow /2;
	*/
 
	if (fCongestionWindow < kMaximumSegmentSize)
		fCongestionWindow = kMaximumSegmentSize;
 
	// qtss_printf("Congestion window now %d\n", fCongestionWindow);
	fIsRetransmitting = true;
}

