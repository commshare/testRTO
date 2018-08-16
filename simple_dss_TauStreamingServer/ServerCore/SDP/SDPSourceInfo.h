
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SDPSourceInfo.h
Description: This object parsers input SDP data, and uses it to support the SourceInfo API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef __SDP_SOURCE_INFO_H__
#define __SDP_SOURCE_INFO_H__

#include "StrPtrLen.h"
#include "SourceInfo.h"
#include "StringParser.h"

class StringParser;
class StrPtrLen;

class SDPSourceInfo : public SourceInfo
{
    public:
    
        // Uses the SDP Data to build up the StreamInfo structures
        SDPSourceInfo(char* sdpData, UInt32 sdpLen) { Parse(sdpData, sdpLen); }
        SDPSourceInfo() {}
        virtual ~SDPSourceInfo();
        
        // Parses out the SDP file provided, sets up the StreamInfo structures
		/* 这个函数的作用是从已有的sdp信息中提取出基类成员fStreamArray的结构体StreamInfo的信息,并配置fStreamArray的各项信息 */
		/* THE IMPORTANT AND FUNDAMENTAL FUNCTION */
        void    Parse(char* sdpData, UInt32 sdpLen);

        // This function uses the Parsed SDP file, and strips out(剥离出) all the network information,
        // producing an SDP file that appears to be local(得到本地sdp文件).
		/* 返回缓存中存放的按指定顺序排列的sdp数据的指针,并将写入sdp数据的长度给入参newSDPLen */
        virtual char*   GetLocalSDP(UInt32* newSDPLen);

		/***************************ADDED by taoyx***********************************/
		// Get the http-linked address of key to encrypt a MP4 file from DRM-Agent
		char* GetKeyAddress(char* sdpData, UInt32 sdpLen);
		/***************************ADDED by taoyx***********************************/

        // Returns the SDP data
		/* return a pointer to StrPtrLen-type struct */
        StrPtrLen*  GetSDPData()    { return &fSDPData; }
        
        // Utility routines
        
        // Assuming the parser is currently pointing at the beginning of an dotted-
        // decimal(点分十进制) IP address, this consumes it (stopping at inStopChar), and returns
        // the IP address (host ordered) as a UInt32
		/* used in SDPSourceInfo::Parse() to obtain the Dest IP Address */
        static UInt32 GetIPAddr(StringParser* inParser, char inStopChar);
      
    private:

		/* set dauft value of Time To Live */
        enum
        {
            kDefaultTTL = 15    //UInt16
        };

		/* apply fundamental data structure StrPtrLen to describle sdp data */
		/* a sole member  */
		/* 相当于SDPContainer::fSDPBuffer */
        StrPtrLen   fSDPData;
};
#endif // __SDP_SOURCE_INFO_H__

