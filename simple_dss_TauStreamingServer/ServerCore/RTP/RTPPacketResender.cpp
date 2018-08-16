/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPPacketResender.h
Description: RTPPacketResender class to buffer and track（缓存并追踪）re-transmits of RTP packets..
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



/* 追踪流控时间和重传包队列的最大长度的日志文件类 */
class MyAckListLog : public QTSSRollingLog
{
    public:
    
        MyAckListLog(char * logFName) : QTSSRollingLog() {this->SetTaskName("MyAckListLog"); ::strcpy( fLogFName, logFName ); }
        virtual ~MyAckListLog() {}
    
		/* 得到日志文件名称 */
        virtual char* GetLogName() 
        { 
            char *logFileNameStr = NEW char[80];    
            ::strcpy( logFileNameStr, fLogFName );
            return logFileNameStr; 
        }
        
		/* 得到日志文件目录 */
        virtual char* GetLogDir()
        { 
            char *logDirStr = NEW char[80];
           ::strcpy( logDirStr, DEFAULTPATHS_LOG_DIR);
           return logDirStr; 
        }
        
        virtual UInt32 GetRollIntervalInDays()  { return 0; }
        virtual UInt32 GetMaxLogBytes()         { return 0; }
     
		/* 日志文件名称 */
       char    fLogFName[128];
    
};
#endif


static const UInt32 kPacketArrayIncreaseInterval = 32;// 增加重传包数组大小的步长(一次就增加32个),must be multiple of 2
static const UInt32 kInitialPacketArraySize = 64;// 存放重发包数组的元素个数, must be multiple of kPacketArrayIncreaseInterval (Turns out this is as big as we typically need)
//static const UInt32 kMaxPacketArraySize = 512;// must be multiple of kPacketArrayIncreaseInterval it would have to be a 3 mbit or more
static const UInt32 kMaxDataBufferSize = 1600; //BufferPool的每片缓存大小设定为1600字节

OSBufferPool RTPPacketResender::sBufferPool(kMaxDataBufferSize);
unsigned int RTPPacketResender::sNumWastedBytes = 0;/* BufferPool中消耗的字节数 */

RTPPacketResender::RTPPacketResender()
:   fBandwidthTracker(NULL),//需要另外创建
    fSocket(NULL),
    fDestAddr(0),
    fDestPort(0),
    fMaxPacketsInList(0),
    fPacketsInList(0),
    fNumResends(0),
    fNumExpired(0),
    fNumAcksForMissingPackets(0),
    fNumSent(0),
    fPacketArray(NULL), //创建见下面
    fPacketArraySize(kInitialPacketArraySize),//default 64
    fPacketArrayMask(0),
    fHighestSeqNum(0),
    fLastUsed(0),
    fPacketQMutex()
{
	/***************** 超级重要,注意这里创建结构体数组的简洁表达!!  **************************/
    fPacketArray = (RTPResenderEntry*) NEW char[sizeof(RTPResenderEntry) * fPacketArraySize];
    ::memset(fPacketArray,0,sizeof(RTPResenderEntry) * fPacketArraySize);
	/***************** 超级重要,注意这里创建结构体数组的简洁表达!!  **************************/

}

RTPPacketResender::~RTPPacketResender()
{
	/* 逐个遍历重传包数组,释放额外分配的缓存,更新静态变量sNumWastedBytes和sBufferPool */
    for (UInt32 x = 0; x < fPacketArraySize; x++)
    {
        if (fPacketArray[x].fPacketSize > 0)/* 逐个减去所有包未填满的部分 */
            atomic_sub(&sNumWastedBytes, kMaxDataBufferSize - fPacketArray[x].fPacketSize);
        if (fPacketArray[x].fPacketData != NULL)
        {
			/* 假如该重传包是存放在额外的special buffer中,就要将其删除.它的创建参见RTPPacketResender::GetEmptyEntry() */
            if (fPacketArray[x].fIsSpecialBuffer)
                delete [] (char*)fPacketArray[x].fPacketData;
            else
				/* 否则,直接将其作为队列元放入OSBufferPool */
                sBufferPool.Put(fPacketArray[x].fPacketData);
        }
    }
       
	/**************** 注意:这里是删除相应的结构体数组的相关信息,包数据在OSBufferPool中,处理见上面 *********************/
    delete [] fPacketArray;
	/**************** 注意:这里是删除相应的结构体数组 *********************/
}

#if RTP_PACKET_RESENDER_DEBUGGING
/* 写日志文件,并打印日志信息到屏幕,注意对多线程不安全 */
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
		/* 写日志文件并打印日志信息到屏幕  */
        fLogger->WriteToLog(buff, false);
        qtss_printf( buff );
    }
}

/* 设置相应信息 */
void RTPPacketResender::SetDebugInfo(UInt32 trackID, UInt16 remoteRTCPPort, UInt32 curPacketDelay)
{
    fTrackID = trackID;
    fRemoteRTCPPort = remoteRTCPPort;
    fCurrentPacketDelay = curPacketDelay;
}

/* 创建指定名称的日志文件对象,并打开 */
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

	/* 创建指定名称的日志文件对象 */
    fLogger = new MyAckListLog( logFName );
    
	/* 打开日志文件 */
    fLogger->EnableLog();
}

/* 追踪流控时间(1s)和重传包队列的最大长度,打印日志信息 */
void RTPPacketResender::LogClose(SInt64 inTimeSpentInFlowControl)
{
    this->logprintf("Flow control duration msec: %"_64BITARG_"d. Max outstanding packets: %d\n", inTimeSpentInFlowControl, this->GetMaxPacketsInList());
    
}

/* 对指定时间间隔内发生的字节数做分析,并返回入参 */
UInt32 RTPPacketResender::SpillGuts(UInt32 inBytesSentThisInterval)
{
    if (fInfoDisplayTimer.DurationInMilliseconds() > 1000 )
    {
        //fDisplayCount++;
        
        // spill our guts on the state of KRR
        char *isFlowed = "open";
        
		/* 假如使用流控 */
        if ( fBandwidthTracker->IsFlowControlled() )
            isFlowed = "flowed";
        
		/* 计算发送速度kbit/s */
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

/* 设置重传包队列发送的目的地的ip&port,UDPSocket类,该类在调用前必须先调用这个函数,参见RTPStream::Setup() */
void RTPPacketResender::SetDestination(UDPSocket* inOutputSocket, UInt32 inDestAddr, UInt16 inDestPort)
{
    fSocket = inOutputSocket;
    fDestAddr = inDestAddr;
    fDestPort = inDestPort;
}

/* 当一个包得到确认/新的play请求到来/数据包被放弃重发/异常状况(GetEmptyEntry)时，需要调用RemovePacket方法，将指定的数据包从队列中移除。 */
/* 找到第一个入参指定的重传包位置,移其数据进OSBufferPool.若第二个入参指明要重用该包位置,缩减重传包数组,经最后元移到当前包位置;若不重用该位置,
则用给定包大小重置congestion Window大小,但不更新其字节数fBytesInList */
void RTPPacketResender::RemovePacket(UInt32 packetIndex, Bool16 reuseIndex)
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);

	//确保入参packetIndex在[0,fPacketArraySize-1]范围内
    Assert(packetIndex < fPacketArraySize);
    if (packetIndex >= fPacketArraySize)
        return;
    
	//没有一个包,无需移去
    if (fPacketsInList == 0)
        return;
    
	/* 得到第一个入参指定的重传包指针 */
    RTPResenderEntry* theEntry = &fPacketArray[packetIndex];
	/* 假如其实际长度为零,则无需移去,立即返回 */
    if (theEntry->fPacketSize == 0)
        return;
        
    // Track the number of wasted bytes we have
	/* 追踪BufferPool中已浪费的字节数,减去该包未用的字节数 */
    atomic_sub(&sNumWastedBytes, kMaxDataBufferSize - theEntry->fPacketSize);
    Assert(theEntry->fPacketSize > 0);

    // Update our list information
	/* 确保重发包数组中当前还有重传包 */
    Assert(fPacketsInList > 0);
    
	/* 假如是专门Buffer,直接释放掉;否则若该重传包有数据,就在BufferPool放入等长的缓存块 */
    if (theEntry->fIsSpecialBuffer)
    {   
		delete [] (char*)theEntry->fPacketData;
    }
    else if (theEntry->fPacketData != NULL)
        sBufferPool.Put(theEntry->fPacketData);
        
    /* 如果指定数据包的位置希望被复用，则把队列中最后一个包拷贝到当前位置，最后一个包位置置零,并使队列中数据包数目fPacketsInList减1，
	但是并不调整cwnd，因为在调用RemovePacket之前或之后都会调用fBandwidthTracker->EmptyWindow方法去调整。 */
    if (reuseIndex) // we are re-using the space so keep array contiguous
    {
        fPacketArray[packetIndex] = fPacketArray[fPacketsInList -1]; 
        ::memset(&fPacketArray[fPacketsInList -1],0,sizeof(RTPResenderEntry));
        fPacketsInList--;

    }
    else    // the array is full
    {
		/* 用给定包大小按照一定算法,重置congestion Window大小,但不更新其字节数fBytesInList */
        fBandwidthTracker->EmptyWindow( theEntry->fPacketSize, false ); // keep window available
        ::memset(theEntry,0,sizeof(RTPResenderEntry));//清空该结构体
    }
    
}

/* 在重传包数组中找到并返回一个初始化后的EmptyEntry(要么返回NULL,要么创建,要么重用),存放指定的RTP重传包信息,同时设定它的包数据缓存来自OSBufferPool,或者是另外创建的special buffer */
RTPResenderEntry*   RTPPacketResender::GetEmptyEntry(UInt16 inSeqNum, UInt32 inPacketSize)
{ 
    RTPResenderEntry* theEntry = NULL;
    
	/* 遍历重传包数组,查找是否有指定SeqNum的RTP包,有就表示没有Empty entry,返回NULL */
    for (UInt32 packetIndex = 0 ;packetIndex < fPacketsInList; packetIndex++) // see if packet is already in the array
    {   
		if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
        {  
			return NULL;
        }
    }

	/* 假如重传包数组达到指定的个数上限(此处是64),扩容数组 */
    if (fPacketsInList == fPacketArraySize) // allocate a new array
    {
		/* 将大小再增加32个 */
        fPacketArraySize += kPacketArrayIncreaseInterval;
		/* 分配新的64+32=96个重传包数组,初始化,复制进原来的重传包数组,删除原来的重传包数组 */
        RTPResenderEntry* tempArray = (RTPResenderEntry*) NEW char[sizeof(RTPResenderEntry) * fPacketArraySize];
        ::memset(tempArray,0,sizeof(RTPResenderEntry) * fPacketArraySize);
        ::memcpy(tempArray,fPacketArray,sizeof(RTPResenderEntry) * fPacketsInList);
        delete [] fPacketArray;
		/* 重置原来的重传包数组 */
        fPacketArray = tempArray;
        qtss_printf("NewArray size=%ld packetsInList=%ld\n",fPacketArraySize, fPacketsInList);
    }

	/* 假如当前元素个数没有达到重传包数组元素大小(64) */
    if (fPacketsInList <  fPacketArraySize) // have an open spot
    {   
		/* 将当前最后一个重传包的下一个位置,指定给要找的EmptyEntry */
		theEntry = &fPacketArray[fPacketsInList];
		/* 当前元素个数加1 */
        fPacketsInList++;
        
		/* 及时更新fLastUsed,下次复用的重传RTP包的Index */
        if (fPacketsInList < fPacketArraySize)
            fLastUsed = fPacketsInList;
        else
            fLastUsed = fPacketArraySize;
    }
    else  /* 否则,fPacketsInList=fPacketArraySize(参见上面数组扩容),继续重置上次复用的重传RTP包的Index */
    {
        // nothing open so re-use 
		/* 不能打开该索引,所以重用它 */
        if (fLastUsed < fPacketArraySize - 1)
            fLastUsed ++;/* 将fLastUsed变为fPacketArraySize-1 */
        else
            fLastUsed = 0; /* 重置索引 */
            
        qtss_printf("array is full = %lu reusing index=%lu\n",fPacketsInList,fLastUsed);
		/* 将本次复用的重传包给EmptyEntry,要么是重传包数组头,要么是尾 */
        theEntry = &fPacketArray[fLastUsed];
		/* 移除该重用包的数据,在BufferPool放入等长的缓存块,空出它的位置 */
        RemovePacket(fLastUsed, false); // delete packet in place don't fill we will use the spot
    }
            
    // Check to see if this packet is too big for the buffer. If it is, then we need to specially allocate a special buffer
	/* 假如指定的RTP包大小超过OSBufferPool的大小(1600字节),就额外再分配一块special buffer来存储该RTP包的数据 */
    if (inPacketSize > kMaxDataBufferSize)
    {
        //sBufferPool.Put(theEntry->fPacketData);
        theEntry->fIsSpecialBuffer = true;
        theEntry->fPacketData = NEW char[inPacketSize];
    }
    else// It is not special, it's from the buffer pool
    {   
		/* 否则,重传包的数据缓存直接取自OSBufferPool(它必须由Put()先放入) */
		theEntry->fIsSpecialBuffer = false;
        theEntry->fPacketData = sBufferPool.Get();/* 删除另见析构函数 */
    }

    return theEntry;
}

/* 每一次play请求来了以后，服务器处理play请求，都将调用ClearOutstandingPackets方法，移去当前的缓存队列所有数据重传包,清空发送窗口数据并调整阻塞窗口大小。 */
void RTPPacketResender::ClearOutstandingPackets()
{   
    //OSMutexLocker packetQLocker(&fPacketQMutex);
    
    for (UInt16 packetIndex = 0; packetIndex < fPacketsInList; packetIndex++)
    {
		/* 移去当前的缓存队列所有数据重传包(在BufferPool放入等长的缓存块),并调整阻塞窗口大小,但不更新其字节数fBytesInList */
        this->RemovePacket(packetIndex,false);// don't move packets delete in place
        Assert(fPacketArray[packetIndex].fPacketSize==0);
    }

    if (fBandwidthTracker != NULL)
		/* 清空窗口数据,使fBytesInList为0，再次调整阻塞窗大侠 */
        fBandwidthTracker->EmptyWindow(fBandwidthTracker->BytesInList()); //clean it out

    fPacketsInList = 0; // deleting in place doesn't decrement，清空重传包队列  
    Assert(fPacketsInList == 0);
}

/* used in RTPStream::ReliableRTPWrite() */
/* 将指定的RTP包加入重传包数组,设置其各成员的值,放入Congestion Window中,更新发送但未得到确认的字节数 */
void RTPPacketResender::AddPacket( void * inRTPPacket, UInt32 packetSize, SInt32 ageLimit )
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);

    // the caller needs to adjust the overall age limit by reducing it by the current packet lateness. 
    // we compute a re-transmit timeout(RTT) based on the Karn's RTT estimate
    
	/* 将RTPPacket指针转换为UInt16类型 */
    UInt16* theSeqNumP = (UInt16*)inRTPPacket;
	/* 取RTPPacket数据中的第3和4字节,得到该包的序列号 */
    UInt16 theSeqNum = ntohs(theSeqNumP[1]);
    
	// ageLimit = 可以容忍的发送时间延时 -（当前时间 - 该包本该发送时间），大于零说明该数据包依然有效，准备发送
    if ( ageLimit > 0 )
    {   
		/* 在重传包数组中找到一个初始化后的EmptyEntry,存放指定的RTP重传包信息,同时将它的包数据存入OSBufferPool,或者是另外创建的special buffer */
        RTPResenderEntry* theEntry = this->GetEmptyEntry(theSeqNum, packetSize);

        // This may happen if this sequence number has already been added.参见RTPPacketResender::GetEmptyEntry()的处理代码
        // That may happen if we have repeat packets in the stream.
		/* 假如该RTP包已在重传包数组中,或是是重复包(未初始化),就无需加入,立即返回 */
        if (theEntry == NULL || theEntry->fPacketSize > 0)
            return;
            
        // Reset all the information in the RTPResenderEntry
		//在队列中设置该包的各个参数，供重传，调整cwnd等操作用
		/************** 重传包结构体赋值  ******************/
        ::memcpy(theEntry->fPacketData, inRTPPacket, packetSize);//将RTP包的实际数据放入OSBufferPool,或是专有缓存
        theEntry->fPacketSize = packetSize;
        theEntry->fAddedTime = OS::Milliseconds();//加入重传包的当前时间戳
		/* 获取该RTP包的RTO */
        theEntry->fOrigRetransTimeout = fBandwidthTracker->CurRetransmitTimeout();
		/* 实际上为当前时间戳+可以容忍的发送时间延时 */
        theEntry->fExpireTime = theEntry->fAddedTime + ageLimit;
		/* 该包的重传次数 */
        theEntry->fNumResends = 0;
		/* 该包的序列号 */
        theEntry->fSeqNum = theSeqNum;
		/************** 重传包结构体赋值  ******************/
        
        // Track the number of wasted bytes we have
		//统计队列中浪费的字节数,为程序设计中设置缓存包大小等参数提供依据,加上该包浪费的字节数
        atomic_add(&sNumWastedBytes, kMaxDataBufferSize - packetSize);
        
        //PLDoubleLinkedListNode<RTPResenderEntry> * listNode = NEW PLDoubleLinkedListNode<RTPResenderEntry>( new RTPResenderEntry(inRTPPacket, packetSize, ageLimit, fRTTEstimator.CurRetransmitTimeout() ) );
        //fAckList.AddNodeToTail(listNode);
		 //更新发送但未得到确认的字节数，该数据与cwnd的大小作比较，用于判断当前网络是否已经满负荷
        fBandwidthTracker->FillWindow(packetSize);
    }
    else //否则该包得丢弃
    {
#if RTP_PACKET_RESENDER_DEBUGGING 
		/* 注意下面更精练的表达式 */
        this->logprintf( "packet too old to add: seq# %li, age limit %li, cur late %li, track id %li\n", (long)ntohs( *((UInt16*)(((char*)inRTPPacket)+2)) ), (long)ageLimit, fCurrentPacketDelay, fTrackID );
#endif
		/* 作废的重传包个数加1 */
        fNumExpired++;
    }

    //总的重传次数加1
    fNumSent++;
}


/* 从客户端收到Ack数据包确认以后,对获得Ack的重传包作如下处理:对已过期不在重传包队列中的重传包,以最大数据包值1466来设置congestion Window(cwd),但是不更新未得到确认的字节数;
对收到Ack的(包括多次重发)已在重传包队列中的重传包,用其大小来调整cwnd的大小，更新未得到确认的字节数,最后从队列中删除该数据包,包括包数据  */
void RTPPacketResender::AckPacket( UInt16 inSeqNum, SInt64& inCurTimeInMsec )
{
    //OSMutexLocker packetQLocker(&fPacketQMutex);
    
	/* 找到的入参SeqNum指定的重传包的数组元素索引 */
    SInt32 foundIndex = -1;

	/* 遍历重传包数组,查找入参SeqNum指定的重传包.找到后就中止循环 */
    for (UInt32 packetIndex = 0 ;packetIndex < fPacketsInList; packetIndex++)
    {   if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
        {   
			foundIndex = packetIndex;
            break;
        }
    }
    
	/* 找出第一个入参SeqNum指定的重传包 */
    RTPResenderEntry* theEntry = NULL;
    if (foundIndex != -1)
        theEntry = &fPacketArray[foundIndex];

    /*  we got an ack for a packet that has already expired or for a packet whose re-transmit crossed with it's original ack  */
	/* 假如收到Ack的重传包过期了,不在当前重传队列中,或者,重传该包后,Ack又回来了,就以1466调整congestion Window大小 */
    if (theEntry == NULL || theEntry->fPacketSize == 0 )
    {    
#if RTP_PACKET_RESENDER_DEBUGGING   
        this->logprintf( "acked packet not found: %li, track id %li, OS::MSecs %li\n" , 
       (long)inSeqNum, fTrackID, (long)OS::Milliseconds() );
#endif
		/* 过期不在重传包队列中,但是收到Ack包的重传包的个数,加1 */
        fNumAcksForMissingPackets++;
        qtss_printf("Ack for missing packet: %d\n", inSeqNum);
         
        // hmm.. we -should not have- closed down the window in this case so reopen it a bit as we normally would.
        // 我们关闭了(本不应该)窗口,此时向通常做的那样,重新打开它
        // ?? who know's what it really was, just use kMaximumSegmentSize
		/* 由于已经找不到这个数据包，假定该包大小就是一个数据包的最大值（1466个字节），根据该值调整cwnd的大小，但是不更新未得到确认的字节数。 */
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
    else /* 假如找到收到应答Ack的重传包 */
    {

#if RTP_PACKET_RESENDER_DEBUGGING
        Assert(inSeqNum == theEntry->fSeqNum);
        this->logprintf( "Ack for packet: %li, track id %li, OS::MSecs %qd\n", (long)inSeqNum, fTrackID, OS::Milliseconds());     
#endif   
		/* 用找到的这个重传包大小来调整cwnd的大小，更新未得到确认的字节数(第二个入参默认为true) */
        fBandwidthTracker->EmptyWindow(theEntry->fPacketSize);

		/* 用首次发送并得到Ack的重传包的RTT作为样本来Estimate当前的RTO,这与Karn Algorithm的思想密切相关!  */
        if ( theEntry->fNumResends == 0 )
        {
            // add RTT sample...        
            // only use rtt from packets acked after their initial send, do not use
            // estimates gatherered from re-trasnmitted packets.
            // fRTTEstimator.AddToEstimate( theEntry->fPacketRTTDuration.DurationInMilliseconds() );
			// 注意入参是收到该Ack的当前时间戳-该重传包发送的时间戳(当加入该重传RTP包时的当前时间戳)
            fBandwidthTracker->AddToRTTEstimate( (SInt32) ( inCurTimeInMsec - theEntry->fAddedTime ) );
            qtss_printf("Got ack for packet %d RTT = %qd\n", inSeqNum, inCurTimeInMsec - theEntry->fAddedTime);
        }
        else /* 假如这是一个多次重传后才收到Ack的包,就打印显示调试信息 */
        {
    #if RTP_PACKET_RESENDER_DEBUGGING
            this->logprintf( "re-tx'd packet acked.  ack num : %li, pack seq #: %li, num resends %li, track id %li, size %li, OS::MSecs %qd\n" \
            , (long)inSeqNum, (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ), (long)theEntry->fNumResends
            , (long)fTrackID, theEntry->fPacketSize, OS::Milliseconds() );
    #endif
        }
		//从队列中删除该数据包,包括包数据
        this->RemovePacket(foundIndex);
    }
}

/* used in RTPStream::ReliableRTPWrite() */
/* 从最后往前,遍历重传包队列,对到了超时发送时间却没有收到Ack确认的重传包,分成两类：
一类是已经超过最后期限，放弃重发;另一类是还未到最后期限，反复循环,继续尝试重发 */
void RTPPacketResender::ResendDueEntries()
{
	/* 假如没有重传包,立即返回 */
    if (fPacketsInList <= 0)
        return;
        
    //OSMutexLocker packetQLocker(&fPacketQMutex);

	/* resend loop count */
    SInt32 numResends = 0;
    RTPResenderEntry* theEntry = NULL; 
    SInt64 curTime = OS::Milliseconds();

    //从数据包队列中最后一个存有数据的元素开始回溯
    for (SInt32 packetIndex = fPacketsInList -1; packetIndex >= 0; packetIndex--) // walk backwards because remove packet moves array members forward
    {
		/* 取得指定索引的重传包 */
        theEntry = &fPacketArray[packetIndex]; 
		/* 假如是个空包,继续找下一个重传包 */
        if (theEntry->fPacketSize == 0) 
            continue;
        
		/* 假如该重传包加入重传包队列的时间距离当前时间戳,超过了当前RTO,把数据包队列中超时没有收到确认的数据分成两类：
		一类是已经超过最后期限，放弃重发;另一类是还未到最后期限，继续尝试多次重发 */
        if ((curTime - theEntry->fAddedTime) > fBandwidthTracker->CurRetransmitTimeout())
        {
            // Change:  Only expire packets after they were due to be resent(他们是因为重发). This gives the client
            // a chance to ack them and improves congestion avoidance and RTT calculation
            // 给了Client一个机会去确认它们,改善了拥塞避免和重传时间RTT的计算
			/* 假如当前包过期了 */
            if (curTime > theEntry->fExpireTime)
            {
				//当且仅当RTP_PACKET_RESENDER_DEBUGGING宏被设定时，这个数量的统计值将被写进调试日志当中
    #if RTP_PACKET_RESENDER_DEBUGGING   
                unsigned char version;
                version = *((char*)theEntry->fPacketData);
                version &= 0x84;    // grab most sig 2 bits
                version = version >> 6; // shift by 6 bits,获得该RTP包的version
                this->logprintf( "expired:  seq number %li, track id %li (port: %li), vers # %li, pack seq # %li, size: %li, OS::Msecs: %qd\n", \
                                    (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ), fTrackID,  (long) ntohs(fDestPort), \
                                    (long)version, (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2))), theEntry->fPacketSize, OS::Milliseconds() );
    #endif
                // This packet is expired
				/* 过期包总数加1 */
                fNumExpired++;
                //qtss_printf("Packet expired: %d\n", ((UInt16*)thePacket)[1]);

				/* 调整cwd窗口大小,并更新当前未得到确认的字节数fBytesInList */
                fBandwidthTracker->EmptyWindow(theEntry->fPacketSize);
				/* 从数据包队列中删除该包,及其包数据 */
                this->RemovePacket(packetIndex);
                qtss_printf("Expired packet %d\n", theEntry->fSeqNum);

				/* 跳到下一个重传包 */
                continue;
            }
            
            // Resend this packet
			/* 假如没过期(没超过当前RTO),用udp Socket重传该包给Client */
            fSocket->SendTo(fDestAddr, fDestPort, theEntry->fPacketData, theEntry->fPacketSize);
            qtss_printf("Packet resent: %d\n", ((UInt16*)theEntry->fPacketData)[1]);

			/* 该包被重发的次数加1 */
            theEntry->fNumResends++;
    #if RTP_PACKET_RESENDER_DEBUGGING   
            this->logprintf( "re-sent: %li RTO %li, track id %li (port %li), size: %li, OS::Ms %qd\n", (long)ntohs( *((UInt16*)(((char*)theEntry->fPacketData)+2)) ),  curTime - theEntry->fAddedTime, \
                    fTrackID, (long) ntohs(fDestPort), theEntry->fPacketSize, OS::Milliseconds());                
    #endif      
       	
            fNumResends++;//重发数据包的总次数加1
            numResends ++;/* resend loop count */
            qtss_printf("resend loop numResends=%ld packet theEntry->fNumResends=%ld stream fNumResends=\n",numResends,theEntry->fNumResends++, fNumResends);
                        
            // ok -- lets try this.. add 1.5x of the INITIAL duration since the last send to the rto estimator
            // since we won't get an ack on this packet this should keep us from exponentially increasing due o a one time increase 
            // in the actuall rtt, only AddToEstimate on the first resend ( assume that it's a dupe欺骗 )
            // if it's not a dupe, but rather an actual loss, the subseqnuent actuals wil bring down the average quickly
            
			/* 假如是首次重传该包,则依Karn算法计算RTT */
            if ( theEntry->fNumResends == 1 )
                fBandwidthTracker->AddToRTTEstimate( (SInt32) ((theEntry->fOrigRetransTimeout  * 3) / 2 ));
            
            qtss_printf("Retransmitted packet %d\n", theEntry->fSeqNum);

			//更新发送时间,以便if循环使用
            theEntry->fAddedTime = curTime;
			//重发数据包意味着超时发生,因此每隔250ms,按照一定的算法,调整ssthresh和cwnd
            fBandwidthTracker->AdjustWindowForRetransmit();
            continue;/* 跳到下一个重传包 */
        }
        
    }
}

void RTPPacketResender::RemovePacket(RTPResenderEntry* inEntry){ Assert(0); }


