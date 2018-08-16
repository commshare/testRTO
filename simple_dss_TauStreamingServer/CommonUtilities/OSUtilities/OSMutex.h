
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


#ifndef _OSMUTEX_H_
#define _OSMUTEX_H_

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"
#include "OSThread.h" /* 这个是必须包含的,Thread和Mutex密切相关 */
#include "MyAssert.h"

class OSCond;

/* OSMutex类中真正的执行函数是RecursiveLock()/RecursiveUnlock()/RecursiveTryLock() */
class OSMutex
{
    public:

        OSMutex();
        ~OSMutex();

		/* 这两个函数在 OSMutexLocker中封装 */
        inline void Lock();
        inline void Unlock();
        
        // Returns true on successful grab of the lock, false on failure
		/* 判断是否成功获取到锁? */
        inline Bool16 TryLock();

    private:

        pthread_mutex_t fMutex;
        // These two platforms don't implement pthreads recursive mutexes, so
        // we have to do it manually
        pthread_t   fHolder;
        UInt32      fHolderCount;//进入临界区线程数

		/* 递归锁/解锁/试锁? */

		/* 假如我们已经有了互斥锁(上次刚用的),再次抢占进入时仅重新计数并返回;否则首先进入临界区,获取当前Thread ID并使线程计数增加1 */
        void        RecursiveLock();

		/* 假如不是当前线程,无须解锁,返回;否则确保当前线程引用计数>0,多次调用使其变为0.当线程引用计数为0时,使线程ID为0并离开临界区 */
        void        RecursiveUnlock();

		/* 除了函数返回的是Bool值而非void外,其它和OSMutex::RecursiveLock()相同 */
        Bool16      RecursiveTryLock();

        friend class OSCond;
};

/* 注意OSMutexLocker类是OSMutex类的封装,包括Lock()和Unlock(),是专门执行上锁/解锁的互斥锁类,在实际执行中,只使用该类即可! */
class   OSMutexLocker
{
    public:

		/* 注意这个函数非常重要,在使用Mutex的地方经常使用 */
        OSMutexLocker(OSMutex *inMutexP) : fMutex(inMutexP) { if (fMutex != NULL) fMutex->Lock(); }
        ~OSMutexLocker() {  if (fMutex != NULL) fMutex->Unlock(); }
        
        void Lock()         { if (fMutex != NULL) fMutex->Lock(); }
        void Unlock()       { if (fMutex != NULL) fMutex->Unlock(); }
        
    private:
        /* define the critical region variable */
		/* 唯一的数据成员,被多处引用到 */
        OSMutex*    fMutex;
};

//member functions definitions of class OSMutex

void OSMutex::Lock()
{
    this->RecursiveLock();
}

void OSMutex::Unlock()
{
    this->RecursiveUnlock();
}

Bool16 OSMutex::TryLock()
{
    return this->RecursiveTryLock();
}

#endif //_OSMUTEX_H_
