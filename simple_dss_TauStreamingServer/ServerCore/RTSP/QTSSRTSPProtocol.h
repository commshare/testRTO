
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSRTSPProtocol.h
Description: Constant & Enum definitions for RTSP protocol type parts of QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef QTSS_RTSPPROTOCOL_H
#define QTSS_RTSPPROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "OSHeaders.h" 

enum
{
    qtssDescribeMethod      = 0, 
    qtssSetupMethod         = 1,
    qtssTeardownMethod      = 2,
    qtssNumVIPMethods       = 3,

    qtssPlayMethod          = 3,
    qtssPauseMethod         = 4,	
    qtssOptionsMethod       = 5,/* 服务器默认的方法, used in QTSServer::DoInitRole() */
    qtssAnnounceMethod      = 6,
    qtssGetParameterMethod  = 7,
    qtssSetParameterMethod  = 8,
    qtssRedirectMethod      = 9,
    qtssRecordMethod        = 10,
    
	/* used in QTSServer::SetupPublicHeader(),QTSSModuleUtils::SetupSupportedMethods() */
    qtssNumMethods          = 11,
    qtssIllegalMethod       = 11
    
};
typedef UInt32 QTSS_RTSPMethod;


/* used in RTSPProtocol::GetRequestHeader(),分三类:Extension/VIP/others Headers */
enum
{
    //These are the common request headers(VIP) (optimized)
    qtssAcceptHeader            = 0,
    qtssCSeqHeader              = 1,
    qtssUserAgentHeader         = 2,/* used in RTSPRequest::ParseClientPortSubHeader()/RTSPSession::SetupClientSessionAttrs() */
    qtssTransportHeader         = 3,
    qtssSessionHeader           = 4,
    qtssRangeHeader             = 5,
    qtssNumVIPHeaders           = 6,/* 以上6个为VIP Headers */
    
    //Other request headers
    qtssAcceptEncodingHeader    = 6,
    qtssAcceptLanguageHeader    = 7,
    qtssAuthorizationHeader     = 8,        
    qtssBandwidthHeader         = 9,
    qtssBlockSizeHeader         = 10,

    qtssCacheControlHeader      = 11,
    qtssConferenceHeader        = 12,       
    qtssConnectionHeader        = 13,
    qtssContentBaseHeader       = 14,
    qtssContentEncodingHeader   = 15,
    qtssContentLanguageHeader   = 16,
    qtssContentLengthHeader     = 17,/* used in QTSSModuleUtils::SendErrorResponse() */
    qtssContentLocationHeader   = 18,
    qtssContentTypeHeader       = 19,/* used in RTSPSession::SetupRequest() */
    qtssDateHeader              = 20,

    qtssExpiresHeader           = 21,
    qtssFromHeader              = 22,
    qtssHostHeader              = 23,/* used in RTSPRequest::ParseURI() */
    qtssIfMatchHeader           = 24,
    qtssIfModifiedSinceHeader   = 25,/* used in RTSPRequest::ParseIfModSinceHeader() */
    qtssLastModifiedHeader      = 26,/* used in QTSSFileModule.cpp:DoDescribe() */
    qtssLocationHeader          = 27,
    qtssProxyAuthenticateHeader = 28,
    qtssProxyRequireHeader      = 29,
    qtssRefererHeader           = 30,

    qtssRetryAfterHeader        = 31,
    qtssRequireHeader           = 32,/* used in RTSPSession::SetupRequest() */
    qtssRTPInfoHeader           = 33,
    qtssScaleHeader             = 34,
    qtssSpeedHeader             = 35,
    qtssTimestampHeader         = 36,
    qtssVaryHeader              = 37,
    qtssViaHeader               = 38,
    qtssNumRequestHeaders       = 39,
    
    //Additional response headers
    qtssAllowHeader             = 39,
    qtssPublicHeader            = 40, /* used in RTSPSession::SetupRequest() */
    qtssServerHeader            = 41,
    qtssUnsupportedHeader       = 42,
    qtssWWWAuthenticateHeader   = 43,
    qtssSameAsLastHeader        = 44, /* used in RTSPRequestInterface::AppendRTPInfoHeader() */
    
    //Newly added headers
    qtssExtensionHeaders        = 45, /* 以下为Extension Headers */
    
    qtssXRetransmitHeader       = 45, /* used in RTPStream::SendSetupResponse() */
    qtssXAcceptRetransmitHeader = 46,
    qtssXRTPMetaInfoHeader      = 47,
    qtssXTransportOptionsHeader = 48,
    qtssXPacketRangeHeader      = 49,/* used in QTSSFileModule.cpp:DoPlay() */
    qtssXPreBufferHeader        = 50,
	qtssXDynamicRateHeader      = 51,/* used in RTSPSessionInterface::RevertOutputStream()/RTPStream::SendSetupResponse() */
	qtssXAcceptDynamicRateHeader= 52,
	// DJM PROTOTYPE
	qtssXRandomDataSizeHeader   = 53,/* used in RTSPSession::SetupRequest() */
	
	qtssNumHeaders				= 54,
	qtssIllegalHeader 			= 54
    
};
typedef UInt32 QTSS_RTSPHeader;

/* used in RTSPSession::SetupRequest()/RTSPProtocol::sStatusCodeStrings[] */
enum
{
    qtssContinue                        = 0,        //100
    qtssSuccessOK                       = 1,        //200
    qtssSuccessCreated                  = 2,        //201
    qtssSuccessAccepted                 = 3,        //202
    qtssSuccessNoContent                = 4,        //203
    qtssSuccessPartialContent           = 5,        //204
    qtssSuccessLowOnStorage             = 6,        //250
    qtssMultipleChoices                 = 7,        //300
    qtssRedirectPermMoved               = 8,        //301
    qtssRedirectTempMoved               = 9,        //302
    qtssRedirectSeeOther                = 10,       //303
    qtssRedirectNotModified             = 11,       //304
    qtssUseProxy                        = 12,       //305
    qtssClientBadRequest                = 13,       //400
    qtssClientUnAuthorized              = 14,       //401
    qtssPaymentRequired                 = 15,       //402
    qtssClientForbidden                 = 16,       //403
    qtssClientNotFound                  = 17,       //404
    qtssClientMethodNotAllowed          = 18,       //405
    qtssNotAcceptable                   = 19,       //406
    qtssProxyAuthenticationRequired     = 20,       //407
    qtssRequestTimeout                  = 21,       //408
    qtssClientConflict                  = 22,       //409
    qtssGone                            = 23,       //410
    qtssLengthRequired                  = 24,       //411
    qtssPreconditionFailed              = 25,       //412
    qtssRequestEntityTooLarge           = 26,       //413
    qtssRequestURITooLarge              = 27,       //414
    qtssUnsupportedMediaType            = 28,       //415
    qtssClientParameterNotUnderstood    = 29,       //451
    qtssClientConferenceNotFound        = 30,       //452
    qtssClientNotEnoughBandwidth        = 31,       //453
    qtssClientSessionNotFound           = 32,       //454
    qtssClientMethodNotValidInState     = 33,       //455
    qtssClientHeaderFieldNotValid       = 34,       //456
    qtssClientInvalidRange              = 35,       //457
    qtssClientReadOnlyParameter         = 36,       //458
    qtssClientAggregateOptionNotAllowed = 37,       //459
    qtssClientAggregateOptionAllowed    = 38,       //460/* RTSPRequest::ParseURI() */
    qtssClientUnsupportedTransport      = 39,       //461
    qtssClientDestinationUnreachable    = 40,       //462
    qtssServerInternal                  = 41,       //500
    qtssServerNotImplemented            = 42,       //501
    qtssServerBadGateway                = 43,       //502
    qtssServerUnavailable               = 44,       //503
    qtssServerGatewayTimeout            = 45,       //505
    qtssRTSPVersionNotSupported         = 46,       //504
    qtssServerOptionNotSupported        = 47,       //551
    qtssNumStatusCodes                  = 48
    
};
typedef UInt32 QTSS_RTSPStatusCode;

#ifdef __cplusplus
}
#endif

#endif //QTSS_RTSPPROTOCOL_H
