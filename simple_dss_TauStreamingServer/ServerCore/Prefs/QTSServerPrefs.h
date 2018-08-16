
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
        // This function sets up the dictionary map. Must be called before instantiating(ʵ����)
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
        
        //This is the value we advertise to clients (lower than the real one,����ʵֵС)
        UInt32  GetRTSPTimeoutInSecs()  { return fRTSPTimeoutInSecs; }
        UInt32  GetRTPTimeoutInSecs()   { return fRTPTimeoutInSecs; }
        StrPtrLen*  GetRTSPTimeoutAsString() { return &fRTSPTimeoutString; }  
        //This is the real timeout
        UInt32  GetRealRTSPTimeoutInSecs(){ return fRealRTSPTimeoutInSecs; }
        
        //-1 means unlimited
        SInt32  GetMaxConnections()         { return fMaximumConnections; }
        SInt32  GetMaxKBitsBandwidth()      { return fMaxBandwidthInKBits; }
        
        // Thinning algorithm parameters �ݻ��㷨����
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

        
    private: //58��Ԥ��ֵ

        UInt32      fRTSPTimeoutInSecs;        //�ϵ�Idle RTSP Client�ĵȴ�ʱ��(s)
        char        fRTSPTimeoutBuf[20];
        StrPtrLen   fRTSPTimeoutString; 
        UInt32      fRealRTSPTimeoutInSecs;    //��Client����RTSP Response�ĳ�ʱֵ
        UInt32      fRTPTimeoutInSecs;         //�ϵ�Idle RTP Client�ĵȴ�ʱ��(s)
        
        SInt32  fMaximumConnections;           //���������
        SInt32  fMaxBandwidthInKBits;          //������(kilobits/sec)

		SInt32  fDropAllPacketsTimeInMsec;     //�������а�����ʱֵ(ms)
		SInt32  fDropAllVideoPacketsTimeInMsec;//����������Ƶ������ʱֵ(ms)
		SInt32  fThinAllTheWayTimeInMsec;      //һֱ����������ʱֵ(ms),thin to key frames
		SInt32  fAlwaysThinTimeInMsec;         //ż������������ʱֵ(ms),we always start to thin at this point
		SInt32  fStartThinningTimeInMsec;      //���ܱ���������ʱֵ(ms),lateness at which we might start thinning
		SInt32  fStartThickingTimeInMsec;      //��ʼ��������ʱֵ(ms),maybe start thicking at this point
		SInt32  fThickAllTheWayTimeInMsec;     //�ָ�ȫ����������ĳ�ǰֵ(ms)
		UInt32  fQualityCheckIntervalInMsec;   //����������ȼ�(�����ʱ�̶�)�ļ��(ms),adjust thinnning params this often

		UInt32  fMinTCPBufferSizeInBytes;      //TCP���ͻ������Сֵ
		UInt32  fMaxTCPBufferSizeInBytes;      //TCP���ͻ�������ֵ
		Float32 fTCPSecondsToBuffer;           //TCP���ͻ���Ļ���ʱ��(s)

		Bool16  fDoReportHTTPConnectionAddress;//�Ƿ��Client��������������ip��ַ?
		Bool16  fAppendSrcAddrInTransport;     //��Transportͷ���Ƿ񸽼�����������ip��ַ?
        Bool16  fBreakOnAssert;                //�����Գ���ʱ�ж�
        Bool16  fAutoRestart;                  //�Ƿ��ڱ������Զ�����?
        UInt32  fTBUpdateTimeInSecs;           //���������ֽ����͵�ǰ�����ͳ�Ƶĸ��¼��(s)
        UInt32  fABUpdateTimeInSecs;           //������ƽ�������ͳ�Ƶĸ��¼��(s)
        UInt32  fSafePlayDurationInSecs;       //Client���ᱻ�ϵ��İ�ȫ����ʱ�� 
        
		Bool16  fErrorLogEnabled;              //�Ƿ��¼������־?
		Bool16  fScreenLoggingEnabled;         //�Ƿ��ô�����־������ʾ?
        UInt32  fErrorLogBytes;                //������־���ļ���С(�ֽ�)
		UInt32  fErrorRollIntervalInDays;      //������־�Ļع����(��)
        UInt32  fErrorLogVerbosity;            //��ǰ������־�ļ���
        
        UInt32  fMaxRetransDelayInMsec;        //����ش���ʱ(�趨���ش�ʱ���ʵ���ش�ʱ��Ĳ�), needed by RTPSession::run()
        Bool16  fIsAckLoggingEnabled;          //�Ƿ���յ�RUDP Ack���ĵ�����־?Debugging only: turns on detailed logging of UDP acks / retransmits
        UInt32  fRTCPPollIntervalInMsec;       //���������RTCP����ʱ����(ms)
        UInt32  fRTCPSocketRcvBufSizeInK;      //����������RTCP Ack����UDPSocket�����С(Kbytes)
        UInt32  fSendIntervalInMsec;           //�������Ͱ�����С���(ms), needed by RTPSession::run() 
        UInt32  fMaxSendAheadTimeInSecs;       //�����������ǰ�Ͱ�ʱ��(25s)
		Bool16  fIsSlowStartEnabled;           //RUDP�Ƿ�����������?
        Bool16  fAutoStart;                    //�Ƿ񿪻��Զ�����?ע����streamingserver.xml��û��!!If true, streaming server likes to be started at system startup
        Bool16  fReliableUDP;                  //�Ƿ����RUDP?
        Bool16  fReliableUDPPrintfs;           //�Ƿ�ʹ��RUDP��ӡ?
        Bool16  fEnableRTSPErrMsg;             //�Ƿ��ӡRTSP Response�Ĵ�����Ϣ?
        Bool16  fEnableRTSPDebugPrintfs;       //�Ƿ��ӡRTSPЭ��ĵ�����Ϣ?
        Bool16  fEnableRTSPServerInfo;         //�Ƿ���RTSP Response�з����������Ϣ
        UInt32  fNumThreads;                   //ָ�������̵߳ĸ���,��Ϊ0,��һ��CPU��һ�������߳�
        Bool16  fEnableMonitorStatsFile;       //�Ƿ�ʹ��״̬����ļ�?�����ⲿ���ģ��
        UInt32  fStatsFileIntervalSeconds;     //����״̬����ļ���ʱ����(s)
	
		Float32	fOverbufferRate;               //������

		UInt32  fSmallWindowSizeInK;           //Client���ڴ�С(С),��λKbytes
		UInt32	fMediumWindowSizeInK;          //Client���ڴ�С(��),��λKbytes
		UInt32  fLargeWindowSizeInK;           //Client���ڴ�С(��),��λKbytes
		UInt32  fWindowSizeThreshold;          //ʹ��ClientС���ڵĴ�����ֵ(kbps)
		UInt32	fWindowSizeMaxThreshold;       //ʹ��Client�󴰿ڵĴ�����ֵ(kbps)
		
        Bool16  fEnablePacketHeaderPrintfs;    //�Ƿ��ӡRTP/RTCP��ͷ? used in QTSSFileModule::SendPackets() 
        UInt32  fPacketHeaderPrintfOptions;    //��ӡRTP/RTCP��ͷ������
        Bool16  fCloseLogsOnWrite;             // ÿ��д��ǿ����־�ļ��ر���? 
        
        Bool16  fDisableThinning;              //�Ƿ�ʹ�÷����������㷨?ע����streamingserver.xml��û��!!
		Bool16  fauto_delete_sdp_files;        //��t=endtime����,SDP�ļ��Ƿ�ɾ��?
		UInt32  fsdp_file_delete_interval_seconds;//���SDP�ļ��ļ��(s)

		QTSS_AuthScheme fAuthScheme;           //��֤��ʽ

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
            UInt32  fAllowMultipleValues;// ֵΪ0��1,0��ʾ�������ֵ,1��֮,����μ�����
            char*   fDefaultValue;       // Ԥ���Ĭ��ֵ
            char**  fAdditionalDefVals;  // Ԥ��Ķ���ֵ,For prefs with multiple default values
        };
            
        void SetupAttributes();
        void UpdateAuthScheme();
        void UpdatePrintfOptions();
        
        // Returns the string preference with the specified ID. If there
        // was any problem, this will return an empty string.
		// ����ָ��ID���ַ�����ʽ��Ĭ��ֵ
        char* GetStringPref(QTSS_AttributeID inAttrID);
        
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];//������Ԥ��ֵ�ֵ�����
        static PrefInfo                     sPrefInfo[];  //������Ĭ��Ԥ��ֵ��Ϣ
         
        static char*    sAdditionalDefaultPorts[];  // Prefs that have multiple default values (rtsp_ports) have to be dealt with specially,�����˿��ַ�Ĭ������ֵ 
        static char*    sRTP_Header_Players[];      // Player prefs,������Ԥ��ֵ
        static char*    sAdjust_Bandwidth_Players[];
       
};
#endif //__QTSSPREFS_H__
