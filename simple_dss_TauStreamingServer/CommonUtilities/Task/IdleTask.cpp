
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 IdleTask.cpp
Description: Implemention an normal task class with one exception: If you call SetIdleTimer
             on one, after the time has elapsed the task object will receive an Task_IDLE event.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "IdleTask.h"
#include "OSMemory.h"
#include "OS.h"



//IDLETASKTHREAD IMPLEMENTATION:
/* 只有一个空闲任务线程被所有的空闲任务分享 */
/* 若没有空闲线程时,用IdleTask::Initialize()创建并启动它 */
/* 定义static variable,初始化见IdleTask::Initialize() */
IdleTaskThread*     IdleTask::sIdleThread = NULL;



/* 给指定任务设置超时时间,使其轮询 */
void IdleTaskThread::SetIdleTimer(IdleTask *activeObj, SInt64 msec)
{
    //note: OSHeap doesn't support a random remove, so this function
    //won't change the timeout value if there is already one set
	/* 若OSHeapElem是某个OSHeap的元,无须再将其放入fIdleHeap中,无条件返回 */
    if (activeObj->fIdleElem.IsMemberOfAnyHeap())
        return;

	/* 设置这个OSHeapElem的值,轮询以便使超时时刻到来时超时 */
    activeObj->fIdleElem.SetValue(OS::Milliseconds() + msec);
    
    {
		/* 对OSHeap(fIdleHeap)操作时加锁 */
        OSMutexLocker locker(&fHeapMutex);
		/* 将入参中的OSHeapElem加入OSHeapElem,使其轮询 */
        fIdleHeap.Insert(&activeObj->fIdleElem);
    }

	/* 到超时时间发信号,唤醒挂起的/通知相应的TaskThread */
    fHeapCond.Signal();
}

/* 从任务队列中删去超时的指定的任务 */
void IdleTaskThread::CancelTimeout(IdleTask* idleObj)
{
	/* 确保IdleTask存在! */
    Assert(idleObj != NULL);
    OSMutexLocker locker(&fHeapMutex);
	/* 从指定的OSHeap中移去OSHeapElem */
    fIdleHeap.Remove(&idleObj->fIdleElem);  
}

/* 获取Heap中的最上面元素,当其时间小于当前时间时,发送空闲事件;同时让下一个Heap中的最小元等待相应的时间 */

/* 当fIdleHeap中无元素时无限等待;当超时时间到来时,取出fIdleHeap中相应元素发Signal超时任务;当未到超时时间,就等待最小的超时时间 */
void   IdleTaskThread::Entry()
{
	/* 对整个Idle Timer Heap加上OSMutex */
    OSMutexLocker locker(&fHeapMutex);
    
	/* 唯一的IdleTaskThread反复循环执行 */
    while (true)
    {
        //if there are no events to process, block.
		/* 假如Heap大小为0,说明没有事件要处理,等待 */
        if (fIdleHeap.CurrentHeapSize() == 0)
			/* 利用OSCond发送wait信号,让任务线程无限制等待 */
            fHeapCond.Wait(&fHeapMutex);
		/* 获取当前时间 */
        SInt64 msec = OS::Milliseconds();
        
        //pop elements out of the heap as long as their timeout time has arrived
		/* 当当前Heap大小非零,且擦看到最上面的元素的值(先前已设置好的超时值)小于当前时间,说明已到超时时间,就将其压出fIdleHeap，并发信号(将其放入任务队列) */
        while ((fIdleHeap.CurrentHeapSize() > 0) && (fIdleHeap.PeekMin()->GetValue() <= msec))
        {
			/* 取出Heap中最上面元素,它所在的类对象是我们关心的,剩下的fIdleHeap中的元素要重排 */
            IdleTask* elem = (IdleTask*)fIdleHeap.ExtractMin()->GetEnclosingObject();
            Assert(elem != NULL);
			/* 发空闲事件信号,唤醒挂起的任务,将其放入该线程的任务队列 */
            elem->Signal(Task::kIdleEvent);
        }
                        
        //we are done sending idle events. If there is a lowest tick count, then
        //we need to sleep until that time.
		/* 在发送空闲事件信号时,让下一个Heap中的最小元等待相应的时间 */
        if (fIdleHeap.CurrentHeapSize() > 0)
        {
			/* 查看fIdleHeap中的元素的最小值,注意剩下的fIdleHeap中的元素不重排 */
            SInt64 timeoutTime = fIdleHeap.PeekMin()->GetValue();
            
			/* 及时更新最上面元素的超时值(注意它的超时值和原来不同) */
            timeoutTime -= msec;
            Assert(timeoutTime > 0);
			//because sleep takes a 32 bit number
			/* 设置相对超时时间 */
            UInt32 smallTime = (UInt32)timeoutTime;
			/* 等待指定相对超时时间 */
			/* 通知相应的任务等待指定的超时时间 */
            fHeapCond.Wait(&fHeapMutex, smallTime);
        }
    }   
}

/* 若没有空闲线程时,创建并启动它 */
void IdleTask::Initialize()
{
	/* 当没有空闲线程时,创建它 */
    if (sIdleThread == NULL)
    {
        sIdleThread = NEW IdleTaskThread();
		/* 启动空闲线程 */
        sIdleThread->Start();
    }
}

/* 利用IdleTaskThread::CancelTimeout()从空闲任务线程的Heap中用删除HeapElem,取消超时任务 */
IdleTask::~IdleTask()
{
    //clean up stuff used by idle thread routines
	/* 确保空闲线程存在 */
    Assert(sIdleThread != NULL);
    /* 用互斥锁锁定 */
    OSMutexLocker locker(&sIdleThread->fHeapMutex);

    //Check to see if there is a pending(即将到来的) timeout. If so, get this object
    //out of the heap
	/* 确信有一个Heap存在 */
    if (fIdleElem.IsMemberOfAnyHeap())
		/* 从OSHeap中取消超时任务 */
        sIdleThread->CancelTimeout(this);
}



