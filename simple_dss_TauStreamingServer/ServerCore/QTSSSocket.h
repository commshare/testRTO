
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSSocket.h
Description: A QTSS Stream object for a generic socket.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSS_SOCKET_H__
#define __QTSS_SOCKET_H__

#include "QTSS.h"
#include "QTSSStream.h"
#include "EventContext.h"
#include "Socket.h"

class QTSSSocket : public QTSSStream
{
    public:

        QTSSSocket(int inFileDesc) : fEventContext(inFileDesc, Socket::GetEventThread()) {}
        virtual ~QTSSSocket() {}
        
        //
        // The only operation this stream supports is the requesting of events.
        virtual QTSS_Error  RequestEvent(QTSS_EventType inEventMask);
        
    private:
    
        EventContext fEventContext;
};

#endif //__QTSS_SOCKET_H__

