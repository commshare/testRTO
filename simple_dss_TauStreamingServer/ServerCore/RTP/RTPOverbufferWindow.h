
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
		/* ����Overbuffering�е������ */
		void ResetOverBufferWindow();
		
        // ACCESSORS
        
        UInt32  GetSendInterval() { return fSendInterval; }
        
        // This may be negative! ��ȡoverbuffer����ʣ��ռ��С
        SInt32  AvailableSpaceInWindow() { return fWindowSize - fBytesSentSinceLastReport; }
        
        // The window size may be changed at any time
		/* ���ʹ��ڴ�С����,����������:��RTPStream::Setup()������ΪkUInt32_Max,�����RTPStream::ProcessIncomingRTCPPacket()��
		����ΪCompressedQTSSPacket��overbufferWindowSize��С */
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
		/****************** ��������Ҫ�ĺ��� ************************/
		// ����overbuffer����ʣ��ռ�,�����´��Ͱ��ľ���ʱ���, �����������͸ð�,����-1
        SInt64 CheckTransmitTime(const SInt64& inTransmitTime, const SInt64& inCurrentTime, SInt32 inPacketSize);
        
        // Remembers that this packet has been sent
		/* used in RTPStream::Write() */
		/* �Ե�ǰ�ͳ���RTP��,�õ�ǰ����С��������� */
        void AddPacketToWindow(SInt32 inPacketSize);
        
        // ����ϴη���ʱoverbuffer window�������µİ�,�ͷŸô����ڴ�ռ�
        // As time passes, transmit times that were in the future become transmit
        // times that are in the past or present. Call this function to empty
        // those old packets out of the window, freeing up space in the window.
        void EmptyOutWindow(const SInt64& inCurrentTime);
        
        // MarkBeginningOfWriteBurst
        // Call this on the first write of a write burst for a client. This
        // allows the overbuffer window to track whether the bitrate of the movie
        // is above the play rate.
		/* used in RTPStream::Write() */
		/* ���д�ŷ�,������overbuffering Window��ر����� */
        void MarkBeginningOfWriteBurst() { fWriteBurstBeginning = true; }       

    private:
        
		//overbuffer���ʹ��ڴ�С
        SInt32 fWindowSize;
		//��ǰ�����ֽ���
        SInt32 fBytesSentSinceLastReport;
		//���ݰ����ͼ������СֵĬ��50����,�μ�streamingserver.xml
        SInt32 fSendInterval;

		//��ǰһ���ڷ��͵��ֽ���
        SInt32 fBytesDuringLastSecond;
		//��ǰһ�뿪ʼ��ʱ�䣬cf.fPreviousSecondStart
        SInt64 fLastSecondStart;

		//ǰһ���ڷ��͵����ֽ���
        SInt32 fBytesDuringPreviousSecond;
		//ǰһ��Ŀ�ʼʱ��
        SInt64 fPreviousSecondStart;

		//һ������ڷ��͵��ֽ���,һ�����Ϊ50����
        SInt32 fBytesDuringBucket;
		//һ������Ŀ�ʼʱ��
        SInt64 fBucketBegin;
		//ǰһ������Ŀ�ʼʱ��
        SInt64 fPreviousBucketBegin;

		//��ǰBucket interval�ĳ�ǰʱ��(�Ͱ���ʱ) = ����ʱ�� �� ��ǰʱ��
        SInt64 fBucketTimeAhead;
		//ǰBucket interval�ĳ�ǰʱ��
        SInt64 fPreviousBucketTimeAhead;

		//���ǰ�Ͱ�ʱ�䣬Ĭ��Ϊ25��
        UInt32 fMaxSendAheadTime;

		//�Ƿ�ʼ����д?
        Bool16 fWriteBurstBeginning;

        //��ǰ���Ϳ���,���òμ�RTPStream::SetOverBufferState()
        Bool16 fOverbufferingEnabled;

		//��ǰ���ʣ����Ƶ�ǰ��ǰʱ�䲻�ܹ���Ĭ��Ϊ2.0,һ�����1���μ�streamingserver.xml
		Float32 fOverbufferRate;

        //��ǰ���ͳ���ʱ��,1000����(�����캯��)?
		UInt32 fSendAheadDurationInMsec;
		
		//���ó�ǰ���ʹ��ڿ�ʼ��ʱ��ʱ��
		SInt64 fOverbufferWindowBegin;
};


#endif // __RTP_OVERBUFFER_TRACKER_H__


