
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSFlowControlModule.h
Description: A module that uses information in RTCP Ack packets to adjust the 
             speed of sending data packetes on the server side.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


#ifndef _QTSSFLOWCONTROLMODULE_H_
#define _QTSSFLOWCONTROLMODULE_H_

#include "QTSS.h"

extern "C"
{
    EXPORT QTSS_Error QTSSFlowControlModule_Main(void* inPrivateArgs);
}

#endif //_QTSSFLOWCONTROLMODULE_H_
