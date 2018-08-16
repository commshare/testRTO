


## DSS 代码分析【Reliable UDP之数据重传】
2017年11月21日 21:04:51
阅读数：123
Darwin 流媒体服务器支持使用Reliable UDP的方式发送RTP，Reliable UDP使用数据重传和拥挤控制算法，
与TCP协议采取的算法类似。

在使用Reliable UDP发送RTP数据时调用RTPStream::ReliableRTPWrite，具体实现如下：
```cpp
QTSS_Error RTPStream::ReliableRTPWrite(void* inBuffer, UInt32 inLen, const SInt64& curPacketDelay)
{
	// Send retransmits for all streams on this session
	RTPStream** retransStream = NULL;
	UInt32 retransStreamLen = 0;
	//
	// Send retransmits if we need to
	for (int streamIter = 0; fSession->GetValuePtr(qtssCliSesStreamObjects, streamIter, (void**)&retransStream, &retransStreamLen) == QTSS_NoErr; streamIter++)
	{
		if (retransStream != NULL && *retransStream != NULL)
			(*retransStream)->fResender.ResendDueEntries();
	}
......
	else
	{
		//
		// Assign a lifetime to the packet using the current delay of the packet and
		// the time until this packet becomes stale.
		fBytesSentThisInterval += inLen;
		fResender.AddPacket(inBuffer, inLen, (SInt32)(fDropAllPacketsForThisStreamDelay - curPacketDelay));
		(void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, inBuffer, inLen);
	}
	return err;
}
```

1.在调用fSockets->GetSocketA()->SendTo发送数据前，
fResender.AddPacket添加该包到重传数组fPacketArray里，并记录添加的时间以及包的过期时间，代码如下：

void RTPPacketResender::AddPacket(void * inRTPPacket, UInt32 packetSize, SInt32 ageLimit)
{
	//OSMutexLocker packetQLocker(&fPacketQMutex);
	// the caller needs to adjust the overall age limit by reducing it
	// by the current packet lateness.
	// we compute a re-transmit timeout based on the Karns RTT esmitate
	UInt16* theSeqNumP = (UInt16*)inRTPPacket;
	UInt16 theSeqNum = ntohs(theSeqNumP[1]);
	if (ageLimit > 0)
	{
		RTPResenderEntry* theEntry = this->GetEmptyEntry(theSeqNum, packetSize);
......
		// Reset all the information in the RTPResenderEntry
		::memcpy(theEntry->fPacketData, inRTPPacket, packetSize);
		theEntry->fPacketSize = packetSize;
		theEntry->fAddedTime = OS::Milliseconds();//记录添加的时间
		theEntry->fOrigRetransTimeout = fBandwidthTracker->CurRetransmitTimeout();
		theEntry->fExpireTime = theEntry->fAddedTime + ageLimit;//记录过期时间
		theEntry->fNumResends = 0;
		theEntry->fSeqNum = theSeqNum;
 
		//
		// Track the number of wasted bytes we have
		atomic_add(&sNumWastedBytes, kMaxDataBufferSize - packetSize);
 
		//PLDoubleLinkedListNode<RTPResenderEntry> * listNode = NEW PLDoubleLinkedListNode<RTPResenderEntry>( new RTPResenderEntry(inRTPPacket, packetSize, ageLimit, fRTTEstimator.CurRetransmitTimeout() ) );
		//fAckList.AddNodeToTail(listNode);
		fBandwidthTracker->FillWindow(packetSize);
	}
	else
	{
		fNumExpired++;
	}
	fNumSent++;
}
2.当采用Reliable UDP模式时客户端会定期发送APP类型的RTCP包(ack应答包)，
应答包的有效负荷由RTP序列号以及紧跟其后的可变长度的位掩码组成。
序列号标识客户端正在应答的首个RTP数据包，此外的每个被应答的RTP数据包都由位掩码中设置的一个位来表示。
位掩码是相对于指定序列号的偏移量，掩码中的第一个字节高位表示比指定序列号大１的数据包，
第二个位表示比指定数据包大２的数据包，以此类推。位掩码必须以多个four octets的方式发送。

Ack Packet format
0                                      1                                      2                                     3
0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |   PT=APP=204  |             length                                                 |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                                                                                 |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)  = 'qtak'                                                                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                                                                                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Reserved             |          Seq num                                                                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Mask...                                                                                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
服务器端有个RTCPTask的任务会在TaskThread的线程里一直执行，接收客户端发送过来的RTCP包，
并调用ProcessIncomingRTCPPacket将RTCP包交给相应的RTPStream类处理
```
                               SInt64 RTCPTask::Run()
                                   {
                                        ......
                                        while (true) //get all the outstanding packets for this socket
					{
						thePacket.Len = 0;
						theSocket->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket.Ptr,
							kMaxRTCPPacketSize, &thePacket.Len);
						if (thePacket.Len == 0)
						{
							theSocket->RequestEvent(EV_RE);
							break;//no more packets on this socket!
						}
 
						//if this socket has a demuxer, find the target RTPStream
						if (theDemuxer != NULL)
						{
							RTPStream* theStream = (RTPStream*)theDemuxer->GetTask(theRemoteAddr, theRemotePort);
							if (theStream != NULL)
								theStream->ProcessIncomingRTCPPacket(&thePacket);
						}
					}
                                        ......
                                    }
RTPStream::ProcessIncomingRTCPPacket会对接收的RTCP进行解析，当解析的是RTCPAckPacket::kAckPacketName类型的包时，会调用ProcessAckPacket，具体代码如下：
void RTPStream::ProcessIncomingRTCPPacket(StrPtrLen* inPacket)
{
......
	while (currentPtr.Len > 0)
	{
		RTCPPacket rtcpPacket;
		if (!rtcpPacket.ParsePacket((UInt8*)currentPtr.Ptr, currentPtr.Len))
		{
			fSession->GetSessionMutex()->Unlock();
			DEBUG_RTCP_PRINTF(("malformed rtcp packet\n"));
			return;//abort if we discover a malformed RTCP packet
		}
......
		switch (rtcpPacket.GetPacketType())
		{
		case RTCPPacket::kReceiverPacketType:
			.
 
		case RTCPPacket::kAPPPacketType:
			{
				DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket kAPPPacketType\n"));
				Bool16 packetOK = false;
				RTCPAPPPacket theAPPPacket;
				if (!theAPPPacket.ParseAPPPacket((UInt8*)currentPtr.Ptr, currentPtr.Len))
				{
					fSession->GetSessionMutex()->Unlock();
					return;//abort if we discover a malformed receiver report
				}
				UInt32 itemName = theAPPPacket.GetAppPacketName();
				itemName = theAPPPacket.GetAppPacketName();
				switch (itemName)
				{
 
				case RTCPAckPacket::kAckPacketName:
				case RTCPAckPacket::kAckPacketAlternateName:
					{
						packetOK = this->ProcessAckPacket(rtcpPacket, curTime);
					}
					break;
......
		}
	}
}
```
RTPStream::ProcessAckPacket会调用fResender.AckPacket对收到的包进行确认（从发送数组中移除）
Bool16 RTPStream::ProcessAckPacket(RTCPPacket &rtcpPacket, SInt64 &curTime)
{
	RTCPAckPacket theAckPacket;
	UInt8* packetBuffer = rtcpPacket.GetPacketBuffer();
	UInt32 packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;
 
	if (!theAckPacket.ParseAPPData(packetBuffer, packetLen))
		return false;
 
	if (NULL != fTracker && false == fTracker->ReadyForAckProcessing()) // this stream must be ready to receive acks.  Between RTSP setup and sending of first packet on stream we must protect against a bad ack.
		return false;//abort if we receive an ack when we haven't sent anything.
 
 
	this->PrintPacketPrefEnabled((char*)packetBuffer, packetLen, RTPStream::rtcpACK);
	// Only check for ack packets if we are using Reliable UDP
	if (fTransportType == qtssRTPTransportTypeReliableUDP)
	{
		UInt16 theSeqNum = theAckPacket.GetAckSeqNum();
		fResender.AckPacket(theSeqNum, curTime);
		//qtss_printf("Got ack: %d\n",theSeqNum);
 
		for (UInt16 maskCount = 0; maskCount < theAckPacket.GetAckMaskSizeInBits(); maskCount++)
		{
			if (theAckPacket.IsNthBitEnabled(maskCount))
			{
				fResender.AckPacket(theSeqNum + maskCount + 1, curTime);
				//qtss_printf("Got ack in mask: %d\n",theSeqNum + maskCount + 1);
			}
		}
	}
	return true;
}
RTPPacketResender::AckPacket调用RTPPacketResender::RemovePacket将确认的包从fPacketArray中移除。

3.每次调用RTPStream::ReliableRTPWrite发送数据时，
首先都会调用(*retransStream)->fResender.ResendDueEntries()去检查有没有包在超时时间内没有收到确认，
加入有就重发， ResendDueEntries()具体代码如下：

void RTPPacketResender::ResendDueEntries()
{
	if (fPacketsInList <= 0)
		return;
 
	SInt32 numResends = 0;
	RTPResenderEntry* theEntry = NULL;
	SInt64 curTime = OS::Milliseconds();
	for (SInt32 packetIndex = fPacketsInList - 1; packetIndex >= 0; packetIndex--) // walk backwards because remove packet moves array members forward
	{
		theEntry = &fPacketArray[packetIndex];
 
		if (theEntry->fPacketSize == 0)
			continue;
 
		if ((curTime - theEntry->fAddedTime) > fBandwidthTracker->CurRetransmitTimeout())
		{
			// Change:  Only expire packets after they were due to be resent. This gives the client
			// a chance to ack them and improves congestion avoidance and RTT calculation
			if (curTime > theEntry->fExpireTime)
			{
				//
				// This packet is expired
				fNumExpired++;
				//qtss_printf("Packet expired: %d\n", ((UInt16*)thePacket)[1]);
				fBandwidthTracker->EmptyWindow(theEntry->fPacketSize);
				this->RemovePacket(packetIndex);
				//              qtss_printf("Expired packet %d\n", theEntry->fSeqNum);
				continue;
			}
 
			// Resend this packet
			fSocket->SendTo(fDestAddr, fDestPort, theEntry->fPacketData, theEntry->fPacketSize);
			//qtss_printf("Packet resent: %d\n", ((UInt16*)theEntry->fPacketData)[1]);
 
			theEntry->fNumResends++;    
 
			fNumResends++;
 
			numResends++;
			//qtss_printf("resend loop numResends=%" _S32BITARG_ " packet theEntry->fNumResends=%" _S32BITARG_ " stream fNumResends=\n",numResends,theEntry->fNumResends++, fNumResends);
 
			// ok -- lets try this.. add 1.5x of the INITIAL duration since the last send to the rto estimator
			// since we won't get an ack on this packet
			// this should keep us from exponentially increasing due o a one time increase
			// in the actuall rtt, only AddToEstimate on the first resend ( assume that it's a dupe )
			// if it's not a dupe, but rather an actual loss, the subseqnuent actuals wil bring down the average quickly
 
			if (theEntry->fNumResends == 1)
				fBandwidthTracker->AddToRTTEstimate((SInt32)((theEntry->fOrigRetransTimeout * 3) / 2));
 
			//          qtss_printf("Retransmitted packet %d\n", theEntry->fSeqNum);
			theEntry->fAddedTime = curTime;
			fBandwidthTracker->AdjustWindowForRetransmit();
			continue;
		}
 
	}
}
从上面代码可以看出，首先判断是否超时，接着还需要判断包是否过期了，
如果过期了直接调用RemovePacket删除，如果没有就重发。

超时判断代码：if((curTime - theEntry->fAddedTime) > fBandwidthTracker->CurRetransmitTimeout())

包过期判断代码：if (curTime > theEntry->fExpireTime)

还有一点关于超时时间RTT的计算，会在后面的文章中说明。


## DSS 代码分析【Reliable UDP之超时时间计算】
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



## DSS 代码分析【Reliable UDP之拥塞控制】
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


## DSS 代码分析【BufferWindow实现】
2017年11月22日 15:37:43
阅读数：77
Darwin 流媒体服务器可以根据客户端BufferWindow的大小，动态控制发包的速度；客户端需要设定BufferWindow，并通过RTCP包定期将当前可用BufferWindow大小告诉服务器。

服务器端实现流程如下：

1.服务器端有个RTCPTask的任务会在TaskThread的线程里一直执行，接收客户端发送过来的RTCP包，并调用ProcessIncomingRTCPPacket将RTCP包交给相应的RTPStream类处理

[plain] view plain copy
                          SInt64 RTCPTask::Run()  
                              {  
                                   ......  
                                   while (true) //get all the outstanding packets for this socket  
{  
    thePacket.Len = 0;  
    theSocket->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket.Ptr,  
        kMaxRTCPPacketSize, &thePacket.Len);  
    if (thePacket.Len == 0)  
    {  
        theSocket->RequestEvent(EV_RE);  
        break;//no more packets on this socket!  
    }  
  
    //if this socket has a demuxer, find the target RTPStream  
    if (theDemuxer != NULL)  
    {  
        RTPStream* theStream = (RTPStream*)theDemuxer->GetTask(theRemoteAddr, theRemotePort);  
        if (theStream != NULL)  
            theStream->ProcessIncomingRTCPPacket(&thePacket);  
    }  
}  
                                   ......  
                               }  
RTPStream::ProcessIncomingRTCPPacket会对接收的RTCP进行解析，当解析的是RTCPAckPacket::kAckPacketName类型的包时，会调用ProcessCompressedQTSSPacket，具体代码如下：
[plain] view plain copy
void RTPStream::ProcessIncomingRTCPPacket(StrPtrLen* inPacket)  
{  
......  
    while (currentPtr.Len > 0)  
    {  
        RTCPPacket rtcpPacket;  
        if (!rtcpPacket.ParsePacket((UInt8*)currentPtr.Ptr, currentPtr.Len))  
        {  
            fSession->GetSessionMutex()->Unlock();  
            DEBUG_RTCP_PRINTF(("malformed rtcp packet\n"));  
            return;//abort if we discover a malformed RTCP packet  
        }  
......  
        switch (rtcpPacket.GetPacketType())  
        {  
        case RTCPPacket::kReceiverPacketType:  
            .  
  
        case RTCPPacket::kAPPPacketType:  
            {  
                DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket kAPPPacketType\n"));  
                Bool16 packetOK = false;  
                RTCPAPPPacket theAPPPacket;  
                if (!theAPPPacket.ParseAPPPacket((UInt8*)currentPtr.Ptr, currentPtr.Len))  
                {  
                    fSession->GetSessionMutex()->Unlock();  
                    return;//abort if we discover a malformed receiver report  
                }  
                UInt32 itemName = theAPPPacket.GetAppPacketName();  
                itemName = theAPPPacket.GetAppPacketName();  
                switch (itemName)  
                {  
  
                ...... 
                case RTCPCompressedQTSSPacket::kCompressedQTSSPacketName:  
                    {  
                        packetOK = this->ProcessCompressedQTSSPacket(rtcpPacket, curTime, currentPtr);  
                    }  
                    break;  
......  
        }  
    }  
}  
RTPStream::ProcessCompressedQTSSPacket会解析该RTCP包并调用SetWindowSize，代码如下：
/*
QTSS APP: QTSS Application-defined RTCP packet


     0                                      1                                     2                                     3
     0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |  PT=APP=204  |             length                                                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                                                                                 |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)                                                                                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <---- app data start
   |                           SSRC/CSRC                                                                                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        version                |             length                                                                    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      field name='ob' other    |   version=0   |   length=4                                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Over-buffer window size in bytes                                                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


fieldnames = rr, lt, ls, dl, :), :|, :(, ey, pr, pd, pl, bl, fr, xr, d#, ob


 */


Bool16 RTPStream::ProcessCompressedQTSSPacket(RTCPPacket &rtcpPacket, SInt64 &curTime, StrPtrLen ¤tPtr)
{
	RTCPCompressedQTSSPacket compressedQTSSPacket;
	UInt8* packetBuffer = rtcpPacket.GetPacketBuffer();
	UInt32 packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;
 
	this->PrintPacketPrefEnabled((char*)packetBuffer, packetLen, RTPStream::rtcpAPP);
 
	if (!compressedQTSSPacket.ParseAPPData((UInt8*)currentPtr.Ptr, currentPtr.Len))
		return false;//abort if we discover a malformed app packet
 
 
	fReceiverBitRate = compressedQTSSPacket.GetReceiverBitRate();
	fAvgLateMsec = compressedQTSSPacket.GetAverageLateMilliseconds();
 
	fPercentPacketsLost = compressedQTSSPacket.GetPercentPacketsLost();
	fAvgBufDelayMsec = compressedQTSSPacket.GetAverageBufferDelayMilliseconds();
	fIsGettingBetter = (UInt16)compressedQTSSPacket.GetIsGettingBetter();
	fIsGettingWorse = (UInt16)compressedQTSSPacket.GetIsGettingWorse();
	fNumEyes = compressedQTSSPacket.GetNumEyes();
	fNumEyesActive = compressedQTSSPacket.GetNumEyesActive();
	fNumEyesPaused = compressedQTSSPacket.GetNumEyesPaused();
	fTotalPacketsRecv = compressedQTSSPacket.GetTotalPacketReceived();
	fTotalPacketsDropped = compressedQTSSPacket.GetTotalPacketsDropped();
	fTotalPacketsLost = compressedQTSSPacket.GetTotalPacketsLost();
	fClientBufferFill = compressedQTSSPacket.GetClientBufferFill();
	fFrameRate = compressedQTSSPacket.GetFrameRate();
	fExpectedFrameRate = compressedQTSSPacket.GetExpectedFrameRate();
	fAudioDryCount = compressedQTSSPacket.GetAudioDryCount();
 
 
	// Update our overbuffer window size to match what the client is telling us
	if (fTransportType != qtssRTPTransportTypeUDP)
	{
		//  qtss_printf("Setting over buffer to %d\n", compressedQTSSPacket.GetOverbufferWindowSize());
		fSession->GetOverbufferWindow()->SetWindowSize(compressedQTSSPacket.GetOverbufferWindowSize());
	}
 
#ifdef DEBUG_RTCP_PACKETS
	compressedQTSSPacket.Dump();
#endif
 
	return true;
 
}
RTPOverbufferWindow::SetWindowSize设置客户端当前可用的BufferWindow的大小，并将计算上个发送周期内发送的字节数设为0，用来统计当前周期发送的字节数。

void RTPOverbufferWindow::SetWindowSize(UInt32 inWindowSizeInBytes)
{
	fWindowSize = inWindowSizeInBytes;
	fBytesSentSinceLastReport = 0;
}


服务器在发包时调用RTPStream::Write，该函数执行时会调用RTPOverbufferWindow::CheckTransmitTime检查客户端剩余的BufferWindow是否够用，该函数的返回值为延迟发送的时间。

	// if the client is running low on memory, wait a while for it to be freed up
	// there's nothing magic bout these numbers, we're just trying to be conservative
	if ((fWindowSize != -1) && (inPacketSize * 5 > fWindowSize - fBytesSentSinceLastReport))
	{
		return inCurrentTime + (fSendInterval * 5);  // client reports don't come that often
	}

 fWindowSize - fBytesSentSinceLastReport为当前周期内客户端BufferWindow还有多少可用的空间，当剩余的空间小于当前包大小的5倍时，返回五个发送间隔的延迟发送时间，这样就避免了由于服务端发包过快，客户端来不及处理导致的丢包。
