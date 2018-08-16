
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TimeoutTask.h
Description: Implemention an normal task class, but can be scheduled for timeouts.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 




#ifndef __TIMEOUTTASK_H__
#define __TIMEOUTTASK_H__


#include "StrPtrLen.h"
#include "IdleTask.h"
#include "OSThread.h"
#include "OSQueue.h"
#include "OSMutex.h"
#include "OS.h"



#define TIMEOUT_DEBUGGING 0 //messages to help debugging timeouts

class TimeoutTaskThread : public IdleTask
{
    public:
    
        //All timeout tasks get timed out from this thread
                    TimeoutTaskThread() : IdleTask(), fMutex() {this->SetTaskName("TimeoutTask");}
        virtual     ~TimeoutTaskThread(){}

    private:
        
        //this thread runs every minute and checks for timeouts
        enum
        {
			/* used in TimeoutTaskThread::Run() */
            kIntervalSeconds = 60   //UInt32
        };

        virtual SInt64          Run();
        OSMutex                 fMutex;
        OSQueue                 fQueue;
        
        friend class TimeoutTask;
};

class TimeoutTask
{
    //TimeoutTask is not a derived object off of Task, to add flexibility as
    //to how this object can be utilitized
    
    public:
    
        //Call Initialize before using this class
        static  void Initialize();
        //Pass in the task you'd like to send timeouts to. 
        //Also pass in the timeout you'd like to use. By default, the timeout is 0 (NEVER).
        TimeoutTask(Task* inTask, SInt64 inTimeoutInMilSecs = 60);
        ~TimeoutTask();
        
        //MODIFIERS

        // Changes the timeout time, also refreshes the timeout
        void        SetTimeout(SInt64 inTimeoutInMilSecs);
        
        // Specified task will get a Task::kTimeoutEvent if this
        // function isn't called within the timeout period
        void        RefreshTimeout() { fTimeoutAtThisTime = OS::Milliseconds() + fTimeoutInMilSecs; Assert(fTimeoutAtThisTime > 0); }
        
        void        SetTask(Task* inTask) { fTask = inTask; }
    private:
    
        Task*       fTask;
		/* 这两个值的关系参见RefreshTimeout() */
		/* 超时时间(绝对时间) */
        SInt64      fTimeoutAtThisTime;
		/* 超时时间(相对时间) */
        SInt64      fTimeoutInMilSecs;
        //for putting on our global queue of timeout tasks
        OSQueueElem fQueueElem;
        
        static TimeoutTaskThread*   sThread;
        
        friend class TimeoutTaskThread;
};
#endif //__TIMEOUTTASK_H__

