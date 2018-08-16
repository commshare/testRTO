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
#include "OSMutex.h" /* �������������û����� */
#include "OSThread.h" /* �̻߳������������ */
#include "MyAssert.h"

#include <sys/time.h>


/* �����¼�����,��ȡ�������Ա����fCondition */
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

    /* ����û��ָ��ʱ��,�������ȴ�,ֱ�����ź� */
    if (inTimeoutInMilSecs == 0)
        (void)pthread_cond_wait(&fCondition, &inMutex->fMutex);
    else
    {
		/* �������ʱ�� */
        gettimeofday(&tv, &tz);
        sec = inTimeoutInMilSecs / 1000;
        inTimeoutInMilSecs = inTimeoutInMilSecs - (sec * 1000);
        Assert(inTimeoutInMilSecs < 1000);
        usec = inTimeoutInMilSecs * 1000;
        Assert(tv.tv_usec < 1000000);
        ts.tv_sec = tv.tv_sec + sec;
        ts.tv_nsec = (tv.tv_usec + usec) * 1000;
		/* ����Ȼû�� */
        Assert(ts.tv_nsec < 2000000000);
		/* ʹ���΢������� */
        if(ts.tv_nsec > 999999999)
        {
             ts.tv_sec++;
             ts.tv_nsec -= 1000000000;
        }
		/* ������ָ����ʱ��Ϊֹ */
        (void)pthread_cond_timedwait(&fCondition, &inMutex->fMutex, &ts);
    }


    inMutex->fHolderCount++;
    inMutex->fHolder = pthread_self();
    
}

