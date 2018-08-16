
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPTask.h
Description: A task object that processes all incoming RTCP packets for the server, 
             and passes each one onto the task for which it belongs, by goes through 
			 all the UDPSockets in the RTPSocketPool.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#ifndef __RTCP_TASK_H__
#define __RTCP_TASK_H__

#include "Task.h"

class RTCPTask : public Task
{
    public:
        //This task handles all incoming RTCP data. It just polls, so make sure
        //to start the polling process by signalling a start event.
        RTCPTask() : Task() {this->SetTaskName("RTCPTask"); this->Signal(Task::kStartEvent); }
        virtual ~RTCPTask() {}
    
    private:
        virtual SInt64 Run();
};

#endif //__RTCP_TASK_H__
