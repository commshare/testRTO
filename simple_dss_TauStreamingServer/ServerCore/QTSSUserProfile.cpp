
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSUserProfile.cpp
Description: An object to store User Profile, for authentication and authorization
             Implements the RTSP Request dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


//INCLUDES:
#include "QTSSUserProfile.h"

QTSSAttrInfoDict::AttrInfo  QTSSUserProfile::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "qtssUserName",       NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssUserPassword",   NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
    /* 2 */ { "qtssUserGroups",     NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite}, 
    /* 3 */ { "qtssUserRealm",      NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite}
};

void  QTSSUserProfile::Initialize()
{
    //Setup all the dictionary stuff
    for (UInt32 x = 0; x < qtssUserNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kQTSSUserProfileDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                            sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);  
}

//CONSTRUCTOR / DESTRUCTOR: very simple stuff
QTSSUserProfile::QTSSUserProfile()
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kQTSSUserProfileDictIndex))
{
    this->SetEmptyVal(qtssUserName, &fUserNameBuf[0], kMaxUserProfileNameLen);
    this->SetEmptyVal(qtssUserPassword, &fUserPasswordBuf[0], kMaxUserProfilePasswordLen);
}

