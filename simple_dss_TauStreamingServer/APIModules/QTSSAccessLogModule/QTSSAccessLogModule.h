
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSAccessLogModule.h
Description: Implementation of an RTP access log module.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/


#ifndef _QTSSACCESSLOGMODULE_H_
#define _QTSSACCESSLOGMODULE_H_

#include "QTSS.h"

extern "C"
{
    EXPORT QTSS_Error QTSSAccessLogModule_Main(void* inPrivateArgs);
}

#endif //_QTSSACCESSLOGMODULE_H_
