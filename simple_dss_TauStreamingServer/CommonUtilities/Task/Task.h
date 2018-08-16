
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
            kKillEvent =    0x1 << 0x0, //left-shift operator, how to interpret here?左移多少bit
            kIdleEvent =    0x1 << 0x1,
            kStartEvent =   0x1 << 0x2,
            kTimeoutEvent = 0x1 << 0x3,/* 左移3个bit */
       
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
        // >0-> invoke(调用) me after this number of MilSecs with a kIdleEvent
        // 0 don't reinvoke me at all.(绝不要再调用)
        //-1 delete me
        //Suggested practice is that any task should be deleted by returning true from the
        //Run function. That way, we know that the Task is not running at the time it is
        //deleted. This object provides no protection against calling a method, such as Signal,
        //at the same time the object is being deleted (because it can't really), so watch
        //those dangling references!(当心引用挂空)

		/************* NOTE Important! **************/
		/* 具体执行由下面各派生类实现,如TCPListenerSocket::Run() */
        virtual SInt64          Run() = 0;
        
        //Send an event to this task.
        void                    Signal(EventFlags eventFlags);
        void                    GlobalUnlock();

		/* 判断任务名是否合法,返回bool值 */
        Bool16                  Valid(); // for debugging
		char            fTaskName[48]; /* 长度为48的字符串,由Task::SetTaskName()设置 */
		void            SetTaskName(char* name); /* 定义见Task.cpp */
        
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
                                                        fUseThisThread = (TaskThread*)OSThread::GetCurrent();/* 获取当前任务线程 */
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

		/* 标记该Task的状态,event是active还是dead? */
        enum
        {
            kAlive =            0x80000000, //EventFlags, again
            kAliveOff =         0x7fffffff
        };

        void            SetTaskThread(TaskThread *thread);
        
		/* data members */

        EventFlags      fEvents; /* event flag */
		/* 当前任务已经指定了一个线程 */
        TaskThread*     fUseThisThread;/* 所用task thread指针 */
		/* if write lock ? */
        Bool16          fWriteLock;

#if DEBUG
		/* 一个Task的前提是它的Run()函数不能重入 */
        //The whole premise of a task is that the Run function cannot be re-entered.
        //This debugging variable ensures that that is always the case
        volatile UInt32 fInRunCount;
#endif

        //This could later be optimized by using a timing wheel(计时轮) instead of a heap(堆),
        //and that way we wouldn't need both a heap elem and a queue elem here (just queue elem)
		/* 计时器堆元 */
        OSHeapElem      fTimerHeapElem;

		/* 任务队列元,每个任务作为一个Task Queue中的元素 */
        OSQueueElem     fTaskQueueElem;
        
        //Variable used for assigning tasks to threads in a round-robin fashion(轮询方式)
		/* 以轮询方式将task分配给thread的变量,used in Task::Signal() */
        static unsigned int sThreadPicker;//初始值为0，与线程ID相关
        
        friend class    TaskThread; 
};

class TaskThread : public OSThread  //refer to OSThread.h
{
    public:
    
        //Implementation detail: all tasks get run on TaskThreads.
        
                        TaskThread() :  OSThread(), fTaskThreadPoolElem()
                                        {fTaskThreadPoolElem.SetEnclosingObject(this);}/* 将任务线程池的元素设为当前Task Thread */
						virtual         ~TaskThread() { this->StopAndWaitForThread(); }
           
    private:
    
		/* 最小的等待时间是10ms */
        enum
        {
            kMinWaitTimeInMilSecs = 10  //UInt32
        };

		/* member functions */

		/* 本类中最重要的一个函数! */
		/* 调用WaitForTask()等待到任务后,启动Task::Run(),根据函数返回值分情形讨论:当返回负值时,立即删除该任务;当为0时,设置doneProcessingEvent=0;当为正值时,重置TimerHeapElem，轮询任务 */
        virtual void    Entry();
		/* 根据OSHeap中的时间,以轮询方式等待Task,或者删去并返回超时等待的任务 */
        Task*           WaitForTask();

		/* data members */
        /* Task Thread作为Task Thread pool中的元素 */
        OSQueueElem     fTaskThreadPoolElem;
        
		/* 需要详细参考OSHeap.cpp */
		/* 用于时间轮询 */
		//纪录任务运行时间的堆，用于WaitForTask 函数
        OSHeap              fHeap;

		/*关键数据结构：阻塞的任务队列；在Task 的Signal 函数中直接调用
		fTaskQueue 对象的EnQueue 函数将自己加入任务队列*/
		/* 该Task Thread要处理的可能阻塞的任务队列,用到OSCond::Signal() */
		/* 在每个任务线程对象内部都有一个OSQueue_Blocking 类型的任
		务队列，存储该线程需要执行的任务 */
        OSQueue_Blocking    fTaskQueue;
        
        
        friend class Task;
        friend class TaskThreadPool;
};

//Because task threads share a global queue of tasks to execute,
//there can only be one pool of task threads(任务线程池). That is why this object
//is static.
/* 注意很有意思,该类的所有函数及数据成员都是static,因而没有构造和析构函数 */
class TaskThreadPool 
{
public:

    //Adds some threads to the pool
	/* 创建指定大小的任务线程数组,返回true */
    static Bool16   AddThreads(UInt32 numToAdd);
    static void     SwitchPersonality( char *user = NULL, char *group = NULL);
	/* 逐个发送停止请求,传信,删去各个线程 */
    static void     RemoveThreads();
    
private:

	/* 任务线程数组 */
    static TaskThread**     sTaskThreadArray;
	/* 任务线程总数,大小有限制 */
    static UInt32           sNumTaskThreads;
    static OSMutexRW        sMutexRW;
    
    friend class Task;
    friend class TaskThread;
};

#endif /* __TASK_H__ */
