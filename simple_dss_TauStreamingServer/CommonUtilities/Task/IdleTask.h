
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 IdleTask.h
Description: Implemention an normal task class with one exception: If you call SetIdleTimer
             on one, after the time has elapsed the task object will receive an Task_IDLE event.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#ifndef _IDLETASK_H_
#define _IDLETASK_H_

#include "Task.h"
#include "OSThread.h"
#include "OSHeap.h"
#include "OSMutex.h"
#include "OSCond.h"

class IdleTask;

//merely a private implementation detail of IdleTask
class IdleTaskThread : private OSThread
{
private:

    IdleTaskThread() : OSThread(), fHeapMutex() {}
    virtual ~IdleTaskThread() { Assert(fIdleHeap.CurrentHeapSize() == 0); }

	/* ע���������������ĵ�һ����ζ���IdleTask */
	/* ��ָ���������ó�ʱʱ��,ʹ����ѯ */
    void SetIdleTimer(IdleTask *idleObj, SInt64 msec);
	/* �����������ɾȥ��ʱ��ָ�������� */
    void CancelTimeout(IdleTask *idleObj);
    
	// thread entry function
	/* ��ȡHeap�е�������Ԫ��,����ʱ��С�ڵ�ǰʱ��ʱ,���Ϳ����¼�;ͬʱ����һ��Heap�е���СԪ�ȴ���Ӧ��ʱ�� */
    virtual void Entry();

	//data members
	/* IdleTaskThread��һϵ�е�IdleTask��Timeout��OSHeapElem�򽻵�!���������һϵ�е�IdleTask */
	/* ���������fIdleHeap�л�����mutex����������oscond */
    OSHeap  fIdleHeap;
	/* �ڲ���HeapElemʱҪ��Mutex */
    OSMutex fHeapMutex;
	/* ��IdleTimeʱ��Signal(),�μ�IdleTaskThread::SetIdleTimer() */
    OSCond  fHeapCond;
    friend class IdleTask;
};


class IdleTask : public Task
{

public:

    //Call Initialize before using this class
	/* ��û�п����߳�ʱ,������������ */
    static void Initialize();
    
	/* ����IdleTask��������,�Ͷ���Ԫ�����Ķ��� */
    IdleTask() : Task(), fIdleElem() { this->SetTaskName("IdleTask"); fIdleElem.SetEnclosingObject(this); }
    
    //This object does a "best effort" of making sure a timeout isn't
    //pending for an object being deleted. In other words, if there is
    //a timeout pending, and the destructor is called, things will get cleaned
    //up. But callers must ensure that SetIdleTimer isn't called at the same
    //time as the destructor, or all hell will break loose.
	/* �ӿ��������̵߳�Heap��ɾ��HeapElem,ȡ����ʱ���� */
    virtual ~IdleTask();
    
    //SetIdleTimer:
    //This object will receive an OS_IDLE event in the following number of milliseconds.
    //Only one timeout can be outstanding, if there is already a timeout scheduled, this
    //does nothing.
	/* ʵ�����ǵ���IdleTaskThread�е���Ӧ����,��IdleTaskThread�����ÿ��м�ʱ�� */
    void SetIdleTimer(SInt64 msec) { sIdleThread->SetIdleTimer(this, msec); }

    //CancelTimeout
    //If there is a pending timeout for this object, this function cancels it.
    //If there is no pending timeout, this function does nothing.
    //Currently not supported because OSHeap doesn't support random remove
	/* ʵ�����ǵ���IdleTaskThread�е���Ӧ���� */
    void CancelTimeout() { sIdleThread->CancelTimeout(this); }

private:

	/* һ��IdleTask��Timeoutֵ��ΪOSHeap�е�һ��Ԫ��OSHeapElem */
    OSHeapElem fIdleElem;

    //there is only one idle thread shared by all idle tasks.
	/* ֻ��һ�����������̱߳����еĿ���������� */
	/* �ǳ���Ҫ!!ȫ�־�̬���� */
    static IdleTaskThread*  sIdleThread;    

    friend class IdleTaskThread;
};
#endif
