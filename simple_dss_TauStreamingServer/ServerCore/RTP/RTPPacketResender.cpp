/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPPacketResender.h
Description: RTPPacketResender class to buffer and track�����沢׷�٣�re-transmits of RTP packets..
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <stdio.h>

#include "RTPPacketResender.h"
#include "RTPStream.h"
#include "atomic.h"
#include "OSMutex.h"

#if RTP_PACKET_RESENDER_DEBUGGING
#include "QTSSRollingLog.h"
#include "defaultPaths.h"
#include <stdarg.h>



/* ׷������ʱ����ش������е���󳤶ȵ���־�ļ��� */
class MyAckListLog : public QTSSRollingLog
{
    public:
    
        MyAckListLog(char * logFName) : QTSSRollingLog() {this->SetTaskName("MyAckListLog"); ::strcpy( fLogFName, logFName ); }
        virtual ~MyAckListLog() {}
    
		/* �õ���־�ļ����� */
        virtual char* GetLogName() 
        { 
            char *logFileNameStr = NEW char[80];    
            ::strcpy( logFileNameStr, fLogFName );
            return logFileNameStr; 
        }
        
		/* �õ���־�ļ�Ŀ¼ */
        virtual char* GetLogDir()
        { 
            char *logDirStr = NEW char[80];
           ::strcpy( logDirStr, DEFAULTPATHS_LOG_DIR);
           return logDirStr; 
        }
        
        virtual UInt32 GetRollIntervalInDays()  { return 0; }
        virtual UInt32 GetMaxLogBytes()         { return 0; }
     
		/* ��־�ļ����� */
       char    fLogFName[128];
    
};
#endif


static const UInt32 kPacketArrayIncreaseInterval = 32;// �����ش��������С�Ĳ���(һ�ξ�����32��),must be multiple of 2
static const UInt32 kInitialPacketArraySize = 64;// ����ط��������Ԫ�ظ���, must be multiple of kPacketArrayIncreaseInterval (Turns out this is as big as we typically need)
//static const UInt32 kMaxPacketArraySize = 512;// must be multiple of kPacketArrayIncreaseInterval it would have to be a 3 mbit or more
static const UInt32 kMaxDataBufferSize = 1600; //BufferPool��ÿƬ�����С�趨Ϊ1600�ֽ�

OSBufferPool RTPPacketResender::sBufferPool(kMaxDataBufferSize);
unsigned int RTPPacketResender::sNumWastedBytes = 0;/* BufferPool�����ĵ��ֽ��� */

RTPPacketResender::RTPPacketResender()
:   fBandwidthTracker(NULL),//��Ҫ���ⴴ��
    fSocket(NULL),
    fDestAddr(0),
    fDestPort(0),
    fMaxPacketsInList(0),
    fPacketsInList(0),
    fNumResends(0),
    fNumExpired(0),
    fNumAcksForMissingPackets(0),
    fNumSent(0),
    fPacketArray(NULL), //����������
    fPacketArraySize(kInitialPacketArraySize),//default 64
    fPacketArrayMask(0),
    fHighestSeqNum(0),
    fLastUsed(0),
    fPacketQMutex()
{
	/***************** ������Ҫ,ע�����ﴴ���ṹ������ļ����!!  **************************/
    fPacketArray = (RTPResenderEntry*) NEW char[sizeof(RTPResenderEntry) * fPacketArraySize];
    ::memset(fPacketArray,0,sizeof(RTPResenderEntry) * fPacketArraySize);
	/***************** ������Ҫ,ע�����ﴴ���ṹ������ļ����!!  **************************/

}

RTPPacketResender::~RTPPacketResender()
{
	/* ��������ش�������,�ͷŶ������Ļ���,���¾�̬����sNumWastedBytes��sBufferPool */
    for (UInt32 x = 0; x < fPacketArraySize; x++)
    {
        if (fPacketArray[x].fPacketSize > 0)/* �����ȥ���а�δ�����Ĳ��� */
            atomic_sub(&sNumWastedBytes, kMaxDataBufferSize - fPacketArray[x].fPacketSize);
        if (fPacketArray[x].fPacketData != NULL)
        {
			/* ������ش����Ǵ���ڶ����special buffer��,��Ҫ����ɾ��.���Ĵ����μ�RTPPacketResender::GetEmptyEntry() */
            if (fPacketArray[x].fIsSpecialBuffer)
                delete [] (char*)fPacketArray[x].fPacketData;
            else
				/* ����,ֱ�ӽ�����Ϊ����Ԫ����OSBufferPool */
                sBufferPool.Put(fPacketArray[x].fPacketData);
        }
    }
       
	/**************** ע��:������ɾ����Ӧ�Ľṹ������������Ϣ,��������OSBufferPool��,��������� *********************/
    delete [] fPacketArray;
	/**************** ע��:������ɾ����Ӧ�Ľṹ������ *********************/
}

#if RTP_PACKET_RESENDER_DEBUGGING
/* д��־�ļ�,����ӡ��־��Ϣ����Ļ,ע��Զ��̲߳���ȫ */
void RTPPacketResender::logprintf( const char * format, ... )
{
    /*
        WARNING - the logger is not multiple task thread safe.
        its OK when we run just one thread for all of the
        sending tasks though.
        each logger for a given session will open up access
        to the same log file.  with one thread we're serialized
        on writing to the file, so it works. 
    */
    
    va_list argptr;
    char    buff[1024];
    

    va_start( argptr, format); 
    vsprintf( buff, format, argptr);
    va_end(argptr);

    if ( fLogger )
    {
		/* д��־�ļ�����ӡ��־��Ϣ����Ļ  */
        fLogger->WriteToLog(buff, false);
        qtss_printf( buff );
    }
}

/* ������Ӧ��Ϣ */
void RTPPacketResender::SetDebugInfo(UInt32 trackID, UInt16 remoteRTCPPort, UInt32 curPacketDelay)
{
    fTrackID = trackID;
    fRemoteRTCPPort = remoteRTCPPort;
    fCurrentPacketDelay = curPacketDelay;
}

/* ����ָ�����Ƶ���־�ļ�����,���� */
void RTPPacketResender::SetLog( StrPtrLen *logname )
{
    /*
        WARNING - see logprintf()
    */
    
    char    logFName[128];
    
    memcpy( logFName, logname->Ptr, logname->Len );
    logFName[logname->Len] = 0;
    
    if ( fLogger )
        delete fLogger;

	/* ����ָ�����Ƶ���־�ļ����� */
    fLogger = new MyAckListLog( logFName );
    
	/* ����־�ļ� */
    fLogger->EnableLog();
}

/* ׷������ʱ��(1s)���ش������е���󳤶�,��ӡ��־��Ϣ */
void RTPPacketResender::LogClose(SInt64 inTimeSpentInFlowControl)
{
    this->logprintf("Flow control duration msec: %"_64BITARG_"d. Max outstanding packets: %d\n", inTimeSpentInFlowControl, this->GetMaxPacketsInList());
    
}

/* ��ָ��ʱ�����ڷ������ֽ���������,��������� */
UInt32 RTPPacketResender::SpillGuts(UInt32 inBytesSentThisInterval)
{
    if (fInfoDisplayTimer.DurationInMilliseconds() > 1000 )
    {
        //fDisplayCount++;
        
        // spill our guts on the state of KRR
        char *isFlowed = "open";
        
		/* ����ʹ������ */
        if ( fBandwidthTracker->IsFlowControlled() )
            isFlowed = "flowed";
        
		/* ���㷢���ٶ�kbit/s */
        SInt64  kiloBitperSecond = (( (SInt64)inBytesSentThisInterval * (SInt64)1000 * (SInt64)8 ) / fInfoDisplayTimer.DurationInMilliseconds() ) / (SInt64)1024;
        
        //fStreamCumDuration += fInfoDisplayTimer.DurationInMilliseconds();
        fInfoDisplayTimer.Reset();

        //this->logprintf( "\n[%li] info for track %li, cur bytes %li, cur kbit/s %li\n", /*(long)fStreamCumDuration,*/ fTrackID, (long)inBytesSentThisInterval, (long)kiloBitperSecond);
        this->logprintf( "\nx info for track %li, cur bytes %li, cur kbit/s %li\n", /*(long)fStreamCumDuration,*/ fTrackID, (long)inBytesSentThisInterval, (long)kiloBitperSecond);
        this->logprintf( "stream is %s, bytes pending ack %li, cwnd %li, ssth %li, wind %li \n", isFlowed, fBandwidthTracker->BytesInList(), fBandwidthTracker->CongestionWindow(), fBandwidthTracker->SlowStartThreshold(), fBandwidthTracker->ClientWindowSize() );
        this->logprintf( "stats- resends:  %li, expired:  %li, dupe acks: %li, sent: %li\n", fNumResends, fNumExpired, fNumAcksForMissingPackets, fNumSent );
        this->logprintf( "delays- cur:  %li, srtt: %li , dev: %li, rto: %li, bw: %li\n\n", fCurrentPacketDelay, fBandwidthTracker->RunningAverageMSecs(), fBandwidthTracker->RunningMeanDevationMSecs(), fBandwidthTracker->CurRetransmitTimeout(), fBandwidthTracker->GetCurrentBandwidthInBps());
           
        inBytesSentThisInterval = 0;
    }
    return inBytesSentThisInterval;
}

#endif

/* �����ش������з��͵�Ŀ�ĵص�ip&port,UDPSocket��,�����ڵ���ǰ�����ȵ����������,�μ�RTPStream::Setup() */
void RTPPacketResender::SetDestination(UDPSocket* inOutputSocket, UInt32 inDestAddr, UInt16 inDestPort)
{
    fSocket = inOutputSocket;
    fDestAddr = inDestAddr;
    fDestPort = inDestPort;
}

/* ��һ�����õ�ȷ��/�µ�play������/���ݰ��������ط�/�쳣״��(GetEmptyEntry)ʱ����Ҫ����RemovePacket��������ָ�������ݰ��Ӷ������Ƴ��� */
/* �ҵ���һ�����ָ�����ش���λ��,�������ݽ�OSBufferPool.���ڶ������ָ��Ҫ���øð�λ��,�����ش�������,�����Ԫ�Ƶ���ǰ��λ��;�������ø�λ��,
���ø�������С����congestion Window��С,�����������ֽ���fBytesInList */
void RTPPacketResender::RemovePacket(UInt32 packetIndex, Bool16 reuseIndex)
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);

	//ȷ�����packetIndex��[0,fPacketArraySize-1]��Χ��
    Assert(packetIndex < fPacketArraySize);
    if (packetIndex >= fPacketArraySize)
        return;
    
	//û��һ����,������ȥ
    if (fPacketsInList == 0)
        return;
    
	/* �õ���һ�����ָ�����ش���ָ�� */
    RTPResenderEntry* theEntry = &fPacketArray[packetIndex];
	/* ������ʵ�ʳ���Ϊ��,��������ȥ,�������� */
    if (theEntry->fPacketSize == 0)
        return;
        
    // Track the number of wasted bytes we have
	/* ׷��BufferPool�����˷ѵ��ֽ���,��ȥ�ð�δ�õ��ֽ��� */
    atomic_sub(&sNumWastedBytes, kMaxDataBufferSize - theEntry->fPacketSize);
    Assert(theEntry->fPacketSize > 0);

    // Update our list information
	/* ȷ���ط��������е�ǰ�����ش��� */
    Assert(fPacketsInList > 0);
    
	/* ������ר��Buffer,ֱ���ͷŵ�;���������ش���������,����BufferPool����ȳ��Ļ���� */
    if (theEntry->fIsSpecialBuffer)
    {   
		delete [] (char*)theEntry->fPacketData;
    }
    else if (theEntry->fPacketData != NULL)
        sBufferPool.Put(theEntry->fPacketData);
        
    /* ���ָ�����ݰ���λ��ϣ�������ã���Ѷ��������һ������������ǰλ�ã����һ����λ������,��ʹ���������ݰ���ĿfPacketsInList��1��
	���ǲ�������cwnd����Ϊ�ڵ���RemovePacket֮ǰ��֮�󶼻����fBandwidthTracker->EmptyWindow����ȥ������ */
    if (reuseIndex) // we are re-using the space so keep array contiguous
    {
        fPacketArray[packetIndex] = fPacketArray[fPacketsInList -1]; 
        ::memset(&fPacketArray[fPacketsInList -1],0,sizeof(RTPResenderEntry));
        fPacketsInList--;

    }
    else    // the array is full
    {
		/* �ø�������С����һ���㷨,����congestion Window��С,�����������ֽ���fBytesInList */
        fBandwidthTracker->EmptyWindow( theEntry->fPacketSize, false ); // keep window available
        ::memset(theEntry,0,sizeof(RTPResenderEntry));//��ոýṹ��
    }
    
}

/* ���ش����������ҵ�������һ����ʼ�����EmptyEntry(Ҫô����NULL,Ҫô����,Ҫô����),���ָ����RTP�ش�����Ϣ,ͬʱ�趨���İ����ݻ�������OSBufferPool,���������ⴴ����special buffer */
RTPResenderEntry*   RTPPacketResender::GetEmptyEntry(UInt16 inSeqNum, UInt32 inPacketSize)
{ 
    RTPResenderEntry* theEntry = NULL;
    
	/* �����ش�������,�����Ƿ���ָ��SeqNum��RTP��,�оͱ�ʾû��Empty entry,����NULL */
    for (UInt32 packetIndex = 0 ;packetIndex < fPacketsInList; packetIndex++) // see if packet is already in the array
    {   
		if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
        {  
			return NULL;
        }
    }

	/* �����ش�������ﵽָ���ĸ�������(�˴���64),�������� */
    if (fPacketsInList == fPacketArraySize) // allocate a new array
    {
		/* ����С������32�� */
        fPacketArraySize += kPacketArrayIncreaseInterval;
		/* �����µ�64+32=96���ش�������,��ʼ��,���ƽ�ԭ�����ش�������,ɾ��ԭ�����ش������� */
        RTPResenderEntry* tempArray = (RTPResenderEntry*) NEW char[sizeof(RTPResenderEntry) * fPacketArraySize];
        ::memset(tempArray,0,sizeof(RTPResenderEntry) * fPacketArraySize);
        ::memcpy(tempArray,fPacketArray,sizeof(RTPResenderEntry) * fPacketsInList);
        delete [] fPacketArray;
		/* ����ԭ�����ش������� */
        fPacketArray = tempArray;
        qtss_printf("NewArray size=%ld packetsInList=%ld\n",fPacketArraySize, fPacketsInList);
    }

	/* ���統ǰԪ�ظ���û�дﵽ�ش�������Ԫ�ش�С(64) */
    if (fPacketsInList <  fPacketArraySize) // have an open spot
    {   
		/* ����ǰ���һ���ش�������һ��λ��,ָ����Ҫ�ҵ�EmptyEntry */
		theEntry = &fPacketArray[fPacketsInList];
		/* ��ǰԪ�ظ�����1 */
        fPacketsInList++;
        
		/* ��ʱ����fLastUsed,�´θ��õ��ش�RTP����Index */
        if (fPacketsInList < fPacketArraySize)
            fLastUsed = fPacketsInList;
        else
            fLastUsed = fPacketArraySize;
    }
    else  /* ����,fPacketsInList=fPacketArraySize(�μ�������������),���������ϴθ��õ��ش�RTP����Index */
    {
        // nothing open so re-use 
		/* ���ܴ򿪸�����,���������� */
        if (fLastUsed < fPacketArraySize - 1)
            fLastUsed ++;/* ��fLastUsed��ΪfPacketArraySize-1 */
        else
            fLastUsed = 0; /* �������� */
            
        qtss_printf("array is full = %lu reusing index=%lu\n",fPacketsInList,fLastUsed);
		/* �����θ��õ��ش�����EmptyEntry,Ҫô���ش�������ͷ,Ҫô��β */
        theEntry = &fPacketArray[fLastUsed];
		/* �Ƴ������ð�������,��BufferPool����ȳ��Ļ����,�ճ�����λ�� */
        RemovePacket(fLastUsed, false); // delete packet in place don't fill we will use the spot
    }
            
    // Check to see if this packet is too big for the buffer. If it is, then we need to specially allocate a special buffer
	/* ����ָ����RTP����С����OSBufferPool�Ĵ�С(1600�ֽ�),�Ͷ����ٷ���һ��special buffer���洢��RTP�������� */
    if (inPacketSize > kMaxDataBufferSize)
    {
        //sBufferPool.Put(theEntry->fPacketData);
        theEntry->fIsSpecialBuffer = true;
        theEntry->fPacketData = NEW char[inPacketSize];
    }
    else// It is not special, it's from the buffer pool
    {   
		/* ����,�ش��������ݻ���ֱ��ȡ��OSBufferPool(��������Put()�ȷ���) */
		theEntry->fIsSpecialBuffer = false;
        theEntry->fPacketData = sBufferPool.Get();/* ɾ������������� */
    }

    return theEntry;
}

/* ÿһ��play���������Ժ󣬷���������play���󣬶�������ClearOutstandingPackets��������ȥ��ǰ�Ļ���������������ش���,��շ��ʹ������ݲ������������ڴ�С�� */
void RTPPacketResender::ClearOutstandingPackets()
{   
    //OSMutexLocker packetQLocker(&fPacketQMutex);
    
    for (UInt16 packetIndex = 0; packetIndex < fPacketsInList; packetIndex++)
    {
		/* ��ȥ��ǰ�Ļ���������������ش���(��BufferPool����ȳ��Ļ����),�������������ڴ�С,�����������ֽ���fBytesInList */
        this->RemovePacket(packetIndex,false);// don't move packets delete in place
        Assert(fPacketArray[packetIndex].fPacketSize==0);
    }

    if (fBandwidthTracker != NULL)
		/* ��մ�������,ʹfBytesInListΪ0���ٴε������������� */
        fBandwidthTracker->EmptyWindow(fBandwidthTracker->BytesInList()); //clean it out

    fPacketsInList = 0; // deleting in place doesn't decrement������ش�������  
    Assert(fPacketsInList == 0);
}

/* used in RTPStream::ReliableRTPWrite() */
/* ��ָ����RTP�������ش�������,���������Ա��ֵ,����Congestion Window��,���·��͵�δ�õ�ȷ�ϵ��ֽ��� */
void RTPPacketResender::AddPacket( void * inRTPPacket, UInt32 packetSize, SInt32 ageLimit )
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);

    // the caller needs to adjust the overall age limit by reducing it by the current packet lateness. 
    // we compute a re-transmit timeout(RTT) based on the Karn's RTT estimate
    
	/* ��RTPPacketָ��ת��ΪUInt16���� */
    UInt16* theSeqNumP = (UInt16*)inRTPPacket;
	/* ȡRTPPacket�����еĵ�3��4�ֽ�,�õ��ð������к� */
    UInt16 theSeqNum = ntohs(theSeqNumP[1]);
    
	// ageLimit = �������̵ķ���ʱ����ʱ -����ǰʱ�� - �ð����÷���ʱ�䣩��������˵�������ݰ���Ȼ��Ч��׼������
    if ( ageLimit > 0 )
    {   
		/* ���ش����������ҵ�һ����ʼ�����EmptyEntry,���ָ����RTP�ش�����Ϣ,ͬʱ�����İ����ݴ���OSBufferPool,���������ⴴ����special buffer */
        RTPResenderEntry* theEntry = this->GetEmptyEntry(theSeqNum, packetSize);

        // This may happen if this sequence number has already been added.�μ�RTPPacketResender::GetEmptyEntry()�Ĵ������
        // That may happen if we have repeat packets in the stream.
		/* �����RTP�������ش���������,�������ظ���(δ��ʼ��),���������,�������� */
        if (theEntry == NULL || theEntry->fPacketSize > 0)
            return;
            
        // Reset all the information in the RTPResenderEntry
		//�ڶ��������øð��ĸ������������ش�������cwnd�Ȳ�����
		/************** �ش����ṹ�帳ֵ  ******************/
        ::memcpy(theEntry->fPacketData, inRTPPacket, packetSize);//��RTP����ʵ�����ݷ���OSBufferPool,����ר�л���
        theEntry->fPacketSize = packetSize;
        theEntry->fAddedTime = OS::Milliseconds();//�����ش����ĵ�ǰʱ���
		/* ��ȡ��RTP����RTO */
        theEntry->fOrigRetransTimeout = fBandwidthTracker->CurRetransmitTimeout();
		/* ʵ����Ϊ��ǰʱ���+�������̵ķ���ʱ����ʱ */
        theEntry->fExpireTime = theEntry->fAddedTime + ageLimit;
		/* �ð����ش����� */
        theEntry->fNumResends = 0;
		/* �ð������к� */
        theEntry->fSeqNum = theSeqNum;
		/************** �ش����ṹ�帳ֵ  ******************/
        
        // Track the number of wasted bytes we have
		//ͳ�ƶ������˷ѵ��ֽ���,Ϊ������������û������С�Ȳ����ṩ����,���ϸð��˷ѵ��ֽ���
        atomic_add(&sNumWastedBytes, kMaxDataBufferSize - packetSize);
        
        //PLDoubleLinkedListNode<RTPResenderEntry> * listNode = NEW PLDoubleLinkedListNode<RTPResenderEntry>( new RTPResenderEntry(inRTPPacket, packetSize, ageLimit, fRTTEstimator.CurRetransmitTimeout() ) );
        //fAckList.AddNodeToTail(listNode);
		 //���·��͵�δ�õ�ȷ�ϵ��ֽ�������������cwnd�Ĵ�С���Ƚϣ������жϵ�ǰ�����Ƿ��Ѿ�������
        fBandwidthTracker->FillWindow(packetSize);
    }
    else //����ð��ö���
    {
#if RTP_PACKET_RESENDER_DEBUGGING 
		/* ע������������ı��ʽ */
        this->logprintf( "packet too old to add: seq# %li, age limit %li, cur late %li, track id %li\n", (long)ntohs( *((UInt16*)(((char*)inRTPPacket)+2)) ), (long)ageLimit, fCurrentPacketDelay, fTrackID );
#endif
		/* ���ϵ��ش���������1 */
        fNumExpired++;
    }

    //�ܵ��ش�������1
    fNumSent++;
}


/* �ӿͻ����յ�Ack���ݰ�ȷ���Ժ�,�Ի��Ack���ش��������´���:���ѹ��ڲ����ش��������е��ش���,��������ݰ�ֵ1466������congestion Window(cwd),���ǲ�����δ�õ�ȷ�ϵ��ֽ���;
���յ�Ack��(��������ط�)�����ش��������е��ش���,�����С������cwnd�Ĵ�С������δ�õ�ȷ�ϵ��ֽ���,���Ӷ�����ɾ�������ݰ�,����������  */
void RTPPacketResender::AckPacket( UInt16 inSeqNum, SInt64& inCurTimeInMsec )
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);
    
	/* �ҵ������SeqNumָ�����ش���������Ԫ������ */
    SInt32 foundIndex = -1;

	/* �����ش�������,�������SeqNumָ�����ش���.�ҵ������ֹѭ�� */
    for (UInt32 packetIndex = 0 ;packetIndex < fPacketsInList; packetIndex++)
    {   if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
        {   
			foundIndex = packetIndex;
            break;
        }
    }
    
	/* �ҳ���һ�����SeqNumָ�����ش��� */
    RTPResenderEntry* theEntry = NULL;
    if (foundIndex != -1)
        theEntry = &fPacketArray[foundIndex];

    /*  we got an ack for a packet that has already expired or for a packet whose re-transmit crossed with it's original ack  */
	/* �����յ�Ack���ش���������,���ڵ�ǰ�ش�������,����,�ش��ð���,Ack�ֻ�����,����1466����congestion Window��С */
    if (theEntry == NULL || theEntry->fPacketSize == 0 )
    {    
#if RTP_PACKET_RESENDER_DEBUGGING   
        this->logprintf( "acked packet not found: %li, track id %li, OS::MSecs %li\n" , 
       (long)inSeqNum, fTrackID, (long)OS::Milliseconds() );
#endif
		/* ���ڲ����ش���������,�����յ�Ack�����ش����ĸ���,��1 */
        fNumAcksForMissingPackets++;
        qtss_printf("Ack for missing packet: %d\n", inSeqNum);
         
        // hmm.. we -should not have- closed down the window in this case so reopen it a bit as we normally would.
        // ���ǹر���(����Ӧ��)����,��ʱ��ͨ����������,���´���
        // ?? who know's what it really was, just use kMaximumSegmentSize
		/* �����Ѿ��Ҳ���������ݰ����ٶ��ð���С����һ�����ݰ������ֵ��1466���ֽڣ������ݸ�ֵ����cwnd�Ĵ�С�����ǲ�����δ�õ�ȷ�ϵ��ֽ����� */
        fBandwidthTracker->EmptyWindow( RTPBandwidthTracker::kMaximumSegmentSize, false );

        // when we don't add an estimate from re-transmitted segments we're actually *underestimating* 
        // both the variation and srtt since we're throwing away ALL estimates above the current RTO!
        // therefore it's difficult for us to rapidly adapt to increases in RTT, as well as RTT that
        // are higher than our original RTO estimate.
        
        // for duplicate acks, use 1.5x the cur RTO as the RTT sample
        // fRTTEstimator.AddToEstimate( fRTTEstimator.CurRetransmitTimeout() * 3 / 2 );
        // this results in some very very big RTO's since the dupes come in batches of maybe 10 or more!
        qtss_printf("Got ack for expired packet %d\n", inSeqNum);
    }
    else /* �����ҵ��յ�Ӧ��Ack���ش��� */
    {

#if RTP_PACKET_RESENDER_DEBUGGING
        Assert(inSeqNum == theEntry->fSeqNum);
        this->logprintf( "Ack for packet: %li, track id %li, OS::MSecs %qd\n", (long)inSeqNum, fTrackID, OS::Milliseconds());     
#endif   
		/* ���ҵ�������ش�����С������cwnd�Ĵ�С������δ�õ�ȷ�ϵ��ֽ���(�ڶ������Ĭ��Ϊtrue) */
        fBandwidthTracker->EmptyWindow(theEntry->fPacketSize);

		/* ���״η��Ͳ��õ�Ack���ش�����RTT��Ϊ������Estimate��ǰ��RTO,����Karn Algorithm��˼���������!  */
        if ( theEntry->fNumResends == 0 )
        {
            // add RTT sample...        
            // only use rtt from packets acked after their initial send, do not use
            // estimates gatherered from re-trasnmitted packets.
            // fRTTEstimator.AddToEstimate( theEntry->fPacketRTTDuration.DurationInMilliseconds() );
			// ע��������յ���Ack�ĵ�ǰʱ���-���ش������͵�ʱ���(��������ش�RTP��ʱ�ĵ�ǰʱ���)
            fBandwidthTracker->AddToRTTEstimate( (SInt32) ( inCurTimeInMsec - theEntry->fAddedTime ) );
            qtss_printf("Got ack for packet %d RTT = %qd\n", inSeqNum, inCurTimeInMsec - theEntry->fAddedTime);
        }
        else /* ��������һ������ش�����յ�Ack�İ�,�ʹ�ӡ��ʾ������Ϣ */
        {
    #if RTP_PACKET_RESENDER_DEBUGGING
            this->logprintf( "re-tx'd packet acked.  ack num : %li, pack seq #: %li, num resends %li, track id %li, size %li, OS::MSecs %qd\n" \
            , (long)inSeqNum, (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ), (long)theEntry->fNumResends
            , (long)fTrackID, theEntry->fPacketSize, OS::Milliseconds() );
    #endif
        }
		//�Ӷ�����ɾ�������ݰ�,����������
        this->RemovePacket(foundIndex);
    }
}

/* used in RTPStream::ReliableRTPWrite() */
/* �������ǰ,�����ش�������,�Ե��˳�ʱ����ʱ��ȴû���յ�Ackȷ�ϵ��ش���,�ֳ����ࣺ
һ�����Ѿ�����������ޣ������ط�;��һ���ǻ�δ��������ޣ�����ѭ��,���������ط� */
void RTPPacketResender::ResendDueEntries()
{
	/* ����û���ش���,�������� */
    if (fPacketsInList <= 0)
        return;
        
    //OSMutexLocker packetQLocker(&fPacketQMutex);

	/* resend loop count */
    SInt32 numResends = 0;
    RTPResenderEntry* theEntry = NULL; 
    SInt64 curTime = OS::Milliseconds();

    //�����ݰ����������һ���������ݵ�Ԫ�ؿ�ʼ����
    for (SInt32 packetIndex = fPacketsInList -1; packetIndex >= 0; packetIndex--) // walk backwards because remove packet moves array members forward
    {
		/* ȡ��ָ���������ش��� */
        theEntry = &fPacketArray[packetIndex]; 
		/* �����Ǹ��հ�,��������һ���ش��� */
        if (theEntry->fPacketSize == 0) 
            continue;
        
		/* ������ش��������ش������е�ʱ����뵱ǰʱ���,�����˵�ǰRTO,�����ݰ������г�ʱû���յ�ȷ�ϵ����ݷֳ����ࣺ
		һ�����Ѿ�����������ޣ������ط�;��һ���ǻ�δ��������ޣ��������Զ���ط� */
        if ((curTime - theEntry->fAddedTime) > fBandwidthTracker->CurRetransmitTimeout())
        {
            // Change:  Only expire packets after they were due to be resent(��������Ϊ�ط�). This gives the client
            // a chance to ack them and improves congestion avoidance and RTT calculation
            // ����Clientһ������ȥȷ������,������ӵ��������ش�ʱ��RTT�ļ���
			/* ���統ǰ�������� */
            if (curTime > theEntry->fExpireTime)
            {
				//���ҽ���RTP_PACKET_RESENDER_DEBUGGING�걻�趨ʱ�����������ͳ��ֵ����д��������־����
    #if RTP_PACKET_RESENDER_DEBUGGING   
                unsigned char version;
                version = *((char*)theEntry->fPacketData);
                version &= 0x84;    // grab most sig 2 bits
                version = version >> 6; // shift by 6 bits,��ø�RTP����version
                this->logprintf( "expired:  seq number %li, track id %li (port: %li), vers # %li, pack seq # %li, size: %li, OS::Msecs: %qd\n", \
                                    (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ), fTrackID,  (long) ntohs(fDestPort), \
                                    (long)version, (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2))), theEntry->fPacketSize, OS::Milliseconds() );
    #endif
                // This packet is expired
				/* ���ڰ�������1 */
                fNumExpired++;
                //qtss_printf("Packet expired: %d\n", ((UInt16*)thePacket)[1]);

				/* ����cwd���ڴ�С,�����µ�ǰδ�õ�ȷ�ϵ��ֽ���fBytesInList */
                fBandwidthTracker->EmptyWindow(theEntry->fPacketSize);
				/* �����ݰ�������ɾ���ð�,��������� */
                this->RemovePacket(packetIndex);
                qtss_printf("Expired packet %d\n", theEntry->fSeqNum);

				/* ������һ���ش��� */
                continue;
            }
            
            // Resend this packet
			/* ����û����(û������ǰRTO),��udp Socket�ش��ð���Client */
            fSocket->SendTo(fDestAddr, fDestPort, theEntry->fPacketData, theEntry->fPacketSize);
            qtss_printf("Packet resent: %d\n", ((UInt16*)theEntry->fPacketData)[1]);

			/* �ð����ط��Ĵ�����1 */
            theEntry->fNumResends++;
    #if RTP_PACKET_RESENDER_DEBUGGING   
            this->logprintf( "re-sent: %li RTO %li, track id %li (port %li), size: %li, OS::Ms %qd\n", (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ),  curTime - theEntry->fAddedTime, \
                    fTrackID, (long) ntohs(fDestPort), theEntry->fPacketSize, OS::Milliseconds());                
    #endif      
       	
            fNumResends++;//�ط����ݰ����ܴ�����1
            numResends ++;/* resend loop count */
            qtss_printf("resend loop numResends=%ld packet theEntry->fNumResends=%ld stream fNumResends=\n",numResends,theEntry->fNumResends++, fNumResends);
                        
            // ok -- lets try this.. add 1.5x of the INITIAL duration since the last send to the rto estimator
            // since we won't get an ack on this packet this should keep us from exponentially increasing due o a one time increase 
            // in the actuall rtt, only AddToEstimate on the first resend ( assume that it's a dupe��ƭ )
            // if it's not a dupe, but rather an actual loss, the subseqnuent actuals wil bring down the average quickly
            
			/* �������״��ش��ð�,����Karn�㷨����RTT */
            if ( theEntry->fNumResends == 1 )
                fBandwidthTracker->AddToRTTEstimate( (SInt32) ((theEntry->fOrigRetransTimeout  * 3) / 2 ));
            
            qtss_printf("Retransmitted packet %d\n", theEntry->fSeqNum);

			//���·���ʱ��,�Ա�ifѭ��ʹ��
            theEntry->fAddedTime = curTime;
			//�ط����ݰ���ζ�ų�ʱ����,���ÿ��250ms,����һ�����㷨,����ssthresh��cwnd
            fBandwidthTracker->AdjustWindowForRetransmit();
            continue;/* ������һ���ش��� */
        }
        
    }
}

void RTPPacketResender::RemovePacket(RTPResenderEntry* inEntry){ Assert(0); }


