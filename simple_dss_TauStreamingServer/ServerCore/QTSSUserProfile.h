
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSUserProfile.h
Description: An object to store User Profile, for authentication and authorization
             Implements the RTSP Request dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


#ifndef __QTSSUSERPROFILE_H__
#define __QTSSUSERPROFILE_H__

//INCLUDES:
#include "QTSS.h"
#include "QTSSDictionary.h"
#include "StrPtrLen.h"

class QTSSUserProfile : public QTSSDictionary
{
    public:

        //Initialize
        //Call initialize before instantiating this class. For maximum performance, this class builds
        //any response header it can at startup time.
        static void         Initialize();
        
        //CONSTRUCTOR & DESTRUCTOR
        QTSSUserProfile();
        virtual ~QTSSUserProfile() {}
        
    protected:
        
        enum
        {
            kMaxUserProfileNameLen      = 32,
            kMaxUserProfilePasswordLen  = 32
        };
        
        char    fUserNameBuf[kMaxUserProfileNameLen];       // Set by RTSPRequest object
        char    fUserPasswordBuf[kMaxUserProfilePasswordLen];// Set by authentication module through API

        //Dictionary support
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __QTSSUSERPROFILE_H__

