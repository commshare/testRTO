
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSExpirationDate.h
Description: Routine that checks to see if software is expired.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef __QTSS_EXPIRATION_DATE_H__
#define __QTSS_EXPIRATION_DATE_H__

#include "OSHeaders.h"

class QTSSExpirationDate
{
    public:
    
        //checks current time vs. hard coded time constant.
        static Bool16   WillSoftwareExpire(){return sIsExpirationEnabled;}
        static Bool16   IsSoftwareExpired();
        static void     PrintExpirationDate();
        static void     sPrintExpirationDate(char* ioExpireMessage);
        
    private:
    
        static Bool16   sIsExpirationEnabled; //是否能使用到期提示功能?
        static char*    sExpirationDate;      //设定的软件到期时间,格式必须是"5/12/2012"
        
};

#endif //__QTSS_EXPIRATION_DATE_H__
