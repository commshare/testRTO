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
#include "StringParser.h" /* 十分重要的字符串解析工具 */
#include "StringTranslator.h"/* 这个必须先看懂,used in RTSPRequest::ParseURI() */
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "base64.h"



/* 比较StringParser::sEOLWhitespaceMask[],仅比它多了一个'?' */
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
static StrPtrLen    sQuoteCommaSpace("\", ", 3);/* ",_含一位空格 */
static StrPtrLen    sStaleTrue("stale=\"true\", ", 14); /* stale="true",_含一位空格 */

//Parses the request
/* 先从client获取full RTSP Request,解析它的第一行和其他行,让Response header和Request header同步,提取Requested File path */
QTSS_Error RTSPRequest::Parse()
{
	/* 从QTSS.h获取full Request from clients */
    StringParser parser(this->GetValue(qtssRTSPReqFullRequest));
    Assert(this->GetValue(qtssRTSPReqFullRequest)->Ptr != NULL);

    //parse status line.
	/* 注意最后指针fStartGet的位置是移过了第一行的EOL */
    QTSS_Error error = ParseFirstLine(parser);

    //handle any errors that come up    
    if (error != QTSS_NoErr)
        return error;
     
	/* 注意现在指针fStartGet的位置是移到了第二行的开头 */
    error = this->ParseHeaders(parser);
    if (error != QTSS_NoErr)
        return error;
    
    //Response headers should set themselves up to reflect what's in the request headers
	/* 让Response header和Request header同步 */
    fResponseKeepAlive = fRequestKeepAlive;
    
    //Make sure that there was some path that was extracted from this request. If not, there is no way
    //we can process the request, so generate an error
	/* 确保从client's full Request中提取File path,否则记下error进log */
    if (this->GetValue(qtssRTSPReqFilePath)->Len == 0)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoURLInRequest,this->GetValue(qtssRTSPReqFullRequest));
    
    return QTSS_NoErr;
}

//returns: StatusLineTooLong, SyntaxError, BadMethod
/* 注意入参是引用.解析client's full Request的第一行,依次得到RTSP method,uri,version,例如,"DESCRIBE rtsp://tuckru.apple.com/sw.mov RTSP/1.0" */
QTSS_Error RTSPRequest::ParseFirstLine(StringParser &parser)
{   
    //first get the method
	/* 首先从client's full Request的第一行开头解析,fStartGet指针吸收所有的字母,直到遇到非字母的字符就停下,将经过的字符串赋给theParsedData,作为RTSP Request method */
    StrPtrLen theParsedData;
    parser.ConsumeWord(&theParsedData);
    this->SetVal(qtssRTSPReqMethodStr, theParsedData.Ptr, theParsedData.Len);
    
    
    //THIS WORKS UNDER THE ASSUMPTION THAT:
    //valid HTTP/1.1 headers are: GET, HEAD, POST, PUT, OPTIONS, DELETE, TRACE
	/* 将RTSP Request method字符串设置为fMethod的值 */
    fMethod = RTSPProtocol::GetMethod(theParsedData);
    if (fMethod == qtssIllegalMethod)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPMethod, &theParsedData);
    
    //no longer assume this is a space... instead, just consume whitespace
	/* 指针fStartGet遇到whitespace就前进,直到遇到非whitespace就停下 */
    parser.ConsumeWhitespace();

    //now parse the uri
	/* 从client's full Request的第一行的URL信息中依次解析并配置qtssRTSPReqAbsoluteURL,qtssHostHeader,qtssRTSPReqURI,qtssRTSPReqQueryString,qtssRTSPReqFilePath */
    QTSS_Error err = ParseURI(parser);
    if (err != QTSS_NoErr)
        return err;

    //no longer assume this is a space... instead, just consume whitespace
	/* 指针fStartGet遇到whitespace就前进,直到遇到非whitespace就停下 */
    parser.ConsumeWhitespace();

    //if there is a version, consume the version string
	/* 指针fStartGet遇到'/r/n'就停下,将经过的字符串赋给versionStr */
    StrPtrLen versionStr;
    parser.ConsumeUntil(&versionStr, StringParser::sEOLMask);
    
    //check the version
	/* 判断入参versionStr是否合法?长度必须是8个字符,如"RTSP/1.0" */
    if (versionStr.Len > 0)
        fVersion = RTSPProtocol::GetVersion(versionStr);

    //go past the end of line
	/* 正常情况下是遇到eol,并且指针fStartGet前进一格,否则报错 */
	/* 注意时刻关注指针fStartGet的位置十分重要!! */
    if (!parser.ExpectEOL())
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoRTSPVersion,&theParsedData);
    return QTSS_NoErr;
}

//returns: SyntaxError if there was an error in the uri. Or InternalServerError
/* 从client's full Request的第一行的URL信息中依次解析并配置qtssRTSPReqAbsoluteURL,qtssHostHeader,qtssRTSPReqURI,qtssRTSPReqQueryString,qtssRTSPReqFilePath */
QTSS_Error RTSPRequest::ParseURI(StringParser &parser)
{
    //read in the complete URL, set it to be the qtssAbsoluteURLParam

	/* 读进完整的URL,并将它设置为qtssRTSPReqAbsoluteURL的属性值 */
    StrPtrLen theAbsURL;

    //  RTSPRequestInterface::sPathURLStopConditions stop on ? as well as sURLStopConditions
	/* 指针fStartGet遇到sURLStopConditions就停下,将经过的字符串赋给theAbsURL */
    parser.ConsumeUntil(&theAbsURL, sURLStopConditions );

    // set qtssRTSPReqAbsoluteURL to the URL throught the path component; will be : <protocol>://<host-addr>/<path>
    this->SetVal(qtssRTSPReqAbsoluteURL, &theAbsURL);
    
	/* 下面具体解析该uri信息 */
    StringParser urlParser(&theAbsURL);
    
    //we always should have a slash(斜杠) before the uri.
    //If not, that indicates this is a full URI. Also, this could be a '*' OPTIONS request
	/* 假如是完整的URL,获取完整的host-addr,设置为qtssHostHeader的属性值 */
    if ((*theAbsURL.Ptr != '/') && (*theAbsURL.Ptr != '*'))
    {
        //if it is a full URL, store the host name off in a separate parameter
        StrPtrLen theRTSPString;
        urlParser.ConsumeLength(&theRTSPString, 7); //consume "rtsp://"
        //assign the host field here to the proper QTSS param
        StrPtrLen theHost;
		/* 获取完整的host-addr,并记作theHost */
        urlParser.ConsumeUntil(&theHost, '/');
		/* 将完整的host-addr设置为qtssHostHeader的属性值 */
        fHeaderDictionary.SetVal(qtssHostHeader, &theHost);
    }
    
    // don't allow non-aggregate operations indicated by a url/media track=id
	/* 不允许非setup的method中含有"/trackID=" */
    if (qtssSetupMethod != fMethod) // any method not a setup is not allowed to have a "/trackID=" in the url.
    {
		/* 注意theAbsURL是完整的URL,参见上面 */
        StrPtrLenDel tempCStr(theAbsURL.GetAsCString()); 
		/* locate but not output the designated string with differentiating Lower/Upper case,注意函数返回值(入参)应该为空 */
        StrPtrLen nonaggregate(tempCStr.FindString("/trackID="));
        if (nonaggregate.Len > 0) // check for non-aggregate method and return error
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
    }

    // don't allow non-aggregate operations like a setup on a playing session
	/* 不允许setup的Session状态是正在playing */
    if (qtssSetupMethod == fMethod) // if it is a setup but we are playing don't allow it
    {
		/* Session类型应是RTSPSessionInterface*,所以要强制转换 */
        RTSPSession*  theSession =  (RTSPSession*)this->GetSession();
        if (theSession != NULL && theSession->IsPlaying())
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
    }

    //
    // In case there is no URI at all... we have to fake(伪造) it.
    static char* sSlashURI = "/";
      
	/* 注意现在urlParser的指针fStartGet现在已经来到"/"跟前!!下面设置qtssRTSPReqURI的值 */
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
		/* 假如host-addr后根本没有字符,甚至连'/'也没有,比如"rtsp://tuckru.apple.com",此时就要假装有一个'/',变成"rtsp://tuckru.apple.com/" */
        this->SetVal(qtssRTSPReqURI, sSlashURI, 1);

    // parse the query string from the url if present.

    // init qtssRTSPReqQueryString dictionary to an empty string
    /* 先用empty string初始化 */
    StrPtrLen queryString;
    this->SetVal(qtssRTSPReqQueryString, queryString.Ptr, queryString.Len);
    
	/* 获取qtssRTSPReqAbsoluteURL后面可能的以'?'开头的一段字符串(以StringParser::sEOLWhitespaceMask[]结尾,即' '或'\t\r\n'),并赋给qtssRTSPReqQueryString */
    if ( parser.GetDataRemaining() > 0 )
    {   
        if ( parser.PeekFast() == '?' )
        {       
            // we've got some CGI param
            parser.ConsumeLength(&queryString, 1); // toss(扔去) '?'
            
            // consume the rest of the line..
            parser.ConsumeUntilWhitespace(&queryString);
            
            this->SetVal(qtssRTSPReqQueryString, queryString.Ptr, queryString.Len);
        }
    }
 
 
    //
    // If the is a '*', return right now because '*' is not a path
    // so the below functions don't make any sense.
	/* 假如 theAbsURL仅是'*'并设置为qtssRTSPReqFilePath的属性值,但它毫无意义 */
    if ((*theAbsURL.Ptr == '*') && (theAbsURL.Len == 1))
    {
		this->SetValue(qtssRTSPReqFilePath, 0, theAbsURL.Ptr, theAbsURL.Len, QTSSDictionary::kDontObeyReadOnly);
		
        return QTSS_NoErr;
    }
    
    //path strings are statically allocated. Therefore, if they are longer than
    //this length we won't be able to handle the request.
	/* 回头检查qtssRTSPReqURI的属性值不能超过256bytes,否则报错 */
    StrPtrLen* theURLParam = this->GetValue(qtssRTSPReqURI);
    if (theURLParam->Len > RTSPRequestInterface::kMaxFilePathSizeInBytes)
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLTooLong, theURLParam);

    //decode the URL, put the result in the separate buffer for the file path,
    //set the file path StrPtrLen to the proper value
	/* 解码上面得到的theURLParam,把它单独放在fFilePath,长度为256bytes */
    SInt32 theBytesWritten = StringTranslator::DecodeURL(theURLParam->Ptr, theURLParam->Len,
                                                fFilePath, RTSPRequestInterface::kMaxFilePathSizeInBytes);
    //if negative, an error occurred, reported as an QTSS_Error
    //we also need to leave room for a terminator.
	/* 注意存放在fFilePath中的解密后的数据一定要0< fFilePath <256bytes,要留下分隔符的空间,否则报错 */
    if ((theBytesWritten < 0) || (theBytesWritten == RTSPRequestInterface::kMaxFilePathSizeInBytes))
    {
        return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLInBadFormat, theURLParam);
    }

    // Convert from a / delimited path to a local file system path
	/* 将文件中的'/'变为Windows下的'\\' */
    StringTranslator::DecodePath(fFilePath, theBytesWritten);
    
    //setup the proper QTSS param
	/* 在fFilePath最末放上分隔符'\0',并设置为qtssRTSPReqFilePath请求文件路径的属性值 */
    fFilePath[theBytesWritten] = '\0';
    //this->SetVal(qtssRTSPReqFilePath, fFilePath, theBytesWritten);
	this->SetValue(qtssRTSPReqFilePath, 0, fFilePath, theBytesWritten, QTSSDictionary::kDontObeyReadOnly);



    return QTSS_NoErr;
}


//throws eHTTPNoMoreData and eHTTPOutOfBuffer
/* 从client's full RTSP Request的第二行开头,循环parse 所有的RTSP Request header,对应的header value,设置相应的字典属性值;
   注意某些header要特殊处理.最后用"Content-length"对应的字典属性值设置该RTSP Request body length. 注意解析的末尾应是直到
   遇到\r\n\r\n, 说明该Request headers的结束 */
QTSS_Error RTSPRequest::ParseHeaders(StringParser& parser)
{
	/* 两个非常关键的temp variables */
    StrPtrLen theKeyWord;
    Bool16 isStreamOK;
    
    //Repeat until we get a \r\n\r\n, which signals the end of the headers
    
	/* 注意现在指针fStartGet的位置是移到了client's full RTSP Request的第二行开头.
	   下面循环parse 所有的RTSP Request header,对应的header value,设置相应的字典属性值 */
    while ((parser.PeekFast() != '\r') && (parser.PeekFast() != '\n')) 
    {
        //First get the header identifier
        
		/* 指针fStartGet从第二行开头移动,获取第二行冒号':'前的字符串,赋值给theKeyWord.注意指针fStartGet现在移过':'后面 */
        isStreamOK = parser.GetThru(&theKeyWord, ':');
		/* 如果上函数得到false,就报错说RTSP Request header后没有colon冒号 */
        if (!isStreamOK)
            return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoColonAfterHeader, this->GetValue(qtssRTSPReqFullRequest));
          
		/* 去掉theKeyWord开头和结尾的' '和'/t' */
         theKeyWord.TrimWhitespace();
        
        //Look up the proper header enumeration based on the header string.
        //Use the enumeration to look up the dictionary ID of this header,
        //and set that dictionary attribute to be whatever is in the body of the header
        /* 得到QTSSRTSPProtocol.h中的RTSP Request header的Index */
        UInt32 theHeader = RTSPProtocol::GetRequestHeader(theKeyWord);

		//Second check and parse the header value, set corresponding Dictionary Attribute values

		/* 指针fStartGet现在从':'后面移动到'\r' & '\n'之前,将经过的字符串赋值给theHeaderVal(它开头可能含有WhiteSpace(' '&'/t')字符) */
        StrPtrLen theHeaderVal;
		parser.ConsumeUntil(&theHeaderVal, StringParser::sEOLMask);
	
		/* 现在分析每行末尾的字符 */
		StrPtrLen theEOL;
		/* 若指针fStartGet当前指向'\r'&'\n',则前移一位并换行,将扫过的这个字符给theEOL,该行状态是true */
		if ((parser.PeekFast() == '\r') || (parser.PeekFast() == '\n'))
		{
			isStreamOK = true;
			parser.ConsumeEOL(&theEOL);
		}
		else
			isStreamOK = false;
		
		/* 若在theEOL之后存在WhiteSpace(' '&'/t')字符,将指针fStartGet一直移到下一个'\r'&'\n'处,将扫过的长度和原theEOL.Len都累加到theHeaderVal.Len */
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
		/* 检查该行theHeader(实质是RTSP header Index/Dictionary Attribute ID)的合法性,去除theHeaderVal首末可能的WhiteSpace(' '&'/t'),并设置为对应Dictionary属性的值 */
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
		/* 对一些header做得额外的处理 */
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
	/* 取出"Content-length"对应的字典属性值,先跨过whitespace,从中取出所有的数字,设置为该RTSP Request body length */
    StrPtrLen* theContentLengthBody = fHeaderDictionary.GetValue(qtssContentLengthHeader);
    if (theContentLengthBody->Len > 0)
    {
        StringParser theHeaderParser(fHeaderDictionary.GetValue(qtssContentLengthHeader));
		/* 直到遇到non-whitespace,指针fStartGet才停下 */
        theHeaderParser.ConsumeWhitespace();
		/* 取出"Content-length"对应的属性值,先跨过whitespace,从中取出所有的数字,设置为该RTSP Request body length对应的数据成员fRequestBodyLen的值 */
        this->GetSession()->SetRequestBodyLength(theHeaderParser.ConsumeInteger(NULL));
    }
    
	/* 解析到最后行末,看是否遇到\r\n? 也就是说一共遇到\r\n\r\n, which signals the end of the headers */
    isStreamOK = parser.ExpectEOL();
    Assert(isStreamOK);
    return QTSS_NoErr;
}

/* 先读入"Session"对应的属性值,读取它的"Session"并配置给qtssSessionHeader?为啥要配置两次?? */
void RTSPRequest::ParseSessionHeader()
{
    StringParser theSessionParser(fHeaderDictionary.GetValue(qtssSessionHeader));
    StrPtrLen theSessionID;
    (void)theSessionParser.GetThru(&theSessionID, ';');
    fHeaderDictionary.SetVal(qtssSessionHeader, &theSessionID);
}

/* used in RTSPRequest::ParseTransportHeader() */
/* 解析入参inSubHeader,看它是否为"unicast"或"multiicast",并设置数据成员fNetworkMode,返回判断结果true或false */
Bool16 RTSPRequest::ParseNetworkModeSubHeader(StrPtrLen* inSubHeader)
{
    static StrPtrLen sUnicast("unicast");
    static StrPtrLen sMulticast("multiicast");
    Bool16 result = false; // true means header was found
    
    StringParser theSubHeaderParser(inSubHeader);
    
	/* 假如入参inSubHeader为"unicast",就设置fNetworkMode */
    if (!result && inSubHeader->EqualIgnoreCase(sUnicast))
    {
        fNetworkMode = qtssRTPNetworkModeUnicast;
        result = true;
    }
    
	/* 假如入参inSubHeader为"multiicast",就设置fNetworkMode */
    if (!result && inSubHeader->EqualIgnoreCase(sMulticast))
    {
        fNetworkMode = qtssRTPNetworkModeMulticast;
        result = true;
    }
        
    return result;
}

/* 获取TransportHeader的字典属性,解析出第一个含有"RTP/AVP"的transport header,在它里面逐个移到每个theTransportSubHeader,解析并配置相应的数据成员,直到解析完.
   可能配置的数据成员是:fFirstTransport,fTransportType,fNetworkMode,fClientPortA,fClientPortB,fDestinationAddr,fSourceAddr,fTtl,fTransportMode等. */
void RTSPRequest::ParseTransportHeader()
{
	static char* sRTPAVPTransportStr = "RTP/AVP";
	
    StringParser theTransParser(fHeaderDictionary.GetValue(qtssTransportHeader));

    //请参考QTSS文档中的“自动播送”部分
    //transport header from client: Transport: RTP/AVP;unicast;client_port=6974-6975;mode=record\r\n
    //                              Transport: RTP/AVP;multicast;ttl=15;destination=229.41.244.93;client_port=5000-5002\r\n
    //                              Transport: RTP/AVP/TCP;unicast;mode=record;interleaved=0-1\r\n
    
    //
    // A client may send multiple transports to the server, comma separated.
	// In this case, the server should just pick one and use that. 
	/* client可能向Server发送多个transport Header,用','分隔,但只取第一个transport header */
	
	/* 获取fFirstTransport,判断它的开头是否含有"RTP/AVP"?有就中断循环,没有fStartGet指针就移过','继续判断下一个transport header.
	   这样做的目的是获取第一个含有"RTP/AVP"的transport header */
	while (theTransParser.GetDataRemaining() > 0)
	{	
	    (void)theTransParser.ConsumeWhitespace();
        (void)theTransParser.ConsumeUntil(&fFirstTransport, ',');
    
		/* 假如fFirstTransport开头含有"RTP/AVP",就中断循环 */
		if (fFirstTransport.NumEqualIgnoreCase(sRTPAVPTransportStr, ::strlen(sRTPAVPTransportStr)))
			break;

		/* 当指针fStartGet当前指向','时 */
		if (theTransParser.PeekFast() == ',')
			/* 判断指针fStartGet当前是否指向入参','? 若是,调用AdvanceMark(),fStartGet前进一格,并处理是否增行号;否则返回false */
			theTransParser.Expect(',');
	}
	
	/* 进一步分析第一个开头含有"RTP/AVP"的transport header fFirstTransport,获取它中的theTransportSubHeader */
    StringParser theFirstTransportParser(&fFirstTransport);
    
    StrPtrLen theTransportSubHeader;
    (void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');    

	/* 解析theTransportSubHeader,包括上面列出的很多情况: */
    while (theTransportSubHeader.Len > 0)
    {
       
        // Extract the relevent information from the relevent subheader.
        // So far we care about 3 sub-headers
        
		/* 解析入参,看它是否为"unicast"或"multiicast",并设置数据成员fNetworkMode,返回判断结果true或false */
		/* 假如入参不是"unicast"或"multiicast" */
        if (!this->ParseNetworkModeSubHeader(&theTransportSubHeader))
        {
            theTransportSubHeader.TrimWhitespace();

            switch (*theTransportSubHeader.Ptr)
            {
				case 'r':	// rtp/avp/??? Is this tcp or udp?
                case 'R':   // RTP/AVP/??? Is this TCP or UDP?
                {   
					/* 另外判断theTransportSubHeader若是"RTP/AVP/TCP",设置fTransportType */
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
                    
                    //Parse the header, extract the destination address,将它设置为数据成员fDestinationAddr
					/* 提取'='前的字符串并判断它是第二个入参,并将后面的ip address字符串提取并转换为点分十进制数字,并设置为第三个入参 */
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
		/* 逐个移到下一个theTransportSubHeader,直到解析完 */
        (void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');
    }
}

/* 从RangeHeader对应的字典属性中获取形如"npt=2000-2500"的字符串中获取浮点数并配置数据成员fStartTime,fStopTime */
void  RTSPRequest::ParseRangeHeader()
{
    StringParser theRangeParser(fHeaderDictionary.GetValue(qtssRangeHeader));

    // Setup the start and stop time dictionary attributes
	/* 用RTSPRequestInterface::fStartTime/fStopTime配置相应的Dictionary Attributes */
    this->SetVal(qtssRTSPReqStartTime, &fStartTime, sizeof(fStartTime));
    this->SetVal(qtssRTSPReqStopTime, &fStopTime, sizeof(fStopTime));

	/* 指针fStartGet移过"npt=" */
    theRangeParser.GetThru(NULL, '=');//consume "npt="
	/* 指针fStartGet移过' '&'\t'直到碰到non-Whitespace字符才停下 */
    theRangeParser.ConsumeWhitespace();
    /* 指针fStartGet前移,取出连续的浮点数,并设置为fStartTime */
    fStartTime = (Float64)theRangeParser.ConsumeFloat();
    //see if there is a stop time as well.
    if (theRangeParser.GetDataRemaining() > 1)
    {
		/* 指针fStartGet移过"-",再移过' '&'\t'直到碰到non-Whitespace字符才停下 */
        theRangeParser.GetThru(NULL, '-');
        theRangeParser.ConsumeWhitespace();
		/* 指针fStartGet前移,取出连续的浮点数,并设置为fStopTime */
        fStopTime = (Float64)theRangeParser.ConsumeFloat();
    }
}

/* 获取theProtName字符串("our-retransmit")并配置foundRetransmitProt,fTransportType,fWindowSize,fWindowSizeStr */
void  RTSPRequest::ParseRetransmitHeader()
{
    StringParser theRetransmitParser(fHeaderDictionary.GetValue(qtssXRetransmitHeader));
    StrPtrLen theProtName;
    Bool16 foundRetransmitProt = false;
    
	/* 获取theProtName字符串("our-retransmit")并配置foundRetransmitProt */
    do
    {
        theRetransmitParser.ConsumeWhitespace();
        theRetransmitParser.ConsumeWord(&theProtName);
		/* 删去字符串最末的whitespace,直到non-whitespace为止 */
        theProtName.TrimTrailingWhitespace();
		/* 判断上面得到的theProtName是否为RetransmitProtocolName:"our-retransmit"? */
        foundRetransmitProt = theProtName.EqualIgnoreCase(RTSPProtocol::GetRetransmitProtocolName());
    }
	/* 当还没找到RetransmitProt，且theRetransmitParser还能找到',' */
    while ( (!foundRetransmitProt) &&
            (theRetransmitParser.GetThru(NULL, ',')) );

	/* 如果还是没找到,就返回 */
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
         
		/* 去掉首末whitespace */
        theProtArg.TrimWhitespace();
		/* 假如theProtArg现在和"window"相等 */
        if (theProtArg.EqualIgnoreCase(kWindow))
        {
			/* 跳过whitespace,提前连续数字赋给fWindowSize */
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

/* 获取ContentLengthHeader对应的字典属性值,解析出连续的数字,以十进制格式赋给数据成员fContentLength */
void  RTSPRequest::ParseContentLengthHeader()
{
    StringParser theContentLenParser(fHeaderDictionary.GetValue(qtssContentLengthHeader));
    theContentLenParser.ConsumeWhitespace();
    fContentLength = theContentLenParser.ConsumeInteger(NULL);
}

/* 获取PrebufferHeader对应的字典属性值,若形如"maxtime= 2336 :",就取出浮点数来设置数据成员fPrebufferAmt. 注意指针fStartGet已经移到':'之后 */
void  RTSPRequest::ParsePrebufferHeader()
{
    StringParser thePrebufferParser(fHeaderDictionary.GetValue(qtssXPreBufferHeader));

    StrPtrLen thePrebufferArg;
	/* 获取PrebufferHeader从开始到'='之前的字符串,赋值给thePrebufferArg */
    while (thePrebufferParser.GetThru(&thePrebufferArg, '='))   
    {
		/* 去掉thePrebufferArg首末的whitespace(' '&'\t') */
        thePrebufferArg.TrimWhitespace();

        static const StrPtrLen kMaxTimeSubHeader("maxtime");
		/* 若thePrebufferArg字符串等于"maxtime"(不计大小写) */
        if (thePrebufferArg.EqualIgnoreCase(kMaxTimeSubHeader))
        {    
			/* 去掉'='之后的whitespace(' '&'\t') */
            thePrebufferParser.ConsumeWhitespace();
			/* 取出其中的浮点数,赋值给fPrebufferAmt */
            fPrebufferAmt = thePrebufferParser.ConsumeFloat();
        } 
        
		/* 指针fStartGet一直运动,直到越过';' */
        thePrebufferParser.GetThru(NULL, ';'); //Skip past ';'
    
    }
}

/* 获取DynamicRateHeader对应的字典属性值,移过Whitespace,取出连续的数字,根据该十进制数来设置数据成员fEnableDynamicRateState的值 */
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

/* 获取IfModifiedSinceHeader对应的字典属性值,转换日期格式后,设置为数据成员fIfModSinceDate的值,再设置为qtssRTSPReqIfModSinceDate的值  */
void  RTSPRequest::ParseIfModSinceHeader()
{
    fIfModSinceDate = DateTranslator::ParseDate(fHeaderDictionary.GetValue(qtssIfModifiedSinceHeader));

    // Only set the param if this is a legal date
    if (fIfModSinceDate != 0)
        this->SetVal(qtssRTSPReqIfModSinceDate, &fIfModSinceDate, sizeof(fIfModSinceDate));
}

/* 获取SpeedHeader对应的字典属性值,移过Whitespace,取出连续的数字并以浮点数形式赋给fSpeed */
void RTSPRequest::ParseSpeedHeader()
{
    StringParser theSpeedParser(fHeaderDictionary.GetValue(qtssSpeedHeader));
    theSpeedParser.ConsumeWhitespace();
    fSpeed = theSpeedParser.ConsumeFloat();
}

/* 获取TransportOptionsHeader对应的字典属性值,若':'之前含有字串"late-tolerance",就提取'='之后的连续数字,
以实数形式赋给数据成员fLateTolerance,同时设置theRTPOptionsSubHeader为fLateToleranceStr */
void RTSPRequest::ParseTransportOptionsHeader()
{
    StringParser theRTPOptionsParser(fHeaderDictionary.GetValue(qtssXTransportOptionsHeader));
    StrPtrLen theRTPOptionsSubHeader;

    do
    {
        static StrPtrLen sLateTolerance("late-tolerance");
        
		/* 从头逐一比较指定长度的第一个入参字符串和该类对象,只要有一个字符不等就返回false,否则返回true.这说明第一个入参字符串是该类对象的子串 */
		/* 假如"late-tolerance"是theRTPOptionsSubHeader的子串,从theRTPOptionsSubHeader开始起,指针fStartGet跳过'='和whitespace后,
		   获取连续数字,以实数形式赋给数据成员fLateTolerance,同时设置theRTPOptionsSubHeader为fLateToleranceStr */
        if (theRTPOptionsSubHeader.NumEqualIgnoreCase(sLateTolerance.Ptr, sLateTolerance.Len))
        {
            StringParser theLateTolParser(&theRTPOptionsSubHeader);
            theLateTolParser.GetThru(NULL,'=');
            theLateTolParser.ConsumeWhitespace();
            fLateTolerance = theLateTolParser.ConsumeFloat();
            fLateToleranceStr = theRTPOptionsSubHeader;
        }
        
		/* 注意这个程序可以取出多个含':'的项,形如"late-tolerance: ***:",要仔细体会!! */
        (void)theRTPOptionsParser.GetThru(&theRTPOptionsSubHeader, ';');
        
    } while(theRTPOptionsSubHeader.Len > 0);
}


/* 提取'='前的字符串并判断它是第二个入参,并将后面的ip address字符串提取并转换为点分十进制数字,并设置为第三个入参 */
void RTSPRequest::ParseAddrSubHeader(StrPtrLen* inSubHeader, StrPtrLen* inHeaderName, UInt32* outAddr)
{
	/* 确保3个入参指针都存在 */
    if (!inSubHeader || !inHeaderName || !outAddr)
        return;
        
    StringParser theSubHeaderParser(inSubHeader);

    // Skip over to the value
	/* 提取'='前的字符串,去除首末whitespace */
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
    theFirstBit.TrimWhitespace();

    // First make sure this is the proper subheader
	/* 判断上面得到字符串是第二个入参 */
    if (!theFirstBit.EqualIgnoreCase(*inHeaderName))
        return;
    
    //Find the IP address
	/* 指针fStartGet前移,遇到数字才停下 */
    theSubHeaderParser.ConsumeUntilDigit();

    //Set the addr string param.
	/* 获取ip address字符串 */
    StrPtrLen theAddr(theSubHeaderParser.GetCurrentPosition(), theSubHeaderParser.GetDataRemaining());
    
    //Convert the string to a UInt32 IP address
	/* 将与theAddr相邻的下一个字符保存起来,并设置它的新值为'\0' */
    char theTerminator = theAddr.Ptr[theAddr.Len];
    theAddr.Ptr[theAddr.Len] = '\0';
    
	/* 将ip address字符串转换为点分十进制数字 */
    *outAddr = SocketUtils::ConvertStringToAddr(theAddr.Ptr);
    
	/* 将与theAddr相邻的下一个字符换回原来的值 */
    theAddr.Ptr[theAddr.Len] = theTerminator;
    
}

/* 解析入参,获取'='号前的字符串,看它是否是"mode"?若它是,跳过'='号获取字符串,若是"receive"或"record",就设置fTransportMode = qtssRTPTransportModeRecord */
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

/* 解析入参,依次判断'='前是否是"client_port"? 在'='后依次取出两个整数值,配置给成员fClientPortA和fClientPortB,再判断两端口值是否差1,若不是,记录特定的error信息进log,并更正RTCP port数值 */
void RTSPRequest::ParseClientPortSubHeader(StrPtrLen* inClientPortSubHeader)
{
    static StrPtrLen sClientPortSubHeader("client_port");
    static StrPtrLen sErrorMessage("Received invalid client_port field: ");
    StringParser theSubHeaderParser(inClientPortSubHeader);

    // Skip over to the first port
	/* 提取theFirstBit为入参开头到'='之间的字符串 */
    StrPtrLen theFirstBit;
    theSubHeaderParser.GetThru(&theFirstBit, '=');
	/* 去掉首末whitespace */
    theFirstBit.TrimWhitespace();
    
    // Make sure this is the client port subheader
	/* 必须确保该字符串是"client_port",不是就无条件返回 */
    if (!theFirstBit.EqualIgnoreCase(sClientPortSubHeader))
        return;

    // Store the two client ports as integers
	/* 提取'='后的两个端口号,依次配置fClientPortA,fClientPortB */
    theSubHeaderParser.ConsumeWhitespace();
    fClientPortA = (UInt16)theSubHeaderParser.ConsumeInteger(NULL);
    theSubHeaderParser.GetThru(NULL,'-');
    theSubHeaderParser.ConsumeWhitespace();
    fClientPortB = (UInt16)theSubHeaderParser.ConsumeInteger(NULL);
	/* 判断这两个port间的关系,必须差1,否则若出错,则记录error Log为errorPortMessage字符串"Received invalid client_port field:QTS(qtver=6.1,cpu=ppc;os=Mac 10.2.3) client_port \0",
	   并更正RTCP port数值 */
    if (fClientPortB != fClientPortA + 1) // an error in the port values
    {
        // The following to setup and log the error as a message level 2.
		/* 获取UserAgentHeader字典属性值 */
        StrPtrLen *userAgentPtr = fHeaderDictionary.GetValue(qtssUserAgentHeader);
        ResizeableStringFormatter errorPortMessage;
		/* 设置errorPortMessage为"Received invalid client_port field: " */
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

/* 首先解析出'='前的字符串是"ttl",跳过'=',提取整数赋给数据成员fTtl */
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
/* 获取XRandomDataSizeHeader对应的字典属性值,配置数据成员fRandomDataSize,调整它的大小 */
void  RTSPRequest::ParseRandomDataSizeHeader()
{
    StringParser theContentLenParser(fHeaderDictionary.GetValue(qtssXRandomDataSizeHeader));
    theContentLenParser.ConsumeWhitespace();
    fRandomDataSize = theContentLenParser.ConsumeInteger(NULL);
	
	/* 调整它的大小,不超过256kBytes */
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
    char* theFullPath = QTSSModuleUtils::GetFullPath(this, theID, &theLen, NULL);//获取请求文件的完整路径
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
