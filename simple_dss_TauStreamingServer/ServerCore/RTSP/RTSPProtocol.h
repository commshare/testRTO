

/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPProtocol.h
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


#ifndef __RTSPPROTOCOL_H__
#define __RTSPPROTOCOL_H__

#include "QTSSRTSPProtocol.h"
#include "StrPtrLen.h"

class RTSPProtocol
{
    public:

        //METHODS
        
        //  Method enumerated type definition in QTSS_RTSPProtocol.h
            
        //The lookup function. Very simple
		/* 得到RTSP protocol支持的方法 */
        static UInt32   GetMethod(const StrPtrLen &inMethodStr);
        
        static StrPtrLen&   GetMethodString(QTSS_RTSPMethod inMethod)
            { return sMethods[inMethod]; }
        
        //HEADERS

        //  Header enumerated type definitions in QTSS_RTSPProtocol.h
        
        //The lookup function. Very simple
		/* 得到RTSP Request header */
        static UInt32 GetRequestHeader(const StrPtrLen& inHeaderStr);
        
        //The lookup function. Very simple.
        static StrPtrLen& GetHeaderString(UInt32 inHeader)
            { return sHeaders[inHeader]; }
        
        
        //STATUS CODES

        //returns name of this error
		/* 得到RTSP Response的status code */
        static StrPtrLen&       GetStatusCodeString(QTSS_RTSPStatusCode inStat)
            { return sStatusCodeStrings[inStat]; }
        //returns error number for this error
        static SInt32           GetStatusCode(QTSS_RTSPStatusCode inStat)
            { return sStatusCodes[inStat]; }
        //returns error number as a string
        static StrPtrLen&       GetStatusCodeAsString(QTSS_RTSPStatusCode inStat)
            { return sStatusCodeAsStrings[inStat]; }
        
        // VERSIONS
        enum RTSPVersion
        {
            k10Version = 0,
            kIllegalVersion = 1
        };
        
        // NAMES OF THINGS
		/* used in RTSPRequest::ParseRetransmitHeader() */
        static StrPtrLen&       GetRetransmitProtocolName() { return sRetrProtName; }
        
        //accepts strings that look like "RTSP/1.0" etc...
        static RTSPVersion      GetVersion(StrPtrLen &versionStr);
        static StrPtrLen&       GetVersionString(RTSPVersion version)
            { return sVersionString[version]; }
        
    private:

        //for other lookups
        static StrPtrLen            sMethods[];
        static StrPtrLen            sHeaders[];
        static StrPtrLen            sStatusCodeStrings[];
		/* 将StatusCodes作为string,用""包含 */
        static StrPtrLen            sStatusCodeAsStrings[];
        static SInt32               sStatusCodes[];
		/* "RTSP/1.0" */
        static StrPtrLen            sVersionString[];
        
		/* RetransmitProtocolName:"our-retransmit" */
        static StrPtrLen            sRetrProtName;

};
#endif // __RTSPPROTOCOL_H__
