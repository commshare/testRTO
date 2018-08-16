/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSFileModule.h
Description: Content source module that uses the QTFileLib to packetizer a RTP 
             packet and send to clients, also check the sdp file info.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/



#ifndef _RTPFILEMODULE_H_
#define _RTPFILEMODULE_H_

#include "QTSS.h"

extern "C"
{
    EXPORT QTSS_Error QTSSFileModule_Main(void* inPrivateArgs);
}

#endif //_RTPFILEMODULE_H_
