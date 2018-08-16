/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPSession.h
Description: Represents an RTSP session (duh), which I define as a complete TCP connection
             lifetime, from connection to FIN or RESET termination.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#ifndef __RTSPSESSION_H__
#define __RTSPSESSION_H__

#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"
#include "RTSPRequest.h"
#include "RTPSession.h"
#include "TimeoutTask.h"

class RTSPSession : public RTSPSessionInterface
{
    public:

        RTSPSession(Bool16 doReportHTTPConnectionAddress);//由入参确定,是否对Client报告流服务器的ip地址?
        virtual ~RTSPSession();
        
        // Call this before using this object
        static void Initialize();

		//see if playing due to the RTP session status
		/* used in RTSPRequest::ParseURI() */
        Bool16 IsPlaying() {if (fRTPSession == NULL) return false; if (fRTPSession->GetSessionState() == qtssPlayingState) return true; return false; }
        
        
    private:

		/***************** 非常重要 *************************/
        SInt64 Run();
        
		/*************  RTPSession related ***************/
        // Gets & creates RTP session for this request.
        QTSS_Error  FindRTPSession(OSRefTable* inTable);
		/* 在恰当时候生成一个RTPSession,配置会话属性,分配一个随机的唯一的Session ID,注册并加入RTPSessionMap入参inRefTable中,并更新RTPSession总数 */
        QTSS_Error  CreateNewRTPSession(OSRefTable* inTable);
        void        SetupClientSessionAttrs();
        
        // Does request prep & request cleanup, respectively
		/* used to create RTPSession Object for clients  */
        void SetupRequest();
        void CleanupRequest();
        
		Bool16 ParseOptionsResponse();
		
        // Fancy random number generator
        UInt32 GenerateNewSessionID(char* ioBuffer);
        
        // Sends an error response & returns error if not ok.
		/* 判断服务器能否假如新的RTPSession ? */
        QTSS_Error IsOkToAddNewRTPSession();
	    QTSS_Error DumpRequestData();
        
        // Checks authentication parameters
        void CheckAuthentication();
        
        // test current connections handled by this object against server pref connection limit
        Bool16 OverMaxConnections(UInt32 buffer);


		/* 描述RTPSession的ID字符串和指针,参见RTSPSession::CreateNewRTPSession() */
        char                fLastRTPSessionID[QTSS_MAX_SESSION_ID_LENGTH]; //32
        StrPtrLen           fLastRTPSessionIDPtr;

		/* 从RTSPRequsetStream::ReadRequest()读出RTSP Request后,创建一个new RTSPRequest对象,一般解析各种信息 */
        RTSPRequest*        fRequest;
		/* 从一个RTSPSession派生出的RTPSession对象数组 */
        RTPSession*         fRTPSession;
        
    
    /* -- begin adds for HTTP ProxyTunnel -- */

    // This gets grabbed whenever the input side of the session is being used.
    // It is to protect POST snarfage while input stuff is in action
    OSMutex             fReadMutex;
    
    OSRef*              RegisterRTSPSessionIntoHTTPProxyTunnelMap(QTSS_RTSPSessionType inSessionType);
    QTSS_Error          PreFilterForHTTPProxyTunnel();              // prefilter for HTTP proxies
    Bool16              ParseProxyTunnelHTTP();                     // use by PreFilterForHTTPProxyTunnel
    void                HandleIncomingDataPacket();
        
    static              OSRefTable* sHTTPProxyTunnelMap;    // a map of available partners.

    enum
    {
        kMaxHTTPResponseLen = 300
    };
	/* used in RTSPSession::Initialize() */
    static              char        sHTTPResponseHeaderBuf[kMaxHTTPResponseLen];
    static              StrPtrLen   sHTTPResponseHeaderPtr;
    
    static              char        sHTTPResponseNoServerHeaderBuf[kMaxHTTPResponseLen];
    static              StrPtrLen   sHTTPResponseNoServerHeaderPtr;
    
	/* 参见.cpp开头定义 */
    static              char        *sHTTPResponseFormatStr;
    static              char        *sHTTPNoServerResponseFormatStr;
    char                fProxySessionID[QTSS_MAX_SESSION_ID_LENGTH];    // our magic cookie to match proxy connections
    StrPtrLen           fProxySessionIDPtr;
    OSRef               fProxyRef; //当前Proxy Ref

    enum
    {
        // the kinds of HTTP Methods we're interested in for
        // RTSP tunneling
          kHTTPMethodInit       // initialize to this
        , kHTTPMethodUnknown    // tested, but unknown
        , kHTTPMethodGet        // found one of these methods...
        , kHTTPMethodPost
    }; 
    UInt16      fHTTPMethod;

    Bool16      fWasHTTPRequest;
    Bool16      fFoundValidAccept;
    Bool16      fDoReportHTTPConnectionAddress; // true if we need to report our IP adress in reponse to the clients GET request (necessary for servers behind DNS round robin)
    /* -- end adds for HTTP ProxyTunnel -- */
    
    
        // Module invocation and module state.
        // This info keeps track of our current state so that
        // the state machine works properly.
	    /* 状态机的状态 */
        enum
        {
			//RTSPSession 的基本状态
            kReadingRequest             = 0,
            kFilteringRequest           = 1,
            kRoutingRequest             = 2,
            kAuthenticatingRequest      = 3,
            kAuthorizingRequest         = 4,
            kPreprocessingRequest       = 5,
            kProcessingRequest          = 6,
            kSendingResponse            = 7,
            kPostProcessingRequest      = 8,
            kCleaningUp                 = 9,
        
        // states that RTSP sessions that setup RTSP
        // through HTTP tunnels pass through
		//当RTSP 协议通过HTTP 隧道实现时将用到下面的状态
            kWaitingToBindHTTPTunnel = 10,                  // POST or GET side waiting to be joined with it's matching half
            kSocketHasBeenBoundIntoHTTPTunnel = 11,         // POST side after attachment by GET side ( its dying )
            kHTTPFilteringRequest = 12,                     // after kReadingRequest, enter this state
            kReadingFirstRequest = 13,                      // 状态机初始态initial state - the only time we look for an HTTP tunnel
            kHaveNonTunnelMessage = 14                  // we've looked at the message, and its not an HTTP tunnle message
        };
        
		/* RTSP Session state machine, def see enum above  */
		/* RTSPSession::RTSPSession()中初始化为kReadingFirstRequest */
        UInt32 fState;

		/* 当前Module序号,参见RTSPSession::HandleIncomingDataPacket() */
		UInt32 fCurrentModule;
        QTSS_RoleParams     fRoleParams;//module param blocks for roles.
        QTSS_ModuleState    fModuleState;
        
        QTSS_Error SetupAuthLocalPath(RTSPRequest *theRTSPRequest);
        
        
        void SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest);

};
#endif // __RTSPSESSION_H__

