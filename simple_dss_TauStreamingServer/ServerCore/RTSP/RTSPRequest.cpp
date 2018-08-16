/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequest.cpp
Description: This class encapsulates a single RTSP request. It stores the meta data
			 associated with a request, and provides an interface (through its base
			 class) for modules to access request information.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


#include "RTSPRequest.h"
#include "RTSPProtocol.h"
#include "RTSPSession.h"
#include "RTSPSessionInterface.h"
#include "QTSS.h"
#include "QTSSModuleUtils.h"
#include "QTSServerInterface.h"

#include "DateTranslator.h"
#include "SocketUtils.h"
#include "StringParser.h" /* ʮ����Ҫ���ַ����������� */
#include "StringTranslator.h"/* ��������ȿ���,used in RTSPRequest::ParseURI() */
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "base64.h"



/* �Ƚ�StringParser::sEOLWhitespaceMask[],����������һ��'?' */
UInt8
RTSPRequest::sURLStopConditions[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9      //'\t' is a stop condition
    1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39    //' '
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //60-69   //'?' 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
    0, 0, 0, 0, 0, 0             //250-255
};

static StrPtrLen    sDefaultRealm("Streaming Server", 16);
static StrPtrLen    sAuthBasicStr("Basic", 5);
static StrPtrLen    sAuthDigestStr("Digest", 6);
static StrPtrLen    sUsernameStr("username", 8);
static StrPtrLen    sRealmStr("realm", 5);
static StrPtrLen    sNonceStr("nonce", 5);
static StrPtrLen    sUriStr("uri", 3);
static StrPtrLen    sQopStr("qop", 3);
static StrPtrLen    sQopAuthStr("auth", 3);
static StrPtrLen    sQopAuthIntStr("auth-int", 8);
static StrPtrLen    sNonceCountStr("nc", 2);
static StrPtrLen    sResponseStr("response", 8);
static StrPtrLen    sOpaqueStr("opaque", 6);
static StrPtrLen    sEqualQuote("=\"", 2); /* =" */
static StrPtrLen    sQuoteCommaSpace("\", ", 3);/* ",_��һλ�ո� */
static StrPtrLen    sStaleTrue("stale=\"true\", ", 14); /* stale="true",_��һλ�ո� */

//Parses the request
/* �ȴ�client��ȡfull RTSP Request,�������ĵ�һ�к�������,��Response header��Request headerͬ��,��ȡRequested File path */
QTSS_Error RTSPRequest::Parse()
{
	/* ��QTSS.h��ȡfull Request from clients */
    StringParser parser(this->GetValue(qtssRTSPReqFullRequest));
    Assert(this->GetValue(qtssRTSPReqFullRequest)->Ptr != NULL);

    //parse status line.
	/* ע�����ָ��fStartGet��λ�����ƹ��˵�һ�е�EOL */
    QTSS_Error error = ParseFirstLine(parser);

    //handle any errors that come up    
    if (error != QTSS_NoErr)
        return error;
     
	/* ע������ָ��fStartGet��λ�����Ƶ��˵ڶ��еĿ�ͷ */
    error = this->ParseHeaders(parser);
    if (error != QTSS_NoErr)
        return error;
    
    //Response headers should set themselves up to reflect what's in the request headers
	/* ��Response header��Request headerͬ�� */
    fResponseKeepAlive = fRequestKeepAlive;
    
    //Make sure that there was some path that was extracted from this request. If not, there is no way
    //we can process the request, so generate an error
	/* ȷ����client's full Request����ȡFile path,�������error��log */
    if (this->GetValue(qtssRTSPReqFilePath)->Len == 0)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoURLInRequest,this->GetValue(qtssRTSPReqFullRequest));
    
    return QTSS_NoErr;
}

//returns: StatusLineTooLong, SyntaxError, BadMethod
/* ע�����������.����client's full Request�ĵ�һ��,���εõ�RTSP method,uri,version,����,"DESCRIBE rtsp://tuckru.apple.com/sw.mov RTSP/1.0" */
QTSS_Error RTSPRequest::ParseFirstLine(StringParser &parser)
{   
    //first get the method
	/* ���ȴ�client's full Request�ĵ�һ�п�ͷ����,fStartGetָ���������е���ĸ,ֱ����������ĸ���ַ���ͣ��,���������ַ�������theParsedData,��ΪRTSP Request method */
    StrPtrLen theParsedData;
    parser.ConsumeWord(&theParsedData);
    this->SetVal(qtssRTSPReqMethodStr, theParsedData.Ptr, theParsedData.Len);
    
    
    //THIS WORKS UNDER THE ASSUMPTION THAT:
    //valid HTTP/1.1 headers are: GET, HEAD, POST, PUT, OPTIONS, DELETE, TRACE
	/* ��RTSP Request method�ַ�������ΪfMethod��ֵ */
    fMethod = RTSPProtocol::GetMethod(theParsedData);
    if (fMethod == qtssIllegalMethod)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPMethod, &theParsedData);
    
    //no longer assume this is a space... instead, just consume whitespace
	/* ָ��fStartGet����whitespace��ǰ��,ֱ��������whitespace��ͣ�� */
    parser.ConsumeWhitespace();

    //now parse the uri
	/* ��client's full Request�ĵ�һ�е�URL��Ϣ�����ν���������qtssRTSPReqAbsoluteURL,qtssHostHeader,qtssRTSPReqURI,qtssRTSPReqQueryString,qtssRTSPReqFilePath */
    QTSS_Error err = ParseURI(parser);
    if (err != QTSS_NoErr)
        return err;

    //no longer assume this is a space... instead, just consume whitespace
	/* ָ��fStartGet����whitespace��ǰ��,ֱ��������whitespace��ͣ�� */
    parser.ConsumeWhitespace();

    //if there is a version, consume the version string
	/* ָ��fStartGet����'/r/n'��ͣ��,���������ַ�������versionStr */
    StrPtrLen versionStr;
    parser.ConsumeUntil(&versionStr, StringParser::sEOLMask);
    
    //check the version
	/* �ж����versionStr�Ƿ�Ϸ�?���ȱ�����8���ַ�,��"RTSP/1.0" */
    if (versionStr.Len > 0)
        fVersion = RTSPProtocol::GetVersion(versionStr);

    //go past the end of line
	/* ���������������eol,����ָ��fStartGetǰ��һ��,���򱨴� */
	/* ע��ʱ�̹�עָ��fStartGet��λ��ʮ����Ҫ!! */
    if (!parser.ExpectEOL())
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoRTSPVersion,&theParsedData);
    return QTSS_NoErr;
}

//returns: SyntaxError if there was an error in the uri. Or InternalServerError
/* ��client's full Request�ĵ�һ�е�URL��Ϣ�����ν���������qtssRTSPReqAbsoluteURL,qtssHostHeader,qtssRTSPReqURI,qtssRTSPReqQueryString,qtssRTSPReqFilePath */
QTSS_Error RTSPRequest::ParseURI(StringParser &parser)
{
    //read in the complete URL, set it to be the qtssAbsoluteURLParam

	/* ����������URL,����������ΪqtssRTSPReqAbsoluteURL������ֵ */
    StrPtrLen theAbsURL;

    //  RTSPRequestInterface::sPathURLStopConditions stop on ? as well as sURLStopConditions
	/* ָ��fStartGet����sURLStopConditions��ͣ��,���������ַ�������theAbsURL */
    parser.ConsumeUntil(&theAbsURL, sURLStopConditions );

    // set qtssRTSPReqAbsoluteURL to the URL throught the path component; will be : <protocol>://<host-addr>/<path>
    this->SetVal(qtssRTSPReqAbsoluteURL, &theAbsURL);
    
	/* ������������uri��Ϣ */
    StringParser urlParser(&theAbsURL);
    
    //we always should have a slash(б��) before the uri.
    //If not, that indicates this is a full URI. Also, this could be a '*' OPTIONS request
	/* ������������URL,��ȡ������host-addr,����ΪqtssHostHeader������ֵ */
    if ((*theAbsURL.Ptr != '/') && (*theAbsURL.Ptr != '*'))
    {
        //if it is a full URL, store the host name off in a separate parameter
        StrPtrLen theRTSPString;
        urlParser.ConsumeLength(&theRTSPString, 7); //consume "rtsp://"
        //assign the host field here to the proper QTSS param
        StrPtrLen theHost;
		/* ��ȡ������host-addr,������theHost */
        urlParser.ConsumeUntil(&theHost, '/');
		/* ��������host-addr����ΪqtssHostHeader������ֵ */
        fHeaderDictionary.SetVal(qtssHostHeader, &theHost);
    }
    
    // don't allow non-aggregate operations indicated by a url/media track=id
	/* �������setup��method�к���"/trackID=" */
    if (qtssSetupMethod != fMethod) // any method not a setup is not allowed to have a "/trackID=" in the url.
    {
		/* ע��theAbsURL��������URL,�μ����� */
        StrPtrLenDel tempCStr(theAbsURL.GetAsCString()); 
		/* locate but not output the designated string with differentiating Lower/Upper case,ע�⺯������ֵ(���)Ӧ��Ϊ�� */
        StrPtrLen nonaggregate(tempCStr.FindString("/trackID="));
        if (nonaggregate.Len > 0) // check for non-aggregate method and return error
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
    }

    // don't allow non-aggregate operations like a setup on a playing session
	/* ������setup��Session״̬������playing */
    if (qtssSetupMethod == fMethod) // if it is a setup but we are playing don't allow it
    {
		/* Session����Ӧ��RTSPSessionInterface*,����Ҫǿ��ת�� */
        RTSPSession*  theSession =  (RTSPSession*)this->GetSession();
        if (theSession != NULL && theSession->IsPlaying())
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
    }

    //
    // In case there is no URI at all... we have to fake(α��) it.
    static char* sSlashURI = "/";
      
	/* ע������urlParser��ָ��fStartGet�����Ѿ�����"/"��ǰ!!��������qtssRTSPReqURI��ֵ */
    //whatever is in this position in the URL must be the URI. Store that
    //in the qtssURLParam. Confused?
    UInt32 uriLen = urlParser.GetDataReceivedLen() - urlParser.GetDataParsedLen();
    if (uriLen > 0)
        this->SetVal(qtssRTSPReqURI, urlParser.GetCurrentPosition(), urlParser.GetDataReceivedLen() - urlParser.GetDataParsedLen());
    else
        //
        // This might happen if there is nothing after the host at all, not even
        // a '/'. This is legal (RFC 2326, Sec 3.2). If so, just pretend that there
        // is a '/'
		/* ����host-addr�����û���ַ�,������'/'Ҳû��,����"rtsp://tuckru.apple.com",��ʱ��Ҫ��װ��һ��'/',���"rtsp://tuckru.apple.com/" */
        this->SetVal(qtssRTSPReqURI, sSlashURI, 1);

    // parse the query string from the url if present.

    // init qtssRTSPReqQueryString dictionary to an empty string
    /* ����empty string��ʼ�� */
    StrPtrLen queryString;
    this->SetVal(qtssRTSPReqQueryString, queryString.Ptr, queryString.Len);
    
	/* ��ȡqtssRTSPReqAbsoluteURL������ܵ���'?'��ͷ��һ���ַ���(��StringParser::sEOLWhitespaceMask[]��β,��' '��'\t\r\n'),������qtssRTSPReqQueryString */
    if ( parser.GetDataRemaining() > 0 )
    {   
        if ( parser.PeekFast() == '?' )
        {       
            // we've got some CGI param
            parser.ConsumeLength(&queryString, 1); // toss(��ȥ) '?'
            
            // consume the rest of the line..
            parser.ConsumeUntilWhitespace(&queryString);
            
            this->SetVal(qtssRTSPReqQueryString, queryString.Ptr, queryString.Len);
        }
    }
 
 
    //
    // If the is a '*', return right now because '*' is not a path
    // so the below functions don't make any sense.
	/* ���� theAbsURL����'*'������ΪqtssRTSPReqFilePath������ֵ,������������ */
    if ((*theAbsURL.Ptr == '*') && (theAbsURL.Len == 1))
    {
		this->SetValue(qtssRTSPReqFilePath, 0, theAbsURL.Ptr, theAbsURL.Len, QTSSDictionary::kDontObeyReadOnly);
		
        return QTSS_NoErr;
    }
    
    //path strings are statically allocated. Therefore, if they are longer than
    //this length we won't be able to handle the request.
	/* ��ͷ���qtssRTSPReqURI������ֵ���ܳ���256bytes,���򱨴� */
    StrPtrLen* theURLParam = this->GetValue(qtssRTSPReqURI);
    if (theURLParam->Len > RTSPRequestInterface::kMaxFilePathSizeInBytes)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLTooLong, theURLParam);

    //decode the URL, put the result in the separate buffer for the file path,
    //set the file path StrPtrLen to the proper value
	/* ��������õ���theURLParam,������������fFilePath,����Ϊ256bytes */
    SInt32 theBytesWritten = StringTranslator::DecodeURL(theURLParam->Ptr, theURLParam->Len,
                                                fFilePath, RTSPRequestInterface::kMaxFilePathSizeInBytes);
    //if negative, an error occurred, reported as an QTSS_Error
    //we also need to leave room for a terminator.
	/* ע������fFilePath�еĽ��ܺ������һ��Ҫ0< fFilePath <256bytes,Ҫ���·ָ����Ŀռ�,���򱨴� */
    if ((theBytesWritten < 0) || (theBytesWritten == RTSPRequestInterface::kMaxFilePathSizeInBytes))
    {
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLInBadFormat, theURLParam);
    }

    // Convert from a / delimited path to a local file system path
	/* ���ļ��е�'/'��ΪWindows�µ�'\\' */
    StringTranslator::DecodePath(fFilePath, theBytesWritten);
    
    //setup the proper QTSS param
	/* ��fFilePath��ĩ���Ϸָ���'\0',������ΪqtssRTSPReqFilePath�����ļ�·��������ֵ */
    fFilePath[theBytesWritten] = '\0';
    //this->SetVal(qtssRTSPReqFilePath, fFilePath, theBytesWritten);
	this->SetValue(qtssRTSPReqFilePath, 0, fFilePath, theBytesWritten, QTSSDictionary::kDontObeyReadOnly);



    return QTSS_NoErr;
}


//throws eHTTPNoMoreData and eHTTPOutOfBuffer
/* ��client's full RTSP Request�ĵڶ��п�ͷ,ѭ��parse ���е�RTSP Request header,��Ӧ��header value,������Ӧ���ֵ�����ֵ;
   ע��ĳЩheaderҪ���⴦��.�����"Content-length"��Ӧ���ֵ�����ֵ���ø�RTSP Request body length. ע�������ĩβӦ��ֱ��
   ����\r\n\r\n, ˵����Request headers�Ľ��� */
QTSS_Error RTSPRequest::ParseHeaders(StringParser& parser)
{
	/* �����ǳ��ؼ���temp variables */
    StrPtrLen theKeyWord;
    Bool16 isStreamOK;
    
    //Repeat until we get a \r\n\r\n, which signals the end of the headers
    
	/* ע������ָ��fStartGet��λ�����Ƶ���client's full RTSP Request�ĵڶ��п�ͷ.
	   ����ѭ��parse ���е�RTSP Request header,��Ӧ��header value,������Ӧ���ֵ�����ֵ */
    while ((parser.PeekFast() != '\r') && (parser.PeekFast() != '\n')) 
    {
        //First get the header identifier
        
		/* ָ��fStartGet�ӵڶ��п�ͷ�ƶ�,��ȡ�ڶ���ð��':'ǰ���ַ���,��ֵ��theKeyWord.ע��ָ��fStartGet�����ƹ�':'���� */
        isStreamOK = parser.GetThru(&theKeyWord, ':');
		/* ����Ϻ����õ�false,�ͱ���˵RTSP Request header��û��colonð�� */
        if (!isStreamOK)
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoColonAfterHeader, this->GetValue(qtssRTSPReqFullRequest));
          
		/* ȥ��theKeyWord��ͷ�ͽ�β��' '��'/t' */
         theKeyWord.TrimWhitespace();
        
        //Look up the proper header enumeration based on the header string.
        //Use the enumeration to look up the dictionary ID of this header,
        //and set that dictionary attribute to be whatever is in the body of the header
        /* �õ�QTSSRTSPProtocol.h�е�RTSP Request header��Index */
        UInt32 theHeader = RTSPProtocol::GetRequestHeader(theKeyWord);

		//Second check and parse the header value, set corresponding Dictionary Attribute values

		/* ָ��fStartGet���ڴ�':'�����ƶ���'\r' & '\n'֮ǰ,���������ַ�����ֵ��theHeaderVal(����ͷ���ܺ���WhiteSpace(' '&'/t')�ַ�) */
        StrPtrLen theHeaderVal;
		parser.ConsumeUntil(&theHeaderVal, StringParser::sEOLMask);
	
		/* ���ڷ���ÿ��ĩβ���ַ� */
		StrPtrLen theEOL;
		/* ��ָ��fStartGet��ǰָ��'\r'&'\n',��ǰ��һλ������,��ɨ��������ַ���theEOL,����״̬��true */
		if ((parser.PeekFast() == '\r') || (parser.PeekFast() == '\n'))
		{
			isStreamOK = true;
			parser.ConsumeEOL(&theEOL);
		}
		else
			isStreamOK = false;
		
		/* ����theEOL֮�����WhiteSpace(' '&'/t')�ַ�,��ָ��fStartGetһֱ�Ƶ���һ��'\r'&'\n'��,��ɨ���ĳ��Ⱥ�ԭtheEOL.Len���ۼӵ�theHeaderVal.Len */
		while((parser.PeekFast() == ' ') || (parser.PeekFast() == '\t'))
		{
			theHeaderVal.Len += theEOL.Len;
			StrPtrLen temp;
			parser.ConsumeUntil(&temp, StringParser::sEOLMask);
			theHeaderVal.Len += temp.Len;
			
			if ((parser.PeekFast() == '\r') || (parser.PeekFast() == '\n'))
			{
				isStreamOK = true;
				parser.ConsumeEOL(&theEOL);
			}
			else
				isStreamOK = false;			
		}

        // If this is an unknown header, ignore it. Otherwise, set the proper
        // dictionary attribute
		/* ������theHeader(ʵ����RTSP header Index/Dictionary Attribute ID)�ĺϷ���,ȥ��theHeaderVal��ĩ���ܵ�WhiteSpace(' '&'/t'),������Ϊ��ӦDictionary���Ե�ֵ */
        if (theHeader != qtssIllegalHeader)
        {
            Assert(theHeader < qtssNumHeaders);
            theHeaderVal.TrimWhitespace();
            fHeaderDictionary.SetVal(theHeader, &theHeaderVal);
        }
        if (!isStreamOK)
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoEOLAfterHeader);
        
        //some headers require some special processing. If this code begins
        //to get out of control, we made need to come up with a function pointer table
		/* ��һЩheader���ö���Ĵ��� */
        switch (theHeader)
        {
            case qtssSessionHeader:             ParseSessionHeader(); break;
            case qtssTransportHeader:           ParseTransportHeader(); break;
            case qtssRangeHeader:               ParseRangeHeader();     break;
            case qtssIfModifiedSinceHeader:     ParseIfModSinceHeader();break;
            case qtssXRetransmitHeader:         ParseRetransmitHeader();break;
            case qtssContentLengthHeader:       ParseContentLengthHeader();break;
            case qtssSpeedHeader:               ParseSpeedHeader();     break;
            case qtssXTransportOptionsHeader:   ParseTransportOptionsHeader();break;
            case qtssXPreBufferHeader:          ParsePrebufferHeader();break;
			case qtssXDynamicRateHeader:		ParseDynamicRateHeader(); break;
			// DJM PROTOTYPE
			case qtssXRandomDataSizeHeader:		ParseRandomDataSizeHeader(); break;
            default:    break;
        }
    }

    // Tell the session what the request body length is for this request
    // so that it can prevent people from reading past the end of the request.
	/* ȡ��"Content-length"��Ӧ���ֵ�����ֵ,�ȿ��whitespace,����ȡ�����е�����,����Ϊ��RTSP Request body length */
    StrPtrLen* theContentLengthBody = fHeaderDictionary.GetValue(qtssContentLengthHeader);
    if (theContentLengthBody->Len > 0)
    {
        StringParser theHeaderParser(fHeaderDictionary.GetValue(qtssContentLengthHeader));
		/* ֱ������non-whitespace,ָ��fStartGet��ͣ�� */
        theHeaderParser.ConsumeWhitespace();
		/* ȡ��"Content-length"��Ӧ������ֵ,�ȿ��whitespace,����ȡ�����е�����,����Ϊ��RTSP Request body length��Ӧ�����ݳ�ԱfRequestBodyLen��ֵ */
        this->GetSession()->SetRequestBodyLength(theHeaderParser.ConsumeInteger(NULL));
    }
    
	/* �����������ĩ,���Ƿ�����\r\n? Ҳ����˵һ������\r\n\r\n, which signals the end of the headers */
    isStreamOK = parser.ExpectEOL();
    Assert(isStreamOK);
    return QTSS_NoErr;
}

/* �ȶ���"Session"��Ӧ������ֵ,��ȡ����"Session"�����ø�qtssSessionHeader?ΪɶҪ��������?? */
void RTSPRequest::ParseSessionHeader()
{
    StringParser theSessionParser(fHeaderDictionary.GetValue(qtssSessionHeader));
    StrPtrLen theSessionID;
    (void)theSessionParser.GetThru(&theSessionID, ';');
    fHeaderDictionary.SetVal(qtssSessionHeader, &theSessionID);
}

/* used in RTSPRequest::ParseTransportHeader() */
/* �������inSubHeader,�����Ƿ�Ϊ"unicast"��"multiicast",���������ݳ�ԱfNetworkMode,�����жϽ��true��false */
Bool16 RTSPRequest::ParseNetworkModeSubHeader(StrPtrLen* inSubHeader)
{
    static StrPtrLen sUnicast("unicast");
    static StrPtrLen sMulticast("multiicast");
    Bool16 result = false; // true means header was found
    
    StringParser theSubHeaderParser(inSubHeader);
    
	/* �������inSubHeaderΪ"unicast",������fNetworkMode */
    if (!result && inSubHeader->EqualIgnoreCase(sUnicast))
    {
        fNetworkMode = qtssRTPNetworkModeUnicast;
        result = true;
    }
    
	/* �������inSubHeaderΪ"multiicast",������fNetworkMode */
    if (!result && inSubHeader->EqualIgnoreCase(sMulticast))
    {
        fNetworkMode = qtssRTPNetworkModeMulticast;
        result = true;
    }
        
    return result;
}

/* ��ȡTransportHeader���ֵ�����,��������һ������"RTP/AVP"��transport header,������������Ƶ�ÿ��theTransportSubHeader,������������Ӧ�����ݳ�Ա,ֱ��������.
   �������õ����ݳ�Ա��:fFirstTransport,fTransportType,fNetworkMode,fClientPortA,fClientPortB,fDestinationAddr,fSourceAddr,fTtl,fTransportMode��. */
void RTSPRequest::ParseTransportHeader()
{
	static char* sRTPAVPTransportStr = "RTP/AVP";
	
    StringParser theTransParser(fHeaderDictionary.GetValue(qtssTransportHeader));

    //��ο�QTSS�ĵ��еġ��Զ����͡�����
    //transport header from client: Transport: RTP/AVP;unicast;client_port=6974-6975;mode=record\r\n
    //                              Transport: RTP/AVP;multicast;ttl=15;destination=229.41.244.93;client_port=5000-5002\r\n
    //                              Transport: RTP/AVP/TCP;unicast;mode=record;interleaved=0-1\r\n
    
    //
    // A client may send multiple transports to the server, comma separated.
	// In this case, the server should just pick one and use that. 
	/* client������Server���Ͷ��transport Header,��','�ָ�,��ֻȡ��һ��transport header */
	
	/* ��ȡfFirstTransport,�ж����Ŀ�ͷ�Ƿ���"RTP/AVP"?�о��ж�ѭ��,û��fStartGetָ����ƹ�','�����ж���һ��transport header.
	   ��������Ŀ���ǻ�ȡ��һ������"RTP/AVP"��transport header */
	while (theTransParser.GetDataRemaining() > 0)
	{	
	    (void)theTransParser.ConsumeWhitespace();
        (void)theTransParser.ConsumeUntil(&fFirstTransport, ',');
    
		/* ����fFirstTransport��ͷ����"RTP/AVP",���ж�ѭ�� */
		if (fFirstTransport.NumEqualIgnoreCase(sRTPAVPTransportStr, ::strlen(sRTPAVPTransportStr)))
			break;

		/* ��ָ��fStartGet��ǰָ��','ʱ */
		if (theTransParser.PeekFast() == ',')
			/* �ж�ָ��fStartGet��ǰ�Ƿ�ָ�����','? ����,����AdvanceMark(),fStartGetǰ��һ��,�������Ƿ����к�;���򷵻�false */
			theTransParser.Expect(',');
	}
	
	/* ��һ��������һ����ͷ����"RTP/AVP"��transport header fFirstTransport,��ȡ���е�theTransportSubHeader */
    StringParser theFirstTransportParser(&fFirstTransport);
    
    StrPtrLen theTransportSubHeader;
    (void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');    

	/* ����theTransportSubHeader,���������г��ĺܶ����: */
    while (theTransportSubHeader.Len > 0)
    {
       
        // Extract the relevent information from the relevent subheader.
        // So far we care about 3 sub-headers
        
		/* �������,�����Ƿ�Ϊ"unicast"��"multiicast",���������ݳ�ԱfNetworkMode,�����жϽ��true��false */
		/* ������β���"unicast"��"multiicast" */
        if (!this->ParseNetworkModeSubHeader(&theTransportSubHeader))
        {
            theTransportSubHeader.TrimWhitespace();

            switch (*theTransportSubHeader.Ptr)
            {
				case 'r':	// rtp/avp/??? Is this tcp or udp?
                case 'R':   // RTP/AVP/??? Is this TCP or UDP?
                {   
					/* �����ж�theTransportSubHeader����"RTP/AVP/TCP",����fTransportType */
                    if ( theTransportSubHeader.EqualIgnoreCase("RTP/AVP/TCP") )
                        fTransportType = qtssRTPTransportTypeTCP;
                    break;
                }
                case 'c':   //client_port sub-header
                case 'C':   //client_port sub-header
                {
                    this->ParseClientPortSubHeader(&theTransportSubHeader);
                    break;
                }
                case 'd':   //destination sub-header
                case 'D':   //destination sub-header
                {
                    static StrPtrLen sDestinationSubHeader("destination");
                    
                    //Parse the header, extract the destination address,��������Ϊ���ݳ�ԱfDestinationAddr
					/* ��ȡ'='ǰ���ַ������ж����ǵڶ������,���������ip address�ַ�����ȡ��ת��Ϊ���ʮ��������,������Ϊ��������� */
                    this->ParseAddrSubHeader(&theTransportSubHeader, &sDestinationSubHeader, &fDestinationAddr);
                    break;
                }
                case 's':   //source sub-header
                case 'S':   //source sub-header
                {
                    //Same as above code
                    static StrPtrLen sSourceSubHeader("source");
                    this->ParseAddrSubHeader(&theTransportSubHeader, &sSourceSubHeader, &fSourceAddr);
                    break;
                }
                case 't':   //time-to-live sub-header
                case 'T':   //time-to-live sub-header
                {
                    this->ParseTimeToLiveSubHeader(&theTransportSubHeader);
                    break;
                }
                case 'm':   //mode sub-header
                case 'M':   //mode sub-header
                {
                    this->ParseModeSubHeader(&theTransportSubHeader);
                    break;
                }
            }
        }
        
        // Move onto the next parameter
		/* ����Ƶ���һ��theTransportSubHeader,ֱ�������� */
        (void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');
    }
}

/* ��RangeHeader��Ӧ���ֵ������л�ȡ����"npt=2000-2500"���ַ����л�ȡ���������������ݳ�ԱfStartTime,fStopTime */
void  RTSPRequest::ParseRangeHeader()
{
    StringParser theRangeParser(fHeaderDictionary.GetValue(qtssRangeHeader));

    // Setup the start and stop time dictionary attributes
	/* ��RTSPRequestInterface::fStartTime/fStopTime������Ӧ��Dictionary Attributes */
    this->SetVal(qtssRTSPReqStartTime, &fStartTime, sizeof(fStartTime));
    this->SetVal(qtssRTSPReqStopTime, &fStopTime, sizeof(fStopTime));

	/* ָ��fStartGet�ƹ�"npt=" */
    theRangeParser.GetThru(NULL, '=');//consume "npt="
	/* ָ��fStartGet�ƹ�' '&'\t'ֱ������non-Whitespace�ַ���ͣ�� */
    theRangeParser.ConsumeWhitespace();
    /* ָ��fStartGetǰ��,ȡ�������ĸ�����,������ΪfStartTime */
    fStartTime = (Float64)theRangeParser.ConsumeFloat();
    //see if there is a stop time as well.
    if (theRangeParser.GetDataRemaining() > 1)
    {
		/* ָ��fStartGet�ƹ�"-",���ƹ�' '&'\t'ֱ������non-Whitespace�ַ���ͣ�� */
        theRangeParser.GetThru(NULL, '-');
        theRangeParser.ConsumeWhitespace();
		/* ָ��fStartGetǰ��,ȡ�������ĸ�����,������ΪfStopTime */
        fStopTime = (Float64)theRangeParser.ConsumeFloat();
    }
}

/* ��ȡtheProtName�ַ���("our-retransmit")������foundRetransmitProt,fTransportType,fWindowSize,fWindowSizeStr */
void  RTSPRequest::ParseRetransmitHeader()
{
    StringParser theRetransmitParser(fHeaderDictionary.GetValue(qtssXRetransmitHeader));
    StrPtrLen theProtName;
    Bool16 foundRetransmitProt = false;
    
	/* ��ȡtheProtName�ַ���("our-retransmit")������foundRetransmitProt */
    do
    {
        theRetransmitParser.ConsumeWhitespace();
        theRetransmitParser.ConsumeWord(&theProtName);
		/* ɾȥ�ַ�����ĩ��whitespace,ֱ��non-whitespaceΪֹ */
        theProtName.TrimTrailingWhitespace();
		/* �ж�����õ���theProtName�Ƿ�ΪRetransmitProtocolName:"our-retransmit"? */
        foundRetransmitProt = theProtName.EqualIgnoreCase(RTSPProtocol::GetRetransmitProtocolName());
    }
	/* ����û�ҵ�RetransmitProt����theRetransmitParser�����ҵ�',' */
    while ( (!foundRetransmitProt) &&
            (theRetransmitParser.GetThru(NULL, ',')) );

	/* �������û�ҵ�,�ͷ��� */
    if (!foundRetransmitProt)
        return;
    
    //
    // We are using Reliable RTP as the transport for this stream,
    // but if there was a previous transport header that indicated TCP,
    // do not set the transport to be reliable UDP
	/* What type of RTP transport is being used for the RTP stream? UDP/Reliable UDP/TCP? */
    if (fTransportType == qtssRTPTransportTypeUDP)
        fTransportType = qtssRTPTransportTypeReliableUDP;

    StrPtrLen theProtArg;
    while (theRetransmitParser.GetThru(&theProtArg, '='))   
    {
        //
        // Parse out params
        static const StrPtrLen kWindow("window");
         
		/* ȥ����ĩwhitespace */
        theProtArg.TrimWhitespace();
		/* ����theProtArg���ں�"window"��� */
        if (theProtArg.EqualIgnoreCase(kWindow))
        {
			/* ����whitespace,��ǰ�������ָ���fWindowSize */
            theRetransmitParser.ConsumeWhitespace();
            fWindowSize = theRetransmitParser.ConsumeInteger(NULL);
            
            // Save out the window size argument as a string so we
            // can easily put it into the response
            // (we never muck with this header)
            fWindowSizeStr.Ptr = theProtArg.Ptr;
            fWindowSizeStr.Len = theRetransmitParser.GetCurrentPosition() - theProtArg.Ptr;
        }
            
        theRetransmitParser.GetThru(NULL, ';'); //Skip past ';'
    }
}

/* ��ȡContentLengthHeader��Ӧ���ֵ�����ֵ,����������������,��ʮ���Ƹ�ʽ�������ݳ�ԱfContentLength */
void  RTSPRequest::ParseContentLengthHeader()
{
    StringParser theContentLenParser(fHeaderDictionary.GetValue(qtssContentLengthHeader));
    theContentLenParser.ConsumeWhitespace();
    fContentLength = theContentLenParser.ConsumeInteger(NULL);
}

/* ��ȡPrebufferHeader��Ӧ���ֵ�����ֵ,������"maxtime= 2336 :",��ȡ�����������������ݳ�ԱfPrebufferAmt. ע��ָ��fStartGet�Ѿ��Ƶ�':'֮�� */
void  RTSPRequest::ParsePrebufferHeader()
{
    StringParser thePrebufferParser(fHeaderDictionary.GetValue(qtssXPreBufferHeader));

    StrPtrLen thePrebufferArg;
	/* ��ȡPrebufferHeader�ӿ�ʼ��'='֮ǰ���ַ���,��ֵ��thePrebufferArg */
    while (thePrebufferParser.GetThru(&thePrebufferArg, '='))   
    {
		/* ȥ��thePrebufferArg��ĩ��whitespace(' '&'\t') */
        thePrebufferArg.TrimWhitespace();

        static const StrPtrLen kMaxTimeSubHeader("maxtime");
		/* ��thePrebufferArg�ַ�������"maxtime"(���ƴ�Сд) */
        if (thePrebufferArg.EqualIgnoreCase(kMaxTimeSubHeader))
        {    
			/* ȥ��'='֮���whitespace(' '&'\t') */
            thePrebufferParser.ConsumeWhitespace();
			/* ȡ�����еĸ�����,��ֵ��fPrebufferAmt */
            fPrebufferAmt = thePrebufferParser.ConsumeFloat();
        } 
        
		/* ָ��fStartGetһֱ�˶�,ֱ��Խ��';' */
        thePrebufferParser.GetThru(NULL, ';'); //Skip past ';'
    
    }
}

/* ��ȡDynamicRateHeader��Ӧ���ֵ�����ֵ,�ƹ�Whitespace,ȡ������������,���ݸ�ʮ���������������ݳ�ԱfEnableDynamicRateState��ֵ */
void  RTSPRequest::ParseDynamicRateHeader()
{
	StringParser theParser(fHeaderDictionary.GetValue(qtssXDynamicRateHeader));
    theParser.ConsumeWhitespace();
	SInt32 value = theParser.ConsumeInteger(NULL);

	// fEnableDynamicRate: < 0 undefined, 0 disable, > 0 enable
	if (value > 0)
	    fEnableDynamicRateState = 1;
	else
	    fEnableDynamicRateState = 0;
}

/* ��ȡIfModifiedSinceHeader��Ӧ���ֵ�����ֵ,ת�����ڸ�ʽ��,����Ϊ���ݳ�ԱfIfModSinceDate��ֵ,������ΪqtssRTSPReqIfModSinceDate��ֵ  */
void  RTSPRequest::ParseIfModSinceHeader()
{
    fIfModSinceDate = DateTranslator::ParseDate(fHeaderDictionary.GetValue(qtssIfModifiedSinceHeader));

    // Only set the param if this is a legal date
    if (fIfModSinceDate != 0)
        this->SetVal(qtssRTSPReqIfModSinceDate, &fIfModSinceDate, sizeof(fIfModSinceDate));
}

/* ��ȡSpeedHeader��Ӧ���ֵ�����ֵ,�ƹ�Whitespace,ȡ�����������ֲ��Ը�������ʽ����fSpeed */
void RTSPRequest::ParseSpeedHeader()
{
    StringParser theSpeedParser(fHeaderDictionary.GetValue(qtssSpeedHeader));
    theSpeedParser.ConsumeWhitespace();
    fSpeed = theSpeedParser.ConsumeFloat();
}

/* ��ȡTransportOptionsHeader��Ӧ���ֵ�����ֵ,��':'֮ǰ�����ִ�"late-tolerance",����ȡ'='֮�����������,
��ʵ����ʽ�������ݳ�ԱfLateTolerance,ͬʱ����theRTPOptionsSubHeaderΪfLateToleranceStr */
void RTSPRequest::ParseTransportOptionsHeader()
{
    StringParser theRTPOptionsParser(fHeaderDictionary.GetValue(qtssXTransportOptionsHeader));
    StrPtrLen theRTPOptionsSubHeader;

    do
    {
        static StrPtrLen sLateTolerance("late-tolerance");
        
		/* ��ͷ��һ�Ƚ�ָ�����ȵĵ�һ������ַ����͸������,ֻҪ��һ���ַ����Ⱦͷ���false,���򷵻�true.��˵����һ������ַ����Ǹ��������Ӵ� */
		/* ����"late-tolerance"��theRTPOptionsSubHeader���Ӵ�,��theRTPOptionsSubHeader��ʼ��,ָ��fStartGet����'='��whitespace��,
		   ��ȡ��������,��ʵ����ʽ�������ݳ�ԱfLateTolerance,ͬʱ����theRTPOptionsSubHeaderΪfLateToleranceStr */
        if (theRTPOptionsSubHeader.NumEqualIgnoreCase(sLateTolerance.Ptr, sLateTolerance.Len))
        {
            StringParser theLateTolParser(&theRTPOptionsSubHeader);
            theLateTolParser.GetThru(NULL,'=');
            theLateTolParser.ConsumeWhitespace();
            fLateTolerance = theLateTolParser.ConsumeFloat();
            fLateToleranceStr = theRTPOptionsSubHeader;
        }
        
		/* ע������������ȡ�������':'����,����"late-tolerance: ***:",Ҫ��ϸ���!! */
        (void)theRTPOptionsParser.GetThru(&theRTPOptionsSubHeader, ';');
        
    } while(theRTPOptionsSubHeader.Len > 0);
}


/* ��ȡ'='ǰ���ַ������ж����ǵڶ������,���������ip address�ַ�����ȡ��ת��Ϊ���ʮ��������,������Ϊ��������� */
void RTSPRequest::ParseAddrSubHeader(StrPtrLen* inSubHeader, StrPtrLen* inHeaderName, UInt32* outAddr)
{
	/* ȷ��3�����ָ�붼���� */
    if (!inSubHeader || !inHeaderName || !outAddr)
        return;
        
    StringParser theSubHeaderParser(inSubHeader);

    // Skip over to the value
	/* ��ȡ'='ǰ���ַ���,ȥ����ĩwhitespace */
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
    theFirstBit.TrimWhitespace();

    // First make sure this is the proper subheader
	/* �ж�����õ��ַ����ǵڶ������ */
    if (!theFirstBit.EqualIgnoreCase(*inHeaderName))
        return;
    
    //Find the IP address
	/* ָ��fStartGetǰ��,�������ֲ�ͣ�� */
    theSubHeaderParser.ConsumeUntilDigit();

    //Set the addr string param.
	/* ��ȡip address�ַ��� */
    StrPtrLen theAddr(theSubHeaderParser.GetCurrentPosition(), theSubHeaderParser.GetDataRemaining());
    
    //Convert the string to a UInt32 IP address
	/* ����theAddr���ڵ���һ���ַ���������,������������ֵΪ'\0' */
    char theTerminator = theAddr.Ptr[theAddr.Len];
    theAddr.Ptr[theAddr.Len] = '\0';
    
	/* ��ip address�ַ���ת��Ϊ���ʮ�������� */
    *outAddr = SocketUtils::ConvertStringToAddr(theAddr.Ptr);
    
	/* ����theAddr���ڵ���һ���ַ�����ԭ����ֵ */
    theAddr.Ptr[theAddr.Len] = theTerminator;
    
}

/* �������,��ȡ'='��ǰ���ַ���,�����Ƿ���"mode"?������,����'='�Ż�ȡ�ַ���,����"receive"��"record",������fTransportMode = qtssRTPTransportModeRecord */
void RTSPRequest::ParseModeSubHeader(StrPtrLen* inModeSubHeader)
{
    static StrPtrLen sModeSubHeader("mode");
    static StrPtrLen sReceiveMode("receive");
    static StrPtrLen sRecordMode("record");
    StringParser theSubHeaderParser(inModeSubHeader);

    // Skip over to the first port
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
    theFirstBit.TrimWhitespace();
    
    // Make sure this is the client port subheader
    if (theFirstBit.EqualIgnoreCase(sModeSubHeader)) do
    {
        theSubHeaderParser.ConsumeWhitespace();

        StrPtrLen theMode;
		theSubHeaderParser.ConsumeWord(&theMode);
		
		if ( theMode.EqualIgnoreCase(sReceiveMode) || theMode.EqualIgnoreCase(sRecordMode) )
		{	fTransportMode = qtssRTPTransportModeRecord;
			break;
		}
        
    } while (false);
    
}

/* �������,�����ж�'='ǰ�Ƿ���"client_port"? ��'='������ȡ����������ֵ,���ø���ԱfClientPortA��fClientPortB,���ж����˿�ֵ�Ƿ��1,������,��¼�ض���error��Ϣ��log,������RTCP port��ֵ */
void RTSPRequest::ParseClientPortSubHeader(StrPtrLen* inClientPortSubHeader)
{
    static StrPtrLen sClientPortSubHeader("client_port");
    static StrPtrLen sErrorMessage("Received invalid client_port field: ");
    StringParser theSubHeaderParser(inClientPortSubHeader);

    // Skip over to the first port
	/* ��ȡtheFirstBitΪ��ο�ͷ��'='֮����ַ��� */
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
	/* ȥ����ĩwhitespace */
    theFirstBit.TrimWhitespace();
    
    // Make sure this is the client port subheader
	/* ����ȷ�����ַ�����"client_port",���Ǿ����������� */
    if (!theFirstBit.EqualIgnoreCase(sClientPortSubHeader))
        return;

    // Store the two client ports as integers
	/* ��ȡ'='��������˿ں�,��������fClientPortA,fClientPortB */
    theSubHeaderParser.ConsumeWhitespace();
    fClientPortA = (UInt16)theSubHeaderParser.ConsumeInteger(NULL);
    theSubHeaderParser.GetThru(NULL,'-');
    theSubHeaderParser.ConsumeWhitespace();
    fClientPortB = (UInt16)theSubHeaderParser.ConsumeInteger(NULL);
	/* �ж�������port��Ĺ�ϵ,�����1,����������,���¼error LogΪerrorPortMessage�ַ���"Received invalid client_port field:QTS(qtver=6.1,cpu=ppc;os=Mac 10.2.3) client_port \0",
	   ������RTCP port��ֵ */
    if (fClientPortB != fClientPortA + 1) // an error in the port values
    {
        // The following to setup and log the error as a message level 2.
		/* ��ȡUserAgentHeader�ֵ�����ֵ */
        StrPtrLen *userAgentPtr = fHeaderDictionary.GetValue(qtssUserAgentHeader);
        ResizeableStringFormatter errorPortMessage;
		/* ����errorPortMessageΪ"Received invalid client_port field: " */
        errorPortMessage.Put(sErrorMessage);
        if (userAgentPtr != NULL)
            errorPortMessage.Put(*userAgentPtr);
        errorPortMessage.PutSpace();//' '
        errorPortMessage.Put(*inClientPortSubHeader);
        errorPortMessage.PutTerminator(); // \0
        QTSSModuleUtils::LogError(qtssMessageVerbosity,qtssMsgNoMessage, 0, errorPortMessage.GetBufPtr(), NULL);
        
        
        //fix the rtcp port and hope it works.
        fClientPortB = fClientPortA + 1;
    }
}

/* ���Ƚ�����'='ǰ���ַ�����"ttl",����'=',��ȡ�����������ݳ�ԱfTtl */
void RTSPRequest::ParseTimeToLiveSubHeader(StrPtrLen* inTimeToLiveSubHeader)
{
    static StrPtrLen sTimeToLiveSubHeader("ttl");

    StringParser theSubHeaderParser(inTimeToLiveSubHeader);

    // Skip over to the first part
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
    theFirstBit.TrimWhitespace();
    // Make sure this is the ttl subheader
    if (!theFirstBit.EqualIgnoreCase(sTimeToLiveSubHeader))
        return;
    
    // Parse out the time to live...
    theSubHeaderParser.ConsumeWhitespace();
    fTtl = (UInt16)theSubHeaderParser.ConsumeInteger(NULL);
}

// DJM PROTOTYPE
/* ��ȡXRandomDataSizeHeader��Ӧ���ֵ�����ֵ,�������ݳ�ԱfRandomDataSize,�������Ĵ�С */
void  RTSPRequest::ParseRandomDataSizeHeader()
{
    StringParser theContentLenParser(fHeaderDictionary.GetValue(qtssXRandomDataSizeHeader));
    theContentLenParser.ConsumeWhitespace();
    fRandomDataSize = theContentLenParser.ConsumeInteger(NULL);
	
	/* �������Ĵ�С,������256kBytes */
	if (fRandomDataSize > RTSPSessionInterface::kMaxRandomDataSize) {
		fRandomDataSize = RTSPSessionInterface::kMaxRandomDataSize;
	}
}

QTSS_Error RTSPRequest::ParseBasicHeader(StringParser *inParsedAuthLinePtr)
{
    QTSS_Error  theErr = QTSS_NoErr;
    fAuthScheme = qtssAuthBasic;

    StrPtrLen authWord;
    
    inParsedAuthLinePtr->ConsumeWhitespace();
    inParsedAuthLinePtr->ConsumeUntilWhitespace(&authWord);
    if (0 == authWord.Len ) 
        return theErr;
        
    char* encodedStr = authWord.GetAsCString();
    OSCharArrayDeleter encodedStrDeleter(encodedStr);   
    
    char *decodedAuthWord = NEW char[Base64decode_len(encodedStr) + 1];
    OSCharArrayDeleter decodedAuthWordDeleter(decodedAuthWord);

    (void) Base64decode(decodedAuthWord, encodedStr);
    
    StrPtrLen   nameAndPassword;    
    nameAndPassword.Set(decodedAuthWord, ::strlen(decodedAuthWord));
    
    StrPtrLen   name("");
    StrPtrLen   password("");
    StringParser parsedNameAndPassword(&nameAndPassword);

    parsedNameAndPassword.ConsumeUntil(&name,':');          
    parsedNameAndPassword.ConsumeLength(NULL, 1);
    parsedNameAndPassword.GetThruEOL(&password);
        

    // Set the qtssRTSPReqUserName and qtssRTSPReqUserPassword attributes in the Request object
    (void) this->SetValue(qtssRTSPReqUserName, 0,  name.Ptr , name.Len, QTSSDictionary::kDontObeyReadOnly);
    (void) this->SetValue(qtssRTSPReqUserPassword, 0,  password.Ptr , password.Len, QTSSDictionary::kDontObeyReadOnly);

    // Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
    (void) fUserProfile.SetValue(qtssUserName, 0, name.Ptr, name.Len, QTSSDictionary::kDontObeyReadOnly);
    
    return theErr;
}
    
QTSS_Error RTSPRequest::ParseDigestHeader(StringParser *inParsedAuthLinePtr)
{
    QTSS_Error  theErr = QTSS_NoErr;
    fAuthScheme = qtssAuthDigest;
    
    inParsedAuthLinePtr->ConsumeWhitespace();
    
    while(inParsedAuthLinePtr->GetDataRemaining() != 0) 
    {
        StrPtrLen fieldNameAndValue("");
        inParsedAuthLinePtr->GetThru(&fieldNameAndValue, ','); 
        StringParser parsedNameAndValue(&fieldNameAndValue);
        StrPtrLen fieldName("");
        StrPtrLen fieldValue("");
        
        //Parse name="value" pair fields in the auth line
        parsedNameAndValue.ConsumeUntil(&fieldName, '=');
        parsedNameAndValue.ConsumeLength(NULL, 1);
        parsedNameAndValue.GetThruEOL(&fieldValue);
        StringParser::UnQuote(&fieldValue);
        
        // fieldValue.Ptr below is a pointer to a part of the qtssAuthorizationHeader 
        // as GetValue returns a pointer
        // Since the header attribute remains for the entire time the request is alive
        // we don't need to make copies of the values of each field into the request
        // object, and can just keep pointers to the values
        // Thus, no need to delete memory for the following fields when the request is deleted:
        // fAuthRealm, fAuthNonce, fAuthUri, fAuthNonceCount, fAuthResponse, fAuthOpaque
        if(fieldName.Equal(sUsernameStr)) {
            // Set the qtssRTSPReqUserName attribute in the Request object
            (void) this->SetValue(qtssRTSPReqUserName, 0,  fieldValue.Ptr , fieldValue.Len, QTSSDictionary::kDontObeyReadOnly);
            // Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
            (void) fUserProfile.SetValue(qtssUserName, 0, fieldValue.Ptr, fieldValue.Len, QTSSDictionary::kDontObeyReadOnly);
        }
        else if(fieldName.Equal(sRealmStr)) {
            fAuthRealm.Set(fieldValue.Ptr, fieldValue.Len);
        }
        else if(fieldName.Equal(sNonceStr)) {
            fAuthNonce.Set(fieldValue.Ptr, fieldValue.Len);
        }
        else if(fieldName.Equal(sUriStr)) {
            fAuthUri.Set(fieldValue.Ptr, fieldValue.Len);
        }
        else if(fieldName.Equal(sQopStr)) {
            if(fieldValue.Equal(sQopAuthStr))
                fAuthQop = RTSPSessionInterface::kAuthQop;
            else if(fieldValue.Equal(sQopAuthIntStr))
                fAuthQop = RTSPSessionInterface::kAuthIntQop;
        }
        else if(fieldName.Equal(sNonceCountStr)) {
            fAuthNonceCount.Set(fieldValue.Ptr, fieldValue.Len);
        }
        else if(fieldName.Equal(sResponseStr)) {
            fAuthResponse.Set(fieldValue.Ptr, fieldValue.Len);
        }
        else if(fieldName.Equal(sOpaqueStr)) {
            fAuthOpaque.Set(fieldValue.Ptr, fieldValue.Len);
        }
                
        inParsedAuthLinePtr->ConsumeWhitespace();
    }   
    
    return theErr;
}

QTSS_Error RTSPRequest::ParseAuthHeader(void)
{
    QTSS_Error  theErr = QTSS_NoErr;
    QTSSDictionary *theRTSPHeaders = this->GetHeaderDictionary();
    StrPtrLen   *authLine = theRTSPHeaders->GetValue(qtssAuthorizationHeader);
    if ( (authLine == NULL) || (0 == authLine->Len))
        return theErr;
        
    StrPtrLen   authWord("");
    StringParser parsedAuthLine(authLine);
    parsedAuthLine.ConsumeUntilWhitespace(&authWord);
    
    if (authWord.EqualIgnoreCase(sAuthBasicStr.Ptr, sAuthBasicStr.Len)) 
        return ParseBasicHeader(&parsedAuthLine);
        
    if (authWord.EqualIgnoreCase(sAuthDigestStr.Ptr, sAuthDigestStr.Len)) 
        return ParseDigestHeader(&parsedAuthLine);

    return theErr;
}

void RTSPRequest::SetupAuthLocalPath(void)
{
     QTSS_AttributeID theID = qtssRTSPReqFilePath;

    //
    // Get the truncated path on a setup, because setups have the trackID appended
    if (qtssSetupMethod == fMethod)
        theID = qtssRTSPReqFilePathTrunc;   

    UInt32 theLen = 0;
    char* theFullPath = QTSSModuleUtils::GetFullPath(this, theID, &theLen, NULL);//��ȡ�����ļ�������·��
    this->SetValue(qtssRTSPReqLocalPath, 0, theFullPath, theLen, QTSSDictionary::kDontObeyReadOnly);
	delete [] theFullPath;
}

QTSS_Error RTSPRequest::SendDigestChallenge(UInt32 qop, StrPtrLen *nonce, StrPtrLen* opaque)
{
    QTSS_Error theErr = QTSS_NoErr;

    char challengeBuf[kAuthChallengeHeaderBufSize];
    ResizeableStringFormatter challengeFormatter(challengeBuf, kAuthChallengeHeaderBufSize);
    
    StrPtrLen realm;
    char *prefRealmPtr = NULL;
    StrPtrLen *realmPtr = this->GetValue(qtssRTSPReqURLRealm);              // Get auth realm set by the module
    if(realmPtr->Len > 0) {
        realm = *realmPtr;
    }
    else {                                                                  // If module hasn't set the realm
        QTSServerInterface* theServer = QTSServerInterface::GetServer();    // get the realm from prefs
        prefRealmPtr = theServer->GetPrefs()->GetAuthorizationRealm();      // allocates memory
        Assert(prefRealmPtr != NULL);
        if (prefRealmPtr != NULL){  
            realm.Set(prefRealmPtr, strlen(prefRealmPtr));
        }
        else {
            realm = sDefaultRealm;  
        }
    }
    
    // Creating the Challenge header
    challengeFormatter.Put(sAuthDigestStr);             // [Digest]
    challengeFormatter.PutSpace();                      // [Digest ] 
    challengeFormatter.Put(sRealmStr);                  // [Digest realm]
    challengeFormatter.Put(sEqualQuote);                // [Digest realm="]
    challengeFormatter.Put(realm);                      // [Digest realm="somerealm]
    challengeFormatter.Put(sQuoteCommaSpace);           // [Digest realm="somerealm", ]
    if(this->GetStale()) {
        challengeFormatter.Put(sStaleTrue);             // [Digest realm="somerealm", stale="true", ]
    }
    challengeFormatter.Put(sNonceStr);                  // [Digest realm="somerealm", nonce]
    challengeFormatter.Put(sEqualQuote);                // [Digest realm="somerealm", nonce="]
    challengeFormatter.Put(*nonce);                     // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0]
    challengeFormatter.PutChar('"');                    // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"]
    challengeFormatter.PutTerminator();                 // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"\0]

    StrPtrLen challengePtr(challengeFormatter.GetBufPtr(), challengeFormatter.GetBytesWritten() - 1);
    fStatus = qtssClientUnAuthorized;
    this->SetResponseKeepAlive(true);
    this->AppendHeader(qtssWWWAuthenticateHeader, &challengePtr);
    this->SendHeader();
    
    // deleting the memory that was allocated in GetPrefs call above
    if (prefRealmPtr != NULL)
    {   
        delete[] prefRealmPtr;  
    }
        
    return theErr;
}

QTSS_Error RTSPRequest::SendBasicChallenge(void)
{   
    QTSS_Error theErr = QTSS_NoErr;
    char *prefRealmPtr = NULL;
    
    do 
    {   
        char realmBuff[kRealmBuffSize] = "Basic realm=\"";
        StrPtrLen challenge(realmBuff);
        StrPtrLen whichRealm;
        
        // Get the module's realm
        StrPtrLen moduleRealm;
        theErr = this->GetValuePtr(qtssRTSPReqURLRealm, 0,  (void **) &moduleRealm.Ptr, &moduleRealm.Len);
        if ( (QTSS_NoErr == theErr) && (moduleRealm.Len > 0) )
        {
            whichRealm = moduleRealm;   
        }
        else
        {
            theErr = QTSS_NoErr;
            // Get the default realm from the config file or use the static default if config realm is not found
            QTSServerInterface* theServer = QTSServerInterface::GetServer();
            prefRealmPtr = theServer->GetPrefs()->GetAuthorizationRealm(); // allocates memory
            Assert(prefRealmPtr != NULL);
            if (prefRealmPtr != NULL)
            {   whichRealm.Set(prefRealmPtr, strlen(prefRealmPtr));
            }   
            else
            {
                whichRealm = sDefaultRealm;
            }
        }
        
        int realmLen = whichRealm.Len + challenge.Len + 2; // add 2 based on double quote char + end of string 0x00
        if (realmLen > kRealmBuffSize) // The realm is too big so use the default realm
        {   Assert(0);
            whichRealm = sDefaultRealm;
        }
        memcpy(&challenge.Ptr[challenge.Len],whichRealm.Ptr,whichRealm.Len);
        int newLen = challenge.Len + whichRealm.Len;
        
        challenge.Ptr[newLen] = '"'; // add the terminating "" this was accounted for with the size check above
        challenge.Ptr[newLen + 1] = 0;// add the 0 terminator this was accounted for with the size check above
        challenge.Len = newLen +1; // set the real size of the string excluding the 0.
        
        #if (0)
        {  // test code
            char test[256];
            
            memcpy(test,sDefaultRealm.Ptr,sDefaultRealm.Len);
            test[sDefaultRealm.Len] = 0;
            qtss_printf("the static realm =%s \n",test);
            
            OSCharArrayDeleter prefDeleter(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
            memcpy(test,prefDeleter.GetObject(),strlen(prefDeleter.GetObject()));
            test[strlen(prefDeleter.GetObject())] = 0;
            qtss_printf("the Pref realm =%s \n",test);

            memcpy(test,moduleRealm.Ptr,moduleRealm.Len);
            test[moduleRealm.Len] = 0;
            qtss_printf("the moduleRealm  =%s \n",test);
        
            memcpy(test,whichRealm.Ptr,whichRealm.Len);
            test[whichRealm.Len] = 0;
            qtss_printf("the challenge realm  =%s \n",test);
            
            memcpy(test,challenge.Ptr,challenge.Len);
            test[challenge.Len] = 0;
            qtss_printf("the challenge string  =%s len = %ld\n",test, challenge.Len);
        }
        #endif

        fStatus = qtssClientUnAuthorized;
        this->SetResponseKeepAlive(true);
        this->AppendHeader(qtssWWWAuthenticateHeader, &challenge);
        this->SendHeader();

    
    } while (false);
    
    if (prefRealmPtr != NULL)
    {   
        delete[] prefRealmPtr;  
    }
    
    return theErr;
}

QTSS_Error RTSPRequest::SendForbiddenResponse(void)
{
    fStatus = qtssClientForbidden;
    this->SetResponseKeepAlive(false);
    this->SendHeader();

    return QTSS_NoErr;
}
