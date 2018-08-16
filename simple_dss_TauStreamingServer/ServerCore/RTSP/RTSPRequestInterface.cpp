
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequestInterface.cpp
Description: Provides a simple API for modules to access request information and
             manipulate (and possibly send) the client response.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 




#include <sys/types.h>
#include <sys/uio.h>

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"

#include "StringParser.h"
#include "OSMemory.h"
#include "OSThread.h"
#include "DateTranslator.h"
#include "QTSSDataConverter.h"
#include "OSArrayObjectDeleter.h"
#include "QTSSPrefs.h"
#include "QTSServerInterface.h"

char        RTSPRequestInterface::sPremadeHeader[kStaticHeaderSizeInBytes];
StrPtrLen   RTSPRequestInterface::sPremadeHeaderPtr(sPremadeHeader, kStaticHeaderSizeInBytes);

char        RTSPRequestInterface::sPremadeNoHeader[kStaticHeaderSizeInBytes];
StrPtrLen   RTSPRequestInterface::sPremadeNoHeaderPtr(sPremadeNoHeader, kStaticHeaderSizeInBytes);


StrPtrLen   RTSPRequestInterface::sColonSpace(": ", 2);

QTSSAttrInfoDict::AttrInfo  RTSPRequestInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "qtssRTSPReqFullRequest",         NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssRTSPReqMethodStr",           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 2 */ { "qtssRTSPReqFilePath",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 3 */ { "qtssRTSPReqURI",                 NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4 */ { "qtssRTSPReqFilePathTrunc",       GetTruncatedPath,       qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 5 */ { "qtssRTSPReqFileName",            GetFileName,            qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 6 */ { "qtssRTSPReqFileDigit",           GetFileDigit,           qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 7 */ { "qtssRTSPReqAbsoluteURL",         NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 8 */ { "qtssRTSPReqTruncAbsoluteURL",    GetAbsTruncatedPath,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 9 */ { "qtssRTSPReqMethod",              NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 10 */ { "qtssRTSPReqStatusCode",         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 11 */ { "qtssRTSPReqStartTime",          NULL,                   qtssAttrDataTypeFloat64,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 12 */ { "qtssRTSPReqStopTime",           NULL,                   qtssAttrDataTypeFloat64,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 13 */ { "qtssRTSPReqRespKeepAlive",      NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 14 */ { "qtssRTSPReqRootDir",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 15 */ { "qtssRTSPReqRealStatusCode",     GetRealStatusCode,      qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 16 */ { "qtssRTSPReqStreamRef",          NULL,                   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },
    
    /* 17 */ { "qtssRTSPReqUserName",           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 18 */ { "qtssRTSPReqUserPassword",       NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 19 */ { "qtssRTSPReqUserAllowed",        NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 20 */ { "qtssRTSPReqURLRealm",           NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 21 */ { "qtssRTSPReqLocalPath",          GetLocalPath,			qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 22 */ { "qtssRTSPReqIfModSinceDate",     NULL,                   qtssAttrDataTypeTimeVal,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 23 */ { "qtssRTSPReqQueryString",        NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 24 */ { "qtssRTSPReqRespMsg",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 25 */ { "qtssRTSPReqContentLen",         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 26 */ { "qtssRTSPReqSpeed",              NULL,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 27 */ { "qtssRTSPReqLateTolerance",      NULL,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 28 */ { "qtssRTSPReqTransportType",      NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 29 */ { "qtssRTSPReqTransportMode",      NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 30 */ { "qtssRTSPReqSetUpServerPort",    NULL,                   qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
    /* 31 */ { "qtssRTSPReqAction",             NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 32 */ { "qtssRTSPReqUserProfile",        NULL,                   qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 33 */ { "qtssRTSPReqPrebufferMaxTime",   NULL,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 34 */ { "qtssRTSPReqAuthScheme",         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 35 */ { "qtssRTSPReqSkipAuthorization",  NULL,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 36 */ { "qtssRTSPReqNetworkMode",		NULL,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 37 */ { "qtssRTSPReqDynamicRateValue",	NULL,					qtssAttrDataTypeSInt32,		qtssAttrModeRead | qtssAttrModePreempSafe }
 };


/* 生成sPremadeHeaderPtr和sPremadeNoHeaderPtr,搭建两个属性字典RTSPRequestDict和RTSPHeaderDict */
void  RTSPRequestInterface::Initialize()
{
    //make a partially complete header
	/* 生成一个部分完整的RTSP头(包含服务器信息),格式如下:
	   RTSP/1.0 200 OK\r\n
	   Server: QTSS/4.1.3.x (Build/425; Platform/MacOSX; Release/Development;)\r\n
	   Cseq: 
	*/
    StringFormatter headerFormatter(sPremadeHeaderPtr.Ptr, kStaticHeaderSizeInBytes);
    PutStatusLine(&headerFormatter, qtssSuccessOK, RTSPProtocol::k10Version);
    
    headerFormatter.Put(QTSServerInterface::GetServerHeader());
    headerFormatter.PutEOL();
    headerFormatter.Put(RTSPProtocol::GetHeaderString(qtssCSeqHeader));
    headerFormatter.Put(sColonSpace);
    sPremadeHeaderPtr.Len = headerFormatter.GetCurrentOffset();
    Assert(sPremadeHeaderPtr.Len < kStaticHeaderSizeInBytes);
    
    /* 生成一个部分完整的RTSP头(不包含服务器信息),格式如下:
	   RTSP/1.0 200 OK\r\n
	   Cseq: 
	*/    
    StringFormatter noServerInfoHeaderFormatter(sPremadeNoHeaderPtr.Ptr, kStaticHeaderSizeInBytes);
    PutStatusLine(&noServerInfoHeaderFormatter, qtssSuccessOK, RTSPProtocol::k10Version);
    noServerInfoHeaderFormatter.Put(RTSPProtocol::GetHeaderString(qtssCSeqHeader));
    noServerInfoHeaderFormatter.Put(sColonSpace);
    sPremadeNoHeaderPtr.Len = noServerInfoHeaderFormatter.GetCurrentOffset();
    Assert(sPremadeNoHeaderPtr.Len < kStaticHeaderSizeInBytes);
    
    //Setup all the dictionary stuff
	/* 搭建两个属性字典RTSPRequestDict和RTSPHeaderDict */
    for (UInt32 x = 0; x < qtssRTSPReqNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                            sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
    
    QTSSDictionaryMap* theHeaderMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex);
    for (UInt32 y = 0; y < qtssNumHeaders; y++)
        theHeaderMap->SetAttribute(y, RTSPProtocol::GetHeaderString(y).Ptr, NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe);
}

//CONSTRUCTOR / DESTRUCTOR: very simple stuff
/* 注意仔细研究/体会这个类的构造,比较深邃! */
RTSPRequestInterface::RTSPRequestInterface(RTSPSessionInterface *session)
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)),
	fMethod(qtssIllegalMethod),/* 默认200 OK */
	fStatus(qtssSuccessOK),
    fRealStatusCode(0),
    fRequestKeepAlive(true),/* 是心跳请求吗? */
    //fResponseKeepAlive(true), //parameter need not be set
    fVersion(RTSPProtocol::k10Version),/* 默认版本号0  */
    fStartTime(-1),
    fStopTime(-1),
    fClientPortA(0),
    fClientPortB(0),
    fTtl(0),
    fDestinationAddr(0),
    fSourceAddr(0),
    fTransportType(qtssRTPTransportTypeUDP),/* 默认UDP */
    fNetworkMode(qtssRTPNetworkModeDefault),    
    fContentLength(0),
    fIfModSinceDate(0),
    fSpeed(0),
    fLateTolerance(-1),
    fPrebufferAmt(-1),
    fWindowSize(0),
    fMovieFolderPtr(&fMovieFolderPath[0]),
    fHeaderDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex)),
    fAllowed(true),/* 默认允许认证 */
    fTransportMode(qtssRTPTransportModePlay),/* 默认Play而非Record模式 */
    fSetUpServerPort(0),
    fAction(qtssActionFlagsNoFlags),
    fAuthScheme(qtssAuthNone),
    fAuthQop(RTSPSessionInterface::kNoQop),
    fUserProfile(),
    fUserProfilePtr(&fUserProfile),
    fStale(false),
    fSkipAuthorization(false),//默认要Authorize
    fEnableDynamicRateState(-1),// -1 undefined, 0 disabled, 1 enabled
	// DJM PROTOTYPE
	fRandomDataSize(0),
    fSession(session),/* 默认指定的RTSPSessionInterface */
    fOutputStream(session->GetOutputStream()),/* 默认该RTSPSessionInterface的RTSPResponseStream */
    fStandardHeadersWritten(false)/* 默认不写RTSPStandardHeader */
{
    //Setup QTSS parameters that can be setup now. These are typically the parameters that are actually
    //pointers to binary variable values. Because these variables are just member variables of this object,
    //we can properly initialize their pointers right off the bat.

    fStreamRef = this;
    RTSPRequestStream* input = session->GetInputStream();/* 得到指定的RTSPRequestStream */
    this->SetVal(qtssRTSPReqFullRequest, input->GetRequestBuffer()->Ptr, input->GetRequestBuffer()->Len);/* 设置FullRTSPRequest的缓存地址 */
    this->SetVal(qtssRTSPReqMethod, &fMethod, sizeof(fMethod));
    this->SetVal(qtssRTSPReqStatusCode, &fStatus, sizeof(fStatus));
    this->SetVal(qtssRTSPReqRespKeepAlive, &fResponseKeepAlive, sizeof(fResponseKeepAlive));
    this->SetVal(qtssRTSPReqStreamRef, &fStreamRef, sizeof(fStreamRef));
    this->SetVal(qtssRTSPReqContentLen, &fContentLength, sizeof(fContentLength));
    this->SetVal(qtssRTSPReqSpeed, &fSpeed, sizeof(fSpeed));
    this->SetVal(qtssRTSPReqLateTolerance, &fLateTolerance, sizeof(fLateTolerance));
    this->SetVal(qtssRTSPReqPrebufferMaxTime, &fPrebufferAmt, sizeof(fPrebufferAmt));
    
    // Get the default root directory from QTSSPrefs, and store that in the proper parameter
    // Note that the GetMovieFolderPath function may allocate memory, so we check for that
    // in this object's destructor and free that memory if necessary.
    UInt32 pathLen = kMovieFolderBufSizeInBytes;
    fMovieFolderPtr = QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(fMovieFolderPtr, &pathLen);/* 从QTSSPrefs得到默认的媒体文件夹 */
    //this->SetVal(qtssRTSPReqRootDir, fMovieFolderPtr, pathLen);
	this->SetValue(qtssRTSPReqRootDir, 0, fMovieFolderPtr, pathLen, QTSSDictionary::kDontObeyReadOnly);
	    
    //There are actually other attributes that point to member variables that we COULD setup now, but they are attributes that
    //typically aren't set for every request, so we lazy initialize those when we parse the request

    this->SetVal(qtssRTSPReqUserAllowed, &fAllowed, sizeof(fAllowed));
    this->SetVal(qtssRTSPReqTransportType, &fTransportType, sizeof(fTransportType));
    this->SetVal(qtssRTSPReqTransportMode, &fTransportMode, sizeof(fTransportMode));
    this->SetVal(qtssRTSPReqSetUpServerPort, &fSetUpServerPort, sizeof(fSetUpServerPort));
    this->SetVal(qtssRTSPReqAction, &fAction, sizeof(fAction));
    this->SetVal(qtssRTSPReqUserProfile, &fUserProfilePtr, sizeof(QTSSUserProfile*));
    this->SetVal(qtssRTSPReqAuthScheme, &fAuthScheme, sizeof(fAuthScheme));
    this->SetVal(qtssRTSPReqSkipAuthorization, &fSkipAuthorization, sizeof(fSkipAuthorization));

    this->SetVal(qtssRTSPReqDynamicRateState, &fEnableDynamicRateState, sizeof(fEnableDynamicRateState));
 }

/* 向RTSPResponseStream放入如下格式的信息：*/
/***** Standard RTSP Headers部分 **********/
/*
RTSP/1.0 200 OK\r\n
Server: QTSS/4.1.3.x (Build/425; Platform/MacOSX; Release/Development;)\r\n
Cseq: 7\r\n
Session: 6664885458621367225\r\n
Connection: Close\r\n (此行一般没有)
*/
/***** Specified RTSP Headers部分 **********/
/*
HeaderString：inValue \r\n
*/
void RTSPRequestInterface::AppendHeader(QTSS_RTSPHeader inHeader, StrPtrLen* inValue)
{
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();
    
	/* 附加指定的RTSP Header字符串 */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
    fOutputStream->Put(sColonSpace);
    fOutputStream->Put(*inValue);
    fOutputStream->PutEOL();
}

/* 得到如下状态行: RTSP/1.0 200 OK\r\n */
void RTSPRequestInterface::PutStatusLine(StringFormatter* putStream, QTSS_RTSPStatusCode status,
                                        RTSPProtocol::RTSPVersion version)
{
	//RTSP/1.0
    putStream->Put(RTSPProtocol::GetVersionString(version));
    putStream->PutSpace();
    putStream->Put(RTSPProtocol::GetStatusCodeAsString(status));//200
    putStream->PutSpace();
    putStream->Put(RTSPProtocol::GetStatusCodeString(status));//OK
    putStream->PutEOL(); //\r\n   
}

/* 向RTSPResponseStream放入如下格式的信息：
	ANNOUNCE rtsp://127.0.0.1/mystream.sdp RTSP/1.0\r\n
	CSeq: 1\r\n
	Content-Type: application/sdp\r\n
	User-Agent: QTS (qtver=6.1;cpu=PPC;os=Mac 10.2.3)\r\n
	Content-Length: 790\r\n
	\r\n
*/
void RTSPRequestInterface::AppendContentLength(UInt32 contentLength)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	char dataSize[10];
	dataSize[sizeof(dataSize) -1] = 0;
	qtss_snprintf(dataSize, sizeof(dataSize) -1, "%lu", contentLength);
	StrPtrLen contentLengthStr(dataSize);
	/* 附加一行: Content-Length: 790 */
    this->AppendHeader(qtssContentLengthHeader, &contentLengthStr);
    
}

/* 获取当前线程的时间缓冲,附加到Response的Date和Expire头中:
	Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
	Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
*/
void RTSPRequestInterface::AppendDateAndExpires()
{
	/* 若没写标准Response头,就写它 */
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

    Assert(OSThread::GetCurrent() != NULL);
	/* 获得当前线程的时间缓冲theDate */
    DateBuffer* theDateBuffer = OSThread::GetCurrent()->GetDateBuffer();
    theDateBuffer->InexactUpdate(); // Update the date buffer to the current date & time
    StrPtrLen theDate(theDateBuffer->GetDateBuffer(), DateBuffer::kDateBufferLen);
    
    // Append dates, and have this response expire immediately
	/* 附加theDate到Response的Date和Expire头中 */
    this->AppendHeader(qtssDateHeader, &theDate);
    this->AppendHeader(qtssExpiresHeader, &theDate);
}

/* 
  向RTSPResponseStream中附加一行:
  Session: 6664885458621367225;timeout= 20\r\n 
  从而,最终向RTSPResponseStream放入如下内容:
  (1)当有超时值时:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 12\r\n
  Session: 1900075377083826623;timeout=20\r\n
  \r\n
  (2)当无超时值时:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 12\r\n
  Session: 1900075377083826623\r\n
  \r\n
*/
void RTSPRequestInterface::AppendSessionHeaderWithTimeout( StrPtrLen* inSessionID, StrPtrLen* inTimeout )
{

    // Append a session header if there wasn't one already
    if ( GetHeaderDictionary()->GetValue(qtssSessionHeader)->Len == 0)
    {   
        if (!fStandardHeadersWritten)
            this->WriteStandardHeaders();

        static StrPtrLen    sTimeoutString(";timeout=");

        // Just write out the session header and session ID
        if (inSessionID != NULL && inSessionID->Len > 0)
        {
			/* 向RTSPResponseStream中附加一行:Session: 6664885458621367225 */
            fOutputStream->Put( RTSPProtocol::GetHeaderString(qtssSessionHeader ) );
            fOutputStream->Put(sColonSpace);
            fOutputStream->Put( *inSessionID );
        
            /* 向RTSPResponseStream中接着附加:;timeout= 20 */
            if ( inTimeout != NULL && inTimeout->Len != 0)
            {
                fOutputStream->Put( sTimeoutString );
                fOutputStream->Put( *inTimeout );
            }
        
        
            fOutputStream->PutEOL();
        }
    }

}

/* 从full transport header中剥离出指定的field,再将剥离后的transport header放入fOutputStream  */
void RTSPRequestInterface::PutTransportStripped(StrPtrLen &fullTransportHeader, StrPtrLen &fieldToStrip)
{
       
        // skip the fieldToStrip and echo the rest back
	    /* 依照要剥离的field,将原来full transport header分为两部分 */
        UInt32 offset = (UInt32) (fieldToStrip.Ptr - fullTransportHeader.Ptr);        
        StrPtrLen transportStart(fullTransportHeader.Ptr,offset);
		/* 第一部分后退至";"处 */
        while (transportStart.Len > 0) // back up removing chars up to and including ;
        {  
            transportStart.Len --;
            if (transportStart[transportStart.Len] == ';')
                break;
        }
    
        StrPtrLen transportRemainder(fieldToStrip.Ptr,fullTransportHeader.Len - offset);        
        StringParser transportParser(&transportRemainder);
		/* 剩余部分前移至";"处 */
        transportParser.ConsumeUntil(&fieldToStrip, ';'); //remainder starts with ;       
        transportRemainder.Set(transportParser.GetCurrentPosition(),transportParser.GetDataRemaining());
        
		/* 将这两部分依序放入fOutputStream */
        fOutputStream->Put(transportStart);	
        fOutputStream->Put(transportRemainder);	

}

/* 在解析RTSPRequest::ParseTransportHeader()后,发回给client的transportHeaders,其格式如下:
   Transport: RTP/AVP;unicast;mode=record;source=127.0.0.1;client_port=6972-6973;server_port=6978-6979;interleaved=2-3;ssrc=001222AF\r\n
*/
void RTSPRequestInterface::AppendTransportHeader(StrPtrLen* serverPortA,
                                                    StrPtrLen* serverPortB,
                                                    StrPtrLen* channelA,
                                                    StrPtrLen* channelB,
                                                    StrPtrLen* serverIPAddr,
                                                    StrPtrLen* ssrc)
{
    static StrPtrLen    sServerPortString(";server_port=");
    static StrPtrLen    sSourceString(";source=");
    static StrPtrLen    sInterleavedString(";interleaved=");
    static StrPtrLen    sSSRC(";ssrc=");
    static StrPtrLen    sInterLeaved("interleaved");//match the interleaved tag
    static StrPtrLen    sClientPort("client_port");
    static StrPtrLen    sClientPortString(";client_port=");
    
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

    // Just write out the same transport header the client sent to us.
	/* 附加"Transport: " */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssTransportHeader));//Transport
    fOutputStream->Put(sColonSpace);//: 

    StrPtrLen outFirstTransport(fFirstTransport.GetAsCString());
    OSCharArrayDeleter outFirstTransportDeleter(outFirstTransport.Ptr);
	/* 移除fFirstTransport开头的空格和末尾的";" */
    outFirstTransport.RemoveWhitespace();
    while (outFirstTransport[outFirstTransport.Len - 1] == ';')
        outFirstTransport.Len --;

    // see if it contains an interleaved field or client port field
	/* 从fFirstTransport中找出"interleaved"和"client_port"域 */
    StrPtrLen stripClientPortStr;
    StrPtrLen stripInterleavedStr;
    (void) outFirstTransport.FindStringIgnoreCase(sClientPort, &stripClientPortStr);
    (void) outFirstTransport.FindStringIgnoreCase(sInterLeaved, &stripInterleavedStr);
    
    // echo back the transport without the interleaved or client ports fields we will add those in ourselves
	/* 将剥离出"interleaved"和"client_port"域的fFirstTransport放入fOutputStream */
    if (stripClientPortStr.Len != 0)
        PutTransportStripped(outFirstTransport, stripClientPortStr);
    else if (stripInterleavedStr.Len != 0) 
        PutTransportStripped(outFirstTransport, stripInterleavedStr);
    else
        fOutputStream->Put(outFirstTransport);
         
     
    //The source IP addr is optional, only append it if it is provided
	/* 附加";source=172.16.34.22" */
    if (serverIPAddr != NULL)
    {
        fOutputStream->Put(sSourceString);
        fOutputStream->Put(*serverIPAddr);
    }
    
    // Append the client ports,
	/* 附加";client_port=6972-6973" */
    if (stripClientPortStr.Len != 0)
    {
        fOutputStream->Put(sClientPortString);
        UInt16 portA = this->GetClientPortA();
        UInt16 portB = this->GetClientPortB();
        StrPtrLenDel clientPortA(QTSSDataConverter::ValueToString( &portA, sizeof(portA), qtssAttrDataTypeUInt16));
        StrPtrLenDel clientPortB(QTSSDataConverter::ValueToString( &portB, sizeof(portB), qtssAttrDataTypeUInt16));
        
        fOutputStream->Put(clientPortA);
        fOutputStream->PutChar('-');
        fOutputStream->Put(clientPortB);        
    }
    
    // Append the server ports, if provided.
	/* 附加";server_port=6978-6979" */
    if (serverPortA != NULL)
    {
        fOutputStream->Put(sServerPortString);
        fOutputStream->Put(*serverPortA);
        fOutputStream->PutChar('-');
        fOutputStream->Put(*serverPortB);
    }
    
    // Append channel #'s, if provided
	/* 附加";interleaved=2-3"  */
    if (channelA != NULL)
    {
        fOutputStream->Put(sInterleavedString);
        fOutputStream->Put(*channelA);
        fOutputStream->PutChar('-');
        fOutputStream->Put(*channelB);
    }
    
	/* 附加";ssrc=001222AF" */
    if (ssrc != NULL && ssrc->Ptr != NULL && ssrc->Len != 0 && fNetworkMode == qtssRTPNetworkModeUnicast && fTransportMode == qtssRTPTransportModePlay)
    {
        char* theCString = ssrc->GetAsCString();
        OSCharArrayDeleter cStrDeleter(theCString);
        
		/* 从ssrc字符串中找出ssrc值 */
        UInt32 ssrcVal = 0;
        ::sscanf(theCString, "%lu", &ssrcVal);
        ssrcVal = htonl(ssrcVal);
        
        StrPtrLen hexSSRC(QTSSDataConverter::ValueToString( &ssrcVal, sizeof(ssrcVal), qtssAttrDataTypeUnknown));
        OSCharArrayDeleter hexStrDeleter(hexSSRC.Ptr);

        fOutputStream->Put(sSSRC);
        fOutputStream->Put(hexSSRC);
    }

    fOutputStream->PutEOL();
}


/* 附加一行: Content-Base: rtsp://172.16.34.22/demo.mp4/\r\n */
void RTSPRequestInterface::AppendContentBaseHeader(StrPtrLen* theURL)
{
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

	/* 附加一行: Content-Base: rtsp://172.16.34.22/demo.mp4/\r\n */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssContentBaseHeader));//Content-Base
    fOutputStream->Put(sColonSpace);//:
    fOutputStream->Put(*theURL);
    fOutputStream->PutChar('/');
    fOutputStream->PutEOL();
}

/* 附加一行: x-Retransmit: our-retransmit;ack-timeout=10;window=128\r\n */
void RTSPRequestInterface::AppendRetransmitHeader(UInt32 inAckTimeout)
{
    static const StrPtrLen kAckTimeout("ack-timeout=");

	/* 附加一行: x-Retransmit: our-retransmit;ack-timeout=10 */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssXRetransmitHeader));//x-Retransmit
    fOutputStream->Put(sColonSpace);//:
    fOutputStream->Put(RTSPProtocol::GetRetransmitProtocolName());//our-retransmit
    fOutputStream->PutChar(';');
    fOutputStream->Put(kAckTimeout);//ack-timeout=
    fOutputStream->Put(inAckTimeout);
    
    if (fWindowSizeStr.Len > 0)
    {
        //
        // If the client provided a window size, append that as well.
        fOutputStream->PutChar(';');
        fOutputStream->Put(fWindowSizeStr);//window = 1320
    }
    
    fOutputStream->PutEOL();
    
}

/* 附加:"RTPInfo: url=rtsp://172.16.34.22/sample_h264_1mbit.mp4/trackID=3;seq=65038;ssrc=6AB9E057;rtptime=581787324''
注意:(1)若是qtssRTPInfoHeader,就附加:"RTPInfo";若是qtssSameAsLastHeader,就附加",";(2)若是qtssRTPInfoHeader,再附加附加": "(冒号空格);
(3)假如有同步源,附加:";ssrc=6AB9E057";(4)假如是最后一个RTPStream,就附加"\r\n" */
void RTSPRequestInterface::AppendRTPInfoHeader(QTSS_RTSPHeader inHeader,
                                                StrPtrLen* url, StrPtrLen* seqNumber,
                                                StrPtrLen* ssrc, StrPtrLen* rtpTime, Bool16 lastRTPInfo)
{
    static StrPtrLen sURL("url=", 4);
    static StrPtrLen sSeq(";seq=", 5);
    static StrPtrLen sSsrc(";ssrc=", 6);
    static StrPtrLen sRTPTime(";rtptime=", 9);

    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

	/* 若是qtssRTPInfoHeader,就附加:"RTPInfo";若是qtssSameAsLastHeader,就附加"," */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
	/* 假如指定的RTSP头是qtssRTPInfoHeader,不是qtssSameAsLastHeader,就附加": "(冒号空格) */
    if (inHeader != qtssSameAsLastHeader)
        fOutputStream->Put(sColonSpace);
        
    //Only append the various bits of RTP information if they actually have been providied
	/* 附加:"url=rtsp://172.16.34.22/sample_h264_1mbit.mp4/trackID=3" */
    if ((url != NULL) && (url->Len > 0))
    {   /* 附加"url=" */
        fOutputStream->Put(sURL);

		if (true) //3gpp requires this and it follows RTSP RFC.
		{
			RTSPRequestInterface* theRequest = (RTSPRequestInterface*)this;
			/* 得到RTSP Request的absolute url是rtsp://172.16.34.22/sample_h264_1mbit.mp4/ */
			StrPtrLen *path = (StrPtrLen *) theRequest->GetValue(qtssRTSPReqAbsoluteURL);
		    
			if (path != NULL && path->Len > 0)
			{   
				fOutputStream->Put(*path);
				if(path->Ptr[path->Len-1] != '/')
					fOutputStream->PutChar('/');
			}
		}
        
		/* 附加"trackID=3" */
        fOutputStream->Put(*url);
    }

	/* 附加:";seq=65038" */
    if ((seqNumber != NULL) && (seqNumber->Len > 0))
    {
        fOutputStream->Put(sSeq);
        fOutputStream->Put(*seqNumber);
    }
	/* 假如有同步源,附加:";ssrc=6AB9E057" */
    if ((ssrc != NULL) && (ssrc->Len > 0))
    {
        fOutputStream->Put(sSsrc);
        fOutputStream->Put(*ssrc);
    }
	/* 附加:";rtptime=581787324" */
    if ((rtpTime != NULL) && (rtpTime->Len > 0))
    {
        fOutputStream->Put(sRTPTime);
        fOutputStream->Put(*rtpTime);
    }
    
	/* 假如是最后一个RTPStream,就附加"\r\n" */
    if (lastRTPInfo)
        fOutputStream->PutEOL();
}


/* 注意这个函数使用极其频繁! */
/*
  向Client发送响应,向RTSPResponseStream中放入如下格式的信息:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 13\r\n
  Session: 1900075377083826623\r\n
  Connection: Close\r\n
  注意最后一行,依据fResponseKeepAlive的值,若为false,就加上.参见RTPSession::SendTeardownResponse()
*/
void RTSPRequestInterface::WriteStandardHeaders()
{
    static StrPtrLen    sCloseString("Close", 5);

    Assert(sPremadeHeader != NULL);
    fStandardHeadersWritten = true; //must be done here to prevent recursive calls
    
    //if this is a "200 OK" response (most HTTP responses), we have some special
    //optmizations here
	/* 是否发送RTSPServer的信息? */
    Bool16 sendServerInfo = QTSServerInterface::GetServer()->GetPrefs()->GetRTSPServerInfoEnabled();
	/* 假如当前状态码是200 OK  */
    if (fStatus == qtssSuccessOK)
    {
        
        if (sendServerInfo)
        {   fOutputStream->Put(sPremadeHeaderPtr);
        }
        else
        {
            fOutputStream->Put(sPremadeNoHeaderPtr);
        }
		/* 获取"C-Seq"头,并放入fOutputStream */
        StrPtrLen* cSeq = fHeaderDictionary.GetValue(qtssCSeqHeader);
        Assert(cSeq != NULL);
        if (cSeq->Len > 1)
            fOutputStream->Put(*cSeq);
        else if (cSeq->Len == 1)
            fOutputStream->PutChar(*cSeq->Ptr);
        fOutputStream->PutEOL();
    }
    else
    {
#if 0
		// if you want the connection to stay alive when we don't grok
		// the specified parameter then enable this code. - [sfu]
		if (fStatus == qtssClientParameterNotUnderstood) {
			fResponseKeepAlive = true;
		}
#endif 
        //other status codes just get built on the fly
		/* 添上类似状态行:RTSP/1.0 200 OK */
        PutStatusLine(fOutputStream, fStatus, RTSPProtocol::k10Version);
        if (sendServerInfo)
        {
            fOutputStream->Put(QTSServerInterface::GetServerHeader());
            fOutputStream->PutEOL();
        }
		/* 附加"C-Seq"头 */
        AppendHeader(qtssCSeqHeader, fHeaderDictionary.GetValue(qtssCSeqHeader));
    }

    //append sessionID header
	//附加"Session"头
    StrPtrLen* incomingID = fHeaderDictionary.GetValue(qtssSessionHeader);
    if ((incomingID != NULL) && (incomingID->Len > 0))
        AppendHeader(qtssSessionHeader, incomingID);

    //follows the HTTP/1.1 convention: if server wants to close the connection, it
    //tags the response with the Connection: close header
	/* 假如想关闭连接,附加"Connection"头 */
    if (!fResponseKeepAlive)
        AppendHeader(qtssConnectionHeader, &sCloseString);
}

/* used in RTSPSession::SetupRequest(),RTPStream::SendSetupResponse() */
/* 发送标准响应头 */
/*
  向Client发送响应,向RTSPResponseStream中放入如下格式的信息:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 12\r\n
  Session: 1900075377083826623\r\n
  \r\n
*/
void RTSPRequestInterface::SendHeader()
{
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();
    fOutputStream->PutEOL();
}

QTSS_Error
RTSPRequestInterface::Write(void* inBuffer, UInt32 inLength, UInt32* outLenWritten, UInt32 /*inFlags*/)
{
    //now just write whatever remains into the output buffer
    fOutputStream->Put((char*)inBuffer, inLength);
    
    if (outLenWritten != NULL)
        *outLenWritten = inLength;
        
    return QTSS_NoErr;
}

QTSS_Error
RTSPRequestInterface::WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten)
{
    (void)fOutputStream->WriteV(inVec, inNumVectors, inTotalLength, NULL,
                                                RTSPResponseStream::kAlwaysBuffer);
    if (outLenWritten != NULL)
        *outLenWritten = inTotalLength;
    return QTSS_NoErr;  
}

//param retrieval functions described in .h file
/* 获取rtsp请求的绝对路径:rtsp://172.16.34.22/,得到qtssRTSPReqTruncAbsoluteURL为rtsp:// */
void* RTSPRequestInterface::GetAbsTruncatedPath(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function gets called only once
	
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqTruncAbsoluteURL, theRequest->GetValue(qtssRTSPReqAbsoluteURL));

    //Adjust the length to truncate off the last file in the path
    /* 获取rtsp请求的绝对路径:rtsp://172.16.34.22/ */
    StrPtrLen* theAbsTruncPathParam = theRequest->GetValue(qtssRTSPReqTruncAbsoluteURL);
    theAbsTruncPathParam->Len--;
	/* 得到rtsp:// */
    while (theAbsTruncPathParam->Ptr[theAbsTruncPathParam->Len] != kPathDelimiterChar)
        theAbsTruncPathParam->Len--;
    
    return NULL;
}

/* 获取点播的文件名(包含路径),删去末尾的文件名,逆序移动直至移到"/" */
void* RTSPRequestInterface::GetTruncatedPath(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
	
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFilePathTrunc, theRequest->GetValue(qtssRTSPReqFilePath));

    //Adjust the length to truncate off the last file in the path
	/* 获取点播的文件名(包含路径) */
    StrPtrLen* theTruncPathParam = theRequest->GetValue(qtssRTSPReqFilePathTrunc);

    if (theTruncPathParam->Len > 0)
    {
		/* 去掉文件路径名的首字符"/" */
        theTruncPathParam->Len--;
		/* 删去末尾的文件名,逆序移动直至移到"/" */
        while ( (theTruncPathParam->Len != 0) && (theTruncPathParam->Ptr[theTruncPathParam->Len] != kPathDelimiterChar) )
            theTruncPathParam->Len--;
    }

    return NULL;
}

/* 从RTSPRequestInterface中获取点播文件名,找出从"/"到末尾的那段字符串做为文件名 */
void* RTSPRequestInterface::GetFileName(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
    
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFileName, theRequest->GetValue(qtssRTSPReqFilePath));

	/* 获取点播的文件名(包含路径) */
    StrPtrLen* theFileNameParam = theRequest->GetValue(qtssRTSPReqFileName);

    //paranoid check(多疑性)
    if (theFileNameParam->Len == 0)
        return theFileNameParam;
        
    //walk back in the file name until we hit a /
	/* 逆序遍历该文件路径上的每个字符,若若遇到"/"就中断循环 */
    SInt32 x = theFileNameParam->Len - 1;
    for (; x > 0; x--)
        if (theFileNameParam->Ptr[x] == kPathDelimiterChar)
            break;
    //once we do, make the tempPtr point to the next character after the slash,
    //and adjust the length accordingly
	/* 一旦我们遇到"/",就更新文件名 */
    if (theFileNameParam->Ptr[x] == kPathDelimiterChar )
    {
        theFileNameParam->Ptr = (&theFileNameParam->Ptr[x]) + 1;
        theFileNameParam->Len -= (x + 1);
    }
    
    return NULL;        
}

/* 从RTSPRequestInterface中获取点播文件名,逆序找出文件名末尾的连续数字 */
void* RTSPRequestInterface::GetFileDigit(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
    
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFileDigit, theRequest->GetValue(qtssRTSPReqFilePath));

	/* 获取点播的文件名(包含路径) */
    StrPtrLen* theFileDigit = theRequest->GetValue(qtssRTSPReqFileDigit);
    /* 获取点播的文件名(包含路径)的长度 */
    UInt32  theFilePathLen = theRequest->GetValue(qtssRTSPReqFilePath)->Len;

	/* 定位在文件名的末尾 */
    theFileDigit->Ptr += theFilePathLen - 1;
    theFileDigit->Len = 0;
	/* 逆序找出文件名末尾的连续数字 */
    while ((StringParser::sDigitMask[(unsigned int) *(*theFileDigit).Ptr] != '\0') &&
            (theFileDigit->Len <= theFilePathLen))
    {
        theFileDigit->Ptr--;
        theFileDigit->Len++;
    }
    //termination condition means that we aren't actually on a digit right now.
    //Move pointer back onto the digit
	/* 将指针移回第一个数字 */
    theFileDigit->Ptr++;
    
    return NULL;
}

/* 获取此RTSP request的状态码 */
void* RTSPRequestInterface::GetRealStatusCode(QTSSDictionary* inRequest, UInt32* outLen)
{
    // Set the fRealStatusCode variable based on the current fStatusCode.
	// This function always gets called
    RTSPRequestInterface* theReq = (RTSPRequestInterface*)inRequest;
	/* 获取此RTSP request的状态码 */
    theReq->fRealStatusCode = RTSPProtocol::GetStatusCode(theReq->fStatus);
    *outLen = sizeof(UInt32);
    return &theReq->fRealStatusCode;
}

/* 从RTSPRequestInterface中获取点播文件名和根路径名,拼合得到本地路径qtssRTSPReqLocalPath */
void* RTSPRequestInterface::GetLocalPath(QTSSDictionary* inRequest, UInt32* outLen)
{
	// This function always gets called	
	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	QTSS_AttributeID theID = qtssRTSPReqFilePath;
	
    // Get the truncated path on a setup, because setups have the trackID appended
	if (theRequest->GetMethod() == qtssSetupMethod)
	{
        theID = qtssRTSPReqFilePathTrunc;
		// invoke the param retrieval function here so that we can use the internal GetValue function later  
		RTSPRequestInterface::GetTruncatedPath(inRequest, outLen);
	}
    
	/* 得到点播文件路径名称 */
	StrPtrLen* thePath = theRequest->GetValue(theID);
	StrPtrLen filePath(thePath->Ptr, thePath->Len);
	/* 得到点播文件根路径 */
	StrPtrLen* theRootDir = theRequest->GetValue(qtssRTSPReqRootDir);

	/* 拼合根路径和点播文件路径得到qtssRTSPReqLocalPath  */
	if (theRootDir->Len && theRootDir->Ptr[theRootDir->Len -1] == kPathDelimiterChar
	    && thePath->Len  && thePath->Ptr[0] == kPathDelimiterChar)
	{
	    char *thePathEnd = &(filePath.Ptr[filePath.Len]);
		/* 仅移到一个"/" */
	    while (filePath.Ptr != thePathEnd)
	    {
	        if (*filePath.Ptr != kPathDelimiterChar)
	            break;
	            
	        filePath.Ptr ++;
	        filePath.Len --;
	    }
	}
	
	UInt32 fullPathLen = filePath.Len + theRootDir->Len;
	char* theFullPath = NEW char[fullPathLen+1];
	theFullPath[fullPathLen] = '\0';
	
	::memcpy(theFullPath, theRootDir->Ptr, theRootDir->Len);
	::memcpy(theFullPath + theRootDir->Len, filePath.Ptr, filePath.Len);
	
	/* 设置点播文件的qtssRTSPReqLocalPath */
	(void)theRequest->SetValue(qtssRTSPReqLocalPath, 0, theFullPath,fullPathLen , QTSSDictionary::kDontObeyReadOnly);
	
	// delete our copy of the data
	delete [] theFullPath;
	*outLen = 0;
	
	return NULL;
}
