
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
/* �ش�Э���� */
StrPtrLen RTSPProtocol::sRetrProtName("our-retransmit");

/* �μ�QTSS_RTSPMethod in QTSSRTSPProtocol.h */
/* �μ�RFC2326�������Ǹ�Э�鶨���RTSP���� */
StrPtrLen RTSPProtocol::sMethods[] = //11
{
    StrPtrLen("DESCRIBE"),
    StrPtrLen("SETUP"),
    StrPtrLen("TEARDOWN"),
    StrPtrLen("PLAY"),
    StrPtrLen("PAUSE"),
	/* ����ļ��������õ���������������ʱҪ�õ� */
    StrPtrLen("OPTIONS"), /* �κ�ʱ����client���������ڲ�ѯserver֧�ֵ�RTSP methods */
    StrPtrLen("ANNOUNCE"),/* �൱��setup */
    StrPtrLen("GET_PARAMETER"),
    StrPtrLen("SET_PARAMETER"),
    StrPtrLen("REDIRECT"),
    StrPtrLen("RECORD") /* �൱��play */
};


/* used in RTSPRequest::ParseFirstLine() */
/* ���ƴ�Сд,�Ȳ���VIP methods,�ٲ���������methods,��������ַ����е�RTSP method,��û�кϸ��RTSP method,�ͷ���qtssIllegalMethod */
QTSS_RTSPMethod
RTSPProtocol::GetMethod(const StrPtrLen &inMethodStr)
{
    //chances are this is one of our selected "VIP" methods. so check for this.
    QTSS_RTSPMethod theMethod = qtssIllegalMethod;
    
	/* ����һ���ַ�,���Ƿ���VIP methods? */
    switch(*inMethodStr.Ptr)
    {
        case 'S':   case 's':   theMethod = qtssSetupMethod;    break;
        case 'D':   case 'd':   theMethod = qtssDescribeMethod; break;
        case 'T':   case 't':   theMethod = qtssTeardownMethod; break;
        case 'O':   case 'o':   theMethod = qtssOptionsMethod;  break;
        case 'A':   case 'a':   theMethod = qtssAnnounceMethod; break;
    }

	/* ������κ�Ĭ�ϵ�static Method Str���(���ƴ�Сд),�ͷ��ظ÷��� */
    if ((theMethod != qtssIllegalMethod) &&
        (inMethodStr.EqualIgnoreCase(sMethods[theMethod].Ptr, sMethods[theMethod].Len)))
        return theMethod;

	/* ������κ�Ĭ�ϵ�static Method Str���(���ƴ�Сд),���ط�VIP methods */
    for (SInt32 x = qtssNumVIPMethods; x < qtssIllegalMethod; x++)
        if (inMethodStr.EqualIgnoreCase(sMethods[x].Ptr, sMethods[x].Len))
            return x;
    return qtssIllegalMethod;
}

/* ������RTSPЭ���ж����RTSP Request��Response���õ����﷨����RTSPRequestStream.cpp,RTSPResponseStream.cpp��RTPStream��Ƶ���õ� */

StrPtrLen RTSPProtocol::sHeaders[] =
{
    StrPtrLen("Accept"),//0
    StrPtrLen("Cseq"),
    StrPtrLen("User-Agent"),
    StrPtrLen("Transport"),
    StrPtrLen("Session"),
    StrPtrLen("Range"),/* ����6��ΪVIP Headers */

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

	/* ����ΪExtension Headers String */
    StrPtrLen("x-Retransmit"),//45
    StrPtrLen("x-Accept-Retransmit"),
    StrPtrLen("x-RTP-Meta-Info"),
    StrPtrLen("x-Transport-Options"),
    StrPtrLen("x-Packet-Range"),//49

    StrPtrLen("x-Prebuffer"),
	/* used in RTSPSessionInterface::RevertOutputStream(),����0��ʼ����51 */
	StrPtrLen("x-Dynamic-Rate"),//51
	StrPtrLen("x-Accept-Dynamic-Rate"),
	// DJM PROTOTYPE
	StrPtrLen("x-Random-Data-Size")//53
};

/* ����η��������η���:Extension/VIP/others Headers,����ȡRTSP Request Headers(ʵ����Index) */
QTSS_RTSPHeader RTSPProtocol::GetRequestHeader(const StrPtrLen &inHeaderStr)
{
    if (inHeaderStr.Len == 0)
        return qtssIllegalHeader;
    
	/* temp variable,ע����������䵱Index,ֻ����VIP/Extension header,�ǳ���Ҫ!!! */
    QTSS_RTSPHeader theHeader = qtssIllegalHeader;
    
    //chances are this is one of our selected "VIP" headers. so check for this.
	/* �����ε��׸��ַ�ֵ���趨theHeader��ֵ */
    switch(*inHeaderStr.Ptr)
    {
        case 'C':   case 'c':   theHeader = qtssCSeqHeader;         break;
        case 'S':   case 's':   theHeader = qtssSessionHeader;      break;
        case 'U':   case 'u':   theHeader = qtssUserAgentHeader;    break;
        case 'A':   case 'a':   theHeader = qtssAcceptHeader;       break;
        case 'T':   case 't':   theHeader = qtssTransportHeader;    break;
        case 'R':   case 'r':   theHeader = qtssRangeHeader;        break; /* ����6��ΪVIP Headers */
        case 'X':   case 'x':   theHeader = qtssExtensionHeaders;   break; /* Extension Headers,������һ������ */
    }
    
    //
    // Check to see whether this is one of our extension headers. These
    // are very likely to appear in requests.
	/* ������κ�Ĭ�ϵ�static sHeaders[theHeader]���(���ƴ�Сд),����Extension Headers */
    if (theHeader == qtssExtensionHeaders)//45
    {
		/* ��ExtensionHeaders��ʼ��ĩβѭ��,���Ҳ���������ַ�����ͬ(���Դ�Сд)�Ķ�ӦRTSPProtocol::sHeaders[]�е���Ӧ���������� */
        for (SInt32 y = qtssExtensionHeaders; y < qtssNumHeaders; y++)
        {
            if (inHeaderStr.EqualIgnoreCase(sHeaders[y].Ptr, sHeaders[y].Len))
                return y;
        }
    }
    
    //
    // It's not one of our extension headers, check to see if this is one of
    // our normal VIP headers
	/* ��鿴����Ƿ���ͨ����VIP Header,���Ǿͷ��ض�ӦIndex */
    if ((theHeader != qtssIllegalHeader) &&/* ע��˴�˵��theHeader�ѱ����¸�ֵ,��ֻ����VIP header */
        (inHeaderStr.EqualIgnoreCase(sHeaders[theHeader].Ptr, sHeaders[theHeader].Len)))
        return theHeader;

    //
    //If this isn't one of our VIP headers, go through the remaining request headers, trying
    //to find the right one.
	/* ����Ƿ���������Header(extension/VIP Headers����)?���Ǿͷ��ض�ӦIndex */
    for (SInt32 x = qtssNumVIPHeaders; x < qtssNumHeaders; x++)
    {
        if (inHeaderStr.EqualIgnoreCase(sHeaders[x].Ptr, sHeaders[x].Len))
            return x;
    }
    return qtssIllegalHeader;
}


/* ������RTSP����ʱ��״̬���ַ��� */
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

/* ������RTSP����ʱ��״̬���� */
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

/* ������RTSP �汾�ַ��� */
StrPtrLen RTSPProtocol::sVersionString[] = 
{
    StrPtrLen("RTSP/1.0")
};

/* �ж����versionStr�Ƿ�Ϸ�?����"RTSP/1.0" */
RTSPProtocol::RTSPVersion
RTSPProtocol::GetVersion(StrPtrLen &versionStr)
{
    if (versionStr.Len != 8)
        return kIllegalVersion;
    else
        return k10Version;
}
