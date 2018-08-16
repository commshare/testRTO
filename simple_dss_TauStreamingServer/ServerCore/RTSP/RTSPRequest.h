
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequest.h
Description: This class encapsulates a single RTSP request. It stores the meta data
			 associated with a request, and provides an interface (through its base
			 class) for modules to access request information.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/

#ifndef __RTSPREQUEST_H__
#define __RTSPREQUEST_H__

#include "RTSPRequestInterface.h"
#include "RTSPRequestStream.h"
#include "RTSPResponseStream.h"
#include "RTSPSessionInterface.h"
#include "StringParser.h"
#include "QTSSRTSPProtocol.h"

//HTTPRequest class definition
class RTSPRequest : public RTSPRequestInterface
{
public:

    //CONSTRUCTOR / DESTRUCTOR
    //these do very little. Just initialize / delete some member data.
    //
    //Arguments:        session: the session this request is on (massive cyclical dependency)
    RTSPRequest(RTSPSessionInterface* session)
        : RTSPRequestInterface(session) {}
    virtual ~RTSPRequest() {}
    

	/************** NOTE: 该函数十分重要,起到提纲挈领的作用,阅读RTSPRequest.cpp由它开始  ******************/
    //Parses the request. Returns an error handler if there was an error encountered
    //in parsing.
    QTSS_Error Parse();
	/**********************************************************************/
    
    QTSS_Error ParseAuthHeader(void);
    // called by ParseAuthHeader
    QTSS_Error ParseBasicHeader(StringParser *inParsedAuthLinePtr);
    
    // called by ParseAuthHeader
    QTSS_Error ParseDigestHeader(StringParser *inParsedAuthLinePtr);

    void SetupAuthLocalPath(void);
    QTSS_Error SendBasicChallenge(void);
    QTSS_Error SendDigestChallenge(UInt32 qop, StrPtrLen *nonce, StrPtrLen* opaque);
    QTSS_Error SendForbiddenResponse(void);
private:

    //PARSING
    enum { kRealmBuffSize = 512, kAuthNameAndPasswordBuffSize = 128, kAuthChallengeHeaderBufSize = 512};
    
    //Parsing the URI line (first line of RTSP request)
    QTSS_Error ParseFirstLine(StringParser &parser);
    
    //Utility functions called by above
    QTSS_Error ParseURI(StringParser &parser);

    //Parsing the rest of the headers
    //This assumes that the parser is at the beginning of the headers. It will parse
    //the headers, fill out the data & HTTPParameters object.
    //
    //Returns:      A handler object signifying that a fatal syntax error has occurred
    QTSS_Error ParseHeaders(StringParser& parser);


    //Functions to parse the contents of particuarly complicated headers (as a convienence for modules)
    void    ParseRangeHeader();
    void    ParseTransportHeader();
    void    ParseIfModSinceHeader();
    void    ParseAddrSubHeader(StrPtrLen* inSubHeader, StrPtrLen* inHeaderName, UInt32* outAddr);
    void    ParseRetransmitHeader();
    void    ParseContentLengthHeader();
    void    ParseSpeedHeader();
    void    ParsePrebufferHeader();
    void    ParseTransportOptionsHeader();
    void    ParseSessionHeader();
    void    ParseClientPortSubHeader(StrPtrLen* inClientPortSubHeader);
    void    ParseTimeToLiveSubHeader(StrPtrLen* inTimeToLiveSubHeader);
    void    ParseModeSubHeader(StrPtrLen* inModeSubHeader);
    Bool16  ParseNetworkModeSubHeader(StrPtrLen* inSubHeader);
	void 	ParseDynamicRateHeader();
	// DJM PROTOTYPE
	void	ParseRandomDataSizeHeader();

	/* 唯一的数据成员,定义了uri stop signature,see RTSPRequest.cpp */
    static UInt8    sURLStopConditions[];
};
#endif // __RTSPREQUEST_H__

