/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSCond.cpp
Description: Provide a simple condition variable abstraction.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 

#include "OSCond.h"
#include "OSMutex.h" /* 条件变量会引用互斥锁 */
#include "OSThread.h" /* 线程会引用条件编程 */
#include "MyAssert.h"

#include <sys/time.h>


/* 创建事件对象,获取句柄给成员变量fCondition */
OSCond::OSCond()
{
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    int ret = pthread_cond_init(&fCondition, &cond_attr);
    Assert(ret == 0);
}

OSCond::~OSCond()
{
   pthread_cond_destroy(&fCondition);
}

void OSCond::TimedWait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs)
{
    struct timespec ts;
    struct timeval tv;
    struct timezone tz;
    int sec, usec;
    
    //These platforms do refcounting manually, and wait will release the mutex,
    // so we need to update the counts here

    inMutex->fHolderCount--;
    inMutex->fHolder = 0;

    /* 假如没有指定时间,就阻塞等待,直到有信号 */
    if (inTimeoutInMilSecs == 0)
        (void)pthread_cond_wait(&fCondition, &inMutex->fMutex);
    else
    {
		/* 否则更新时间 */
        gettimeofday(&tv, &tz);
        sec = inTimeoutInMilSecs / 1000;
        inTimeoutInMilSecs = inTimeoutInMilSecs - (sec * 1000);
        Assert(inTimeoutInMilSecs < 1000);
        usec = inTimeoutInMilSecs * 1000;
        Assert(tv.tv_usec < 1000000);
        ts.tv_sec = tv.tv_sec + sec;
        ts.tv_nsec = (tv.tv_usec + usec) * 1000;
		/* 这自然没错 */
        Assert(ts.tv_nsec < 2000000000);
		/* 使秒和微秒更分明 */
        if(ts.tv_nsec > 999999999)
        {
             ts.tv_sec++;
             ts.tv_nsec -= 1000000000;
        }
		/* 阻塞到指定的时间为止 */
        (void)pthread_cond_timedwait(&fCondition, &inMutex->fMutex, &ts);
    }


    inMutex->fHolderCount++;
    inMutex->fHolder = pthread_self();
    
}

