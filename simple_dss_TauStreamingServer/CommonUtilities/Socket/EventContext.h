
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
#include "OSRef.h" /* ��������Socket�Ϸ�����event��unique ID */
#include "ev.h" /* ��Ҫ����struct eventreq��select()������صļ������� */



//enable to trace event context execution and the task associated with the context
#define EVENTCONTEXT_DEBUG 0



class EventThread;

class EventContext
{
    public:
    
        //
        // Constructor. Pass in the EventThread you would like to receive
        // events for this context, and the fd(Socket������) that this context applies to
        EventContext(int inFileDesc, EventThread* inThread);
        virtual ~EventContext() { if (fAutoCleanup) this->Cleanup(); }/* ���socket������ */
        
        //
        // InitNonBlocking
        //
        // Sets inFileDesc to be non-blocking. Once this is called, the
        // EventContext object "owns" the file descriptor, and will close it
        // when Cleanup is called. This is necessary because of some weird
        // select() behavior. DON'T CALL CLOSE ON THE FD ONCE THIS IS CALLED!!!!
		/* �����inFileDesc�ļ���������Ϊ��������,һ�������������,�Ͳ�Ҫ�ر�FD */
		/* ��ָ��socket���óɷ�����ģʽ */
        void            InitNonBlocking(int inFileDesc);

        //
        // Cleanup. Will be called by the destructor, but can be called earlier
		/* ͨ������socket�ı�ʶ���ж�,���Ϸ��ʹ�HashTable��ɾȥ��socket,����������ز��� */
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
		/* snarf ��ȡ,�����Ǹ��� */
        void            SnarfEventContext( EventContext &fromContext );
        
        // Don't cleanup this socket automatically
        void            DontAutoCleanup() { fAutoCleanup = false; }
        
        // Direct access to the FD(�ļ�������) is not recommended, but is needed for modules
        // that want to use the Socket classes and need to request events(�����¼�) on the fd.
		/* ��ȡsocket�������� */
        int             GetSocketFD()       { return fFileDesc; }
        
		/* �Ƿ���socket������ */
        enum
        {
            kInvalidFileDesc = -1   //int
        };

    protected:

        //
        // ProcessEvent
        //
        // When an event occurs on this file descriptor(�ļ�������), this function
        // will get called. Default behavior is to Signal(����) the associated
        // task, but that behavior may be altered / overridden.
        //
        // Currently, we always generate a Task::kReadEvent
		/* ��fTask����,�ʹ���Task::kReadEvent,����Ĭ�ϵ����������ֵ */
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
				/* ���Ǵ���ReadEvent */
                fTask->Signal(Task::kReadEvent); 
        }

		/* File Descriptor,������ʶһ��Socket�������� */
        int             fFileDesc;

    private:

		/* event request�ṹ��,����Socket�Ϸ�����event��Ϣ.�μ�ev.h */
		/* �����EventContext�зǳ���Ҫ! ��Task::GetEvent()���� */
		/***************** NOTE!! ***********************/
		//ע���event request
        struct eventreq fEventReq;
        
		/* Hash TableԪ����,�μ�OSRef.h */
        OSRef           fRef;
		/* Ψһ��event/Message ID */
        PointerSizedInt fUniqueID; /* ָ���СΪ4�ֽ� */
		/* ����keyֵ���ַ���,������OSRef�в�ѯ */
        StrPtrLen       fUniqueIDStr;

		/* �μ�����Ķ���,ע�������໥���� */
        EventThread*    fEventThread;

		/* call select_watchevent()? */
		/* ����������¼���? */
        Bool16          fWatchEventCalled;
		/* �����¼�λ���� */
        int             fEventBits;
		/* �Զ�����¼���? */
        Bool16          fAutoCleanup;

		/* �μ�Task.h,��EventContext::ProcessEvent()�е���Task::Signal()����event��ӦTask������ӦTaskThread��Task���� */
        Task*           fTask;
#if DEBUG
		/* ������select_modwatch()��? Ҫ����ָ��Socket�˿��ϵ��¼��Ƿ�ı���? */
        Bool16          fModwatched;
#endif
        /* Ψһ��event/Message ID,��������fUniqueID.�μ�EventContext.cpp,Ҫ�õ�OSRef.h/cpp.cf fUniqueID */
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
		/* ��������,��ȡMessage,����������Socket�ϵȴ�ʱ,�������ݼ���Hash table��,������(signal)ָ���������¼�,���������ü���,֪ͨ�����ȴ��߳� */
        virtual void Entry();

		/* ��������������Socket�ϵ�unique ID */
        OSRefTable      fRefTable;
        
        friend class EventContext;
};

#endif //__EVENT_CONTEXT_H__
