
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPProtocol.cpp
Description: A grouping of static utilities that abstract keyword strings
			 in the RTSP protocol. This should be maintained as new versions
			 of the RTSP protoocl appear & as the server evolves to take
			 advantage of new RTSP features.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/

#include <ctype.h>
#include "RTSPProtocol.h"

/* RetransmitProtocolName */
/* 重传协议名 */
StrPtrLen RTSPProtocol::sRetrProtName("our-retransmit");

/* 参见QTSS_RTSPMethod in QTSSRTSPProtocol.h */
/* 参加RFC2326，下面是该协议定义的RTSP方法 */
StrPtrLen RTSPProtocol::sMethods[] = //11
{
    StrPtrLen("DESCRIBE"),
    StrPtrLen("SETUP"),
    StrPtrLen("TEARDOWN"),
    StrPtrLen("PLAY"),
    StrPtrLen("PAUSE"),
	/* 下面的几个不常用到，但在流的推送时要用到 */
    StrPtrLen("OPTIONS"), /* 任何时候，由client发出，用于查询server支持的RTSP methods */
    StrPtrLen("ANNOUNCE"),/* 相当于setup */
    StrPtrLen("GET_PARAMETER"),
    StrPtrLen("SET_PARAMETER"),
    StrPtrLen("REDIRECT"),
    StrPtrLen("RECORD") /* 相当于play */
};


/* used in RTSPRequest::ParseFirstLine() */
/* 不计大小写,先查找VIP methods,再查找其它的methods,返回入参字符串中的RTSP method,若没有合格的RTSP method,就返回qtssIllegalMethod */
QTSS_RTSPMethod
RTSPProtocol::GetMethod(const StrPtrLen &inMethodStr)
{
    //chances are this is one of our selected "VIP" methods. so check for this.
    QTSS_RTSPMethod theMethod = qtssIllegalMethod;
    
	/* 检查第一个字符,看是否是VIP methods? */
    switch(*inMethodStr.Ptr)
    {
        case 'S':   case 's':   theMethod = qtssSetupMethod;    break;
        case 'D':   case 'd':   theMethod = qtssDescribeMethod; break;
        case 'T':   case 't':   theMethod = qtssTeardownMethod; break;
        case 'O':   case 'o':   theMethod = qtssOptionsMethod;  break;
        case 'A':   case 'a':   theMethod = qtssAnnounceMethod; break;
    }

	/* 假如入参和默认的static Method Str相等(不计大小写),就返回该方法 */
    if ((theMethod != qtssIllegalMethod) &&
        (inMethodStr.EqualIgnoreCase(sMethods[theMethod].Ptr, sMethods[theMethod].Len)))
        return theMethod;

	/* 假如入参和默认的static Method Str相等(不计大小写),返回非VIP methods */
    for (SInt32 x = qtssNumVIPMethods; x < qtssIllegalMethod; x++)
        if (inMethodStr.EqualIgnoreCase(sMethods[x].Ptr, sMethods[x].Len))
            return x;
    return qtssIllegalMethod;
}

/* 下面是RTSP协议中定义的RTSP Request和Response中用到的语法，在RTSPRequestStream.cpp,RTSPResponseStream.cpp和RTPStream中频繁用到 */

StrPtrLen RTSPProtocol::sHeaders[] =
{
    StrPtrLen("Accept"),//0
    StrPtrLen("Cseq"),
    StrPtrLen("User-Agent"),
    StrPtrLen("Transport"),
    StrPtrLen("Session"),
    StrPtrLen("Range"),/* 以上6个为VIP Headers */

    StrPtrLen("Accept-Encoding"),
    StrPtrLen("Accept-Language"),
    StrPtrLen("Authorization"),
    StrPtrLen("Bandwidth"),//9

    StrPtrLen("Blocksize"),
    StrPtrLen("Cache-Control"),
    StrPtrLen("Conference"),
    StrPtrLen("Connection"),
    StrPtrLen("Content-Base"),
    StrPtrLen("Content-Encoding"),
    StrPtrLen("Content-Language"),
    StrPtrLen("Content-length"),
    StrPtrLen("Content-Location"),
    StrPtrLen("Content-Type"),//19

    StrPtrLen("Date"),
    StrPtrLen("Expires"),
    StrPtrLen("From"),
    StrPtrLen("Host"),
    StrPtrLen("If-Match"),
    StrPtrLen("If-Modified-Since"),
    StrPtrLen("Last-Modified"),
    StrPtrLen("Location"),
    StrPtrLen("Proxy-Authenticate"),
    StrPtrLen("Proxy-Require"),//29

    StrPtrLen("Referer"),
    StrPtrLen("Retry-After"),
    StrPtrLen("Require"),//32
    StrPtrLen("RTP-Info"),//33
    StrPtrLen("Scale"),
    StrPtrLen("Speed"),
    StrPtrLen("Timestamp"),
    StrPtrLen("Vary"),
    StrPtrLen("Via"),
    StrPtrLen("Allow"),//39

    StrPtrLen("Public"),
    StrPtrLen("Server"),
    StrPtrLen("Unsupported"),
    StrPtrLen("WWW-Authenticate"),
    StrPtrLen(","),//44

	/* 以下为Extension Headers String */
    StrPtrLen("x-Retransmit"),//45
    StrPtrLen("x-Accept-Retransmit"),
    StrPtrLen("x-RTP-Meta-Info"),
    StrPtrLen("x-Transport-Options"),
    StrPtrLen("x-Packet-Range"),//49

    StrPtrLen("x-Prebuffer"),
	/* used in RTSPSessionInterface::RevertOutputStream(),若从0开始就是51 */
	StrPtrLen("x-Dynamic-Rate"),//51
	StrPtrLen("x-Accept-Dynamic-Rate"),
	// DJM PROTOTYPE
	StrPtrLen("x-Random-Data-Size")//53
};

/* 对入参分三种情形分析:Extension/VIP/others Headers,来获取RTSP Request Headers(实质是Index) */
QTSS_RTSPHeader RTSPProtocol::GetRequestHeader(const StrPtrLen &inHeaderStr)
{
    if (inHeaderStr.Len == 0)
        return qtssIllegalHeader;
    
	/* temp variable,注意这个参数充当Index,只用于VIP/Extension header,非常重要!!! */
    QTSS_RTSPHeader theHeader = qtssIllegalHeader;
    
    //chances are this is one of our selected "VIP" headers. so check for this.
	/* 检查入参的首个字符值来设定theHeader的值 */
    switch(*inHeaderStr.Ptr)
    {
        case 'C':   case 'c':   theHeader = qtssCSeqHeader;         break;
        case 'S':   case 's':   theHeader = qtssSessionHeader;      break;
        case 'U':   case 'u':   theHeader = qtssUserAgentHeader;    break;
        case 'A':   case 'a':   theHeader = qtssAcceptHeader;       break;
        case 'T':   case 't':   theHeader = qtssTransportHeader;    break;
        case 'R':   case 'r':   theHeader = qtssRangeHeader;        break; /* 以上6个为VIP Headers */
        case 'X':   case 'x':   theHeader = qtssExtensionHeaders;   break; /* Extension Headers,下面会进一步讨论 */
    }
    
    //
    // Check to see whether this is one of our extension headers. These
    // are very likely to appear in requests.
	/* 假如入参和默认的static sHeaders[theHeader]相等(不计大小写),返回Extension Headers */
    if (theHeader == qtssExtensionHeaders)//45
    {
		/* 从ExtensionHeaders开始到末尾循环,查找并返回入参字符串相同(忽略大小写)的对应RTSPProtocol::sHeaders[]中的相应分量的索引 */
        for (SInt32 y = qtssExtensionHeaders; y < qtssNumHeaders; y++)
        {
            if (inHeaderStr.EqualIgnoreCase(sHeaders[y].Ptr, sHeaders[y].Len))
                return y;
        }
    }
    
    //
    // It's not one of our extension headers, check to see if this is one of
    // our normal VIP headers
	/* 检查看入参是否是通常的VIP Header,若是就返回对应Index */
    if ((theHeader != qtssIllegalHeader) &&/* 注意此处说明theHeader已被重新赋值,它只能是VIP header */
        (inHeaderStr.EqualIgnoreCase(sHeaders[theHeader].Ptr, sHeaders[theHeader].Len)))
        return theHeader;

    //
    //If this isn't one of our VIP headers, go through the remaining request headers, trying
    //to find the right one.
	/* 检查是否是其它的Header(extension/VIP Headers除外)?若是就返回对应Index */
    for (SInt32 x = qtssNumVIPHeaders; x < qtssNumHeaders; x++)
    {
        if (inHeaderStr.EqualIgnoreCase(sHeaders[x].Ptr, sHeaders[x].Len))
            return x;
    }
    return qtssIllegalHeader;
}


/* 下面是RTSP交互时的状态码字符串 */
StrPtrLen RTSPProtocol::sStatusCodeStrings[] =//48
{
    StrPtrLen("Continue"),                              //kContinue /* 0 */
    StrPtrLen("OK"),                                    //kSuccessOK
    StrPtrLen("Created"),                               //kSuccessCreated
    StrPtrLen("Accepted"),                              //kSuccessAccepted
    StrPtrLen("No Content"),                            //kSuccessNoContent
    StrPtrLen("Partial Content"),                       //kSuccessPartialContent
    StrPtrLen("Low on Storage Space"),                  //kSuccessLowOnStorage
    StrPtrLen("Multiple Choices"),                      //kMultipleChoices
    StrPtrLen("Moved Permanently"),                     //kRedirectPermMoved
    StrPtrLen("Found"),                                 //kRedirectTempMoved /*9 */
    StrPtrLen("See Other"),                             //kRedirectSeeOther
    StrPtrLen("Not Modified"),                          //kRedirectNotModified/* 11 */
    StrPtrLen("Use Proxy"),                             //kUseProxy /* 12 */
    StrPtrLen("Bad Request"),                           //kClientBadRequest /* 13 */
    StrPtrLen("Unauthorized"),                          //kClientUnAuthorized
    StrPtrLen("Payment Required"),                      //kPaymentRequired
    StrPtrLen("Forbidden"),                             //kClientForbidden
    StrPtrLen("Not Found"),                             //kClientNotFound
    StrPtrLen("Method Not Allowed"),                    //kClientMethodNotAllowed
    StrPtrLen("Not Acceptable"),                        //kNotAcceptable
    StrPtrLen("Proxy Authentication Required"),         //kProxyAuthenticationRequired
    StrPtrLen("Request Time-out"),                      //kRequestTimeout
    StrPtrLen("Conflict"),                              //kClientConflict
    StrPtrLen("Gone"),                                  //kGone
    StrPtrLen("Length Required"),                       //kLengthRequired
    StrPtrLen("Precondition Failed"),                   //kPreconditionFailed
    StrPtrLen("Request Entity Too Large"),              //kRequestEntityTooLarge
    StrPtrLen("Request-URI Too Large"),                 //kRequestURITooLarge
    StrPtrLen("Unsupported Media Type"),                //kUnsupportedMediaType
    StrPtrLen("Parameter Not Understood"),              //kClientParameterNotUnderstood
    StrPtrLen("Conference Not Found"),                  //kClientConferenceNotFound
    StrPtrLen("Not Enough Bandwidth"),                  //kClientNotEnoughBandwidth
    StrPtrLen("Session Not Found"),                     //kClientSessionNotFound /* 32 */
    StrPtrLen("Method Not Valid in this State"),        //kClientMethodNotValidInState
    StrPtrLen("Header Field Not Valid For Resource"),   //kClientHeaderFieldNotValid
    StrPtrLen("Invalid Range"),                         //kClientInvalidRange
    StrPtrLen("Parameter Is Read-Only"),                //kClientReadOnlyParameter
    StrPtrLen("Aggregate Option Not Allowed"),          //kClientAggregateOptionNotAllowed
    StrPtrLen("Only Aggregate Option Allowed"),         //kClientAggregateOptionAllowed
    StrPtrLen("Unsupported Transport"),                 //kClientUnsupportedTransport
    StrPtrLen("Destination Unreachable"),               //kClientDestinationUnreachable
    StrPtrLen("Internal Server Error"),                 //kServerInternal
    StrPtrLen("Not Implemented"),                       //kServerNotImplemented
    StrPtrLen("Bad Gateway"),                           //kServerBadGateway
    StrPtrLen("Service Unavailable"),                   //kServerUnavailable
    StrPtrLen("Gateway Timeout"),                       //kServerGatewayTimeout
    StrPtrLen("RTSP Version not supported"),            //kRTSPVersionNotSupported
    StrPtrLen("Option Not Supported")                   //kServerOptionNotSupported
};

/* 下面是RTSP交互时的状态码编号 */
SInt32 RTSPProtocol::sStatusCodes[] =//48
{
    100,        //kContinue
    200,        //kSuccessOK
    201,        //kSuccessCreated
    202,        //kSuccessAccepted
    204,        //kSuccessNoContent
    206,        //kSuccessPartialContent
    250,        //kSuccessLowOnStorage
    300,        //kMultipleChoices
    301,        //kRedirectPermMoved
    302,        //kRedirectTempMoved
    303,        //kRedirectSeeOther
    304,        //kRedirectNotModified
    305,        //kUseProxy
    400,        //kClientBadRequest
    401,        //kClientUnAuthorized
    402,        //kPaymentRequired
    403,        //kClientForbidden
    404,        //kClientNotFound
    405,        //kClientMethodNotAllowed
    406,        //kNotAcceptable
    407,        //kProxyAuthenticationRequired
    408,        //kRequestTimeout
    409,        //kClientConflict
    410,        //kGone
    411,        //kLengthRequired
    412,        //kPreconditionFailed
    413,        //kRequestEntityTooLarge
    414,        //kRequestURITooLarge
    415,        //kUnsupportedMediaType
    451,        //kClientParameterNotUnderstood
    452,        //kClientConferenceNotFound
    453,        //kClientNotEnoughBandwidth
    454,        //kClientSessionNotFound
    455,        //kClientMethodNotValidInState
    456,        //kClientHeaderFieldNotValid
    457,        //kClientInvalidRange
    458,        //kClientReadOnlyParameter
    459,        //kClientAggregateOptionNotAllowed
    460,        //kClientAggregateOptionAllowed
    461,        //kClientUnsupportedTransport
    462,        //kClientDestinationUnreachable
    500,        //kServerInternal
    501,        //kServerNotImplemented
    502,        //kServerBadGateway
    503,        //kServerUnavailable
    504,        //kServerGatewayTimeout
    505,        //kRTSPVersionNotSupported
    551         //kServerOptionNotSupported
};

StrPtrLen RTSPProtocol::sStatusCodeAsStrings[] =
{
    StrPtrLen("100"),       //kContinue
    StrPtrLen("200"),       //kSuccessOK
    StrPtrLen("201"),       //kSuccessCreated
    StrPtrLen("202"),       //kSuccessAccepted
    StrPtrLen("204"),       //kSuccessNoContent
    StrPtrLen("206"),       //kSuccessPartialContent
    StrPtrLen("250"),       //kSuccessLowOnStorage
    StrPtrLen("300"),       //kMultipleChoices
    StrPtrLen("301"),       //kRedirectPermMoved
    StrPtrLen("302"),       //kRedirectTempMoved
    StrPtrLen("303"),       //kRedirectSeeOther
    StrPtrLen("304"),       //kRedirectNotModified
    StrPtrLen("305"),       //kUseProxy
    StrPtrLen("400"),       //kClientBadRequest
    StrPtrLen("401"),       //kClientUnAuthorized
    StrPtrLen("402"),       //kPaymentRequired
    StrPtrLen("403"),       //kClientForbidden
    StrPtrLen("404"),       //kClientNotFound
    StrPtrLen("405"),       //kClientMethodNotAllowed
    StrPtrLen("406"),       //kNotAcceptable
    StrPtrLen("407"),       //kProxyAuthenticationRequired
    StrPtrLen("408"),       //kRequestTimeout
    StrPtrLen("409"),       //kClientConflict
    StrPtrLen("410"),       //kGone
    StrPtrLen("411"),       //kLengthRequired
    StrPtrLen("412"),       //kPreconditionFailed
    StrPtrLen("413"),       //kRequestEntityTooLarge
    StrPtrLen("414"),       //kRequestURITooLarge
    StrPtrLen("415"),       //kUnsupportedMediaType
    StrPtrLen("451"),       //kClientParameterNotUnderstood
    StrPtrLen("452"),       //kClientConferenceNotFound
    StrPtrLen("453"),       //kClientNotEnoughBandwidth
    StrPtrLen("454"),       //kClientSessionNotFound
    StrPtrLen("455"),       //kClientMethodNotValidInState
    StrPtrLen("456"),       //kClientHeaderFieldNotValid
    StrPtrLen("457"),       //kClientInvalidRange
    StrPtrLen("458"),       //kClientReadOnlyParameter
    StrPtrLen("459"),       //kClientAggregateOptionNotAllowed
    StrPtrLen("460"),       //kClientAggregateOptionAllowed
    StrPtrLen("461"),       //kClientUnsupportedTransport
    StrPtrLen("462"),       //kClientDestinationUnreachable
    StrPtrLen("500"),       //kServerInternal
    StrPtrLen("501"),       //kServerNotImplemented
    StrPtrLen("502"),       //kServerBadGateway
    StrPtrLen("503"),       //kServerUnavailable
    StrPtrLen("504"),       //kServerGatewayTimeout
    StrPtrLen("505"),       //kRTSPVersionNotSupported
    StrPtrLen("551")        //kServerOptionNotSupported
};

/* 下面是RTSP 版本字符串 */
StrPtrLen RTSPProtocol::sVersionString[] = 
{
    StrPtrLen("RTSP/1.0")
};

/* 判断入参versionStr是否合法?形如"RTSP/1.0" */
RTSPProtocol::RTSPVersion
RTSPProtocol::GetVersion(StrPtrLen &versionStr)
{
    if (versionStr.Len != 8)
        return kIllegalVersion;
    else
        return k10Version;
}
