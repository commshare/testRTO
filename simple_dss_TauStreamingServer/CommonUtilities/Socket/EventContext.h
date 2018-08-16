
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 EventContext.h
Description: An event context provides the intelligence to take an event (usually EV_RE or EV_WR)
             generated from a UNIX file descriptor and signal a Task.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef __EVENT_CONTEXT_H__
#define __EVENT_CONTEXT_H__

#include "Task.h"
#include "OSThread.h"
#include "OSRef.h" /* 用于描述Socket上发生的event的unique ID */
#include "ev.h" /* 主要关于struct eventreq和select()密切相关的几个函数 */



//enable to trace event context execution and the task associated with the context
#define EVENTCONTEXT_DEBUG 0



class EventThread;

class EventContext
{
    public:
    
        //
        // Constructor. Pass in the EventThread you would like to receive
        // events for this context, and the fd(Socket描述符) that this context applies to
        EventContext(int inFileDesc, EventThread* inThread);
        virtual ~EventContext() { if (fAutoCleanup) this->Cleanup(); }/* 清除socket描述符 */
        
        //
        // InitNonBlocking
        //
        // Sets inFileDesc to be non-blocking. Once this is called, the
        // EventContext object "owns" the file descriptor, and will close it
        // when Cleanup is called. This is necessary because of some weird
        // select() behavior. DON'T CALL CLOSE ON THE FD ONCE THIS IS CALLED!!!!
		/* 将入参inFileDesc文件描述符设为非阻塞的,一旦调用这个函数,就不要关闭FD */
		/* 将指定socket设置成非阻塞模式 */
        void            InitNonBlocking(int inFileDesc);

        //
        // Cleanup. Will be called by the destructor, but can be called earlier
		/* 通过描述socket的标识符判断,若合法就从HashTable中删去该socket,否则设置相关参数 */
        void            Cleanup();

        //
        // Arms this EventContext. Pass in the events you would like to receive
        void            RequestEvent(int theMask = EV_RE);

        
        //
        // Provide the task you would like to be notified
        void            SetTask(Task* inTask)
        {  
            fTask = inTask; 
            if (EVENTCONTEXT_DEBUG)
            {
                if (fTask== NULL)  
                    qtss_printf("EventContext::SetTask context=%lu task= NULL\n", (UInt32) this); 
                else 
                    qtss_printf("EventContext::SetTask context=%lu task= %lu name=%s\n",(UInt32) this,(UInt32) fTask, fTask->fTaskName); 
            }
        }
        
        // when the HTTP Proxy tunnels takes over a TCPSocket, we need to maintain this context too
		/* snarf 窃取,这里是复制 */
        void            SnarfEventContext( EventContext &fromContext );
        
        // Don't cleanup this socket automatically
        void            DontAutoCleanup() { fAutoCleanup = false; }
        
        // Direct access to the FD(文件描述符) is not recommended, but is needed for modules
        // that want to use the Socket classes and need to request events(请求事件) on the fd.
		/* 获取socket的描述符 */
        int             GetSocketFD()       { return fFileDesc; }
        
		/* 非法的socket描述符 */
        enum
        {
            kInvalidFileDesc = -1   //int
        };

    protected:

        //
        // ProcessEvent
        //
        // When an event occurs on this file descriptor(文件描述符), this function
        // will get called. Default behavior is to Signal(触发) the associated
        // task, but that behavior may be altered / overridden.
        //
        // Currently, we always generate a Task::kReadEvent
		/* 若fTask存在,就触发Task::kReadEvent,这里默认的入参是任意值 */
		virtual void ProcessEvent(int /*eventBits*/) 
        {   
            if (EVENTCONTEXT_DEBUG)
            {
                if (fTask== NULL)  
                    qtss_printf("EventContext::ProcessEvent context=%lu task=NULL\n",(UInt32) this); 
                else 
                    qtss_printf("EventContext::ProcessEvent context=%lu task=%lu TaskName=%s\n",(UInt32)this,(UInt32) fTask, fTask->fTaskName); 
            }

            if (fTask != NULL)
				/* 总是传信ReadEvent */
                fTask->Signal(Task::kReadEvent); 
        }

		/* File Descriptor,用来标识一个Socket的描述符 */
        int             fFileDesc;

    private:

		/* event request结构体,描述Socket上发生的event信息.参见ev.h */
		/* 这个在EventContext中非常重要! 被Task::GetEvent()调用 */
		/***************** NOTE!! ***********************/
		//注册的event request
        struct eventreq fEventReq;
        
		/* Hash Table元引用,参见OSRef.h */
        OSRef           fRef;
		/* 唯一的event/Message ID */
        PointerSizedInt fUniqueID; /* 指针大小为4字节 */
		/* 设置key值的字符串,用于在OSRef中查询 */
        StrPtrLen       fUniqueIDStr;

		/* 参见下面的定义,注意两者相互包含 */
        EventThread*    fEventThread;

		/* call select_watchevent()? */
		/* 监视请求的事件吗? */
        Bool16          fWatchEventCalled;
		/* 网络事件位掩码 */
        int             fEventBits;
		/* 自动清除事件吗? */
        Bool16          fAutoCleanup;

		/* 参见Task.h,在EventContext::ProcessEvent()中调用Task::Signal()将该event相应Task放入相应TaskThread的Task队列 */
        Task*           fTask;
#if DEBUG
		/* 调用了select_modwatch()吗? 要监视指定Socket端口上的事件是否改变吗? */
        Bool16          fModwatched;
#endif
        /* 唯一的event/Message ID,用来设置fUniqueID.参见EventContext.cpp,要用到OSRef.h/cpp.cf fUniqueID */
        static unsigned int sUniqueID;
        
        friend class EventThread;
};

class EventThread : public OSThread
{
    public:
    
        EventThread() : OSThread() {}
        virtual ~EventThread() {}
    
    private:
    
		/***************** NOTE ***************************/
		/* 创建窗口,获取Message,当有数据在Socket上等待时,将该数据加入Hash table中,并触发(signal)指定的网络事件,并减少引用计数,通知其它等待线程 */
        virtual void Entry();

		/* 用来描述发生在Socket上的unique ID */
        OSRefTable      fRefTable;
        
        friend class EventContext;
};

#endif //__EVENT_CONTEXT_H__
