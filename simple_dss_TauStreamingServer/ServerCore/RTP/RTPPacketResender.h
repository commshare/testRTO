
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPPacketResender.h
Description: RTPPacketResender class to buffer and track�����沢׷�٣�re-transmits of RTP packets..
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2011-07-03

****************************************************************************/ 


#ifndef __RTP_PACKET_RESENDER_H__
#define __RTP_PACKET_RESENDER_H__


#include "RTPBandwidthTracker.h"/* ����ӵ������ */
#include "PLDoubleLinkedList.h"
#include "DssStopwatch.h"
#include "UDPSocket.h"
#include "OSMemory.h"
#include "OSBufferPool.h"
#include "OSMutex.h"

/* ���Կ���,����ر�,������ڵ㲥�������ٴ�,����ɶδ��� */
#define RTP_PACKET_RESENDER_DEBUGGING 0



class MyAckListLog;

/* �ش�RTP���ṹ��,��¼��һ���ش�RTP Packet�������Ϣ,��û�д�������İ����� */
class RTPResenderEntry
{
public:

	/* ����ش��������ݵ�ָ��,��ָ���ⲿ�Ļ���Ƭ�ζ���OSBufferPool(ÿ������Ƭ����1600�ֽ�),���ô�С����1600�ֽ�ʱ,Ҫ��ר�ŷ���һ��special buffer����Ÿ����� */
	void*               fPacketData;
	/* �ش�����ʵ�ʴ�С */
	UInt32              fPacketSize;

	/* ���صط����ר�Ż���(��OSBufferPool֮��)��?�μ�RTPPacketResender::GetEmptyEntry() */
	Bool16              fIsSpecialBuffer;
	/* ����ʱ��,�����˵�ǰRTO */
	SInt64              fExpireTime;
	/* ��������ش�RTP��ʱ�ĵ�ǰʱ��������ڼ��㳬ʱ,�μ�RTPPacketResender::AddPacket() */
	SInt64              fAddedTime;
	/* �ð���RTO,��fBandwidthTracker->CurRetransmitTimeout()�õ�,�μ�RTPPacketResender::AddPacket() */
	SInt64              fOrigRetransTimeout;
	/* ��RTP���ش�����,�μ�RTPPacketResender::AckPacket()/ResendDueEntries() */
	UInt32              fNumResends;
	/* ��RTP�ش��������к�,�μ�RTPPacketResender::GetEmptyEntry()/AddPacket() */
	UInt16              fSeqNum;
#if RTP_PACKET_RESENDER_DEBUGGING
	/* �������RTP��ʱ,������Ĵ�С */
	UInt32              fPacketArraySizeWhenAdded;
#endif
};


class RTPPacketResender
{
    public:
        
        RTPPacketResender();
        ~RTPPacketResender();
        
        // These must be called before using the object,�μ�RTPStream::Setup()
        void                SetDestination(UDPSocket* inOutputSocket, UInt32 inDestAddr, UInt16 inDestPort);
		/* ����fBandwidthTracker */
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
		
        Bool16              IsFlowControlled()      { return fBandwidthTracker->IsFlowControlled(); }/* �Ƿ��������?ʵ�������õ���RTPBandwithTracker::IsFlowControlled() */
        SInt32              GetMaxPacketsInList()   { return fMaxPacketsInList; }
        SInt32              GetNumPacketsInList()   { return fPacketsInList; }
        SInt32              GetNumResends()         { return fNumResends; }
        	
        static UInt32       GetNumRetransmitBuffers() { return sBufferPool.GetTotalNumBuffers(); }/* ��OSBufferPool�Ĳ�ѯ,�õ�UDP buffer����,�μ�QTSServerInterface::GetNumUDPBuffers() */
        static UInt32       GetWastedBufferBytes() { return sNumWastedBytes; }/* �μ�QTSServerInterface::GetNumWastedBytes() */

#if RTP_PACKET_RESENDER_DEBUGGING
        void                SetDebugInfo(UInt32 trackID, UInt16 remoteRTCPPort, UInt32 curPacketDelay);
        void                SetLog( StrPtrLen *logname );
        UInt32              SpillGuts(UInt32 inBytesSentThisInterval);
        void                LogClose(SInt64 inTimeSpentInFlowControl);	
        void                logprintf( const char * format, ... );  /* д��־�ļ�,����ӡ��־��Ϣ����Ļ */
#else
        void                SetLog( StrPtrLen * /*logname*/) {}
#endif
        
    private:
    
        // Tracking the capacity of the network
		/************************** ����ӵ�����Ƶ��� ***********************/
		/* ��RUDP��������У�RTPPacketResender���RTPBandwidthTracker���Ǳ�֤QoS��������Ҫ�������ࡣ */
        RTPBandwidthTracker* fBandwidthTracker;
		/************************** ����ӵ�����Ƶ��� ***********************/
        
        // Who to send to	
        UDPSocket*          fSocket;/* ���Է�client����RTP���ݵ�Server�˵�socket(��) */
        UInt32              fDestAddr;/* client�˵�ip&port */
        UInt16              fDestPort;

		/* �Ѿ��ش��ش������ܴ���,ע��RTPResenderEntry��Ҳ��һ��ͬ������ */
        UInt32              fNumResends;                // how many total retransmitted packets
		/* ����������=��ʱ̫�಻��������ط�������İ�����+���ڶ����ﵫ�������ط��İ������� */
        UInt32              fNumExpired;                // how many total packets dropped,���ڰ�����
		/* ������,�����ط���������,���յ�Ack�İ����ܸ���,�μ�RTPPacketResender::AckPacket() */
        UInt32              fNumAcksForMissingPackets;  // how many acks received in the case where the packet was not in the list
		/* �ܵ��ش�����,�����Ƿ�����ش�,�μ�RTPPacketResender::AddPacket() */
        UInt32              fNumSent;                   // how many packets sent

#if RTP_PACKET_RESENDER_DEBUGGING	
        MyAckListLog        *fLogger;           /* �õ�����־�ļ��� */    
        UInt32              fTrackID;           /* RTPStream����track ID */
        UInt16              fRemoteRTCPPort;    /* Client�˵�RTCP�˿� */
        UInt32              fCurrentPacketDelay;/* ��ǰRTP���ݰ�����ʱ */
        DssDurationTimer    fInfoDisplayTimer;
#endif
    
        RTPResenderEntry*   fPacketArray;     /* �ش�������,���������RTP Resender Packet����� */	
		UInt32              fPacketArraySize; /* �����ش�������Ĵ�С,defaultֵΪ64,������32,����ֻ��Ϊ64,92,...,�μ�RTPPacketResender::RTPPacketResender()/GetEmptyEntry() */
        UInt32              fPacketArrayMask;
		UInt32              fMaxPacketsInList;/* �ƺ������õ�,�����е�������С */
		UInt32              fPacketsInList;   /* ���ش��������е�ǰ������ݰ��ĸ���,��1����,ע��������ǳ���Ҫ!! */
 
        UInt16              fStartSeqNum;  /* ��ʼ�������к� */	
        UInt16              fHighestSeqNum;/* �ش�����������к� */
        UInt32              fLastUsed;     /* �´θ��õ��ش�RTP����Index,��0��ʼ,�μ�RTPPacketResender::GetEmptyEntry() */

        OSMutex             fPacketQMutex;/* �ش������е�Mutex */

		/* �����������кż����ش��������е��ش��� */
        RTPResenderEntry*   GetEntryByIndex(UInt16 inIndex);
        RTPResenderEntry*   GetEntryBySeqNum(UInt16 inSeqNum);
		/* ���ش����������ҵ�һ��EmptyEntry,���ָ����RTP�ش�����Ϣ,ͬʱ�����İ����ݴ���OSBufferPool,���������ⴴ����special buffer */
        RTPResenderEntry*   GetEmptyEntry(UInt16 inSeqNum, UInt32 inPacketSize);

        void ReallocatePacketArray();                      /* û��Դ��ʵ�� */
	    void UpdateCongestionWindow(SInt32 bytesToOpenBy );/* û��Դ��ʵ�� */
		void RemovePacket(RTPResenderEntry* inEntry);      /* û��Դ��ʵ�� */

		/* �ҵ���һ�����ָ�����ش���λ��,�������ݽ�OSBufferPool.���ڶ������ָ��Ҫ���øð�λ��,�����ش�������,�����Ԫ�Ƶ���ǰ��λ��;�������ø�λ��,
		��ֱ����մ������� */
        void RemovePacket(UInt32 packetIndex, Bool16 reuse=true);
		
        static OSBufferPool sBufferPool;    /* ����ش����������ش������������ݵĻ����,�õ�OSBufferPool�� */	
        static unsigned int sNumWastedBytes;/* ͳ���˷ѵ��ֽ�����(ÿ����δ�����Ĳ���֮��) */
             
};

#endif //__RTP_PACKET_RESENDER_H__
