
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPStream.h
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

#ifndef __RTPSTREAM_H__
#define __RTPSTREAM_H__

#include "QTSS.h"
#include "QTSServerInterface.h"
#include "QTSSDictionary.h"
#include "QTSS_Private.h"

#include "UDPDemuxer.h"
#include "UDPSocketPool.h"

#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTPPacketResender.h"/* 丢包重传类 */


class RTPStream : public QTSSDictionary, public UDPDemuxerTask //注意RTPStream作为哈希表元
{
    public:
        
        // Initializes dictionary resources
        static void Initialize();

        //
        // CONSTRUCTOR / DESTRUCTOR
        
        RTPStream(UInt32 inSSRC, RTPSessionInterface* inSession);
        virtual ~RTPStream();
        
        //
        //ACCESS FUNCTIONS
        
        UInt32      GetSSRC()                   { return fSsrc; }
        UInt8       GetRTPChannelNum()          { return fRTPChannel; }
        UInt8       GetRTCPChannelNum()         { return fRTCPChannel; }

		/* 得到重传数据包类的指针 */
        RTPPacketResender* GetResender()        { return &fResender; }
        QTSS_RTPTransportType GetTransportType(){ return fTransportType; }
        UInt32      GetStalePacketsDropped()    { return fStalePacketsDropped; }
        UInt32      GetTotalPacketsRecv()       { return fTotalPacketsRecv; }

        // Setup uses the info in the RTSPRequestInterface to associate
        // all the necessary resources, ports, sockets, etc, etc, with this
        // stream.
        QTSS_Error Setup(RTSPRequestInterface* request, QTSS_AddStreamFlags inFlags);
        
        // Write sends RTP/SR data to the client. Caller must specify
        // either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
        virtual QTSS_Error  Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, QTSS_WriteFlags inFlags);
 
        
        //UTILITY FUNCTIONS:
        //These are not necessary to call and do not manipulate the state of the
        //stream. They may, however, be useful services exported by the server
        
        // Formats a standard setup response.
		/* 发送SETUP响应 */
        void            SendSetupResponse(RTSPRequestInterface* request);

        //Formats a transport header for this stream. 
		/* 生成传输头 */
        void            AppendTransport(RTSPRequestInterface* request);
        
        //Formats a RTP-Info header for this stream.
        //Isn't useful unless you've already called Play()
		/* 生成 RTP-Info头 */
        void            AppendRTPInfo(QTSS_RTSPHeader inHeader,
                                        RTSPRequestInterface* request, UInt32 inFlags, Bool16 lastInfo);

        //
        // When we get an incoming Interleaved Packet for this stream, this
        // function should be called
		/* 处理收到的Interleaved packet */
        void ProcessIncomingInterleavedData(UInt8 inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket);

        //When we get a new RTCP packet, we can directly invoke the RTP session and tell it
        //to process the packet right now!
		/* 处理RTCP包 */
        void ProcessIncomingRTCPPacket(StrPtrLen* inPacket);

        // Send a RTCP SR on this stream. Pass in true if this SR should also have a BYE
		/* need by RTPSession::run() */
        void SendRTCPSR(const SInt64& inTime, Bool16 inAppendBye = false);
        
        //
        // Retransmits get sent when there is new data to be sent, but this function
        // should be called periodically even if there is no new packet data, as
        // the pipe（管道） should have a steady stream of data in it. 
		/* need by RTPSession::run() */
        void SendRetransmits();

        //
        // Update the thinning parameters for this stream to match current prefs
        void SetThinningParams();
        
		//
		// Reset the delay parameters that are stored for the thinning calculations
		void ResetThinningDelayParams() { fLastCurrentPacketDelay = 0; }
		
		void SetLateTolerance(Float32 inLateToleranceInSec) { fLateToleranceInSec = inLateToleranceInSec; }
		
		void EnableSSRC()  { fEnableSSRC = true; }
		void DisableSSRC() { fEnableSSRC = false; }
		
    private:
        
        enum
        {
            kMaxSsrcSizeInBytes         = 12,
            kMaxStreamURLSizeInBytes    = 32,
            kDefaultPayloadBufSize      = 32, /* 默认的Payload字符串大小 */
            kSenderReportIntervalInSecs = 7,  /* 每7s服务器向Client发送一次SR包 */
            kNumPrebuiltChNums          = 10, /* 预生成的Interleave channel号的个数 */
        };
    
		// Quality
        SInt64      fLastQualityChange;
        SInt32      fQualityInterval; /* Quality Level控制间隔 */
		SInt32      fQualityLevel;    /* Quality Level控制级别 */
		UInt32      fNumQualityLevels;/* Quality Level总数 */

        //either pointers to the statically allocated sockets (maintained by the server)
        //or fresh ones (only fresh in extreme special cases)
        UDPSocketPair*          fSockets; //指向QTSServer启动时全局分配的UDPSocketPair,参见QTSServer::SetupUDPSockets()
        RTPSessionInterface*    fSession;

        // info for kind a reliable UDP
        //DssDurationTimer      fInfoDisplayTimer;	
        SInt32                  fBytesSentThisInterval; /* 此时间间隔发送的字节总数 */
        SInt32                  fDisplayCount;          /* 参见RTPPacketResender::SpillGuts() */
        Bool16                  fSawFirstPacket;	    /* 看到第一个包了吗?参见RTPStream::ReliableRTPWrite() */
        SInt64                  fStreamCumDuration;     /* RTPStream累计时长,参见RTPStream::ReliableRTPWrite() */

        // manages UDP retransmits
		/************** 保证RTP包传输的QoS机制 *******************/
		
        RTPPacketResender       fResender;/* 丢包重传类,应用到下面的RTPBandwidthTracker类 */
        RTPBandwidthTracker*    fTracker; /* 拥塞控制类 */
       
		/************** 保证RTP包传输的QoS机制 *******************/
        
        //who am i sending to?
        UInt32      fRemoteAddr;    //client端的ip
        UInt16      fRemoteRTPPort; //client端接收RTP包的端口
        UInt16      fRemoteRTCPPort;//client端发回RTCP包的端口
        UInt16      fLocalRTPPort;  //Server端发送RTP包的端口

		// If we are interleaving RTP data over the TCP connection,
		// these are channel numbers to use for RTP & RTCP
		/* TCP/RTCP通道号 */
		UInt8       fRTPChannel;
		UInt8       fRTCPChannel;
        
        //RTCP stuff,see RTPStream::ProcessIncomingRTCPPacket()/SendRTCPSR()/RTPStream::Write()  
        SInt64      fLastSenderReportTime; /* 上次Server端发送SR的时间戳 */
        UInt32      fPacketCount;/* 当前发送的RTP和SR包总数, 从开始传输到此SR包产生时该发送者发送的RTP数据包总数。 */
        UInt32      fLastPacketCount;/* 上次发送的RTP包总数 */
        UInt32      fPacketCountInRTCPInterval;/* 在一个RTCP时间间隔内发送的RTP包总数 */
        UInt32      fByteCount; /* Server端发送出的总字节数 */
        
        // DICTIONARY ATTRIBUTES
        
        //Module assigns a streamID to this object
        UInt32      fTrackID; /* 一个track对应一个RTPStream */
        
        //low-level RTP stuff 	
        UInt32      fSsrc; /* 同步源 */
        char        fSsrcString[kMaxSsrcSizeInBytes];/* 同步源字符串 */
        StrPtrLen   fSsrcStringPtr;
        Bool16      fEnableSSRC;/* Server向Client回送RTSP Response时,发送SSRC吗? */
        
        //Payload name and codec type.
        char                    fPayloadNameBuf[kDefaultPayloadBufSize]; /* payload名称字符串,"mpeg4-generic/22050/2"或"MP4V-ES/90000" */
        QTSS_RTPPayloadType     fPayloadType; /* audio & video */	
		Bool16                  fIsTCP;
		QTSS_RTPTransportType   fTransportType;/* 传输类型（UDP/RUDP/TCP） */
		QTSS_RTPNetworkMode     fNetworkMode;   //unicast | multicast

        //Media information.
        UInt16      fFirstSeqNumber;//used in sending the play response
        UInt32      fFirstTimeStamp;//RTP timestamp,see RTPStream::AppendRTPInfo()
        UInt32      fTimescale;     //该track的时间刻度
		UInt32      fLastRTPTimestamp;
        
        //what is the URL for this stream?
        char        fStreamURL[kMaxStreamURLSizeInBytes]; //"trackID=3"
        StrPtrLen   fStreamURLPtr;
        SInt64      fStreamStartTimeOSms; //RTPStream开始的时间
		// Pointer to the stream ref (this is just a this pointer)
		QTSS_StreamRef  fStreamRef; /* stream的this指针 */
     
        // RTCP data,especially for APP
        UInt32      fFractionLostPackets;
        UInt32      fTotalLostPackets;
        UInt32      fJitter;
        UInt32      fReceiverBitRate;              //接收者比特率
        UInt16      fAvgLateMsec;                  //平均延时(ms)
        UInt16      fPercentPacketsLost;           //丢包百分比
        UInt16      fAvgBufDelayMsec;              //平均缓冲延时(ms)
        UInt16      fIsGettingBetter;              //播放质量正在变好吗?
        UInt16      fIsGettingWorse;               //播放质量正在变差吗?
        UInt32      fNumEyes;                      //观众数
        UInt32      fNumEyesActive;                //活跃的观众数
        UInt32      fNumEyesPaused;                //暂停的观众数
        UInt32      fTotalPacketsRecv;             //收到的总包数
        UInt32      fPriorTotalPacketsRecv;        //前一个收到的总包数
        UInt16      fTotalPacketsDropped;          //丢弃的总包数
        UInt16      fTotalPacketsLost;             //丢失的总包数
        UInt32      fCurPacketsLostInRTCPInterval;
        UInt16      fClientBufferFill;             //客户端缓存填充数
        UInt16      fFrameRate;                    //帧率
        UInt16      fExpectedFrameRate;            //期望的帧率
        UInt16      fAudioDryCount;                //音频丢包
        UInt32      fClientSSRC;                   //Client端的SSRC,参见RTCPSRPacket类
          
        // HTTP params 针对TCP方式的薄化丢包
		// Each stream has a set of thinning related tolerances,that are dependent on prefs and parameters in the SETUP. 
		// These params, as well as the current packet delay determine whether a packet gets dropped.
        SInt32      fTurnThinningOffDelay_TCP;
        SInt32      fIncreaseThinningDelay_TCP;
        SInt32      fDropAllPacketsForThisStreamDelay_TCP;
        UInt32      fStalePacketsDropped_TCP;
        SInt64      fTimeStreamCaughtUp_TCP;
        SInt64      fLastQualityLevelIncreaseTime_TCP;

		// the server's thinning algorithm(ms)
        // Each stream has a set of thinning related tolerances,that are dependent on prefs and parameters in the SETUP. 
        // These params, as well as the current packet delay determine whether a packet gets dropped.
        SInt32      fThinAllTheWayDelay;
        SInt32      fAlwaysThinDelay;
        SInt32      fStartThinningDelay;
        SInt32      fStartThickingDelay;
        SInt32      fThickAllTheWayDelay;
        SInt32      fQualityCheckInterval;             //质量检查间隔
        SInt32      fDropAllPacketsForThisStreamDelay; //丢掉这个RTPStream的所有包的延迟极限,参见RTPStream::SetThinningParams()
        UInt32      fStalePacketsDropped;              //扔掉过时包总数
        SInt64      fLastCurrentPacketDelay;           //上次的发包延时值
        Bool16      fWaitOnLevelAdjustment;             //是否等待级别调整?
              
		/* 每发送一个数据包DSS都会调整一次播放质量，函数返回值表示当前包是否应该发送. fLateToleranceInSec为
		上一步得到的客户端延时，如果客户端没有通过SETUP设置则默认值为1.5秒。参见RTPStream::SetThinningParams()/Setup() */
        Float32     fLateToleranceInSec; //客户端延迟,默认1.5s,从RTSPRequestInterface的RTSP头"x-RTP-Options: late-tolerance=3"得到
        Float32     fBufferDelay; // 3.0 from the sdp        
        
		/* Server发送的Ack? */
        UInt32      fCurrentAckTimeout;
        SInt32      fMaxSendAheadTimeMSec; //最大提前发送时间(ms)
        
#if DEBUG	
        UInt32      fNumPacketsDroppedOnTCPFlowControl;/* TCP流控的丢包数 */
        SInt64      fFlowControlStartedMsec; /* 流控开始时间戳(ms) */
        SInt64      fFlowControlDurationMsec;/* 流控持续时间(ms) */
#endif
        		    
		/************************ 各种write functions,都被包装在RTPStream::Write()中 ******************************/
        // acutally write the data out that way in interleaved channels
        QTSS_Error  InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel );

        // implements the ReliableRTP protocol
        QTSS_Error  ReliableRTPWrite(void* inBuffer, UInt32 inLen, const SInt64& curPacketDelay);

        void        SetTCPThinningParams();
        QTSS_Error  TCPWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags);

		/************************ write functions*******************************/

		//dictionary attr
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
        static StrPtrLen                    sChannelNums[];
        static QTSS_ModuleState             sRTCPProcessModuleState;

		//protocol TYPE str
        static char *noType;
        static char *UDP;
        static char *RUDP;
        static char *TCP;
        
        Bool16      UpdateQualityLevel(const SInt64& inTransmitTime, const SInt64& inCurrentPacketDelay,const SInt64& inCurrentTime, UInt32 inPacketSize);                                        
        SInt32      GetQualityLevel();
        void        SetQualityLevel(SInt32 level);
           
		/* RTP/RTCP Packet types */
        enum { rtp = 0, rtcpSR = 1, rtcpRR = 2, rtcpACK = 3, rtcpAPP = 4 };

        char*   GetStreamTypeStr();
        Float32 GetStreamStartTimeSecs() { return (Float32) ((OS::Milliseconds() - this->fSession->GetSessionCreateTime())/1000.0); }
       
		void PrintPacket(char *inBuffer, UInt32 inLen, SInt32 inType); 
        void PrintRTP(char* packetBuff, UInt32 inLen);
        void PrintRTCPSenderReport(char* packetBuff, UInt32 inLen);
inline  void PrintPacketPrefEnabled(char *inBuffer,UInt32 inLen, SInt32 inType) { if (QTSServerInterface::GetServer()->GetPrefs()->PacketHeaderPrintfsEnabled() ) this->PrintPacket(inBuffer,inLen, inType); }

        /* QTSSFileModule::SendPackets() encounter error */
        void SetOverBufferState(RTSPRequestInterface* request);

};

#endif // __RTPSTREAM_H__
