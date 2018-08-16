/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSession.cpp
Description: Provides a class to manipulate transmission of the media data, 
             also receive and respond to client's feedback.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTPSession.h"
#include "RTSPProtocol.h" 

#include "QTSServerInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "OSMemory.h"
#include <errno.h>

#define RTPSESSION_DEBUGGING 1



RTPSession::RTPSession() :
    RTPSessionInterface(), /* invocation related interface */
    fModule(NULL),/* 默认Module指针为空 */
    fHasAnRTPStream(false),/* 默认没有RTP Stream,设置在RTPSession::AddStream() */
    fCurrentModuleIndex(0),
    fCurrentState(kStart),/* rtp session current state */
    fClosingReason(qtssCliSesCloseClientTeardown),/* 默认是Client自己关闭的,设置另见RTPSession::Run() */
    fCurrentModule(0),
    fModuleDoingAsyncStuff(false),
    fLastBandwidthTrackerStatsUpdate(0)
{
#if DEBUG
    fActivateCalled = false;
#endif

    this->SetTaskName("RTPSession"); /* inherited from Task::SetTaskName() */
	/* set QTSS module state vars,进一步设置另见RTPSession::Run() */
    fModuleState.curModule = NULL;
    fModuleState.curTask = this;
    fModuleState.curRole = 0;
}

RTPSession::~RTPSession()
{
    // Delete all the streams
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
    
	/* 假如预设值能打印RUDP信息 */
    if (QTSServerInterface::GetServer()->GetPrefs()->GetReliableUDPPrintfsEnabled())
    {
        SInt32 theNumLatePacketsDropped = 0;/* 丢弃延迟包个数 */
        SInt32 theNumResends = 0; /* 重传包个数 */
        
		/* 遍历该RTPSession的每个RTPStream,计算丢弃的过时包总数和重传包总数 */
        for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
        {
            Assert(theStream != NULL);
            Assert(theLen == sizeof(RTPStream*));
            if (*theStream != NULL)
            {
				/* 计算丢弃的过时包总数 */
                theNumLatePacketsDropped += (*theStream)->GetStalePacketsDropped();
				/* 得到重传包总数 */
                theNumResends += (*theStream)->GetResender()->GetNumResends();
            }
        }
        
		/* 得到客户端请求播放文件的Full URL */
        char* theURL = NULL;
        (void)this->GetValueAsString(qtssCliSesFullURL, 0, &theURL);
        Assert(theURL != NULL);
        /* 获得RTPBandwidthTracker类 */
        RTPBandwidthTracker* tracker = this->GetBandwidthTracker(); 
    
        qtss_printf("Client complete. URL: %s.\n",theURL);
        qtss_printf("Max congestion window: %ld. Min congestion window: %ld. Avg congestion window: %ld\n", tracker->GetMaxCongestionWindowSize(), tracker->GetMinCongestionWindowSize(), tracker->GetAvgCongestionWindowSize());
        qtss_printf("Max RTT: %ld. Min RTT: %ld. Avg RTT: %ld\n", tracker->GetMaxRTO(), tracker->GetMinRTO(), tracker->GetAvgRTO());
        qtss_printf("Num resends: %ld. Num skipped frames: %ld. Num late packets dropped: %ld\n", theNumResends, this->GetFramesSkipped(), theNumLatePacketsDropped);
        
        delete [] theURL;
    }
    
	/* 遍历该RTPSession的每个RTPStream,逐一删除它们 */
    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
            delete *theStream;
    }
    
	/* 获取服务器接口类,逐一将该RTPSession从qtssSvrClientSessions属性中删去 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    {
        OSMutexLocker theLocker(theServer->GetMutex());
        
        RTPSession** theSession = NULL;
        
        // Remove this session from the qtssSvrClientSessions attribute
        UInt32 y = 0;
        for ( ; y < theServer->GetNumRTPSessions(); y++)
        {
            QTSS_Error theErr = theServer->GetValuePtr(qtssSvrClientSessions, y, (void**)&theSession, &theLen, true);
            Assert(theErr == QTSS_NoErr);
            
            if (*theSession == this)
            {
                theErr = theServer->RemoveValue(qtssSvrClientSessions, y, QTSSDictionary::kDontObeyReadOnly);
                break;
            }
        }

        Assert(y < theServer->GetNumRTPSessions());
        theServer->AlterCurrentRTPSessionCount(-1);
        if (!fIsFirstPlay) // The session was started playing (the counter ignores additional pause-play changes while session is active)
            theServer->AlterRTPPlayingSessions(-1);
        
    }


    //we better not be in the RTPSessionMap anymore!
#if DEBUG
    Assert(!fRTPMapElem.IsInTable());
	StrPtrLen theRTSPSessionID(fRTSPSessionIDBuf,sizeof(fRTSPSessionIDBuf));
	/* 从RTPSession Map中删去它们 */
    OSRef* theRef = QTSServerInterface::GetServer()->GetRTPSessionMap()->Resolve(&theRTSPSessionID);
    Assert(theRef == NULL);
#endif
}

/* 用当前RTSPSession构造一个OSRef实例, 在RTPSession Map(Hash表)中注册并加入该引用OSRef元(其key值唯一),
同时更新qtssSvrClientSessions的属性,及QTSServerInterface中的总RTPSession数 */
QTSS_Error  RTPSession::Activate(const StrPtrLen& inSessionID)
{
    //Set the session ID for this session

	/* 用入参配置fRTSPSessionIDBuf的值(是C-String) */
    Assert(inSessionID.Len <= QTSS_MAX_SESSION_ID_LENGTH);
    ::memcpy(fRTSPSessionIDBuf, inSessionID.Ptr, inSessionID.Len);
    fRTSPSessionIDBuf[inSessionID.Len] = '\0';

	/* 用C-String的fRTSPSessionIDBuf设置qtssCliSesRTSPSessionID的属性值 */
    this->SetVal(qtssCliSesRTSPSessionID, &fRTSPSessionIDBuf[0], inSessionID.Len);
    
	/* 用qtssCliSesRTSPSessionID的属性值来得到一个RTPSession Map的引用OSRef元 */
    fRTPMapElem.Set(*this->GetValue(qtssCliSesRTSPSessionID), this);
    
	/* 获取服务器接口 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    //Activate puts the session into the RTPSession Map
	/* 在RTPSession Map(Hash表)中注册并加入一个引用OSRef,若字符串标识唯一,就能成功返回OS_NoErr;若有一个相同key值的元素,就返回错误EPERM  */
    QTSS_Error err = theServer->GetRTPSessionMap()->Register(&fRTPMapElem);
	//若有同名键值,立即返回
    if (err == EPERM)
        return err;

	/* 确保key值唯一 */
    Assert(err == QTSS_NoErr);
    
    //
    // Adding this session into the qtssSvrClientSessions attr and incrementing the number of sessions must be atomic
	/* 锁定服务器对象 */
    OSMutexLocker locker(theServer->GetMutex()); 

    //
    // Put this session into the qtssSvrClientSessions attribute of the server
#if DEBUG
    Assert(theServer->GetNumValues(qtssSvrClientSessions) == theServer->GetNumRTPSessions());//确保属性qtssSvrClientSessions中的RTPSession个数和服务器中实际的个数相同
#endif
	/* 设置qtssSvrClientSessions的第GetNumRTPSessions()个RTPSession的属性 */
    RTPSession* theSession = this;
    err = theServer->SetValue(qtssSvrClientSessions, theServer->GetNumRTPSessions(), &theSession, sizeof(theSession), QTSSDictionary::kDontObeyReadOnly);
    Assert(err == QTSS_NoErr);
    
#if DEBUG
    fActivateCalled = true;
#endif
	/* 及时更新QTSServerInterface中的总RTPSession数  */
    QTSServerInterface::GetServer()->IncrementTotalRTPSessions();
    return QTSS_NoErr;
}

/* 针对TCP Intervead mode,遍历RTPSession中的每个RTPStream,找到入参指定的RTPStream(其RTP/RTCP channel号就是指定channel号)并返回 */
RTPStream*  RTPSession::FindRTPStreamForChannelNum(UInt8 inChannelNum)
{
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;

    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
            if (((*theStream)->GetRTPChannelNum() == inChannelNum) || ((*theStream)->GetRTCPChannelNum() == inChannelNum))
                return *theStream;
    }
    return NULL; // Couldn't find a matching stream
}

/* used in QTSSCallbacks::QTSS_AddRTPStream() */
/*  循环产生一个能唯一标识该RTPSession的RTPStream数组中新的RTPStream的随机数(作为SSRC),搭建一个新的RTPStream,放入RTPStream的数组 */
QTSS_Error RTPSession::AddStream(RTSPRequestInterface* request, RTPStream** outStream, QTSS_AddStreamFlags inFlags)
{
    Assert(outStream != NULL);

    // Create a new SSRC for this stream. This should just be a random number unique
    // to all the streams in the session
	/* 循环产生一个能唯一标识该RTPSession的RTPStream数组中新的RTPStream的随机数,作为SSRC(它和该数组中现存的每个RTPStream的SSRC都不同) */
    UInt32 theSSRC = 0;
    while (theSSRC == 0)
    {
		/* 产生随机数,它在该RTPSession的RTPStream数组中唯一标识一个RTPSream */
        theSSRC = (SInt32)::rand();

        RTPStream** theStream = NULL;
        UInt32 theLen = 0;
    
		/* 遍历RTPSession中的每个RTPStream,若它的SSRC恰和theSSRC相同,就将theSSRC置0,使得该循环继续 */
        for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
        {
            Assert(theStream != NULL);
            Assert(theLen == sizeof(RTPStream*));
            
            if (*theStream != NULL)
                if ((*theStream)->GetSSRC() == theSSRC)
                    theSSRC = 0;
        }
    }

	/***************** 新建一个RTPStream对象,它有唯一标识它的SSRC(随机数) *************/
    *outStream = NEW RTPStream(theSSRC, this);
	/***************** 新建一个RTPStream对象 *************/
    
	/* 以指定加入方式搭建该RTSP Request的RTPStream */
    QTSS_Error theErr = (*outStream)->Setup(request, inFlags);
	/* 假如搭建不成功,就删去新建RTPStream对象 */
    if (theErr != QTSS_NoErr)
        // If we couldn't setup the stream, make sure not to leak memory!
        delete *outStream;
	/* 假如搭建成功,将其放入RTPStream Array中,注意下面qtssCliSesStreamObjects是个多值索引的属性 */
    else
    {
        // If the stream init succeeded, then put it into the array of setup streams
        theErr = this->SetValue(qtssCliSesStreamObjects, this->GetNumValues(qtssCliSesStreamObjects),
                                                    outStream, sizeof(RTPStream*), QTSSDictionary::kDontObeyReadOnly);
        Assert(theErr == QTSS_NoErr);
        fHasAnRTPStream = true;/* 现在有一个RTPStream */
    }

    return theErr;
}

/* used in RTPSession::Play() */
/* 遍历RTPSession中的每个RTPStream,用入参指定的LateTolerance值设置LateTolerance,并设置胖化瘦化参数 */
void RTPSession::SetStreamThinningParams(Float32 inLateTolerance)
{
	// Set the thinning params in all the RTPStreams of the RTPSession
	// Go through all the streams, setting their thinning params
	RTPStream** theStream = NULL;
	UInt32 theLen = 0;
	
	for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
	{
		Assert(theStream != NULL);
		Assert(theLen == sizeof(RTPStream*));
		if (*theStream != NULL)
		{
			(*theStream)->SetLateTolerance(inLateTolerance);
			(*theStream)->SetThinningParams();
		}
	}
}

/* 设置好各种参数后发信号开始触发RTPSession和发送SR包.播放影片,下面是设置步骤:(1)记录并设置相应的播放时间;(2)设置播放状态,播放标志,更新播放的RTPSession总数;
(3)根据电影的平均比特率设置Client Windows size,重置过缓冲窗;(4)遍历RTPSession中的每个RTPStream,设置打薄参数,重置打薄延迟参数,清除上个Play中重传类的RTP包;(5)假如获得的movie
平均比特率>0,就灵活调整TCP RTSP Socket缓存大小以适应movie比特率,且介于预设的最大值和最小值之间;(6)发信号启动该RTP Session任务; */
QTSS_Error  RTPSession::Play(RTSPRequestInterface* request, QTSS_PlayFlags inFlags)
{
    //first setup the play associated session interface variables
    Assert(request != NULL);
	/* 假如没有Module数组,立即返回 */
    if (fModule == NULL)
        return QTSS_RequestFailed;//Can't play if there are no associated streams
    
    //what time is this play being issued(触发) at? 记录Play触发的时刻
	fLastBitRateUpdateTime = fNextSendPacketsTime = fPlayTime = OS::Milliseconds();
	/* 假如是第一次播放,同步当前播放时间 */
    if (fIsFirstPlay)
		fFirstPlayTime = fPlayTime;
	/* 获取当前播放时间戳和RTSP Request Header Range中的fStartTime的差值 */
    fAdjustedPlayTime = fPlayTime - ((SInt64)(request->GetStartTime() * 1000));
    //for RTCP SRs(RTCP发送方报告), we also need to store the play time in NTP
    fNTPPlayTime = OS::TimeMilli_To_1900Fixed64Secs(fPlayTime);
    
    //we are definitely playing now, so schedule(调度) the object!
    fState = qtssPlayingState;
    fIsFirstPlay = false;/* 现在不是第一次播放 */
    fPlayFlags = inFlags;/* 是生成SR包还是AppendServerInfo进SR包 */

	/* track how many sessions are playing,see QTSServerInterface.h,使fNumRTPPlayingSessions加1 */
    QTSServerInterface::GetServer()-> AlterRTPPlayingSessions(1);
    
	/* set Client window size according to bitrate,,参见RTPBandwidthTracker::SetWindowSize() */
	//根据电影的平均比特率设置Client Windows size
    UInt32 theWindowSize;
	/* 获取电影的平均比特率 */
    UInt32 bitRate = this->GetMovieAvgBitrate();
	/* 假如电影的平均比特率为0或超过1Mbps,就设置Windowsize 为64Kbytes */
	if ((bitRate == 0) || (bitRate > QTSServerInterface::GetServer()->GetPrefs()->GetWindowSizeMaxThreshold() * 1024))
        theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetLargeWindowSizeInK();
	/* 假如电影的平均比特率超过200kbps,就设置Windowsize 为48Kbytes */
	else if (bitRate > QTSServerInterface::GetServer()->GetPrefs()->GetWindowSizeThreshold() * 1024)
		theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetMediumWindowSizeInK();
	/* 否则,,就设置Windowsize 为24Kbytes */
    else
        theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetSmallWindowSizeInK();

    qtss_printf("bitrate = %d, window size = %d\n", bitRate, theWindowSize);

	/* 设置Client窗口大小,参见RTPBandwidthTracker::SetWindowSize() */
    this->GetBandwidthTracker()->SetWindowSize(theWindowSize);

	/* 重置过缓冲窗 */
	this->GetOverbufferWindow()->ResetOverBufferWindow();

    // Go through all the streams, setting their thinning params
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
    
	/* 遍历RTPSession中的每个RTPStream,设置打薄参数,重置打薄延迟参数,清除上个Play中重传类的RTP包 */
    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        if (*theStream != NULL)
        {   /* 设置打薄参数 */
            (*theStream)->SetThinningParams();
 			(*theStream)->ResetThinningDelayParams();
            
            // If we are using reliable UDP, then make sure to clear all the packets from the previous play spurt out of the resender 
            (*theStream)->GetResender()->ClearOutstandingPackets();
        }
    }

    qtss_printf("movie bitrate = %d, window size = %d\n", this->GetMovieAvgBitrate(), theWindowSize);
    Assert(this->GetBandwidthTracker()->BytesInList() == 0);
    
    // Set the size of the RTSPSession's send buffer to an appropriate max size
    // based on the bitrate of the movie. This has 2 benefits:
    // 1) Each socket normally defaults to 32 K. A smaller buffer prevents the
    // system from getting buffer starved if lots of clients get flow-controlled
    //
    // 2) We may need to scale up buffer sizes for high-bandwidth movies in order
    // to maximize thruput, and we may need to scale down buffer sizes for low-bandwidth
    // movies to prevent us from buffering lots of data that the client can't use
    //
    // If we don't know any better, assume maximum buffer size.

	/* 获取服务器预设值来得到最大TCP缓冲区大小 */
    QTSServerPrefs* thePrefs = QTSServerInterface::GetServer()->GetPrefs();
    UInt32 theBufferSize = thePrefs->GetMaxTCPBufferSizeInBytes();
    
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession GetMovieAvgBitrate %li\n",(SInt32)this->GetMovieAvgBitrate() );
#endif

	/* 假如获得的movie平均比特率>0,就灵活调整TCP Socket缓存大小以适应movie比特率,且介于预设的最大值和最小值之间 */
    if (this->GetMovieAvgBitrate() > 0)
    {
        // We have a bit rate... use it.
		/* 由电影比特率和预设的缓存秒数(0.5s),得到实际的缓存大小(字节) */
        Float32 realBufferSize = (Float32)this->GetMovieAvgBitrate() * thePrefs->GetTCPSecondsToBuffer();
        theBufferSize = (UInt32)realBufferSize;
        theBufferSize >>= 3; // Divide by 8 to convert from bits to bytes
        
        // Round down to the next lowest power of 2.
		/* 四舍五入到最接近的小于它的2的幂,该函数定义见下面 */
		/* 将入参变为2的幂的特殊算法:先将入参表示为十六进制,从第29bit位开始从高到底开始走,若遇到值为1的bit位,就返回该bit位值为1,其它31个bit位值全为0的数;若一直没有1，返回0 */
        theBufferSize = this->PowerOf2Floor(theBufferSize);
        
		//下面对缓存大小进行保护,使真实值务必介于预设的最大值和最小值之间

        // This is how much data we should buffer based on the scaling factor... if it is lower than the min, raise to min
        if (theBufferSize < thePrefs->GetMinTCPBufferSizeInBytes())
            theBufferSize = thePrefs->GetMinTCPBufferSizeInBytes();
            
        // Same deal for max buffer size
        if (theBufferSize > thePrefs->GetMaxTCPBufferSizeInBytes())
            theBufferSize = thePrefs->GetMaxTCPBufferSizeInBytes();
    }
    
	/* 设置上面的接收缓存大小为TCP RTSP Socket的缓存大小 */
    Assert(fRTSPSession != NULL); // can this ever happen?
    if (fRTSPSession != NULL)
        fRTSPSession->GetSocket()->SetSocketBufSize(theBufferSize);/* set RTSP buffer size by above value */
           
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession %ld: In Play, about to call Signal\n",(SInt32)this);
#endif

	/* after set some parama, a rtp session task is about to start */
	//发信号启动该RTP Session任务
    this->Signal(Task::kStartEvent);
    
    return QTSS_NoErr;
}

/* used in RTPSession::Play() */
/* 将入参变为2的幂的特殊算法:先将入参表示为十六进制,从第29bit位开始从高到底开始走,若遇到值为1的bit位,就返回该bit位值为1,其它31个bit位值全为0的数;若一直没有1，返回0 */
UInt32 RTPSession::PowerOf2Floor(UInt32 inNumToFloor)
{
    UInt32 retVal = 0x10000000;
    while (retVal > 0)
    { /* 十六位数与如何计算? */
        if (retVal & inNumToFloor) /* 输入数最高位域(4bit-wide)的第四个bit的值必须是1,就把该bit以后的28位都变为0 */
            return retVal;
        else
            retVal >>= 1;/* 否则retVal减半 */
    }
    return retVal;
}

/* 减少该RTSP会话的持有对象计数,设置RTPSession状态为暂停,置空fRTSPSession,再发信号删去该RTPSession */
void RTPSession::Teardown()
{
    // To proffer a quick death of the RTSP session, let's disassociate
    // ourselves with it right now.
    
    // Note that this function relies on the session mutex being grabbed, because
    // this fRTSPSession pointer could otherwise be being used simultaneously by
    // an RTP stream.
    if (fRTSPSession != NULL)
        fRTSPSession->DecrementObjectHolderCount();/* 减少该RTSP会话的持有对象计数 */
    fRTSPSession = NULL;
    fState = qtssPausedState;
	/* 发信号删去该RTPSession */
    this->Signal(Task::kKillEvent);
}

/* 遍历RTPSession中的每个RTPStream,若类型为qtssPlayRespWriteTrackInfo,为每个RTPStream附加RTP时间戳和序列号信息,向Client的RTSPResponseStream中放入标准响应内容如下:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 11\r\n
Session: 7736802604597532330\r\n
Range: npt=0.00000-70.00000\r\n
RTP-Info: url=rtsp://172.16.34.22/sample_300kbit.mp4/trackID=3;seq=15724;rtptime=503370233,url=rtsp://172.16.34.22/sample_300kbit.mp4/trackID=4;seq=5452;rtptime=1925920323\r\n
\r\n
*/
void RTPSession::SendPlayResponse(RTSPRequestInterface* request, UInt32 inFlags)
{
    QTSS_RTSPHeader theHeader = qtssRTPInfoHeader;
    
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
	/* 获得该RTPSession所具有的RTPStream的个数 */
    UInt32 valueCount = this->GetNumValues(qtssCliSesStreamObjects);
	/* 是最后一个RTPStream吗? */
    Bool16 lastValue = false;

	/* 遍历RTPSession中的每个RTPStream,为每个RTPStream附加RTP时间戳和序列号信息 */
    for (UInt32 x = 0; x < valueCount; x++)
    {
		/* 获取指定索引值的RTPStream */
        this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen);
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
        {   
			/* 对最后一个RTPStream,设置lastValue为true,这样会额外附加"\r\n" */
			if (x == (valueCount -1))
                lastValue = true;

			/* 在QTSS_SendStandardRTSPResponse中,若类型为qtssPlayRespWriteTrackInfo,附加RTP时间戳和序列号信息 */
            (*theStream)->AppendRTPInfo(theHeader, request, inFlags,lastValue);
            theHeader = qtssSameAsLastHeader;//这样会附加","而非"RTPInfo"
        }
    }

	/* 向Client的RTSPResponseStream中放入上述指定好的响应内容 */
    request->SendHeader();
}

/* 针对Client发送的DESCRIBE,向RTSPResponseStream中附加如下内容:
Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
Cache-Control: must-revalidate\r\n
Content-length: 1209\r\n
Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Content-Type: application/sdp\r\n
x-Accept-Retransmit: our-retransmit\r\n
x-Accept-Dynamic-Rate: 1\r\n
Content-Base: rtsp://172.16.34.22/sample_300kbit.mp4/\r\n
这样,向Client回送如下内容:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 8\r\n
Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
Cache-Control: must-revalidate\r\n
Content-length: 1209\r\n
Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Content-Type: application/sdp\r\n
x-Accept-Retransmit: our-retransmit\r\n
x-Accept-Dynamic-Rate: 1\r\n
Content-Base: rtsp://172.16.34.22/sample_300kbit.mp4/\r\n
\r\n
*/
void    RTPSession::SendDescribeResponse(RTSPRequestInterface* inRequest)
{
	/* 假如当前RTSP Request的状态是"304 Not Modified",仅需向Client的RTSPResponseStream中放入标准响应内容,返回 */
    if (inRequest->GetStatus() == qtssRedirectNotModified)
    {
        (void)inRequest->SendHeader();
        return;
    }
    
    // write date and expires
	/*
	   向Client的Describe的RTSPResponseStream中放入如下内容:
	   Date: Fri, 02 Jul 2010 05:22:36 GMT\r\n
	   Expires: Fri, 02 Jul 2010 05:22:36 GMT\r\n
	*/
    inRequest->AppendDateAndExpires();
    
    //write content type header
	//附加如下一行:Content-Type: application/sdp\r\n
    static StrPtrLen sContentType("application/sdp");
    inRequest->AppendHeader(qtssContentTypeHeader, &sContentType);
    
    // write x-Accept-Retransmit header
	//附加如下一行:x-Accept-Retransmit: our-retransmit\r\n
    static StrPtrLen sRetransmitProtocolName("our-retransmit");
    inRequest->AppendHeader(qtssXAcceptRetransmitHeader, &sRetransmitProtocolName);
	
	// write x-Accept-Dynamic-Rate header
	//附加如下一行:x-Accept-Dynamic-Rate: 1\r\n
	static StrPtrLen dynamicRateEnabledStr("1");
	inRequest->AppendHeader(qtssXAcceptDynamicRateHeader, &dynamicRateEnabledStr);
    
    //write content base header
    //附加如下一行:Content-Base: rtsp://172.16.34.22/sample_h264_1mbit.mp4/\r\n
    inRequest->AppendContentBaseHeader(inRequest->GetValue(qtssRTSPReqAbsoluteURL));
    
    //I believe the only error that can happen is if the client has disconnected.
    //if that's the case, just ignore it, hopefully the calling module will detect
    //this and return control back to the server ASAP 
	/* 向Client的RTSPResponseStream中放入标准响应内容 */
    (void)inRequest->SendHeader();
}

/* 仅需向Client的RTSPResponseStream中放入标准响应内容:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 8\r\n
\r\n
*/
void    RTPSession::SendAnnounceResponse(RTSPRequestInterface* inRequest)
{
    //
    // Currently, no need to do anything special for an announce response
    (void)inRequest->SendHeader();
}

/* 正常状态下Run()函数的返回值有两种：如果返回值为正数，代表下一次发送数据包的时间，规定时间到来的时候，
   TaskThread线程会自动调用Run函数；如果返回值等于0，在下次任何事件发生时，Run函数就会被调用，这种情况
   往往发生在所有数据都已经发送完成或者该RTPSession对象将要被杀死的时候。而如果返回值为负数，则TaskThread
   线程会delete RTPSession对象
*/
SInt64 RTPSession::Run()
{
#if DEBUG
    Assert(fActivateCalled);
#endif
	
	/* first acquire event flags and which event should be send to task(role) */
    EventFlags events = this->GetEvents();
    QTSS_RoleParams theParams;
	/* 注意QTSS_RoleParams是联合体,每次只取一个.这里ClientSession就是指RTPSession */
    theParams.clientSessionClosingParams.inClientSession = this; //every single role being invoked now has this as the first parameter
                                                       
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession %ld: In Run. Events %ld\n",(SInt32)this, (SInt32)events);
#endif

    // Some callbacks look for this struct in the thread object,设置当前线程的私有数据
    OSThreadDataSetter theSetter(&fModuleState, NULL);

	/****************** CASE: Return -1 ************************************************************************/
    //if we have been instructed to go away(走开), then let's delete ourselves
    if ((events & Task::kKillEvent) || (events & Task::kTimeoutEvent) || (fModuleDoingAsyncStuff))
    {     /* 假如Module是同步(sync)模式,从RTP Session map中注销本RTPSession的表元,若失败,就发信号去Kill该RTPSession;
		  若成功,遍历RTPSession中的每个RTPStream,逐一发送BYE包 */
		  if (!fModuleDoingAsyncStuff)
		  {  
			if (events & Task::kTimeoutEvent)
				fClosingReason = qtssCliSesCloseTimeout;//关闭RTPSession是因为超时
	            
			//deletion is a bit complicated. For one thing, it must happen from within
			//the Run function to ensure that we aren't getting events when we are deleting
			//ourselves. We also need to make sure that we aren't getting RTSP requests
			//(or, more accurately, that the stream object isn't being used by any other
			//threads). We do this by first removing the session from the session map.
	        
	#if RTPSESSION_DEBUGGING
			qtss_printf("RTPSession %ld: about to be killed. Eventmask = %ld\n",(SInt32)this, (SInt32)events);
	#endif
			// We cannot block(阻塞) waiting to UnRegister(注销), because we have to
			// give the RTSPSessionTask a chance to release the RTPSession.
			/* 获取服务器全局的 RTP Session map,并尝试从中注销本RTPSession的表元,若不成功就发信号去Kill该RTPSession */
			OSRefTable* sessionTable = QTSServerInterface::GetServer()->GetRTPSessionMap();
			Assert(sessionTable != NULL);
			if (!sessionTable->TryUnRegister(&fRTPMapElem))
			{
				//Send an event to this task.
				this->Signal(Task::kKillEvent);// So that we get back to this place in the code
				return kCantGetMutexIdleTime; /* 10 */
			}
	        
			// The ClientSessionClosing role is allowed to do async stuff
			fModuleState.curTask = this;

			/* 若从RTP Session map中成功注销了本RTPSession的表元,则Module可作异步,要从代码中返回(参见RTSPSession::Run()),以等待下次继续接受数据 */
			fModuleDoingAsyncStuff = true;  // So that we know to jump back to the right place in the code
			fCurrentModule = 0;              
	    
			// Set the reason parameter in object QTSS_ClientSessionClosing_Params
			theParams.clientSessionClosingParams.inReason = fClosingReason;
	        
			// If RTCP packets are being generated internally for this stream, Send a BYE now.
			RTPStream** theStream = NULL;
			UInt32 theLen = 0;
			/* 遍历RTPSession中的每个RTPStream,逐一发送BYE包  */
			if (this->GetPlayFlags() & qtssPlayFlagsSendRTCP)
			{	
				SInt64 byePacketTime = OS::Milliseconds();
				for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
					if (theStream && *theStream != NULL)
						(*theStream)->SendRTCPSR(byePacketTime, true);//true means send BYE
			}
		  }
        
        //at this point, we know no one is using this session, so invoke the
        //session cleanup role. We don't need to grab the session mutex before
        //invoking modules here, because the session is unregistered(注销) and
        //therefore there's no way another thread could get involved anyway
	    //假如Module是异步模式,现在确信没有Module调用该RTPSession,就调用注册Session cleanup role的Module去关闭它,无须使用互斥锁
        UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kClientSessionClosingRole);
        {
            for (; fCurrentModule < numModules; fCurrentModule++)
            {  
                fModuleState.eventRequested = false;
                fModuleState.idleTime = 0;

                QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kClientSessionClosingRole, fCurrentModule);
                /* 调用模块的Client Session Closing角色，使得模块可以在客户会话关闭时进行必要的处理 */
				(void)theModule->CallDispatch(QTSS_ClientSessionClosing_Role, &theParams);

                // If this module has requested an event, return and wait for the event to transpire(显现,发生)
                if (fModuleState.eventRequested)
                    return fModuleState.idleTime; // If the module has requested idle time...
            }
        }
        
        return -1;//doing this will cause the destructor to get called.
   }
    
	/****************** CASE: Return 0 ************************************************************************/
    //if the RTP stream is currently paused, just return without doing anything.We'll get woken up again when a play is issued
    if ((fState == qtssPausedState) || (fModule == NULL))
        return 0;
     
	/****************** CASE: Return any positive value ************************************************************************/
    //Make sure to grab the session mutex here, to protect the module against RTSP requests coming in while it's sending packets
    {
		/* 锁定该RTPSession并设置QTSS_SendPackets_Params准备send packets */
        OSMutexLocker locker(&fSessionMutex);

        //just make sure we haven't been scheduled before our scheduled play
        //time. If so, reschedule ourselves for the proper time. (if client
        //sends a play while we are already playing, this may occur)
		/* obtain the current time to send RTP packets */
		//设定数据包发送时间，防止被提前发送,刷新QTSS_RTPSendPackets_Params中的当前时间戳
        theParams.rtpSendPacketsParams.inCurrentTime = OS::Milliseconds();
		/* fNextSendPacketsTime see RTPSessionInterface.h,表示send Packets的绝对时间戳 */
		//未到发送时间时处理重传和设置等待发包的时间
        if (fNextSendPacketsTime > theParams.rtpSendPacketsParams.inCurrentTime)
        {
			/* 重传RTPStream的二维数组 */
            RTPStream** retransStream = NULL;
            UInt32 retransStreamLen = 0;

            // Send retransmits if we need to
			/* 先查找该RTPSession的重传流类,为该RTPSession的每个RTPStream设置重传 */
            for (int streamIter = 0; this->GetValuePtr(qtssCliSesStreamObjects, streamIter, (void**)&retransStream, &retransStreamLen) == QTSS_NoErr; streamIter++)
				if (retransStream && *retransStream)
                    (*retransStream)->SendRetransmits(); 
            
			//计算还需多长时间才可运行。
			/*outNextPacketTime是间隔时间，以毫秒为单位。在这个角色返回之前，模块需要设定一个合适
			  的outNextPacketTime值，这个值是当前时刻inCurrentTime和服务器再次为当前会话调用QTSS_RTPSendPackets_Role
			  角色的时刻fNextSendPacketsTime之间的时间间隔。*/
			/*  为重传包设置重传的时间间隔,隔这么多时间后就发送该重传包 */
            theParams.rtpSendPacketsParams.outNextPacketTime = fNextSendPacketsTime - theParams.rtpSendPacketsParams.inCurrentTime;
        }
        else /* retransmit scheduled data normally */
        {   /* 下一个送包时间已过? 马上开始发包了 */        
    #if RTPSESSION_DEBUGGING
            qtss_printf("RTPSession %ld: about to call SendPackets\n",(SInt32)this);
    #endif
			/* fLastBandwidthTrackerStatsUpdate see RTPSession.h,是否我们忘记更新状态了? */
			/* 假如更新间隔超过1000毫秒,立即获取最新的BandWidth */
            if ((theParams.rtpSendPacketsParams.inCurrentTime - fLastBandwidthTrackerStatsUpdate) > 1000)
				/* GetBandwidthTracker() see RTPSessionInterface.h,  UpdateStats() see RTPBandwidthTracker.h */
                this->GetBandwidthTracker()->UpdateStats();
        
			//下次运行时间的缺省值为0,不管怎样,马上将该包发送出去
			/* 将送包时间间隔设置为0,准备发包 */
            theParams.rtpSendPacketsParams.outNextPacketTime = 0;
            // Async event registration is definitely allowed from this role.
			/* 不用接受可以发包的通知了 */
            fModuleState.eventRequested = false;
			
            /* make assure that there is a QTSSModule, here we use QTSSFileModule */
			/* 因为我们马上要调用这个模块发包 */
			/* 试问:它是如何获知QTSSModule是哪个模块的?它的调用在RTSPSession::Run()中的kPreprocessingRequest
			和kProcessingRequest等;设置用SetPacketSendingModule(),得到用GetPacketSendingModule() */
			Assert(fModule != NULL);

			/* 调用QTSSFileModuleDispatch(), refer to QTSSFileModule.cpp */
			/* QTSS_RTPSendPackets_Role角色的责任是向客户端发送媒体数据，并告诉服务器什么时候模块(只能是QTSSFileModule)的QTSS_RTPSendPackets_Role角色应该再次被调用。*/
            (void)fModule->CallDispatch(QTSS_RTPSendPackets_Role, &theParams);
    #if RTPSESSION_DEBUGGING
            qtss_printf("RTPSession %ld: back from sendPackets, nextPacketTime = %"_64BITARG_"d\n",(SInt32)this, theParams.rtpSendPacketsParams.outNextPacketTime);
    #endif

            //make sure not to get deleted accidently!
			/* 送完这个包后再设置下次送包的正确的时间间隔 */
			/* make sure that the returned value is nonnegative, otherwise will be deleted by TaskTheread  */
            if (theParams.rtpSendPacketsParams.outNextPacketTime < 0)
                theParams.rtpSendPacketsParams.outNextPacketTime = 0;
			/* fNextSendPacketsTime see RTPSessionInterface.h */
			/* QTSS_RTPSendPackets_Params中重要的时间关系 */
			/* 紧接着设置下次送包的绝对时间戳 吗 */
            fNextSendPacketsTime = theParams.rtpSendPacketsParams.inCurrentTime + theParams.rtpSendPacketsParams.outNextPacketTime;
        }     
    }
    
    // Make sure the duration between calls to Run() isn't greater than the max retransmit delay interval.发送间隔(50毫秒)<=重传间隔(仅针对RUDP,默认500毫秒)

	/* obtain the preferred max retransmit delay time in Msec, see QTSServerInterface.h  */
    UInt32 theRetransDelayInMsec = QTSServerInterface::GetServer()->GetPrefs()->GetMaxRetransmitDelayInMsec();
	/* obtain the preferred duration between two calls to Run() in Msec, see QTSServerInterface.h  */
    UInt32 theSendInterval = QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec();
    
    // We want to avoid waking up to do retransmits, and then going back to sleep for like, 1 msec. So, 
    // only adjust the time to wake up if the next packet time is greater than the max retransmit delay +
    // the standard interval between wakeups.
	/* adjust the time to wake up  */
	/* in general,theRetransDelayInMsec is bigger than  theSendInterval 必要时缩短下一个包到来的时间间隔 */
    if (theParams.rtpSendPacketsParams.outNextPacketTime > (theRetransDelayInMsec + theSendInterval))
        theParams.rtpSendPacketsParams.outNextPacketTime = theRetransDelayInMsec;
    
    Assert(theParams.rtpSendPacketsParams.outNextPacketTime >= 0);//we'd better not get deleted accidently!
	/* return the next desired runtime  */
    return theParams.rtpSendPacketsParams.outNextPacketTime;
}

