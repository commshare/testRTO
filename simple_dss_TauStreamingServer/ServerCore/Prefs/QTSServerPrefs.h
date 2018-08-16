
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSServerPrefs.h
Description: A object that stores for RTSP server preferences.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 




#ifndef __QTSSERVERPREFS_H__
#define __QTSSERVERPREFS_H__

#include "StrPtrLen.h"
#include "QTSSPrefs.h"
#include "XMLPrefsParser.h"



class QTSServerPrefs : public QTSSPrefs
{
    public:

        // INITIALIZE
        //
        // This function sets up the dictionary map. Must be called before instantiating(实例化)
        // the first RTSPPrefs object.
    
        static void Initialize();

        QTSServerPrefs(XMLPrefsParser* inPrefsSource, Bool16 inWriteMissingPrefs);
        virtual ~QTSServerPrefs() {}
        
        //This is callable at any time, and is thread safe wrt to the accessors.
        //Pass in true if you want this function to update the prefs file if
        //any defaults need to be used. False otherwise
        void RereadServerPreferences(Bool16 inWriteMissingPrefs);
        
        //Individual accessor methods for preferences.
        
        //Amount of idle time after which respective protocol sessions are timed out
        //(stored in seconds)
        
        //This is the value we advertise to clients (lower than the real one,比真实值小)
        UInt32  GetRTSPTimeoutInSecs()  { return fRTSPTimeoutInSecs; }
        UInt32  GetRTPTimeoutInSecs()   { return fRTPTimeoutInSecs; }
        StrPtrLen*  GetRTSPTimeoutAsString() { return &fRTSPTimeoutString; }  
        //This is the real timeout
        UInt32  GetRealRTSPTimeoutInSecs(){ return fRealRTSPTimeoutInSecs; }
        
        //-1 means unlimited
        SInt32  GetMaxConnections()         { return fMaximumConnections; }
        SInt32  GetMaxKBitsBandwidth()      { return fMaxBandwidthInKBits; }
        
        // Thinning algorithm parameters 瘦化算法参数
        SInt32  GetDropAllPacketsTimeInMsec()           { return fDropAllPacketsTimeInMsec; }
        SInt32  GetDropAllVideoPacketsTimeInMsec()      { return fDropAllVideoPacketsTimeInMsec; }
        SInt32  GetThinAllTheWayTimeInMsec()            { return fThinAllTheWayTimeInMsec; }
        SInt32  GetAlwaysThinTimeInMsec()               { return fAlwaysThinTimeInMsec; }
        SInt32  GetStartThinningTimeInMsec()            { return fStartThinningTimeInMsec; }
        SInt32  GetStartThickingTimeInMsec()            { return fStartThickingTimeInMsec; }
        SInt32  GetThickAllTheWayTimeInMsec()           { return fThickAllTheWayTimeInMsec; }
        UInt32  GetQualityCheckIntervalInMsec()         { return fQualityCheckIntervalInMsec; }
                
        // for tcp buffer size scaling
        UInt32  GetMinTCPBufferSizeInBytes()            { return fMinTCPBufferSizeInBytes; }
        UInt32  GetMaxTCPBufferSizeInBytes()            { return fMaxTCPBufferSizeInBytes; }
        Float32 GetTCPSecondsToBuffer()                 { return fTCPSecondsToBuffer; }
        
        //for joining HTTP sessions from behind a round-robin DNS, used in RTSPListenerSocket::GetSessionTask()
        Bool16  GetDoReportHTTPConnectionAddress()      { return fDoReportHTTPConnectionAddress;  }
		Bool16  GetAppendSrcAddrInTransport()   { return fAppendSrcAddrInTransport; }
        
        //for debugging, mainly
        Bool16      ShouldServerBreakOnAssert()         { return fBreakOnAssert; }
        Bool16      IsAutoRestartEnabled()              { return fAutoRestart; }

        UInt32      GetTotalBytesUpdateTimeInSecs()     { return fTBUpdateTimeInSecs; }
        UInt32      GetAvgBandwidthUpdateTimeInSecs()   { return fABUpdateTimeInSecs; }
        UInt32      GetSafePlayDurationInSecs()         { return fSafePlayDurationInSecs; }
        
        // For the compiled-in error logging module
        
        Bool16  IsErrorLogEnabled()             { return fErrorLogEnabled; }
        Bool16  IsScreenLoggingEnabled()        { return fScreenLoggingEnabled; }

        UInt32  GetMaxErrorLogBytes()           { return fErrorLogBytes; }
        UInt32  GetErrorRollIntervalInDays()    { return fErrorRollIntervalInDays; }
        UInt32  GetErrorLogVerbosity()          { return fErrorLogVerbosity; }
        void	SetErrorLogVerbosity(UInt32 verbosity)		{ fErrorLogVerbosity = verbosity; }
        
        // For UDP retransmits
        UInt32  GetMaxRetransmitDelayInMsec()   { return fMaxRetransDelayInMsec; }/* needed by RTPSession::run() */
        Bool16  IsAckLoggingEnabled()           { return fIsAckLoggingEnabled; }
        UInt32  GetRTCPPollIntervalInMsec()     { return fRTCPPollIntervalInMsec; }
        UInt32  GetRTCPSocketRcvBufSizeinK()    { return fRTCPSocketRcvBufSizeInK; }
        UInt32  GetSendIntervalInMsec()         { return fSendIntervalInMsec; }/* needed by RTPSession::run() */
        UInt32  GetMaxSendAheadTimeInSecs()     { return fMaxSendAheadTimeInSecs; }
        Bool16  IsSlowStartEnabled()            { return fIsSlowStartEnabled; }
		UInt32  IsReliableUDPEnabled()          { return fReliableUDP; }
        Bool16  GetReliableUDPPrintfsEnabled()  { return fReliableUDPPrintfs; }
        Bool16  GetRTSPDebugPrintfs()           { return fEnableRTSPDebugPrintfs; }
        Bool16  GetRTSPServerInfoEnabled()      { return fEnableRTSPServerInfo; }
        
		Float32	GetOverbufferRate()				{ return fOverbufferRate; }
		
        // RUDP window size
        UInt32  GetSmallWindowSizeInK()         { return fSmallWindowSizeInK; }
		UInt32	GetMediumWindowSizeInK()		{ return fMediumWindowSizeInK; }
        UInt32  GetLargeWindowSizeInK()         { return fLargeWindowSizeInK; }
        UInt32  GetWindowSizeThreshold()        { return fWindowSizeThreshold; }
 		UInt32	GetWindowSizeMaxThreshold()		{ return fWindowSizeMaxThreshold; }

		Bool16 PacketHeaderPrintfsEnabled() { return fEnablePacketHeaderPrintfs; }/* error in QTSSFileModule::SendPackets() */
		Bool16 PrintRTPHeaders()    { return (Bool16) (fPacketHeaderPrintfOptions & kRTPALL); }
		Bool16 PrintSRHeaders()     { return (Bool16) (fPacketHeaderPrintfOptions & kRTCPSR); }
		Bool16 PrintRRHeaders()     { return (Bool16) (fPacketHeaderPrintfOptions & kRTCPRR); }
		Bool16 PrintAPPHeaders()     { return (Bool16) (fPacketHeaderPrintfOptions & kRTCPAPP); }
		Bool16 PrintACKHeaders()     { return (Bool16) (fPacketHeaderPrintfOptions & kRTCPACK); }

       
        // force logs to close after each write (true or false)
        Bool16  GetCloseLogsOnWrite()           { return fCloseLogsOnWrite; }
        void    SetCloseLogsOnWrite(Bool16 closeLogsOnWrite);

		Bool16 ServerStatFileEnabled()      { return fEnableMonitorStatsFile; }
		UInt32 GetStatFileIntervalSec()     { return fStatsFileIntervalSeconds; }

		Bool16  DisableThinning()           { return fDisableThinning; }
		Bool16  AutoDeleteSDPFiles()        { return fauto_delete_sdp_files; }
		UInt32 DeleteSDPFilesInterval()     { return fsdp_file_delete_interval_seconds; }

		QTSS_AuthScheme GetAuthScheme()     { return fAuthScheme; }

		UInt32  GetNumThreads()             { return fNumThreads; }     
        
        // Optionally require that reliable UDP content be in certain folders
        Bool16 IsPathInsideReliableUDPDir(StrPtrLen* inPath);

        // Movie folder pref. If the path fits inside the buffer provided,
        // the path is copied into that buffer. Otherwise, a new buffer is allocated
        // and returned.
        char*   GetMovieFolder(char* inBuffer, UInt32* ioLen);
        
        //
        // Transport addr pref. Caller must provide a buffer big enough for an IP addr
        void    GetTransportSrcAddr(StrPtrLen* ioBuf);
                
        // String preferences. Note that the pointers returned here is allocated
        // memory that you must delete!
        
        char*   GetErrorLogDir()
            { return this->GetStringPref(qtssPrefsErrorLogDir); }
        char*   GetErrorLogName()
            { return this->GetStringPref(qtssPrefsErrorLogName); }

        char*   GetModuleDirectory()
            { return this->GetStringPref(qtssPrefsModuleFolder); }
            
        char*   GetAuthorizationRealm()
            { return this->GetStringPref(qtssPrefsDefaultAuthorizationRealm); }

        char*   GetRunUserName()
            { return this->GetStringPref(qtssPrefsRunUserName); }
        char*   GetRunGroupName()
            { return this->GetStringPref(qtssPrefsRunGroupName); }

		char*   GetPidFilePath()
			{ return this->GetStringPref(qtssPrefsPidFile); }

        char*   GetStatsMonitorFileName()
            { return this->GetStringPref(qtssPrefsMonitorStatsFileName); }

        
    private: //58个预设值

        UInt32      fRTSPTimeoutInSecs;        //断掉Idle RTSP Client的等待时间(s)
        char        fRTSPTimeoutBuf[20];
        StrPtrLen   fRTSPTimeoutString; 
        UInt32      fRealRTSPTimeoutInSecs;    //向Client做出RTSP Response的超时值
        UInt32      fRTPTimeoutInSecs;         //断掉Idle RTP Client的等待时间(s)
        
        SInt32  fMaximumConnections;           //最大连接数
        SInt32  fMaxBandwidthInKBits;          //最大带宽(kilobits/sec)

		SInt32  fDropAllPacketsTimeInMsec;     //丢掉所有包的延时值(ms)
		SInt32  fDropAllVideoPacketsTimeInMsec;//丢掉所有视频包的延时值(ms)
		SInt32  fThinAllTheWayTimeInMsec;      //一直薄化流的延时值(ms),thin to key frames
		SInt32  fAlwaysThinTimeInMsec;         //偶尔薄化流的延时值(ms),we always start to thin at this point
		SInt32  fStartThinningTimeInMsec;      //可能薄化流的延时值(ms),lateness at which we might start thinning
		SInt32  fStartThickingTimeInMsec;      //开始厚化流的延时值(ms),maybe start thicking at this point
		SInt32  fThickAllTheWayTimeInMsec;     //恢复全部质量级别的超前值(ms)
		UInt32  fQualityCheckIntervalInMsec;   //检查流质量等级(或包延时程度)的间隔(ms),adjust thinnning params this often

		UInt32  fMinTCPBufferSizeInBytes;      //TCP发送缓存的最小值
		UInt32  fMaxTCPBufferSizeInBytes;      //TCP发送缓存的最大值
		Float32 fTCPSecondsToBuffer;           //TCP发送缓存的缓冲时间(s)

		Bool16  fDoReportHTTPConnectionAddress;//是否对Client报告流服务器的ip地址?
		Bool16  fAppendSrcAddrInTransport;     //在Transport头中是否附加流服务器的ip地址?
        Bool16  fBreakOnAssert;                //当断言成立时中断
        Bool16  fAutoRestart;                  //是否在崩溃后自动重启?
        UInt32  fTBUpdateTimeInSecs;           //服务器总字节数和当前带宽的统计的更新间隔(s)
        UInt32  fABUpdateTimeInSecs;           //服务器平均带宽的统计的更新间隔(s)
        UInt32  fSafePlayDurationInSecs;       //Client不会被断掉的安全播放时长 
        
		Bool16  fErrorLogEnabled;              //是否记录错误日志?
		Bool16  fScreenLoggingEnabled;         //是否让错误日志滚屏显示?
        UInt32  fErrorLogBytes;                //错误日志的文件大小(字节)
		UInt32  fErrorRollIntervalInDays;      //错误日志的回滚间隔(天)
        UInt32  fErrorLogVerbosity;            //当前错误日志的级别
        
        UInt32  fMaxRetransDelayInMsec;        //最大重传超时(设定的重传时间和实际重传时间的差), needed by RTPSession::run()
        Bool16  fIsAckLoggingEnabled;          //是否打开收到RUDP Ack包的调试日志?Debugging only: turns on detailed logging of UDP acks / retransmits
        UInt32  fRTCPPollIntervalInMsec;       //服务器检查RTCP包的时间间隔(ms)
        UInt32  fRTCPSocketRcvBufSizeInK;      //服务器接收RTCP Ack包的UDPSocket缓存大小(Kbytes)
        UInt32  fSendIntervalInMsec;           //服务器送包的最小间隔(ms), needed by RTPSession::run() 
        UInt32  fMaxSendAheadTimeInSecs;       //服务器最大提前送包时间(25s)
		Bool16  fIsSlowStartEnabled;           //RUDP是否启用慢启动?
        Bool16  fAutoStart;                    //是否开机自动启动?注意在streamingserver.xml中没有!!If true, streaming server likes to be started at system startup
        Bool16  fReliableUDP;                  //是否可用RUDP?
        Bool16  fReliableUDPPrintfs;           //是否使用RUDP打印?
        Bool16  fEnableRTSPErrMsg;             //是否打印RTSP Response的错误信息?
        Bool16  fEnableRTSPDebugPrintfs;       //是否打印RTSP协议的调试信息?
        Bool16  fEnableRTSPServerInfo;         //是否在RTSP Response中放入服务器信息
        UInt32  fNumThreads;                   //指定任务线程的个数,若为0,则一个CPU核一个任务线程
        Bool16  fEnableMonitorStatsFile;       //是否使用状态监测文件?用于外部监测模块
        UInt32  fStatsFileIntervalSeconds;     //更新状态监测文件的时间间隔(s)
	
		Float32	fOverbufferRate;               //缓冲率

		UInt32  fSmallWindowSizeInK;           //Client窗口大小(小),单位Kbytes
		UInt32	fMediumWindowSizeInK;          //Client窗口大小(中),单位Kbytes
		UInt32  fLargeWindowSizeInK;           //Client窗口大小(大),单位Kbytes
		UInt32  fWindowSizeThreshold;          //使用Client小窗口的带宽阈值(kbps)
		UInt32	fWindowSizeMaxThreshold;       //使用Client大窗口的带宽阈值(kbps)
		
        Bool16  fEnablePacketHeaderPrintfs;    //是否打印RTP/RTCP包头? used in QTSSFileModule::SendPackets() 
        UInt32  fPacketHeaderPrintfOptions;    //打印RTP/RTCP包头的类型
        Bool16  fCloseLogsOnWrite;             // 每次写后强迫日志文件关闭吗? 
        
        Bool16  fDisableThinning;              //是否使用服务器薄化算法?注意在streamingserver.xml中没有!!
		Bool16  fauto_delete_sdp_files;        //在t=endtime过后,SDP文件是否删除?
		UInt32  fsdp_file_delete_interval_seconds;//检查SDP文件的间隔(s)

		QTSS_AuthScheme fAuthScheme;           //认证格式

        enum //fPacketHeaderPrintfOptions
        {
            kRTPALL = 1 << 0,
            kRTCPSR = 1 << 1,
            kRTCPRR = 1 << 2,
            kRTCPAPP = 1<< 3,
            kRTCPACK = 1<< 4
        };
        
        enum
        {
            kAllowMultipleValues     = 1,
            kDontAllowMultipleValues = 0
        };
        
        struct PrefInfo
        {
            UInt32  fAllowMultipleValues;// 值为0或1,0表示不允许多值,1反之,定义参见上面
            char*   fDefaultValue;       // 预设的默认值
            char**  fAdditionalDefVals;  // 预设的额外值,For prefs with multiple default values
        };
            
        void SetupAttributes();
        void UpdateAuthScheme();
        void UpdatePrintfOptions();
        
        // Returns the string preference with the specified ID. If there
        // was any problem, this will return an empty string.
		// 返回指定ID的字符串形式的默认值
        char* GetStringPref(QTSS_AttributeID inAttrID);
        
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];//服务器预设值字典属性
        static PrefInfo                     sPrefInfo[];  //服务器默认预设值信息
         
        static char*    sAdditionalDefaultPorts[];  // Prefs that have multiple default values (rtsp_ports) have to be dealt with specially,侦听端口字符默认数组值 
        static char*    sRTP_Header_Players[];      // Player prefs,播放器预设值
        static char*    sAdjust_Bandwidth_Players[];
       
};
#endif //__QTSSPREFS_H__
