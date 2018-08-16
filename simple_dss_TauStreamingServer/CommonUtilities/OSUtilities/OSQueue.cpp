/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSQueue.cpp
Description: Provide a queue operation class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "OSQueue.h"


OSQueue::OSQueue() : fLength(0)
{
    fSentinel.fNext = &fSentinel;
    fSentinel.fPrev = &fSentinel;
}

/* 将入参补在当前队列的锚点队列元素后面 */
void OSQueue::EnQueue(OSQueueElem* elem)
{
    Assert(elem != NULL);
	/* 假如该队列所在的队列指针就是当前队列指针,这说明该队列元已在当前队列中,无条件返回 */
    if (elem->fQueue == this)
        return;
	/* 所以要加入一个新的队列元,一定要确保它的队列指针为空! */
    Assert(elem->fQueue == NULL);
	/* 将elem加到fQueue的后面 */
    elem->fNext = fSentinel.fNext;
    elem->fPrev = &fSentinel;
    elem->fQueue = this;
	/* 修改相应的位置关系 */
    fSentinel.fNext->fPrev = elem;
    fSentinel.fNext = elem;
	/* 长度增加1 */
    fLength++;
}

/* 删除并返回锚点队列元素的前一个元素, used in FileBlockPool::GetBufferElement() */
OSQueueElem* OSQueue::DeQueue()
{
    if (fLength > 0)
    {
		/* 取fSentinel的前一个队列元素 */
        OSQueueElem* elem = fSentinel.fPrev;
		/* 断言两个相邻元素不相等  */
        Assert(fSentinel.fPrev != &fSentinel);
		/* 调整删去fSentinel的前一个队列元素后的位置关系 */
        elem->fPrev->fNext = &fSentinel;
        fSentinel.fPrev = elem->fPrev;
		/* 删去该队列元素 */
        elem->fQueue = NULL;
		/* 队列长度减1 */
        fLength--;
        return elem;
    }
    else
        return NULL;
}

/* 从当前队列中移去入参 */
void OSQueue::Remove(OSQueueElem* elem)
{
	/* 确保入参非空且不是数据成员 */
    Assert(elem != NULL);
    Assert(elem != &fSentinel);
    
	/* 确保入参就是当前队列中的元素,才能移去它 */
    if (elem->fQueue == this)
    {
		/* 调整删去fSentinel的前一个队列元素后的位置关系 */
        elem->fNext->fPrev = elem->fPrev;
        elem->fPrev->fNext = elem->fNext;
		/* 删去该队列元素 */
        elem->fQueue = NULL;
		/* 队列长度减1 */
        fLength--;
    }
}

/* 下面的demo将1,2,3加入当前队列,验证OSQueueElem和OSQueue中的方法 */
#if OSQUEUETESTING
Bool16 OSQueue::Test()
{
    OSQueue theVictim;
    void *x = (void*)1;
    OSQueueElem theElem1(x);
    x = (void*)2;
    OSQueueElem theElem2(x);
    x = (void*)3;
    OSQueueElem theElem3(x);
    
    if (theVictim.GetHead() != NULL)
        return false;
    if (theVictim.GetTail() != NULL)
        return false;
    
    theVictim.EnQueue(&theElem1);
    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem1)
        return false;
    
    OSQueueElem* theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;
    
    if (theVictim.GetHead() != NULL)
        return false;
    if (theVictim.GetTail() != NULL)
        return false;
    
    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);

    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;
        
    theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem2)
        return false;

    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);
    theVictim.EnQueue(&theElem3);

    if (theVictim.GetHead() != &theElem1)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem1)
        return false;

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem2)
        return false;

    if (theVictim.GetHead() != &theElem3)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theElem = theVictim.DeQueue();
    if (theElem != &theElem3)
        return false;

    theVictim.EnQueue(&theElem1);
    theVictim.EnQueue(&theElem2);
    theVictim.EnQueue(&theElem3);
    
    OSQueueIter theIterVictim(&theVictim);
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem3)
        return false;
    theIterVictim.Next();
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem2)
        return false;
    theIterVictim.Next();
    if (theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != &theElem1)
        return false;
    theIterVictim.Next();
    if (!theIterVictim.IsDone())
        return false;
    if (theIterVictim.GetCurrent() != NULL)
        return false;

    theVictim.Remove(&theElem1);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theVictim.Remove(&theElem1);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem3)
        return false;

    theVictim.Remove(&theElem3);

    if (theVictim.GetHead() != &theElem2)
        return false;
    if (theVictim.GetTail() != &theElem2)
        return false;

    return true;
}   
#endif


/* 该函数处理Queue element的前移问题,主要是要时刻判断是否位于队列末尾? */
void OSQueueIter::Next()
{
	/* 依据当前队列元素指针的位置分情形 */
	/* 判断当当前队列元素指针和其后的队列元素相等时 */
    if (fCurrentElemP == fQueueP->GetTail())
        fCurrentElemP = NULL;
    else
		/* 后移一个 */
        fCurrentElemP = fCurrentElemP->Prev();
}

/* used in TaskThread::WaitForTask() */
/* 当队列长度为0时,仅让当前线程阻塞,超时等待指定时间后,返回NULL;否则,否则删去并返回当前队列中锚点元素的前一个队列元素 */
OSQueueElem* OSQueue_Blocking::DeQueueBlocking(OSThread* inCurThread, SInt32 inTimeoutInMilSecs)
{
    OSMutexLocker theLocker(&fMutex);
#ifdef __Win32_
	 /* 当当前队列长度为0,则仅让当前线程阻塞,超时等待,并返回空值 */
     if (fQueue.GetLength() == 0) 
	 {	fCond.Wait(&fMutex, inTimeoutInMilSecs);
		return NULL;
	 }
#else
    if (fQueue.GetLength() == 0) 
        fCond.Wait(&fMutex, inTimeoutInMilSecs);
#endif

	/* 否则删去并返回当前队列中锚点元素的前一个队列元素 */
    OSQueueElem* retval = fQueue.DeQueue();
    return retval;
}

/* 删去并返回当前队列中锚点元素的前一个队列元素 */
OSQueueElem*    OSQueue_Blocking::DeQueue()
{
    OSMutexLocker theLocker(&fMutex);
	/* 用到OSQueue::DeQueue() */
    OSQueueElem* retval = fQueue.DeQueue(); 
    return retval;
}

/* 将入参加入当前队列,并传信 */
void OSQueue_Blocking::EnQueue(OSQueueElem* obj)
{
    {
		/* 获取互斥锁 */
        OSMutexLocker theLocker(&fMutex);
		/* 将当前入参加入描点队列元素后 */
        fQueue.EnQueue(obj);
    }
	/* 参见OSCond.h,等同于::SetEvent(fCondition) */
	/* 发信号通知外部条件已改变 */
    fCond.Signal();
}
