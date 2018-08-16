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

/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,ͬʱ���µ�ǰ�Ļ�Ծ������ */
void OSMutexRW::LockRead()
{
	/* ����������û���õ� */
    OSMutexLocker locker(&fInternalLock);
#if DEBUGMUTEXRW
    if (fState != 0) 
    {   qtss_printf("LockRead(conflict) fState = %d active readers = %d, waiting writers = %d, waiting readers=%d\n",fState,  fActiveReaders, fWriteWaiters, fReadWaiters);
        CountConflict(1);  
    }
 
#endif
    
	/* ����һ�����ȴ����� */
    AddReadWaiter();
	/* ����Ϊд��Ծ״̬(�ж����д?),�����ȴ� */
    while   (   ActiveWriter() // active writer so wait
            ||  WaitingWriters() // reader must wait for write waiters
            )
    {   
		/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,�μ�OSCond::Wait() */
        fReadersCond.Wait(&fInternalLock,OSMutexRW::eMaxWait);
    }
    
	/* ����һ�����ȴ����� */
    RemoveReadWaiter();
	/* ����һ����Ծ������,��ʹfState��1 */
    AddActiveReader(); // add 1 to active readers
	/* ���õ�ǰ�Ļ�Ծ������ */
    fActiveReaders = fState;
    
#if DEBUGMUTEXRW
//  qtss_printf("LockRead(conflict) fState = %d active readers = %d, waiting writers = %d, waiting readers=%d\n",fState,  fActiveReaders, fWriteWaiters, fReadWaiters);
#endif
}

/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,ͬʱ���µ�ǰ�Ļ�Ծ������ */
void OSMutexRW::LockWrite()
{
    OSMutexLocker locker(&fInternalLock);
	/* ����һ��д�ȴ� */
    AddWriteWaiter();       //  1 writer queued            
#if DEBUGMUTEXRW

    if (Active()) 
    {   qtss_printf("LockWrite(conflict) state = %d active readers = %d, waiting writers = %d, waiting readers=%d\n", fState, fActiveReaders, fWriteWaiters, fReadWaiters);
        CountConflict(1);  
    }

    qtss_printf("LockWrite 'waiting' fState = %d locked active readers = %d, waiting writers = %d, waiting readers=%d\n",fState, fActiveReaders, fReadWaiters, fWriteWaiters);
#endif

	/* ���ж��ڽ���ʱ,����ȴ� */
    while   (ActiveReaders())  // active readers
    {   
		/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,�μ�OSCond::Wait() */
        fWritersCond.Wait(&fInternalLock,OSMutexRW::eMaxWait);
    }

	/* ʹ�ȴ�д�ĸ�����1 (ʵ���ϴ�ʱΪ-1)*/
    RemoveWriteWaiter(); // remove from waiting writers
	/* ����Ϊ��Ծд״̬(fState=-1) */
    SetState(OSMutexRW::eActiveWriterState);    // this is the active writer  
	/* ���õ�ǰ�Ļ�Ծ������Ϊ-1 */
    fActiveReaders = fState; 
#if DEBUGMUTEXRW
//  qtss_printf("LockWrite 'locked' fState = %d locked active readers = %d, waiting writers = %d, waiting readers=%d\n",fState, fActiveReaders, fReadWaiters, fWriteWaiters);
#endif

}

/* ����������:��ǰΪ��Ծд״̬,��ָ��д�������֪ͨ״̬,����ͨ��ѭ��ʹ���ж�ָ���������֪ͨ״̬;��ǰ״̬Ϊ��Ծ��״̬ʱ,ʹ��Ծ��������
   һ,��û�ж��ȴ�ʱ,��д�����������֪ͨ״̬. ���Ҫ���»�Ծ����ĿfActiveReaders */
void OSMutexRW::Unlock()
{           
    OSMutexLocker locker(&fInternalLock);
#if DEBUGMUTEXRW
//  qtss_printf("Unlock active readers = %d, waiting writers = %d, waiting readers=%d\n", fActiveReaders, fReadWaiters, fWriteWaiters);

#endif

	/* ����ǰΪд��Ծ״̬(fState=-1) */
    if (ActiveWriter()) 
    {   
		/* ���õ�ǰ״̬ΪfState=0,����״̬ */
        SetState(OSMutexRW::eNoWriterState); // this was the active writer 
		/* ����д�����ȴ�ʱ */
        if (WaitingWriters()) // there are waiting writers
        {   
			/* ��ָ��д�����������֪ͨ״̬ */
			fWritersCond.Signal();
        }
        else
        {   
			/* ͨ��ѭ��ʹ���ж�ָ�������������֪ͨ״̬ */
			fReadersCond.Broadcast();
        }
#if DEBUGMUTEXRW
        qtss_printf("Unlock(writer) active readers = %d, waiting writers = %d, waiting readers=%d\n", fActiveReaders, fReadWaiters, fWriteWaiters);
#endif
    }
    else
    {
		/* ʹ���ȴ�����һ,��ΪĿǰ���ڴ���һ�����ȴ�,�ɶ�ε���OSMutexRW::Unlock()ʹfReadWaiters=fActiveReaders=0  */
        RemoveActiveReader(); // this was a reader
		/* ����û�ж��ȴ� */
        if (!ActiveReaders()) // no active readers
        {   
			/* ���õ�ǰΪ����״̬ */
			SetState(OSMutexRW::eNoWriterState); // this was the active writer now no actives threads
			/* ��ָ��д�����������֪ͨ״̬ */
            fWritersCond.Signal();
        } 
    }
	/* ���»�Ծ����Ŀ */
    fActiveReaders = fState;

}



// Returns true on successful grab of the lock, false on failure
/* ��OSMutexRW::LockWrite()�İ�װ,��������״̬���ж� */
int OSMutexRW::TryLockWrite()
{
    int    status  = EBUSY;
    OSMutexLocker locker(&fInternalLock);

	/* �������ɵ���û�еȴ���д����ʱ */
    if ( !Active() && !WaitingWriters()) // no writers, no readers, no waiting writers
    {
		/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,ͬʱ���µ�ǰ�Ļ�Ծ������ */
        this->LockWrite();
        status = 0;
    }

    return status;
}

/* ��OSMutexRW::LockRead()�İ�װ,��������״̬���ж� */
int OSMutexRW::TryLockRead()
{
    int    status  = EBUSY;
    OSMutexLocker locker(&fInternalLock);

	/* ����ǰ���ǵȴ�д������д״̬ʱ */
    if ( !ActiveWriter() && !WaitingWriters() ) // no current writers but other readers ok
    {
		/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1,ͬʱ���µ�ǰ�Ļ�Ծ������ */
        this->LockRead(); 
        status = 0;
    }
    
    return status;
}



