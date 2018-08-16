
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSessionInterface.h
Description: Provides an API interface for objects to access the attributes 
             related to a RTPSession, also implements the RTP Session Dictionary.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _RTPSESSIONINTERFACE_H_
#define _RTPSESSIONINTERFACE_H_

#include "QTSSDictionary.h"
#include "QTSServerInterface.h"

#include "TimeoutTask.h"
#include "Task.h"
#include "RTCPSRPacket.h"
#include "RTSPSessionInterface.h"
#include "RTPBandwidthTracker.h"
#include "RTPOverbufferWindow.h"

#include "OSMutex.h"
#include "atomic.h"

class RTSPRequestInterface;

class RTPSessionInterface : public QTSSDictionary, public Task
{
    public:
    
        // Initializes dictionary resources
        static void Initialize();
        
        // CONSTRUCTOR / DESTRUCTOR   
        RTPSessionInterface();
        virtual ~RTPSessionInterface()
        {   
            if (GetQualityLevel() != 0)
                QTSServerInterface::GetServer()->IncrementNumThinned(-1);
            if (fRTSPSession != NULL)
                fRTSPSession->DecrementObjectHolderCount();
            delete [] fSRBuffer.Ptr;
            delete [] fAuthNonce.Ptr;       
            delete [] fAuthOpaque.Ptr;      
        }

        virtual void SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen);

        //Timeouts. This allows clients to refresh the timeout on this session
        void    RefreshTimeout()        { fTimeoutTask.RefreshTimeout(); }
        void    RefreshRTSPTimeout()    { if (fRTSPSession != NULL) fRTSPSession->RefreshTimeout(); }
        void    RefreshTimeouts()       { RefreshTimeout(); RefreshRTSPTimeout();}
        
        // ACCESSORS
        
        Bool16  IsFirstPlay()           { return fIsFirstPlay; }
        SInt64  GetFirstPlayTime()      { return fFirstPlayTime; }
        //Time (msec) most recent play was issued
        SInt64  GetPlayTime()           { return fPlayTime; }
        SInt64  GetNTPPlayTime()        { return fNTPPlayTime; }
		/* used in RTPStatsUpdaterTask::GetNewestSession() */
        SInt64  GetSessionCreateTime()  { return fSessionCreateTime; }
        //Time (msec) most recent play, adjusted for start time of the movie
        //ex: PlayTime() == 20,000. Client said start 10 sec into the movie,
        //so AdjustedPlayTime() == 10,000

        QTSS_PlayFlags GetPlayFlags()   { return fPlayFlags; } /* need by RTPSession::run() */
        OSMutex*        GetSessionMutex()   { return &fSessionMutex; }
        UInt32          GetPacketsSent()    { return fPacketsSent; }
        UInt32          GetBytesSent()  { return fBytesSent; }

		/* 得到Hash Table表元(也就是RTPSession) */
        OSRef*      GetRef()            { return &fRTPMapElem; }
		/* 得到RTSP SessionInterface */
        RTSPSessionInterface* GetRTSPSession() { return fRTSPSession; }

        UInt32      GetMovieAvgBitrate(){ return fMovieAverageBitRate; }/* needed by RTPSession::Play() */
        QTSS_CliSesTeardownReason GetTeardownReason() { return fTeardownReason; }
        QTSS_RTPSessionState    GetSessionState() { return fState; }
        void    SetUniqueID(UInt32 theID)   {fUniqueID = theID;}
        UInt32  GetUniqueID()           { return fUniqueID; }


        RTPBandwidthTracker* GetBandwidthTracker() { return &fTracker; } /* needed by RTPSession::run() */
        RTPOverbufferWindow* GetOverbufferWindow() { return &fOverbufferWindow; }
        UInt32  GetFramesSkipped() { return fFramesSkipped; }
        
        // MEMORY FOR RTCP PACKETS
        
        // Class for easily building a standard RTCP SR
        RTCPSRPacket*   GetSRPacket()       { return &fRTCPSRPacket; }//注意它的设置在RTPStream::SendRTCPSR()中

        // Memory if you want to build your own
        char*           GetSRBuffer(UInt32 inSRLen);
        
        // STATISTICS UPDATING
        
        //The session tracks the total number of bytes sent during the session.
        //Streams can update that value by calling this function
		/* 更新该RTPSession发送的字节总数 */
        void            UpdateBytesSent(UInt32 bytesSent) { fBytesSent += bytesSent; }
                        
        //The session tracks the total number of packets sent during the session.
        //Streams can update that value by calling this function 
		/* 更新该RTPSession发送的RTP Packet总数 */
        void            UpdatePacketsSent(UInt32 packetsSent) { fPacketsSent += packetsSent; }
                                        
        void            UpdateCurrentBitRate(const SInt64& curTime)
                         { if (curTime > (fLastBitRateUpdateTime + 10000)) this->UpdateBitRateInternal(curTime); }

		/* 根据入参设置all track是否interleaved? */
        void            SetAllTracksInterleaved(Bool16 newValue) { fAllTracksInterleaved = newValue; }      
       
        // RTSP RESPONSE
        // This function appends a session header to the SETUP response, and
        // checks to see if it is a 304 Not Modified. If it is, it sends the entire
        // response and returns an error
        QTSS_Error DoSessionSetupResponse(RTSPRequestInterface* inRequest);
        
        // RTSP SESSION                    
        // This object has a current RTSP session. This may change over the
        // life of the RTSPSession, so update it. It keeps an RTSP session in
        // case interleaved data or commands need to be sent back to the client. 
		/* 更新RTSPSession */
        void            UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession);
                
        // let's RTSP Session pass along it's query string
		/* 设置查询字符串 */
        void            SetQueryString( StrPtrLen* queryString );
        
        // SETTERS and ACCESSORS for auth information
        // Authentication information that needs to be kept around
        // for the duration of the session      
        QTSS_AuthScheme GetAuthScheme() { return fAuthScheme; }
        StrPtrLen*      GetAuthNonce() { return &fAuthNonce; }
        UInt32          GetAuthQop() { return fAuthQop; }
        UInt32          GetAuthNonceCount() { return fAuthNonceCount; }
        StrPtrLen*      GetAuthOpaque() { return &fAuthOpaque; }
        void            SetAuthScheme(QTSS_AuthScheme scheme) { fAuthScheme = scheme; }
        // Use this if the auth scheme or the qop has to be changed from the defaults 
        // of scheme = Digest, and qop = auth
        void            SetChallengeParams(QTSS_AuthScheme scheme, UInt32 qop, Bool16 newNonce, Bool16 createOpaque);
        // Use this otherwise...if newNonce == true, it will create a new nonce
        // and reset nonce count. If newNonce == false but nonce was never created earlier
        // a nonce will be created. If newNonce == false, and there is an existing nonce,
        // the nounce count will be incremented.
        void            UpdateDigestAuthChallengeParams(Bool16 newNonce, Bool16 createOpaque, UInt32 qop);
    
        Float32*        GetPacketLossPercent() { UInt32 outLen;return  (Float32*) this->PacketLossPercent(this, &outLen);}

        SInt32          GetQualityLevel()   { return fSessionQualityLevel; }
        SInt32*         GetQualityLevelPtr()    { return &fSessionQualityLevel; }
        void            SetQualityLevel(SInt32 level)   { if (fSessionQualityLevel == 0 && level != 0)
                                                                QTSServerInterface::GetServer()->IncrementNumThinned(1);
                                                            else if (fSessionQualityLevel != 0 && level == 0)
                                                                QTSServerInterface::GetServer()->IncrementNumThinned(-1);
                                                            fSessionQualityLevel = level; 
                                                         }

		//the server's thinning algorithm(quality level),参见RTPStream::UpdateQualityLevel()
        SInt64          fLastQualityCheckTime;          //上次quality level检查时间
		SInt64			fLastQualityCheckMediaTime;     //上次发送的RTP设定的发送绝对时间戳
		Bool16			fStartedThinning;               //是否开始瘦化?
		
        // Used by RTPStream to increment the RTCP packet and byte counts.
		/* 用于RTCP包 */
        void            IncrTotalRTCPPacketsRecv()         { fTotalRTCPPacketsRecv++; }
        UInt32          GetTotalRTCPPacketsRecv()          { return fTotalRTCPPacketsRecv; }
        void            IncrTotalRTCPBytesRecv(UInt16 cnt) { fTotalRTCPBytesRecv += cnt; }
        UInt32          GetTotalRTCPBytesRecv()            { return fTotalRTCPBytesRecv; }

    protected:
    
        // These variables are setup by the derived RTPSession object when Play and Pause get called 
        //Some stream related information that is shared amongst all streams,参见RTPSession::Play()
        Bool16      fIsFirstPlay; /* 是第一次播放吗? */
        SInt64      fFirstPlayTime;//in milliseconds,第一次播放的时间
        SInt64      fPlayTime;//RTP包的当前播放时间戳,参见RTPStream::UpdateQualityLevel()
        SInt64      fAdjustedPlayTime;/* 获取当前播放时间戳和RTSP Request Header Range中的fStartTime的差值,这个量非常重要 */
        SInt64      fNTPPlayTime;/* 为方便SR包,将fPlayTime存成NTP格式的值 */
		Bool16      fAllTracksInterleaved;/* all stream 通过RTSP channel交替发送吗? */

		/* very important! this is actual absolute timestamp of send the next RTP packets */
        SInt64      fNextSendPacketsTime;/* need by RTPSession::run()/RTPSession::Play() */
        
		/* RTPSession的quality level */
        SInt32      fSessionQualityLevel;
        
        //keeps track of whether we are playing or not
		/* Play or Pause,need by RTPSession::run() */
        QTSS_RTPSessionState fState;
        
        // have the server generate RTCP Sender Reports or have the server append the server info APP packet to your RTCP Sender Reports
		/* see QTSS.h */
        QTSS_PlayFlags  fPlayFlags;
        
        //Session mutex. This mutex should be grabbed before invoking the module
        //responsible for managing this session. This allows the module to be
        //non-preemptive-safe with respect to a session
        OSMutex     fSessionMutex; /* 会话互斥锁,used in RTPSession::run() */

        //Stores the RTPsession ID
		/* RTPSession存为RTPSessionMap的HashTable表元 */
        OSRef       fRTPMapElem;
		/* RTSPSession的ID值,是个唯一的随机数,比如"58147963412401" */
        char        fRTSPSessionIDBuf[QTSS_MAX_SESSION_ID_LENGTH + 4];
        
		/* 以当前比特率送出的字节数 */
        UInt32      fLastBitRateBytes;
		/* 当前比特率的更新时间 */
        SInt64      fLastBitRateUpdateTime;
		/* movie的当前比特率,它是如何计算的? 参见RTPSessionInterface::UpdateBitRateInternal() */
        UInt32      fMovieCurrentBitRate;
        
        // In order to facilitate sending data over the RTSP channel from
        // an RTP session, we store a pointer to the RTSP session used in
        // the last RTSP request.
		/* needed by RTPSession::Play(),与RTSP会话相关的纽带 */
		/* 为了便于用RTSP channel发送Interleaved数据,在上一个RTSPRequest中存放一个RTSPSession的指针,这个量非常重要! */
        RTSPSessionInterface* fRTSPSession;

    private:
    
        // Utility function for calculating current bit rate
        void UpdateBitRateInternal(const SInt64& curTime);

		/* 下面这三个函数用于setup ClientSessionDictionary Attr */
        static void* CurrentBitRate(QTSSDictionary* inSession, UInt32* outLen);
        static void* PacketLossPercent(QTSSDictionary* inSession, UInt32* outLen);
        static void* TimeConnected(QTSSDictionary* inSession, UInt32* outLen);
         
        // Create nonce
		/* 生成摘要认证随机序列,Nonce指随机序列 */
        void CreateDigestAuthenticationNonce();

        // One of the RTP session attributes is an iterated list of all streams.
        // As an optimization, we preallocate a "large" buffer of stream pointers,
        // even though we don't know how many streams we need at first.
		/* 这些常数下面要用到 */
        enum
        {
            kStreamBufSize              = 4,
            kUserAgentBufSize           = 256,
            kPresentationURLSize        = 256,
            kFullRequestURLBufferSize   = 256,
            kRequestHostNameBufferSize  = 80,
            
            kIPAddrStrBufSize           = 20,
            kLocalDNSBufSize            = 80,
            
            kAuthNonceBufSize           = 32,
            kAuthOpaqueBufSize          = 32,       
        };
        
        void*       fStreamBuffer[kStreamBufSize];

        
        // theses are dictionary items picked up by the RTSPSession
        // but we need to store copies of them for logging purposes.
		/* 从RTSPSession获取的相关字典属性,为了日志记录,需要它们,参见RTSPSession::SetupClientSessionAttrs() */
        char        fUserAgentBuffer[kUserAgentBufSize]; //eg VLC media player (LIVE555 Streaming Media v2007.02.20)
        char        fPresentationURL[kPresentationURLSize];         // eg /foo/bar.mov,/sample_300kbit.mp4
        char        fFullRequestURL[kFullRequestURLBufferSize];     // eg rtsp://172.16.34.22/sample_300kbit.mp4
        char        fRequestHostName[kRequestHostNameBufferSize];   // eg 172.16.34.22
    
        char        fRTSPSessRemoteAddrStr[kIPAddrStrBufSize];
        char        fRTSPSessLocalDNS[kLocalDNSBufSize];
        char        fRTSPSessLocalAddrStr[kIPAddrStrBufSize];
        
        char        fUserNameBuf[RTSPSessionInterface::kMaxUserNameLen];
        char        fUserPasswordBuf[RTSPSessionInterface::kMaxUserPasswordLen];
        char        fUserRealmBuf[RTSPSessionInterface::kMaxUserRealmLen];
        UInt32      fLastRTSPReqRealStatusCode;

        //for timing out this session
        TimeoutTask fTimeoutTask; /* 使会话超时的TimeoutTask类 */	
        UInt32      fTimeout;/* 超时时间,默认1s */

		UInt32      fUniqueID;/* RTPSession ID */
        
        // Time when this session got created
		/* 生成RTPSession的时间 */
        SInt64      fSessionCreateTime;

        //Packet priority levels. Each stream has a current level, and
        //the module that owns this session sets what the number of levels is.
		/* 当前RTPSession的Packet优先级,相关Module可以设置 */
        UInt32      fNumQualityLevels;
        
        //Statistics
        UInt32 fBytesSent;/* 发送的字节数 */
        UInt32 fPacketsSent; /* 发送的包数 */   
        Float32 fPacketLossPercent; /* 丢包百分比 */
        SInt64 fTimeConnected; /* 连接时长 */
        UInt32 fTotalRTCPPacketsRecv; /* 接收到的RTCP包总数 */
        UInt32 fTotalRTCPBytesRecv; /* 接收到的RTCP包字节总数 */


        // Movie size & movie duration. It may not be so good to associate these
        // statistics with the movie, for a session MAY have multiple movies...
        // however, the 1 movie assumption is in too many subsystems at this point
		/* 电影的持续时间 */
        Float64     fMovieDuration;
		/* 当前movie的字节数 */
        UInt64      fMovieSizeInBytes;
		/* movie的平均比特率 */
        UInt32      fMovieAverageBitRate;
        
		/* RTPSession TEARDOWN的原因 */
        QTSS_CliSesTeardownReason fTeardownReason;
        
        // So the streams can send sender reports(SR)
        RTCPSRPacket        fRTCPSRPacket;
        StrPtrLen           fSRBuffer;
        
		/* needed by RTPSession::run() */
		/* 追踪当前带宽的类 */
        RTPBandwidthTracker fTracker;
		/* overbuffering Windows超前发送机制 */
        RTPOverbufferWindow fOverbufferWindow;
        
        // Built in dictionary attributes
		/* 静态的内建字典属性 */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
        static unsigned int sRTPSessionIDCounter; /* RTPSessionID计数器 */

        // Authentication information that needs to be kept around
        // for the duration of the session      
        QTSS_AuthScheme             fAuthScheme; /* 使用的认证格式 */
        StrPtrLen                   fAuthNonce; /* 认证序列 */
        UInt32                      fAuthQop;
        UInt32                      fAuthNonceCount;                    
        StrPtrLen                   fAuthOpaque;
        UInt32                      fQualityUpdate;// fQualityUpdate is a counter, the starting value is the unique ID, so every session starts at a different position
        
        UInt32                      fFramesSkipped;
		/* 是否启用overbuffering Window,参见RTPOverbufferWindow.h中同名字段 */
        Bool16                      fOverBufferEnabled;
};

#endif //_RTPSESSIONINTERFACE_H_
