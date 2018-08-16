
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
        enum{   eMaxAttributeSize   =  60 }; /* ����������󳤶� */
        struct UserAgentFields /* ����һ������ */
        {
            char                    fFieldName[eMaxAttributeSize + 1];
            UInt32                  fLen;
            UInt32                  fID;
        };

        struct UserAgentData /* ����һ�����Ե����� */
        {           
            StrPtrLen               fData;
            bool                    fFound;
        };

        enum  /* ��ʶ�������Ե�����ID */
        {   
			eQtid   = 0,
            eQtver  = 1,
            eLang   = 2,
            eOs     = 3,
            eOsver  = 4,
            eCpu    = 5,
            eNumAttributes = 6 
        };

        
        static UInt8 sEOLWhitespaceEqualMask[];/* �й�EOL,Whitespace,Equal��mask(��Щ����Ϊ1) */
        static UInt8 sEOLSemicolonCloseParenMask[];/* �й�EOL,�ֺ�,�����ŵ�mask(��Щ����Ϊ1) */ 
        static UInt8 sWhitespaceMask[];

		static UserAgentFields sFieldIDs[];/* ����User Agent ���Ե����� */
        UserAgentData fFieldData[eNumAttributes];/* ����User Agent �������ݵ����� */
            
        void Parse(StrPtrLen *inStream);

        StrPtrLen* GetUserID()          { return    &(fFieldData[eQtid].fData);     };
        StrPtrLen* GetUserVersion()     { return    &(fFieldData[eQtver].fData);    };
        StrPtrLen* GetUserLanguage()    { return    &(fFieldData[eLang].fData);     };
        StrPtrLen* GetrUserOS()         { return    &(fFieldData[eOs].fData);       };
        StrPtrLen* GetUserOSVersion()   { return    &(fFieldData[eOsver].fData);    };
        StrPtrLen* GetUserCPU()         { return    &(fFieldData[eCpu].fData);      };
        
		//ֻ�й��캯��
        UserAgentParser (StrPtrLen *inStream)  { if (inStream != NULL) Parse(inStream); }
};


#endif // _USERAGENTPARSER_H_
