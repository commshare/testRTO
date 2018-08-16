
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPPacketResender.h
Description: RTPPacketResender class to buffer and track（缓存并追踪）re-transmits of RTP packets..
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2011-07-03

****************************************************************************/ 


#ifndef __RTP_PACKET_RESENDER_H__
#define __RTP_PACKET_RESENDER_H__


#include "RTPBandwidthTracker.h"/* 用于拥塞控制 */
#include "PLDoubleLinkedList.h"
#include "DssStopwatch.h"
#include "UDPSocket.h"
#include "OSMemory.h"
#include "OSBufferPool.h"
#include "OSMutex.h"

/* 调试开关,必须关闭,否则会在点播结束后再打开,会造成段错误 */
#define RTP_PACKET_RESENDER_DEBUGGING 0



class MyAckListLog;

/* 重传RTP包结构体,记录了一个重传RTP Packet的相关信息,但没有存放真正的包数据 */
class RTPResenderEntry
{
public:

	/* 存放重传包的数据的指针,它指向外部的缓存片段队列OSBufferPool(每个缓存片段是1600字节),当该大小超过1600字节时,要再专门分配一块special buffer来存放该数据 */
	void*               fPacketData;
	/* 重传包的实际大小 */
	UInt32              fPacketSize;

	/* 是特地分配的专门缓存(在OSBufferPool之外)吗?参见RTPPacketResender::GetEmptyEntry() */
	Bool16              fIsSpecialBuffer;
	/* 过期时间,超过了当前RTO */
	SInt64              fExpireTime;
	/* 当加入该重传RTP包时的当前时间戳，用于计算超时,参见RTPPacketResender::AddPacket() */
	SInt64              fAddedTime;
	/* 该包的RTO,由fBandwidthTracker->CurRetransmitTimeout()得到,参见RTPPacketResender::AddPacket() */
	SInt64              fOrigRetransTimeout;
	/* 该RTP包重传次数,参见RTPPacketResender::AckPacket()/ResendDueEntries() */
	UInt32              fNumResends;
	/* 该RTP重传包的序列号,参见RTPPacketResender::GetEmptyEntry()/AddPacket() */
	UInt16              fSeqNum;
#if RTP_PACKET_RESENDER_DEBUGGING
	/* 当加入该RTP包时,包数组的大小 */
	UInt32              fPacketArraySizeWhenAdded;
#endif
};


class RTPPacketResender
{
    public:
        
        RTPPacketResender();
        ~RTPPacketResender();
        
        // These must be called before using the object,参见RTPStream::Setup()
        void                SetDestination(UDPSocket* inOutputSocket, UInt32 inDestAddr, UInt16 inDestPort);
		/* 设置fBandwidthTracker */
        void                SetBandwidthTracker(RTPBandwidthTracker* inTracker) { fBandwidthTracker = inTracker; }
        
        // AddPacket adds a new packet to the resend queue. This will not send the packet.AddPacket itself is not thread safe. 
        void                AddPacket( void * rtpPacket, UInt32 packetSize, SInt32 ageLimitInMsec );
        
        // Acks a packet. Also not thread safe.
        void                AckPacket( UInt16 sequenceNumber, SInt64& inCurTimeInMsec );

        // Resends outstanding packets in the queue. Guess what. Not thread safe.
        void                ResendDueEntries();
        
        // Clear outstanding packets - if we no longer care about any of the outstanding, unacked packets
        void                ClearOutstandingPackets();

        // ACCESSORS
		
        Bool16              IsFlowControlled()      { return fBandwidthTracker->IsFlowControlled(); }/* 是否采用流控?实质上引用的是RTPBandwithTracker::IsFlowControlled() */
        SInt32              GetMaxPacketsInList()   { return fMaxPacketsInList; }
        SInt32              GetNumPacketsInList()   { return fPacketsInList; }
        SInt32              GetNumResends()         { return fNumResends; }
        	
        static UInt32       GetNumRetransmitBuffers() { return sBufferPool.GetTotalNumBuffers(); }/* 对OSBufferPool的查询,得到UDP buffer个数,参见QTSServerInterface::GetNumUDPBuffers() */
        static UInt32       GetWastedBufferBytes() { return sNumWastedBytes; }/* 参见QTSServerInterface::GetNumWastedBytes() */

#if RTP_PACKET_RESENDER_DEBUGGING
        void                SetDebugInfo(UInt32 trackID, UInt16 remoteRTCPPort, UInt32 curPacketDelay);
        void                SetLog( StrPtrLen *logname );
        UInt32              SpillGuts(UInt32 inBytesSentThisInterval);
        void                LogClose(SInt64 inTimeSpentInFlowControl);	
        void                logprintf( const char * format, ... );  /* 写日志文件,并打印日志信息到屏幕 */
#else
        void                SetLog( StrPtrLen * /*logname*/) {}
#endif
        
    private:
    
        // Tracking the capacity of the network
		/************************** 用于拥塞控制的类 ***********************/
		/* 在RUDP传输过程中，RTPPacketResender类和RTPBandwidthTracker类是保证QoS的至关重要的两个类。 */
        RTPBandwidthTracker* fBandwidthTracker;
		/************************** 用于拥塞控制的类 ***********************/
        
        // Who to send to	
        UDPSocket*          fSocket;/* 给对方client发送RTP数据的Server端的socket(对) */
        UInt32              fDestAddr;/* client端的ip&port */
        UInt16              fDestPort;

		/* 已经重传重传包的总词数,注意RTPResenderEntry中也有一个同名的量 */
        UInt32              fNumResends;                // how many total retransmitted packets
		/* 丢弃包总数=因超时太多不把其放在重发队列里的包个数+已在队列里但被放弃重发的包个数。 */
        UInt32              fNumExpired;                // how many total packets dropped,过期包总数
		/* 过期了,不在重发包数组中,但收到Ack的包的总个数,参见RTPPacketResender::AckPacket() */
        UInt32              fNumAcksForMissingPackets;  // how many acks received in the case where the packet was not in the list
		/* 总的重传次数,不管是否真的重传,参见RTPPacketResender::AddPacket() */
        UInt32              fNumSent;                   // how many packets sent

#if RTP_PACKET_RESENDER_DEBUGGING	
        MyAckListLog        *fLogger;           /* 用到的日志文件类 */    
        UInt32              fTrackID;           /* RTPStream所在track ID */
        UInt16              fRemoteRTCPPort;    /* Client端的RTCP端口 */
        UInt32              fCurrentPacketDelay;/* 当前RTP数据包的延时 */
        DssDurationTimer    fInfoDisplayTimer;
#endif
    
        RTPResenderEntry*   fPacketArray;     /* 重传包数组,引用上面的RTP Resender Packet类对象 */	
		UInt32              fPacketArraySize; /* 上述重传包数组的大小,default值为64,增量是32,所以只能为64,92,...,参见RTPPacketResender::RTPPacketResender()/GetEmptyEntry() */
        UInt32              fPacketArrayMask;
		UInt32              fMaxPacketsInList;/* 似乎很少用到,链表中的最大包大小 */
		UInt32              fPacketsInList;   /* 在重传包数组中当前存放数据包的个数,从1计数,注意这个量非常重要!! */
 
        UInt16              fStartSeqNum;  /* 起始包的序列号 */	
        UInt16              fHighestSeqNum;/* 重传包的最大序列号 */
        UInt32              fLastUsed;     /* 下次复用的重传RTP包的Index,从0开始,参见RTPPacketResender::GetEmptyEntry() */

        OSMutex             fPacketQMutex;/* 重传包队列的Mutex */

		/* 由索引或序列号检索重传包数组中的重传包 */
        RTPResenderEntry*   GetEntryByIndex(UInt16 inIndex);
        RTPResenderEntry*   GetEntryBySeqNum(UInt16 inSeqNum);
		/* 在重传包数组中找到一个EmptyEntry,存放指定的RTP重传包信息,同时将它的包数据存入OSBufferPool,或者是另外创建的special buffer */
        RTPResenderEntry*   GetEmptyEntry(UInt16 inSeqNum, UInt32 inPacketSize);

        void ReallocatePacketArray();                      /* 没有源码实现 */
	    void UpdateCongestionWindow(SInt32 bytesToOpenBy );/* 没有源码实现 */
		void RemovePacket(RTPResenderEntry* inEntry);      /* 没有源码实现 */

		/* 找到第一个入参指定的重传包位置,移其数据进OSBufferPool.若第二个入参指明要重用该包位置,缩减重传包数组,经最后元移到当前包位置;若不重用该位置,
		则直接清空窗口数据 */
        void RemovePacket(UInt32 packetIndex, Bool16 reuse=true);
		
        static OSBufferPool sBufferPool;    /* 存放重传包队列中重传包的真正数据的缓存池,用到OSBufferPool类 */	
        static unsigned int sNumWastedBytes;/* 统计浪费的字节总数(每个包未填满的部分之和) */
             
};

#endif //__RTP_PACKET_RESENDER_H__
