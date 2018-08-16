
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSessionInterface.cpp
Description: Provides an API interface for objects to access the attributes 
             related to a RTPSession, also implements the RTP Session Dictionary.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTPStream.h"
#include "QTSServerInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "md5.h"
#include "md5digest.h"
#include "base64.h"



unsigned int            RTPSessionInterface::sRTPSessionIDCounter = 0;


QTSSAttrInfoDict::AttrInfo  RTPSessionInterface::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0  */ { "qtssCliSesStreamObjects",           NULL,   qtssAttrDataTypeQTSS_Object,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1  */ { "qtssCliSesCreateTimeInMsec",        NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 2  */ { "qtssCliSesFirstPlayTimeInMsec",     NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 3  */ { "qtssCliSesPlayTimeInMsec",          NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4  */ { "qtssCliSesAdjustedPlayTimeInMsec",  NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 5  */ { "qtssCliSesRTPBytesSent",            NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 6  */ { "qtssCliSesRTPPacketsSent",          NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 7  */ { "qtssCliSesState",                   NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 8  */ { "qtssCliSesPresentationURL",         NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 9  */ { "qtssCliSesFirstUserAgent",          NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 10 */ { "qtssCliStrMovieDurationInSecs",     NULL,   qtssAttrDataTypeFloat64,        qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 11 */ { "qtssCliStrMovieSizeInBytes",        NULL,   qtssAttrDataTypeUInt64,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 12 */ { "qtssCliSesMovieAverageBitRate",     NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 13 */ { "qtssCliSesLastRTSPSession",         NULL,   qtssAttrDataTypeQTSS_Object,    qtssAttrModeRead | qtssAttrModePreempSafe } ,
    /* 14 */ { "qtssCliSesFullURL",                 NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe } ,
    /* 15 */ { "qtssCliSesHostName",                NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },

    /* 16 */ { "qtssCliRTSPSessRemoteAddrStr",      NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 17 */ { "qtssCliRTSPSessLocalDNS",           NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 18 */ { "qtssCliRTSPSessLocalAddrStr",       NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 19 */ { "qtssCliRTSPSesUserName",            NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 20 */ { "qtssCliRTSPSesUserPassword",        NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 21 */ { "qtssCliRTSPSesURLRealm",            NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 22 */ { "qtssCliRTSPReqRealStatusCode",      NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 23 */ { "qtssCliTeardownReason",             NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 24 */ { "qtssCliSesReqQueryString",          NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 25 */ { "qtssCliRTSPReqRespMsg",             NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    
    /* 26 */ { "qtssCliSesCurrentBitRate",          CurrentBitRate,     qtssAttrDataTypeUInt32,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 27 */ { "qtssCliSesPacketLossPercent",       PacketLossPercent,  qtssAttrDataTypeFloat32, qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 28 */ { "qtssCliSesTimeConnectedinMsec",     TimeConnected,      qtssAttrDataTypeSInt64,  qtssAttrModeRead | qtssAttrModePreempSafe },    
    /* 29 */ { "qtssCliSesCounterID",               NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 30 */ { "qtssCliSesRTSPSessionID",           NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 31 */ { "qtssCliSesFramesSkipped",           NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 32 */ { "qtssCliSesTimeoutMsec", 			NULL, 	qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 33 */ { "qtssCliSesOverBufferEnabled",       NULL, 	qtssAttrDataTypeBool16,		qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 34 */ { "qtssCliSesRTCPPacketsRecv",         NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 35 */ { "qtssCliSesRTCPBytesRecv",           NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 36 */ { "qtssCliSesStartedThinning",         NULL, 	qtssAttrDataTypeBool16,		qtssAttrModeRead | qtssAttrModeWrite  | qtssAttrModePreempSafe }
    
};

/* 设置Client Session Dictionary指定的QTSS_ClientSessionAttributes,see QTSS.h */
void    RTPSessionInterface::Initialize()
{
    for (UInt32 x = 0; x < qtssCliSesNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

RTPSessionInterface::RTPSessionInterface()
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex), NULL),
    Task(),
    fLastQualityCheckTime(0),
	fLastQualityCheckMediaTime(0),
	fStartedThinning(false),    
    fIsFirstPlay(true),/* 默认首次播放 */
    fAllTracksInterleaved(true), // assume true until proven false! 默认使用RTSP TCP channel传输RTP/RTCP data
    fFirstPlayTime(0),
    fPlayTime(0),
    fAdjustedPlayTime(0),/* 设置见RTPSession::Play() */
    fNTPPlayTime(0),
    fNextSendPacketsTime(0),
    fSessionQualityLevel(0),
    fState(qtssPausedState),/* 默认暂停状态 */
    fPlayFlags(0),
    fLastBitRateBytes(0),
    fLastBitRateUpdateTime(0),
    fMovieCurrentBitRate(0),
    fRTSPSession(NULL),
    fLastRTSPReqRealStatusCode(200),/* 默认 200 OK */
    fTimeoutTask(NULL, QTSServerInterface::GetServer()->GetPrefs()->GetRTPTimeoutInSecs() * 1000),/* 使用预设超时时间 */
    fNumQualityLevels(0),
    fBytesSent(0),
    fPacketsSent(0),
    fPacketLossPercent(0.0),
    fTimeConnected(0),
    fTotalRTCPPacketsRecv(0),    
    fTotalRTCPBytesRecv(0),    
    fMovieDuration(0),
    fMovieSizeInBytes(0),
    fMovieAverageBitRate(0),
    fTeardownReason(0),
    fUniqueID(0),
    fTracker(QTSServerInterface::GetServer()->GetPrefs()->IsSlowStartEnabled()),/* 使用预设慢启动 */
	fOverbufferWindow(QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec(),kUInt32_Max, QTSServerInterface::GetServer()->GetPrefs()->GetMaxSendAheadTimeInSecs(),QTSServerInterface::GetServer()->GetPrefs()->GetOverbufferRate()),/* 初始化OverbufferWindow类对象 */
    fAuthScheme(QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme()),/* 使用预设认证格式 */
    fAuthQop(RTSPSessionInterface::kNoQop),
    fAuthNonceCount(0),
    fFramesSkipped(0)
{
    //don't actually setup the fTimeoutTask until the session has been bound!
    //(we don't want to get timeouts before the session gets bound)

    fTimeoutTask.SetTask(this);/* 设置当前RTPSessionInterface为超时任务 */
    fTimeout = QTSServerInterface::GetServer()->GetPrefs()->GetRTPTimeoutInSecs() * 1000;/* 使用预设的超时时间 */
    fUniqueID = (UInt32)atomic_add(&sRTPSessionIDCounter, 1);/* 获取唯一的当前RTPSession ID */
    
    // fQualityUpdate is a counter, the starting value is the unique ID, so every session starts at a different position
    fQualityUpdate = fUniqueID;
    
    //mark the session create time
    fSessionCreateTime = OS::Milliseconds();/* 记录RTPSession生成的时间 */

    // Setup all dictionary attribute values
    
    // Make sure the dictionary knows about our preallocated memory for the RTP stream array
	//注意预先分配的内存
    this->SetEmptyVal(qtssCliSesFirstUserAgent, &fUserAgentBuffer[0], kUserAgentBufSize);
    this->SetEmptyVal(qtssCliSesStreamObjects, &fStreamBuffer[0], kStreamBufSize);
    this->SetEmptyVal(qtssCliSesPresentationURL, &fPresentationURL[0], kPresentationURLSize);
    this->SetEmptyVal(qtssCliSesFullURL, &fFullRequestURL[0], kRequestHostNameBufferSize);
    this->SetEmptyVal(qtssCliSesHostName, &fRequestHostName[0], kFullRequestURLBufferSize);

    this->SetVal(qtssCliSesCreateTimeInMsec,    &fSessionCreateTime, sizeof(fSessionCreateTime));
    this->SetVal(qtssCliSesFirstPlayTimeInMsec, &fFirstPlayTime, sizeof(fFirstPlayTime));
    this->SetVal(qtssCliSesPlayTimeInMsec,      &fPlayTime, sizeof(fPlayTime));
    this->SetVal(qtssCliSesAdjustedPlayTimeInMsec, &fAdjustedPlayTime, sizeof(fAdjustedPlayTime));
    this->SetVal(qtssCliSesRTPBytesSent,        &fBytesSent, sizeof(fBytesSent));
    this->SetVal(qtssCliSesRTPPacketsSent,      &fPacketsSent, sizeof(fPacketsSent));
    this->SetVal(qtssCliSesState,               &fState, sizeof(fState));
    this->SetVal(qtssCliSesMovieDurationInSecs, &fMovieDuration, sizeof(fMovieDuration));
    this->SetVal(qtssCliSesMovieSizeInBytes,    &fMovieSizeInBytes, sizeof(fMovieSizeInBytes));
    this->SetVal(qtssCliSesLastRTSPSession,     &fRTSPSession, sizeof(fRTSPSession));
    this->SetVal(qtssCliSesMovieAverageBitRate, &fMovieAverageBitRate, sizeof(fMovieAverageBitRate));
    this->SetEmptyVal(qtssCliRTSPSessRemoteAddrStr, &fRTSPSessRemoteAddrStr[0], kIPAddrStrBufSize );
    this->SetEmptyVal(qtssCliRTSPSessLocalDNS, &fRTSPSessLocalDNS[0], kLocalDNSBufSize);
    this->SetEmptyVal(qtssCliRTSPSessLocalAddrStr, &fRTSPSessLocalAddrStr[0], kIPAddrStrBufSize);

    this->SetEmptyVal(qtssCliRTSPSesUserName, &fUserNameBuf[0],RTSPSessionInterface::kMaxUserNameLen);
    this->SetEmptyVal(qtssCliRTSPSesUserPassword, &fUserPasswordBuf[0], RTSPSessionInterface::kMaxUserPasswordLen);
    this->SetEmptyVal(qtssCliRTSPSesURLRealm, &fUserRealmBuf[0], RTSPSessionInterface::kMaxUserRealmLen);

    this->SetVal(qtssCliRTSPReqRealStatusCode, &fLastRTSPReqRealStatusCode, sizeof(fLastRTSPReqRealStatusCode));

    this->SetVal(qtssCliTeardownReason, &fTeardownReason, sizeof(fTeardownReason));
 //   this->SetVal(qtssCliSesCurrentBitRate, &fMovieCurrentBitRate, sizeof(fMovieCurrentBitRate));
    this->SetVal(qtssCliSesCounterID, &fUniqueID, sizeof(fUniqueID));
    this->SetEmptyVal(qtssCliSesRTSPSessionID, &fRTSPSessionIDBuf[0], QTSS_MAX_SESSION_ID_LENGTH + 4);
    this->SetVal(qtssCliSesFramesSkipped, &fFramesSkipped, sizeof(fFramesSkipped));
    this->SetVal(qtssCliSesRTCPPacketsRecv, &fTotalRTCPPacketsRecv, sizeof(fTotalRTCPPacketsRecv));
    this->SetVal(qtssCliSesRTCPBytesRecv, &fTotalRTCPBytesRecv, sizeof(fTotalRTCPBytesRecv));

	this->SetVal(qtssCliSesTimeoutMsec,	&fTimeout, sizeof(fTimeout));
	
	this->SetVal(qtssCliSesOverBufferEnabled, this->GetOverbufferWindow()->OverbufferingEnabledPtr(), sizeof(Bool16));
	this->SetVal(qtssCliSesStartedThinning, &fStartedThinning, sizeof(Bool16));
	
}

void RTPSessionInterface::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
	if (inAttrIndex == qtssCliSesTimeoutMsec)
	{
		Assert(inNewValueLen == sizeof(UInt32));
		UInt32 newTimeOut = *((UInt32 *) inNewValue);
		fTimeoutTask.SetTimeout((SInt64) newTimeOut);/* 根据入参设置超时任务的超时值 */
	}
}

/* 使旧的RTSPSession的对象持有计数减1,更新fRTSPSession并增加1个对象持有计数 */
void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession)
{   
    if (inNewRTSPSession != fRTSPSession)
    {
        // If there was an old session, let it know that we are done
        if (fRTSPSession != NULL)
            fRTSPSession->DecrementObjectHolderCount();
        
        // Increment this count to prevent the RTSP session from being deleted
        fRTSPSession = inNewRTSPSession;
        fRTSPSession->IncrementObjectHolderCount();
    }
}

/* 若指定的长度大于SR包的缓存,设置指定大小2倍的SR包的缓存,返回该缓存的起始地址 */
char* RTPSessionInterface::GetSRBuffer(UInt32 inSRLen)
{
    if (fSRBuffer.Len < inSRLen)
    {
        delete [] fSRBuffer.Ptr;
        fSRBuffer.Set(NEW char[2*inSRLen], 2*inSRLen);
    }
    return fSRBuffer.Ptr;
}

/* 从服务器预设值获取RTSP timeout值,若不为0,添加一行"Session:664885455621367225;timeout=20\r\n"到SETUP response中去;
   若为0,添加一行"Session:664885455621367225\r\n".
   从而,最终向RTSPResponseStream放入如下内容:
   (1)当有超时值时:
   RTSP/1.0 200 OK\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623;timeout=20\r\n
   \r\n
   (2)当无超时值时:
   RTSP/1.0 200 OK\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623\r\n
   \r\n
   假如是"RTSP/1.0 304 Not Modified",向RTSPResponseStream中放入指定格式的RTSPHeader信息,并返回错误
   RTSP/1.0 304 Not Modified\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623;timeout=20\r\n
   \r\n
*/
QTSS_Error RTPSessionInterface::DoSessionSetupResponse(RTSPRequestInterface* inRequest)
{
    // This function appends a session header to the SETUP response, and
    // checks to see if it is a 304 Not Modified. If it is, it sends the entire
    // response and returns an error
	/* 从服务器预设值获取RTSP timeout值,若不为0,添加一行"Session:664885455621367225;timeout=20\r\n"到SETUP response中去;
	   若为0,添加一行"Session:664885455621367225\r\n"
	*/
    if ( QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutInSecs() > 0 )  // adv the timeout
        inRequest->AppendSessionHeaderWithTimeout( this->GetValue(qtssCliSesRTSPSessionID), QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutAsString() );
    else
        inRequest->AppendSessionHeaderWithTimeout( this->GetValue(qtssCliSesRTSPSessionID), NULL ); // no timeout in resp.
    
	/* 假如是"RTSP/1.0 304 Not Modified",向RTSPResponseStream中放入指定格式的RTSPHeader信息,并返回错误 */
    if (inRequest->GetStatus() == qtssRedirectNotModified)
    {
        (void)inRequest->SendHeader();
        return QTSS_RequestFailed;
    }
    return QTSS_NoErr;
}

/* 在入参给定时刻,更新movie平均比特率 */
void RTPSessionInterface::UpdateBitRateInternal(const SInt64& curTime)
{   
	/* 假如是暂停状态,仅记录比特率更新时间 */
    if (fState == qtssPausedState)
    {   
		/* 比特率当然是0 */
		fMovieCurrentBitRate = 0;
	     /* 更新当前比特率的时间 */
         fLastBitRateUpdateTime = curTime;
		 /* 当前比特率送出的字节数,和原来一样 */
         fLastBitRateBytes = fBytesSent;
    }
    else
    {
		/* 当前间隔时间段内送出的比特数 */
        UInt32 bitsInInterval = (fBytesSent - fLastBitRateBytes) * 8;
		/* 更新时段(ms) */
        SInt64 updateTime = (curTime - fLastBitRateUpdateTime) / 1000;
		/****************** 计算当前比特率的公式 ***************************************************/
        if (updateTime > 0) // leave Bit Rate the same if updateTime is 0 also don't divide by 0.
            fMovieCurrentBitRate = (UInt32) (bitsInInterval / updateTime);
		/****************** 计算当前比特率的公式 ***************************************************/

		/* 计算Ack包到来的时间(20-100ms内) */
        fTracker.UpdateAckTimeout(bitsInInterval, curTime - fLastBitRateUpdateTime);
		/* ??此时没有字节送出,和原来一样,仅是更新比特率 */
        fLastBitRateBytes = fBytesSent;
        fLastBitRateUpdateTime = curTime;
    }
    qtss_printf("fMovieCurrentBitRate=%lu\n",fMovieCurrentBitRate);
    qtss_printf("Cur bandwidth: %d. Cur ack timeout: %d.\n",fTracker.GetCurrentBandwidthInBps(), fTracker.RecommendedClientAckTimeout());
}

/* 计算该RTPSessionInterface自创建到现在的连接时间,并返回 */
void* RTPSessionInterface::TimeConnected(QTSSDictionary* inSession, UInt32* outLen)
{
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
	/* 计算该RTPSessionInterface自创建到现在的连接时间 */
    theSession->fTimeConnected = (OS::Milliseconds() - theSession->GetSessionCreateTime());

    // Return the result
    *outLen = sizeof(theSession->fTimeConnected);
    return &theSession->fTimeConnected;
}

/* 计算当前时间的movie平均比特率,并返回 */
void* RTPSessionInterface::CurrentBitRate(QTSSDictionary* inSession, UInt32* outLen)
{
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
	/* 计算当前时间的movie平均比特率 */
    theSession->UpdateBitRateInternal(OS::Milliseconds());
    
    // Return the result
    *outLen = sizeof(theSession->fMovieCurrentBitRate);
    return &theSession->fMovieCurrentBitRate;
}

/* 遍历该RTPSession的每个RTPStream,计算一个RTCP Interval内当前的丢包数和总包数,计算并返回丢包百分比 */
void* RTPSessionInterface::PacketLossPercent(QTSSDictionary* inSession, UInt32* outLen)
{   
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
    RTPStream* theStream = NULL;
    UInt32 theLen = sizeof(theStream);
            
    SInt64 packetsLost = 0;
    SInt64 packetsSent = 0;
    
	/* 遍历该RTPSession的每个RTPStream,计算一个RTCP Interval内当前的丢包数和总包数 */
    for (int x = 0; theSession->GetValue(qtssCliSesStreamObjects, x, (void*)&theStream, &theLen) == QTSS_NoErr; x++)
    {       
        if (theStream != NULL  )
        {
            UInt32 streamCurPacketsLost = 0;
            theLen = sizeof(UInt32);
            (void) theStream->GetValue(qtssRTPStrCurPacketsLostInRTCPInterval,0, &streamCurPacketsLost, &theLen);
            qtss_printf("stream = %d streamCurPacketsLost = %lu \n",x, streamCurPacketsLost);
            
            UInt32 streamCurPackets = 0;
            theLen = sizeof(UInt32);
            (void) theStream->GetValue(qtssRTPStrPacketCountInRTCPInterval,0, &streamCurPackets, &theLen);
            qtss_printf("stream = %d streamCurPackets = %lu \n",x, streamCurPackets);
                
            packetsSent += (SInt64)  streamCurPackets;
            packetsLost += (SInt64) streamCurPacketsLost;
            qtss_printf("stream calculated loss = %f \n",x, (Float32) streamCurPacketsLost / (Float32) streamCurPackets);
            
        }

        theStream = NULL;
        theLen = sizeof(UInt32);
    }
    
    //Assert(packetsLost <= packetsSent);
	/* 计算丢包百分比 */
    if (packetsSent > 0)
    {   if  (packetsLost <= packetsSent)
            theSession->fPacketLossPercent =(Float32) (( ((Float32) packetsLost / (Float32) packetsSent) * 100.0) );
        else
            theSession->fPacketLossPercent = 100.0;
    }
    else
        theSession->fPacketLossPercent = 0.0;
    qtss_printf("Session loss percent packetsLost = %qd packetsSent= %qd theSession->fPacketLossPercent=%f\n",packetsLost,packetsSent,theSession->fPacketLossPercent);
    
	// Return the result
    *outLen = sizeof(theSession->fPacketLossPercent);

    return &theSession->fPacketLossPercent;
}

/* 用RTSPsessionid:timestamp生成的认证序列给fAuthNonce赋值 */
void RTPSessionInterface::CreateDigestAuthenticationNonce() 
{

    // Calculate nonce: MD5 of sessionid:timestamp
	/* 得到时间戳字符串 */
    SInt64 curTime = OS::Milliseconds();
    char* curTimeStr = NEW char[128];
    qtss_sprintf(curTimeStr, "%"_64BITARG_"d", curTime);
    
    // Delete old nonce before creating a new one
	/* 删去旧的认证序列 */
    if(fAuthNonce.Ptr != NULL)
        delete [] fAuthNonce.Ptr;
        
    MD5_CTX ctxt;
    unsigned char nonceStr[16];
    unsigned char colon[] = ":";
    MD5_Init(&ctxt);
	/* 得到与其相关的RTSPSession ID */
    StrPtrLen* sesID = this->GetValue(qtssCliSesRTSPSessionID);

	/* 更新ctxt内容为RTSPsessionid:timestamp */
    MD5_Update(&ctxt, (unsigned char *)sesID->Ptr, sesID->Len);
    MD5_Update(&ctxt, (unsigned char *)colon, 1);
    MD5_Update(&ctxt, (unsigned char *)curTimeStr, ::strlen(curTimeStr));

	/* 由ctxt生成认证字符串 */
    MD5_Final(nonceStr, &ctxt);
	/* 用生成的认证赋值给fAuthNonce赋值 */
    HashToString(nonceStr, &fAuthNonce);

    delete [] curTimeStr; // No longer required once nonce is created
        
    // Set the nonce count value to zero 
    // as a new nonce has been created  
    fAuthNonceCount = 0;

}

/* 由入参设置认证格式,假如是Digest认证格式:假如没有新的认证序列,就生成它;假如要生成fAuthOpaque,就由当前时间戳产生的随机数的
字符串及其长度基于64bit编码后得到;否则,删除已存在的fAuthOpaque,使Nonce Count加1  */
void RTPSessionInterface::SetChallengeParams(QTSS_AuthScheme scheme, UInt32 qop, Bool16 newNonce, Bool16 createOpaque)
{   
    // Set challenge params 
    // Set authentication scheme
    fAuthScheme = scheme; /* 由入参设置认证格式 */  
    
	/* 假如是Digest认证格式 */
    if(fAuthScheme == qtssAuthDigest) 
	{
        // Set Quality of Protection 
        // auth-int (Authentication with integrity) not supported yet
        fAuthQop = qop;/* 由入参设置质量保护Qop */
    
		/* 假如没有新的认证序列,就生成它 */
        if(newNonce || (fAuthNonce.Ptr == NULL))
            this->CreateDigestAuthenticationNonce();
    
		/* 假如要生成fAuthOpaque,就由当前时间戳产生的随机数的字符串及其长度基于64bit编码后得到;否则,删除已存在的fAuthOpaque */
        if(createOpaque) 
		{
            // Generate a random UInt32 and convert it to a string 
            // The base64 encoded form of the string is made the opaque value
			/* 由当前时间戳生成随机数 */
            SInt64 theMicroseconds = OS::Microseconds();
            ::srand((unsigned int)theMicroseconds);
            UInt32 randomNum = ::rand();
			/* 获得上面生成随机数的字符串,对其长度64bit编码 */
            char* randomNumStr = NEW char[128];
            qtss_sprintf(randomNumStr, "%lu", randomNum);
            int len = ::strlen(randomNumStr);
            fAuthOpaque.Len = Base64encode_len(len);
            char *opaqueStr = NEW char[fAuthOpaque.Len];
            (void) Base64encode(opaqueStr, randomNumStr,len);
			/* 先删除已存在的fAuthOpaque,将上面生成随机数的字符串64bit编码,设为fAuthOpaque */
            delete [] randomNumStr;                 // Don't need this anymore
            if(fAuthOpaque.Ptr != NULL)             // Delete existing pointer before assigning new one
                delete [] fAuthOpaque.Ptr;              
            fAuthOpaque.Ptr = opaqueStr;
        }
        else 
		{
            if(fAuthOpaque.Ptr != NULL) 
                delete [] fAuthOpaque.Ptr;
            fAuthOpaque.Len = 0;    
        }
        // Increase the Nonce Count by one
        // This number is a count of the next request the server
        // expects with this nonce. (Implies that the server
        // has already received nonce count - 1 requests that 
        // sent authorization with this nonce
		/* 使Nonce Count加1 */
        fAuthNonceCount ++;
    }
}

/* 假如要生成fAuthOpaque,就由当前时间戳产生的随机数的字符串及其长度基于64bit编码后得到;否则,删除已存在的fAuthOpaque */
void RTPSessionInterface::UpdateDigestAuthChallengeParams(Bool16 newNonce, Bool16 createOpaque, UInt32 qop) 
{
	/* 假如没有新的认证序列,就生成它 */
    if(newNonce || (fAuthNonce.Ptr == NULL))
        this->CreateDigestAuthenticationNonce();
    
            
    if(createOpaque) 
	{
        // Generate a random UInt32 and convert it to a string 
        // The base64 encoded form of the string is made the opaque value
        SInt64 theMicroseconds = OS::Microseconds();
        ::srand((unsigned int)theMicroseconds);
        UInt32 randomNum = ::rand();
        char* randomNumStr = NEW char[128];
        qtss_sprintf(randomNumStr, "%lu", randomNum);
        int len = ::strlen(randomNumStr);
        fAuthOpaque.Len = Base64encode_len(len);
        char *opaqueStr = NEW char[fAuthOpaque.Len];
        (void) Base64encode(opaqueStr, randomNumStr,len);
        delete [] randomNumStr;                 // Don't need this anymore
        if(fAuthOpaque.Ptr != NULL)             // Delete existing pointer before assigning new
            delete [] fAuthOpaque.Ptr;              // one
        fAuthOpaque.Ptr = opaqueStr;
        fAuthOpaque.Len = ::strlen(opaqueStr);
    }
    else 
	{
        if(fAuthOpaque.Ptr != NULL) 
            delete [] fAuthOpaque.Ptr;
        fAuthOpaque.Len = 0;    
    }

	/* 使Nonce Count加1 */
    fAuthNonceCount ++;
    /* 使用入参设置fAuthQop */
    fAuthQop = qop;
}
