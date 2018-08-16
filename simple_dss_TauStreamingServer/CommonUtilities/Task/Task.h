
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


#ifndef __TASK_H__
#define __TASK_H__

#include "OSQueue.h"
#include "OSHeap.h"
#include "OSThread.h"
#include "OSMutexRW.h"

#define TASK_DEBUG 0

class  TaskThread;  
class Task
{
    public:
        
        typedef unsigned int EventFlags;

        //EVENTS
        //here are all the events that can be sent to a task
        enum
        { 
		  //these are all of type "EventFlags"
            kKillEvent =    0x1 << 0x0, //left-shift operator, how to interpret here?���ƶ���bit
            kIdleEvent =    0x1 << 0x1,
            kStartEvent =   0x1 << 0x2,
            kTimeoutEvent = 0x1 << 0x3,/* ����3��bit */
       
          //socket events
            kReadEvent =        0x1 << 0x4, //All of type "EventFlags"
            kWriteEvent =       0x1 << 0x5,
           
           //update event
            kUpdateEvent =      0x1 << 0x6
        };
        
        //CONSTRUCTOR / DESTRUCTOR
        //You must assign priority at create time.
                                Task();
        virtual                 ~Task() {}

        //return:
        // >0-> invoke(����) me after this number of MilSecs with a kIdleEvent
        // 0 don't reinvoke me at all.(����Ҫ�ٵ���)
        //-1 delete me
        //Suggested practice is that any task should be deleted by returning true from the
        //Run function. That way, we know that the Task is not running at the time it is
        //deleted. This object provides no protection against calling a method, such as Signal,
        //at the same time the object is being deleted (because it can't really), so watch
        //those dangling references!(�������ùҿ�)

		/************* NOTE Important! **************/
		/* ����ִ���������������ʵ��,��TCPListenerSocket::Run() */
        virtual SInt64          Run() = 0;
        
        //Send an event to this task.
        void                    Signal(EventFlags eventFlags);
        void                    GlobalUnlock();

		/* �ж��������Ƿ�Ϸ�,����boolֵ */
        Bool16                  Valid(); // for debugging
		char            fTaskName[48]; /* ����Ϊ48���ַ���,��Task::SetTaskName()���� */
		void            SetTaskName(char* name); /* �����Task.cpp */
        
    protected:
    
        //Only the tasks themselves may find out what events they have received
        EventFlags              GetEvents();
        
        // ForceSameThread
        //
        // A task, inside its run function, may want to ensure that the same task thread
        // is used for subsequent calls to Run(). This may be the case if the task is holding
        // a mutex between calls to run. By calling this function, the task ensures that the
        // same task thread will be used for the next call to Run(). It only applies to the
        // next call to run.
        void                    ForceSameThread()   {
                                                        fUseThisThread = (TaskThread*)OSThread::GetCurrent();/* ��ȡ��ǰ�����߳� */
                                                        Assert(fUseThisThread != NULL);/* line 101 */
                                                        if (TASK_DEBUG) if (fTaskName[0] == 0) ::strcpy(fTaskName, " corrupt task");
                                                        if (TASK_DEBUG) qtss_printf("Task::ForceSameThread fUseThisThread %lu task %s enque elem=%lu enclosing %lu\n", (UInt32)fUseThisThread, fTaskName,(UInt32) &fTaskQueueElem,(UInt32) this);
                                                    }
        // used in QTSSModule::Run()
        SInt64                  CallLocked()        {   ForceSameThread();
                                                        fWriteLock = true;
                                                        return (SInt64) 10; // minimum of 10 milliseconds between locks
                                                    }

    private:

		/* ��Ǹ�Task��״̬,event��active����dead? */
        enum
        {
            kAlive =            0x80000000, //EventFlags, again
            kAliveOff =         0x7fffffff
        };

        void            SetTaskThread(TaskThread *thread);
        
		/* data members */

        EventFlags      fEvents; /* event flag */
		/* ��ǰ�����Ѿ�ָ����һ���߳� */
        TaskThread*     fUseThisThread;/* ����task threadָ�� */
		/* if write lock ? */
        Bool16          fWriteLock;

#if DEBUG
		/* һ��Task��ǰ��������Run()������������ */
        //The whole premise of a task is that the Run function cannot be re-entered.
        //This debugging variable ensures that that is always the case
        volatile UInt32 fInRunCount;
#endif

        //This could later be optimized by using a timing wheel(��ʱ��) instead of a heap(��),
        //and that way we wouldn't need both a heap elem and a queue elem here (just queue elem)
		/* ��ʱ����Ԫ */
        OSHeapElem      fTimerHeapElem;

		/* �������Ԫ,ÿ��������Ϊһ��Task Queue�е�Ԫ�� */
        OSQueueElem     fTaskQueueElem;
        
        //Variable used for assigning tasks to threads in a round-robin fashion(��ѯ��ʽ)
		/* ����ѯ��ʽ��task�����thread�ı���,used in Task::Signal() */
        static unsigned int sThreadPicker;//��ʼֵΪ0�����߳�ID���
        
        friend class    TaskThread; 
};

class TaskThread : public OSThread  //refer to OSThread.h
{
    public:
    
        //Implementation detail: all tasks get run on TaskThreads.
        
                        TaskThread() :  OSThread(), fTaskThreadPoolElem()
                                        {fTaskThreadPoolElem.SetEnclosingObject(this);}/* �������̳߳ص�Ԫ����Ϊ��ǰTask Thread */
						virtual         ~TaskThread() { this->StopAndWaitForThread(); }
           
    private:
    
		/* ��С�ĵȴ�ʱ����10ms */
        enum
        {
            kMinWaitTimeInMilSecs = 10  //UInt32
        };

		/* member functions */

		/* ����������Ҫ��һ������! */
		/* ����WaitForTask()�ȴ��������,����Task::Run(),���ݺ�������ֵ����������:�����ظ�ֵʱ,����ɾ��������;��Ϊ0ʱ,����doneProcessingEvent=0;��Ϊ��ֵʱ,����TimerHeapElem����ѯ���� */
        virtual void    Entry();
		/* ����OSHeap�е�ʱ��,����ѯ��ʽ�ȴ�Task,����ɾȥ�����س�ʱ�ȴ������� */
        Task*           WaitForTask();

		/* data members */
        /* Task Thread��ΪTask Thread pool�е�Ԫ�� */
        OSQueueElem     fTaskThreadPoolElem;
        
		/* ��Ҫ��ϸ�ο�OSHeap.cpp */
		/* ����ʱ����ѯ */
		//��¼��������ʱ��Ķѣ�����WaitForTask ����
        OSHeap              fHeap;

		/*�ؼ����ݽṹ��������������У���Task ��Signal ������ֱ�ӵ���
		fTaskQueue �����EnQueue �������Լ������������*/
		/* ��Task ThreadҪ����Ŀ����������������,�õ�OSCond::Signal() */
		/* ��ÿ�������̶߳����ڲ�����һ��OSQueue_Blocking ���͵���
		����У��洢���߳���Ҫִ�е����� */
        OSQueue_Blocking    fTaskQueue;
        
        
        friend class Task;
        friend class TaskThreadPool;
};

//Because task threads share a global queue of tasks to execute,
//there can only be one pool of task threads(�����̳߳�). That is why this object
//is static.
/* ע�������˼,��������к��������ݳ�Ա����static,���û�й������������ */
class TaskThreadPool 
{
public:

    //Adds some threads to the pool
	/* ����ָ����С�������߳�����,����true */
    static Bool16   AddThreads(UInt32 numToAdd);
    static void     SwitchPersonality( char *user = NULL, char *group = NULL);
	/* �������ֹͣ����,����,ɾȥ�����߳� */
    static void     RemoveThreads();
    
private:

	/* �����߳����� */
    static TaskThread**     sTaskThreadArray;
	/* �����߳�����,��С������ */
    static UInt32           sNumTaskThreads;
    static OSMutexRW        sMutexRW;
    
    friend class Task;
    friend class TaskThread;
};

#endif /* __TASK_H__ */
