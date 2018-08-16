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

	/* �������ݳ�Ա���ڵ��� */
	fTaskQueueElem.SetEnclosingObject(this);
	fTimerHeapElem.SetEnclosingObject(this);

}

/* used by RTPSession::RTPSession() */
/* ��Task����Ϊ�ܳ���Ϊ48���ַ���"live_**************" */
void Task::SetTaskName(char* name) 
{
    if (name == NULL) 
        return;
   
   ::strncpy(fTaskName,sTaskStateStr,sizeof(fTaskName));
   ::strncat(fTaskName,name,sizeof(fTaskName));
   /* ����Ľض��Ա���48���ֽڳ��� */
   fTaskName[sizeof(fTaskName) -1] = 0; //terminate in case it is longer than ftaskname.
   
}

/* �ж��������Ƿ�Ϸ�,����boolֵ */
Bool16 Task::Valid()
{
	/* ����fTaskNameΪ�ջ�ǰ5���ַ�����"live_",�����������ʾ */
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
/* ������ԭʼ��event bit���(��λbitΪ0),ͬʱ��fEvents��ΪkAlive��0 */
Task::EventFlags Task::GetEvents()
{
    //Mask off every event currently in the mask except for the alive bit, of course,
    //which should remain unaffected and unreported by this call.
	/* ����alive��0,fEventsֵδ�� */
    EventFlags events = fEvents & kAliveOff;
	/* ����fEvents,����kAliveOff */
	/* fEvents��Ϊ0,eventsû��,����fEventsԭ����ֵ */
    (void)atomic_sub(&fEvents, events);//fEvents��ΪkAlive��0
    return events;//������ԭʼ��event bit���(��λbitΪ0)
}

/* �ǳ���Ҫ�ĺ���֮һ */
/* ����ָ����event flags,֪ͨ��Ӧ��Task,��Ϊ��ǰ����ָ��һ���߳�,�������̵߳�Task Queue�� */
void Task::Signal(EventFlags events)
{
	/* �ȼ��������fTaskName�Ƿ�Ϸ�?���Ϸ������������� */
    if (!this->Valid())
        return;
        
    //Fancy no mutex implementation. We atomically mask the new events into
    //the event mask. Because atomic_or returns the old state of the mask,
    //we only schedule this task once(���ȸ�����һ��).
	/* ������alive flag,��Ǹ�eventflag��active��� */
    events |= kAlive;
	/* ����ԭfEvents(oldEvents��ֵ),ͬʱ�ں����н�����ֵ�ı�Ϊ����ָ����flagλ */
    EventFlags oldEvents = atomic_or(&fEvents, events);
	/* ����ԭ����event�Ƿ�alive(�ϲ���ȷ����alive��),ע��oldEvents & kAlive=0x0 */
    if ((!(oldEvents & kAlive)) && (TaskThreadPool::sNumTaskThreads > 0))
    {
		/* ���統ǰ�����Ѿ�ָ����һ���߳� */
        if (fUseThisThread != NULL)
            // Task needs to be placed on a particular thread.
         {
            if (TASK_DEBUG) if (fTaskName[0] == 0) ::strcpy(fTaskName, " corrupt task");
            if (TASK_DEBUG) qtss_printf("Task::Signal enque TaskName=%s fUseThisThread=%lu q elem=%lu enclosing=%lu\n", fTaskName, (UInt32) fUseThisThread, (UInt32) &fTaskQueueElem, (UInt32) this);
			/* ���������Ӧ�Ķ���Ԫָ����뵱ǰ�����߳����ڵ�Task���� */
            fUseThisThread->fTaskQueue.EnQueue(&fTaskQueueElem);
        }
        else /* ���������ѯ��ʽ���������һ���߳� */
        {
            //find a thread to put this task on
			/* ����ǰ�߳�ID��1��module�߳�����,������ѯ(round robin)��ʽ����һ���߳� */
            unsigned int theThread = atomic_add(&sThreadPicker, 1);
            theThread %= TaskThreadPool::sNumTaskThreads;
			/* Ԥ�ȴ�����,ֻ����TASK_DEBUGʱ���� */
            if (TASK_DEBUG) if (fTaskName[0] == 0) ::strcpy(fTaskName, " corrupt task");
            if (TASK_DEBUG) qtss_printf("Task::Signal enque TaskName=%s thread=%lu q elem=%lu enclosing=%lu\n", fTaskName, (UInt32)TaskThreadPool::sTaskThreadArray[theThread],(UInt32) &fTaskQueueElem,(UInt32) this);
            /* ���������Ӧ�Ķ���Ԫָ����뵱ǰ�����߳����ڵ�Task���� */
			TaskThreadPool::sTaskThreadArray[theThread]->fTaskQueue.EnQueue(&fTaskQueueElem);
        }
    }
    else/* ����ԭ����Task����alive��,��ɶ�²���! */
        if (TASK_DEBUG) qtss_printf("Task::Signal sent to dead TaskName=%s  q elem=%lu  enclosing=%lu\n",  fTaskName, (UInt32) &fTaskQueueElem, (UInt32) this);
        

}

/* ��д��ʱ,���н��� */
void Task::GlobalUnlock()    
{   
    if (this->fWriteLock)
    {   this->fWriteLock = false;   
        TaskThreadPool::sMutexRW.Unlock();
    }                                               
}


/* ����WaitForTask()����������,���ȴ��������,����Task::Run(),���ݺ�������ֵ����������:�����ظ�ֵʱ,����ɾ��������;��Ϊ0ʱ,����doneProcessingEvent=0;��Ϊ��ֵʱ,����TimerHeapElem����ѯ���� */
void TaskThread::Entry()
{
	//��Taskָ��
    Task* theTask = NULL;
    
	//�߳�ѭ��ִ��
	//����Ƿ�����Ҫִ�е���������оͷ��ظ����񣻷���������
    while (true) 
    {
		/* ����WaitForTask()����������,��ȡ�ȴ�������,��������� */
        theTask = this->WaitForTask();

        //
        // WaitForTask returns NULL when it is time to quit
		/* ����û�еȵ������õ������񲻺Ϸ�,������������ */
        if (theTask == NULL || false == theTask->Valid() )
            return;
          
		//��δ�����¼�
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
			/* ��û����Ӧ���߳�,�������� */
            theTask->fUseThisThread = NULL; // Each invocation of Run must independently
                                            // request a specific thread.
            SInt64 theTimeout = 0;
            
			/* ���������run()���� */
            if (theTask->fWriteLock)
            {   
                OSMutexWriteLocker mutexLocker(&TaskThreadPool::sMutexRW);
                if (TASK_DEBUG) qtss_printf("TaskThread::Entry run global locked TaskName=%s CurMSec=%.3f thread=%ld task=%ld\n", theTask->fTaskName, OS::StartTimeMilli_Float() ,(SInt32) this,(SInt32) theTask);
                
				/* ��ȫ������������ */
                theTimeout = theTask->Run();
				/* ���� */
                theTask->fWriteLock = false;
            }
            else
            {
                OSMutexReadLocker mutexLocker(&TaskThreadPool::sMutexRW);
                if (TASK_DEBUG) qtss_printf("TaskThread::Entry run TaskName=%s CurMSec=%.3f thread=%ld task=%ld\n", theTask->fTaskName, OS::StartTimeMilli_Float(), (SInt32) this,(SInt32) theTask);

				/* ����Task��Run() */
                theTimeout = theTask->Run();
            
            }
#if DEBUG
            Assert(this->GetNumLocksHeld() == 0);
            theTask->fInRunCount--;
            Assert(theTask->fInRunCount == 0);
#endif       
			/* ��Task::Run()�ķ���ֵ����������: */
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
                    
					/* ע��~ Task::kAlive=Task::kAliveOff,��theTask->fEvents��&���������theTask->fEvents */
                    if (theTask->fEvents &~ Task::kAlive)
                        qtss_printf ("TaskThread::Entry flags still set  before delete\n");

                    (void)atomic_sub(&theTask->fEvents, 0);
                    
					/* ��fTaskName���渽��" deleted" */
                    ::strncat (theTask->fTaskName, " deleted", sizeof(theTask->fTaskName) -1);
                }
				/* ��fTaskName�ĵ�һ���ַ�����ΪD */
                theTask->fTaskName[0] = 'D'; //mark as dead
                delete theTask;
                theTask = NULL;
                doneProcessingEvent = true;

            }
            else if (theTimeout == 0)
            {
                //We want to make sure that 100% definitely the task's Run function WILL
                //be invoked when another thread calls Signal. We also want to make sure
                //that if an event sneaks in right(ǡ�����Ľ���) as the task is returning from Run()
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
                /* ����timer Heap elem */
				theTask->fTimerHeapElem.SetValue(OS::Milliseconds() + theTimeout);
				/* ���ղ����õ�timer Heap elem����OSHeap����ѵ */
                fHeap.Insert(&theTask->fTimerHeapElem);
				/* �ı��һ������fEvents��ֵ,����һ��Idle bitλ,��Ǹ�Task�Ǹ�Idle Task */
                (void)atomic_or(&theTask->fEvents, Task::kIdleEvent);
                doneProcessingEvent = true;
            }
        
        
        #if TASK_DEBUG
        SInt64  yieldStart = OS::Milliseconds();
        #endif
        
		/* ����һ���߳� */
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

/* ����OSHeap�е�ʱ��,����ѯ��ʽ�ȴ�Task,����ɾȥ�����س�ʱ�ȴ������� */
Task* TaskThread::WaitForTask()
{
    while (true)
    {
		/* ��ȡ��ǰʱ�� */
        SInt64 theCurrentTime = OS::Milliseconds();
        
		/* ����OSHeap�е���СOSHeapElementԪ�ǿ�,�Ҵ�С��������ǰʱ��(�����Ѿ���ִ��ʱ��),(����ѯ��ʽ)������СԪ��Ӧ������ */
        if ((fHeap.PeekMin() != NULL) && (fHeap.PeekMin()->GetValue() <= theCurrentTime))
        {    
            if (TASK_DEBUG) qtss_printf("TaskThread::WaitForTask found timer-task=%s thread %lu fHeap.CurrentHeapSize(%lu) taskElem = %lu enclose=%lu\n",((Task*)fHeap.PeekMin()->GetEnclosingObject())->fTaskName, (UInt32) this, fHeap.CurrentHeapSize(), (UInt32) fHeap.PeekMin(), (UInt32) fHeap.PeekMin()->GetEnclosingObject());
            return (Task*)fHeap.ExtractMin()->GetEnclosingObject();
        }
    
        //if there is an element waiting for a timeout, figure out how long we should wait.
		/* �����ڱȵ�ǰʱ����Ԫ(��δ������ִ��ʱ��),����(��Ҫ�ȴ�)��ʱ�ȴ���ʱ�� */
        SInt64 theTimeout = 0;
        if (fHeap.PeekMin() != NULL)
            theTimeout = fHeap.PeekMin()->GetValue() - theCurrentTime;
        Assert(theTimeout >= 0);
        
        //
        // Make sure we can't go to sleep for some ridiculously short
        // period of time
        // Do not allow a timeout below 10 ms without first verifying reliable udp 1-2mbit live streams. 
        // Test with streamingserver.xml pref reliablUDP printfs enabled and look for packet loss and check client for  buffer ahead recovery.
	    /* ���ǵ�R-UDP,ע�ⳬʱ��С����10ms */
		if (theTimeout < 10) 
           theTimeout = 10;
            
        //wait...
		/* ɾȥ�����ص�ǰ���������ê��Ԫ�ص�ǰһ������Ԫ��(����) */
		/* �ȴ�theTimeout ʱ���Ӷ���ȡ�����񷵻� */
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

/* ����TaskThreadPool������ݳ�Ա��ֵ,����ἰʱ�������ǵ�ֵ */
TaskThread** TaskThreadPool::sTaskThreadArray = NULL;
UInt32       TaskThreadPool::sNumTaskThreads = 0;

/* ����ָ����С�������߳����鲢����,����true */
Bool16 TaskThreadPool::AddThreads(UInt32 numToAdd)
{
	/* ȷ��sTaskThreadArray�ǿ�,������һ��ָ����С��sTaskThreadArray */
    Assert(sTaskThreadArray == NULL);
    sTaskThreadArray = new TaskThread*[numToAdd];
     
	/* ������������߳�,���������߳� */
    for (UInt32 x = 0; x < numToAdd; x++)
    {
        sTaskThreadArray[x] = NEW TaskThread();
		/* �����½��������߳�,�μ�OSThread::Start() */
        sTaskThreadArray[x]->Start();
    }
	/* �������ݳ�Ա */
    sNumTaskThreads = numToAdd;
    return true;
}

/* �������ֹͣ����,����,ɾȥ�����߳� */
void TaskThreadPool::RemoveThreads()
{
    //Tell all the threads to stop
    for (UInt32 x = 0; x < sNumTaskThreads; x++)
		/* ��������ֹͣ���е��߳� */
        sTaskThreadArray[x]->SendStopRequest();

    //Because any (or all) threads may be blocked(����) on the queue, cycle through
    //all the threads, signalling(����) each one
    for (UInt32 y = 0; y < sNumTaskThreads; y++)
		/* �μ�OSQueue::GetCond(),������� */
        sTaskThreadArray[y]->fTaskQueue.GetCond()->Signal();
    
    //Ok, now wait for the selected threads to terminate, deleting them and removing
    //them from the queue.
    for (UInt32 z = 0; z < sNumTaskThreads; z++)
        delete sTaskThreadArray[z];
    
    sNumTaskThreads = 0;
}
