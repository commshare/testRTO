/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 EventContext.cpp
Description: An event context provides the intelligence to take an event (usually EV_RE or EV_WR)
             generated from a UNIX file descriptor and signal a Task.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "EventContext.h"
#include "OSThread.h"
#include "atomic.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>




#define EVENT_CONTEXT_DEBUG 0

#if EVENT_CONTEXT_DEBUG
#include "OS.h"
#endif

unsigned int EventContext::sUniqueID = 1;


/* ���ΪSocket����������Ӧ���¼��߳� */
EventContext::EventContext(int inFileDesc, EventThread* inThread)
:   fFileDesc(inFileDesc),
    fUniqueID(0),
    fUniqueIDStr((char*)&fUniqueID, sizeof(fUniqueID)),
    fEventThread(inThread),
    fWatchEventCalled(false),
    fAutoCleanup(true)
{}

/* ��ָ��socket���óɷ�����ģʽ */
void EventContext::InitNonBlocking(int inFileDesc)
{
	/* ������ʶһ��Socket�������� */
    fFileDesc = inFileDesc;

    int flag = ::fcntl(fFileDesc, F_GETFL, 0);
    int err = ::fcntl(fFileDesc, F_SETFL, flag | O_NONBLOCK);

	/* �������ʾ����ԭ�� */
    AssertV(err == 0, OSThread::GetErrno());
}

/* ͨ������socket�ı�ʶ���ж�,���Ϸ��ʹ�HashTable��ɾȥ��socket,�Ӷ�����д����ɾ����socket fd,�ر��׽���,����ֱ�ӹر��׽���,������ز��� */
void EventContext::Cleanup()
{
    int err = 0;
    
	/* �����ʶsocket���������Ϸ�,�ʹ�Hash Table��ע����socket */
    if (fFileDesc != kInvalidFileDesc)
    {
        //if this object is registered in the table, unregister it now
        if (fUniqueID > 0)
        {
            fEventThread->fRefTable.UnRegister(&fRef);
			/* �Ӷ�����д����ɾ����socket fd */
            select_removeevent(fFileDesc);//The eventqueue / select shim requires this
        }
        else
            err = ::close(fFileDesc);
    }

	/* ���������Socket�����ز��� */
    fFileDesc = kInvalidFileDesc;
    fUniqueID = 0;
    
    AssertV(err == 0, OSThread::GetErrno());//we don't really care if there was an error, but it's nice to know
}

/* �����������ȡ���ݳ�Ա��ֵ,�������ݳ�ԱfRef����Hash Table��,ɾȥ�����ͬkeyֵ��Ref */
void EventContext::SnarfEventContext( EventContext &fromContext )
{  
    //+ show that we called watchevent
    // copy the unique id
    // set our fUniqueIDStr to the unique id
    // copy the eventreq
    // find the old event object
    // show us as the object in the fRefTable
    //      we take the OSRef from the old context, point it at our context
    //
    //TODO - this whole operation causes a race condition for Event posting
    //  way up the chain we need to disable event posting
    // or copy the posted events after this op completes
    
	/* ����������ø����ݳ�Ա��ֵ */
    fromContext.fFileDesc = kInvalidFileDesc;
    
    fWatchEventCalled = fromContext.fWatchEventCalled; 
    fUniqueID = fromContext.fUniqueID;
    fUniqueIDStr.Set((char*)&fUniqueID, sizeof(fUniqueID)),
    
    ::memcpy( &fEventReq, &fromContext.fEventReq, sizeof( struct eventreq  ) );

	/* ����OSRef::Set()�������ݳ�ԱfRef */
    fRef.Set( fUniqueIDStr, this );
	/* ��fRef�滻��ԭ����ͬkeyֵ��Ref */
    fEventThread->fRefTable.Swap(&fRef);
	/* ɾȥ����е�Ref */
    fEventThread->fRefTable.UnRegister(&fromContext.fRef);
}

/* select_modwatch�ڲ�������WSAsyncSelect(),ͨ��RequestEvent ��������Ը���������ĳЩ�¼�(һ��ΪEV_RE)�ļ��� */

/* ��������select_modwatch()���ָ������sMsgWindow�Ƿ���ָ�������event����?
��������Ψһ��fEventReq��Ϣ��,����select_watchevent()������ָ������sMsgWindow�Ƿ���ָ�������event����?*/
//ע���¼�
void EventContext::RequestEvent(int theMask)
{
#if DEBUG
    fModwatched = true;//���������select_modwatch()
#endif

    //
    // The first time this function gets called, we're supposed to
    // call watchevent. Each subsequent time, call modwatch. That's
    // the way the MacOS X event queue works.
	/* ע����Win32��,watchevent��modwatchû������!,�μ�win32ev.cpp */
    
	/* ��������select_modwatch()���ָ������sMsgWindow�Ƿ���ָ�������event����? */
	/* ����Ѿ����ù�select_watchevent()�������ǵ�һ�ε���  */
    if (fWatchEventCalled)
    {
		/* ���������¼����� */
		//ע��ָ��event(Ĭ��ΪEV_RE)
        fEventReq.er_eventbits = theMask;

		/* ����Windows Socket DLLΪָ������sMsgWindow������ָ��Socket����theEventָ���������¼��Ĵ�����ϢtheMsg */
		/* ����EV_RE,��ָ��eventreq���ļ�������fd�������,��д����ɾ��;
          ����EV_WR,��ָ��eventreq���ļ�������fd����д��,�Ӷ�����ɾ��.
          ��������ļ�������sMaxFDPos,��¼req->er_data��sCookieArray[],дpipe,���Ƿ��غ���ֵ0 */
        if (select_modwatch(&fEventReq, theMask) != 0)
            AssertV(false, OSThread::GetErrno());
    }
	// ����ǵ�һ�����룬��Ҫ����select_watchevent()�ȷ���һ��Ψһ�ı�ʶ��
	// ����fRefע�ᵽfRefTable���Թ�EventThread�ں����������ҵ���Ӧ�Ķ���
    else /* ��������Ψһ��fEventReq��Ϣ��,����select_watchevent()������ָ������sMsgWindow�Ƿ���ָ�������event����? */
    {
        //allocate a Unique ID for this socket, and add it to the ref table
		/* ע��������仰! ע��fUniqueID����������fEventReq.er_data */
        
		/* ����Message/event id��������һ����ֵ��Windows��8192��Linux��10000000���ʹ�һ����ֵ��ʼ��Windows��0x0400��Linux��1�� */
        if (!compare_and_store(10000000, 1, &sUniqueID))
            fUniqueID = (PointerSizedInt)atomic_add(&sUniqueID, 1);
        else
            fUniqueID = 1;

		/* ����key�ַ���,��Hash TableԪ��������ǵ�ǰ����EventContext */
		/* ��ָ����event id����hash table,��ʵ�ϸ��о������ø�Hash TableԪ */
        fRef.Set(fUniqueIDStr, this);
		/* ����Hash TableԪ���뵽��ǰRef�б��� */
        fEventThread->fRefTable.Register(&fRef);
            
        //fill out the eventreq data structure
		/* ����event request struct��ʼ��Ϊ0 */
        ::memset( &fEventReq, '\0', sizeof(fEventReq));
		/* �ļ����������� */
        fEventReq.er_type = EV_FD;
		/* Socket�ļ������� */
        fEventReq.er_handle = fFileDesc;
		/* �����¼�����λ,Ĭ��EV_RE */
        fEventReq.er_eventbits = theMask;
		/* ��Ψһ��event ID�������¼���������� */
        fEventReq.er_data = (void*)fUniqueID;

		/*��������Ҫ����select_watchevent(),����Ϊtrue */
        fWatchEventCalled = true;

		/* ��ָ��eventreq���ļ�������fd�������,��д����ɾ��,��������ļ�������sMaxFDPos,��¼�¼�ID req->er_data��sCookieArray[],дpipe */
		/* ����ʵ���ϵ��õ�select_modwatch() */
        if (select_watchevent(&fEventReq, theMask) != 0)
            //this should never fail, but if it does, cleanup.
            AssertV(false, OSThread::GetErrno());
            
    }
}

/* ����Ҫ�ĺ���֮һ�� */
/* ��������,��ȡMessage,����������Socket�ϵȴ�ʱ,�������ݼ���Hash table��,������(signal)ָ���������¼�,���������ü���,֪ͨ�����ȴ��߳� */
void EventThread::Entry()
{
	/* ��ʼ��һ���ṹ��ķ��� */
    struct eventreq theCurrentEvent;
    ::memset( &theCurrentEvent, '\0', sizeof(theCurrentEvent) );
    
	/* ѭ��������е�Socket �˿��Ƿ������ݵ��� */
    while (true)
    {
        int theErrno = EINTR;

		/* ����ѭ��ֱ��theErrnoֵ����EINTR */
        while (theErrno == EINTR)
        {
			/* ��������,��ȡMessage,ע���һ������û������,����ֵֻ��0,��ֵ��EINTR(4) */
			/* select_waitevent����select()���� */
            int theReturnValue = select_waitevent(&theCurrentEvent, NULL);

            //Sort of a hack. In the POSIX version of the server, waitevent can return
            //an actual POSIX errorcode.
            if (theReturnValue >= 0)
                theErrno = theReturnValue;
            else
                theErrno = OSThread::GetErrno();//���ظ�ֵ,����
        }
        
		//ȷ���õ�theErrnoΪ0
        AssertV(theErrno == 0, theErrno);
        
        //ok, there's data waiting on this socket. Send a wakeup.
		//���¼�������������Ӧ��Socket �˿�
        if (theCurrentEvent.er_data != NULL)
        {
            //The cookie in this event is an ObjectID. Resolve that objectID into
            //a pointer.
			//ͨ���¼��еı�ʶ�ҵ���Ӧ�Ķ���ο�ָ��
			/* ����Socket�ȴ���event IDת����StrPtrLen����,ע��˴��ǳ�����!! */
            StrPtrLen idStr((char*)&theCurrentEvent.er_data, sizeof(theCurrentEvent.er_data));
            /* ͨ��ָ���ַ���ȥ����(ʶ���ȡ��)��event id�õ���Ӧ������(Hash TableԪ) */
            OSRef* ref = fRefTable.Resolve(&idStr);
			/* ���ɹ�ȡ�ø�����(Hash TableԪ) */
            if (ref != NULL)
            {
                /* ��ȡ����(Hash TableԪ)���ڵ���ָ�� */
                EventContext* theContext = (EventContext*)ref->GetObject();
#if DEBUG
                theContext->fModwatched = false;
#endif
				/* ��ָ���ĸ�EventContext����,��fTask����,����Task::Signal(Task::kReadEvent)����,�ʹ���Task::kReadEvent */
                theContext->ProcessEvent(theCurrentEvent.er_eventbits);
				/* �������ü���,֪ͨ�����ȴ��߳� */
                fRefTable.Release(ref);
                
                
            }
        }

#if EVENT_CONTEXT_DEBUG
        SInt64  yieldStart = OS::Milliseconds();
#endif

		/* ��Windowsƽ̨����,�����߳�����OSThread::Start()��ʵ�� */
        this->ThreadYield();

#if EVENT_CONTEXT_DEBUG
        SInt64  yieldDur = OS::Milliseconds() - yieldStart;
        static SInt64   numZeroYields;
        
        if ( yieldDur > 1 )
        {
            qtss_printf( "EventThread time in OSTHread::Yield %i, numZeroYields %i\n", (long)yieldDur, (long)numZeroYields );
            numZeroYields = 0;
        }
        else
            numZeroYields++;
#endif
    }
}
