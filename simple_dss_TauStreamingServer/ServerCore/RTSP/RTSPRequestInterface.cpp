
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


/* ����sPremadeHeaderPtr��sPremadeNoHeaderPtr,����������ֵ�RTSPRequestDict��RTSPHeaderDict */
void  RTSPRequestInterface::Initialize()
{
    //make a partially complete header
	/* ����һ������������RTSPͷ(������������Ϣ),��ʽ����:
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
    
    /* ����һ������������RTSPͷ(��������������Ϣ),��ʽ����:
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
	/* ����������ֵ�RTSPRequestDict��RTSPHeaderDict */
    for (UInt32 x = 0; x < qtssRTSPReqNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                            sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
    
    QTSSDictionaryMap* theHeaderMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex);
    for (UInt32 y = 0; y < qtssNumHeaders; y++)
        theHeaderMap->SetAttribute(y, RTSPProtocol::GetHeaderString(y).Ptr, NULL, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe);
}

//CONSTRUCTOR / DESTRUCTOR: very simple stuff
/* ע����ϸ�о�/��������Ĺ���,�Ƚ�����! */
RTSPRequestInterface::RTSPRequestInterface(RTSPSessionInterface *session)
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)),
	fMethod(qtssIllegalMethod),/* Ĭ��200 OK */
	fStatus(qtssSuccessOK),
    fRealStatusCode(0),
    fRequestKeepAlive(true),/* ������������? */
    //fResponseKeepAlive(true), //parameter need not be set
    fVersion(RTSPProtocol::k10Version),/* Ĭ�ϰ汾��0  */
    fStartTime(-1),
    fStopTime(-1),
    fClientPortA(0),
    fClientPortB(0),
    fTtl(0),
    fDestinationAddr(0),
    fSourceAddr(0),
    fTransportType(qtssRTPTransportTypeUDP),/* Ĭ��UDP */
    fNetworkMode(qtssRTPNetworkModeDefault),    
    fContentLength(0),
    fIfModSinceDate(0),
    fSpeed(0),
    fLateTolerance(-1),
    fPrebufferAmt(-1),
    fWindowSize(0),
    fMovieFolderPtr(&fMovieFolderPath[0]),
    fHeaderDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex)),
    fAllowed(true),/* Ĭ��������֤ */
    fTransportMode(qtssRTPTransportModePlay),/* Ĭ��Play����Recordģʽ */
    fSetUpServerPort(0),
    fAction(qtssActionFlagsNoFlags),
    fAuthScheme(qtssAuthNone),
    fAuthQop(RTSPSessionInterface::kNoQop),
    fUserProfile(),
    fUserProfilePtr(&fUserProfile),
    fStale(false),
    fSkipAuthorization(false),//Ĭ��ҪAuthorize
    fEnableDynamicRateState(-1),// -1 undefined, 0 disabled, 1 enabled
	// DJM PROTOTYPE
	fRandomDataSize(0),
    fSession(session),/* Ĭ��ָ����RTSPSessionInterface */
    fOutputStream(session->GetOutputStream()),/* Ĭ�ϸ�RTSPSessionInterface��RTSPResponseStream */
    fStandardHeadersWritten(false)/* Ĭ�ϲ�дRTSPStandardHeader */
{
    //Setup QTSS parameters that can be setup now. These are typically the parameters that are actually
    //pointers to binary variable values. Because these variables are just member variables of this object,
    //we can properly initialize their pointers right off the bat.

    fStreamRef = this;
    RTSPRequestStream* input = session->GetInputStream();/* �õ�ָ����RTSPRequestStream */
    this->SetVal(qtssRTSPReqFullRequest, input->GetRequestBuffer()->Ptr, input->GetRequestBuffer()->Len);/* ����FullRTSPRequest�Ļ����ַ */
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
    fMovieFolderPtr = QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(fMovieFolderPtr, &pathLen);/* ��QTSSPrefs�õ�Ĭ�ϵ�ý���ļ��� */
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

/* ��RTSPResponseStream�������¸�ʽ����Ϣ��*/
/***** Standard RTSP Headers���� **********/
/*
RTSP/1.0 200 OK\r\n
Server: QTSS/4.1.3.x (Build/425; Platform/MacOSX; Release/Development;)\r\n
Cseq: 7\r\n
Session: 6664885458621367225\r\n
Connection: Close\r\n (����һ��û��)
*/
/***** Specified RTSP Headers���� **********/
/*
HeaderString��inValue \r\n
*/
void RTSPRequestInterface::AppendHeader(QTSS_RTSPHeader inHeader, StrPtrLen* inValue)
{
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();
    
	/* ����ָ����RTSP Header�ַ��� */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
    fOutputStream->Put(sColonSpace);
    fOutputStream->Put(*inValue);
    fOutputStream->PutEOL();
}

/* �õ�����״̬��: RTSP/1.0 200 OK\r\n */
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

/* ��RTSPResponseStream�������¸�ʽ����Ϣ��
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
	/* ����һ��: Content-Length: 790 */
    this->AppendHeader(qtssContentLengthHeader, &contentLengthStr);
    
}

/* ��ȡ��ǰ�̵߳�ʱ�仺��,���ӵ�Response��Date��Expireͷ��:
	Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
	Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
*/
void RTSPRequestInterface::AppendDateAndExpires()
{
	/* ��ûд��׼Responseͷ,��д�� */
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

    Assert(OSThread::GetCurrent() != NULL);
	/* ��õ�ǰ�̵߳�ʱ�仺��theDate */
    DateBuffer* theDateBuffer = OSThread::GetCurrent()->GetDateBuffer();
    theDateBuffer->InexactUpdate(); // Update the date buffer to the current date & time
    StrPtrLen theDate(theDateBuffer->GetDateBuffer(), DateBuffer::kDateBufferLen);
    
    // Append dates, and have this response expire immediately
	/* ����theDate��Response��Date��Expireͷ�� */
    this->AppendHeader(qtssDateHeader, &theDate);
    this->AppendHeader(qtssExpiresHeader, &theDate);
}

/* 
  ��RTSPResponseStream�и���һ��:
  Session: 6664885458621367225;timeout= 20\r\n 
  �Ӷ�,������RTSPResponseStream������������:
  (1)���г�ʱֵʱ:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 12\r\n
  Session: 1900075377083826623;timeout=20\r\n
  \r\n
  (2)���޳�ʱֵʱ:
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
			/* ��RTSPResponseStream�и���һ��:Session: 6664885458621367225 */
            fOutputStream->Put( RTSPProtocol::GetHeaderString(qtssSessionHeader ) );
            fOutputStream->Put(sColonSpace);
            fOutputStream->Put( *inSessionID );
        
            /* ��RTSPResponseStream�н��Ÿ���:;timeout= 20 */
            if ( inTimeout != NULL && inTimeout->Len != 0)
            {
                fOutputStream->Put( sTimeoutString );
                fOutputStream->Put( *inTimeout );
            }
        
        
            fOutputStream->PutEOL();
        }
    }

}

/* ��full transport header�а����ָ����field,�ٽ�������transport header����fOutputStream  */
void RTSPRequestInterface::PutTransportStripped(StrPtrLen &fullTransportHeader, StrPtrLen &fieldToStrip)
{
       
        // skip the fieldToStrip and echo the rest back
	    /* ����Ҫ�����field,��ԭ��full transport header��Ϊ������ */
        UInt32 offset = (UInt32) (fieldToStrip.Ptr - fullTransportHeader.Ptr);        
        StrPtrLen transportStart(fullTransportHeader.Ptr,offset);
		/* ��һ���ֺ�����";"�� */
        while (transportStart.Len > 0) // back up removing chars up to and including ;
        {  
            transportStart.Len --;
            if (transportStart[transportStart.Len] == ';')
                break;
        }
    
        StrPtrLen transportRemainder(fieldToStrip.Ptr,fullTransportHeader.Len - offset);        
        StringParser transportParser(&transportRemainder);
		/* ʣ�ಿ��ǰ����";"�� */
        transportParser.ConsumeUntil(&fieldToStrip, ';'); //remainder starts with ;       
        transportRemainder.Set(transportParser.GetCurrentPosition(),transportParser.GetDataRemaining());
        
		/* �����������������fOutputStream */
        fOutputStream->Put(transportStart);	
        fOutputStream->Put(transportRemainder);	

}

/* �ڽ���RTSPRequest::ParseTransportHeader()��,���ظ�client��transportHeaders,���ʽ����:
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
	/* ����"Transport: " */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssTransportHeader));//Transport
    fOutputStream->Put(sColonSpace);//: 

    StrPtrLen outFirstTransport(fFirstTransport.GetAsCString());
    OSCharArrayDeleter outFirstTransportDeleter(outFirstTransport.Ptr);
	/* �Ƴ�fFirstTransport��ͷ�Ŀո��ĩβ��";" */
    outFirstTransport.RemoveWhitespace();
    while (outFirstTransport[outFirstTransport.Len - 1] == ';')
        outFirstTransport.Len --;

    // see if it contains an interleaved field or client port field
	/* ��fFirstTransport���ҳ�"interleaved"��"client_port"�� */
    StrPtrLen stripClientPortStr;
    StrPtrLen stripInterleavedStr;
    (void) outFirstTransport.FindStringIgnoreCase(sClientPort, &stripClientPortStr);
    (void) outFirstTransport.FindStringIgnoreCase(sInterLeaved, &stripInterleavedStr);
    
    // echo back the transport without the interleaved or client ports fields we will add those in ourselves
	/* �������"interleaved"��"client_port"���fFirstTransport����fOutputStream */
    if (stripClientPortStr.Len != 0)
        PutTransportStripped(outFirstTransport, stripClientPortStr);
    else if (stripInterleavedStr.Len != 0) 
        PutTransportStripped(outFirstTransport, stripInterleavedStr);
    else
        fOutputStream->Put(outFirstTransport);
         
     
    //The source IP addr is optional, only append it if it is provided
	/* ����";source=172.16.34.22" */
    if (serverIPAddr != NULL)
    {
        fOutputStream->Put(sSourceString);
        fOutputStream->Put(*serverIPAddr);
    }
    
    // Append the client ports,
	/* ����";client_port=6972-6973" */
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
	/* ����";server_port=6978-6979" */
    if (serverPortA != NULL)
    {
        fOutputStream->Put(sServerPortString);
        fOutputStream->Put(*serverPortA);
        fOutputStream->PutChar('-');
        fOutputStream->Put(*serverPortB);
    }
    
    // Append channel #'s, if provided
	/* ����";interleaved=2-3"  */
    if (channelA != NULL)
    {
        fOutputStream->Put(sInterleavedString);
        fOutputStream->Put(*channelA);
        fOutputStream->PutChar('-');
        fOutputStream->Put(*channelB);
    }
    
	/* ����";ssrc=001222AF" */
    if (ssrc != NULL && ssrc->Ptr != NULL && ssrc->Len != 0 && fNetworkMode == qtssRTPNetworkModeUnicast && fTransportMode == qtssRTPTransportModePlay)
    {
        char* theCString = ssrc->GetAsCString();
        OSCharArrayDeleter cStrDeleter(theCString);
        
		/* ��ssrc�ַ������ҳ�ssrcֵ */
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


/* ����һ��: Content-Base: rtsp://172.16.34.22/demo.mp4/\r\n */
void RTSPRequestInterface::AppendContentBaseHeader(StrPtrLen* theURL)
{
    if (!fStandardHeadersWritten)
        this->WriteStandardHeaders();

	/* ����һ��: Content-Base: rtsp://172.16.34.22/demo.mp4/\r\n */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssContentBaseHeader));//Content-Base
    fOutputStream->Put(sColonSpace);//:
    fOutputStream->Put(*theURL);
    fOutputStream->PutChar('/');
    fOutputStream->PutEOL();
}

/* ����һ��: x-Retransmit: our-retransmit;ack-timeout=10;window=128\r\n */
void RTSPRequestInterface::AppendRetransmitHeader(UInt32 inAckTimeout)
{
    static const StrPtrLen kAckTimeout("ack-timeout=");

	/* ����һ��: x-Retransmit: our-retransmit;ack-timeout=10 */
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

/* ����:"RTPInfo: url=rtsp://172.16.34.22/sample_h264_1mbit.mp4/trackID=3;seq=65038;ssrc=6AB9E057;rtptime=581787324''
ע��:(1)����qtssRTPInfoHeader,�͸���:"RTPInfo";����qtssSameAsLastHeader,�͸���",";(2)����qtssRTPInfoHeader,�ٸ��Ӹ���": "(ð�ſո�);
(3)������ͬ��Դ,����:";ssrc=6AB9E057";(4)���������һ��RTPStream,�͸���"\r\n" */
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

	/* ����qtssRTPInfoHeader,�͸���:"RTPInfo";����qtssSameAsLastHeader,�͸���"," */
    fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
	/* ����ָ����RTSPͷ��qtssRTPInfoHeader,����qtssSameAsLastHeader,�͸���": "(ð�ſո�) */
    if (inHeader != qtssSameAsLastHeader)
        fOutputStream->Put(sColonSpace);
        
    //Only append the various bits of RTP information if they actually have been providied
	/* ����:"url=rtsp://172.16.34.22/sample_h264_1mbit.mp4/trackID=3" */
    if ((url != NULL) && (url->Len > 0))
    {   /* ����"url=" */
        fOutputStream->Put(sURL);

		if (true) //3gpp requires this and it follows RTSP RFC.
		{
			RTSPRequestInterface* theRequest = (RTSPRequestInterface*)this;
			/* �õ�RTSP Request��absolute url��rtsp://172.16.34.22/sample_h264_1mbit.mp4/ */
			StrPtrLen *path = (StrPtrLen *) theRequest->GetValue(qtssRTSPReqAbsoluteURL);
		    
			if (path != NULL && path->Len > 0)
			{   
				fOutputStream->Put(*path);
				if(path->Ptr[path->Len-1] != '/')
					fOutputStream->PutChar('/');
			}
		}
        
		/* ����"trackID=3" */
        fOutputStream->Put(*url);
    }

	/* ����:";seq=65038" */
    if ((seqNumber != NULL) && (seqNumber->Len > 0))
    {
        fOutputStream->Put(sSeq);
        fOutputStream->Put(*seqNumber);
    }
	/* ������ͬ��Դ,����:";ssrc=6AB9E057" */
    if ((ssrc != NULL) && (ssrc->Len > 0))
    {
        fOutputStream->Put(sSsrc);
        fOutputStream->Put(*ssrc);
    }
	/* ����:";rtptime=581787324" */
    if ((rtpTime != NULL) && (rtpTime->Len > 0))
    {
        fOutputStream->Put(sRTPTime);
        fOutputStream->Put(*rtpTime);
    }
    
	/* ���������һ��RTPStream,�͸���"\r\n" */
    if (lastRTPInfo)
        fOutputStream->PutEOL();
}


/* ע���������ʹ�ü���Ƶ��! */
/*
  ��Client������Ӧ,��RTSPResponseStream�з������¸�ʽ����Ϣ:
  RTSP/1.0 200 OK\r\n
  Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
  Cseq: 13\r\n
  Session: 1900075377083826623\r\n
  Connection: Close\r\n
  ע�����һ��,����fResponseKeepAlive��ֵ,��Ϊfalse,�ͼ���.�μ�RTPSession::SendTeardownResponse()
*/
void RTSPRequestInterface::WriteStandardHeaders()
{
    static StrPtrLen    sCloseString("Close", 5);

    Assert(sPremadeHeader != NULL);
    fStandardHeadersWritten = true; //must be done here to prevent recursive calls
    
    //if this is a "200 OK" response (most HTTP responses), we have some special
    //optmizations here
	/* �Ƿ���RTSPServer����Ϣ? */
    Bool16 sendServerInfo = QTSServerInterface::GetServer()->GetPrefs()->GetRTSPServerInfoEnabled();
	/* ���統ǰ״̬����200 OK  */
    if (fStatus == qtssSuccessOK)
    {
        
        if (sendServerInfo)
        {   fOutputStream->Put(sPremadeHeaderPtr);
        }
        else
        {
            fOutputStream->Put(sPremadeNoHeaderPtr);
        }
		/* ��ȡ"C-Seq"ͷ,������fOutputStream */
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
		/* ��������״̬��:RTSP/1.0 200 OK */
        PutStatusLine(fOutputStream, fStatus, RTSPProtocol::k10Version);
        if (sendServerInfo)
        {
            fOutputStream->Put(QTSServerInterface::GetServerHeader());
            fOutputStream->PutEOL();
        }
		/* ����"C-Seq"ͷ */
        AppendHeader(qtssCSeqHeader, fHeaderDictionary.GetValue(qtssCSeqHeader));
    }

    //append sessionID header
	//����"Session"ͷ
    StrPtrLen* incomingID = fHeaderDictionary.GetValue(qtssSessionHeader);
    if ((incomingID != NULL) && (incomingID->Len > 0))
        AppendHeader(qtssSessionHeader, incomingID);

    //follows the HTTP/1.1 convention: if server wants to close the connection, it
    //tags the response with the Connection: close header
	/* ������ر�����,����"Connection"ͷ */
    if (!fResponseKeepAlive)
        AppendHeader(qtssConnectionHeader, &sCloseString);
}

/* used in RTSPSession::SetupRequest(),RTPStream::SendSetupResponse() */
/* ���ͱ�׼��Ӧͷ */
/*
  ��Client������Ӧ,��RTSPResponseStream�з������¸�ʽ����Ϣ:
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
/* ��ȡrtsp����ľ���·��:rtsp://172.16.34.22/,�õ�qtssRTSPReqTruncAbsoluteURLΪrtsp:// */
void* RTSPRequestInterface::GetAbsTruncatedPath(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function gets called only once
	
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqTruncAbsoluteURL, theRequest->GetValue(qtssRTSPReqAbsoluteURL));

    //Adjust the length to truncate off the last file in the path
    /* ��ȡrtsp����ľ���·��:rtsp://172.16.34.22/ */
    StrPtrLen* theAbsTruncPathParam = theRequest->GetValue(qtssRTSPReqTruncAbsoluteURL);
    theAbsTruncPathParam->Len--;
	/* �õ�rtsp:// */
    while (theAbsTruncPathParam->Ptr[theAbsTruncPathParam->Len] != kPathDelimiterChar)
        theAbsTruncPathParam->Len--;
    
    return NULL;
}

/* ��ȡ�㲥���ļ���(����·��),ɾȥĩβ���ļ���,�����ƶ�ֱ���Ƶ�"/" */
void* RTSPRequestInterface::GetTruncatedPath(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
	
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFilePathTrunc, theRequest->GetValue(qtssRTSPReqFilePath));

    //Adjust the length to truncate off the last file in the path
	/* ��ȡ�㲥���ļ���(����·��) */
    StrPtrLen* theTruncPathParam = theRequest->GetValue(qtssRTSPReqFilePathTrunc);

    if (theTruncPathParam->Len > 0)
    {
		/* ȥ���ļ�·���������ַ�"/" */
        theTruncPathParam->Len--;
		/* ɾȥĩβ���ļ���,�����ƶ�ֱ���Ƶ�"/" */
        while ( (theTruncPathParam->Len != 0) && (theTruncPathParam->Ptr[theTruncPathParam->Len] != kPathDelimiterChar) )
            theTruncPathParam->Len--;
    }

    return NULL;
}

/* ��RTSPRequestInterface�л�ȡ�㲥�ļ���,�ҳ���"/"��ĩβ���Ƕ��ַ�����Ϊ�ļ��� */
void* RTSPRequestInterface::GetFileName(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
    
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFileName, theRequest->GetValue(qtssRTSPReqFilePath));

	/* ��ȡ�㲥���ļ���(����·��) */
    StrPtrLen* theFileNameParam = theRequest->GetValue(qtssRTSPReqFileName);

    //paranoid check(������)
    if (theFileNameParam->Len == 0)
        return theFileNameParam;
        
    //walk back in the file name until we hit a /
	/* ����������ļ�·���ϵ�ÿ���ַ�,��������"/"���ж�ѭ�� */
    SInt32 x = theFileNameParam->Len - 1;
    for (; x > 0; x--)
        if (theFileNameParam->Ptr[x] == kPathDelimiterChar)
            break;
    //once we do, make the tempPtr point to the next character after the slash,
    //and adjust the length accordingly
	/* һ����������"/",�͸����ļ��� */
    if (theFileNameParam->Ptr[x] == kPathDelimiterChar )
    {
        theFileNameParam->Ptr = (&theFileNameParam->Ptr[x]) + 1;
        theFileNameParam->Len -= (x + 1);
    }
    
    return NULL;        
}

/* ��RTSPRequestInterface�л�ȡ�㲥�ļ���,�����ҳ��ļ���ĩβ���������� */
void* RTSPRequestInterface::GetFileDigit(QTSSDictionary* inRequest, UInt32* /*outLen*/)
{
    // This function always gets called
    
    RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
    theRequest->SetVal(qtssRTSPReqFileDigit, theRequest->GetValue(qtssRTSPReqFilePath));

	/* ��ȡ�㲥���ļ���(����·��) */
    StrPtrLen* theFileDigit = theRequest->GetValue(qtssRTSPReqFileDigit);
    /* ��ȡ�㲥���ļ���(����·��)�ĳ��� */
    UInt32  theFilePathLen = theRequest->GetValue(qtssRTSPReqFilePath)->Len;

	/* ��λ���ļ�����ĩβ */
    theFileDigit->Ptr += theFilePathLen - 1;
    theFileDigit->Len = 0;
	/* �����ҳ��ļ���ĩβ���������� */
    while ((StringParser::sDigitMask[(unsigned int) *(*theFileDigit).Ptr] != '\0') &&
            (theFileDigit->Len <= theFilePathLen))
    {
        theFileDigit->Ptr--;
        theFileDigit->Len++;
    }
    //termination condition means that we aren't actually on a digit right now.
    //Move pointer back onto the digit
	/* ��ָ���ƻص�һ������ */
    theFileDigit->Ptr++;
    
    return NULL;
}

/* ��ȡ��RTSP request��״̬�� */
void* RTSPRequestInterface::GetRealStatusCode(QTSSDictionary* inRequest, UInt32* outLen)
{
    // Set the fRealStatusCode variable based on the current fStatusCode.
	// This function always gets called
    RTSPRequestInterface* theReq = (RTSPRequestInterface*)inRequest;
	/* ��ȡ��RTSP request��״̬�� */
    theReq->fRealStatusCode = RTSPProtocol::GetStatusCode(theReq->fStatus);
    *outLen = sizeof(UInt32);
    return &theReq->fRealStatusCode;
}

/* ��RTSPRequestInterface�л�ȡ�㲥�ļ����͸�·����,ƴ�ϵõ�����·��qtssRTSPReqLocalPath */
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
    
	/* �õ��㲥�ļ�·������ */
	StrPtrLen* thePath = theRequest->GetValue(theID);
	StrPtrLen filePath(thePath->Ptr, thePath->Len);
	/* �õ��㲥�ļ���·�� */
	StrPtrLen* theRootDir = theRequest->GetValue(qtssRTSPReqRootDir);

	/* ƴ�ϸ�·���͵㲥�ļ�·���õ�qtssRTSPReqLocalPath  */
	if (theRootDir->Len && theRootDir->Ptr[theRootDir->Len -1] == kPathDelimiterChar
	    && thePath->Len  && thePath->Ptr[0] == kPathDelimiterChar)
	{
	    char *thePathEnd = &(filePath.Ptr[filePath.Len]);
		/* ���Ƶ�һ��"/" */
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
	
	/* ���õ㲥�ļ���qtssRTSPReqLocalPath */
	(void)theRequest->SetValue(qtssRTSPReqLocalPath, 0, theFullPath,fullPathLen , QTSSDictionary::kDontObeyReadOnly);
	
	// delete our copy of the data
	delete [] theFullPath;
	*outLen = 0;
	
	return NULL;
}
