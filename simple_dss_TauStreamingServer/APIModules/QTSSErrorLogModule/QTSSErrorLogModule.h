
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSErrorLogModule.h
Description: A module that uses QTSSRollingLog to write error messages to a file.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/

#ifndef __QTSS_ERROR_LOG_MODULE_H__
#define __QTSS_ERROR_LOG_MODULE_H__

#include "QTSS.h"

QTSS_Error QTSSErrorLogModule_Main(void* inPrivateArgs);

#endif // __QTSS_ERROR_LOG_MODULE_H__
