
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSSocket.cpp
Description: A QTSS Stream object for a generic socket.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSSocket.h"

QTSS_Error  QTSSSocket::RequestEvent(QTSS_EventType inEventMask)
{
    int theMask = 0;
    
    if (inEventMask & QTSS_ReadableEvent)
        theMask |= EV_RE;
    if (inEventMask & QTSS_WriteableEvent)
        theMask |= EV_WR;
        
    fEventContext.SetTask(this->GetTask());
    fEventContext.RequestEvent(theMask);
    return QTSS_NoErr;
}
