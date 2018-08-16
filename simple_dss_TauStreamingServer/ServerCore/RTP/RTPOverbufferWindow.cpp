
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPOverbufferWindow.cpp
Description: Class that tracks packets that are part of the "overbuffer". That is,
			 packets that are being sent ahead of time. This class can be used
			 to make sure the server isn't overflowing the client's overbuffer size.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-17

****************************************************************************/ 



#include "RTPOverbufferWindow.h"
#include "OSMemory.h"
#include "MyAssert.h"



// 入参参见RTPSessionInterface::RTPSessionInterface(),都是服务器的预设值
RTPOverbufferWindow::RTPOverbufferWindow(UInt32 inSendInterval, UInt32 inInitialWindowSize, UInt32 inMaxSendAheadTimeInSec,Float32 inOverbufferRate)									
:   fWindowSize(inInitialWindowSize),/* kUInt32_Max,参见RTPStream::Setup()/ProcessIncomingRTCPPacket() */
    fBytesSentSinceLastReport(0),
    fSendInterval(inSendInterval),
    fBytesDuringLastSecond(0),
    fLastSecondStart(-1),
    fBytesDuringPreviousSecond(0),
    fPreviousSecondStart(-1),
    fBytesDuringBucket(0),
    fBucketBegin(0),/* 注意这个初始值!! */
    fBucketTimeAhead(0),/* 发包延时 */
    fPreviousBucketTimeAhead(0),
    fMaxSendAheadTime(inMaxSendAheadTimeInSec * 1000), /* 25s */
    fWriteBurstBeginning(false),/* 还没有开始大量写包 */
	fOverbufferingEnabled(true),/* 可以使用Overbuffering超前发送机制 */
	fOverbufferRate(inOverbufferRate), /* 默认设置2.0 */
	fSendAheadDurationInMsec(1000),/* 注意提前发送的持续时间是1s */
	fOverbufferWindowBegin(-1) /* 注意这个初始值!! */
{
	/* 假如送包间隔0,就不用超前发送机制,但将送包间隔减慢为200ms */
    if (fSendInterval == 0)
    {
		/* 注意关闭超前发送机制 */
        fOverbufferingEnabled = false;
        fSendInterval = 200;
    }
	
	/* 确保overbufferRate至少是1.0,默认为2.0 */
	if(fOverbufferRate < 1.0)
		fOverbufferRate = 1.0;

}

/****************************** 本类中最核心的一个函数 ***********************************************************************/
/* 这个函数的作用是:针对当前包设定的发送时间戳,检查当前数据包是否可以提前发送,提前多长时间发送?并调节提前发包的节奏快慢。第一个参数表示数据包的设定发送时间，第二个参数表示发包的当前时间，第三个参数表示发送数据包大小。
如果函数返回－1，则数据会立刻被发送；如果返回值大于当前时间则数据会等待下一次被发送。 */
SInt64 RTPOverbufferWindow::CheckTransmitTime(const SInt64& inTransmitTime, const SInt64& inCurrentTime, SInt32 inPacketSize)
{
    // if this is the beginning of a bucket interval, roll over figures from last time.滚动上次的数字
    // accumulate statistics over the period of a second 累计一秒内的统计数字

	/* 假如当前时间距离Bucket开始时间戳,超过一个bucket interval,更新相关量:当前Bucket interval起始时间,超前时间,发送字节数 */
    if (inCurrentTime - fBucketBegin > fSendInterval)
    {
        fPreviousBucketBegin = fBucketBegin;
        fBucketBegin = inCurrentTime;
		/* 记录上一个Bucket interval开始的绝对时间 */
		if (fPreviousBucketBegin == 0)
			fPreviousBucketBegin = fBucketBegin - fSendInterval;
        fBytesDuringBucket = 0;

		/* 每隔1000ms,更新上一秒内发送的字节数和起始时间相关量 */
        if (inCurrentTime - fLastSecondStart > 1000)
        {
			/* 记录上一秒内发送的字节数 */
            fBytesDuringPreviousSecond = fBytesDuringLastSecond;
			/* 更新上一秒发送的字节数 */
            fBytesDuringLastSecond = 0;
			/* 记录上一秒起始的时间 */
            fPreviousSecondStart = fLastSecondStart;
            /* 更新上一秒起始的时间 */
            fLastSecondStart = inCurrentTime;
        }
        
		/* 记录当前Bucket interval提前的时间,它就是该包的发送延迟 */
        fPreviousBucketTimeAhead = fBucketTimeAhead;
    }
    
	/* 更新Overbuffer窗口的起始时间 */
	if (fOverbufferWindowBegin == -1)
		fOverbufferWindowBegin = inCurrentTime;
	
	/* 假如该RTP包设定的发送时间在当前发送时间段内,或者现在可以提前发送,并且该RTP包处于提前发送和正常发送的时间段中,就立即发送出该包 */
	if ((inTransmitTime <= inCurrentTime + fSendInterval) || 
		(fOverbufferingEnabled && (inTransmitTime <= inCurrentTime + fSendInterval + fSendAheadDurationInMsec)))
    {
        // If this happens, this packet needs to be sent regardless of overbuffering
        return -1;
    }
    
	//如果Overbuffering开关关闭,或者overbuffer窗大小为0,则直接返回该包设定的发送时间,不用超前发送
    if (!fOverbufferingEnabled || (fWindowSize == 0))
        return inTransmitTime;
    
	//如果overbuffer发送窗口剩余空间不足5*数据包大小,则延缓至5个发送间隔后发送该包
    // if the client is running low on memory, wait a while for it to be freed up
    // there's nothing magic bout these numbers, we're just trying to be conservative保守些
    if ((fWindowSize != -1) && (inPacketSize * 5 > fWindowSize - fBytesSentSinceLastReport))
    {
        return inCurrentTime + (fSendInterval * 5);  // client reports don't come that often
    }
    
	//fMaxSendAheadTime为25秒，这里表示发送超前时间不能大于25秒，否则需至少等待一个发送间隔后超前发送
    // if we're far enough ahead, then wait until it's time to send more packets
    if (inTransmitTime - inCurrentTime > fMaxSendAheadTime)
        return inTransmitTime - fMaxSendAheadTime + fSendInterval;
        
    // during the first second just send packets normally
//    if (fPreviousSecondStart == -1)
//        return inCurrentTime + fSendInterval;
    
    // now figure if we want to send this packet during this bucket.  We have two limitations.
    // First we scale up bitrate slowly, so we should only try and send a little more than we
    // sent recently (averaged over a second or two).  However, we always try and send at
    // least the current bitrate and never more than double.
//    SInt32 currentBitRate = fBytesDuringBucket * 1000 / (inCurrentTime - fPreviousBucketBegin);
//    SInt32 averageBitRate = (fBytesDuringPreviousSecond + fBytesDuringLastSecond) * 1000 / (inCurrentTime - fPreviousSecondStart);
//    SInt32 averageBitRate = fBytesDuringPreviousSecond * 1000 / (fLastSecondStart - fPreviousSecondStart);

	/* 当前Bucket interval内提前的发送时间(发包延时) */
	fBucketTimeAhead = inTransmitTime - inCurrentTime;
//	printf("Current br = %d, average br = %d (cta = %qd, pta = %qd)\n", currentBitRate, averageBitRate, currentTimeAhead, fPreviousBucketTimeAhead);
    
	//如果本次超前时间小于上次超前时间(发送延迟有减小趋势),则直接发送该包
    // always try and stay as far ahead as we were before
	if (fBucketTimeAhead < fPreviousBucketTimeAhead)
        return -1;
      
	//本次包的超时节奏较之上次不能过大,这里是为了保持数据发送的稳定性,如果太大(超过一个发送间隔),则延缓一个发送间隔再发。
	/* 不要以高于2倍Bitrate发送,而是仅比指定时间稍快一点发送 */
    // but don't send at more that double the bitrate (for any given time we should only get further
    // ahead by that amount of time)
	//printf("cta - pta = %qd, ct - pbb = %qd\n", fBucketTimeAhead - fPreviousBucketTimeAhead, SInt64((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0)));
	if (fBucketTimeAhead - fPreviousBucketTimeAhead > ((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0)))
	{ 
		fBucketTimeAhead = fPreviousBucketTimeAhead + SInt64((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0));
		return inCurrentTime + fSendInterval;		// this will get us to the next bucket
	}
        
	// don't send more than 10% over the average bitrate for the previous second
//    if (currentBitRate > averageBitRate * 11 / 10)
//        return inCurrentTime + fSendInterval;       // this will get us to the next bucket
    
    return -1;  // send this packet 立即送出该包
}

/* RTPSession::Play() */
/* 重置Overbuffering中的相关量 */
void RTPOverbufferWindow::ResetOverBufferWindow()
{
    fBytesDuringLastSecond = 0;
    fLastSecondStart = -1;
    fBytesDuringPreviousSecond = 0;
    fPreviousSecondStart = -1;
    fBytesDuringBucket = 0;
    fBucketBegin = 0;
    fBucketTimeAhead = 0;
    fPreviousBucketTimeAhead = 0;
	fOverbufferWindowBegin = -1;
}

/* used in RTPStream::Write() */
/* 对当前送出的RTP包,用当前包大小更新相关量 */
void RTPOverbufferWindow::AddPacketToWindow(SInt32 inPacketSize)
{
    fBytesDuringBucket += inPacketSize;
    fBytesDuringLastSecond += inPacketSize;
    fBytesSentSinceLastReport += inPacketSize;
}

/* used in RTPStream::Write() */
void RTPOverbufferWindow::EmptyOutWindow(const SInt64& inCurrentTime)
{
    // no longer needed
}

/* used  in RTPStream::Setup()/RTPStream::ProcessIncomingRTCPPacket() */
/* 用入参设置Overbuffering中Client窗口的大小 */
void RTPOverbufferWindow::SetWindowSize(UInt32 inWindowSizeInBytes)
{
    fWindowSize = inWindowSizeInBytes;
    fBytesSentSinceLastReport = 0;
}
