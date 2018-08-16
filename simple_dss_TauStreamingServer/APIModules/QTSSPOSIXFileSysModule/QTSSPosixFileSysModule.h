
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSPosixFileSysModule.h
Description: Provide the async file I/O mechanism.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-23

****************************************************************************/ 


#ifndef __QTSSPOSIXFILESYSMODULE_H__
#define __QTSSPOSIXFILESYSMODULE_H__

#include "QTSS.h"
#include "QTSS_Private.h"   // This module MUST be compiled directly into the server!
                            // This is because it uses the server's private internal event
                            // mechanism for doing async file I/O

extern "C"
{
    EXPORT QTSS_Error QTSSPosixFileSysModule_Main(void* inPrivateArgs);
}

#endif // __QTSSPOSIXFILESYSMODULE_H__

