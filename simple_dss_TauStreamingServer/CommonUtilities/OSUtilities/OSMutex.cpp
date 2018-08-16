/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMutex.h
Description: Provide a abstraction mutex class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include "OSMutex.h"
#include "SafeStdLib.h"
#include <stdlib.h>
#include <string.h>


static pthread_mutexattr_t  *sMutexAttr=NULL;
static void MutexAttrInit();
static pthread_once_t sMutexAttrInit = PTHREAD_ONCE_INIT;
 
    

OSMutex::OSMutex()
{
    (void)pthread_once(&sMutexAttrInit, MutexAttrInit);
    (void)pthread_mutex_init(&fMutex, sMutexAttr);
    
    fHolder = 0;
    fHolderCount = 0;
}

void MutexAttrInit()
{
    sMutexAttr = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
    ::memset(sMutexAttr, 0, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(sMutexAttr);
}

OSMutex::~OSMutex()
{
    pthread_mutex_destroy(&fMutex);
}


/* 假如我们已经有了互斥锁(上次刚用的),再次抢占进入时仅重新计数并返回;否则首先进入临界区,获取当前Thread ID并使线程计数增加1 */
void        OSMutex::RecursiveLock()
{
    // We already have this mutex. Just refcount and return
	/* 假如我们已经有了互斥锁(上次刚用的),再次抢占进入时仅重新计数并返回 */
    if (OSThread::GetCurrentThreadID() == fHolder)
    {  /* increment ref count */
		/* 线程id的递归计数器加1 */
        fHolderCount++;
        return;
    }
    (void)pthread_mutex_lock(&fMutex);

	/* 刚进入时fHolder和fHolderCount一定要都为0 */
    Assert(fHolder == 0);
	/* update the flag of holder of Critical Section */
	/* 获取当前Thread ID */
    fHolder = OSThread::GetCurrentThreadID();
	/* 线程计数增加1 */
    fHolderCount++;
    Assert(fHolderCount == 1);
}

/* 假如不是当前线程,无须解锁,返回;否则确保当前线程引用计数>0,多次调用使其变为0.当线程引用计数为0时,使线程ID为0并离开临界区 */
void        OSMutex::RecursiveUnlock()
{
	/* 假如不是当前线程,无须解锁,返回 */
    if (OSThread::GetCurrentThreadID() != fHolder)
        return;
    
	/* 确保当前有线程存在,线程计数非0,多次调用使其变为0 */
    Assert(fHolderCount > 0);
    fHolderCount--;
	/* 仅当线程计数为0时,离开临界区 */
    if (fHolderCount == 0)
    {
		/* 将线程id置0,以便下次进入时再用 */
        fHolder = 0;
        pthread_mutex_unlock(&fMutex);
    }
}

/* compare RecursiveTryLock() with RecursiveLock()  */
/* 除了函数返回的是Bool值而非void外,其它和OSMutex::RecursiveLock()相同 */
Bool16      OSMutex::RecursiveTryLock()
{
    // We already have this mutex. Just refcount and return
    /* 假如我们已经有了互斥锁(上次刚用的),再次抢占进入时仅重新计数并返回true,说明我们已经获得了锁 */
    if (OSThread::GetCurrentThreadID() == fHolder)
    {
        fHolderCount++;
        return true;
    }

    int theErr = pthread_mutex_trylock(&fMutex);
    if (theErr != 0)
    {
        Assert(theErr == EBUSY);
        return false;
    }

	/* 处理和OSMutex::RecursiveLock()相同 */
    Assert(fHolder == 0);
    fHolder = OSThread::GetCurrentThreadID();
    fHolderCount++;
    Assert(fHolderCount == 1);
	/* always return true  */
    return true;
}

