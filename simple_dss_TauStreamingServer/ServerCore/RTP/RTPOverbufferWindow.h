
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPOverbufferWindow.h
Description: Class that tracks packets that are part of the "overbuffer". That is,
			 packets that are being sent ahead of time. This class can be used
			 to make sure the server isn't overflowing the client's overbuffer size.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-17

****************************************************************************/ 


#ifndef __RTP_OVERBUFFER_WINDOW_H__
#define __RTP_OVERBUFFER_WINDOW_H__

#include "OSHeaders.h"



class RTPOverbufferWindow
{
    public:

		RTPOverbufferWindow(UInt32 inSendInterval, UInt32 inInitialWindowSize, UInt32 inMaxSendAheadTimeInSec, Float32 inOverbufferRate);						
        ~RTPOverbufferWindow() { }
        
		/* RTPSession::Play() */
		/* 重置Overbuffering中的相关量 */
		void ResetOverBufferWindow();
		
        // ACCESSORS
        
        UInt32  GetSendInterval() { return fSendInterval; }
        
        // This may be negative! 获取overbuffer窗的剩余空间大小
        SInt32  AvailableSpaceInWindow() { return fWindowSize - fBytesSentSinceLastReport; }
        
        // The window size may be changed at any time
		/* 发送窗口大小设置,有两次设置:在RTPStream::Setup()中设置为kUInt32_Max,随后在RTPStream::ProcessIncomingRTCPPacket()中
		设置为CompressedQTSSPacket的overbufferWindowSize大小 */
		void	SetWindowSize(UInt32 inWindowSizeInBytes);
      
        // Without changing the window size, you can enable / disable all overbuffering
        // using these calls. Defaults to enabled
        void    TurnOffOverbuffering()  { fOverbufferingEnabled = false; }
        void    TurnOnOverbuffering()   { fOverbufferingEnabled = true; }
		Bool16* OverbufferingEnabledPtr()  { return &fOverbufferingEnabled; }
        
        // If the overbuffer window is full, this returns a time in the future when
        // enough space will open up for this packet. Otherwise, returns -1.
        // The overbuffer window is full if the byte count is filled up, or if the
        // bitrate is above the max play rate.
		/****************** 本类最重要的函数 ************************/
		// 假如overbuffer窗无剩余空间,返回下次送包的绝对时间戳, 否则立即发送该包,返回-1
        SInt64 CheckTransmitTime(const SInt64& inTransmitTime, const SInt64& inCurrentTime, SInt32 inPacketSize);
        
        // Remembers that this packet has been sent
		/* used in RTPStream::Write() */
		/* 对当前送出的RTP包,用当前包大小更新相关量 */
        void AddPacketToWindow(SInt32 inPacketSize);
        
        // 清空上次发送时overbuffer window中遗留下的包,释放该窗的内存空间
        // As time passes, transmit times that were in the future become transmit
        // times that are in the past or present. Call this function to empty
        // those old packets out of the window, freeing up space in the window.
        void EmptyOutWindow(const SInt64& inCurrentTime);
        
        // MarkBeginningOfWriteBurst
        // Call this on the first write of a write burst for a client. This
        // allows the overbuffer window to track whether the bitrate of the movie
        // is above the play rate.
		/* used in RTPStream::Write() */
		/* 标记写迸发,将启动overbuffering Window监控比特率 */
        void MarkBeginningOfWriteBurst() { fWriteBurstBeginning = true; }       

    private:
        
		//overbuffer发送窗口大小
        SInt32 fWindowSize;
		//当前发送字节数
        SInt32 fBytesSentSinceLastReport;
		//数据包发送间隔，最小值默认50毫秒,参见streamingserver.xml
        SInt32 fSendInterval;

		//当前一秒内发送的字节数
        SInt32 fBytesDuringLastSecond;
		//当前一秒开始的时间，cf.fPreviousSecondStart
        SInt64 fLastSecondStart;

		//前一秒内发送的总字节数
        SInt32 fBytesDuringPreviousSecond;
		//前一秒的开始时间
        SInt64 fPreviousSecondStart;

		//一个间隔内发送的字节数,一个间隔为50毫秒
        SInt32 fBytesDuringBucket;
		//一个间隔的开始时间
        SInt64 fBucketBegin;
		//前一个间隔的开始时间
        SInt64 fPreviousBucketBegin;

		//当前Bucket interval的超前时间(送包延时) = 发送时间 － 当前时间
        SInt64 fBucketTimeAhead;
		//前Bucket interval的超前时间
        SInt64 fPreviousBucketTimeAhead;

		//最大超前送包时间，默认为25秒
        UInt32 fMaxSendAheadTime;

		//是否开始大量写?
        Bool16 fWriteBurstBeginning;

        //超前发送开关,设置参见RTPStream::SetOverBufferState()
        Bool16 fOverbufferingEnabled;

		//超前比率，控制当前超前时间不能过大，默认为2.0,一般大于1，参见streamingserver.xml
		Float32 fOverbufferRate;

        //超前发送持续时间,1000毫秒(见构造函数)?
		UInt32 fSendAheadDurationInMsec;
		
		//设置超前发送窗口开始计时的时间
		SInt64 fOverbufferWindowBegin;
};


#endif // __RTP_OVERBUFFER_TRACKER_H__


