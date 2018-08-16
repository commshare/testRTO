/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 Task.h
Description: Implemention an class which can be scheduled by signal() and get
             cpu time to finish work by Run().
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#include "Task.h"
#include "OS.h"
#include "OSMemory.h"
#include "atomic.h" /* use atom_sub() */
#include "OSMutexRW.h"


unsigned int    Task::sThreadPicker = 0;
OSMutexRW       TaskThreadPool::sMutexRW;
static char*    sTaskStateStr="live_"; //Alive

Task::Task()
:   fEvents(0), fUseThisThread(NULL), fWriteLock(false), fTimerHeapElem(), fTaskQueueElem()
{
#if DEBUG
    fInRunCount = 0;
#endif
    this->SetTaskName("unknown");

	/* 设置数据成员所在的类 */
	fTaskQueueElem.SetEnclosingObject(this);
	fTimerHeapElem.SetEnclosingObject(this);

}

/* used by RTPSession::RTPSession() */
/* 将Task名设为总长度为48的字符串"live_**************" */
void Task::SetTaskName(char* name) 
{
    if (name == NULL) 
        return;
   
   ::strncpy(fTaskName,sTaskStateStr,sizeof(fTaskName));
   ::strncat(fTaskName,name,sizeof(fTaskName));
   /* 多余的截断以保持48个字节长度 */
   fTaskName[sizeof(fTaskName) -1] = 0; //terminate in case it is longer than ftaskname.
   
}

/* 判断任务名是否合法,返回bool值 */
Bool16 Task::Valid()
{
	/* 假如fTaskName为空或前5个字符不是"live_",就输出错误提示 */
    if  (   (this->fTaskName == NULL)
         || (0 != ::strncmp(sTaskStateStr,this->fTaskName, 5))
         )
     {
        if (TASK_DEBUG) qtss_printf(" Task::Valid Found invalid task = %ld\n", this);
        
        return false;
     }
    
    return true;
}

/* used by RTPSession::Run() */
/* 返回最原始的event bit组合(首位bit为0),同时将fEvents置为kAlive或0 */
Task::EventFlags Task::GetEvents()
{
    //Mask off every event currently in the mask except for the alive bit, of course,
    //which should remain unaffected and unreported by this call.
	/* 仅将alive置0,fEvents值未变 */
    EventFlags events = fEvents & kAliveOff;
	/* 屏蔽fEvents,保留kAliveOff */
	/* fEvents变为0,events没变,还是fEvents原来的值 */
    (void)atomic_sub(&fEvents, events);//fEvents变为kAlive或0
    return events;//返回最原始的event bit组合(首位bit为0)
}

/* 非常重要的函数之一 */
/* 根据指定的event flags,通知对应的Task,即为当前任务指定一个线程,并放入线程的Task Queue中 */
void Task::Signal(EventFlags events)
{
	/* 先检查任务名fTaskName是否合法?不合法就无条件返回 */
    if (!this->Valid())
        return;
        
    //Fancy no mutex implementation. We atomically mask the new events into
    //the event mask. Because atomic_or returns the old state of the mask,
    //we only schedule this task once(调度该任务一次).
	/* 给设置alive flag,标记该eventflag是active活的 */
    events |= kAlive;
	/* 返回原fEvents(oldEvents的值),同时在函数中将它的值改变为增加指定的flag位 */
    EventFlags oldEvents = atomic_or(&fEvents, events);
	/* 假如原来的event是非alive(上步已确保是alive的),注意oldEvents & kAlive=0x0 */
    if ((!(oldEvents & kAlive)) && (TaskThreadPool::sNumTaskThreads > 0))
    {
		/* 假如当前任务已经指定了一个线程 */
        if (fUseThisThread != NULL)
            // Task needs to be placed on a particular thread.
         {
            if (TASK_DEBUG) if (fTaskName[0] == 0) ::strcpy(fTaskName, " corrupt task");
            if (TASK_DEBUG) qtss_printf("Task::Signal enque TaskName=%s fUseThisThread=%lu q elem=%lu enclosing=%lu\n", fTaskName, (UInt32) fUseThisThread, (UInt32) &fTaskQueueElem, (UInt32) this);
			/* 将该任务对应的队列元指针放入当前任务线程所在的Task队列 */
            fUseThisThread->fTaskQueue.EnQueue(&fTaskQueueElem);
        }
        else /* 否则就用轮询方式赋予该任务一个线程 */
        {
            //find a thread to put this task on
			/* 将当前线程ID加1再module线程总数,即以轮询(round robin)方式赋予一个线程 */
            unsigned int theThread = atomic_add(&sThreadPicker, 1);
            theThread %= TaskThreadPool::sNumTaskThreads;
			/* 预先错误处理,只有在TASK_DEBUG时才行 */
            if (TASK_DEBUG) if (fTaskName[0] == 0) ::strcpy(fTaskName, " corrupt task");
            if (TASK_DEBUG) qtss_printf("Task::Signal enque TaskName=%s thread=%lu q elem=%lu enclosing=%lu\n", fTaskName, (UInt32)TaskThreadPool::sTaskThreadArray[theThread],(UInt32) &fTaskQueueElem,(UInt32) this);
            /* 将该任务对应的队列元指针放入当前任务线程所在的Task队列 */
			TaskThreadPool::sTaskThreadArray[theThread]->fTaskQueue.EnQueue(&fTaskQueueElem);
        }
    }
    else/* 假如原来的Task就是alive的,就啥事不干! */
        if (TASK_DEBUG) qtss_printf("Task::Signal sent to dead TaskName=%s  q elem=%lu  enclosing=%lu\n",  fTaskName, (UInt32) &fTaskQueueElem, (UInt32) this);
        

}

/* 当写锁时,进行解锁 */
void Task::GlobalUnlock()    
{   
    if (this->fWriteLock)
    {   this->fWriteLock = false;   
        TaskThreadPool::sMutexRW.Unlock();
    }                                               
}


/* 调用WaitForTask()监测任务队列,若等待到任务后,启动Task::Run(),根据函数返回值分情形讨论:当返回负值时,立即删除该任务;当为0时,设置doneProcessingEvent=0;当为正值时,重置TimerHeapElem，轮询任务 */
void TaskThread::Entry()
{
	//空Task指针
    Task* theTask = NULL;
    
	//线程循环执行
	//监测是否有需要执行的任务，如果有就返回该任务；否则阻塞；
    while (true) 
    {
		/* 调用WaitForTask()监测任务队列,获取等待的任务,定义见下面 */
        theTask = this->WaitForTask();

        //
        // WaitForTask returns NULL when it is time to quit
		/* 假如没有等到任务或得到的任务不合法,就无条件返回 */
        if (theTask == NULL || false == theTask->Valid() )
            return;
          
		//尚未处理事件
        Bool16 doneProcessingEvent = false;
        
        while (!doneProcessingEvent)
        {
            //If a task holds locks when it returns from its Run function,
            //that would be catastrophic and certainly lead to a deadlock
#if DEBUG
            Assert(this->GetNumLocksHeld() == 0);
            Assert(theTask->fInRunCount == 0);
            theTask->fInRunCount++;
#endif
			/* 还没有相应的线程,下面会产生 */
            theTask->fUseThisThread = NULL; // Each invocation of Run must independently
                                            // request a specific thread.
            SInt64 theTimeout = 0;
            
			/* 启动任务的run()函数 */
            if (theTask->fWriteLock)
            {   
                OSMutexWriteLocker mutexLocker(&TaskThreadPool::sMutexRW);
                if (TASK_DEBUG) qtss_printf("TaskThread::Entry run global locked TaskName=%s CurMSec=%.3f thread=%ld task=%ld\n", theTask->fTaskName, OS::StartTimeMilli_Float() ,(SInt32) this,(SInt32) theTask);
                
				/* 以全局锁运行任务 */
                theTimeout = theTask->Run();
				/* 解锁 */
                theTask->fWriteLock = false;
            }
            else
            {
                OSMutexReadLocker mutexLocker(&TaskThreadPool::sMutexRW);
                if (TASK_DEBUG) qtss_printf("TaskThread::Entry run TaskName=%s CurMSec=%.3f thread=%ld task=%ld\n", theTask->fTaskName, OS::StartTimeMilli_Float(), (SInt32) this,(SInt32) theTask);

				/* 运行Task的Run() */
                theTimeout = theTask->Run();
            
            }
#if DEBUG
            Assert(this->GetNumLocksHeld() == 0);
            theTask->fInRunCount--;
            Assert(theTask->fInRunCount == 0);
#endif       
			/* 对Task::Run()的返回值分情形讨论: */
            if (theTimeout < 0)
            {
                if (TASK_DEBUG) 
                {
                    qtss_printf("TaskThread::Entry delete TaskName=%s CurMSec=%.3f thread=%ld task=%ld\n", theTask->fTaskName, OS::StartTimeMilli_Float(), (SInt32) this, (SInt32) theTask);
                     
                    theTask->fUseThisThread = NULL;
                    
                    if (NULL != fHeap.Remove(&theTask->fTimerHeapElem)) 
                        qtss_printf("TaskThread::Entry task still in heap before delete\n");
                    
                    if (NULL != theTask->fTaskQueueElem.InQueue())
                        qtss_printf("TaskThread::Entry task still in queue before delete\n");
                    
                    theTask->fTaskQueueElem.Remove();
                    
					/* 注意~ Task::kAlive=Task::kAliveOff,与theTask->fEvents作&运算后仍是theTask->fEvents */
                    if (theTask->fEvents &~ Task::kAlive)
                        qtss_printf ("TaskThread::Entry flags still set  before delete\n");

                    (void)atomic_sub(&theTask->fEvents, 0);
                    
					/* 在fTaskName后面附加" deleted" */
                    ::strncat (theTask->fTaskName, " deleted", sizeof(theTask->fTaskName) -1);
                }
				/* 将fTaskName的第一个字符设置为D */
                theTask->fTaskName[0] = 'D'; //mark as dead
                delete theTask;
                theTask = NULL;
                doneProcessingEvent = true;

            }
            else if (theTimeout == 0)
            {
                //We want to make sure that 100% definitely the task's Run function WILL
                //be invoked when another thread calls Signal. We also want to make sure
                //that if an event sneaks in right(恰好悄悄进来) as the task is returning from Run()
                //(via Signal) that the Run function will be invoked again.
                doneProcessingEvent = compare_and_store(Task::kAlive, 0, &theTask->fEvents);
                if (doneProcessingEvent)
                    theTask = NULL; 
            }
            else /* if (theTimeout > 0) */
            {
                //note that if we get here, we don't reset theTask, so it will get passed into
                //WaitForTask
                if (TASK_DEBUG) qtss_printf("TaskThread::Entry insert TaskName=%s in timer heap thread=%lu elem=%lu task=%ld timeout=%.2f\n", theTask->fTaskName,  (UInt32) this, (UInt32) &theTask->fTimerHeapElem,(SInt32) theTask, (float)theTimeout / (float) 1000);
                /* 设置timer Heap elem */
				theTask->fTimerHeapElem.SetValue(OS::Milliseconds() + theTimeout);
				/* 将刚才设置的timer Heap elem放入OSHeap中轮训 */
                fHeap.Insert(&theTask->fTimerHeapElem);
				/* 改变第一个参数fEvents的值,加上一个Idle bit位,标记该Task是个Idle Task */
                (void)atomic_or(&theTask->fEvents, Task::kIdleEvent);
                doneProcessingEvent = true;
            }
        
        
        #if TASK_DEBUG
        SInt64  yieldStart = OS::Milliseconds();
        #endif
        
		/* 产生一个线程 */
        this->ThreadYield();
        #if TASK_DEBUG
        SInt64  yieldDur = OS::Milliseconds() - yieldStart;
        static SInt64   numZeroYields;
        
        if ( yieldDur > 1 )
        {
            if (TASK_DEBUG) qtss_printf( "TaskThread::Entry time in Yield %i, numZeroYields %i\n", (long)yieldDur, (long)numZeroYields );
            numZeroYields = 0;
        }
        else
            numZeroYields++;
        #endif
        
        }
    }
}

/* 根据OSHeap中的时间,以轮询方式等待Task,或者删去并返回超时等待的任务 */
Task* TaskThread::WaitForTask()
{
    while (true)
    {
		/* 获取当前时间 */
        SInt64 theCurrentTime = OS::Milliseconds();
        
		/* 假如OSHeap中的最小OSHeapElement元非空,且大小不超过当前时间(任务已经到执行时间),(以轮询方式)返回最小元对应的任务 */
        if ((fHeap.PeekMin() != NULL) && (fHeap.PeekMin()->GetValue() <= theCurrentTime))
        {    
            if (TASK_DEBUG) qtss_printf("TaskThread::WaitForTask found timer-task=%s thread %lu fHeap.CurrentHeapSize(%lu) taskElem = %lu enclose=%lu\n",((Task*)fHeap.PeekMin()->GetEnclosingObject())->fTaskName, (UInt32) this, fHeap.CurrentHeapSize(), (UInt32) fHeap.PeekMin(), (UInt32) fHeap.PeekMin()->GetEnclosingObject());
            return (Task*)fHeap.ExtractMin()->GetEnclosingObject();
        }
    
        //if there is an element waiting for a timeout, figure out how long we should wait.
		/* 当存在比当前时间大的元(尚未到任务执行时间),计算(需要等待)超时等待的时间 */
        SInt64 theTimeout = 0;
        if (fHeap.PeekMin() != NULL)
            theTimeout = fHeap.PeekMin()->GetValue() - theCurrentTime;
        Assert(theTimeout >= 0);
        
        //
        // Make sure we can't go to sleep for some ridiculously short
        // period of time
        // Do not allow a timeout below 10 ms without first verifying reliable udp 1-2mbit live streams. 
        // Test with streamingserver.xml pref reliablUDP printfs enabled and look for packet loss and check client for  buffer ahead recovery.
	    /* 考虑到R-UDP,注意超时最小就是10ms */
		if (theTimeout < 10) 
           theTimeout = 10;
            
        //wait...
		/* 删去并返回当前任务队列中锚点元素的前一个队列元素(任务) */
		/* 等待theTimeout 时间后从堆中取出任务返回 */
        OSQueueElem* theElem = fTaskQueue.DeQueueBlocking(this, (SInt32) theTimeout);
        if (theElem != NULL)
        {    
            if (TASK_DEBUG) qtss_printf("TaskThread::WaitForTask found signal-task=%s thread %lu fTaskQueue.GetLength(%lu) taskElem = %lu enclose=%lu\n", ((Task*)theElem->GetEnclosingObject())->fTaskName,  (UInt32) this, fTaskQueue.GetQueue()->GetLength(), (UInt32)  theElem,  (UInt32)theElem->GetEnclosingObject() );
            return (Task*)theElem->GetEnclosingObject();
        }

        //
        // If we are supposed to stop, return NULL, which signals the caller to stop
        if (OSThread::GetCurrent()->IsStopRequested())
            return NULL;
    }   
}

/* 重置TaskThreadPool类的数据成员的值,下面会及时更新它们的值 */
TaskThread** TaskThreadPool::sTaskThreadArray = NULL;
UInt32       TaskThreadPool::sNumTaskThreads = 0;

/* 创建指定大小的任务线程数组并开启,返回true */
Bool16 TaskThreadPool::AddThreads(UInt32 numToAdd)
{
	/* 确保sTaskThreadArray非空,并创建一个指定大小的sTaskThreadArray */
    Assert(sTaskThreadArray == NULL);
    sTaskThreadArray = new TaskThread*[numToAdd];
     
	/* 逐个创建任务线程,并开启该线程 */
    for (UInt32 x = 0; x < numToAdd; x++)
    {
        sTaskThreadArray[x] = NEW TaskThread();
		/* 开启新建的任务线程,参见OSThread::Start() */
        sTaskThreadArray[x]->Start();
    }
	/* 更新数据成员 */
    sNumTaskThreads = numToAdd;
    return true;
}

/* 逐个发送停止请求,传信,删去各个线程 */
void TaskThreadPool::RemoveThreads()
{
    //Tell all the threads to stop
    for (UInt32 x = 0; x < sNumTaskThreads; x++)
		/* 发送请求停止所有的线程 */
        sTaskThreadArray[x]->SendStopRequest();

    //Because any (or all) threads may be blocked(阻塞) on the queue, cycle through
    //all the threads, signalling(传信) each one
    for (UInt32 y = 0; y < sNumTaskThreads; y++)
		/* 参见OSQueue::GetCond(),逐个传信 */
        sTaskThreadArray[y]->fTaskQueue.GetCond()->Signal();
    
    //Ok, now wait for the selected threads to terminate, deleting them and removing
    //them from the queue.
    for (UInt32 z = 0; z < sNumTaskThreads; z++)
        delete sTaskThreadArray[z];
    
    sNumTaskThreads = 0;
}
