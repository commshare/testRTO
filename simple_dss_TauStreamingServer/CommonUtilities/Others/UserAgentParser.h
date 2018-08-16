
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UserAgentParser.h
Description: Provide some API interface for parsing the user agent field received from RTSP clients.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _USERAGENTPARSER_H_
#define _USERAGENTPARSER_H_


#include "StringParser.h"
#include "StringFormatter.h"
#include "StrPtrLen.h"



class UserAgentParser 
{
    public:
        enum{   eMaxAttributeSize   =  60 }; /* 属性名的最大长度 */
        struct UserAgentFields /* 描述一个属性 */
        {
            char                    fFieldName[eMaxAttributeSize + 1];
            UInt32                  fLen;
            UInt32                  fID;
        };

        struct UserAgentData /* 描述一个属性的数据 */
        {           
            StrPtrLen               fData;
            bool                    fFound;
        };

        enum  /* 标识具体属性的属性ID */
        {   
			eQtid   = 0,
            eQtver  = 1,
            eLang   = 2,
            eOs     = 3,
            eOsver  = 4,
            eCpu    = 5,
            eNumAttributes = 6 
        };

        
        static UInt8 sEOLWhitespaceEqualMask[];/* 有关EOL,Whitespace,Equal的mask(这些符号为1) */
        static UInt8 sEOLSemicolonCloseParenMask[];/* 有关EOL,分号,右括号的mask(这些符号为1) */ 
        static UInt8 sWhitespaceMask[];

		static UserAgentFields sFieldIDs[];/* 描述User Agent 属性的数组 */
        UserAgentData fFieldData[eNumAttributes];/* 描述User Agent 属性数据的数组 */
            
        void Parse(StrPtrLen *inStream);

        StrPtrLen* GetUserID()          { return    &(fFieldData[eQtid].fData);     };
        StrPtrLen* GetUserVersion()     { return    &(fFieldData[eQtver].fData);    };
        StrPtrLen* GetUserLanguage()    { return    &(fFieldData[eLang].fData);     };
        StrPtrLen* GetrUserOS()         { return    &(fFieldData[eOs].fData);       };
        StrPtrLen* GetUserOSVersion()   { return    &(fFieldData[eOsver].fData);    };
        StrPtrLen* GetUserCPU()         { return    &(fFieldData[eCpu].fData);      };
        
		//只有构造函数
        UserAgentParser (StrPtrLen *inStream)  { if (inStream != NULL) Parse(inStream); }
};


#endif // _USERAGENTPARSER_H_
