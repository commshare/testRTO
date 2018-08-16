
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPSession.cpp
Description: Represents an RTSP session (duh), which I define as a complete TCP connection
             lifetime, from connection to FIN or RESET termination.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2011-07-03

****************************************************************************/ 



#define __RTSP_HTTP_DEBUG__ 1
#define __RTSP_HTTP_VERBOSE__ 1
#define __RTSP_AUTHENTICATION_DEBUG__ 1

#include "RTSPSession.h"
#include "RTSPRequest.h"
#include "QTSS.h"
#include "QTSSModuleUtils.h"
#include "QTSServerInterface.h"

#include "MyAssert.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"

#include "UserAgentParser.h"
#include "base64.h"
#include "md5digest.h"

#include <unistd.h>
#include <errno.h>
#include <crypt.h>



#if __RTSP_HTTP_DEBUG__
    #define HTTP_TRACE(s) qtss_printf(s);
    #define HTTP_TRACE_SPL(s) PrintfStrPtrLen(s);
    #define HTTP_TRACE_ONE(s, one ) qtss_printf(s, one);
    #define HTTP_TRACE_TWO(s, one, two ) qtss_printf(s, one, two);
#else
    #define HTTP_TRACE(s);
    #define HTTP_TRACE_SPL(s);
    #define HTTP_TRACE_ONE(s, one );
    #define HTTP_TRACE_TWO(s, one, two );
#endif

#if __RTSP_HTTP_VERBOSE__
    #define HTTP_VTRACE(s) qtss_printf(s);
    #define HTTP_VTRACE_SPL(s) PrintfStrPtrLen(s);
    #define HTTP_VTRACE_ONE(s, one ) qtss_printf(s, one);
    #define HTTP_VTRACE_TWO(s, one, two ) qtss_printf(s, one, two);
#else
    #define HTTP_VTRACE(s);
    #define HTTP_VTRACE_SPL(s);
    #define HTTP_VTRACE_ONE(s, one );
    #define HTTP_VTRACE_TWO(s, one, two );
#endif



#if  __RTSP_HTTP_DEBUG__ || __RTSP_HTTP_VERBOSE__

/*  ��ӡָ����StrPtrLen  */
static void PrintfStrPtrLen( StrPtrLen *splRequest )
{
    char    buff[1024];  
    memcpy( buff, splRequest->Ptr, splRequest->Len );   
    buff[ splRequest->Len] = 0;  
    HTTP_TRACE_ONE( "%s\n", buff )
    //qtss_printf( "%s\n", buff );
}
#endif

//hack stuff
//qtssClientSessionObjectType see RTSPSession::RTSPSession()
static char*                    sBroadcasterSessionName="QTSSReflectorModuleBroadcasterSession";
static QTSS_AttributeID         sClientBroadcastSessionAttr =   qtssIllegalAttrID;


static StrPtrLen    sVideoStr("video");
static StrPtrLen    sAudioStr("audio");
static StrPtrLen    sRtpMapStr("rtpmap");
static StrPtrLen    sControlStr("control");
static StrPtrLen    sBufferDelayStr("x-bufferdelay");
static StrPtrLen    sContentType("application/x-random-data");

static StrPtrLen    sAuthAlgorithm("md5");
static StrPtrLen    sAuthQop("auth");// Quality of protection(Qop)
static StrPtrLen    sEmptyStr("");

// static class member  initialized in RTSPSession ctor
OSRefTable* RTSPSession::sHTTPProxyTunnelMap = NULL;

char        RTSPSession::sHTTPResponseHeaderBuf[kMaxHTTPResponseLen];//300
StrPtrLen   RTSPSession::sHTTPResponseHeaderPtr(sHTTPResponseHeaderBuf, kMaxHTTPResponseLen);

char        RTSPSession::sHTTPResponseNoServerHeaderBuf[kMaxHTTPResponseLen];//300
StrPtrLen   RTSPSession::sHTTPResponseNoServerHeaderPtr(sHTTPResponseNoServerHeaderBuf, kMaxHTTPResponseLen);

// stock reponse with place holder for server header and optional "x-server-ip-address" header ( %s%s%s for  "x-server-ip-address" + ip address + \r\n )
// the optional version must be generated at runtime to include a valid IP address for the actual interface
char*       RTSPSession::sHTTPResponseFormatStr =  "HTTP/1.0 200 OK\r\n%s%s%s%s\r\nConnection: close\r\nDate: Thu, 19 Aug 1982 18:30:00 GMT\r\nCache-Control: no-store\r\nPragma: no-cache\r\nContent-Type: application/x-rtsp-tunnelled\r\n\r\n";
char*       RTSPSession::sHTTPNoServerResponseFormatStr =  "HTTP/1.0 200 OK\r\n%s%s%s%sConnection: close\r\nDate: Thu, 19 Aug 1982 18:30:00 GMT\r\nCache-Control: no-store\r\nPragma: no-cache\r\nContent-Type: application/x-rtsp-tunnelled\r\n\r\n";

void RTSPSession::Initialize()
{
	/* ����http proxy tunnel map */
    sHTTPProxyTunnelMap = new OSRefTable(OSRefTable::kDefaultTableSize);

    // Construct premade HTTP response for HTTP proxy tunnel
	/* ����sHTTPResponseHeaderBuf���� */
    qtss_sprintf(sHTTPResponseHeaderBuf, sHTTPResponseFormatStr, "","","", QTSServerInterface::GetServerHeader().Ptr);
    sHTTPResponseHeaderPtr.Len = ::strlen(sHTTPResponseHeaderBuf);
    Assert(sHTTPResponseHeaderPtr.Len < kMaxHTTPResponseLen);
    
    /* ����sHTTPResponseNoServerHeaderBuf���� */
    qtss_sprintf(sHTTPResponseNoServerHeaderBuf, sHTTPNoServerResponseFormatStr, "","","","");
    sHTTPResponseNoServerHeaderPtr.Len = ::strlen(sHTTPResponseNoServerHeaderBuf);
    Assert(sHTTPResponseNoServerHeaderPtr.Len < kMaxHTTPResponseLen);
        
}

/* used in RTSPListenerSocket::GetSessionTask() */
// this inParam tells the server to report its IP address in the reply to the HTTP GET request when tunneling RTSP through HTTP
RTSPSession::RTSPSession( Bool16 doReportHTTPConnectionAddress )
: RTSPSessionInterface(),
  fRequest(NULL),/* ������RTSPSession::Run() */
  fRTPSession(NULL),/* ��û����RTPSession,������RTSPSession::SetupRequest() */
  fReadMutex(),
  fHTTPMethod( kHTTPMethodInit ),
  fWasHTTPRequest( false ),
  fFoundValidAccept( false),
  fDoReportHTTPConnectionAddress(doReportHTTPConnectionAddress),//�����ȷ��,�Ƿ��Client��������������ip��ַ?
  fCurrentModule(0),
  fState(kReadingFirstRequest) /* RTSPSession��״̬���ĳ�ʼ״̬,ע��˴���ֵ��RTSPSession::Run()��Ҫʹ�� */
{
    this->SetTaskName("RTSPSession");

    // must guarantee this map is present
    Assert(sHTTPProxyTunnelMap != NULL);
    
	/* ���µ�ǰRTSPSession����,����1 */
    QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(1);

    // Setup the QTSS Role param block, as none of these fields will change through the course of this session.
	//��Ự������,����������RTSPSession�в���
    fRoleParams.rtspRequestParams.inRTSPSession = this;
    fRoleParams.rtspRequestParams.inRTSPRequest = NULL;
    fRoleParams.rtspRequestParams.inClientSession = NULL;/* ����RTPSession */
    
	//setup QTSS_ModuleState
    fModuleState.curModule = NULL;
    fModuleState.curTask = this;
    fModuleState.curRole = 0;
    fModuleState.globalLockRequested = false;
       
	//��һ���ַ���Ϊ0
    fProxySessionID[0] = 0;
    fProxySessionIDPtr.Set( fProxySessionID, 0 );//����Proxy Session ID

	//��һ���ַ���Ϊ0,ע��fLastRTPSessionID����,�μ�RTSPSession::CreateNewRTPSession()
    fLastRTPSessionID[0] = 0;
    fLastRTPSessionIDPtr.Set( fLastRTPSessionID, 0 );
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);
                    
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sBroadcasterSessionName, &sClientBroadcastSessionAttr);

}

/* ����rtspSessionClosingParams����,������ע��QTSSModule::kRTSPSessionClosingRole��ɫ��ģ��,ʹ��ǰRTSPSession������1,��sHTTPProxyTunnelMap��ɾ��fProxyRef */
RTSPSession::~RTSPSession()
{
    // Invoke the session closing modules
    QTSS_RoleParams theParams;
    theParams.rtspSessionClosingParams.inRTSPSession = this;
    
    // Invoke modules
	//����ע��QTSSModule::kRTSPSessionClosingRole��ɫ��ģ��
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPSessionClosingRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kRTSPSessionClosingRole, x)->CallDispatch(QTSS_RTSPSessionClosing_Role, &theParams);

    this->CleanupRequest();// Make sure that all our objects are deleted

	/* ���µ�ǰRTSPSession����,��1 */
    if (fSessionType == qtssRTSPSession)
        QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(-1);
    else
        QTSServerInterface::GetServer()->AlterCurrentRTSPHTTPSessionCount(-1);
    
	/* ��sHTTPProxyTunnelMap��ɾ��fProxyRef */
    if ( *fProxySessionID != '\0')
    {
#if DEBUG
        char * str = "???";
        
        if ( fSessionType == qtssRTSPHTTPInputSession )
            str = "input session";
        else if ( fSessionType == qtssRTSPHTTPSession )
            str = "input session";
        
        HTTP_VTRACE_TWO( "~RTSPSession, was a fProxySessionID (%s), %s\n", fProxySessionID, str )
#endif      
        sHTTPProxyTunnelMap->UnRegister( &fProxyRef );  
    }
}


/* ���������Ҫ�������Modules�򽻵�. */
SInt64 RTSPSession::Run()
{
	//ȡ���¼�
    EventFlags events = this->GetEvents();
    QTSS_Error err = QTSS_NoErr;

	/* �����״̬���л��õ�Module */

	/************** NOTE ����*********************/
	/* �ض�Module��ָ�룬��������SendPacket��Module */
    QTSSModule* theModule = NULL;
	/* Module���� */
    UInt32 numModules = 0;

	/* ȷ��������������һ���ַ���,���RTSPSession::RTSPSession( Bool16 doReportHTTPConnectionAddress ) */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);
    // Some callbacks look for this struct in the thread object
	// �趨��ǰ��Module ״̬
    OSThreadDataSetter theSetter(&fModuleState, NULL);
        
    //check for a timeout or a kill. If so, just consider the session dead
    if ((events & Task::kTimeoutEvent) || (events & Task::kKillEvent))
        fLiveSession = false;
    
	//�����live session��ִ��״̬��
    while (this->IsLiveSession())
    {
		/* ��ʾ��������RTSPSession ��״̬������Ϊ�ڴ���RTSP ��������У���
		����ط���ҪRun ���������Ա���������µ��¼���Ϊ�ˣ�������Ҫ���ٵ�ǰ
		������״̬���Ա��ڱ���Ϻ��ܻص�ԭ״̬*/
        // RTSP Session state machine. There are several well defined points in an RTSP request
        // where this session may have to return from its run function and wait for a new event.
        // Because of this, we need to track our current state and return to it.
        
        switch (fState)
        {
			/* ��һ�����󵽴����kReadingFirstRequest ״̬����״̬��Ҫ�����
			RTSPRequestStream ��Ķ���fInputStream �ж����ͻ���RTSP ���� */
            case kReadingFirstRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kReadingFirstRequest\n" )
                if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
                {
					/* RequestStream����QTSS_NoErr ��ζ�����������Ѿ�
					 ��Socket �ж��������в��ܹ���һ��������������˱���ȴ���������ݵ��� */

                    // If the RequestStream returns QTSS_NoErr, it means
                    // that we've read all outstanding data off the socket,
                    // and still don't have a full request. Wait for more data.
                    
                    //+rt use the socket that reads the data, may be different now.
					//��û�еõ�������RTSP request,�������������
                    fInputSocketP->RequestEvent(EV_RE);
					//Run�������أ��ȴ���һ���¼�����
                    return 0;
                }
                
				//����ֹͣ����
                if ((err != QTSS_RequestArrived) && (err != E2BIG))
                {
                    // Any other error implies that the client has gone away. At this point,
                    // we can't have 2 sockets, so we don't need to do the "half closed" check
                    // we do below
                    Assert(err > 0); 
                    Assert(!this->IsLiveSession());
                    break;
                }

				//RTSP�����Ѿ���ȫ���ת��kHTTPFilteringRequest ״̬
                if (err == QTSS_RequestArrived)
                    fState = kHTTPFilteringRequest;

				//���ջ����������ת��kHaveNonTunnelMessage ״̬
                // If we get an E2BIG, it means our buffer was overfilled.
                // In that case, we can just jump into the following state, and
                // the code their does a check for this error and returns an error.
                if (err == E2BIG)
                    fState = kHaveNonTunnelMessage;
            }
            continue;//��������
            
			/* ��������£��ڻ��һ��������RTSP�����,ϵͳ������
			kHTTPFilteringRequest״̬.��״̬���RTSP�����Ƿ���Ҫ����HTTP����ʵ
			�֣��粻��Ҫ��ת��kHaveNonTunnelMessage ״̬�� */
            case kHTTPFilteringRequest:
            {    
                HTTP_TRACE( "RTSPSession::Run kHTTPFilteringRequest\n" )
            
                fState = kHaveNonTunnelMessage; // assume it's not a tunnel setup message
                                                // prefilter will set correct tunnel state if it is.

                QTSS_Error  preFilterErr = this->PreFilterForHTTPProxyTunnel();
                                
                if ( preFilterErr == QTSS_NoErr )
                {   
                    HTTP_TRACE( "RTSPSession::Run kHTTPFilteringRequest over\n" )
                    continue;
                }
                else
                {   
                    // pre filter error indicates a tunnelling message that could 
                    // not join to a session.
                    HTTP_TRACE( "RTSPSession::Run kHTTPFilteringRequest Tunnel protocol ERROR.\n" )
                    return -1;
                    
                }
            }
            
            case kWaitingToBindHTTPTunnel:

				HTTP_TRACE( "RTSPSession::Run kWaitingToBindHTTPTunnel\n" )
                //flush the GET response, if it's there
                err = fOutputStream.Flush();
                if (err == EAGAIN)
                {
                    // If we get this error, we are currently flow-controlled and should
                    // wait for the socket to become writeable again
                    fSocket.RequestEvent(EV_WR);
                }
                return 0;
                //continue;
            
            case kSocketHasBeenBoundIntoHTTPTunnel:

                HTTP_TRACE( "RTSPSession::Run kSocketHasBeenBoundIntoHTTPTunnel\n" )
                // DMS - Can this execute either? I don't think so... this one
                // we may not need...
                
                // I've been joined, it's time to kill this session.
                Assert(!this->IsLiveSession()); // at least the socket should not report connected any longer
                HTTP_TRACE( "RTSPSession has died of snarfage.\n" )
                break;
                
            /* �ǵ�һ�ζ�ȡRTSP Request������ */
            case kReadingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kReadingRequest\n" )
				/* ����ȡdataʱҪ����,�Է�POST����� */
                // We should lock down the session while reading in data,
                // because we can't snarf up a POST while reading.
                OSMutexLocker readMutexLocker(&fReadMutex);

                // we should be only reading an RTSP request here, no HTTP tunnel messages
                
                if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
                {
					/* ��ȡ���ݺ�û�еõ�������RTSP Request,�������������ȡ���� */
                    // If the RequestStream returns QTSS_NoErr, it means
                    // that we've read all outstanding data off the socket,
                    // and still don't have a full request. Wait for more data.
                    
                    //+rt use the socket that reads the data, may be different now.
					/* �ٴ������event */
                    fInputSocketP->RequestEvent(EV_RE);
                    return 0;
                }
                
				/* ����,�ϵ����� */
                if ((err != QTSS_RequestArrived) && (err != E2BIG) && (err != QTSS_BadArgument))
                {
                    //Any other error implies that the input connection has gone away.
                    // We should only kill the whole session if we aren't doing HTTP.
                    // (If we are doing HTTP, the POST connection can go away)
                    Assert(err > 0);
                    if (fOutputSocketP->IsConnected())
                    {
                        // If we've gotten here, this must be an HTTP session with
                        // a dead input connection. If that's the case, we should
                        // clean up immediately so as to not have an open socket
                        // needlessly lingering around, taking up space.
                        Assert(fOutputSocketP != fInputSocketP);
                        Assert(!fInputSocketP->IsConnected());
						/* �ر�socket,��event queue������� */
                        fInputSocketP->Cleanup();
                        return 0;
                    }
                    else
                    {  //����һ��dead session
                        Assert(!this->IsLiveSession());
                        break;
                    }
                }
                fState = kHaveNonTunnelMessage;
                // fall thru to kHaveNonTunnelMessage
            }
            
			/* ����kHaveNonTunnelMessage ״̬��ϵͳ������RTSPRequest ��Ķ���
			fRequest���ö�������ͻ���RTSP ���󣬲�����������ԡ�fRequest ���󱻴�
			�ݸ�����״̬���� */
            case kHaveNonTunnelMessage:
            {   
				HTTP_TRACE( "RTSPSession::Run kHaveNonTunnelMessage\n" )
                // should only get here when fInputStream has a full message built.
                /* �õ�һ��RTSP request buffer����ʼ��ַ */
                Assert( fInputStream.GetRequestBuffer() );
                
                Assert(fRequest == NULL);
				/* ����RTSPRequestʵ������ */
                fRequest = NEW RTSPRequest(this);
                fRoleParams.rtspRequestParams.inRTSPRequest = fRequest;
				/* ��ȡRTSPRequest�ֵ� */
                fRoleParams.rtspRequestParams.inRTSPHeaders = fRequest->GetHeaderDictionary();

				/* ���ڶ�full RTSP Request���д���,�����Է�������̱����,ֱ�����Ƿ���Response */
                // We have an RTSP request and are about to begin processing. We need to
                // make sure that anyone sending interleaved data on this session won't
                // be allowed to do so until we are done sending our response
                // We also make sure that a POST session can't snarf in while we're
                // processing the request.
				/* ��ֹPOST snarf ��� */
                fReadMutex.Lock();
				/* ��ֹRTPSession��� */
                fSessionMutex.Lock();
                
				/* ��RTSP Response��д���ֽ���������Ϊ0,����һ��RTSP Response����ʱ�����¼��� */
                // The fOutputStream's fBytesWritten counter is used to
                // count the #(����) of bytes for this RTSP response. So, at
                // this point, reset it to 0 (we can then just let it increment
                // until the next request comes in)
                fOutputStream.ResetBytesWritten();
                
                // Check for an overfilled buffer, and return an error.
				/* �����ջ������ʱ,����״̬��ΪkPostProcessingRequest */
                if (err == E2BIG)
                {
                    (void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest,
                                                                    qtssMsgRequestTooLong);
                    fState = kPostProcessingRequest;
                    break;
                }
                // Check for a corrupt base64 error, return an error
                if (err == QTSS_BadArgument)
                {
                    (void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest,
                                                                    qtssMsgBadBase64);
                    fState = kPostProcessingRequest;
                    break;
                }

				/* ȷ��һ��������RTSP Request����,ת��kFilteringRequest */
                Assert(err == QTSS_RequestArrived);
                fState = kFilteringRequest;
                
				/* ע������û�д��,���������������һ�� */
                // Note that there is no break here. We'd like to continue onto the next
                // state at this point. This goes for every case in this case statement
            }
            
			/* ���Ž���kFilteringRequest ״̬�����ο�����Ա����ͨ����дModule �Կ�
			���������������⴦������ͻ�������Ϊ������RTSP ����ϵͳ����
			SetupRequest �����������ڹ������ݴ����RTPSession ����� */
            case kFilteringRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kFilteringRequest\n" )
                // We received something so auto refresh
                // The need to auto refresh is because the api doesn't allow a module to refresh at this point
                // 
                fTimeoutTask.RefreshTimeout();/* �����Ѿ����յ�������Client������,����ˢ�³�ʱ,���������߳�ɾȥ��RTSPSession���� */

                //
                // Before we even do this, check to see if this is a *data* packet,
                // in which case this isn't an RTSP request, so we don't need to go
                // through any of the remaining steps
                /* ������յ������ݰ�,�ͽ���,�˳�״̬�� */
                if (fInputStream.IsDataPacket()) // can this interfere with MP3?
                {
                    this->HandleIncomingDataPacket();
                    fState = kCleaningUp;
                    break;
                }
                
                
                //
                // In case a module wants to replace the request
                char* theReplacedRequest = NULL;
                char* oldReplacedRequest = NULL;
                
                // Setup the filter param block
                QTSS_RoleParams theFilterParams;
                theFilterParams.rtspFilterParams.inRTSPSession = this;
                theFilterParams.rtspFilterParams.inRTSPRequest = fRequest;
                theFilterParams.rtspFilterParams.outNewRequest = &theReplacedRequest;
                
                // Invoke filter modules
				/* ͳ��ע��kRTSPFilterRole��Module���� */
                numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPFilterRole);
				/* �Ե�ǰע��ģ��,��û����RTSPResponseʱ��������eventʱ,�����д��� */
                for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                {
                    fModuleState.eventRequested = false;
                    fModuleState.idleTime = 0;
                    if (fModuleState.globalLockRequested )
                    {   
						fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    }
                    
					/* ��ȡָ����ŵ�ע��kRTSPFilterRole��Module */
					/**************** NOTE���� ********************************/
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPFilterRole, fCurrentModule);
					/* ���ø�module */
                    (void)theModule->CallDispatch(QTSS_RTSPFilter_Role, &theFilterParams);
                    fModuleState.isGlobalLocked = false;
                    
                    // If this module has requested an event, return and wait for the event to transpire
                    if (fModuleState.globalLockRequested) // call this request back locked
                        return this->CallLocked();
                            
                    if (fModuleState.eventRequested)
                    {   
                        this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                    // the same task thread to be used for next Run()
                        return fModuleState.idleTime; // If the module has requested idle time...
                    }
            
                    //
                    // Check to see if this module has replaced the request. If so, check
                    // to see if there is an old replacement that we should delete
                    if (theReplacedRequest != NULL)
                    {
                        if (oldReplacedRequest != NULL)
                            delete [] oldReplacedRequest;
                        
                        fRequest->SetVal(qtssRTSPReqFullRequest, theReplacedRequest, ::strlen(theReplacedRequest));
                        oldReplacedRequest = theReplacedRequest;
                        theReplacedRequest = NULL;
                    }
                    
                }
                
                fCurrentModule = 0;
				/* �����Ѿ�����Response,����������׶�;ע�ⷢ��Response��kSendingRequest�׶� */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }

				/* ��������Options request(����Ϊ"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"),�ͽ����յ���Options Response,����������ʱ�� */
				if (fSentOptionsRequest && this->ParseOptionsResponse())
				{
					/* ����Options���ͺͷ��ص�����ʱ�� */
					fRoundTripTime = (SInt32) (OS::Milliseconds() - fOptionsRequestSendTime);
					qtss_printf("RTSPSession::Run RTT time = %ld msec\n", fRoundTripTime);
					fState = kSendingResponse;
					break;
				}
				else
				/************************ NOTE Important !!  *****************************/
                // Otherwise, this is a normal RTSP request, so parse it and get the RTPSession.
                    this->SetupRequest();
                /************************ NOTE Important !!  *****************************/
                
                // This might happen if there is some syntax or other error,
                // or if it is an OPTIONS request
                /* �����ѷ���Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }

                fState = kRoutingRequest;
            }


			/* ����kRoutingRequest ״̬�����ö��ο�����Ա�����Module�����ڽ���
			����·�ɣ�Routing����ȥ��ȱʡ����£�ϵͳ����Դ�״̬�������� */
            case kRoutingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kRoutingRequest\n" )
                // Invoke router modules
                numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPRouteRole);
                {
                    // Manipulation of the RTPSession from the point of view of
                    // a module is guaranteed to be atomic by the API.
                    Assert(fRTPSession != NULL);
                    OSMutexLocker   locker(fRTPSession->GetSessionMutex());
                    
                    for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                    {  
                        fModuleState.eventRequested = false;
                        fModuleState.idleTime = 0;
                        if (fModuleState.globalLockRequested )
                        {   fModuleState.globalLockRequested = false;
                            fModuleState.isGlobalLocked = true;
                        } 
                        
						/* ��ȡָ����ŵ�ע��kRTSPRouteRole��Module */
                        theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPRouteRole, fCurrentModule);
						/* ���ø�module */
                        (void)theModule->CallDispatch(QTSS_RTSPRoute_Role, &fRoleParams);
                        fModuleState.isGlobalLocked = false;

                        if (fModuleState.globalLockRequested) // call this request back locked
                            return this->CallLocked();

                        // If this module has requested an event, return and wait for the event to transpire
                        if (fModuleState.eventRequested)
                        {
                            this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                        // the same thread to be used for next Run()
                            return fModuleState.idleTime; // If the module has requested idle time...
                        }
                    }
                }
                fCurrentModule = 0;
                
                // SetupAuthLocalPath must happen after kRoutingRequest and before kAuthenticatingRequest
                // placed here so that if the state is shifted to kPostProcessingRequest from a response being sent
                // then the AuthLocalPath will still be set.
				fRequest->SetupAuthLocalPath(); 
                
				/* ����Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }
                
				/* ��������Authorize�׶�,Ĭ��ҪAuthorize */
                if(fRequest->SkipAuthorization())
                {
                    // Skip the authentication and authorization states
                    
                    // The foll. normally gets executed at the end of the authorization state 
                    // Prepare for kPreprocessingRequest state.
                    fState = kPreprocessingRequest;

					/* ���統ǰRTSP method��SETUP */
                    if (fRequest->GetMethod() == qtssSetupMethod)
                    // Make sure to erase the session ID stored in the request at this point.
                    // If we fail to do so, this same session would be used if another
                    // SETUP was issued on this same TCP connection.
                        fLastRTPSessionIDPtr.Len = 0;
                    else if (fLastRTPSessionIDPtr.Len == 0)
                        fLastRTPSessionIDPtr.Len = ::strlen(fLastRTPSessionIDPtr.Ptr); 
                        
                    break;
                }
                else/* �������Authorize�׶� */
                    fState = kAuthenticatingRequest;
            }
            
			/* ����kAuthenticatingRequest ״̬�����ö��ο�����Ա����İ�ȫģ�飬��
			Ҫ���ڿͻ������֤�Լ����������Ĵ����������ϣ������������ҵ��;��
			��ʽý�����������ģ�������ж��ο����� */
            case kAuthenticatingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kAuthenticatingRequest\n" )
                /* �õ�client���͵�RTSP method */
                QTSS_RTSPMethod method = fRequest->GetMethod();
                if (method != qtssIllegalMethod) do  
                {   //Set the request action before calling the authentication module
                    /* ����request method��Announce����Setup�е�Record mode,��ΪW flag */
                    if((method == qtssAnnounceMethod) || ((method == qtssSetupMethod) && fRequest->IsPushRequest()))
                    {   fRequest->SetAction(qtssActionFlagsWrite);
                        break;
                    }
                    
                    void* theSession = NULL;
                    UInt32 theLen = sizeof(theSession);
					/* ����client�������broadcast session */
                    if (QTSS_NoErr == fRTPSession->GetValue(sClientBroadcastSessionAttr, 0,  &theSession, &theLen) )
                    {   fRequest->SetAction(qtssActionFlagsWrite); // an incoming broadcast session
                        break;
                    }

					/* ����RTSPRequestΪread flag */
                    fRequest->SetAction(qtssActionFlagsRead);
                } while (false);
                else
                {   Assert(0);
                }
                
				/* ����Authorize��������,��ô�ͻ�ȡ������Ԥ��ֵ�е����� */
                if(fRequest->GetAuthScheme() == qtssAuthNone)
                {
                    QTSS_AuthScheme scheme = QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme();
                    if( scheme == qtssAuthBasic)
                            fRequest->SetAuthScheme(qtssAuthBasic);
                    else if( scheme == qtssAuthDigest)
                            fRequest->SetAuthScheme(qtssAuthDigest);
                }
                
                // Setup the authentication param block
                QTSS_RoleParams theAuthenticationParams;
                theAuthenticationParams.rtspAthnParams.inRTSPRequest = fRequest;
            
                fModuleState.eventRequested = false;
                fModuleState.idleTime = 0;
                if (QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPAthnRole) > 0)
                {
                    if (fModuleState.globalLockRequested )
                    {   
                        fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    } 

					/* �õ�Authorize module */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPAthnRole, 0);
					/* ����Authorize module */
                    (void)theModule->CallDispatch(QTSS_RTSPAuthenticate_Role, &theAuthenticationParams);
                    fModuleState.isGlobalLocked = false;
                        
                    if (fModuleState.globalLockRequested) // call this request back locked
                        return this->CallLocked();

                    // If this module has requested an event, return and wait for the event to transpire
                    if (fModuleState.eventRequested)
                    {
                        this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                    // the same thread to be used for next Run()
                        return fModuleState.idleTime; // If the module has requested idle time...
                    }
                }
                
                this->CheckAuthentication();//�����֤�Ƿ�ͨ��?
                                                
                fCurrentModule = 0;
				/* �����ѷ���Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }
                fState = kAuthorizingRequest;
            }

            case kAuthorizingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kAuthorizingRequest\n" )
                // Invoke authorization modules
                numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPAuthRole);

                Bool16      allowed = true; 
                QTSS_Error  theErr = QTSS_NoErr;
                                
                // Invoke authorization modules
                
                // Manipulation of the RTPSession from the point of view of
                // a module is guaranteed to be atomic by the API.
                Assert(fRTPSession != NULL);
                OSMutexLocker   locker(fRTPSession->GetSessionMutex());

				/* ��������Authorize */
                fRequest->SetAllowed(allowed);                  
            
				/* ѭ������Module */
                for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                {
                    fModuleState.eventRequested = false;
                    fModuleState.idleTime = 0;
                    if (fModuleState.globalLockRequested )
                    {   fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    } 
                    
					/* �õ�Authorize module */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPAuthRole, fCurrentModule);
					/* ����Authorize module */
                    (void)theModule->CallDispatch(QTSS_RTSPAuthorize_Role, &fRoleParams);
                    fModuleState.isGlobalLocked = false;

                    if (fModuleState.globalLockRequested) // call this request back locked
                        return this->CallLocked();
                        
                    // If this module has requested an event, return and wait for the event to transpire
                    if (fModuleState.eventRequested)
                    {
                        this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                    // the same thread to be used for next Run()
                        return fModuleState.idleTime; // If the module has requested idle time...
                    }
                        
                    // If module has failed the request send a response and exit loop 
					/* ���������,allowedֵΪ��ֵtrue,Module������Ϊfalse,�˳�ѭ�� */
                    allowed = fRequest->GetAllowed();
                    
                    #if __RTSP_AUTHENTICATION_DEBUG__
                    {
                        UInt32 len = sizeof(Bool16);
                        theErr = fRequest->GetValue(qtssRTSPReqUserAllowed,0,  &allowed, &len);
                        qtss_printf("Module result for qtssRTSPReqUserAllowed = %d err = %ld \n",allowed,theErr);
                    }
                    #endif
                    
                    if (!allowed) 
                        break;

                }
                
				/* ����Authorize��֤���� */
                this->SaveRequestAuthorizationParams(fRequest);

                if (!allowed) 
                {   /* �ڷ���Responseǰ */
                    if (false == fRequest->HasResponseBeenSent())
                    {
						/* ��ȡclient quest��Authorize���� */
                        QTSS_AuthScheme challengeScheme = fRequest->GetAuthScheme();
                                                                
                        if(challengeScheme == qtssAuthBasic) {
                            fRTPSession->SetAuthScheme(qtssAuthBasic);
                            theErr = fRequest->SendBasicChallenge();
                        }
                        else if(challengeScheme == qtssAuthDigest) {
                            fRTPSession->UpdateDigestAuthChallengeParams(false, false, RTSPSessionInterface::kNoQop);
                            theErr = fRequest->SendDigestChallenge(fRTPSession->GetAuthQop(), fRTPSession->GetAuthNonce(), fRTPSession->GetAuthOpaque());
                        }
                        else {
                            // No authentication scheme is given and the request was not allowed,
                            // so send a 403: Forbidden message
                            theErr = fRequest->SendForbiddenResponse();
                        }

						/* ���緢��Authorize Response����,���жϸ�request */
                        if (QTSS_NoErr != theErr) // We had an error so bail on the request, quit the session and post process the request.
                        {   
                            fRequest->SetResponseKeepAlive(false);
                            fCurrentModule = 0;
                            fState = kPostProcessingRequest;
                            //if (fRTPSession) fRTPSession->Teardown(); // make sure the RTP Session is ended and logged
                            break;
                            
                        }                   
                    }
                    //if (fRTPSession) fRTPSession->Teardown(); // close RTP Session and log
                }
                    
                fCurrentModule = 0;
				/* �����Ѿ�������Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }

                // Prepare for kPreprocessingRequest state.
                fState = kPreprocessingRequest;

                if (fRequest->GetMethod() == qtssSetupMethod)
                    // Make sure to erase the session ID stored in the request at this point.
                    // If we fail to do so, this same session would be used if another
                    // SETUP was issued on this same TCP connection.
                    fLastRTPSessionIDPtr.Len = 0;
                 else if (fLastRTPSessionIDPtr.Len == 0)
                    fLastRTPSessionIDPtr.Len = ::strlen(fLastRTPSessionIDPtr.Ptr); 
            }
            
			/* ����kPreprocessingRequest ��kProcessingRequest ��kPostProcessingRequest
			״̬��������״̬����ͨ������ϵͳ�Դ�����ο�����Ա��ӵ�Module ������
			RTSP ��������ϵͳ�ṩ��QTSSReflector Module��QTSSSplitter Module �Լ�
			QTSSFileModule ��ģ�顣 */
            case kPreprocessingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kPreprocessingRequest\n" )
                // Invoke preprocessor modules
				/* �õ�ע��kRTSPPreProcessorRole��Module���� */
                numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPPreProcessorRole);
                {
                    // Manipulation of the RTPSession from the point of view of
                    // a module is guarenteed to be atomic by the API.
                    Assert(fRTPSession != NULL);
                    OSMutexLocker   locker(fRTPSession->GetSessionMutex());
                        
                    for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                    {
                        fModuleState.eventRequested = false;
                        fModuleState.idleTime = 0;
                        if (fModuleState.globalLockRequested )
                        {   fModuleState.globalLockRequested = false;
                            fModuleState.isGlobalLocked = true;
                        } 
                        
						/* �õ�ע��kRTSPPreProcessorRole��ģ�鲢���� */
                        theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPPreProcessorRole, fCurrentModule);
                        (void)theModule->CallDispatch(QTSS_RTSPPreProcessor_Role, &fRoleParams);
                        fModuleState.isGlobalLocked = false;

                        // The way the API is set up currently, the first module that adds a stream
                        // to the session is responsible for sending RTP packets for the session.
						/* ��һ����RTPStream����RTP Session��Module����send Packet */
                        if (fRTPSession->HasAnRTPStream() && (fRTPSession->GetPacketSendingModule() == NULL))
                            fRTPSession->SetPacketSendingModule(theModule);
                                                
                        if (fModuleState.globalLockRequested) // call this request back locked
                            return this->CallLocked();

                        // If this module has requested an event, return and wait for the event to transpire
                        if (fModuleState.eventRequested)
                        {
                            this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                        // the same thread to be used for next Run()
                            return fModuleState.idleTime; // If the module has requested idle time...
                        }
                    }
                }
                fCurrentModule = 0;
				/* ����Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }
                fState = kProcessingRequest;
            }

            case kProcessingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kProcessingRequest\n" )
                // If no preprocessor sends a response, move onto the request processing module. It
                // is ALWAYS supposed to send a response, but if it doesn't, we have a canned error
                // to send back.
                fModuleState.eventRequested = false;
                fModuleState.idleTime = 0;
                if (QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPRequestRole) > 0)
                {
                    // Manipulation of the RTPSession from the point of view of
                    // a module is guarenteed to be atomic by the API.
                    Assert(fRTPSession != NULL);
                    OSMutexLocker   locker(fRTPSession->GetSessionMutex());
                        
                    if (fModuleState.globalLockRequested )
                    {   fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    } 

					/************************NOTE!! *******************************************/
					/* ������ȡע��QTSSModule::kRTSPRequestRole��Module,ֻ����QTSSFileModule */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPRequestRole, 0);
					/* ���ø�Moudule */
                    (void)theModule->CallDispatch(QTSS_RTSPRequest_Role, &fRoleParams);
                    fModuleState.isGlobalLocked = false;

                    // Do the same check as above for the preprocessor
					/* ����kPreprocessingRequest��ɫ��ͬ���Ĵ��� */
                    if (fRTPSession->HasAnRTPStream() && fRTPSession->GetPacketSendingModule() == NULL)
						/* ����SendPacketģ��ΪQTSSFileModule */
                        fRTPSession->SetPacketSendingModule(theModule);
                        
                    if (fModuleState.globalLockRequested) // call this request back locked
                        return this->CallLocked();

                    // If this module has requested an event, return and wait for the event to transpire
                    if (fModuleState.eventRequested)
                    {
                        this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                    // the same thread to be used for next Run()
                        return fModuleState.idleTime; // If the module has requested idle time...
                    }
                }
                
                
                /* ����Response */
                if (!fRequest->HasResponseBeenSent())
                {
                    // no modules took this one so send back a parameter error
					/* �����client�õ���method��SetParameter */
                    if (fRequest->GetMethod() == qtssSetParameterMethod) // keep session
                    {
                        QTSS_RTSPStatusCode statusCode = qtssSuccessOK; //qtssClientParameterNotUnderstood;
                        fRequest->SetValue(qtssRTSPReqStatusCode, 0, &statusCode, sizeof(statusCode));
                        fRequest->SendHeader();
                    }
                    else
                    {
                        QTSSModuleUtils::SendErrorResponse(fRequest, qtssServerInternal, qtssMsgNoModuleForRequest);
                    }
                }

                fState = kPostProcessingRequest;
            }

			/* �ڷ���Response֮ǰ,����RTSP request */
            case kPostProcessingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kPostProcessingRequest\n" )
                // Post process the request *before* sending the response. Therefore, we
                // will post process regardless of whether the client actually gets our response
                // or not.
                
                //if this is not a keepalive request, we should kill the session NOW
				/* ͨ��fResponseKeepAlive��ֵ(����RTPSession::SendTeardownResponse()������Ϊfalse)�ж��Ƿ��ǻ�Ծ��RTSPSession,������true */
                fLiveSession = fRequest->GetResponseKeepAlive();
                
                if (fRTPSession != NULL)
                {
                    // Invoke postprocessor modules only if there is an RTP session. We do NOT want
                    // postprocessors running when filters or syntax errors have occurred in the request!
					/* ���ע��kRTSPPostProcessorRole��ģ����Ŀ */
                    numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPPostProcessorRole);
                    {
                        // Manipulation of the RTPSession from the point of view of
                        // a module is guarenteed to be atomic by the API.
                        OSMutexLocker   locker(fRTPSession->GetSessionMutex());
    
                        // Make sure the RTPSession contains a copy of the realStatusCode in this request
						/* �õ���ǰRTSPSession��status code,����404(�Ҳ���client)������RTSPSession�ֵ����Ӧ����ֵ */
                        UInt32 realStatusCode = RTSPProtocol::GetStatusCode(fRequest->GetStatus());
                        (void) fRTPSession->SetValue(qtssCliRTSPReqRealStatusCode,(UInt32) 0,(void *) &realStatusCode, sizeof(realStatusCode), QTSSDictionary::kDontObeyReadOnly);

                        // Make sure the RTPSession contains a copy of the qtssRTSPReqRespMsg in this request
						/* �õ���ǰRTSPSession��RespMsg,������RTSPSession�ֵ����Ӧ����ֵ */
                        StrPtrLen* theRespMsg = fRequest->GetValue(qtssRTSPReqRespMsg);
                        if (theRespMsg->Len > 0)
                            (void)fRTPSession->SetValue(qtssCliRTSPReqRespMsg, 0, theRespMsg->Ptr, theRespMsg->Len, QTSSDictionary::kDontObeyReadOnly);
                
                        // Set the current RTSP session for this RTP session.
                        // We do this here because we need to make sure the SessionMutex
                        // is grabbed while we do this. Only do this if the RTSP session
                        // is still alive, of course
						/* Ϊ��ǰ��RTP session����RTSP Session */
                        if (this->IsLiveSession())
                            fRTPSession->UpdateRTSPSession(this);
                    
                        for (; (fCurrentModule < numModules) ||  (fModuleState.eventRequested) ; fCurrentModule++)
                        {
                            fModuleState.eventRequested = false;
                            fModuleState.idleTime = 0;
                            if (fModuleState.globalLockRequested )
                            {   fModuleState.globalLockRequested = false;
                                fModuleState.isGlobalLocked = true;
                            } 
                            
							/* �õ�ע��kRTSPPostProcessorRole��Module������ */
                            theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPPostProcessorRole, fCurrentModule);
                            (void)theModule->CallDispatch(QTSS_RTSPPostProcessor_Role, &fRoleParams);
                            fModuleState.isGlobalLocked = false;
                            
                            if (fModuleState.globalLockRequested) // call this request back locked
                                return this->CallLocked();
                                
                            // If this module has requested an event, return and wait for the event to transpire
                            if (fModuleState.eventRequested)
                            {
                                this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                            // the same thread to be used for next Run()
                                return fModuleState.idleTime; // If the module has requested idle time...
                            }
                        }
                    }
                }
                fCurrentModule = 0;
                fState = kSendingResponse;
            }

			/* ����kSendingResponse ״̬�����ڷ��ͶԿͻ�RTSP���������֮���
			��Ӧ��ϵͳ�ڸ�״̬������fOutputStream.Flush()��������fOutputStream ����δ
			������������Ӧͨ��Socket �˿���ȫ���ͳ�ȥ�� */
            case kSendingResponse:
            {
				HTTP_TRACE( "RTSPSession::Run kSendingResponse\n" )
                // Sending the RTSP response consists of making sure the
                // RTSP request output buffer is completely flushed(���) to the socket.
                Assert(fRequest != NULL);
                
				// If x-dynamic-rate header is sent with a value of 1, send OPTIONS request
				/* ����RTSP method��Setup�ҷ���200 OK,�õ���"x-dynamic-rate: 1\r\n",��Ҫ��������ʱ��(RTT),���ݲ�����fOutputStream�Ļ���,����Options request */
				if ((fRequest->GetMethod() == qtssSetupMethod) && (fRequest->GetStatus() == qtssSuccessOK)
				    && (fRequest->GetDynamicRateState() == 1) && fRoundTripTimeCalculation)
				{
					this->SaveOutputStream();//����fOutputStream�����е�����
					this->ResetOutputStream();//���fOutputStream�����е�����,������ָ�����ڿ�ͷ
					this->SendOptionsRequest();//��fOutputStream�з�������"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"
				}
			
				/* �����ѷ�����Options request,��RTSP Method�Ƿ�,���������ݸ��Ƶ�fOutputStream�� */
				if (fSentOptionsRequest && (fRequest->GetMethod() == qtssIllegalMethod))
				{
					this->ResetOutputStream();//���fOutputStream�����е�����,������ָ�����ڿ�ͷ
					this->RevertOutputStream();//����fOldOutputStreamBuffer�е�����,�����Ƶ�fOutputStream��,ɾ��fOldOutputStreamBuffer
					fSentOptionsRequest = false;
				}
				
				/* ��ͳ��buffer�л��ж��ٴ��ͳ�������,����Socket::Send()���ⷢ������.ֻҪ�ܹ��ͳ�����,��refresh timeout.
				����һ��ȫ���ͳ�����,��flush�����,�����ۼ����ͳ�������,����EAGAIN */
                err = fOutputStream.Flush();//��fOutputStream�е�����ȫ�����͸�Client
                
				/* ������EAGAIN,��ʹ������,����tcp socket����W_R�¼�,��socket��Ϊ��дʱ�ٷ������� */
                if (err == EAGAIN)
                {
                    // If we get this error, we are currently flow-controlled and should
                    // wait for the socket to become writeable again
                    fSocket.RequestEvent(EV_WR);
                    this->ForceSameThread();    // We are holding mutexes, so we need to force the same thread to be used for next Run()
                                                
                    return 0;
                }
                else if (err != QTSS_NoErr)
                {
                    // Any other error means that the client has disconnected, right?
                    Assert(!this->IsLiveSession());
                    break;
                }
            
                fState = kCleaningUp;
            }
            
			/* ����kCleaningUp ״̬����������ϴδ�������ݣ�����״̬����Ϊ
			kReadingRequest �ȴ��´����󵽴 */
            case kCleaningUp:
            {
				HTTP_TRACE( "RTSPSession::Run kCleaningUp\n" )
                // Cleaning up consists of making sure we've read all the incoming Request Body
                // data off of the socket
                if (this->GetRemainingReqBodyLen() > 0)
                {
					/* ��ӡ��RTSP request������ */
                    err = this->DumpRequestData();
                    
					/* ������EAGAIN,��ʹ������,����tcp socket����W_R�¼�,��socket��Ϊ��дʱ�ٷ������� */
                    if (err == EAGAIN)
                    {
                        fInputSocketP->RequestEvent(EV_RE);
                        this->ForceSameThread();    // We are holding mutexes, so we need to force
                                                    // the same thread to be used for next Run()
                        return 0;
                    }
                }
                    
                // If we've gotten here, we've flushed all the data. Cleanup,
                // and wait for our next request!
				/* ��յ�ǰRTSP request */
                this->CleanupRequest();
                fState = kReadingRequest;
            }
        }
    }
    
    // Make absolutely sure there are no resources being occupied by the session
    // at this point.
    this->CleanupRequest();
	HTTP_TRACE( "RTSPSession::Run CleanupRequest() over\n" )

    // Only delete if it is ok to delete!
    if (fObjectHolders == 0)
        return -1;

    // If we are here because of a timeout, but we can't delete because someone
    // is holding onto a reference to this session, just reschedule(���µ���) the timeout.
    //
    // At this point, however, the session is DEAD.
    return 0;
}



Bool16 RTSPSession::ParseProxyTunnelHTTP()
{
    /*
        if it's an HTTP request
        parse the interesing parts from the request       
        - check for GET or POST, set fHTTPMethod
        - checck for HTTP protocol, set fWasHTTPRequest
        - check for SessionID header, set fProxySessionID char array
        - check for accept "application/x-rtsp-tunnelled.      
    */
    
    Bool16          isHTTPRequest = false;
    StrPtrLen       *splRequest;
    
	/* ��ӡ���� */
    HTTP_VTRACE( "ParseProxyTunnelHTTP in\n" ) 
		/* ��ȡRTSP Request������ */
    splRequest = fInputStream.GetRequestBuffer();
    fFoundValidAccept = true;
    Assert( splRequest );
    
    if ( splRequest )
    {
        fHTTPMethod = kHTTPMethodUnknown;
    
    #if __RTSP_HTTP_DEBUG__ 
        {
			/* ��ӡ����ȡ��RTSP Request���� */
            char    buff[1024];     
            memcpy( buff, splRequest->Ptr, splRequest->Len );    
            buff[ splRequest->Len] = 0;    
            HTTP_VTRACE( buff )
        }
    #endif

		/* ���������ȡ��RTSP Request���� */
        StrPtrLen       theParsedData;
        StringParser    parser(splRequest);
        
        parser.ConsumeWord(&theParsedData);
        
        HTTP_VTRACE( "request method: \n" )
        HTTP_VTRACE_SPL( &theParsedData )
        
        // does first line begin with POST or GET?
        if (theParsedData.EqualIgnoreCase("post", 4 ))
        {   
            fHTTPMethod = kHTTPMethodPost;
        }
        else if (theParsedData.EqualIgnoreCase("get", 3 ))
        {
    
            fHTTPMethod = kHTTPMethodGet;
        }
        
        if ( fHTTPMethod != kHTTPMethodUnknown )
        {
            HTTP_VTRACE( "IsAHTTPProxyTunnelPostRequest found POST or GET\n" )
            parser.ConsumeWhitespace(); // skip over ws past method

            parser.ConsumeUntilWhitespace( &theParsedData ); // theParsedData now contains the URL and CGI params ( we don't need yet );
            
            parser.ConsumeWhitespace(); // skip over ws past url
            
            parser.ConsumeWord(&theParsedData); // now should contain "HTTP"
            
            HTTP_VTRACE( "should be HTTP/1.* next: \n" )
            HTTP_VTRACE_SPL( &theParsedData )
            
            // DMS - why use NumEqualIgnoreCase? Wouldn't EqualIgnoreCase do the trick here?
            if (theParsedData.NumEqualIgnoreCase("http", 4 ))
            {   HTTP_TRACE( "ParseProxyTunnelHTTP found HTTP\n" )
                fWasHTTPRequest = true;
            }
        
        }
        
        
        if ( fWasHTTPRequest )
        {
            // it's HTTP and one of the methods we like....
            // now, find the Session ID and Accept headers
            const char* kSessionHeaderName = "X-SessionCookie:";
            const int   kSessionHeaderNameLen = ::strlen(kSessionHeaderName);
            const char* kAcceptHeaderName = "Accept:";
            const int   kAcceptHeaderNameLen = ::strlen(kAcceptHeaderName);
            //const char* kAcceptData = "application/x-rtsp-tunnelled";
            //const int kAcceptDataLen = ::strlen(kAcceptData);
            
            while ( parser.GetDataRemaining() > 0 )
            {
                parser.GetThruEOL( &theParsedData ); // we don't need this, but there is not a ComsumeThru...
            
                parser.ConsumeUntilWhitespace( &theParsedData ); // theParsedData now contains the URL and CGI params ( we don't need yet );
            
                if ( theParsedData.EqualIgnoreCase( kSessionHeaderName, kSessionHeaderNameLen ) )
                {
                    // we got a weener!
                    if ( parser.GetDataRemaining() > 0 )
                        parser.ConsumeWhitespace();
                    
                    if ( parser.GetDataRemaining() > 0 )
                    {   
                        StrPtrLen   sessionID;
                        
                        parser.ConsumeUntil( &sessionID, StringParser::sEOLMask );
                    
                        // cache the ID so we can use it to remove ourselves from the map
                        if ( sessionID.Len < QTSS_MAX_SESSION_ID_LENGTH )
                        {   
                            ::memcpy( fProxySessionID, sessionID.Ptr,  sessionID.Len );
                            fProxySessionID[sessionID.Len] = 0;
                            fProxySessionIDPtr.Set( fProxySessionID, ::strlen(fProxySessionID) );
                            HTTP_VTRACE_ONE( "found session id: %s\n", fProxySessionID )
                        }
                    }
                }
                else if ( theParsedData.EqualIgnoreCase( kAcceptHeaderName, kAcceptHeaderNameLen ) )
                {
                    StrPtrLen   hTTPAcceptHeader;
                    
                    // we got another weener!
                    if ( parser.GetDataRemaining() > 0 )
                        parser.ConsumeWhitespace();
                    
                    if ( parser.GetDataRemaining() > 0 )
                    {   
                        parser.ConsumeUntil( &hTTPAcceptHeader, StringParser::sEOLMask );           
                        
                        #if __RTSP_HTTP_DEBUG__ 
                        {
                            char    buff[1024];
                            
                            memcpy( buff, hTTPAcceptHeader.Ptr, hTTPAcceptHeader.Len );
                            
                            buff[ hTTPAcceptHeader.Len] = 0;
                            
                            HTTP_VTRACE_ONE( "client will accept: %s\n", buff )
                        }
                        #endif
                            
                        // we really don't need to check thisif ( theParsedData.EqualIgnoreCase( kAcceptData, kAcceptDataLen ) ) 
                        {   fFoundValidAccept = true;
                            
                            HTTP_VTRACE( "found valid accept\n" )
                        }
                    
                    }
                        
                }
            }           
        
        }
        
    }
    
    // we found all that we were looking for
    if ( fFoundValidAccept && *fProxySessionID  && fWasHTTPRequest )
        isHTTPRequest = true;
    
	HTTP_VTRACE( "ParseProxyTunnelHTTP out\n" ) 
    return isHTTPRequest;
    
}

/*

    "pre" filter the request looking for the HHTP Proxy
    tunnel HTTP requests, merge the 2 sessions
    into one, let the donor Session die.
    

*/

QTSS_Error RTSPSession::PreFilterForHTTPProxyTunnel()
{
	HTTP_VTRACE( "PreFilterForHTTPProxyTunnel()\n" ) 
    // returns true if it's an HTTP request that can tunnel
    if (!this->ParseProxyTunnelHTTP())
        return QTSS_NoErr;
    
    // This is an RTSP / HTTP session, so decrement the total RTSP sessions
    // and increment the total HTTP sessions
    Assert(fSessionType == qtssRTSPSession);
    QTSServerInterface::GetServer()->SwapFromRTSPToHTTP();
    
    // Setup our ProxyTunnel OSRefTable Ref
    Assert( fProxySessionIDPtr.Len > 0 );
    fProxyRef.Set(fProxySessionIDPtr, this);

    // We have to set this here, because IF we are able to put ourselves in the map,
    // the GET may arrive immediately after, and the GET checks this state.
    fState = kWaitingToBindHTTPTunnel;
    QTSS_RTSPSessionType theOtherSessionType = qtssRTSPSession;

    if ( fHTTPMethod == kHTTPMethodPost )
    {
        HTTP_TRACE( "RTSPSession is a POST request.\n" )
        fSessionType            = qtssRTSPHTTPInputSession;
        theOtherSessionType     = qtssRTSPHTTPSession;
    }
	else if ( fHTTPMethod == kHTTPMethodGet )
    {
        HTTP_TRACE( "RTSPSession is a GET request.\n" )
        // we're session O (outptut)  the POST half is session 1 ( input )
        fSessionType            = qtssRTSPHTTPSession;  
        theOtherSessionType     = qtssRTSPHTTPInputSession;
        
        Bool16 showServerInfo = QTSServerInterface::GetServer()->GetPrefs()->GetRTSPServerInfoEnabled();
        if (fDoReportHTTPConnectionAddress )
        {   // contruct a 200 OK header with an "x-server-ip-address" header
        
            char        responseHeaderBuf[kMaxHTTPResponseLen];
            char        localIPAddrBuf[20] = { 0 };
            StrPtrLen   localIPAddr(localIPAddrBuf, 19);
            
            // get a copy of the local IP address from the dictionary
            this->GetValue(qtssRTSPSesLocalAddrStr, 0, localIPAddr.Ptr, &localIPAddr.Len);
            Assert( localIPAddr.Len < sizeof( localIPAddrBuf ) );
            localIPAddrBuf[localIPAddr.Len] = 0;
            
            char *headerFieldPtr = "";
            if(showServerInfo)
            {
                headerFieldPtr = QTSServerInterface::GetServerHeader().Ptr;    
                qtss_sprintf( responseHeaderBuf, sHTTPResponseFormatStr, "X-server-ip-address: ", localIPAddrBuf, "\r\n", headerFieldPtr );
           }
            else
            {
                qtss_sprintf( responseHeaderBuf, sHTTPNoServerResponseFormatStr, "X-server-ip-address: ", localIPAddrBuf, "\r\n", headerFieldPtr);
                
            }          
            Assert(::strlen(responseHeaderBuf) < kMaxHTTPResponseLen);
            fOutputStream.Put(responseHeaderBuf); 
            

        }
        else // use the premade stopck version
        {   if (showServerInfo)
                fOutputStream.Put(sHTTPResponseHeaderPtr);  // 200 OK just means we connected...
            else
                fOutputStream.Put(sHTTPResponseNoServerHeaderPtr);  // 200 OK just means we connected...

        }
    }   
    else
        Assert(0);
        
    // This function attempts to register our Ref into the map. If there is another
    // session with a matching magic number, it resolves it and returns that Ref.
    // If it returns NULL, something bad has happened, and we should just kill the session.
    OSRef* rtspSessionRef = this->RegisterRTSPSessionIntoHTTPProxyTunnelMap(theOtherSessionType);

    // Something went wrong (usually we get here because there is a session with this magic
    // number, and that session is currently doing something
    if (rtspSessionRef == NULL)
    {
        HTTP_TRACE("RegisterRTSPSessionIntoHTTPProxyTunnelMap returned NULL. Abort.\n");
        return QTSS_RequestFailed;
    }

    // We registered ourselves into the map (we are the first half), so wait for our other half
    if (rtspSessionRef == &fProxyRef)
    {
        HTTP_TRACE("Registered this session into map. Waiting to bind\n");
        return QTSS_NoErr;
    }
    
    OSRefReleaser theRefReleaser(sHTTPProxyTunnelMap, rtspSessionRef); // auto release this ref
    RTSPSession* theOtherSession = (RTSPSession*)theRefReleaser.GetRef()->GetObject();

    // We must lock down this session, for we (may) be manipulating its socket & input
    // stream, and therefore it cannot be in the process of reading data or processing a request.
    // If it is, well, safest thing to do is probably just deny this attempt to bind.
    if (!theOtherSession->fReadMutex.TryLock())
    {
        HTTP_TRACE("Found another session in map, but couldn't grab fReadMutex. Abort.\n");
        return QTSS_RequestFailed;
    }
    
    if (fHTTPMethod == kHTTPMethodPost)
    {
        // take the input session's socket. This also grabs the other session's input stream
        theOtherSession->SnarfInputSocket(this);

        // Attempt to bind to this GET connection
        // this will reset our state on success.
        HTTP_TRACE_ONE( "RTSPSession POST snarfed a donor session successfuly (%s).\n", fProxySessionID )
        fState = kSocketHasBeenBoundIntoHTTPTunnel;
        theOtherSession->fState = kReadingRequest;
        theOtherSession->Signal(Task::kReadEvent);
    }
    else if (fHTTPMethod == kHTTPMethodGet)
    {
        Assert( theOtherSession->fState == kWaitingToBindHTTPTunnel );
        HTTP_TRACE_ONE( "RTSPSession GET snarfed a donor session successfuly (%s).\n", fProxySessionID )

        // take the input session's socket. This also grabs the other session's input stream
        this->SnarfInputSocket(theOtherSession);

        // we assume the donor's place in the map.
        sHTTPProxyTunnelMap->Swap( &fProxyRef );

        // the 1/2 connections are bound
        // the output Session state goes back to reading a request, this time an RTSP request
        // the socket donor Session(rtspSessionInput) state goes to kSocketHasBeenBoundIntoHTTPTunnel to die
        fState = kReadingRequest;
        theOtherSession->fState = kSocketHasBeenBoundIntoHTTPTunnel;
        theOtherSession->Signal(Task::kKillEvent);
    }
    
    theOtherSession->fReadMutex.Unlock();
    return QTSS_NoErr;
}

/* ����ǰ��RTSPSession��fProxyRefע���HTTPProxyTunnelMap,���������:(1)����ע��ɹ�,���ص�ǰRTSPSession��fProxyRef;
(2)������һ��RTSPSession����ͬ��magic number��ָ����sessionType, ������һ���Ự��fProxyRef;(3)����ע��ɹ�,�����
RTSPSession���Ǳ�Ψһ��userʹ��,����ָ����sessionType,����NULL. */
OSRef* RTSPSession::RegisterRTSPSessionIntoHTTPProxyTunnelMap(QTSS_RTSPSessionType inSessionType)
{
    // This function attempts to register the current session's fProxyRef into the map, and
    // 1) returns the current session's fProxyRef if registration was successful
    // 2) returns another session's fProxyRef if it has the same magic number and is the right sessionType
    // 3) returns NULL if there is a session with the same magic # but it couldn't be resolved.
    
    OSMutexLocker locker(sHTTPProxyTunnelMap->GetMutex());
    OSRef* theRef = sHTTPProxyTunnelMap->RegisterOrResolve(&fProxyRef);
    if (theRef == NULL)
        return &fProxyRef;
        
    RTSPSession* rtspSession = (RTSPSession*)theRef->GetObject();
    
    // we can be the only user of the object right now
    Assert(theRef->GetRefCount() > 0);
    if (theRef->GetRefCount() > 1 || rtspSession->fSessionType != inSessionType)
    {
        sHTTPProxyTunnelMap->Release(theRef);
        theRef = NULL;
    }
    return theRef;
}

/* ���RTSP request��client���͵���֤����͵�ǰ�ĻỰ�����Ƿ���ͬ?����ͬ,��ʹ����֤;����ͬ,����һϸ��Ϊ�������:None/Basic/Digest,
   ������֤ʧ��,������֤ͨ�����ǹ���,���û���/����/�û����ÿ�.
*/
void RTSPSession::CheckAuthentication() {
    
	/* ��RTSP request�л�ȡ�û���/���� */
    QTSSUserProfile* profile = fRequest->GetUserProfile();
    StrPtrLen* userPassword = profile->GetValue(qtssUserPassword);
	/* ��RTSP request�л�ȡ��֤����:None/Basic/Digest */
    QTSS_AuthScheme scheme = fRequest->GetAuthScheme();

	/* ��֤ͨ�� */
    Bool16 authenticated = true;
    
    // Check if authorization information returned by the client is for the scheme that the server sent the challenge
	/* ���RTSP request��client���͵���֤����͵�ǰ�ĻỰ�����Ƿ���ͬ? */
	/* ����ͬ,��ʹ����֤ */
    if(scheme != (fRTPSession->GetAuthScheme())) {
        authenticated = false;
    }
	/* ����ͬ,����һϸ��Ϊ�������:None/Basic/Digest */
    else if(scheme == qtssAuthBasic) {  
        // For basic authentication, the authentication module returns the crypt of the password, 
        // so compare crypt of qtssRTSPReqUserPassword and the text in qtssUserPassword
		/* �Ƚϴ�RTSP request��õ��û���/����,��Module���صļ��ܹ����û���/���� */
        StrPtrLen* reqPassword = fRequest->GetValue(qtssRTSPReqUserPassword);
        char* userPasswdStr = userPassword->GetAsCString(); // memory allocated
        char* reqPasswdStr = reqPassword->GetAsCString();   // memory allocated
        
		/* ������ܺ��RTSP request��õ��û���/����,��Module���صļ��ܹ����û���/���벻ͬ,��֤Ϊfalse */
        if(userPassword->Len == 0)
        {
              authenticated = false;
        }
        else
        {
#ifdef __Win32__
          // The password is md5 encoded for win32
          char md5EncodeResult[120];
          // no memory is allocated in this function call
          MD5Encode(reqPasswdStr, userPasswdStr, md5EncodeResult, sizeof(md5EncodeResult));
          if(::strcmp(userPasswdStr, md5EncodeResult) != 0)
            authenticated = false;
#else
          if(::strcmp(userPasswdStr, (char*)crypt(reqPasswdStr, userPasswdStr)) != 0)
            authenticated = false;
#endif
        }

        delete [] userPasswdStr;    // deleting allocated memory
        userPasswdStr = NULL;
        delete [] reqPasswdStr;
        reqPasswdStr = NULL;        // deleting allocated memory
    }
    else if(scheme == qtssAuthDigest) { // For digest authentication, md5 digest comparison
        // The text returned by the authentication module in qtssUserPassword is MD5 hash of (username:realm:password)
        
        UInt32 qop = fRequest->GetAuthQop();
        StrPtrLen* opaque = fRequest->GetAuthOpaque();
        StrPtrLen* sessionOpaque = fRTPSession->GetAuthOpaque();
        UInt32 sessionQop = fRTPSession->GetAuthQop();
        
        do {
            // The Opaque string should be the same as that sent by the server
            // The QoP should be the same as that sent by the server
            if((sessionOpaque->Len != 0) && !(sessionOpaque->Equal(*opaque))) {
                authenticated = false;
                break;
            }
            
            if(sessionQop != qop) {
                authenticated = false;
                break;
            }
            
            // All these are just pointers to existing memory... no new memory is allocated
            //StrPtrLen* userName = profile->GetValue(qtssUserName);
            //StrPtrLen* realm = fRequest->GetAuthRealm();
            StrPtrLen* nonce = fRequest->GetAuthNonce();
            StrPtrLen method = RTSPProtocol::GetMethodString(fRequest->GetMethod());
            StrPtrLen* digestUri = fRequest->GetAuthUri();
            StrPtrLen* responseDigest = fRequest->GetAuthResponse();
            //StrPtrLen hA1;
            StrPtrLen requestDigest;
            StrPtrLen emptyStr;
            
            StrPtrLen* cNonce = fRequest->GetAuthCNonce();
            // Since qtssUserPassword = md5(username:realm:password)
            // Just convert the 16 bit hash to a 32 bit char array to get HA1
            //HashToString((unsigned char *)userPassword->Ptr, &hA1);
            //CalcHA1(&sAuthAlgorithm, userName, realm, userPassword, nonce, cNonce, &hA1);
            
            
            // For qop="auth"
            if(qop ==  RTSPSessionInterface::kAuthQop) {
                StrPtrLen* nonceCount = fRequest->GetAuthNonceCount();
                // Convert nounce count (which is a string of 8 hex digits) into a UInt32
                UInt32 ncValue, i, pos = 1;
                ncValue = (nonceCount->Ptr)[nonceCount->Len - 1];
                for(i = (nonceCount->Len - 1); i > 0; i--) {
                    pos *= 16;
                    ncValue += (nonceCount->Ptr)[i - 1] * pos;
                }
                // nonce count must not be repeated by the client
                if(ncValue < (fRTPSession->GetAuthNonceCount())) { 
                    authenticated = false;
                    break;
                }
                
                // allocates memory for requestDigest.Ptr
                CalcRequestDigest(userPassword, nonce, nonceCount, cNonce, &sAuthQop, &method, digestUri, &emptyStr, &requestDigest);
                // If they are equal, check if nonce used by client is same as the one sent by the server
                
            }   // For No qop
            else if(qop == RTSPSessionInterface::kNoQop) 
            {
                // allocates memory for requestDigest->Ptr
                CalcRequestDigest(userPassword, nonce, &emptyStr, &emptyStr, &emptyStr, &method, digestUri, &emptyStr, &requestDigest);             
            }
            
            // hA1 is allocated memory 
            //delete [] hA1.Ptr;
            
            if(responseDigest->Equal(requestDigest)) {
                if(!(nonce->Equal(*(fRTPSession->GetAuthNonce()))))
                    fRequest->SetStale(true);
                authenticated = true;
            }
            else { 
                authenticated = false;
            }
            
            // delete the memory allocated in CalcRequestDigest above 
            delete [] requestDigest.Ptr;
            requestDigest.Len = 0;
            
        } while(false);             
    }
    
    // If authenticaton failed, set qtssUserName in the qtssRTSPReqUserProfile attribute
    // to NULL and clear out the password and any groups that have been set.
	/* ������֤ʧ��,������֤ͨ�����ǹ���,���û���/����/�û����ÿ� */
    if((!authenticated) || (authenticated && (fRequest->GetStale()))) {
        (void)profile->SetValue(qtssUserName, 0,  sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
        (void)profile->SetValue(qtssUserPassword, 0,  sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
        (void)profile->SetNumValues(qtssUserGroups, 0);
    }
}

// ������ϵRTSPSessionInterface::SendOptionsRequest()
/* ��RTSP request��ȡfull request data,�ж���ǰ4���ַ��Ƿ���"RTSP"?����true��false */
Bool16 RTSPSession::ParseOptionsResponse()
{
	/* ��RTSP request��ȡfull request data */
	StringParser parser(fRequest->GetValue(qtssRTSPReqFullRequest));
	Assert(fRequest->GetValue(qtssRTSPReqFullRequest)->Ptr != NULL);
	static StrPtrLen sRTSPStr("RTSP", 4);
	StrPtrLen theProtocol;
	/* ��full request data�е�ǰ4���ַ�����theProtocol */
	parser.ConsumeLength(&theProtocol, 4);
	
	/* �ж���ǰ4���ַ��Ƿ���"RTSP"? */
	return (theProtocol.Equal(sRTSPStr));
}

/* ���Ƚ�������Client��full RTSP Request,����ͬ��Methods,���Ҷ�Ӧ��RTP Session,û�о��½�һ��RTP Session.��Play Request������thinning parameters */
void RTSPSession::SetupRequest()
{
    //
    // First parse the request
	/* �ȴ�client��ȡfull RTSP Request,�������ĵ�һ�к�������,��Response header��Request headerͬ��,��ȡRequested File path */
    QTSS_Error theErr = fRequest->Parse();
	/* �������,ֱ�ӷ��� */
    if (theErr != QTSS_NoErr)
        return;
    
	//���Ҹ������RTPSession
    // let's also refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
    // Attempt to find the RTP session for this request.
	/* �õ�RTPSessionMap��Hash Table,�Եõ������RTPSessionMap��Ԫ */
    OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();

	/* ��RTSPRequest�л�ȡ���Ӧ��RTPSession��ID,���ҵ���ʶ���ҳ���Ӧ��HashTableԪ,�����ҵ���Ӧ��RTPSession;
	��û���ҵ�,˵������һ���µ�RTPSession,���õ�ǰ��fLastRTPSessionID[]����RTPSession ID,�ҵ���Ӧ��RTPSession */
    theErr = this->FindRTPSession(theMap);
    
	/* ˢ�µ�ǰ���RTPSession��Timeout,����RTSPSession����ͬ�� */
    if (fRTPSession != NULL)
        fRTPSession->RefreshTimeout();

    QTSS_RTSPStatusCode statusCode = qtssSuccessOK;
    char *body = NULL;
    UInt32 bodySizeBytes = 0;
    
	//OPTIONS ����,�򵥷��ر�׼OPTIONS��Ӧ(����)����
	/*
	#S->C:
	#time: ms=9159 date=Fri, 02 Jul 2010 05:03:08 GMT
	RTSP/1.0 200 OK\r\n
	Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
	Cseq: 7\r\n
	Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n
	\r\n
	*/
    // If this is an OPTIONS request, don't even bother letting modules see it. Just
    // send a standard OPTIONS response, and bedone.
    if (fRequest->GetMethod() == qtssOptionsMethod)
    {
		/* ��ȡRTSP Request�е�CSeq��RTSP header */
        StrPtrLen* cSeqPtr = fRequest->GetHeaderDictionary()->GetValue(qtssCSeqHeader);
		/* ����CSeqͷΪ��,��RTSPResponseStream�з���������Ϣ:
		   RTSP/1.0 400 Bad Request\r\n
		   Server: QTSS/4.1.3.x(Build/425:Platform/MacOSX;Release/Development;)\r\n
		   CSeq:
		   Session: 50887676\r\n
		   \r\n
		*/
        if (cSeqPtr == NULL || cSeqPtr->Len == 0)
        {   
            statusCode = qtssClientBadRequest;
            fRequest->SetValue(qtssRTSPReqStatusCode, 0, &statusCode, sizeof(statusCode));
            fRequest->SendHeader();
            return;
        }

        /* ����,��RTSPResponseStream�з���������Ϣ:
		RTSP/1.0 400 200 OK\r\n
		Server: QTSS/4.1.3.x(Build/425:Platform/MacOSX;Release/Development;)\r\n
		CSeq: 91
		Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n
		\r\n
		*/    
        fRequest->AppendHeader(qtssPublicHeader, QTSServerInterface::GetPublicHeader());

        // DJM PROTOTYPE
        StrPtrLen* requirePtr = fRequest->GetHeaderDictionary()->GetValue(qtssRequireHeader);
		/* ��������ͷ��"x-Random-Data-Size",����OPTIONS��Ӧ��,�ٸ���"Content-length: 1209\r\n" */
        if ( requirePtr && requirePtr->EqualIgnoreCase(RTSPProtocol::GetHeaderString(qtssXRandomDataSizeHeader)) )
        {
			/* λ��fOutputStream��,�μ�RTSPSessionInterface::SendOptionsRequest() */
            body = (char*) RTSPSessionInterface::sOptionsRequestBody;//1400�ֽ�
			/* ��ȡ"x-Random-Data-Size"ͷ�����ݳ��� */
            bodySizeBytes = fRequest->GetRandomDataSize();
            Assert( bodySizeBytes <= sizeof(RTSPSessionInterface::sOptionsRequestBody) );//64K�ֽ�
			//����"Content-Type: application/x-random-data"
            fRequest->AppendHeader(qtssContentTypeHeader, &sContentType);
			//����"Content-length: 1209\r\n",ע�ⳤ����"x-Random-Data-Size"ͷ�����ݳ���
            fRequest->AppendContentLength(bodySizeBytes);
        } 
		
		
		/* ��������Client���͵����ݷ���RTSPResponseStream */
		fRequest->SendHeader();
	    
	    // now write the body if there is one
		/* ��Client���ͻ�RTSP��Ӧ */
        if (bodySizeBytes > 0 && body != NULL)
            fRequest->Write(body, bodySizeBytes, NULL, 0);

        return;
    }

	// If this is a SET_PARAMETER request, don't let modules see it.
	if (fRequest->GetMethod() == qtssSetParameterMethod)
	{
         

		// Check that it has the CSeq header
		/* ��ȡRTSP Request�е�CSeq��RTSP header */
		StrPtrLen* cSeqPtr = fRequest->GetHeaderDictionary()->GetValue(qtssCSeqHeader);
		/* /* ����CSeqͷΪ��,����RTSPRequest��״̬��Ϊ:RTSP/1.0 400 Bad Request\r\n */
		if (cSeqPtr == NULL || cSeqPtr->Len == 0) // keep session
		{	
            statusCode = qtssClientBadRequest;
            fRequest->SetValue(qtssRTSPReqStatusCode, 0, &statusCode, sizeof(statusCode));
            fRequest->SendHeader();
			return;
		}
	
		
		// If the RTPSession doesn't exist, return error
		if (fRTPSession == NULL) // keep session
		{
            statusCode = qtssClientSessionNotFound;
			/* ����CSeqͷΪ��,����RTSPRequest��״̬��Ϊ:RTSP/1.0 454 Session Not Found\r\n */
            fRequest->SetValue(qtssRTSPReqStatusCode, 0, &statusCode, sizeof(statusCode));
            fRequest->SendHeader();
			return;
		}
		
		// refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
		if (fRequest->GetLateToleranceInSec() != -1)
        {
			/* ��ȡlate-tolerance����ֵ�����ô򱡲��� */
            fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());
            fRequest->SendHeader();
            return;
        }
		// let modules handle it if they want it.
        
	}

	//DESCRIBE ����,���뱣֤û��SessionID,����,��Client����
    // If this is a DESCRIBE request, make sure there is no SessionID. This is not allowed,
    // and may screw up modules if we let them see this request.
    if (fRequest->GetMethod() == qtssDescribeMethod)
    {
        if (fRequest->GetHeaderDictionary()->GetValue(qtssSessionHeader)->Len > 0)
        {
            (void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientHeaderFieldNotValid, qtssMsgNoSesIDOnDescribe);
            return;
        }
    }
    
    //���δ���ҵ�������һ���µ�RTPSession
    // If we don't have an RTP session yet, create one...
    if (fRTPSession == NULL)
    {
		/* ����һ��RTPSession,����theMap���ǵ�ǰ��RTPSessionMap(Ҳ����HashTable) */
        theErr = this->CreateNewRTPSession(theMap);
		/* ���紴��ʧ��,�������� */
        if (theErr != QTSS_NoErr)
            return;
    }

	/* ������Play Request,��Client������late-tolerance����ֵ,������Thinning Parameters */
	// If it's a play request and the late tolerance is sent in the request use this value
	if ((fRequest->GetMethod() == qtssPlayMethod) && (fRequest->GetLateToleranceInSec() != -1))
		fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());
	
	/* ������û��Rangeͷ(������ʼʱ�䶼Ϊ-1)�����ڲ��ŵ�Play Request,�ͷ��ء�200 OK����Ӧ */
    //
    // Check to see if this is a "ping" PLAY request (a PLAY request while already
    // playing with no Range header). If so, just send back a 200 OK response and do nothing.
    // No need to go to modules to do this, because this is an RFC documented behavior  
    if ((fRequest->GetMethod() == qtssPlayMethod) && (fRTPSession->GetSessionState() == qtssPlayingState)
        && (fRequest->GetStartTime() == -1) && (fRequest->GetStopTime() == -1))
    {
        fRequest->SendHeader();
		/* ˢ�¸�RTPSession��timeoutʱ��,�Է������߳�ɾ���ûỰ */
        fRTPSession->RefreshTimeout();
        return;
    }

     
	/* �������Ҫȷ����һ��RTP Session */
    Assert(fRTPSession != NULL); // At this point, we must have one!
    fRoleParams.rtspRequestParams.inClientSession = fRTPSession;
    
    // Setup Authorization params;
    fRequest->ParseAuthHeader();    
    
        
}

/* ɾ����ǰRTSPSession����,ɾ��RTSP request���ݼ�RTSPRequest����,���fRoleParams.rtspRequestParams */
void RTSPSession::CleanupRequest()
{
    if (fRTPSession != NULL)
    {
        // Release the ref.
        OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
        theMap->Release(fRTPSession->GetRef());
        
        // NULL out any references to this RTP session
        fRTPSession = NULL;
        fRoleParams.rtspRequestParams.inClientSession = NULL;
    }
    
	/* ɾ��RTSP request���ݼ�RTSPRequest����,���fRoleParams.rtspRequestParams */
    if (fRequest != NULL)
    {
        // Check to see if a filter module has replaced the request. If so, delete
        // their request now.
        if (fRequest->GetValue(qtssRTSPReqFullRequest)->Ptr != fInputStream.GetRequestBuffer()->Ptr)
            delete [] fRequest->GetValue(qtssRTSPReqFullRequest)->Ptr;
            
        // NULL out any references to the current request
        delete fRequest;
        fRequest = NULL;
        fRoleParams.rtspRequestParams.inRTSPRequest = NULL;
        fRoleParams.rtspRequestParams.inRTSPHeaders = NULL;
    }
    
	/* �ͷŻỰ������ */
    fSessionMutex.Unlock();
    fReadMutex.Unlock();
    
    // Clear out our last value for request body length before moving onto the next request
    this->SetRequestBodyLength(-1);
}

/* ��RTSP Request�ж�λǡ����RTPSession,ʹ�����ַ���: (1)��RTSPRequest�л�ȡ���Ӧ��RTSPSession��ID,���ҵ�
��ʶ���ҳ���Ӧ��HashTableԪ,�����ҵ���Ӧ��RTPSession; (2)��û���ҵ�,˵������һ���µ�RTPSession,���õ�ǰ��
fLastRTPSessionID[]����RTSPSession ID,�ҵ���Ӧ��RTPSession */
QTSS_Error  RTSPSession::FindRTPSession(OSRefTable* inRefTable)
{
    // This function attempts to locate the appropriate RTP session for this RTSP
    // Request. It uses an RTSP session ID as a key to finding the correct RTP session,
    // and it looks for this session ID in two places. First, the RTSP session ID header
    // in the RTSP request, and if there isn't one there, in the RTSP session object itself.
    
	/* ��RTSPRequest�е�"Session: 7736802604597532330\r\n",��ȡ���Ӧ��RTSPSession��ID */
    StrPtrLen* theSessionID = fRequest->GetHeaderDictionary()->GetValue(qtssSessionHeader); 

	/* �����ҵ���RTPSession ID,��ʶ���ҳ���Ӧ��HashTableԪ,�����ҵ���Ӧ��RTPSession */
    if (theSessionID != NULL && theSessionID->Len > 0)
    {
        OSRef* theRef = inRefTable->Resolve(theSessionID);

       if (theRef != NULL)
            fRTPSession = (RTPSession*)theRef->GetObject();
    }
    
    // If there wasn't a session ID in the headers, look for one in the RTSP session itself
	/* ����û���ҵ�RTPSession ID,���õ�ǰ��fLastRTPSessionID[]����(˵������һ���µ�RTPSession) */
    if ( (theSessionID == NULL || theSessionID->Len == 0) && fLastRTPSessionIDPtr.Len > 0)
    {
        OSRef* theRef = inRefTable->Resolve(&fLastRTPSessionIDPtr);
        if (theRef != NULL)
            fRTPSession = (RTPSession*)theRef->GetObject();
    }
    
    return QTSS_NoErr;
}

/* ��ǡ��ʱ������һ��RTPSession,���ûỰ����,����һ�������Ψһ��Session ID,ע�Ტ����RTPSessionMap���inRefTable��,������RTPSession���� */
QTSS_Error  RTSPSession::CreateNewRTPSession(OSRefTable* inRefTable)
{
	/* ȷ��������������һ����:�����ַ��������ʼλ��,���潫����RTPSession ID��ֵ */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

    // This is a brand spanking(ո�µ�) new session. At this point, we need to create
    // a new RTPSession object that will represent this session until it completes.
    // Then, we need to pass the session onto one of the modules

    // First of all, ask the server if it's ok to add a new session
	/* �����������ܼ���һ���µ�RTPSession��?����������ǰ״̬,���������,�������Ƶ� */
    QTSS_Error theErr = this->IsOkToAddNewRTPSession();
	/* ���ڲ��ܼ����µ�RTPSession,ֱ�ӷ��� */
    if (theErr != QTSS_NoErr)
        return theErr;

    // Create the RTPSession object
	/* ȷ����RTSPSessionû����Ӧ��RTPSession */
    Assert(fRTPSession == NULL);

	/***************** NOTE!! ******************/
    fRTPSession = NEW RTPSession();
    
    {
        //
        // Lock the RTP session down so that it won't delete itself in the
        // unusual event there is a timeout while we are doing this.
		/* ������RTPSession */
        OSMutexLocker locker(fRTPSession->GetSessionMutex());

        // Because this is a new RTP session, setup some dictionary attributes
        // pertaining to RTSP that only need to be set once
		/* ���ø�RTPSession�Ự���й�RTSP��ص�Dictionary����(ֻ���ڴ˴�����һ��) */
        this->SetupClientSessionAttrs();    
        
        // So, generate a unique session ID for this session
        QTSS_Error activationError = EPERM;

		/* ͨ��while()ѭ������activationError��ֵֻ����QTSS_NoErr,��������EPERM,��retry,ֱ���ҵ�Ψһ��keyֵ */
        while (activationError == EPERM)
        {
			/* ���������RTPSession��ID����fLastRTPSessionID */
            fLastRTPSessionIDPtr.Len = this->GenerateNewSessionID(fLastRTPSessionID);
            
            //ok, some module has bound this session, we can activate it.
            //At this point, we may find out that this new session ID is a duplicate.
            //If that's the case, we'll simply retry(����) until we get a unique ID
			/* ��OSRef::Set()���ø�RTPSession��Session ID,����qtssCliSesRTSPSessionID��qtssSvrClientSessions������ֵ, ��Hash����ע�Ტ����һ������OSRef,
			��ʱ����RTPSession���� */
			/* ͨ��while()ѭ������activationError��ֵֻ����QTSS_NoErr,��������EPERM,��retry,ֱ���ҵ�Ψһ��keyֵ */
            activationError = fRTPSession->Activate(fLastRTPSessionID);
        }
        Assert(activationError == QTSS_NoErr);
    }
	/* �ٴ�ȷ��������һ����!�������fLastRTPSessionID */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

    // Activate adds this session into the RTP session map. We need to therefore
    // make sure to resolve the RTPSession object out of the map, even though
    // we don't actually need to pointer.
	/* ͨ��ָ���ļ�ֵ�ַ���(�����Ψһ��RTPSession ID)ȥʶ���ȡ�ù�ϣ���е�(��ϣ��ԪRTPSession)����,�������ǻ���û��Ҫָ����RTPSession 
	(�����Ǽ����һ�ر���)*/
	/* ע�����������ʹ�� */
    OSRef* theRef = inRefTable->Resolve(&fLastRTPSessionIDPtr);
    Assert(theRef != NULL);
    
    return QTSS_NoErr;
}

/* �μ�RTPSessionInterface.h����Ӧ���ݳ�Ա */
/* ����RTSPRequest�еĲ���,����ȡRTSPSession Dictionary�е�����ֵ,������Client��Session��QTSS_ClientSessionAttributes���Բ���. */
void RTSPSession::SetupClientSessionAttrs()
{
    // get and pass presentation url
    StrPtrLen* theValue = fRequest->GetValue(qtssRTSPReqURI);
    Assert(theValue != NULL);
    (void)fRTPSession->SetValue(qtssCliSesPresentationURL, 0, theValue->Ptr, theValue->Len, QTSSDictionary::kDontObeyReadOnly);
    
    // get and pass full request url(includes rtsp://domain.com prefix)
    theValue = fRequest->GetValue(qtssRTSPReqAbsoluteURL);
    Assert(theValue != NULL);
    (void)fRTPSession->SetValue(qtssCliSesFullURL, 0, theValue->Ptr, theValue->Len, QTSSDictionary::kDontObeyReadOnly);
    
    // get and pass request host name
    theValue = fRequest->GetHeaderDictionary()->GetValue(qtssHostHeader);
    Assert(theValue != NULL);   
    (void)fRTPSession->SetValue(qtssCliSesHostName, 0, theValue->Ptr, theValue->Len, QTSSDictionary::kDontObeyReadOnly);

    // get and pass user agent header
    theValue = fRequest->GetHeaderDictionary()->GetValue(qtssUserAgentHeader);
    Assert(theValue != NULL);   
    (void)fRTPSession->SetValue(qtssCliSesFirstUserAgent, 0, theValue->Ptr, theValue->Len, QTSSDictionary::kDontObeyReadOnly);

    // get and pass CGI params(RTSP request query string)
    if (fRequest->GetMethod() == qtssDescribeMethod)
    {
        theValue = fRequest->GetValue(qtssRTSPReqQueryString);
        Assert(theValue != NULL);   
        (void)fRTPSession->SetValue(qtssCliSesReqQueryString, 0, theValue->Ptr, theValue->Len, QTSSDictionary::kDontObeyReadOnly);
    }
    
    // store RTSP session info in the RTPSession Dictionary. 
	// get and pass IP address addr of client
    StrPtrLen tempStr;
    tempStr.Len = 0;
    (void) this->GetValuePtr(qtssRTSPSesRemoteAddrStr, (UInt32) 0, (void **) &tempStr.Ptr, &tempStr.Len);
    Assert(tempStr.Len != 0);   
    (void) fRTPSession->SetValue(qtssCliRTSPSessRemoteAddrStr, (UInt32) 0, tempStr.Ptr, tempStr.Len, QTSSDictionary::kDontObeyReadOnly );
    
	// get and pass DNS name of local IP address
    tempStr.Len = 0;
    (void) this->GetValuePtr(qtssRTSPSesLocalDNS, (UInt32) 0, (void **) &tempStr.Ptr, &tempStr.Len);
    Assert(tempStr.Len != 0);   
    (void) fRTPSession->SetValue(qtssCliRTSPSessLocalDNS, (UInt32) 0,  (void **)tempStr.Ptr, tempStr.Len, QTSSDictionary::kDontObeyReadOnly );

	// get and pass local ip address string
    tempStr.Len = 0;
    (void) this->GetValuePtr(qtssRTSPSesLocalAddrStr, (UInt32) 0, (void **) &tempStr.Ptr, &tempStr.Len);
    Assert(tempStr.Len != 0);   
    (void) fRTPSession->SetValue(qtssCliRTSPSessLocalAddrStr, (UInt32) 0, tempStr.Ptr, tempStr.Len, QTSSDictionary::kDontObeyReadOnly );
}

/* ͨ��ƴ�������������,�õ�SInt64 theSessionID,�ٽ��ַ���ʽ��Ϊ���ioBuffer,�����ַ��������Ժ���ֵ���� */
UInt32 RTSPSession::GenerateNewSessionID(char* ioBuffer)
{
    //RANDOM NUMBER GENERATOR
    
    //We want to make our session IDs as random as possible, so use a bunch of
    //current server statistics to generate a random SInt64.

    //Generate the random number in two UInt32 parts. The first UInt32 uses
    //statistics out of a random RTP session.
	/* ����ǰʱ����Ϊ��������� */
    SInt64 theMicroseconds = OS::Microseconds();
    ::srand((unsigned int)theMicroseconds);
    UInt32 theFirstRandom = ::rand();
    
	/* ��ȡ�������ӿ� */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    {
		/* ����RTPSessionMap */
        OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
		/* ���RTPSessionMap��Hash Tableָ�� */
        OSRefHashTable* theHashTable = theServer->GetRTPSessionMap()->GetHashTable();
        if (theHashTable->GetNumEntries() > 0)
        {
			/* ��һ����������ǵ�ǰ��ϣ��Ԫ����������,�ٳ���2 */
            theFirstRandom %= theHashTable->GetNumEntries();
            theFirstRandom >>= 2;
            
			/* ���ɲ���ʼ��OSHashTable�ĵ�����Ķ��� */
            OSRefHashTableIter theIter(theHashTable);
            //Iterate through the session map, finding a random session
			/* ��������RTPSessionMap,�ҵ��׸���������theFirstRandom��HashTable��Ԫ */
            for (UInt32 theCount = 0; theCount < theFirstRandom; theIter.Next(), theCount++)
				/* ȷ����ǰHashTable��Ԫ�ǿ� */
                Assert(!theIter.IsDone());
            
			/* ��ȡ��ǰHashTable��Ԫ���ڵ�RTPSession���� */
            RTPSession* theSession = (RTPSession*)theIter.GetCurrent()->GetObject();
			/* ��ϵ�ǰRTPSession statisticsʹ�õ�һ�����������ø���� */
            theFirstRandom += theSession->GetPacketsSent();
            theFirstRandom += (UInt32)theSession->GetSessionCreateTime();
            theFirstRandom += (UInt32)theSession->GetPlayTime();
            theFirstRandom += (UInt32)theSession->GetBytesSent();
        }
    }

    //Generate the first half of the random number
	/* ʹ��ǰ��һ���������Ϊ����,�ٴβ�������� */
    ::srand((unsigned int)theFirstRandom);
    theFirstRandom = ::rand();
    
    //Now generate the second half
	/* ��������ڶ���������� */
    UInt32 theSecondRandom = ::rand();
    theSecondRandom += theServer->GetCurBandwidthInBits();
    theSecondRandom += theServer->GetAvgBandwidthInBits();
    theSecondRandom += theServer->GetRTPPacketsPerSec();
    theSecondRandom += (UInt32)theServer->GetTotalRTPBytes();
    theSecondRandom += theServer->GetTotalRTPSessions();
    
	/* ���Եڶ����������Ϊ����,���ɵڶ���������� */
    ::srand((unsigned int)theSecondRandom);
    theSecondRandom = ::rand();
    
	/* ͨ��ƴ�������������,�õ�SInt64 theSessionID,�ٽ��ַ���ʽ��Ϊ���ioBuffer,���ַ��������Ժ���ֵ���� */
    SInt64 theSessionID = (SInt64)theFirstRandom;
    theSessionID <<= 32;
    theSessionID += (SInt64)theSecondRandom;
    qtss_sprintf(ioBuffer, "%"_64BITARG_"d", theSessionID);
    Assert(::strlen(ioBuffer) < QTSS_MAX_SESSION_ID_LENGTH);
    return ::strlen(ioBuffer);
}

/* �жϷ������Ƿ񳬹����������?��������true,���򷵻�false. ע����RTSPSession::IsOkToAddNewRTPSession()�����Ϊ0 */
Bool16 RTSPSession::OverMaxConnections(UInt32 buffer)
{
	/* ��ȡ�������ӿ� */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
	/* ��streamingserver.xml��Ĭ��Ϊ1000 */
    SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();

	/* ���������������������? */
    Bool16 overLimit = false;
    
    if (maxConns > -1) // limit connections
    { 
		/* ȷ����������� */
        UInt32 maxConnections = (UInt32) maxConns + buffer;
		/* �жϵ�ǰ��RTPSession������RTSPSession�����Ƿ�ᳬ��1000? */
        if  ( (theServer->GetNumRTPSessions() > maxConnections) 
              ||
              ( theServer->GetNumRTSPSessions() + theServer->GetNumRTSPHTTPSessions() > maxConnections ) 
            )
        {
            overLimit = true;          
        }
    } 
    
    return overLimit;
     
}

/* �ӷ������ĵ�ǰ״̬,���������,������,���жϷ������ܷ�����µ�RTPSession ?�����ܼ������Client���ʹ�����Ӧ */
QTSS_Error RTSPSession::IsOkToAddNewRTPSession()
{
	/* ��ȡ�������ӿ�,������ȡ������״̬ */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    QTSS_ServerState theServerState = theServer->GetServerState();
    
    //we may want to deny this connection for a couple of different reasons
    //if the server is refusing new connections
	/* ���������״̬�ܾ����� */
    if ((theServerState == qtssRefusingConnectionsState) ||
        (theServerState == qtssIdleState) ||
        (theServerState == qtssFatalErrorState) ||
        (theServerState == qtssShuttingDownState))
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssServerUnavailable,
                                                    qtssMsgRefusingConnections);

    //if the max connection limit has been hit 
	/* ���糬���趨����������� */
    if  ( this->OverMaxConnections(0)) 
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
                                                    qtssMsgTooManyClients);

    //if the max bandwidth limit has been hit
	/* ���糬��������(Bits/sec) */
    SInt32 maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
    if ( (maxKBits > -1) && (theServer->GetCurBandwidthInBits() >= ((UInt32)maxKBits*1024)) )
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
                                                    qtssMsgTooMuchThruput);

    //if the server is too loaded down (CPU too high, whatever)
    // --INSERT WORKING CODE HERE--
    
    return QTSS_NoErr;                                                  
}

/* ��RTSP request�л�ȡ�û���\����\URL Realm,����RTSPSession��RTPSession����Ӧ����,��û��,ʹ�÷�����Ԥ��ֵ */
void RTSPSession::SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest)
{
    // Set the RTSP session's copy of the user name
	/* ��RTSP request�л�ȡ�û��� */
    StrPtrLen* tempPtr = theRTSPRequest->GetValue(qtssRTSPReqUserName);
    Assert(tempPtr != NULL);
	/* �������ȡ���û���������RTSPSession��RTPSession���û��� */
    if (tempPtr)
    {   (void)this->SetValue(qtssRTSPSesLastUserName, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
        (void)fRTPSession->SetValue(qtssCliRTSPSesUserName, (UInt32) 0, tempPtr->Ptr, tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
    }
    
    // Same thing... user password
	/* ��RTSP request�л�ȡ���� */
    tempPtr = theRTSPRequest->GetValue(qtssRTSPReqUserPassword);
    Assert(tempPtr != NULL);
	/* �������ȡ������������RTSPSession��RTPSession������ */
    if (tempPtr)
    {   (void)this->SetValue(qtssRTSPSesLastUserPassword, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
        (void)fRTPSession->SetValue(qtssCliRTSPSesUserPassword, (UInt32) 0, tempPtr->Ptr, tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
    }
    
	/* ��RTSP request�л�ȡURL Realm */
    tempPtr = theRTSPRequest->GetValue(qtssRTSPReqURLRealm);
    if (tempPtr)
    {
        if (tempPtr->Len == 0)
        {
            // If there is no realm explicitly specified in the request, then let's get the default out of the prefs
            OSCharArrayDeleter theDefaultRealm(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
			/* ��ȡ������Realm��Ԥ��ֵ */
            char *realm = theDefaultRealm.GetObject();
            UInt32 len = ::strlen(theDefaultRealm.GetObject());
			/* ʹ�÷�����Realm��Ԥ��ֵ������RTSPSession��RTPSession����Ӧ�ֶ� */
            (void)this->SetValue(qtssRTSPSesLastURLRealm, 0, realm, len,QTSSDictionary::kDontObeyReadOnly);
            (void)fRTPSession->SetValue(qtssCliRTSPSesURLRealm, (UInt32) 0,realm,len, QTSSDictionary::kDontObeyReadOnly );
        }
        else
        {
			/* ʹ�ô�RTSP request�л�ȡ��URL Realm������RTSPSession��RTPSession����Ӧ�ֶ� */
            (void)this->SetValue(qtssRTSPSesLastURLRealm, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
            (void)fRTPSession->SetValue(qtssCliRTSPSesURLRealm, (UInt32) 0,tempPtr->Ptr,tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
        }
    }
}

/* ����RTSPRequestInterface::Read(),ȡ��RTSP request������,������ʱ���� */
QTSS_Error RTSPSession::DumpRequestData()
{
    char theDumpBuffer[2048];
    
    QTSS_Error theErr = QTSS_NoErr;
    while (theErr == QTSS_NoErr)
        theErr = this->Read(theDumpBuffer, 2048, NULL);
        
    return theErr;
}

/* �Ա���ϵ���ܵ�RTSPSessionInterface::InterleavedWrite() */
void RTSPSession::HandleIncomingDataPacket()
{
    
    // Attempt to find the RTP session for this request.
	/* ��RTSP Request�л�ȡRTCP�����ڵ�ͨ���� */
    UInt8   packetChannel = (UInt8)fInputStream.GetRequestBuffer()->Ptr[1];
	/* ��ͨ���ŵõ���Ӧ��RTSPSession ID */
    StrPtrLen* theSessionID = this->GetSessionIDForChannelNum(packetChannel);
    
    if (theSessionID == NULL)
    {
        Assert(0);
        theSessionID = &fLastRTPSessionIDPtr;

    }
    
	/* ��RTPSession Map�ҵ�ָ���ỰID��ref,�Ӷ��õ������ڵ�RTPSession */
    OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
    OSRef* theRef = theMap->Resolve(theSessionID);
    
    if (theRef != NULL)
        fRTPSession = (RTPSession*)theRef->GetObject();

	/* ��û��RTPSession,ֱ�ӷ��� */
    if (fRTPSession == NULL)
        return;

	/* �õ�RTCP������ʵ����,����RTPInterleaveHeader��4���ֽ� */
    StrPtrLen packetWithoutHeaders(fInputStream.GetRequestBuffer()->Ptr + 4, fInputStream.GetRequestBuffer()->Len - 4);
    
	/* ��ȡRTPSession������,��ˢ�³�ʱ */
    OSMutexLocker locker(fRTPSession->GetMutex());
	/* ˢ�¸�RTPSession,�Է�������ɾȥ */
    fRTPSession->RefreshTimeout();
	/* �ɰ��Ų���ָ���ĸ�RTPSession�е�RTPStream */
    RTPStream* theStream = fRTPSession->FindRTPStreamForChannelNum(packetChannel);
    theStream->ProcessIncomingInterleavedData(packetChannel, this, &packetWithoutHeaders);

    // We currently don't support async notifications from within this role
	/* ����packetParams.rtspIncomingDataParams */
    QTSS_RoleParams packetParams;
    packetParams.rtspIncomingDataParams.inRTSPSession = this;
    packetParams.rtspIncomingDataParams.inClientSession = fRTPSession;//����RTPSession
    packetParams.rtspIncomingDataParams.inPacketData = fInputStream.GetRequestBuffer()->Ptr;
    packetParams.rtspIncomingDataParams.inPacketLen = fInputStream.GetRequestBuffer()->Len;
    
	/* ����ע��QTSSModule::kRTSPIncomingDataRole��ģ��,���ַ����� */
    UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPIncomingDataRole);
    for (; fCurrentModule < numModules; fCurrentModule++)
    {
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPIncomingDataRole, fCurrentModule);
        (void)theModule->CallDispatch(QTSS_RTSPIncomingData_Role, &packetParams);
    }
    fCurrentModule = 0;
}
