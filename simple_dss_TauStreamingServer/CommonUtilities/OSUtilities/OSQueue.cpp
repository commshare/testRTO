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

/* ����β��ڵ�ǰ���е�ê�����Ԫ�غ��� */
void OSQueue::EnQueue(OSQueueElem* elem)
{
    Assert(elem != NULL);
	/* ����ö������ڵĶ���ָ����ǵ�ǰ����ָ��,��˵���ö���Ԫ���ڵ�ǰ������,���������� */
    if (elem->fQueue == this)
        return;
	/* ����Ҫ����һ���µĶ���Ԫ,һ��Ҫȷ�����Ķ���ָ��Ϊ��! */
    Assert(elem->fQueue == NULL);
	/* ��elem�ӵ�fQueue�ĺ��� */
    elem->fNext = fSentinel.fNext;
    elem->fPrev = &fSentinel;
    elem->fQueue = this;
	/* �޸���Ӧ��λ�ù�ϵ */
    fSentinel.fNext->fPrev = elem;
    fSentinel.fNext = elem;
	/* ��������1 */
    fLength++;
}

/* ɾ��������ê�����Ԫ�ص�ǰһ��Ԫ��, used in FileBlockPool::GetBufferElement() */
OSQueueElem* OSQueue::DeQueue()
{
    if (fLength > 0)
    {
		/* ȡfSentinel��ǰһ������Ԫ�� */
        OSQueueElem* elem = fSentinel.fPrev;
		/* ������������Ԫ�ز����  */
        Assert(fSentinel.fPrev != &fSentinel);
		/* ����ɾȥfSentinel��ǰһ������Ԫ�غ��λ�ù�ϵ */
        elem->fPrev->fNext = &fSentinel;
        fSentinel.fPrev = elem->fPrev;
		/* ɾȥ�ö���Ԫ�� */
        elem->fQueue = NULL;
		/* ���г��ȼ�1 */
        fLength--;
        return elem;
    }
    else
        return NULL;
}

/* �ӵ�ǰ��������ȥ��� */
void OSQueue::Remove(OSQueueElem* elem)
{
	/* ȷ����ηǿ��Ҳ������ݳ�Ա */
    Assert(elem != NULL);
    Assert(elem != &fSentinel);
    
	/* ȷ����ξ��ǵ�ǰ�����е�Ԫ��,������ȥ�� */
    if (elem->fQueue == this)
    {
		/* ����ɾȥfSentinel��ǰһ������Ԫ�غ��λ�ù�ϵ */
        elem->fNext->fPrev = elem->fPrev;
        elem->fPrev->fNext = elem->fNext;
		/* ɾȥ�ö���Ԫ�� */
        elem->fQueue = NULL;
		/* ���г��ȼ�1 */
        fLength--;
    }
}

/* �����demo��1,2,3���뵱ǰ����,��֤OSQueueElem��OSQueue�еķ��� */
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


/* �ú�������Queue element��ǰ������,��Ҫ��Ҫʱ���ж��Ƿ�λ�ڶ���ĩβ? */
void OSQueueIter::Next()
{
	/* ���ݵ�ǰ����Ԫ��ָ���λ�÷����� */
	/* �жϵ���ǰ����Ԫ��ָ������Ķ���Ԫ�����ʱ */
    if (fCurrentElemP == fQueueP->GetTail())
        fCurrentElemP = NULL;
    else
		/* ����һ�� */
        fCurrentElemP = fCurrentElemP->Prev();
}

/* used in TaskThread::WaitForTask() */
/* �����г���Ϊ0ʱ,���õ�ǰ�߳�����,��ʱ�ȴ�ָ��ʱ���,����NULL;����,����ɾȥ�����ص�ǰ������ê��Ԫ�ص�ǰһ������Ԫ�� */
OSQueueElem* OSQueue_Blocking::DeQueueBlocking(OSThread* inCurThread, SInt32 inTimeoutInMilSecs)
{
    OSMutexLocker theLocker(&fMutex);
#ifdef __Win32_
	 /* ����ǰ���г���Ϊ0,����õ�ǰ�߳�����,��ʱ�ȴ�,�����ؿ�ֵ */
     if (fQueue.GetLength() == 0) 
	 {	fCond.Wait(&fMutex, inTimeoutInMilSecs);
		return NULL;
	 }
#else
    if (fQueue.GetLength() == 0) 
        fCond.Wait(&fMutex, inTimeoutInMilSecs);
#endif

	/* ����ɾȥ�����ص�ǰ������ê��Ԫ�ص�ǰһ������Ԫ�� */
    OSQueueElem* retval = fQueue.DeQueue();
    return retval;
}

/* ɾȥ�����ص�ǰ������ê��Ԫ�ص�ǰһ������Ԫ�� */
OSQueueElem*    OSQueue_Blocking::DeQueue()
{
    OSMutexLocker theLocker(&fMutex);
	/* �õ�OSQueue::DeQueue() */
    OSQueueElem* retval = fQueue.DeQueue(); 
    return retval;
}

/* ����μ��뵱ǰ����,������ */
void OSQueue_Blocking::EnQueue(OSQueueElem* obj)
{
    {
		/* ��ȡ������ */
        OSMutexLocker theLocker(&fMutex);
		/* ����ǰ��μ���������Ԫ�غ� */
        fQueue.EnQueue(obj);
    }
	/* �μ�OSCond.h,��ͬ��::SetEvent(fCondition) */
	/* ���ź�֪ͨ�ⲿ�����Ѹı� */
    fCond.Signal();
}
