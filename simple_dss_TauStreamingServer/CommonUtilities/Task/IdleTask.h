
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

	/* 注意以下两个函数的第一个入参都是IdleTask */
	/* 给指定任务设置超时时间,使其轮询 */
    void SetIdleTimer(IdleTask *idleObj, SInt64 msec);
	/* 从任务队列中删去超时的指定的任务 */
    void CancelTimeout(IdleTask *idleObj);
    
	// thread entry function
	/* 获取Heap中的最上面元素,当其时间小于当前时间时,发送空闲事件;同时让下一个Heap中的最小元等待相应的时间 */
    virtual void Entry();

	//data members
	/* IdleTaskThread和一系列的IdleTask的Timeout的OSHeapElem打交道!有序管理着一系列的IdleTask */
	/* 空闲任务堆fIdleHeap有互斥锁mutex和条件变量oscond */
    OSHeap  fIdleHeap;
	/* 在操作HeapElem时要加Mutex */
    OSMutex fHeapMutex;
	/* 当IdleTime时发Signal(),参见IdleTaskThread::SetIdleTimer() */
    OSCond  fHeapCond;
    friend class IdleTask;
};


class IdleTask : public Task
{

public:

    //Call Initialize before using this class
	/* 若没有空闲线程时,创建并启动它 */
    static void Initialize();
    
	/* 设置IdleTask的任务名,和队列元关联的对象 */
    IdleTask() : Task(), fIdleElem() { this->SetTaskName("IdleTask"); fIdleElem.SetEnclosingObject(this); }
    
    //This object does a "best effort" of making sure a timeout isn't
    //pending for an object being deleted. In other words, if there is
    //a timeout pending, and the destructor is called, things will get cleaned
    //up. But callers must ensure that SetIdleTimer isn't called at the same
    //time as the destructor, or all hell will break loose.
	/* 从空闲任务线程的Heap中删除HeapElem,取消超时任务 */
    virtual ~IdleTask();
    
    //SetIdleTimer:
    //This object will receive an OS_IDLE event in the following number of milliseconds.
    //Only one timeout can be outstanding, if there is already a timeout scheduled, this
    //does nothing.
	/* 实质上是调用IdleTaskThread中的相应函数,由IdleTaskThread来设置空闲计时器 */
    void SetIdleTimer(SInt64 msec) { sIdleThread->SetIdleTimer(this, msec); }

    //CancelTimeout
    //If there is a pending timeout for this object, this function cancels it.
    //If there is no pending timeout, this function does nothing.
    //Currently not supported because OSHeap doesn't support random remove
	/* 实质上是调用IdleTaskThread中的相应函数 */
    void CancelTimeout() { sIdleThread->CancelTimeout(this); }

private:

	/* 一个IdleTask的Timeout值作为OSHeap中的一个元素OSHeapElem */
    OSHeapElem fIdleElem;

    //there is only one idle thread shared by all idle tasks.
	/* 只有一个空闲任务线程被所有的空闲任务分享 */
	/* 非常重要!!全局静态变量 */
    static IdleTaskThread*  sIdleThread;    

    friend class IdleTaskThread;
};
#endif
