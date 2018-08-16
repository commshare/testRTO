
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

/*  打印指定的StrPtrLen  */
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
	/* 创建http proxy tunnel map */
    sHTTPProxyTunnelMap = new OSRefTable(OSRefTable::kDefaultTableSize);

    // Construct premade HTTP response for HTTP proxy tunnel
	/* 构造sHTTPResponseHeaderBuf缓存 */
    qtss_sprintf(sHTTPResponseHeaderBuf, sHTTPResponseFormatStr, "","","", QTSServerInterface::GetServerHeader().Ptr);
    sHTTPResponseHeaderPtr.Len = ::strlen(sHTTPResponseHeaderBuf);
    Assert(sHTTPResponseHeaderPtr.Len < kMaxHTTPResponseLen);
    
    /* 构造sHTTPResponseNoServerHeaderBuf缓存 */
    qtss_sprintf(sHTTPResponseNoServerHeaderBuf, sHTTPNoServerResponseFormatStr, "","","","");
    sHTTPResponseNoServerHeaderPtr.Len = ::strlen(sHTTPResponseNoServerHeaderBuf);
    Assert(sHTTPResponseNoServerHeaderPtr.Len < kMaxHTTPResponseLen);
        
}

/* used in RTSPListenerSocket::GetSessionTask() */
// this inParam tells the server to report its IP address in the reply to the HTTP GET request when tunneling RTSP through HTTP
RTSPSession::RTSPSession( Bool16 doReportHTTPConnectionAddress )
: RTSPSessionInterface(),
  fRequest(NULL),/* 创建在RTSPSession::Run() */
  fRTPSession(NULL),/* 还没创建RTPSession,创建在RTSPSession::SetupRequest() */
  fReadMutex(),
  fHTTPMethod( kHTTPMethodInit ),
  fWasHTTPRequest( false ),
  fFoundValidAccept( false),
  fDoReportHTTPConnectionAddress(doReportHTTPConnectionAddress),//由入参确定,是否对Client报告流服务器的ip地址?
  fCurrentModule(0),
  fState(kReadingFirstRequest) /* RTSPSession中状态机的初始状态,注意此处初值在RTSPSession::Run()中要使用 */
{
    this->SetTaskName("RTSPSession");

    // must guarantee this map is present
    Assert(sHTTPProxyTunnelMap != NULL);
    
	/* 更新当前RTSPSession总数,增加1 */
    QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(1);

    // Setup the QTSS Role param block, as none of these fields will change through the course of this session.
	//搭建会话参数块,它们在整个RTSPSession中不变
    fRoleParams.rtspRequestParams.inRTSPSession = this;
    fRoleParams.rtspRequestParams.inRTSPRequest = NULL;
    fRoleParams.rtspRequestParams.inClientSession = NULL;/* 代表RTPSession */
    
	//setup QTSS_ModuleState
    fModuleState.curModule = NULL;
    fModuleState.curTask = this;
    fModuleState.curRole = 0;
    fModuleState.globalLockRequested = false;
       
	//第一个字符设为0
    fProxySessionID[0] = 0;
    fProxySessionIDPtr.Set( fProxySessionID, 0 );//设置Proxy Session ID

	//第一个字符设为0,注意fLastRTPSessionID设置,参见RTSPSession::CreateNewRTPSession()
    fLastRTPSessionID[0] = 0;
    fLastRTPSessionIDPtr.Set( fLastRTPSessionID, 0 );
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);
                    
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sBroadcasterSessionName, &sClientBroadcastSessionAttr);

}

/* 设置rtspSessionClosingParams参数,并调用注册QTSSModule::kRTSPSessionClosingRole角色的模块,使当前RTSPSession总数减1,从sHTTPProxyTunnelMap中删除fProxyRef */
RTSPSession::~RTSPSession()
{
    // Invoke the session closing modules
    QTSS_RoleParams theParams;
    theParams.rtspSessionClosingParams.inRTSPSession = this;
    
    // Invoke modules
	//调用注册QTSSModule::kRTSPSessionClosingRole角色的模块
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPSessionClosingRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kRTSPSessionClosingRole, x)->CallDispatch(QTSS_RTSPSessionClosing_Role, &theParams);

    this->CleanupRequest();// Make sure that all our objects are deleted

	/* 更新当前RTSPSession总数,减1 */
    if (fSessionType == qtssRTSPSession)
        QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(-1);
    else
        QTSServerInterface::GetServer()->AlterCurrentRTSPHTTPSessionCount(-1);
    
	/* 从sHTTPProxyTunnelMap中删除fProxyRef */
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


/* 这个函数主要与第三方Modules打交道. */
SInt64 RTSPSession::Run()
{
	//取出事件
    EventFlags events = this->GetEvents();
    QTSS_Error err = QTSS_NoErr;

	/* 下面的状态机中会用到Module */

	/************** NOTE ！！*********************/
	/* 特定Module的指针，用来设置SendPacket的Module */
    QTSSModule* theModule = NULL;
	/* Module总数 */
    UInt32 numModules = 0;

	/* 确保两者描述的是一个字符串,亦见RTSPSession::RTSPSession( Bool16 doReportHTTPConnectionAddress ) */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);
    // Some callbacks look for this struct in the thread object
	// 设定当前的Module 状态
    OSThreadDataSetter theSetter(&fModuleState, NULL);
        
    //check for a timeout or a kill. If so, just consider the session dead
    if ((events & Task::kTimeoutEvent) || (events & Task::kKillEvent))
        fLiveSession = false;
    
	//如果是live session，执行状态机
    while (this->IsLiveSession())
    {
		/* 提示：下面是RTSPSession 的状态机。因为在处理RTSP 请求过程中，有
		多个地方需要Run 方法返回以便继续监听新的事件。为此，我们需要跟踪当前
		的运行状态，以便在被打断后还能回到原状态*/
        // RTSP Session state machine. There are several well defined points in an RTSP request
        // where this session may have to return from its run function and wait for a new event.
        // Because of this, we need to track our current state and return to it.
        
        switch (fState)
        {
			/* 第一次请求到达进入kReadingFirstRequest 状态，该状态主要负责从
			RTSPRequestStream 类的对象fInputStream 中读出客户的RTSP 请求 */
            case kReadingFirstRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kReadingFirstRequest\n" )
                if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
                {
					/* RequestStream返回QTSS_NoErr 意味着所有数据已经
					 从Socket 中读出，但尚不能构成一个完整的请求，因此必须等待更多的数据到达 */

                    // If the RequestStream returns QTSS_NoErr, it means
                    // that we've read all outstanding data off the socket,
                    // and still don't have a full request. Wait for more data.
                    
                    //+rt use the socket that reads the data, may be different now.
					//还没有得到完整的RTSP request,请求继续读数据
                    fInputSocketP->RequestEvent(EV_RE);
					//Run函数返回，等待下一个事件发生
                    return 0;
                }
                
				//出错，停止处理
                if ((err != QTSS_RequestArrived) && (err != E2BIG))
                {
                    // Any other error implies that the client has gone away. At this point,
                    // we can't have 2 sockets, so we don't need to do the "half closed" check
                    // we do below
                    Assert(err > 0); 
                    Assert(!this->IsLiveSession());
                    break;
                }

				//RTSP请求已经完全到达，转入kHTTPFilteringRequest 状态
                if (err == QTSS_RequestArrived)
                    fState = kHTTPFilteringRequest;

				//接收缓冲区溢出，转入kHaveNonTunnelMessage 状态
                // If we get an E2BIG, it means our buffer was overfilled.
                // In that case, we can just jump into the following state, and
                // the code their does a check for this error and returns an error.
                if (err == E2BIG)
                    fState = kHaveNonTunnelMessage;
            }
            continue;//反复进行
            
			/* 正常情况下，在获得一个完整的RTSP请求后,系统将进入
			kHTTPFilteringRequest状态.该状态检查RTSP连接是否需要经过HTTP代理实
			现；如不需要，转入kHaveNonTunnelMessage 状态。 */
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
                
            /* 非第一次读取RTSP Request的数据 */
            case kReadingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kReadingRequest\n" )
				/* 当读取data时要加锁,以防POST溜进来 */
                // We should lock down the session while reading in data,
                // because we can't snarf up a POST while reading.
                OSMutexLocker readMutexLocker(&fReadMutex);

                // we should be only reading an RTSP request here, no HTTP tunnel messages
                
                if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
                {
					/* 读取数据后没有得到完整的RTSP Request,所以申请继续读取数据 */
                    // If the RequestStream returns QTSS_NoErr, it means
                    // that we've read all outstanding data off the socket,
                    // and still don't have a full request. Wait for more data.
                    
                    //+rt use the socket that reads the data, may be different now.
					/* 再次申请读event */
                    fInputSocketP->RequestEvent(EV_RE);
                    return 0;
                }
                
				/* 出错,断掉连接 */
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
						/* 关闭socket,从event queue中清除它 */
                        fInputSocketP->Cleanup();
                        return 0;
                    }
                    else
                    {  //这是一个dead session
                        Assert(!this->IsLiveSession());
                        break;
                    }
                }
                fState = kHaveNonTunnelMessage;
                // fall thru to kHaveNonTunnelMessage
            }
            
			/* 进入kHaveNonTunnelMessage 状态后，系统创建了RTSPRequest 类的对象
			fRequest，该对象解析客户的RTSP 请求，并保存各种属性。fRequest 对象被传
			递给其他状态处理。 */
            case kHaveNonTunnelMessage:
            {   
				HTTP_TRACE( "RTSPSession::Run kHaveNonTunnelMessage\n" )
                // should only get here when fInputStream has a full message built.
                /* 得到一个RTSP request buffer的起始地址 */
                Assert( fInputStream.GetRequestBuffer() );
                
                Assert(fRequest == NULL);
				/* 创建RTSPRequest实例对象 */
                fRequest = NEW RTSPRequest(this);
                fRoleParams.rtspRequestParams.inRTSPRequest = fRequest;
				/* 获取RTSPRequest字典 */
                fRoleParams.rtspRequestParams.inRTSPHeaders = fRequest->GetHeaderDictionary();

				/* 现在对full RTSP Request进行处理,加锁以防处理过程被打断,直至我们发送Response */
                // We have an RTSP request and are about to begin processing. We need to
                // make sure that anyone sending interleaved data on this session won't
                // be allowed to do so until we are done sending our response
                // We also make sure that a POST session can't snarf in while we're
                // processing the request.
				/* 防止POST snarf 打断 */
                fReadMutex.Lock();
				/* 防止RTPSession打断 */
                fSessionMutex.Lock();
                
				/* 将RTSP Response的写入字节总数重置为0,当下一个RTSP Response到来时才重新计数 */
                // The fOutputStream's fBytesWritten counter is used to
                // count the #(总数) of bytes for this RTSP response. So, at
                // this point, reset it to 0 (we can then just let it increment
                // until the next request comes in)
                fOutputStream.ResetBytesWritten();
                
                // Check for an overfilled buffer, and return an error.
				/* 当接收缓冲溢出时,设置状态机为kPostProcessingRequest */
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

				/* 确保一个完整的RTSP Request到来,转入kFilteringRequest */
                Assert(err == QTSS_RequestArrived);
                fState = kFilteringRequest;
                
				/* 注意这里没有打断,我们想继续进入下一步 */
                // Note that there is no break here. We'd like to continue onto the next
                // state at this point. This goes for every case in this case statement
            }
            
			/* 接着进入kFilteringRequest 状态，二次开发人员可以通过编写Module 对客
			户的请求做出特殊处理。如果客户的请求为正常的RTSP 请求，系统调用
			SetupRequest 函数建立用于管理数据传输的RTPSession 类对象 */
            case kFilteringRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kFilteringRequest\n" )
                // We received something so auto refresh
                // The need to auto refresh is because the api doesn't allow a module to refresh at this point
                // 
                fTimeoutTask.RefreshTimeout();/* 至此已经接收到完整的Client端数据,立即刷新超时,以免任务线程删去该RTSPSession对象 */

                //
                // Before we even do this, check to see if this is a *data* packet,
                // in which case this isn't an RTSP request, so we don't need to go
                // through any of the remaining steps
                /* 假如接收的是数据包,就解析,退出状态机 */
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
				/* 统计注册kRTSPFilterRole的Module个数 */
                numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPFilterRole);
				/* 对当前注册模块,在没发送RTSPResponse时或已申请event时,作下列处理 */
                for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                {
                    fModuleState.eventRequested = false;
                    fModuleState.idleTime = 0;
                    if (fModuleState.globalLockRequested )
                    {   
						fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    }
                    
					/* 获取指定序号的注册kRTSPFilterRole的Module */
					/**************** NOTE！！ ********************************/
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPFilterRole, fCurrentModule);
					/* 调用该module */
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
				/* 假如已经发送Response,就跳到后处理阶段;注意发送Response在kSendingRequest阶段 */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }

				/* 当发送了Options request(内容为"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"),就解析收到的Options Response,并计算往返时间 */
				if (fSentOptionsRequest && this->ParseOptionsResponse())
				{
					/* 计算Options发送和返回的往返时间 */
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
                /* 假如已发送Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }

                fState = kRoutingRequest;
            }


			/* 进入kRoutingRequest 状态，调用二次开发人员加入的Module，用于将该
			请求路由（Routing）出去。缺省情况下，系统本身对此状态不做处理。 */
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
                        
						/* 获取指定序号的注册kRTSPRouteRole的Module */
                        theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPRouteRole, fCurrentModule);
						/* 调用该module */
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
                
				/* 发送Response */
                if (fRequest->HasResponseBeenSent())
                {
                    fState = kPostProcessingRequest;
                    break;
                }
                
				/* 假如跳过Authorize阶段,默认要Authorize */
                if(fRequest->SkipAuthorization())
                {
                    // Skip the authentication and authorization states
                    
                    // The foll. normally gets executed at the end of the authorization state 
                    // Prepare for kPreprocessingRequest state.
                    fState = kPreprocessingRequest;

					/* 假如当前RTSP method是SETUP */
                    if (fRequest->GetMethod() == qtssSetupMethod)
                    // Make sure to erase the session ID stored in the request at this point.
                    // If we fail to do so, this same session would be used if another
                    // SETUP was issued on this same TCP connection.
                        fLastRTPSessionIDPtr.Len = 0;
                    else if (fLastRTPSessionIDPtr.Len == 0)
                        fLastRTPSessionIDPtr.Len = ::strlen(fLastRTPSessionIDPtr.Ptr); 
                        
                    break;
                }
                else/* 否则进入Authorize阶段 */
                    fState = kAuthenticatingRequest;
            }
            
			/* 进入kAuthenticatingRequest 状态，调用二次开发人员加入的安全模块，主
			要用于客户身份验证以及其他如规则的处理。读者如果希望开发具有商业用途的
			流式媒体服务器，该模块必须进行二次开发。 */
            case kAuthenticatingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kAuthenticatingRequest\n" )
                /* 得到client发送的RTSP method */
                QTSS_RTSPMethod method = fRequest->GetMethod();
                if (method != qtssIllegalMethod) do  
                {   //Set the request action before calling the authentication module
                    /* 假如request method是Announce或者Setup中的Record mode,设为W flag */
                    if((method == qtssAnnounceMethod) || ((method == qtssSetupMethod) && fRequest->IsPushRequest()))
                    {   fRequest->SetAction(qtssActionFlagsWrite);
                        break;
                    }
                    
                    void* theSession = NULL;
                    UInt32 theLen = sizeof(theSession);
					/* 假如client请求的是broadcast session */
                    if (QTSS_NoErr == fRTPSession->GetValue(sClientBroadcastSessionAttr, 0,  &theSession, &theLen) )
                    {   fRequest->SetAction(qtssActionFlagsWrite); // an incoming broadcast session
                        break;
                    }

					/* 设置RTSPRequest为read flag */
                    fRequest->SetAction(qtssActionFlagsRead);
                } while (false);
                else
                {   Assert(0);
                }
                
				/* 假如Authorize级别是无,那么就获取服务器预设值中的设置 */
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

					/* 得到Authorize module */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPAthnRole, 0);
					/* 调用Authorize module */
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
                
                this->CheckAuthentication();//检查认证是否通过?
                                                
                fCurrentModule = 0;
				/* 假如已发送Response */
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

				/* 设置允许Authorize */
                fRequest->SetAllowed(allowed);                  
            
				/* 循环调用Module */
                for (; (fCurrentModule < numModules) && ((!fRequest->HasResponseBeenSent()) || fModuleState.eventRequested); fCurrentModule++)
                {
                    fModuleState.eventRequested = false;
                    fModuleState.idleTime = 0;
                    if (fModuleState.globalLockRequested )
                    {   fModuleState.globalLockRequested = false;
                        fModuleState.isGlobalLocked = true;
                    } 
                    
					/* 得到Authorize module */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPAuthRole, fCurrentModule);
					/* 调用Authorize module */
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
					/* 正常情况下,allowed值为初值true,Module出错后变为false,退出循环 */
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
                
				/* 保存Authorize认证参数 */
                this->SaveRequestAuthorizationParams(fRequest);

                if (!allowed) 
                {   /* 在发送Response前 */
                    if (false == fRequest->HasResponseBeenSent())
                    {
						/* 获取client quest的Authorize级别 */
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

						/* 假如发送Authorize Response出错,就中断该request */
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
				/* 假如已经发送了Response */
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
            
			/* 进入kPreprocessingRequest 和kProcessingRequest 及kPostProcessingRequest
			状态，这三种状态都是通过调用系统自带或二次开发人员添加的Module 来处理
			RTSP 请求，例如系统提供了QTSSReflector Module、QTSSSplitter Module 以及
			QTSSFileModule 等模块。 */
            case kPreprocessingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kPreprocessingRequest\n" )
                // Invoke preprocessor modules
				/* 得到注册kRTSPPreProcessorRole的Module个数 */
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
                        
						/* 得到注册kRTSPPreProcessorRole的模块并调用 */
                        theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPPreProcessorRole, fCurrentModule);
                        (void)theModule->CallDispatch(QTSS_RTSPPreProcessor_Role, &fRoleParams);
                        fModuleState.isGlobalLocked = false;

                        // The way the API is set up currently, the first module that adds a stream
                        // to the session is responsible for sending RTP packets for the session.
						/* 第一个将RTPStream加入RTP Session的Module负责send Packet */
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
				/* 发送Response */
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
					/* 用来获取注册QTSSModule::kRTSPRequestRole的Module,只能是QTSSFileModule */
                    theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPRequestRole, 0);
					/* 调用该Moudule */
                    (void)theModule->CallDispatch(QTSS_RTSPRequest_Role, &fRoleParams);
                    fModuleState.isGlobalLocked = false;

                    // Do the same check as above for the preprocessor
					/* 类似kPreprocessingRequest角色作同样的处理 */
                    if (fRTPSession->HasAnRTPStream() && fRTPSession->GetPacketSendingModule() == NULL)
						/* 设置SendPacket模块为QTSSFileModule */
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
                
                
                /* 发送Response */
                if (!fRequest->HasResponseBeenSent())
                {
                    // no modules took this one so send back a parameter error
					/* 假如从client得到的method是SetParameter */
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

			/* 在发送Response之前,后处理RTSP request */
            case kPostProcessingRequest:
            {
				HTTP_TRACE( "RTSPSession::Run kPostProcessingRequest\n" )
                // Post process the request *before* sending the response. Therefore, we
                // will post process regardless of whether the client actually gets our response
                // or not.
                
                //if this is not a keepalive request, we should kill the session NOW
				/* 通过fResponseKeepAlive的值(仅在RTPSession::SendTeardownResponse()中设置为false)判断是否是活跃的RTSPSession,这里是true */
                fLiveSession = fRequest->GetResponseKeepAlive();
                
                if (fRTPSession != NULL)
                {
                    // Invoke postprocessor modules only if there is an RTP session. We do NOT want
                    // postprocessors running when filters or syntax errors have occurred in the request!
					/* 获得注册kRTSPPostProcessorRole的模块数目 */
                    numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPPostProcessorRole);
                    {
                        // Manipulation of the RTPSession from the point of view of
                        // a module is guarenteed to be atomic by the API.
                        OSMutexLocker   locker(fRTPSession->GetSessionMutex());
    
                        // Make sure the RTPSession contains a copy of the realStatusCode in this request
						/* 得到当前RTSPSession的status code,比如404(找不到client)并设置RTSPSession字典的相应属性值 */
                        UInt32 realStatusCode = RTSPProtocol::GetStatusCode(fRequest->GetStatus());
                        (void) fRTPSession->SetValue(qtssCliRTSPReqRealStatusCode,(UInt32) 0,(void *) &realStatusCode, sizeof(realStatusCode), QTSSDictionary::kDontObeyReadOnly);

                        // Make sure the RTPSession contains a copy of the qtssRTSPReqRespMsg in this request
						/* 得到当前RTSPSession的RespMsg,并设置RTSPSession字典的相应属性值 */
                        StrPtrLen* theRespMsg = fRequest->GetValue(qtssRTSPReqRespMsg);
                        if (theRespMsg->Len > 0)
                            (void)fRTPSession->SetValue(qtssCliRTSPReqRespMsg, 0, theRespMsg->Ptr, theRespMsg->Len, QTSSDictionary::kDontObeyReadOnly);
                
                        // Set the current RTSP session for this RTP session.
                        // We do this here because we need to make sure the SessionMutex
                        // is grabbed while we do this. Only do this if the RTSP session
                        // is still alive, of course
						/* 为当前的RTP session更新RTSP Session */
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
                            
							/* 得到注册kRTSPPostProcessorRole的Module并调用 */
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

			/* 进入kSendingResponse 状态，用于发送对客户RTSP请求处理完成之后的
			响应。系统在该状态调用了fOutputStream.Flush()函数将在fOutputStream 中尚未
			发出的请求响应通过Socket 端口完全发送出去。 */
            case kSendingResponse:
            {
				HTTP_TRACE( "RTSPSession::Run kSendingResponse\n" )
                // Sending the RTSP response consists of making sure the
                // RTSP request output buffer is completely flushed(清空) to the socket.
                Assert(fRequest != NULL);
                
				// If x-dynamic-rate header is sent with a value of 1, send OPTIONS request
				/* 假如RTSP method是Setup且返回200 OK,得到的"x-dynamic-rate: 1\r\n",需要计算往返时间(RTT),备份并重置fOutputStream的缓存,发送Options request */
				if ((fRequest->GetMethod() == qtssSetupMethod) && (fRequest->GetStatus() == qtssSuccessOK)
				    && (fRequest->GetDynamicRateState() == 1) && fRoundTripTimeCalculation)
				{
					this->SaveOutputStream();//备份fOutputStream缓存中的数据
					this->ResetOutputStream();//清空fOutputStream缓存中的数据,将数据指针置于开头
					this->SendOptionsRequest();//在fOutputStream中放入请求"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"
				}
			
				/* 假如已发送了Options request,但RTSP Method非法,将备份数据复制到fOutputStream中 */
				if (fSentOptionsRequest && (fRequest->GetMethod() == qtssIllegalMethod))
				{
					this->ResetOutputStream();//清空fOutputStream缓存中的数据,将数据指针置于开头
					this->RevertOutputStream();//解析fOldOutputStreamBuffer中的数据,并复制到fOutputStream中,删除fOldOutputStreamBuffer
					fSentOptionsRequest = false;
				}
				
				/* 先统计buffer中还有多少待送出的数据,再用Socket::Send()向外发送数据.只要能够送出数据,就refresh timeout.
				假如一次全部送出数据,就flush清空它,否则累计已送出的数据,返回EAGAIN */
                err = fOutputStream.Flush();//将fOutputStream中的数据全部发送给Client
                
				/* 若返回EAGAIN,就使用流控,请求tcp socket侦听W_R事件,当socket变为可写时再发送数据 */
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
            
			/* 进入kCleaningUp 状态，清除所有上次处理的数据，并将状态设置为
			kReadingRequest 等待下次请求到达。 */
            case kCleaningUp:
            {
				HTTP_TRACE( "RTSPSession::Run kCleaningUp\n" )
                // Cleaning up consists of making sure we've read all the incoming Request Body
                // data off of the socket
                if (this->GetRemainingReqBodyLen() > 0)
                {
					/* 打印出RTSP request的内容 */
                    err = this->DumpRequestData();
                    
					/* 若返回EAGAIN,就使用流控,请求tcp socket侦听W_R事件,当socket变为可写时再发送数据 */
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
				/* 清空当前RTSP request */
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
    // is holding onto a reference to this session, just reschedule(重新调度) the timeout.
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
    
	/* 打印该行 */
    HTTP_VTRACE( "ParseProxyTunnelHTTP in\n" ) 
		/* 获取RTSP Request的数据 */
    splRequest = fInputStream.GetRequestBuffer();
    fFoundValidAccept = true;
    Assert( splRequest );
    
    if ( splRequest )
    {
        fHTTPMethod = kHTTPMethodUnknown;
    
    #if __RTSP_HTTP_DEBUG__ 
        {
			/* 打印出获取的RTSP Request数据 */
            char    buff[1024];     
            memcpy( buff, splRequest->Ptr, splRequest->Len );    
            buff[ splRequest->Len] = 0;    
            HTTP_VTRACE( buff )
        }
    #endif

		/* 下面解析获取的RTSP Request数据 */
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

/* 将当前的RTSPSession的fProxyRef注册进HTTPProxyTunnelMap,有三种情况:(1)假如注册成功,返回当前RTSPSession的fProxyRef;
(2)假如另一个RTSPSession有相同的magic number和指定的sessionType, 返回另一个会话的fProxyRef;(3)假如注册成功,但这个
RTSPSession不是被唯一的user使用,或不是指定的sessionType,返回NULL. */
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

/* 检查RTSP request中client发送的认证级别和当前的会话级别是否相同?若不同,不使用认证;若相同,再逐一细化为三种情况:None/Basic/Digest,
   假如认证失败,或是认证通过但是过期,则将用户名/密码/用户组置空.
*/
void RTSPSession::CheckAuthentication() {
    
	/* 从RTSP request中获取用户名/密码 */
    QTSSUserProfile* profile = fRequest->GetUserProfile();
    StrPtrLen* userPassword = profile->GetValue(qtssUserPassword);
	/* 从RTSP request中获取认证级别:None/Basic/Digest */
    QTSS_AuthScheme scheme = fRequest->GetAuthScheme();

	/* 认证通过 */
    Bool16 authenticated = true;
    
    // Check if authorization information returned by the client is for the scheme that the server sent the challenge
	/* 检查RTSP request中client发送的认证级别和当前的会话级别是否相同? */
	/* 若不同,不使用认证 */
    if(scheme != (fRTPSession->GetAuthScheme())) {
        authenticated = false;
    }
	/* 若相同,再逐一细化为三种情况:None/Basic/Digest */
    else if(scheme == qtssAuthBasic) {  
        // For basic authentication, the authentication module returns the crypt of the password, 
        // so compare crypt of qtssRTSPReqUserPassword and the text in qtssUserPassword
		/* 比较从RTSP request获得的用户名/密码,和Module返回的加密过的用户名/密码 */
        StrPtrLen* reqPassword = fRequest->GetValue(qtssRTSPReqUserPassword);
        char* userPasswdStr = userPassword->GetAsCString(); // memory allocated
        char* reqPasswdStr = reqPassword->GetAsCString();   // memory allocated
        
		/* 假如加密后的RTSP request获得的用户名/密码,与Module返回的加密过的用户名/密码不同,认证为false */
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
	/* 假如认证失败,或是认证通过但是过期,则将用户名/密码/用户组置空 */
    if((!authenticated) || (authenticated && (fRequest->GetStale()))) {
        (void)profile->SetValue(qtssUserName, 0,  sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
        (void)profile->SetValue(qtssUserPassword, 0,  sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
        (void)profile->SetNumValues(qtssUserGroups, 0);
    }
}

// 紧密联系RTSPSessionInterface::SendOptionsRequest()
/* 从RTSP request获取full request data,判断这前4个字符是否是"RTSP"?返回true或false */
Bool16 RTSPSession::ParseOptionsResponse()
{
	/* 从RTSP request获取full request data */
	StringParser parser(fRequest->GetValue(qtssRTSPReqFullRequest));
	Assert(fRequest->GetValue(qtssRTSPReqFullRequest)->Ptr != NULL);
	static StrPtrLen sRTSPStr("RTSP", 4);
	StrPtrLen theProtocol;
	/* 将full request data中的前4个字符赋给theProtocol */
	parser.ConsumeLength(&theProtocol, 4);
	
	/* 判断这前4个字符是否是"RTSP"? */
	return (theProtocol.Equal(sRTSPStr));
}

/* 首先解析来自Client的full RTSP Request,处理不同的Methods,查找对应的RTP Session,没有就新建一个RTP Session.对Play Request作配置thinning parameters */
void RTSPSession::SetupRequest()
{
    //
    // First parse the request
	/* 先从client获取full RTSP Request,解析它的第一行和其他行,让Response header和Request header同步,提取Requested File path */
    QTSS_Error theErr = fRequest->Parse();
	/* 如果出错,直接返回 */
    if (theErr != QTSS_NoErr)
        return;
    
	//查找该请求的RTPSession
    // let's also refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
    // Attempt to find the RTP session for this request.
	/* 得到RTPSessionMap的Hash Table,以得到下面的RTPSessionMap表元 */
    OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();

	/* 从RTSPRequest中获取相对应的RTPSession的ID,若找到就识别并找出对应的HashTable元,进而找到相应的RTPSession;
	若没有找到,说明不是一个新的RTPSession,就用当前的fLastRTPSessionID[]代替RTPSession ID,找到相应的RTPSession */
    theErr = this->FindRTPSession(theMap);
    
	/* 刷新当前这个RTPSession的Timeout,已与RTSPSession保持同步 */
    if (fRTPSession != NULL)
        fRTPSession->RefreshTimeout();

    QTSS_RTSPStatusCode statusCode = qtssSuccessOK;
    char *body = NULL;
    UInt32 bodySizeBytes = 0;
    
	//OPTIONS 请求,简单发回标准OPTIONS响应(如下)即可
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
		/* 获取RTSP Request中的CSeq行RTSP header */
        StrPtrLen* cSeqPtr = fRequest->GetHeaderDictionary()->GetValue(qtssCSeqHeader);
		/* 假如CSeq头为空,向RTSPResponseStream中放入如下信息:
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

        /* 否则,向RTSPResponseStream中放入如下信息:
		RTSP/1.0 400 200 OK\r\n
		Server: QTSS/4.1.3.x(Build/425:Platform/MacOSX;Release/Development;)\r\n
		CSeq: 91
		Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n
		\r\n
		*/    
        fRequest->AppendHeader(qtssPublicHeader, QTSServerInterface::GetPublicHeader());

        // DJM PROTOTYPE
        StrPtrLen* requirePtr = fRequest->GetHeaderDictionary()->GetValue(qtssRequireHeader);
		/* 假如请求头是"x-Random-Data-Size",放入OPTIONS响应中,再附加"Content-length: 1209\r\n" */
        if ( requirePtr && requirePtr->EqualIgnoreCase(RTSPProtocol::GetHeaderString(qtssXRandomDataSizeHeader)) )
        {
			/* 位于fOutputStream中,参见RTSPSessionInterface::SendOptionsRequest() */
            body = (char*) RTSPSessionInterface::sOptionsRequestBody;//1400字节
			/* 获取"x-Random-Data-Size"头的数据长度 */
            bodySizeBytes = fRequest->GetRandomDataSize();
            Assert( bodySizeBytes <= sizeof(RTSPSessionInterface::sOptionsRequestBody) );//64K字节
			//附加"Content-Type: application/x-random-data"
            fRequest->AppendHeader(qtssContentTypeHeader, &sContentType);
			//附加"Content-length: 1209\r\n",注意长度是"x-Random-Data-Size"头的数据长度
            fRequest->AppendContentLength(bodySizeBytes);
        } 
		
		
		/* 将上述向Client发送的内容放入RTSPResponseStream */
		fRequest->SendHeader();
	    
	    // now write the body if there is one
		/* 向Client发送回RTSP响应 */
        if (bodySizeBytes > 0 && body != NULL)
            fRequest->Write(body, bodySizeBytes, NULL, 0);

        return;
    }

	// If this is a SET_PARAMETER request, don't let modules see it.
	if (fRequest->GetMethod() == qtssSetParameterMethod)
	{
         

		// Check that it has the CSeq header
		/* 获取RTSP Request中的CSeq行RTSP header */
		StrPtrLen* cSeqPtr = fRequest->GetHeaderDictionary()->GetValue(qtssCSeqHeader);
		/* /* 假如CSeq头为空,设置RTSPRequest的状态码为:RTSP/1.0 400 Bad Request\r\n */
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
			/* 假如CSeq头为空,设置RTSPRequest的状态码为:RTSP/1.0 454 Session Not Found\r\n */
            fRequest->SetValue(qtssRTSPReqStatusCode, 0, &statusCode, sizeof(statusCode));
            fRequest->SendHeader();
			return;
		}
		
		// refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
		if (fRequest->GetLateToleranceInSec() != -1)
        {
			/* 获取late-tolerance参数值来设置打薄参数 */
            fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());
            fRequest->SendHeader();
            return;
        }
		// let modules handle it if they want it.
        
	}

	//DESCRIBE 请求,必须保证没有SessionID,否则,向Client报错
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
    
    //如果未查找到，建立一个新的RTPSession
    // If we don't have an RTP session yet, create one...
    if (fRTPSession == NULL)
    {
		/* 创建一个RTPSession,其中theMap就是当前的RTPSessionMap(也就是HashTable) */
        theErr = this->CreateNewRTPSession(theMap);
		/* 假如创建失败,立即返回 */
        if (theErr != QTSS_NoErr)
            return;
    }

	/* 假如是Play Request,且Client发送了late-tolerance参数值,就设置Thinning Parameters */
	// If it's a play request and the late tolerance is sent in the request use this value
	if ((fRequest->GetMethod() == qtssPlayMethod) && (fRequest->GetLateToleranceInSec() != -1))
		fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());
	
	/* 假如是没有Range头(播放起始时间都为-1)的已在播放的Play Request,就返回“200 OK”响应 */
    //
    // Check to see if this is a "ping" PLAY request (a PLAY request while already
    // playing with no Range header). If so, just send back a 200 OK response and do nothing.
    // No need to go to modules to do this, because this is an RFC documented behavior  
    if ((fRequest->GetMethod() == qtssPlayMethod) && (fRTPSession->GetSessionState() == qtssPlayingState)
        && (fRequest->GetStartTime() == -1) && (fRequest->GetStopTime() == -1))
    {
        fRequest->SendHeader();
		/* 刷新该RTPSession的timeout时间,以防任务线程删除该会话 */
        fRTPSession->RefreshTimeout();
        return;
    }

     
	/* 现在务必要确保有一个RTP Session */
    Assert(fRTPSession != NULL); // At this point, we must have one!
    fRoleParams.rtspRequestParams.inClientSession = fRTPSession;
    
    // Setup Authorization params;
    fRequest->ParseAuthHeader();    
    
        
}

/* 删除当前RTSPSession引用,删除RTSP request数据及RTSPRequest对象,清空fRoleParams.rtspRequestParams */
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
    
	/* 删除RTSP request数据及RTSPRequest对象,清空fRoleParams.rtspRequestParams */
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
    
	/* 释放会话互斥锁 */
    fSessionMutex.Unlock();
    fReadMutex.Unlock();
    
    // Clear out our last value for request body length before moving onto the next request
    this->SetRequestBodyLength(-1);
}

/* 从RTSP Request中定位恰当的RTPSession,使用两种方法: (1)从RTSPRequest中获取相对应的RTSPSession的ID,若找到
就识别并找出对应的HashTable元,进而找到相应的RTPSession; (2)若没有找到,说明不是一个新的RTPSession,就用当前的
fLastRTPSessionID[]代替RTSPSession ID,找到相应的RTPSession */
QTSS_Error  RTSPSession::FindRTPSession(OSRefTable* inRefTable)
{
    // This function attempts to locate the appropriate RTP session for this RTSP
    // Request. It uses an RTSP session ID as a key to finding the correct RTP session,
    // and it looks for this session ID in two places. First, the RTSP session ID header
    // in the RTSP request, and if there isn't one there, in the RTSP session object itself.
    
	/* 从RTSPRequest中的"Session: 7736802604597532330\r\n",获取相对应的RTSPSession的ID */
    StrPtrLen* theSessionID = fRequest->GetHeaderDictionary()->GetValue(qtssSessionHeader); 

	/* 假如找到该RTPSession ID,就识别并找出对应的HashTable元,进而找到相应的RTPSession */
    if (theSessionID != NULL && theSessionID->Len > 0)
    {
        OSRef* theRef = inRefTable->Resolve(theSessionID);

       if (theRef != NULL)
            fRTPSession = (RTPSession*)theRef->GetObject();
    }
    
    // If there wasn't a session ID in the headers, look for one in the RTSP session itself
	/* 假如没有找到RTPSession ID,就用当前的fLastRTPSessionID[]代替(说明不是一个新的RTPSession) */
    if ( (theSessionID == NULL || theSessionID->Len == 0) && fLastRTPSessionIDPtr.Len > 0)
    {
        OSRef* theRef = inRefTable->Resolve(&fLastRTPSessionIDPtr);
        if (theRef != NULL)
            fRTPSession = (RTPSession*)theRef->GetObject();
    }
    
    return QTSS_NoErr;
}

/* 在恰当时候生成一个RTPSession,配置会话属性,分配一个随机的唯一的Session ID,注册并加入RTPSessionMap入参inRefTable中,并更新RTPSession总数 */
QTSS_Error  RTSPSession::CreateNewRTPSession(OSRefTable* inRefTable)
{
	/* 确保两者描述的是一回事:都是字符数组的起始位置,下面将存入RTPSession ID的值 */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

    // This is a brand spanking(崭新的) new session. At this point, we need to create
    // a new RTPSession object that will represent this session until it completes.
    // Then, we need to pass the session onto one of the modules

    // First of all, ask the server if it's ok to add a new session
	/* 服务器现在能加入一个新的RTPSession吗?检查服务器当前状态,最大连接数,带宽限制等 */
    QTSS_Error theErr = this->IsOkToAddNewRTPSession();
	/* 现在不能加入新的RTPSession,直接返回 */
    if (theErr != QTSS_NoErr)
        return theErr;

    // Create the RTPSession object
	/* 确保该RTSPSession没有相应的RTPSession */
    Assert(fRTPSession == NULL);

	/***************** NOTE!! ******************/
    fRTPSession = NEW RTPSession();
    
    {
        //
        // Lock the RTP session down so that it won't delete itself in the
        // unusual event there is a timeout while we are doing this.
		/* 锁定该RTPSession */
        OSMutexLocker locker(fRTPSession->GetSessionMutex());

        // Because this is a new RTP session, setup some dictionary attributes
        // pertaining to RTSP that only need to be set once
		/* 设置该RTPSession会话的有关RTSP相关的Dictionary属性(只需在此处设置一次) */
        this->SetupClientSessionAttrs();    
        
        // So, generate a unique session ID for this session
        QTSS_Error activationError = EPERM;

		/* 通过while()循环设置activationError的值只能是QTSS_NoErr,否则若是EPERM,就retry,直到找到唯一的key值 */
        while (activationError == EPERM)
        {
			/* 将随机生成RTPSession的ID存入fLastRTPSessionID */
            fLastRTPSessionIDPtr.Len = this->GenerateNewSessionID(fLastRTPSessionID);
            
            //ok, some module has bound this session, we can activate it.
            //At this point, we may find out that this new session ID is a duplicate.
            //If that's the case, we'll simply retry(重试) until we get a unique ID
			/* 用OSRef::Set()设置该RTPSession的Session ID,设置qtssCliSesRTSPSessionID和qtssSvrClientSessions的属性值, 在Hash表中注册并加入一个引用OSRef,
			及时更新RTPSession总数 */
			/* 通过while()循环设置activationError的值只能是QTSS_NoErr,否则若是EPERM,就retry,直到找到唯一的key值 */
            activationError = fRTPSession->Activate(fLastRTPSessionID);
        }
        Assert(activationError == QTSS_NoErr);
    }
	/* 再次确保两者是一回事!它存放了fLastRTPSessionID */
    Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

    // Activate adds this session into the RTP session map. We need to therefore
    // make sure to resolve the RTPSession object out of the map, even though
    // we don't actually need to pointer.
	/* 通过指定的键值字符串(随机的唯一的RTPSession ID)去识别和取得哈希表中的(哈希表元RTPSession)引用,尽管我们或许没必要指出该RTPSession 
	(这里是加最后一重保险)*/
	/* 注意入参在这里使用 */
    OSRef* theRef = inRefTable->Resolve(&fLastRTPSessionIDPtr);
    Assert(theRef != NULL);
    
    return QTSS_NoErr;
}

/* 参见RTPSessionInterface.h中相应数据成员 */
/* 解析RTSPRequest中的参数,并获取RTSPSession Dictionary中的属性值,来配置Client端Session的QTSS_ClientSessionAttributes属性参数. */
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

/* 通过拼合两部分随机数,得到SInt64 theSessionID,再将字符形式存为入参ioBuffer,且其字符串长度以函数值返回 */
UInt32 RTSPSession::GenerateNewSessionID(char* ioBuffer)
{
    //RANDOM NUMBER GENERATOR
    
    //We want to make our session IDs as random as possible, so use a bunch of
    //current server statistics to generate a random SInt64.

    //Generate the random number in two UInt32 parts. The first UInt32 uses
    //statistics out of a random RTP session.
	/* 将当前时间作为随机数种子 */
    SInt64 theMicroseconds = OS::Microseconds();
    ::srand((unsigned int)theMicroseconds);
    UInt32 theFirstRandom = ::rand();
    
	/* 获取服务器接口 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    {
		/* 锁定RTPSessionMap */
        OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
		/* 获得RTPSessionMap的Hash Table指针 */
        OSRefHashTable* theHashTable = theServer->GetRTPSessionMap()->GetHashTable();
        if (theHashTable->GetNumEntries() > 0)
        {
			/* 第一部分随机数是当前哈希表元个数的余数,再除以2 */
            theFirstRandom %= theHashTable->GetNumEntries();
            theFirstRandom >>= 2;
            
			/* 生成并初始化OSHashTable的迭代类的对象 */
            OSRefHashTableIter theIter(theHashTable);
            //Iterate through the session map, finding a random session
			/* 遍历整个RTPSessionMap,找到首个索引超过theFirstRandom的HashTable表元 */
            for (UInt32 theCount = 0; theCount < theFirstRandom; theIter.Next(), theCount++)
				/* 确保当前HashTable表元非空 */
                Assert(!theIter.IsDone());
            
			/* 获取当前HashTable表元所在的RTPSession对象 */
            RTPSession* theSession = (RTPSession*)theIter.GetCurrent()->GetObject();
			/* 结合当前RTPSession statistics使得第一部分随机数变得更随机 */
            theFirstRandom += theSession->GetPacketsSent();
            theFirstRandom += (UInt32)theSession->GetSessionCreateTime();
            theFirstRandom += (UInt32)theSession->GetPlayTime();
            theFirstRandom += (UInt32)theSession->GetBytesSent();
        }
    }

    //Generate the first half of the random number
	/* 使当前第一部分随机数为种子,再次产生随机数 */
    ::srand((unsigned int)theFirstRandom);
    theFirstRandom = ::rand();
    
    //Now generate the second half
	/* 先随机化第二部分随机数 */
    UInt32 theSecondRandom = ::rand();
    theSecondRandom += theServer->GetCurBandwidthInBits();
    theSecondRandom += theServer->GetAvgBandwidthInBits();
    theSecondRandom += theServer->GetRTPPacketsPerSec();
    theSecondRandom += (UInt32)theServer->GetTotalRTPBytes();
    theSecondRandom += theServer->GetTotalRTPSessions();
    
	/* 再以第二部分随机数为种子,生成第二部分随机数 */
    ::srand((unsigned int)theSecondRandom);
    theSecondRandom = ::rand();
    
	/* 通过拼合两部分随机数,得到SInt64 theSessionID,再将字符形式存为入参ioBuffer,且字符串长度以函数值返回 */
    SInt64 theSessionID = (SInt64)theFirstRandom;
    theSessionID <<= 32;
    theSessionID += (SInt64)theSecondRandom;
    qtss_sprintf(ioBuffer, "%"_64BITARG_"d", theSessionID);
    Assert(::strlen(ioBuffer) < QTSS_MAX_SESSION_ID_LENGTH);
    return ::strlen(ioBuffer);
}

/* 判断服务器是否超过最大连接数?超过返回true,否则返回false. 注意在RTSPSession::IsOkToAddNewRTPSession()中入参为0 */
Bool16 RTSPSession::OverMaxConnections(UInt32 buffer)
{
	/* 获取服务器接口 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
	/* 在streamingserver.xml中默认为1000 */
    SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();

	/* 连接数超过最大限制了吗? */
    Bool16 overLimit = false;
    
    if (maxConns > -1) // limit connections
    { 
		/* 确定最大连接数 */
        UInt32 maxConnections = (UInt32) maxConns + buffer;
		/* 判断当前的RTPSession个数或RTSPSession个数是否会超过1000? */
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

/* 从服务器的当前状态,最大连接数,最大带宽,来判断服务器能否加入新的RTPSession ?若不能加入就像Client发送错误响应 */
QTSS_Error RTSPSession::IsOkToAddNewRTPSession()
{
	/* 获取服务器接口,进而获取服务器状态 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    QTSS_ServerState theServerState = theServer->GetServerState();
    
    //we may want to deny this connection for a couple of different reasons
    //if the server is refusing new connections
	/* 假如服务器状态拒绝连接 */
    if ((theServerState == qtssRefusingConnectionsState) ||
        (theServerState == qtssIdleState) ||
        (theServerState == qtssFatalErrorState) ||
        (theServerState == qtssShuttingDownState))
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssServerUnavailable,
                                                    qtssMsgRefusingConnections);

    //if the max connection limit has been hit 
	/* 假如超过设定的最大连接数 */
    if  ( this->OverMaxConnections(0)) 
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
                                                    qtssMsgTooManyClients);

    //if the max bandwidth limit has been hit
	/* 假如超过最大带宽(Bits/sec) */
    SInt32 maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
    if ( (maxKBits > -1) && (theServer->GetCurBandwidthInBits() >= ((UInt32)maxKBits*1024)) )
        return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
                                                    qtssMsgTooMuchThruput);

    //if the server is too loaded down (CPU too high, whatever)
    // --INSERT WORKING CODE HERE--
    
    return QTSS_NoErr;                                                  
}

/* 从RTSP request中获取用户名\密码\URL Realm,设置RTSPSession和RTPSession的相应属性,若没有,使用服务器预设值 */
void RTSPSession::SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest)
{
    // Set the RTSP session's copy of the user name
	/* 从RTSP request中获取用户名 */
    StrPtrLen* tempPtr = theRTSPRequest->GetValue(qtssRTSPReqUserName);
    Assert(tempPtr != NULL);
	/* 用上面获取的用户名来设置RTSPSession和RTPSession的用户名 */
    if (tempPtr)
    {   (void)this->SetValue(qtssRTSPSesLastUserName, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
        (void)fRTPSession->SetValue(qtssCliRTSPSesUserName, (UInt32) 0, tempPtr->Ptr, tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
    }
    
    // Same thing... user password
	/* 从RTSP request中获取密码 */
    tempPtr = theRTSPRequest->GetValue(qtssRTSPReqUserPassword);
    Assert(tempPtr != NULL);
	/* 用上面获取的密码来设置RTSPSession和RTPSession的密码 */
    if (tempPtr)
    {   (void)this->SetValue(qtssRTSPSesLastUserPassword, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
        (void)fRTPSession->SetValue(qtssCliRTSPSesUserPassword, (UInt32) 0, tempPtr->Ptr, tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
    }
    
	/* 从RTSP request中获取URL Realm */
    tempPtr = theRTSPRequest->GetValue(qtssRTSPReqURLRealm);
    if (tempPtr)
    {
        if (tempPtr->Len == 0)
        {
            // If there is no realm explicitly specified in the request, then let's get the default out of the prefs
            OSCharArrayDeleter theDefaultRealm(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
			/* 获取服务器Realm的预设值 */
            char *realm = theDefaultRealm.GetObject();
            UInt32 len = ::strlen(theDefaultRealm.GetObject());
			/* 使用服务器Realm的预设值来设置RTSPSession和RTPSession的相应字段 */
            (void)this->SetValue(qtssRTSPSesLastURLRealm, 0, realm, len,QTSSDictionary::kDontObeyReadOnly);
            (void)fRTPSession->SetValue(qtssCliRTSPSesURLRealm, (UInt32) 0,realm,len, QTSSDictionary::kDontObeyReadOnly );
        }
        else
        {
			/* 使用从RTSP request中获取的URL Realm来设置RTSPSession和RTPSession的相应字段 */
            (void)this->SetValue(qtssRTSPSesLastURLRealm, 0, tempPtr->Ptr, tempPtr->Len,QTSSDictionary::kDontObeyReadOnly);
            (void)fRTPSession->SetValue(qtssCliRTSPSesURLRealm, (UInt32) 0,tempPtr->Ptr,tempPtr->Len, QTSSDictionary::kDontObeyReadOnly );
        }
    }
}

/* 利用RTSPRequestInterface::Read(),取出RTSP request的内容,存入临时缓存 */
QTSS_Error RTSPSession::DumpRequestData()
{
    char theDumpBuffer[2048];
    
    QTSS_Error theErr = QTSS_NoErr;
    while (theErr == QTSS_NoErr)
        theErr = this->Read(theDumpBuffer, 2048, NULL);
        
    return theErr;
}

/* 对比联系紧密的RTSPSessionInterface::InterleavedWrite() */
void RTSPSession::HandleIncomingDataPacket()
{
    
    // Attempt to find the RTP session for this request.
	/* 由RTSP Request中获取RTCP包所在的通道号 */
    UInt8   packetChannel = (UInt8)fInputStream.GetRequestBuffer()->Ptr[1];
	/* 由通道号得到对应的RTSPSession ID */
    StrPtrLen* theSessionID = this->GetSessionIDForChannelNum(packetChannel);
    
    if (theSessionID == NULL)
    {
        Assert(0);
        theSessionID = &fLastRTPSessionIDPtr;

    }
    
	/* 从RTPSession Map找到指定会话ID的ref,从而得到其所在的RTPSession */
    OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
    OSRef* theRef = theMap->Resolve(theSessionID);
    
    if (theRef != NULL)
        fRTPSession = (RTPSession*)theRef->GetObject();

	/* 若没有RTPSession,直接返回 */
    if (fRTPSession == NULL)
        return;

	/* 得到RTCP包的真实数据,跳过RTPInterleaveHeader的4个字节 */
    StrPtrLen packetWithoutHeaders(fInputStream.GetRequestBuffer()->Ptr + 4, fInputStream.GetRequestBuffer()->Len - 4);
    
	/* 获取RTPSession互斥锁,并刷新超时 */
    OSMutexLocker locker(fRTPSession->GetMutex());
	/* 刷新该RTPSession,以防服务器删去 */
    fRTPSession->RefreshTimeout();
	/* 由包号查找指定的该RTPSession中的RTPStream */
    RTPStream* theStream = fRTPSession->FindRTPStreamForChannelNum(packetChannel);
    theStream->ProcessIncomingInterleavedData(packetChannel, this, &packetWithoutHeaders);

    // We currently don't support async notifications from within this role
	/* 设置packetParams.rtspIncomingDataParams */
    QTSS_RoleParams packetParams;
    packetParams.rtspIncomingDataParams.inRTSPSession = this;
    packetParams.rtspIncomingDataParams.inClientSession = fRTPSession;//就是RTPSession
    packetParams.rtspIncomingDataParams.inPacketData = fInputStream.GetRequestBuffer()->Ptr;
    packetParams.rtspIncomingDataParams.inPacketLen = fInputStream.GetRequestBuffer()->Len;
    
	/* 查找注册QTSSModule::kRTSPIncomingDataRole的模块,并分发它们 */
    UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRTSPIncomingDataRole);
    for (; fCurrentModule < numModules; fCurrentModule++)
    {
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRTSPIncomingDataRole, fCurrentModule);
        (void)theModule->CallDispatch(QTSS_RTSPIncomingData_Role, &packetParams);
    }
    fCurrentModule = 0;
}
