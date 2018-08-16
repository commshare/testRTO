
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
/* ֻ��һ�����������̱߳����еĿ���������� */
/* ��û�п����߳�ʱ,��IdleTask::Initialize()������������ */
/* ����static variable,��ʼ����IdleTask::Initialize() */
IdleTaskThread*     IdleTask::sIdleThread = NULL;



/* ��ָ���������ó�ʱʱ��,ʹ����ѯ */
void IdleTaskThread::SetIdleTimer(IdleTask *activeObj, SInt64 msec)
{
    //note: OSHeap doesn't support a random remove, so this function
    //won't change the timeout value if there is already one set
	/* ��OSHeapElem��ĳ��OSHeap��Ԫ,�����ٽ������fIdleHeap��,���������� */
    if (activeObj->fIdleElem.IsMemberOfAnyHeap())
        return;

	/* �������OSHeapElem��ֵ,��ѯ�Ա�ʹ��ʱʱ�̵���ʱ��ʱ */
    activeObj->fIdleElem.SetValue(OS::Milliseconds() + msec);
    
    {
		/* ��OSHeap(fIdleHeap)����ʱ���� */
        OSMutexLocker locker(&fHeapMutex);
		/* ������е�OSHeapElem����OSHeapElem,ʹ����ѯ */
        fIdleHeap.Insert(&activeObj->fIdleElem);
    }

	/* ����ʱʱ�䷢�ź�,���ѹ����/֪ͨ��Ӧ��TaskThread */
    fHeapCond.Signal();
}

/* �����������ɾȥ��ʱ��ָ�������� */
void IdleTaskThread::CancelTimeout(IdleTask* idleObj)
{
	/* ȷ��IdleTask����! */
    Assert(idleObj != NULL);
    OSMutexLocker locker(&fHeapMutex);
	/* ��ָ����OSHeap����ȥOSHeapElem */
    fIdleHeap.Remove(&idleObj->fIdleElem);  
}

/* ��ȡHeap�е�������Ԫ��,����ʱ��С�ڵ�ǰʱ��ʱ,���Ϳ����¼�;ͬʱ����һ��Heap�е���СԪ�ȴ���Ӧ��ʱ�� */

/* ��fIdleHeap����Ԫ��ʱ���޵ȴ�;����ʱʱ�䵽��ʱ,ȡ��fIdleHeap����ӦԪ�ط�Signal��ʱ����;��δ����ʱʱ��,�͵ȴ���С�ĳ�ʱʱ�� */
void   IdleTaskThread::Entry()
{
	/* ������Idle Timer Heap����OSMutex */
    OSMutexLocker locker(&fHeapMutex);
    
	/* Ψһ��IdleTaskThread����ѭ��ִ�� */
    while (true)
    {
        //if there are no events to process, block.
		/* ����Heap��СΪ0,˵��û���¼�Ҫ����,�ȴ� */
        if (fIdleHeap.CurrentHeapSize() == 0)
			/* ����OSCond����wait�ź�,�������߳������Ƶȴ� */
            fHeapCond.Wait(&fHeapMutex);
		/* ��ȡ��ǰʱ�� */
        SInt64 msec = OS::Milliseconds();
        
        //pop elements out of the heap as long as their timeout time has arrived
		/* ����ǰHeap��С����,�Ҳ������������Ԫ�ص�ֵ(��ǰ�����úõĳ�ʱֵ)С�ڵ�ǰʱ��,˵���ѵ���ʱʱ��,�ͽ���ѹ��fIdleHeap�������ź�(��������������) */
        while ((fIdleHeap.CurrentHeapSize() > 0) && (fIdleHeap.PeekMin()->GetValue() <= msec))
        {
			/* ȡ��Heap��������Ԫ��,�����ڵ�����������ǹ��ĵ�,ʣ�µ�fIdleHeap�е�Ԫ��Ҫ���� */
            IdleTask* elem = (IdleTask*)fIdleHeap.ExtractMin()->GetEnclosingObject();
            Assert(elem != NULL);
			/* �������¼��ź�,���ѹ��������,���������̵߳�������� */
            elem->Signal(Task::kIdleEvent);
        }
                        
        //we are done sending idle events. If there is a lowest tick count, then
        //we need to sleep until that time.
		/* �ڷ��Ϳ����¼��ź�ʱ,����һ��Heap�е���СԪ�ȴ���Ӧ��ʱ�� */
        if (fIdleHeap.CurrentHeapSize() > 0)
        {
			/* �鿴fIdleHeap�е�Ԫ�ص���Сֵ,ע��ʣ�µ�fIdleHeap�е�Ԫ�ز����� */
            SInt64 timeoutTime = fIdleHeap.PeekMin()->GetValue();
            
			/* ��ʱ����������Ԫ�صĳ�ʱֵ(ע�����ĳ�ʱֵ��ԭ����ͬ) */
            timeoutTime -= msec;
            Assert(timeoutTime > 0);
			//because sleep takes a 32 bit number
			/* ������Գ�ʱʱ�� */
            UInt32 smallTime = (UInt32)timeoutTime;
			/* �ȴ�ָ����Գ�ʱʱ�� */
			/* ֪ͨ��Ӧ������ȴ�ָ���ĳ�ʱʱ�� */
            fHeapCond.Wait(&fHeapMutex, smallTime);
        }
    }   
}

/* ��û�п����߳�ʱ,������������ */
void IdleTask::Initialize()
{
	/* ��û�п����߳�ʱ,������ */
    if (sIdleThread == NULL)
    {
        sIdleThread = NEW IdleTaskThread();
		/* ���������߳� */
        sIdleThread->Start();
    }
}

/* ����IdleTaskThread::CancelTimeout()�ӿ��������̵߳�Heap����ɾ��HeapElem,ȡ����ʱ���� */
IdleTask::~IdleTask()
{
    //clean up stuff used by idle thread routines
	/* ȷ�������̴߳��� */
    Assert(sIdleThread != NULL);
    /* �û��������� */
    OSMutexLocker locker(&sIdleThread->fHeapMutex);

    //Check to see if there is a pending(����������) timeout. If so, get this object
    //out of the heap
	/* ȷ����һ��Heap���� */
    if (fIdleElem.IsMemberOfAnyHeap())
		/* ��OSHeap��ȡ����ʱ���� */
        sIdleThread->CancelTimeout(this);
}



