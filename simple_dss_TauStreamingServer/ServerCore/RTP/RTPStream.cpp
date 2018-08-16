
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPStream.cpp
Description: Represents a single client stream (audio, video, etc), contains 
             all stream-specific data & resources & API, used by RTPSession when it
			 wants to send out or receive data for this stream, also implements 
			 the RTP stream dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 




#include <stdlib.h>
#include <errno.h>
#include "SafeStdLib.h"
#include "SocketUtils.h"

#include "QTSSModuleUtils.h"
#include "QTSServerInterface.h"
#include "OS.h"

#include "RTPStream.h"
#include "RTCPPacket.h"
#include "RTCPAPPPacket.h"
#include "RTCPAckPacket.h"
#include "RTCPSRPacket.h"



#if DEBUG
#define RTP_TCP_STREAM_DEBUG 1
#else
#define RTP_TCP_STREAM_DEBUG 0
#endif

//static vars definition

//RTPStream attributes,see also QTSS_RTPStreamAttributes in QTSS.h
QTSSAttrInfoDict::AttrInfo  RTPStream::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0  */ { "qtssRTPStrTrackID",                 NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 1  */ { "qtssRTPStrSSRC",                    NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe }, 
    /* 2  */ { "qtssRTPStrPayloadName",             NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 3  */ { "qtssRTPStrPayloadType",             NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 4  */ { "qtssRTPStrFirstSeqNumber",          NULL,   qtssAttrDataTypeSInt16, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 5  */ { "qtssRTPStrFirstTimestamp",          NULL,   qtssAttrDataTypeSInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 6  */ { "qtssRTPStrTimescale",               NULL,   qtssAttrDataTypeSInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 7  */ { "qtssRTPStrQualityLevel",            NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 8  */ { "qtssRTPStrNumQualityLevels",        NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    /* 9  */ { "qtssRTPStrBufferDelayInSecs",       NULL,   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite   },
    
    /* 10 */ { "qtssRTPStrFractionLostPackets",     NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 11 */ { "qtssRTPStrTotalLostPackets",        NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 12 */ { "qtssRTPStrJitter",                  NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 13 */ { "qtssRTPStrRecvBitRate",             NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 14 */ { "qtssRTPStrAvgLateMilliseconds",     NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 15 */ { "qtssRTPStrPercentPacketsLost",      NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 16 */ { "qtssRTPStrAvgBufDelayInMsec",       NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 17 */ { "qtssRTPStrGettingBetter",           NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 18 */ { "qtssRTPStrGettingWorse",            NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 19 */ { "qtssRTPStrNumEyes",                 NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 20 */ { "qtssRTPStrNumEyesActive",           NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 21 */ { "qtssRTPStrNumEyesPaused",           NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 22 */ { "qtssRTPStrTotPacketsRecv",          NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 23 */ { "qtssRTPStrTotPacketsDropped",       NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 24 */ { "qtssRTPStrTotPacketsLost",          NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 25 */ { "qtssRTPStrClientBufFill",           NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 26 */ { "qtssRTPStrFrameRate",               NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 27 */ { "qtssRTPStrExpFrameRate",            NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 28 */ { "qtssRTPStrAudioDryCount",           NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 29 */ { "qtssRTPStrIsTCP",                   NULL,   qtssAttrDataTypeBool16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 30 */ { "qtssRTPStrStreamRef",               NULL,   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 31 */ { "qtssRTPStrTransportType",           NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 32 */ { "qtssRTPStrStalePacketsDropped",     NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 33 */ { "qtssRTPStrCurrentAckTimeout",       NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 34 */ { "qtssRTPStrCurPacketsLostInRTCPInterval",    NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 35 */ { "qtssRTPStrPacketCountInRTCPInterval",       NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 36 */ { "qtssRTPStrSvrRTPPort",              NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 37 */ { "qtssRTPStrClientRTPPort",           NULL,   qtssAttrDataTypeUInt16, qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 38 */ { "qtssRTPStrNetworkMode",             NULL,   qtssAttrDataTypeUInt32, qtssAttrModeRead | qtssAttrModePreempSafe  }

};

StrPtrLen   RTPStream::sChannelNums[] =
{
    StrPtrLen("0"),
    StrPtrLen("1"),
    StrPtrLen("2"),
    StrPtrLen("3"),
    StrPtrLen("4"),
    StrPtrLen("5"),
    StrPtrLen("6"),
    StrPtrLen("7"),
    StrPtrLen("8"),
    StrPtrLen("9")
};

//protocol TYPE str
char *RTPStream::noType = "no-type";
char *RTPStream::UDP    = "UDP";
char *RTPStream::RUDP   = "RUDP";
char *RTPStream::TCP    = "TCP";

QTSS_ModuleState RTPStream::sRTCPProcessModuleState = { NULL, 0, NULL, false };

//set RTPStream attributes array
void    RTPStream::Initialize()
{
    for (int x = 0; x < qtssRTPStrNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTPStreamDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

RTPStream::RTPStream(UInt32 inSSRC, RTPSessionInterface* inSession)
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTPStreamDictIndex), NULL),
    fLastQualityChange(0),
    fSockets(NULL),
    fSession(inSession),
    fBytesSentThisInterval(0),
    fDisplayCount(0),
    fSawFirstPacket(false),
    fTracker(NULL),
    fRemoteAddr(0),
    fRemoteRTPPort(0),
    fRemoteRTCPPort(0),
    fLocalRTPPort(0),
    fLastSenderReportTime(0),
    fPacketCount(0),
    fLastPacketCount(0),
    fPacketCountInRTCPInterval(0),
    fByteCount(0),

    /* DICTIONARY ATTRIBUTES */
    fTrackID(0),
    fSsrc(inSSRC),
    fSsrcStringPtr(fSsrcString, 0),
    fEnableSSRC(false),
    fPayloadType(qtssUnknownPayloadType),
    fFirstSeqNumber(0),
    fFirstTimeStamp(0),
    fTimescale(0),
    fStreamURLPtr(fStreamURL, 0),
    fQualityLevel(0),
    fNumQualityLevels(0),
    fLastRTPTimestamp(0),
    
	//RTCP data
    fFractionLostPackets(0),
    fTotalLostPackets(0),
    //fPriorTotalLostPackets(0),
    fJitter(0),
    fReceiverBitRate(0),
    fAvgLateMsec(0),
    fPercentPacketsLost(0),
    fAvgBufDelayMsec(0),
    fIsGettingBetter(0),
    fIsGettingWorse(0),
    fNumEyes(0),
    fNumEyesActive(0),
    fNumEyesPaused(0),
    fTotalPacketsRecv(0),
    fTotalPacketsDropped(0),
    fTotalPacketsLost(0),
    fCurPacketsLostInRTCPInterval(0),
    fClientBufferFill(0),
    fFrameRate(0),
    fExpectedFrameRate(0),
    fAudioDryCount(0),
    fClientSSRC(0),
    fIsTCP(false),//不使用TCP传输
    fTransportType(qtssRTPTransportTypeUDP),//默认UDP传输

	//使用TCP传输的控制参数
    fTurnThinningOffDelay_TCP(0),
    fIncreaseThinningDelay_TCP(0),
    fDropAllPacketsForThisStreamDelay_TCP(0),
    fStalePacketsDropped_TCP(0),
    fTimeStreamCaughtUp_TCP(0),
    fLastQualityLevelIncreaseTime_TCP(0),

    fThinAllTheWayDelay(0),
    fAlwaysThinDelay(0),
    fStartThinningDelay(0),
    fStartThickingDelay(0),
    fThickAllTheWayDelay(0),
    fQualityCheckInterval(0),
    fDropAllPacketsForThisStreamDelay(0),
    fStalePacketsDropped(0),
    fLastCurrentPacketDelay(0),
    fWaitOnLevelAdjustment(true),//等待Level control
    fBufferDelay(3.0),//缓冲延迟3s ?
    fLateToleranceInSec(0),//注意这个量十分重要,从RTSP request的RTSP头"x-RTP-Options: late-tolerance=3"得到,默认1.5,参见 RTPStream::Setup()
    fCurrentAckTimeout(0),
    fMaxSendAheadTimeMSec(0),
    fRTPChannel(0),
    fRTCPChannel(0),
    fNetworkMode(qtssRTPNetworkModeDefault),
    fStreamStartTimeOSms(OS::Milliseconds())/* RTPStream开始的时间 */
{
    /* 设置stream对象指针 */
    fStreamRef = this;
#if DEBUG
	//TCP flow control
    fNumPacketsDroppedOnTCPFlowControl = 0;
    fFlowControlStartedMsec = 0;
    fFlowControlDurationMsec = 0;
#endif
    //format the ssrc as a string
    qtss_sprintf(fSsrcString, "%lu", fSsrc);
    fSsrcStringPtr.Len = ::strlen(fSsrcString);
    Assert(fSsrcStringPtr.Len < kMaxSsrcSizeInBytes);

    // SETUP DICTIONARY ATTRIBUTES
    //设置RTPStream Attribute values
    this->SetVal(qtssRTPStrTrackID,             &fTrackID,              sizeof(fTrackID));
    this->SetVal(qtssRTPStrSSRC,                &fSsrc,                 sizeof(fSsrc));
    this->SetEmptyVal(qtssRTPStrPayloadName,    &fPayloadNameBuf,       kDefaultPayloadBufSize);
    this->SetVal(qtssRTPStrPayloadType,         &fPayloadType,          sizeof(fPayloadType));
    this->SetVal(qtssRTPStrFirstSeqNumber,      &fFirstSeqNumber,       sizeof(fFirstSeqNumber));
    this->SetVal(qtssRTPStrFirstTimestamp,      &fFirstTimeStamp,       sizeof(fFirstTimeStamp));
    this->SetVal(qtssRTPStrTimescale,           &fTimescale,            sizeof(fTimescale));
    this->SetVal(qtssRTPStrQualityLevel,        &fQualityLevel,         sizeof(fQualityLevel));
    this->SetVal(qtssRTPStrNumQualityLevels,    &fNumQualityLevels,     sizeof(fNumQualityLevels));
    this->SetVal(qtssRTPStrBufferDelayInSecs,   &fBufferDelay,          sizeof(fBufferDelay));
    this->SetVal(qtssRTPStrFractionLostPackets, &fFractionLostPackets,  sizeof(fFractionLostPackets));
    this->SetVal(qtssRTPStrTotalLostPackets,    &fTotalLostPackets,     sizeof(fTotalLostPackets));
    this->SetVal(qtssRTPStrJitter,              &fJitter,               sizeof(fJitter));
    this->SetVal(qtssRTPStrRecvBitRate,         &fReceiverBitRate,      sizeof(fReceiverBitRate));
    this->SetVal(qtssRTPStrAvgLateMilliseconds, &fAvgLateMsec,          sizeof(fAvgLateMsec));
    this->SetVal(qtssRTPStrPercentPacketsLost,  &fPercentPacketsLost,   sizeof(fPercentPacketsLost));
    this->SetVal(qtssRTPStrAvgBufDelayInMsec,   &fAvgBufDelayMsec,      sizeof(fAvgBufDelayMsec));
    this->SetVal(qtssRTPStrGettingBetter,       &fIsGettingBetter,      sizeof(fIsGettingBetter));
    this->SetVal(qtssRTPStrGettingWorse,        &fIsGettingWorse,       sizeof(fIsGettingWorse));
    this->SetVal(qtssRTPStrNumEyes,             &fNumEyes,              sizeof(fNumEyes));
    this->SetVal(qtssRTPStrNumEyesActive,       &fNumEyesActive,        sizeof(fNumEyesActive));
    this->SetVal(qtssRTPStrNumEyesPaused,       &fNumEyesPaused,        sizeof(fNumEyesPaused));
    this->SetVal(qtssRTPStrTotPacketsRecv,      &fTotalPacketsRecv,     sizeof(fTotalPacketsRecv));
    this->SetVal(qtssRTPStrTotPacketsDropped,   &fTotalPacketsDropped,  sizeof(fTotalPacketsDropped));
    this->SetVal(qtssRTPStrTotPacketsLost,      &fTotalPacketsLost,     sizeof(fTotalPacketsLost));
    this->SetVal(qtssRTPStrClientBufFill,       &fClientBufferFill,     sizeof(fClientBufferFill));
    this->SetVal(qtssRTPStrFrameRate,           &fFrameRate,            sizeof(fFrameRate));
    this->SetVal(qtssRTPStrExpFrameRate,        &fExpectedFrameRate,    sizeof(fExpectedFrameRate));
    this->SetVal(qtssRTPStrAudioDryCount,       &fAudioDryCount,        sizeof(fAudioDryCount));
    this->SetVal(qtssRTPStrIsTCP,               &fIsTCP,                sizeof(fIsTCP));
    this->SetVal(qtssRTPStrStreamRef,           &fStreamRef,            sizeof(fStreamRef));
    this->SetVal(qtssRTPStrTransportType,       &fTransportType,        sizeof(fTransportType));
    this->SetVal(qtssRTPStrStalePacketsDropped, &fStalePacketsDropped,  sizeof(fStalePacketsDropped));
    this->SetVal(qtssRTPStrCurrentAckTimeout,   &fCurrentAckTimeout,    sizeof(fCurrentAckTimeout));
    this->SetVal(qtssRTPStrCurPacketsLostInRTCPInterval ,       &fCurPacketsLostInRTCPInterval ,        sizeof(fPacketCountInRTCPInterval));
    this->SetVal(qtssRTPStrPacketCountInRTCPInterval,       &fPacketCountInRTCPInterval,        sizeof(fPacketCountInRTCPInterval));
    this->SetVal(qtssRTPStrSvrRTPPort,          &fLocalRTPPort,         sizeof(fLocalRTPPort));
    this->SetVal(qtssRTPStrClientRTPPort,       &fRemoteRTPPort,        sizeof(fRemoteRTPPort));
    this->SetVal(qtssRTPStrNetworkMode,         &fNetworkMode,          sizeof(fNetworkMode));
    
    
}

//释放该RTPStream相关的UDPSocketPair上的Task,再从服务器的RTPSocketPool中删除该UDPSocketPair
RTPStream::~RTPStream()
{
    QTSS_Error err = QTSS_NoErr;
    if (fSockets != NULL)
    {
        // If there is an UDP socket pair associated with this stream, make sure to free it up
        Assert(fSockets->GetSocketB()->GetDemuxer() != NULL);
        fSockets->GetSocketB()->GetDemuxer()->UnregisterTask(fRemoteAddr, fRemoteRTCPPort, this);       
        Assert(err == QTSS_NoErr);
    
        QTSServerInterface::GetServer()->GetSocketPool()->ReleaseUDPSocketPair(fSockets);
    }
    
#if RTP_PACKET_RESENDER_DEBUGGING
    //fResender.LogClose(fFlowControlDurationMsec);
    //qtss_printf("Flow control duration msec: %"_64BITARG_"d. Max outstanding packets: %d\n", fFlowControlDurationMsec, fResender.GetMaxPacketsInList());
#endif

#if RTP_TCP_STREAM_DEBUG
    if ( fIsTCP )
        qtss_printf( "DEBUG: ~RTPStream %li sends got EAGAIN'd.\n", (long)fNumPacketsDroppedOnTCPFlowControl );
#endif
}

/* used in RTPStream::Write() */
/* 若是UDP方式,直接返回RTPStream的Quality level,否则返回RTPSessionInterface中的Quality level */
SInt32 RTPStream::GetQualityLevel()
{
    if (fTransportType == qtssRTPTransportTypeUDP)
        return fQualityLevel;
    else
        return fSession->GetQualityLevel();
}

/* 设置RTPStream的Quality level */
void RTPStream::SetQualityLevel(SInt32 level)
{
    if (level <  0 ) // invalid level
    {  
	   Assert(0);
       level = 0;
    }

	//假如服务器预设值不让瘦化,直接设置quality level为最高级别0
    if (QTSServerInterface::GetServer()->GetPrefs()->DisableThinning())
        level = 0;
        
    if (fTransportType == qtssRTPTransportTypeUDP)
        fQualityLevel = level;
    else
        fSession->SetQualityLevel(level);
}

//设置是否关闭OverbufferWindow? 这由stream的传输类型(TCP/RUDP/UDP)和client端的OverBufferState设置共同决定.
void  RTPStream::SetOverBufferState(RTSPRequestInterface* request)
{
	/* 得到Client端的OverBufferState */
    SInt32 requestedOverBufferState = request->GetDynamicRateState();
    Bool16 enableOverBuffer = false;
    
    switch (fTransportType)
    {
        case qtssRTPTransportTypeReliableUDP:
        {
            enableOverBuffer = true; // default is on
            if (requestedOverBufferState == 0) // client specifically set to false
                enableOverBuffer = false;
        }
        break;
        
        case qtssRTPTransportTypeUDP:
        {   
            enableOverBuffer = false; // always off
        }
        break;
        
        
        case qtssRTPTransportTypeTCP:
        {
             //允许tcp因阻塞或慢启动而落后的补偿
             enableOverBuffer = true; // default is on same as 4.0 and earlier. Allows tcp to compensate for falling behind from congestion or slow-start. 
            if (requestedOverBufferState == 0) // client specifically set to false
                enableOverBuffer = false;
        }
        break;
        
    }
    
    //over buffering is enabled for the session by default
    //if any stream turns it off then it is off for all streams
    //a disable is from either the stream type default or a specific rtsp command to disable
    if (!enableOverBuffer)
        fSession->GetOverbufferWindow()->TurnOffOverbuffering();
}

/* used in RTPSession::AddStream() */
/* 依据RTSP request的SETUP命令来搭建一个RTPStream,设置好其相关联的UDPSocketPair */
QTSS_Error RTPStream::Setup(RTSPRequestInterface* request, QTSS_AddStreamFlags inFlags)
{
    //Get the URL for this track
	//得到RTPStream是URL(含文件名),比如"trackID=3"
    fStreamURLPtr.Len = kMaxStreamURLSizeInBytes;
    if (request->GetValue(qtssRTSPReqFileName, 0, fStreamURLPtr.Ptr, &fStreamURLPtr.Len) != QTSS_NoErr)
        return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgFileNameTooLong);
    fStreamURL[fStreamURLPtr.Len] = '\0';//just in case someone wants to use string routines
    
    // Store the late-tolerance value that came out of the x-RTP-Options header,
    // so that when it comes time to determine our thinning params (when we PLAY),
    // we will know this
	/* 从RTSPRequestInterface的RTSP头"x-RTP-Options: late-tolerance=3"得到late-tolerance value */
    fLateToleranceInSec = request->GetLateToleranceInSec(); //-1.0,说明RTSPRequest中没有该字段,是构造函数的初始化值
    if (fLateToleranceInSec == -1.0)
        fLateToleranceInSec = 1.5;

    // Setup the transport type
	/* 得到传输类型(UDP/RUDP/TCP)和网络类型(unicast|multicast) */
    fTransportType = request->GetTransportType();
    fNetworkMode = request->GetNetworkMode();

    // Only allow reliable UDP if it is enabled
	/* 由服务器预设值确定是否用RUDP?若Client请求用RUDP,但服务器预设值禁用,就改用UDP */
    if ((fTransportType == qtssRTPTransportTypeReliableUDP) && (!QTSServerInterface::GetServer()->GetPrefs()->IsReliableUDPEnabled()))
        fTransportType = qtssRTPTransportTypeUDP;

    // 若Client请求用RUDP,但点播的文件路径不在服务器预设值设定的RUDP文件路径中,就改用UDP
    // Check to see if we are inside a valid reliable UDP directory
    if ((fTransportType == qtssRTPTransportTypeReliableUDP) && (!QTSServerInterface::GetServer()->GetPrefs()->IsPathInsideReliableUDPDir(request->GetValue(qtssRTSPReqFilePath))))
        fTransportType = qtssRTPTransportTypeUDP;

    // Check to see if caller is forcing raw UDP transport
	/* 当调用者强制使用UD,但加入方式是强制UDP方式P时,改用UDP */
    if ((fTransportType == qtssRTPTransportTypeReliableUDP) && (inFlags & qtssASFlagsForceUDPTransport))
        fTransportType = qtssRTPTransportTypeUDP;
        
	// decide whether to overbuffer
	/* 由Client的接收缓存大小,确定是否使用overbuffering?若是UDP方式,关闭OverbufferWindow */
	this->SetOverBufferState(request);
        
    // Check to see if this RTP stream should be sent over TCP.
	/* 假如使用TCP传输,设置overbuffering Windows大小,和RTP/RTCP的通道号,并直接返回 */
    if (fTransportType == qtssRTPTransportTypeTCP)
    {
        fIsTCP = true;
        fSession->GetOverbufferWindow()->SetWindowSize(kUInt32_Max);//对TCP方式,设置overbuffer窗无限大
        
        // If it is, get 2 channel numbers from the RTSP session.
		// 为指定ID的RTSPSession查找RTP/RTCP channel number
        fRTPChannel = request->GetSession()->GetTwoChannelNumbers(fSession->GetValue(qtssCliSesRTSPSessionID));
        fRTCPChannel = fRTPChannel+1;
        
        // If we are interleaving, this is all we need to do to setup.
        return QTSS_NoErr;
    }
    
    // This track is not interleaved, so let the session know that all
    // tracks are not interleaved. This affects our scheduling of packets
	/* 设置所有的track不Interleaved,这会影响我们的包分发 */
    fSession->SetAllTracksInterleaved(false);
    
	//设置client ip和RTP/RTCP port
    //Get and store the remote addresses provided by the client. The remote addr is the
    //same as the RTSP client's IP address, unless an alternate was specified in the transport header.
	/* 记录下client提供的remote ip address,它和Client的点播地址相同,除非在Transport头中明确指出 */
    fRemoteAddr = request->GetSession()->GetSocket()->GetRemoteAddr();

	/* 假如目的地IP存在,但是入参inFlags又非qtssASFlagsAllowDestination,则报错,并设置fRemoteAddr为该IP地址 */
    if (request->GetDestAddr() != INADDR_ANY)
    {
        // Sending data to other addresses could be used in malicious(恶意的) ways, therefore
        // it is up to the module as to whether this sort of request might be allowed
        if (!(inFlags & qtssASFlagsAllowDestination))
            return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgAltDestNotAllowed);
        fRemoteAddr = request->GetDestAddr();
    }

	// 获得Client端UDPSocket的RTP/RTCP port
    fRemoteRTPPort = request->GetClientPortA();
    fRemoteRTCPPort = request->GetClientPortB();

	// 假如没有,就报错说Transport头中没有Client Port,类似如下:Transport: RTP/AVP;unicast;source=172.16.34.22;client_port=2642-2643;server_port=6970-6971;ssrc=6F11FF16\r\n
    if ((fRemoteRTPPort == 0) || (fRemoteRTCPPort == 0))
        return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgNoClientPortInTransport);       
    
    //make sure that the client is advertising an even-numbered RTP port,
    //and that the RTCP port is actually one greater than the RTP port
	/* 确保RTP port必须是偶数(最后一个bit不是1) */
    if ((fRemoteRTPPort & 1) != 0)
        return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgRTPPortMustBeEven);     
 
	/* 确保RTCP比RTP端口大1 */
 // comment out check below. This allows the rtcp port to be non-contiguous with the rtp port.
 //   if (fRemoteRTCPPort != (fRemoteRTPPort + 1))
 //       return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgRTCPPortMustBeOneBigger);       
    
    // Find the right source address for this stream. If it isn't specified in the
    // RTSP request, assume it is the same interface as for the RTSP request.
	/* 通过获取RTSP Request的RTSPSessionInterface中的TCPSocket,来设置流服务器的local IP */
    UInt32 sourceAddr = request->GetSession()->GetSocket()->GetLocalAddr();
    if ((request->GetSourceAddr() != INADDR_ANY) && (SocketUtils::IsLocalIPAddr(request->GetSourceAddr())))
        sourceAddr = request->GetSourceAddr();

    // if the transport is TCP or RUDP, then we only want one session quality level instead of a per stream one
	/* 假如是非UDP方式,设置quality level为RTPSession的,而非RTPStream的quality level */
    if (fTransportType != qtssRTPTransportTypeUDP)
    {
      //this->SetVal(qtssRTPStrQualityLevel, fSession->GetQualityLevelPtr(), sizeof(fQualityLevel));
        this->SetQualityLevel(*(fSession->GetQualityLevelPtr()));
    }
    
    
    // If the destination address is multicast, we need to setup multicast socket options
    // on the sockets. Because these options may be different for each stream, we need
    // a dedicated set of sockets
	/* 假如client IP是multicast IP,就要设置复杂的socket set */
    if (SocketUtils::IsMulticastIPAddr(fRemoteAddr))
    {
		// 在RTPSocketPool中新增一个UDPSocketPair,它的目的地是流服务器的侦听IP
        fSockets = QTSServerInterface::GetServer()->GetSocketPool()->CreateUDPSocketPair(sourceAddr, 0);
        
        if (fSockets != NULL)
        {
            //Set options on both sockets. Not really sure why we need to specify an
            //outgoing interface, because these sockets are already bound to an interface!
			// 设置发送RTP数据包的那个UDPSocket的ttl(存活期),进而设置接收RTCP包的那个UDPSocket的ttl,继续设置其多播接口
            QTSS_Error err = fSockets->GetSocketA()->SetTtl(request->GetTtl());
            if (err == QTSS_NoErr)
                err = fSockets->GetSocketB()->SetTtl(request->GetTtl());
            if (err == QTSS_NoErr)
                err = fSockets->GetSocketA()->SetMulticastInterface(fSockets->GetSocketA()->GetLocalAddr());
            if (err == QTSS_NoErr)
                err = fSockets->GetSocketB()->SetMulticastInterface(fSockets->GetSocketB()->GetLocalAddr());
			// 若出错, 向Client发送响应,"不能搭建多播"
            if (err != QTSS_NoErr)
                return QTSSModuleUtils::SendErrorResponse(request, qtssServerInternal, qtssMsgCantSetupMulticast);      
        }
    }
	/* 假如是unicast,就直接获取RTPSocketPool中的UDPSocketPair */
    else
        fSockets = QTSServerInterface::GetServer()->GetSocketPool()->GetUDPSocketPair(sourceAddr, 0, fRemoteAddr, fRemoteRTCPPort); 
                                                                                        

	/* 若出错,报告port用完了 */
    if (fSockets == NULL)
        return QTSSModuleUtils::SendErrorResponse(request, qtssServerInternal, qtssMsgOutOfPorts);      
    /* 若是RUDP传输,确定是否使用慢启动?设置丢包重传 */
    else if (fTransportType == qtssRTPTransportTypeReliableUDP)
    {
        //
        // FIXME - we probably want to get rid of this slow start flag in the API
		/* 是否使用慢启动?除非指明,一般使用 */
        Bool16 useSlowStart = !(inFlags & qtssASFlagsDontUseSlowStart);
		/* 若预设值指明不用慢启动,就不用 */
        if (!QTSServerInterface::GetServer()->GetPrefs()->IsSlowStartEnabled())
            useSlowStart = false;
        
		/* 设置BandwidthTracker对象 */
        fTracker = fSession->GetBandwidthTracker();
        /* 设置RTPPacketResender类指针 */    
        fResender.SetBandwidthTracker( fTracker );
		/* 设置丢包重传RTPPacketResender类的UDP socket,远端ip地址和port */
        fResender.SetDestination( fSockets->GetSocketA(), fRemoteAddr, fRemoteRTPPort );

		/* 记录该RTPStream的PacketResender log */
#if RTP_PACKET_RESENDER_DEBUGGING
        if (QTSServerInterface::GetServer()->GetPrefs()->IsAckLoggingEnabled())
        {
            char        url[256];
            char        logfile[256];
            qtss_sprintf(logfile, "resend_log_%lu", fSession->GetRTSPSession()->GetSessionID());
            StrPtrLen   logName(logfile);
            fResender.SetLog(&logName);
        
            StrPtrLen   *presoURL = fSession->GetValue(qtssCliSesPresentationURL);
            UInt32      clientAddr = request->GetSession()->GetSocket()->GetRemoteAddr();
            memcpy( url, presoURL->Ptr, presoURL->Len );
            url[presoURL->Len] = 0;
            qtss_printf( "RTPStream::Setup for %s will use ACKS, ip addr: %li.%li.%li.%li\n", url, (clientAddr & 0xff000000) >> 24
                                                                                                 , (clientAddr & 0x00ff0000) >> 16
                                                                                                 , (clientAddr & 0x0000ff00) >> 8
                                                                                                 , (clientAddr & 0x000000ff)
                                                                                                  );
        }
#endif
    }
    
    // 记录服务器发送RTP包的UDPSocket的本地Port
    // Record the Server RTP port
    fLocalRTPPort = fSockets->GetSocketA()->GetLocalPort();

    //finally, register with the demuxer to get RTCP packets from the proper address
	// 确保能得到接收RTCP包的UDPSocket的复用器类UDPDemux
    Assert(fSockets->GetSocketB()->GetDemuxer() != NULL);
	/* 用远端client的ip&port在该接收RTCP包的UDPSocket上注册该RTPStream任务,来得到一个哈希表元并放入RTPSocketPool,由RTCPTask轮询处理 */
    QTSS_Error err = fSockets->GetSocketB()->GetDemuxer()->RegisterTask(fRemoteAddr, fRemoteRTCPPort, this);
    //errors should only be returned if there is a routing problem, there should be none
    Assert(err == QTSS_NoErr);
    return QTSS_NoErr;
}

/* 获取timeout,时间缓冲,x-RTP-Options,retransmit header,dynamic rate,添加到SETUP response的头部,最后发送该SETUP Response头! 
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 9\r\n
Last-Modified: Wed, 25 Nov 2009 04:19:01 GMT\r\n
Cache-Control: must-revalidate\r\n
Session: 1900075377083826623\r\n
Date: Fri, 02 Jul 2010 05:22:36 GMT\r\n
Expires: Fri, 02 Jul 2010 05:22:36 GMT\r\n
Transport: RTP/AVP;unicast;source=172.16.34.22;client_port=2756-2757;server_port=6970-6971;ssrc=53D607E8\r\n
x-RTP-Options:\r\n
x-Retransmit: \r\n
x-Dynamic-Rate: \r\n
\r\n
*/
void RTPStream::SendSetupResponse( RTSPRequestInterface* inRequest )
{
	/* 假如是"RTSP/1.0 304 Not Modified",发送给Client后,直接返回 */
    if (fSession->DoSessionSetupResponse(inRequest) != QTSS_NoErr)
        return;
    
	/*  获取当前线程的时间缓冲,附加到Response的Date和Expire头中 */
    inRequest->AppendDateAndExpires();
	/* 附加传输头"Transport: RTP/AVP;unicast;source=172.16.34.22;client_port=2758-2759;server_port=6970-6971;ssrc=6AB9E057\r\n" */
    this->AppendTransport(inRequest);
    
    //
    // Append the x-RTP-Options header if there was a late-tolerance field
	/* 若有x-RTP-Options,就附加到RTSP request */
    if (inRequest->GetLateToleranceStr()->Len > 0)
        inRequest->AppendHeader(qtssXTransportOptionsHeader, inRequest->GetLateToleranceStr());
    
    //
    // Append the retransmit header if the client sent it
	/* 当是RUDP时,获取retransmit header,附加到Response头 */
    StrPtrLen* theRetrHdr = inRequest->GetHeaderDictionary()->GetValue(qtssXRetransmitHeader);
    if ((theRetrHdr->Len > 0) && (fTransportType == qtssRTPTransportTypeReliableUDP))
        inRequest->AppendHeader(qtssXRetransmitHeader, theRetrHdr);

	// Append the dynamic rate header if the client sent it
	/* 若client发送了 dynamic rate,在overbuffering打开时,在Response头中附加1,否则附加0 */
	SInt32 theRequestedRate =inRequest->GetDynamicRateState();
	static StrPtrLen sHeaderOn("1",1);
	static StrPtrLen sHeaderOff("0",1);
	if (theRequestedRate > 0)	// the client sent the header and wants a dynamic rate
	{	
		if(*(fSession->GetOverbufferWindow()->OverbufferingEnabledPtr()))
			inRequest->AppendHeader(qtssXDynamicRateHeader, &sHeaderOn); // send 1 if overbuffering is turned on
		else
			inRequest->AppendHeader(qtssXDynamicRateHeader, &sHeaderOff); // send 0 if overbuffering is turned off
	}
    else if (theRequestedRate == 0) // the client sent the header but doesn't want a dynamic rate
        inRequest->AppendHeader(qtssXDynamicRateHeader, &sHeaderOff);        
    //else the client didn't send a header so do nothing 
	
	/* 最后发送该SETUP Response头! */
    inRequest->SendHeader();
}

/* 依据TCP和非TCP分情形:附加RTP/RTCP channel 或 port,以及同步源,类似"Transport: RTP/AVP;unicast;source=172.16.34.22;client_port=2758-2759;server_port=6970-6971;ssrc=6AB9E057\r\n" */
void RTPStream::AppendTransport(RTSPRequestInterface* request)
{
    /* 获得同步源指针 */
    StrPtrLen* ssrcPtr = NULL;
    if (fEnableSSRC)
        ssrcPtr = &fSsrcStringPtr;

    // We are either going to append the RTP / RTCP port numbers (UDP),
    // or the channel numbers (TCP, interleaved)
	/* 假如是UDP和RUDP方式 */
    if (!fIsTCP)
    {
        //
        // With UDP retransmits its important the client starts sending RTCPs
        // to the right address right away. The sure-firest way to get the client
        // to do this is to put the src address in the transport. So now we do that always.
        //
		/* 从预设值获得源IP配置，否则用Local IP设置 */
        char srcIPAddrBuf[20];
        StrPtrLen theSrcIPAddress(srcIPAddrBuf, 20);
        QTSServerInterface::GetServer()->GetPrefs()->GetTransportSrcAddr(&theSrcIPAddress);
        if (theSrcIPAddress.Len == 0)       
            theSrcIPAddress = *fSockets->GetSocketA()->GetLocalAddrStr();


		/* 假如在SETUP中的transport mode是Record而非Play,获取SetUpServerPort */
        if(request->IsPushRequest())
        {
            char rtpPortStr[10];
            char rtcpPortStr[10];
            qtss_sprintf(rtpPortStr, "%u", request->GetSetUpServerPort());     
            qtss_sprintf(rtcpPortStr, "%u", request->GetSetUpServerPort()+1);
            //qtss_printf(" RTPStream::AppendTransport rtpPort=%u rtcpPort=%u \n",request->GetSetUpServerPort(),request->GetSetUpServerPort()+1);
            StrPtrLen rtpSPL(rtpPortStr);
            StrPtrLen rtcpSPL(rtcpPortStr);
            // Append UDP socket port numbers.
            request->AppendTransportHeader(&rtpSPL, &rtcpSPL, NULL, NULL, &theSrcIPAddress,ssrcPtr);
        }       
        else /*假如是Play transport mode,附加local TCP/RTCP port */
        {
            // Append UDP socket port numbers.
            UDPSocket* theRTPSocket = fSockets->GetSocketA();
            UDPSocket* theRTCPSocket = fSockets->GetSocketB();
            request->AppendTransportHeader(theRTPSocket->GetLocalPortStr(), theRTCPSocket->GetLocalPortStr(), NULL, NULL, &theSrcIPAddress,ssrcPtr);
        }
    }
	/* 假如是TCP方式,且是预生成的0-9个通道号 */
    else if (fRTCPChannel < kNumPrebuiltChNums)
        // We keep a certain number of channel number strings prebuilt, so most of the time
        // we won't have to call qtss_sprintf
        request->AppendTransportHeader(NULL, NULL, &sChannelNums[fRTPChannel],  &sChannelNums[fRTCPChannel],NULL,ssrcPtr);
    else/* 假如是TCP方式,且是预生成的0-9个通道号之外,给出RTP/RTCP通道号 */
    {
        // If these channel numbers fall outside prebuilt range, we will have to call qtss_sprintf.
        char rtpChannelBuf[10];
        char rtcpChannelBuf[10];
        qtss_sprintf(rtpChannelBuf, "%d", fRTPChannel);        
        qtss_sprintf(rtcpChannelBuf, "%d", fRTCPChannel);
        
        StrPtrLen rtpChannel(rtpChannelBuf);
        StrPtrLen rtcpChannel(rtcpChannelBuf);

        request->AppendTransportHeader(NULL, NULL, &rtpChannel, &rtcpChannel,NULL,ssrcPtr);
    }
}

/* used in RTPSession::SendPlayResponse() */
/* 在QTSS_SendStandardRTSPResponse中,若类型为qtssPlayRespWriteTrackInfo,附加时间戳和序列号信息 */
void    RTPStream::AppendRTPInfo(QTSS_RTSPHeader inHeader, RTSPRequestInterface* request, UInt32 inFlags, Bool16 lastInfo)
{
    //format strings for the various numbers we need to send back to the client
    char rtpTimeBuf[20];
    StrPtrLen rtpTimeBufPtr;
    if (inFlags & qtssPlayRespWriteTrackInfo)
    {
        qtss_sprintf(rtpTimeBuf, "%lu", fFirstTimeStamp);
        rtpTimeBufPtr.Set(rtpTimeBuf, ::strlen(rtpTimeBuf));
        Assert(rtpTimeBufPtr.Len < 20);
    }   
    
    char seqNumberBuf[20];
    StrPtrLen seqNumberBufPtr;
    if (inFlags & qtssPlayRespWriteTrackInfo)
    {
        qtss_sprintf(seqNumberBuf, "%u", fFirstSeqNumber);
        seqNumberBufPtr.Set(seqNumberBuf, ::strlen(seqNumberBuf));
        Assert(seqNumberBufPtr.Len < 20);
    }

    StrPtrLen *nullSSRCPtr = NULL; // There is no SSRC in RTP-Info header, it goes in the transport header.
    request->AppendRTPInfoHeader(inHeader, &fStreamURLPtr, &seqNumberBufPtr, nullSSRCPtr, &rtpTimeBufPtr,lastInfo);

}

/*********************************
/
/   InterleavedWrite
/
/   Write the given RTP packet out on the RTSP channel in interleaved format.
/   update quality levels and statistics
/   on success refresh the RTP session timeout to keep it alive
/
*/

//ReliableRTPWrite must be called from a fSession mutex protected caller
/* 在RTSP channel上以interleaved方式写RTP packet,更新quality level,成功后刷新timeout */
QTSS_Error  RTPStream::InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel)
{
    /* 得到RTSPSessionInterface,若为空,直接返回EAGAIN */
    if (fSession->GetRTSPSession() == NULL) // RTSPSession required for interleaved write
    {
        return EAGAIN;
    }

    //char blahblah[2048];
    
    QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite( inBuffer, inLen, outLenWritten, channel);
    //QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite( blahblah, 2044, outLenWritten, channel);
#if DEBUG
    //if (outLenWritten != NULL)
    //{
    //  Assert((*outLenWritten == 0) || (*outLenWritten == 2044));
    //}
#endif
    

#if DEBUG
    if ( err == EAGAIN )
    {
        fNumPacketsDroppedOnTCPFlowControl++;
    }
#endif      

    // reset the timeouts when the connection is still alive
    // when transmitting over HTTP, we're not going to get
    // RTCPs that would normally Refresh the session time.
    if ( err == QTSS_NoErr )
        fSession->RefreshTimeout(); // RTSP session gets refreshed internally in WriteV

    #if RTP_TCP_STREAM_DEBUG
    //qtss_printf( "DEBUG: RTPStream fCurrentPacketDelay %li, fQualityLevel %i\n", (long)fCurrentPacketDelay, (int)fQualityLevel );
    #endif

    return err;
}

//SendRetransmits must be called from a fSession mutex protected caller
/*对RUDP方式,利用PacketResender类发送重传包 */
void    RTPStream::SendRetransmits()
{
    if ( fTransportType == qtssRTPTransportTypeReliableUDP )
        fResender.ResendDueEntries(); //从最后往前,遍历重传包队列,逐一重发包 
}

//ReliableRTPWrite must be called from a fSession mutex protected caller
/* 使用RUDP方式发送RTP包,若使用流控,使用丢包重传逐个发送丢包队列;若不使用流控,将指定包加入队列,用SendTo()发送出去 */
QTSS_Error RTPStream::ReliableRTPWrite(void* inBuffer, UInt32 inLen, const SInt64& curPacketDelay)
{
    QTSS_Error err = QTSS_NoErr;

    // this must ALSO be called in response to a packet timeout event that can be resecheduled as necessary by the fResender.
    // for -hacking- purposes we'l do it just as we write packets, but we won't be able to play low bit-rate movies ( like MIDI )
    // until this is a schedulable task
    
    // Send retransmits for all streams on this session
    RTPStream** retransStream = NULL;
    UInt32 retransStreamLen = 0;

    //
    // Send retransmits if we need to
	/* 遍历所有的RTPStream,丢包重传 */
    for (int streamIter = 0; fSession->GetValuePtr(qtssCliSesStreamObjects, streamIter, (void**)&retransStream, &retransStreamLen) == QTSS_NoErr; streamIter++)
    {
        qtss_printf("Resending packets for stream: %d\n",(*retransStream)->fTrackID);
        qtss_printf("RTPStream::ReliableRTPWrite. Calling ResendDueEntries\n");
        if (retransStream != NULL && *retransStream != NULL)	
            (*retransStream)->fResender.ResendDueEntries();/* 对整个丢包队列,重传 */
    }
    
    if ( !fSawFirstPacket )
    {
        fSawFirstPacket = true;
        fStreamCumDuration = 0;
		
        fStreamCumDuration = OS::Milliseconds() - fSession->GetPlayTime(); /* 累计每个包的播放时间和发送时间戳之差,为stream duration */
        //fInfoDisplayTimer.ResetToDuration( 1000 - fStreamCumDuration % 1000 );
    }
    
#if RTP_PACKET_RESENDER_DEBUGGING
    fResender.SetDebugInfo(fTrackID, fRemoteRTCPPort, curPacketDelay);
    fBytesSentThisInterval = fResender.SpillGuts(fBytesSentThisInterval);
#endif

	/* 假如当前RTPStream中发送但未得到确认的字节数超过阻塞窗的大小时,使用流控,立即返回QTSS_WouldBlock,参见RTPBandwidthTracker.h */
    if ( fResender.IsFlowControlled() )
    {   
        qtss_printf("Flow controlled\n");
#if DEBUG
		/* 更新流控开始时间 */
        if (fFlowControlStartedMsec == 0)
        {
            qtss_printf("Flow control start\n");
            fFlowControlStartedMsec = OS::Milliseconds();//更新流控开始时间
        }
#endif
        err = QTSS_WouldBlock;
    }
    else/* 假如不使用流控 */
    {
#if DEBUG  
		/* 累计流控总时间,并将流控起始时间置0 */
        if (fFlowControlStartedMsec != 0)
        {
            fFlowControlDurationMsec += OS::Milliseconds() - fFlowControlStartedMsec;
            fFlowControlStartedMsec = 0;
        }
#endif
        
        // Assign a lifetime to the packet using the current delay of the packet and
        // the time until this packet becomes stale.
		/* 更新发送的字节数 */
        fBytesSentThisInterval += inLen;

		/* 将指定的RTP包加入重传包数组,设置其各成员的值,放入Congestion Window中,更新发送但未得到确认的字节数 */
        fResender.AddPacket( inBuffer, inLen, (SInt32) (fDropAllPacketsForThisStreamDelay - curPacketDelay) );

		/* 用UDP socket发送出去 */
        (void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, inBuffer, inLen);
    }


    return err;
}

/* 根据服务器的预设值,设置瘦化胖化参数.只在流刚开始被传送前设置,一旦设置之后,不再改动。 */
void RTPStream::SetThinningParams()
{
	/* 记录下瘦化胖化参数的调整差,注意fLateToleranceInSec从RTSP request中得到,默认是1.5s,参见RTPStream::Setup(),注意这个量十分重要!! */
    SInt32 toleranceAdjust = 1500 - (SInt32(fLateToleranceInSec * 1000));//0
    
	/* 获取预设值接口 */
    QTSServerPrefs* thePrefs = QTSServerInterface::GetServer()->GetPrefs();
    
	// fDropAllPacketsForThisStreamDelay表示发送当前包允许的最大延时
    if (fPayloadType == qtssVideoPayloadType)
        fDropAllPacketsForThisStreamDelay = thePrefs->GetDropAllVideoPacketsTimeInMsec() - toleranceAdjust; //1750
    else
        fDropAllPacketsForThisStreamDelay = thePrefs->GetDropAllPacketsTimeInMsec() - toleranceAdjust;//2500

	// fThinAllTheWayDelay表示所有数据流瘦化最大延时，我们可以理解为最高级延时
    fThinAllTheWayDelay = thePrefs->GetThinAllTheWayTimeInMsec() - toleranceAdjust; //1500
	// fAlwaysThinDelay表示单一数据流瘦化最大延时，我们可以理解为第二级延时
    fAlwaysThinDelay = thePrefs->GetAlwaysThinTimeInMsec() - toleranceAdjust; //750
	// fStartThinningDelay表示开始瘦化延时，我们可以理解为第三级延时
    fStartThinningDelay = thePrefs->GetStartThinningTimeInMsec() - toleranceAdjust;//0

	// fStartThickingDelay表示开始胖化延时
    fStartThickingDelay = thePrefs->GetStartThickingTimeInMsec() - toleranceAdjust;//250
	// fThickAllTheWayDelay表示所有数据流胖化最小延时
    fThickAllTheWayDelay = thePrefs->GetThickAllTheWayTimeInMsec();//-2000

	//质量检测周期，默认为1秒
    fQualityCheckInterval = thePrefs->GetQualityCheckIntervalInMsec(); //1s
	//上次质量检测时间
    fSession->fLastQualityCheckTime = 0;
	//上次质量检测时的数据发送的时间
	fSession->fLastQualityCheckMediaTime = 0;
	//是否开始瘦化
	fSession->fStartedThinning = false;
}

/* used in RTPStream::Write() */
/* 对当前传送的包,根据入参和服务器的预设值,依据服务器瘦化算法来判断并设置quality level相关的参数,并决定是否发送该包? true表示发送,false表示丢掉.
注意:第一个入参表示建立会话以来,设定发包的绝对时间戳,第三个入参是发包的当前时间戳,第二个入参是它们的时间差 */
Bool16 RTPStream::UpdateQualityLevel(const SInt64& inTransmitTime, const SInt64& inCurrentPacketDelay,
                                        const SInt64& inCurrentTime, UInt32 inPacketSize)
{
    Assert(fNumQualityLevels > 0);//默认Quality Level为5级
    
	//假如设定发包的绝对时间戳早于该包的播放时间,表示正常,无须更新quality level
    if (inTransmitTime <= fSession->GetPlayTime())
        return true;

	//UDP数据不在这里做质量控制
    if (fTransportType == qtssRTPTransportTypeUDP)
        return true;

	//假如是第一个数据包发送,设置参数,直接返回
	if (fSession->fLastQualityCheckTime == 0)
	{
		// Reset the interval for checking quality levels
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
		fLastCurrentPacketDelay = inCurrentPacketDelay;
		return true;
	}
	
	//如果瘦化操作还没有开始,做以下处理
	if (!fSession->fStartedThinning)
	{
		// if we're still behind but not falling further behind, then don't thin
        //第一个条件说明有延时,第二个条件说明相对上次发包延时250ms内,不用瘦化,直接发送,并直接返回
		if ((inCurrentPacketDelay > fStartThinningDelay) && (inCurrentPacketDelay - fLastCurrentPacketDelay < 250))
		{
			//当前包延时没有上一个包延时大,更新上次发包延时后,可以直接发送
			if (inCurrentPacketDelay < fLastCurrentPacketDelay)
				fLastCurrentPacketDelay = inCurrentPacketDelay;
			return true;
		}
		else
		{	
			//说明当前延时 － 上一次延时 > 250毫秒了,开始瘦化
			fSession->fStartedThinning = true;
		}
	}
	
	//假如没有设置上次质量检查时间,或当前延时已经大过所有数据流允许的最大延时了,需要重置质量检查参数
	if ((fSession->fLastQualityCheckTime == 0) || (inCurrentPacketDelay > fThinAllTheWayDelay))
	{
		// Reset the interval for checking quality levels
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
		fLastCurrentPacketDelay = inCurrentPacketDelay;

		// 假如发包的当前延时已经大于server's thinning algorithm设定的最大延迟极限,直接设为最高quality level(5级)
		if (inCurrentPacketDelay > fThinAllTheWayDelay ) 
        {
            // If we have fallen behind enough such that we risk trasmitting stale packets to the client, AGGRESSIVELY thin the stream
			// fNumQualityLevels在DSS中为5，表示只发送关键帧
            SetQualityLevel(fNumQualityLevels);
//          if (fPayloadType == qtssVideoPayloadType)
//              qtss_printf("Q=%d, delay = %qd\n", GetQualityLevel(), inCurrentPacketDelay);
			//假如发包延时太大了,甚至超过扔掉所有包的级别,当前包要被放弃发送,直接返回
            if (inCurrentPacketDelay > fDropAllPacketsForThisStreamDelay)
            {	
                fStalePacketsDropped++;//丢弃过时包的数量加1
                return false; // We should not send this packet,一定不能发送这个包
            }
        }
    }
    
	//因为在DSS中fNumQualityLevels为5，所以这里可能永远不会被执行
    if (fNumQualityLevels <= 2)
    {
		//假如没有足够质量级别去做精细调整,当发包的当前延时低于胖化延迟底限,且当前Quality Level为正,直接设置当前Quality Level为最高级别0
        if ((inCurrentPacketDelay < fStartThickingDelay) && (GetQualityLevel() > 0))
            SetQualityLevel(0);
            
        return true;        // not enough quality levels to do fine tuning,没有足够质量级别去做精细调整
    }
    
	//假如设定发包的绝对时间戳或发包的当前时间戳,距离上次质量检查的时间差>质量检查间隔,质量检查周期到了
	if (((inCurrentTime - fSession->fLastQualityCheckTime) > fQualityCheckInterval) || 
		((inTransmitTime - fSession->fLastQualityCheckMediaTime) > fQualityCheckInterval))
	{
		//假如发包的当前延时已经大于单个流允许的最大延时,我们要降低播放质量(增大一级QualityLevel)
        if ((inCurrentPacketDelay > fAlwaysThinDelay) && (GetQualityLevel() <  (SInt32) fNumQualityLevels))
            SetQualityLevel(GetQualityLevel() + 1);
		//发包的当前延时大于了最低级别的瘦化延时界限,且较之上次发包,延时程度加重
        else if ((inCurrentPacketDelay > fStartThinningDelay) && (inCurrentPacketDelay > fLastCurrentPacketDelay))
        {
            if (!fWaitOnLevelAdjustment && (GetQualityLevel() < (SInt32) fNumQualityLevels))
            {
                SetQualityLevel(GetQualityLevel() + 1); //增大一级QualityLevel
                fWaitOnLevelAdjustment = true; //设置要级别调整
            }
            else
                fWaitOnLevelAdjustment = false; //假如已到最大QualityLevel,设置不要级别调整
        }
        
		//假如发包的当前延时低于胖化延时界限,且较之上次发包,延时程度减轻,我们要增加播放质量(减小一级QualityLevel)
        if ((inCurrentPacketDelay < fStartThickingDelay) && (GetQualityLevel() > 0) && (inCurrentPacketDelay < fLastCurrentPacketDelay))
        {
            SetQualityLevel(GetQualityLevel() - 1);
            fWaitOnLevelAdjustment = true; //设置要级别调整
        }
          
		//当前延时已经低于所有数据流的最小胖化界限，我们把播放质量调到最大
        if (inCurrentPacketDelay < fThickAllTheWayDelay)
        {
            SetQualityLevel(0);
            fWaitOnLevelAdjustment = false; //已到最高质量级别,设置不要级别调整
        }

//		if (fPayloadType == qtssVideoPayloadType)
//			printf("Q=%d, delay = %qd\n", GetQualityLevel(), inCurrentPacketDelay);

		//更新本次质量检查参数
		fLastCurrentPacketDelay = inCurrentPacketDelay;
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
    }

    return true; // We should send this packet
}

/* 获取数据包,按照QTSS_Write的写方式,分情形讨论,按照相应的传输方式(TCP/RUDP/UDP)发送RTP包或SR包给Client */
QTSS_Error  RTPStream::Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags)
{
	/* 尝试获取互斥锁,不能获得,直接返回EAGAIN */
    Assert(fSession != NULL);
    if (!fSession->GetSessionMutex()->TryLock())
        return EAGAIN;

    QTSS_Error err = QTSS_NoErr;
	/* 获取当前系统时间,对计算包的延迟时间,制定送包策略非常重要! */
    SInt64 theTime = OS::Milliseconds();
    
    // Data passed into this version of write must be a QTSS_PacketStruct
	/* 传递给这个版本的Write的数据必定是QTSS_PacketStruct结构体,获取当前RTPStream中的包结构 */
    QTSS_PacketStruct* thePacket = (QTSS_PacketStruct*)inBuffer;
    thePacket->suggestedWakeupTime = -1;
	/* 计算该包的delay延误时间=当前时间-送包的绝对时间,可正可负 */
    SInt64 theCurrentPacketDelay = theTime - thePacket->packetTransmitTime;
    
#if RTP_PACKET_RESENDER_DEBUGGING
	/* 获取该RTP包的seqnumber */
    UInt16* theSeqNum = (UInt16*)thePacket->packetData;
#endif

    // Empty the overbuffer window
	/* 没有代码定义,见RTPOverbufferWindow::EmptyOutWindow() */
    fSession->GetOverbufferWindow()->EmptyOutWindow(theTime);

    // Update the bit rate value
	/* 在入参给定时刻,更新movie平均比特率 */
    fSession->UpdateCurrentBitRate(theTime);
    
    // Is this the first write in a write burst?
    if (inFlags & qtssWriteFlagsWriteBurstBegin)
		/* 获取overbuffering Window,标记写迸发,从而调用overbuffering Window来监控比特率是否超标?以决定是否启用overbuffering 超前发送机制 */
        fSession->GetOverbufferWindow()->MarkBeginningOfWriteBurst();
    
	/* 假如写RTCP数据包的SR包 */
    if (inFlags & qtssWriteFlagsIsRTCP)
    {   
		// Check to see if this packet is ready to send
		/* 假如不能使用overbuffering,检查到该RTCP包会在当前时间之后发送,则直接返回 */
		if (false == *(fSession->GetOverbufferWindow()->OverbufferingEnabledPtr())) // only force rtcps on time if overbuffering is off
		{
			/* 检查该RTP包是否要提前发送?提前多少发送? */
            thePacket->suggestedWakeupTime = fSession->GetOverbufferWindow()->CheckTransmitTime(thePacket->packetTransmitTime, theTime, inLen);
            /* 假如该RCTP包等待下次发送,直接返回 */
			if (thePacket->suggestedWakeupTime > theTime)
            {
                Assert(thePacket->suggestedWakeupTime >= fSession->GetOverbufferWindow()->GetSendInterval());			
                fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
                return QTSS_WouldBlock;
            }
        }

		/* 使用TCP的interleave write */
        if ( fTransportType == qtssRTPTransportTypeTCP )// write out in interleave format on the RTSP TCP channel
        {
            err = this->InterleavedWrite( thePacket->packetData, inLen, outLenWritten, fRTCPChannel );
        }
        else if ( inLen > 0 )
        {
			/* 使用UDPsocket::SendTo()直接发送 */
            (void)fSockets->GetSocketB()->SendTo(fRemoteAddr, fRemoteRTCPPort, thePacket->packetData, inLen);
        }
        
        if (err == QTSS_NoErr)
			/* 成功发送出该SR包,就在屏幕上打印rtcpSR包 */
            PrintPacketPrefEnabled( (char*) thePacket->packetData, inLen, (SInt32) RTPStream::rtcpSR);
    }
	/* 假如写RTP数据包,成功发送出后,还要发送一个SR包 */
    else if (inFlags & qtssWriteFlagsIsRTP)
    {
        // Check to see if this packet fits in the overbuffer window
		/* 检查该RTP包是否要提前发送?提前多少发送?-1表示要立即发送 */
        thePacket->suggestedWakeupTime = fSession->GetOverbufferWindow()->CheckTransmitTime(thePacket->packetTransmitTime, theTime, inLen);
         /* 假如该RTP包等待下次发送,直接返回 */
		if (thePacket->suggestedWakeupTime > theTime)
        {
            Assert(thePacket->suggestedWakeupTime >= fSession->GetOverbufferWindow()->GetSendInterval());//务必确保该包至少在一个发包间隔以后Wake up
#if RTP_PACKET_RESENDER_DEBUGGING
            fResender.logprintf("Overbuffer window full. Num bytes in overbuffer: %d. Wakeup time: %qd\n",fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), thePacket->packetTransmitTime);
#endif
            qtss_printf("Overbuffer window full. Returning: %qd\n", thePacket->suggestedWakeupTime - theTime);//在等待多少毫秒后返回
            
            fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
            return QTSS_WouldBlock;
        }

        // Check to make sure our quality level is correct. This function also tells us whether this packet is just too old to send
		/* 根据当前包的参数,依据服务器瘦化算法和服务器预设值,来判断是否发送该包,并更新quality level相应参数,发送返回true,丢弃返回false */
        if (this->UpdateQualityLevel(thePacket->packetTransmitTime, theCurrentPacketDelay, theTime, inLen))
        {
            if ( fTransportType == qtssRTPTransportTypeTCP )    // write out in interleave format on the RTSP TCP channel
                err = this->InterleavedWrite( thePacket->packetData, inLen, outLenWritten, fRTPChannel );       
            else if ( fTransportType == qtssRTPTransportTypeReliableUDP )//用RUDP写
                err = this->ReliableRTPWrite( thePacket->packetData, inLen, theCurrentPacketDelay );
            else if ( inLen > 0 )//使用UDPSocket::SendTo()写
                (void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, thePacket->packetData, inLen);
            
            if (err == QTSS_NoErr)
				/* 若成功发送,就打印rtp包 */
                PrintPacketPrefEnabled( (char*) thePacket->packetData, inLen, (SInt32) RTPStream::rtp);
        
            if (err == 0)
            {
                static SInt64 time = -1;
                static int byteCount = 0;
                static SInt64 startTime = -1;
                static int totalBytes = 0;
                static int numPackets = 0;
                static SInt64 firstTime;
                
                if (theTime - time > 1000)
                {
                    if (time != -1)
                    {
//                      qtss_printf("   %qd KBit (%d in %qd secs)", byteCount * 8 * 1000 / (theTime - time) / 1024, totalBytes, (theTime - startTime) / 1000);
//                      if (fTracker)
//                          qtss_printf(" Window = %d\n", fTracker->CongestionWindow());
//                      else
//                          qtss_printf("\n");
//                      qtss_printf("Packet #%d xmit time = %qd\n", numPackets, (thePacket->packetTransmitTime - firstTime) / 1000);
                    }
                    else
                    {
                        startTime = theTime;
                        firstTime = thePacket->packetTransmitTime;
                    }
                    
                    byteCount = 0;
                    time = theTime;
                }

				// update statics
                byteCount += inLen;
                totalBytes += inLen;
                numPackets++;
                
//              UInt16* theSeqNumP = (UInt16*)thePacket->packetData;
//              UInt16 theSeqNum = ntohs(theSeqNumP[1]);
//				qtss_printf("Packet %d for time %qd sent at %qd (%d bytes)\n", theSeqNum, thePacket->packetTransmitTime - fSession->GetPlayTime(), theTime - fSession->GetPlayTime(), inLen);
            }
        }   
            
#if RTP_PACKET_RESENDER_DEBUGGING
        if (err != QTSS_NoErr)
            fResender.logprintf("Flow controlled: %qd Overbuffer window: %d. Cur time %qd\n", theCurrentPacketDelay, fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), theTime);
        else
            fResender.logprintf("Sent packet: %d. Overbuffer window: %d Transmit time %qd. Cur time %qd\n", ntohs(theSeqNum[1]), fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), thePacket->packetTransmitTime, theTime);
#endif
        //if (err != QTSS_NoErr)
        //  qtss_printf("flow controlled\n");
        if ( err == QTSS_NoErr && inLen > 0 )
        {
            // Update statistics if we were actually able to send the data (don't
            // update if the socket is flow controlled or some such thing)    
            fSession->GetOverbufferWindow()->AddPacketToWindow(inLen); //对当前送出的RTP包,用当前包大小更新RTPOverbufferWindow类的相关量
            fSession->UpdatePacketsSent(1);   //更新该RTPSession送出的总包数
            fSession->UpdateBytesSent(inLen); //更新该RTPSession送出的总字节数
            QTSServerInterface::GetServer()->IncrementTotalRTPBytes(inLen); //累计服务器送出的RTP字节总数
            QTSServerInterface::GetServer()->IncrementTotalPackets();       //累计服务器送出的RTP包总数
            QTSServerInterface::GetServer()->IncrementTotalLate(theCurrentPacketDelay); //累计总延迟
            QTSServerInterface::GetServer()->IncrementTotalQuality(this->GetQualityLevel());

            // Record the RTP timestamp for RTCPs
			/* 记录已经发送出的上个RTP packet的timestamp */
            UInt32* timeStampP = (UInt32*)(thePacket->packetData);
            fLastRTPTimestamp = ntohl(timeStampP[1]);
            
            //stream statistics
            fPacketCount++;
            fByteCount += inLen;

            // Send an RTCP sender report if it's time. Again, we only want to send an RTCP if the RTP packet was sent sucessfully 
			/* 假如QTSS_PlayFlags是RTCPSR且已到发送时间,就发送rtcpSR包 */
            if ((fSession->GetPlayFlags() & qtssPlayFlagsSendRTCP) && (theTime > (fLastSenderReportTime + (kSenderReportIntervalInSecs * 1000))))    
            {
				/* 更新上次rtcpSR包时间 */
                fLastSenderReportTime = theTime;
                // CISCO comments
                // thePacket->packetTransmissionTime is the expected transmission time, which is what we should report in RTCP for
                // synchronization purposes, not theTime, which is the actual transmission time.
				/* 发送rtcpSR包 */
                this->SendRTCPSR(thePacket->packetTransmitTime);
            }        
        }
    } //(inFlags & qtssWriteFlagsIsRTP)
    else //若是其他类型,直接返回错误
    {   fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
        return QTSS_BadArgument;//qtssWriteFlagsIsRTCP or qtssWriteFlagsIsRTP wasn't specified
    }
    
	/* 更新输出参数为实际发送的数据包长度 */
    if (outLenWritten != NULL)
        *outLenWritten = inLen;
    
	/* 释放互斥锁 */
    fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
    return err;
}

/***************** 注意以下需要对RTCP packet有仔细的研究!请先搞清楚RFC3550 ***************************/

// SendRTCPSR is called by the session as well as the strem
// SendRTCPSR must be called from a fSession mutex protected caller
/* 从RTPSessionInterface中获得结构体RTCPSRPacket,配置各项域,通过RTSP TCP channel或UDP方式发送该SR包  */
void RTPStream::SendRTCPSR(const SInt64& inTime, Bool16 inAppendBye)
{
        //
        // This will roll over, after which payloadByteCount will be all messed up.
        // But because it is a 32 bit number, that is bound to happen eventually,
        // and we are limited by the RTCP packet format in that respect, so this is
        // pretty much ok.
	/* 计算从开始传输到此SR包产生时该发送者发送的payload字节总数,每个RTP包的包头12个字节去掉 */
    UInt32 payloadByteCount = fByteCount - (12 * fPacketCount);
    
	/* 从RTPSessionInterface中获得结构体RTCPSRPacket,它需要我们配置 */
    RTCPSRPacket* theSR = fSession->GetSRPacket();

	/* 配置该RTCPSRPacket,注意设置方法 */
    theSR->SetSSRC(fSsrc);
    theSR->SetClientSSRC(fClientSSRC);
    theSR->SetNTPTimestamp(fSession->GetNTPPlayTime() + OS::TimeMilli_To_Fixed64Secs(inTime - fSession->GetPlayTime()));                        
    theSR->SetRTPTimestamp(fLastRTPTimestamp);
	/* 设置从开始传输到此SR包产生时该发送者发送的总包数和payload字节总数 */
    theSR->SetPacketCount(fPacketCount);
    theSR->SetByteCount(payloadByteCount);
#if RTP_PACKET_RESENDER_DEBUGGING
    fResender.logprintf("Recommending ack timeout of: %d\n",fSession->GetBandwidthTracker()->RecommendedClientAckTimeout());
#endif
    theSR->SetAckTimeout(fSession->GetBandwidthTracker()->RecommendedClientAckTimeout());
    
	/* 得到SR包长度 */
    UInt32 thePacketLen = theSR->GetSRPacketLen();
	/* 假如还有附加Bye包,计算总长度 */
    if (inAppendBye)
        thePacketLen = theSR->GetSRWithByePacketLen();
     
	/* 通过RTSP TCP channel或UDP方式发送该SR包 */
    QTSS_Error err = QTSS_NoErr;
    if ( fTransportType == qtssRTPTransportTypeTCP )    // write out in interleave format on the RTSP TCP channel
    {
       UInt32  wasWritten; 
       err = this->InterleavedWrite( theSR->GetSRPacket(), thePacketLen, &wasWritten, fRTCPChannel );
    }
    else
    {
       err = fSockets->GetSocketB()->SendTo(fRemoteAddr, fRemoteRTCPPort, theSR->GetSRPacket(), thePacketLen);
    }
    
	// 假如通过tcp或udp或RUDP方式发送成功,就打印出该SR包的内容
    if (err == QTSS_NoErr)
        PrintPacketPrefEnabled((char *) theSR->GetSRPacket(), thePacketLen, (SInt32) RTPStream::rtcpSR); // if we are flow controlled, this packet is not sent
}

/* 对TCP方式传输RTP/RTCP包的情形,还是使用RTPStream::ProcessIncomingRTCPPacket()处理RTCP包 */
void RTPStream::ProcessIncomingInterleavedData(UInt8 inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket)
{
    if (inChannelNum == fRTPChannel)
    {
        //
        // Currently we don't do anything with incoming RTP packets. Eventually,
        // we might need to make a role to deal with these
    }
    else if (inChannelNum == fRTCPChannel)
        this->ProcessIncomingRTCPPacket(inPacket);
}


/* used in RTCPTask::Run() */
/* 尝试成功获取RTPSessionInterface的互斥锁,刷新RTPSessionInterface和RTSPSessionInterface的timeout,逐个解析和处理client发送过来的
compound RTCP 包,设置rtcpProcessParams,逐个调用注册QTSSModule::kRTCPProcessRole的模块,解锁 */
void RTPStream::ProcessIncomingRTCPPacket(StrPtrLen* inPacket)
{
    StrPtrLen currentPtr(*inPacket);
    SInt64 curTime = OS::Milliseconds(); /* 获取当前时间 */

    // Modules are guarenteed atomic access to the session. Also, the RTSP Session accessed
    // below could go away at any time. So we need to lock the RTP session mutex.
    // *BUT*, when this function is called, the caller already has the UDP socket pool &
    // UDP Demuxer mutexes. Blocking on grabbing this mutex could cause a deadlock.
    // So, dump this RTCP packet if we can't get the mutex.
	/* 尝试获取RTPSessionInterface的互斥锁,失败立即返回 */
    if (!fSession->GetSessionMutex()->TryLock())
        return;
    
    //no matter what happens (whether or not this is a valid packet) reset the timeouts
	/* 我们一收到RTCP包,不管其是否合法,立即刷新RTPSessionInterface和RTSPSessionInterface的timeout */
    fSession->RefreshTimeout();
	/* 假如RTSPSessionInterface存在,刷新超时 */
    if (fSession->GetRTSPSession() != NULL)
        fSession->GetRTSPSession()->RefreshTimeout();
     
	/* 逐个解析一个compound RTCP包的单个RTCP包,要解析两次,第一次解析通常RTCP包的合法性并得出具体类型,第二次解析具体RTCP包的合法性 */
    while ( currentPtr.Len > 0 )
    {
        /*
            Due to the variable-type nature of RTCP packets, this is a bit unusual...
            We initially treat the packet as a generic RTCPPacket in order to determine its'
            actual packet type.  Once that is figgered out, we treat it as its' actual packet type
        */
        RTCPPacket rtcpPacket;
		/* 假如RTCP包格式不对,就解锁并立即返回 */
        if (!rtcpPacket.ParsePacket((UInt8*)currentPtr.Ptr, currentPtr.Len))
        {  
			fSession->GetSessionMutex()->Unlock();
            return;//abort if we discover a malformed RTCP packet
        }

        // Increment our RTCP Packet and byte counters for the session.      
		/* 对合法的RTCP包,增加包总数和字节总数 */
        fSession->IncrTotalRTCPPacketsRecv();
        fSession->IncrTotalRTCPBytesRecv( (SInt16) currentPtr.Len);

		/* 对RTCP包分情形讨论:RR/APP/Ack/SDES */
        switch (rtcpPacket.GetPacketType())
        {
			/************************************ 对RR包 ************************************/
            case RTCPPacket::kReceiverPacketType:
            {
                RTCPReceiverPacket receiverPacket;
				/* 假如RR包格式不对,就解锁并立即返回 */
                if (!receiverPacket.ParseReceiverReport((UInt8*)currentPtr.Ptr, currentPtr.Len))
                {   fSession->GetSessionMutex()->Unlock();
                    return;//abort if we discover a malformed receiver report
                }

                this->PrintPacketPrefEnabled(currentPtr.Ptr,  currentPtr.Len, RTPStream::rtcpRR);
        
                // Set the Client SSRC based on latest RTCP
				/* 基于最新的RTCP包更新Client SSRC */
                fClientSSRC = rtcpPacket.GetPacketSSRC();

				/* 解析出RR包中的下面两个域fraction lost\interarrival jitter */
                fFractionLostPackets = receiverPacket.GetCumulativeFractionLostPackets();
                fJitter = receiverPacket.GetCumulativeJitter();
                
				/* 解析出RR包中的cumulative number of packets lost,获取此时间间隔的丢包数 */
                UInt32 curTotalLostPackets = receiverPacket.GetCumulativeTotalLostPackets();
                
                // Workaround for client problem.  Sometimes it appears to report a bogus lost packet count.
                // Since we can't have lost more packets than we sent, ignore the packet if that seems to be the case
				/* 假如丢包数不超过发送的RTP包(事实就是这样) */
                if (curTotalLostPackets - fTotalLostPackets <= fPacketCount - fLastPacketCount)
                {
                    // if current value is less than the old value, that means that the packets are out of order
                    //  just wait for another packet that arrives in the right order later and for now, do nothing
					/* 当前丢的RTP包更多了 */
                    if (curTotalLostPackets > fTotalLostPackets)
                    {   
                        //increment the server total by the new delta
						/* 记录下丢包数 */
                        QTSServerInterface::GetServer()->IncrementTotalRTPPacketsLost(curTotalLostPackets - fTotalLostPackets);
                        /* 在一个RTCP间隔内的当前丢包数 */
						fCurPacketsLostInRTCPInterval = curTotalLostPackets - fTotalLostPackets;
                        qtss_printf("fCurPacketsLostInRTCPInterval = %d\n", fCurPacketsLostInRTCPInterval);				
                        fTotalLostPackets = curTotalLostPackets; /* 及时更新 */
                    }
                    else if(curTotalLostPackets == fTotalLostPackets)
                    {
						/* 数据发送平稳,丢包变化不大 */
                        fCurPacketsLostInRTCPInterval = 0;
                        qtss_printf("fCurPacketsLostInRTCPInterval set to 0\n");
                    }
                    
                    /* 在此RTCP时间间隔内发送的RTP包总数 */                
                    fPacketCountInRTCPInterval = fPacketCount - fLastPacketCount;			
                    fLastPacketCount = fPacketCount;/* 及时更新 */
                }

#ifdef DEBUG_RTCP_PACKETS
				/* 打印出该RTCP包 */
                receiverPacket.Dump();
#endif
            }
            break;
            
			/************************************************** 对APP包(ACK/RTCPCompressedQTSSPacket) *******************************************/
            case RTCPPacket::kAPPPacketType:
            {   
                //
                // Check and see if this is an Ack packet. If it is, update the UDP Resender
                RTCPAckPacket theAckPacket;
                UInt8* packetBuffer = rtcpPacket.GetPacketBuffer();
				/* 获取Ack包的长度 */
                UInt32 packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;
                
                /* 假如该Ack包合法 */
                if (theAckPacket.ParseAckPacket(packetBuffer, packetLen))
                {
			   // this stream must be ready to receive acks.  Between RTSP setup and sending of first packet on stream we must protect against a bad ack.
                    if (NULL != fTracker && false == fTracker->ReadyForAckProcessing())
                    {   
						/* 当RTPBandwidthTracker没有准备好接收Ack包时,它来了,就解锁返回 */
						fSession->GetSessionMutex()->Unlock();
                        return;//abort if we receive an ack when we haven't sent anything.
                    }
                    
					//打印出该Ack包
                    this->PrintPacketPrefEnabled( (char*)packetBuffer,  packetLen, RTPStream::rtcpACK);

                    // Only check for ack packets if we are using Reliable UDP
					/* 仅对RUDP传输方式,检查ACK包 */
                    if (fTransportType == qtssRTPTransportTypeReliableUDP)
                    {
						/* 获得序列号 */
                        UInt16 theSeqNum = theAckPacket.GetAckSeqNum();
						/* 依据Ack包,处理重传包队列 */
                        fResender.AckPacket(theSeqNum, curTime);
                        qtss_printf("Got ack: %d\n",theSeqNum);
                        
						/* 分析Ack包的Mask位,遍历32bit,得到是否收到其它指定的序列号的Ack包,进而处理其对应的RTP包,是否重发或丢弃? */
                        for (UInt16 maskCount = 0; maskCount < theAckPacket.GetAckMaskSizeInBits(); maskCount++)
                        {
                            if (theAckPacket.IsNthBitEnabled(maskCount))
                            {
                                fResender.AckPacket( theSeqNum + maskCount + 1, curTime);
                                qtss_printf("Got ack in mask: %d\n",theSeqNum + maskCount + 1);
                            }
                        }             
                    }
                }
                else //它不是APP包,假设它是the qtss APP packet
                {   
                   this->PrintPacketPrefEnabled( (char*) packetBuffer, packetLen, RTPStream::rtcpAPP);
                   
                    // If it isn't an ACK, assume its the qtss APP packet
                    RTCPCompressedQTSSPacket compressedQTSSPacket;
					/* 假如the qtss APP packet包格式不对,就解锁并立即返回 */
                    if (!compressedQTSSPacket.ParseCompressedQTSSPacket((UInt8*)currentPtr.Ptr, currentPtr.Len))
                    {  
						fSession->GetSessionMutex()->Unlock();
                        return;//abort if we discover a malformed app packet
                    }
                    
                    fReceiverBitRate =      compressedQTSSPacket.GetReceiverBitRate();
                    fAvgLateMsec =          compressedQTSSPacket.GetAverageLateMilliseconds();
                    
                    fPercentPacketsLost =   compressedQTSSPacket.GetPercentPacketsLost();
                    fAvgBufDelayMsec =      compressedQTSSPacket.GetAverageBufferDelayMilliseconds();
                    fIsGettingBetter = (UInt16)compressedQTSSPacket.GetIsGettingBetter();
                    fIsGettingWorse = (UInt16)compressedQTSSPacket.GetIsGettingWorse();
                    fNumEyes =              compressedQTSSPacket.GetNumEyes();
                    fNumEyesActive =        compressedQTSSPacket.GetNumEyesActive();
                    fNumEyesPaused =        compressedQTSSPacket.GetNumEyesPaused();
                    fTotalPacketsRecv =     compressedQTSSPacket.GetTotalPacketReceived();
                    fTotalPacketsDropped =  compressedQTSSPacket.GetTotalPacketsDropped();
                    fTotalPacketsLost =     compressedQTSSPacket.GetTotalPacketsLost();
                    fClientBufferFill =     compressedQTSSPacket.GetClientBufferFill();
                    fFrameRate =            compressedQTSSPacket.GetFrameRate();
                    fExpectedFrameRate =    compressedQTSSPacket.GetExpectedFrameRate();
                    fAudioDryCount =        compressedQTSSPacket.GetAudioDryCount();
                    
//                  if (fPercentPacketsLost == 0)
//                  {
//                      qtss_printf("***\n");
//                      fCurPacketsLostInRTCPInterval = 0;
//                  }
                    
                    // Update our overbuffer window size to match what the client is telling us
					/* 对UDP传输方式,依据客户端告诉的值,设置OverbufferWindow大小 */
                    if (fTransportType != qtssRTPTransportTypeUDP)
                    {
                        qtss_printf("Setting over buffer to %d\n", compressedQTSSPacket.GetOverbufferWindowSize());
                        fSession->GetOverbufferWindow()->SetWindowSize(compressedQTSSPacket.GetOverbufferWindowSize());
                    }
                    
#ifdef DEBUG_RTCP_PACKETS
                compressedQTSSPacket.Dump();//先打印出RTCP包头,再打印出存贮在mDumpArray中的所有数据
#endif
                }
            }
            break;
            
			/********************* 对SDES包 **************************************/
            case RTCPPacket::kSDESPacketType:
            {
#ifdef DEBUG_RTCP_PACKETS
                SourceDescriptionPacket sedsPacket;
				/* 假如SDES包格式不对,就解锁并立即返回 */
                if (!sedsPacket.ParsePacket((UInt8*)currentPtr.Ptr, currentPtr.Len))
                {   
					fSession->GetSessionMutex()->Unlock();
                    return;//abort if we discover a malformed app packet
                }

                sedsPacket.Dump();
#endif
            }
            break;
            
            default:
               WarnV(false, "Unknown RTCP Packet Type");
            break;
        
        } //switch
        
        /* 将指针移到RTCP包的结尾 */
        currentPtr.Ptr += (rtcpPacket.GetPacketLength() * 4 ) + 4;
        currentPtr.Len -= (rtcpPacket.GetPacketLength() * 4 ) + 4;
    } //  while

    // Invoke the RTCP modules, allowing them to process this packet
	/* 设置rtcpProcessParams,让流控模块处理 */
    QTSS_RoleParams theParams;
    theParams.rtcpProcessParams.inRTPStream = this;
    theParams.rtcpProcessParams.inClientSession = fSession;
    theParams.rtcpProcessParams.inRTCPPacketData = inPacket->Ptr;
    theParams.rtcpProcessParams.inRTCPPacketDataLen = inPacket->Len;
    
    // We don't allow async events from this role, so just set an empty module state.
	/* 将Module状态设置为线程私有数据 */
    OSThreadDataSetter theSetter(&sRTCPProcessModuleState, NULL);
    
    // Invoke RTCP processing modules
	/* 逐个调用注册QTSSModule::kRTCPProcessRole的模块,现在只有QTSSFlowControlModule */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTCPProcessRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kRTCPProcessRole, x)->CallDispatch(QTSS_RTCPProcess_Role, &theParams);

	/* 解锁 */
    fSession->GetSessionMutex()->Unlock();
}

/* 依据fTransportType,返回字符串"UDP",或"RUDP",或"TCP",或"no-type" */
char* RTPStream::GetStreamTypeStr()
{
    char *streamType = NULL;
     
    switch (fTransportType)
     {
        case qtssRTPTransportTypeUDP:   
			streamType = RTPStream::UDP;//"UDP"
            break;
        
        case qtssRTPTransportTypeReliableUDP: 
			streamType = RTPStream::RUDP;//"RUDP"
            break;
        
        case qtssRTPTransportTypeTCP: 
			streamType = RTPStream::TCP;//"TCP"
           break;
        
        default: 
           streamType = RTPStream::noType;//"no-type"
     };
  
    return streamType;
}

/* 打印指定的RTP信息 */
void RTPStream::PrintRTP(char* packetBuff, UInt32 inLen)
{
    /* 获取RTP包的seqnum\timestamp\ssrc */
    UInt16 sequence = ntohs( ((UInt16*)packetBuff)[1]);
    UInt32 timestamp = ntohl( ((UInt32*)packetBuff)[1]);
    UInt32 ssrc = ntohl( ((UInt32*)packetBuff)[2]);
    
     
       
    if (fFirstTimeStamp == 0)
        fFirstTimeStamp = timestamp;
        
    Float32 rtpTimeInSecs = 0.0; 
    if (fTimescale > 0 && fFirstTimeStamp < timestamp)
		/* ?? */
        rtpTimeInSecs = (Float32) (timestamp - fFirstTimeStamp) /  (Float32) fTimescale;
      
    /* 获取RTPStrPayloadName */ 
    StrPtrLen   *payloadStr = this->GetValue(qtssRTPStrPayloadName);
	/* 打印该PayLoad name */
    if (payloadStr && payloadStr->Len > 0)
        payloadStr->PrintStr();
    else
        qtss_printf("?");

    
     qtss_printf(" H_ssrc=%ld H_seq=%u H_ts=%lu seq_count=%lu ts_secs=%.3f \n", ssrc, sequence, timestamp, fPacketCount +1, rtpTimeInSecs );

}

/* 打印出指定的RTCPSR包 */
void RTPStream::PrintRTCPSenderReport(char* packetBuff, UInt32 inLen)
{

    char timebuffer[kTimeStrSize];    
     UInt32* theReport = (UInt32*)packetBuff;
    
    /* 得到SSRC */
    theReport++;
    UInt32 ssrc = htonl(*theReport);
    
    theReport++;
    SInt64 ntp = 0;
	/* 获取ntp timestamp域 */
    ::memcpy(&ntp, theReport, sizeof(SInt64));
    ntp = OS::NetworkToHostSInt64(ntp);
    time_t theTime = OS::Time1900Fixed64Secs_To_UnixTimeSecs(ntp);
     
	/* 得到RTP timestamp */
    theReport += 2;
    UInt32 timestamp = ntohl(*theReport);
    Float32 theTimeInSecs = 0.0;

    if (fFirstTimeStamp == 0)
        fFirstTimeStamp = timestamp;

    if (fTimescale > 0 && fFirstTimeStamp < timestamp )
		/* 得到从开始传输到此SR包产生时的时间间隔(s) */
        theTimeInSecs = (Float32) (timestamp - fFirstTimeStamp) /  (Float32) fTimescale;
    
	/* 得到Packet Count域,即从开始传输到此SR包产生时该发送者发送的RTP数据包总数 */
    theReport++;        
    UInt32 packetcount = ntohl(*theReport);
    
	/* 得到Byte Count域,即从开始传输到此SR包产生时该发送者发送的RTP数据包的总字节数(不含包头12字节) */
    theReport++;
    UInt32 bytecount = ntohl(*theReport);          
    
	/* 获取并打印该RTPStream的负载类型 */
    StrPtrLen   *payloadStr = this->GetValue(qtssRTPStrPayloadName);
    if (payloadStr && payloadStr->Len > 0)
        payloadStr->PrintStr();
    else
        qtss_printf("?");

    qtss_printf(" H_ssrc=%lu H_bytes=%lu H_ts=%lu H_pckts=%lu ts_secs=%.3f H_ntp=%s", ssrc,bytecount, timestamp, packetcount, theTimeInSecs, ::qtss_ctime( &theTime,timebuffer,sizeof(timebuffer)) );
 }

/* 引用了RTPStream::PrintRTP()和RTPStream::PrintRTCPSenderReport() */
/* 针对包的类型(RTP/SR/RR/APP/Ack),依据服务器预设值,决定是否打印输出.注意:对client发送的RR/APP/Ack,要先解析合法性 */
void RTPStream::PrintPacket(char *inBuffer, UInt32 inLen, SInt32 inType)
{
    static char* rr="RR";
    static char* ack="ACK";
    static char* app="APP";
    static char* sTypeAudio=" type=audio";
    static char* sTypeVideo=" type=video";
    static char* sUnknownTypeStr = "?";

    char* theType = sUnknownTypeStr;
    
    if (fPayloadType == qtssVideoPayloadType)
        theType = sTypeVideo;
    else if (fPayloadType == qtssAudioPayloadType)
        theType = sTypeAudio;

	/* 对给定的RTP/RTCP包类型,分析如下 */
    switch (inType)
    {
        case RTPStream::rtp:
           if (QTSServerInterface::GetServer()->GetPrefs()->PrintRTPHeaders())
           {
                qtss_printf("<send sess=%lu: RTP %s xmit_sec=%.3f %s size=%lu ", this->fSession->GetUniqueID(), this->GetStreamTypeStr(), this->GetStreamStartTimeSecs(), theType, inLen);
                PrintRTP(inBuffer, inLen);
           }
        break;
         
        case RTPStream::rtcpSR:
            if (QTSServerInterface::GetServer()->GetPrefs()->PrintSRHeaders())
            {
                qtss_printf("<send sess=%lu: SR %s xmit_sec=%.3f %s size=%lu ", this->fSession->GetUniqueID(), this->GetStreamTypeStr(), this->GetStreamStartTimeSecs(), theType, inLen);
                PrintRTCPSenderReport(inBuffer, inLen);
            }
        break;
        
        case RTPStream::rtcpRR:
           if (QTSServerInterface::GetServer()->GetPrefs()->PrintRRHeaders())
           {   
                RTCPReceiverPacket rtcpRR;
				/* 首先要解析RR包 */
                if (rtcpRR.ParseReceiverReport( (UInt8*) inBuffer, inLen))
                {
                    qtss_printf(">recv sess=%lu: RTCP %s recv_sec=%.3f %s size=%lu ",this->fSession->GetUniqueID(), rr, this->GetStreamStartTimeSecs(), theType, inLen);
                    rtcpRR.Dump();
                }
           }
        break;
        
        case RTPStream::rtcpAPP:
            if (QTSServerInterface::GetServer()->GetPrefs()->PrintAPPHeaders())
            {   
                Bool16 debug = true;
				/* 首先要解析RR包 */
                RTCPCompressedQTSSPacket compressedQTSSPacket(debug);
                if (compressedQTSSPacket.ParseCompressedQTSSPacket((UInt8*)inBuffer, inLen))
                {
                    qtss_printf(">recv sess=%lu: RTCP %s recv_sec=%.3f %s size=%lu ",this->fSession->GetUniqueID(), app, this->GetStreamStartTimeSecs(), theType, inLen);
                    compressedQTSSPacket.Dump();
                }
            }
        break;
        
        case RTPStream::rtcpACK:
            if (QTSServerInterface::GetServer()->GetPrefs()->PrintACKHeaders())
            {   
				/* 首先要解析RR包 */
                RTCPAckPacket rtcpAck;
                if (rtcpAck.ParseAckPacket((UInt8*)inBuffer,inLen))
                {
                    qtss_printf(">recv sess=%lu: RTCP %s recv_sec=%.3f %s size=%lu ",this->fSession->GetUniqueID(), ack, this->GetStreamStartTimeSecs(), theType, inLen);
                    rtcpAck.Dump();
                }
            }
        break;

		default:
			break;              
    }

}
