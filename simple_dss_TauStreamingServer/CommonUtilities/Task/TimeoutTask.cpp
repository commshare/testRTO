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

/* ����������TimeoutTaskThread,����IdleTask::Initialize() */
void TimeoutTask::Initialize()
{
    if (sThread == NULL)
    {
        sThread = NEW TimeoutTaskThread();
        sThread->Signal(Task::kStartEvent);
    }
    
}

/* ��ʼ�������ݳ�Ա,����fQueueElem����TimeoutTaskThread��Queue���� */
TimeoutTask::TimeoutTask(Task* inTask, SInt64 inTimeoutInMilSecs)
: fTask(inTask), fQueueElem()
{
	/* ����fQueueElem���ڵ������ָ�� */
	fQueueElem.SetEnclosingObject(this);
	/* �����������ݳ�ԱfTimeoutInMilSecs��fTimeoutInMilSecs */
    this->SetTimeout(inTimeoutInMilSecs);
	/* ����û��Task,�ͽ���fTask��Ϊ��ǰ���� */
    if (NULL == inTask)
		fTask = (Task *) this;
	/* ע����TimeoutTask::Initialize()�����ù��� */
    Assert(sThread != NULL); // this can happen if RunServer intializes tasks in the wrong order

    OSMutexLocker locker(&sThread->fMutex);
	/* ��fQueueElem����TimeoutTaskThread��Queue���� */
    sThread->fQueue.EnQueue(&fQueueElem);
}

/* ʹ�û�����,�ӳ�ʱ�����������ȥ��ʱ���� */
TimeoutTask::~TimeoutTask()
{
    OSMutexLocker locker(&sThread->fMutex);
    sThread->fQueue.Remove(&fQueueElem);
}

/* ��������ó�ʱ������ʱ����� */
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
	/* ���ֵ�ĵ�������Ҫ,�μ������ѭ�� */
	SInt64 intervalMilli = kIntervalSeconds * 1000;//always default to 60 seconds but adjust to smallest interval > 0
	/* TimeoutTask��� */
	SInt64 taskInterval = intervalMilli;
	
	/* ����Queueѭ������,ֱ����ǰԪ��ָ��Ϊ��ʱֹͣ */
    for (OSQueueIter iter(&fQueue); !iter.IsDone(); iter.Next())
    {
		/* �õ���ǰTimeoutTask���ڵĶ��� */
        TimeoutTask* theTimeoutTask = (TimeoutTask*)iter.GetCurrent()->GetEnclosingObject();
        
        //if it's time to time this task out, signal it
		/* �����õĳ�ʱʱ�䵽��ʱ,����Task::kTimeoutEvent */
        if ((theTimeoutTask->fTimeoutAtThisTime > 0) && (curTime >= theTimeoutTask->fTimeoutAtThisTime))
        {
#if TIMEOUT_DEBUGGING
            qtss_printf("TimeoutTask %ld timed out. Curtime = %"_64BITARG_"d, timeout time = %"_64BITARG_"d\n",(SInt32)theTimeoutTask, curTime, theTimeoutTask->fTimeoutAtThisTime);
#endif
			theTimeoutTask->fTask->Signal(Task::kTimeoutEvent);
		}
		else
		{
			/* ���㻹�ж೤ʱ��Ϳ�ʼ��ʱ? */
			taskInterval = theTimeoutTask->fTimeoutAtThisTime - curTime;
			/* ���ȴ���ʱʱ��taskInterval<ʵ�ʵļ��ʱ��intervalMilli,Ϊ������Darwin������,��Ҫ����intervalMilli��ֵ */
			if ( (taskInterval > 0) && (theTimeoutTask->fTimeoutInMilSecs > 0) && (intervalMilli > taskInterval) )
				/* ע���ֵ�ļ���,Ҫ�����Ϊ����ֵ���ص� */
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
    
	/* ע���������ֵ */
    return intervalMilli;//don't delete me!
}
