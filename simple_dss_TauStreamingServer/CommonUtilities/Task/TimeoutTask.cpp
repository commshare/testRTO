/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TimeoutTask.cpp
Description: Implemention an normal task class, but can be scheduled for timeouts.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "TimeoutTask.h"
#include "OSMemory.h"



TimeoutTaskThread*  TimeoutTask::sThread = NULL;

/* 创建并启动TimeoutTaskThread,类似IdleTask::Initialize() */
void TimeoutTask::Initialize()
{
    if (sThread == NULL)
    {
        sThread = NEW TimeoutTaskThread();
        sThread->Signal(Task::kStartEvent);
    }
    
}

/* 初始化各数据成员,并将fQueueElem加入TimeoutTaskThread的Queue队列 */
TimeoutTask::TimeoutTask(Task* inTask, SInt64 inTimeoutInMilSecs)
: fTask(inTask), fQueueElem()
{
	/* 设置fQueueElem所在的类对象指针 */
	fQueueElem.SetEnclosingObject(this);
	/* 设置两个数据成员fTimeoutInMilSecs和fTimeoutInMilSecs */
    this->SetTimeout(inTimeoutInMilSecs);
	/* 假如没有Task,就将本fTask设为当前任务 */
    if (NULL == inTask)
		fTask = (Task *) this;
	/* 注意在TimeoutTask::Initialize()已做该工作 */
    Assert(sThread != NULL); // this can happen if RunServer intializes tasks in the wrong order

    OSMutexLocker locker(&sThread->fMutex);
	/* 将fQueueElem加入TimeoutTaskThread的Queue队列 */
    sThread->fQueue.EnQueue(&fQueueElem);
}

/* 使用互斥锁,从超时任务队列中移去超时任务 */
TimeoutTask::~TimeoutTask()
{
    OSMutexLocker locker(&sThread->fMutex);
    sThread->fQueue.Remove(&fQueueElem);
}

/* 用入参配置超时的两个时间参数 */
void TimeoutTask::SetTimeout(SInt64 inTimeoutInMilSecs)
{
    fTimeoutInMilSecs = inTimeoutInMilSecs;
    if (inTimeoutInMilSecs == 0)
        fTimeoutAtThisTime = 0;
    else
        fTimeoutAtThisTime = OS::Milliseconds() + fTimeoutInMilSecs;
}

SInt64 TimeoutTaskThread::Run()
{
    //ok, check for timeouts now. Go through the whole queue
    OSMutexLocker locker(&fMutex);
    SInt64 curTime = OS::Milliseconds();
	/* 这个值的调整很重要,参见下面的循环 */
	SInt64 intervalMilli = kIntervalSeconds * 1000;//always default to 60 seconds but adjust to smallest interval > 0
	/* TimeoutTask间隔 */
	SInt64 taskInterval = intervalMilli;
	
	/* 整个Queue循环查找,直至当前元素指针为空时停止 */
    for (OSQueueIter iter(&fQueue); !iter.IsDone(); iter.Next())
    {
		/* 得到当前TimeoutTask所在的对象 */
        TimeoutTask* theTimeoutTask = (TimeoutTask*)iter.GetCurrent()->GetEnclosingObject();
        
        //if it's time to time this task out, signal it
		/* 当设置的超时时间到来时,触发Task::kTimeoutEvent */
        if ((theTimeoutTask->fTimeoutAtThisTime > 0) && (curTime >= theTimeoutTask->fTimeoutAtThisTime))
        {
#if TIMEOUT_DEBUGGING
            qtss_printf("TimeoutTask %ld timed out. Curtime = %"_64BITARG_"d, timeout time = %"_64BITARG_"d\n",(SInt32)theTimeoutTask, curTime, theTimeoutTask->fTimeoutAtThisTime);
#endif
			theTimeoutTask->fTask->Signal(Task::kTimeoutEvent);
		}
		else
		{
			/* 计算还有多长时间就开始超时? */
			taskInterval = theTimeoutTask->fTimeoutAtThisTime - curTime;
			/* 当等待超时时间taskInterval<实际的间隔时间intervalMilli,为了满足Darwin的设置,就要调整intervalMilli的值 */
			if ( (taskInterval > 0) && (theTimeoutTask->fTimeoutInMilSecs > 0) && (intervalMilli > taskInterval) )
				/* 注意此值的计算,要最后作为函数值返回的 */
				intervalMilli = taskInterval + 1000; // set timeout to 1 second past this task's timeout
#if TIMEOUT_DEBUGGING
			qtss_printf("TimeoutTask %ld not being timed out. Curtime = %"_64BITARG_"d. timeout time = %"_64BITARG_"d\n", (SInt32)theTimeoutTask, curTime, theTimeoutTask->fTimeoutAtThisTime);
#endif
		}
	}
	(void)this->GetEvents();//we must clear the event mask!
	
	OSThread::ThreadYield();
	
#if TIMEOUT_DEBUGGING
	qtss_printf ("TimeoutTaskThread::Run interval seconds= %ld\n", (SInt32) intervalMilli/1000);
#endif
    
	/* 注意这个返回值 */
    return intervalMilli;//don't delete me!
}
