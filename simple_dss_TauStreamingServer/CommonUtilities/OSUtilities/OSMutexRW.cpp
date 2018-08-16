/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMutexRW.cpp
Description: Provide a common OSMutex class,but use for read/write date casees.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include "SafeStdLib.h"
#include "OSMutexRW.h"
#include "OSMutex.h"
#include "OSCond.h"

#include <stdlib.h>
#include <string.h>


#if DEBUGMUTEXRW
    int OSMutexRW::fCount = 0;
    int OSMutexRW::fMaxCount =0;
#endif
    

#if DEBUGMUTEXRW
void OSMutexRW::CountConflict(int i)            
{
    fCount += i;
    if (i == -1) qtss_printf("Num Conflicts: %d\n", fMaxCount);
    if (fCount > fMaxCount)
    fMaxCount = fCount;

}
#endif

/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,同时更新当前的活跃读个数 */
void OSMutexRW::LockRead()
{
	/* 这个对象好像没有用到 */
    OSMutexLocker locker(&fInternalLock);
#if DEBUGMUTEXRW
    if (fState != 0) 
    {   qtss_printf("LockRead(conflict) fState = %d active readers = %d, waiting writers = %d, waiting readers=%d\n",fState,  fActiveReaders, fWriteWaiters, fReadWaiters);
        CountConflict(1);  
    }
 
#endif
    
	/* 增加一个读等待计数 */
    AddReadWaiter();
	/* 假如为写活跃状态(有多个在写?),则必须等待 */
    while   (   ActiveWriter() // active writer so wait
            ||  WaitingWriters() // reader must wait for write waiters
            )
    {   
		/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,参见OSCond::Wait() */
        fReadersCond.Wait(&fInternalLock,OSMutexRW::eMaxWait);
    }
    
	/* 减少一个读等待计数 */
    RemoveReadWaiter();
	/* 增加一个活跃读计数,即使fState加1 */
    AddActiveReader(); // add 1 to active readers
	/* 设置当前的活跃读个数 */
    fActiveReaders = fState;
    
#if DEBUGMUTEXRW
//  qtss_printf("LockRead(conflict) fState = %d active readers = %d, waiting writers = %d, waiting readers=%d\n",fState,  fActiveReaders, fWriteWaiters, fReadWaiters);
#endif
}

/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,同时更新当前的活跃读个数 */
void OSMutexRW::LockWrite()
{
    OSMutexLocker locker(&fInternalLock);
	/* 增加一个写等待 */
    AddWriteWaiter();       //  1 writer queued            
#if DEBUGMUTEXRW

    if (Active()) 
    {   qtss_printf("LockWrite(conflict) state = %d active readers = %d, waiting writers = %d, waiting readers=%d\n", fState, fActiveReaders, fWriteWaiters, fReadWaiters);
        CountConflict(1);  
    }

    qtss_printf("LockWrite 'waiting' fState = %d locked active readers = %d, waiting writers = %d, waiting readers=%d\n",fState, fActiveReaders, fReadWaiters, fWriteWaiters);
#endif

	/* 当有读在进行时,必须等待 */
    while   (ActiveReaders())  // active readers
    {   
		/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,参见OSCond::Wait() */
        fWritersCond.Wait(&fInternalLock,OSMutexRW::eMaxWait);
    }

	/* 使等待写的个数减1 (实际上此时为-1)*/
    RemoveWriteWaiter(); // remove from waiting writers
	/* 设置为活跃写状态(fState=-1) */
    SetState(OSMutexRW::eActiveWriterState);    // this is the active writer  
	/* 设置当前的活跃读个数为-1 */
    fActiveReaders = fState; 
#if DEBUGMUTEXRW
//  qtss_printf("LockWrite 'locked' fState = %d locked active readers = %d, waiting writers = %d, waiting readers=%d\n",fState, fActiveReaders, fReadWaiters, fWriteWaiters);
#endif

}

/* 分情形讨论:当前为活跃写状态,将指定写句柄置于通知状态,否则通过循环使所有读指定句柄置于通知状态;当前状态为活跃读状态时,使活跃读计数减
   一,当没有读等待时,将写句柄对象置于通知状态. 最后都要更新活跃读数目fActiveReaders */
void OSMutexRW::Unlock()
{           
    OSMutexLocker locker(&fInternalLock);
#if DEBUGMUTEXRW
//  qtss_printf("Unlock active readers = %d, waiting writers = %d, waiting readers=%d\n", fActiveReaders, fReadWaiters, fWriteWaiters);

#endif

	/* 若当前为写活跃状态(fState=-1) */
    if (ActiveWriter()) 
    {   
		/* 设置当前状态为fState=0,自由状态 */
        SetState(OSMutexRW::eNoWriterState); // this was the active writer 
		/* 当有写操作等待时 */
        if (WaitingWriters()) // there are waiting writers
        {   
			/* 将指定写句柄对象置于通知状态 */
			fWritersCond.Signal();
        }
        else
        {   
			/* 通过循环使所有读指定句柄对象置于通知状态 */
			fReadersCond.Broadcast();
        }
#if DEBUGMUTEXRW
        qtss_printf("Unlock(writer) active readers = %d, waiting writers = %d, waiting readers=%d\n", fActiveReaders, fReadWaiters, fWriteWaiters);
#endif
    }
    else
    {
		/* 使读等待数减一,因为目前正在处理一个读等待,可多次调用OSMutexRW::Unlock()使fReadWaiters=fActiveReaders=0  */
        RemoveActiveReader(); // this was a reader
		/* 假如没有读等待 */
        if (!ActiveReaders()) // no active readers
        {   
			/* 设置当前为自由状态 */
			SetState(OSMutexRW::eNoWriterState); // this was the active writer now no actives threads
			/* 将指定写句柄对象置于通知状态 */
            fWritersCond.Signal();
        } 
    }
	/* 更新活跃读数目 */
    fActiveReaders = fState;

}



// Returns true on successful grab of the lock, false on failure
/* 是OSMutexRW::LockWrite()的包装,仅增加了状态的判断 */
int OSMutexRW::TryLockWrite()
{
    int    status  = EBUSY;
    OSMutexLocker locker(&fInternalLock);

	/* 当是自由的且没有等待的写操作时 */
    if ( !Active() && !WaitingWriters()) // no writers, no readers, no waiting writers
    {
		/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,同时更新当前的活跃读个数 */
        this->LockWrite();
        status = 0;
    }

    return status;
}

/* 是OSMutexRW::LockRead()的包装,仅增加了状态的判断 */
int OSMutexRW::TryLockRead()
{
    int    status  = EBUSY;
    OSMutexLocker locker(&fInternalLock);

	/* 当当前不是等待写操作和写状态时 */
    if ( !ActiveWriter() && !WaitingWriters() ) // no current writers but other readers ok
    {
		/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1,同时更新当前的活跃读个数 */
        this->LockRead(); 
        status = 0;
    }
    
    return status;
}



