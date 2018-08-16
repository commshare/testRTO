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


/* 入参为Socket描述符和相应的事件线程 */
EventContext::EventContext(int inFileDesc, EventThread* inThread)
:   fFileDesc(inFileDesc),
    fUniqueID(0),
    fUniqueIDStr((char*)&fUniqueID, sizeof(fUniqueID)),
    fEventThread(inThread),
    fWatchEventCalled(false),
    fAutoCleanup(true)
{}

/* 将指定socket设置成非阻塞模式 */
void EventContext::InitNonBlocking(int inFileDesc)
{
	/* 用来标识一个Socket的描述字 */
    fFileDesc = inFileDesc;

    int flag = ::fcntl(fFileDesc, F_GETFL, 0);
    int err = ::fcntl(fFileDesc, F_SETFL, flag | O_NONBLOCK);

	/* 出错后显示错误原因 */
    AssertV(err == 0, OSThread::GetErrno());
}

/* 通过描述socket的标识符判断,若合法就从HashTable中删去该socket,从读集和写集中删除该socket fd,关闭套接字,否则直接关闭套接字,设置相关参数 */
void EventContext::Cleanup()
{
    int err = 0;
    
	/* 假如标识socket的描述符合法,就从Hash Table中注销该socket */
    if (fFileDesc != kInvalidFileDesc)
    {
        //if this object is registered in the table, unregister it now
        if (fUniqueID > 0)
        {
            fEventThread->fRefTable.UnRegister(&fRef);
			/* 从读集和写集中删除该socket fd */
            select_removeevent(fFileDesc);//The eventqueue / select shim requires this
        }
        else
            err = ::close(fFileDesc);
    }

	/* 设置清除该Socket后的相关参数 */
    fFileDesc = kInvalidFileDesc;
    fUniqueID = 0;
    
    AssertV(err == 0, OSThread::GetErrno());//we don't really care if there was an error, but it's nice to know
}

/* 利用入参来获取数据成员的值,并将数据成员fRef加入Hash Table中,删去入参中同key值的Ref */
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
    
	/* 从入参来配置各数据成员的值 */
    fromContext.fFileDesc = kInvalidFileDesc;
    
    fWatchEventCalled = fromContext.fWatchEventCalled; 
    fUniqueID = fromContext.fUniqueID;
    fUniqueIDStr.Set((char*)&fUniqueID, sizeof(fUniqueID)),
    
    ::memcpy( &fEventReq, &fromContext.fEventReq, sizeof( struct eventreq  ) );

	/* 利用OSRef::Set()设置数据成员fRef */
    fRef.Set( fUniqueIDStr, this );
	/* 用fRef替换掉原来的同key值的Ref */
    fEventThread->fRefTable.Swap(&fRef);
	/* 删去入参中的Ref */
    fEventThread->fRefTable.UnRegister(&fromContext.fRef);
}

/* select_modwatch内部调用了WSAsyncSelect(),通过RequestEvent 函数申请对该描述符中某些事件(一般为EV_RE)的监听 */

/* 首先运用select_modwatch()监控指定窗口sMsgWindow是否有指定掩码的event发生?
否则配置唯一的fEventReq信息后,运用select_watchevent()来监听指定窗口sMsgWindow是否有指定掩码的event发生?*/
//注册事件
void EventContext::RequestEvent(int theMask)
{
#if DEBUG
    fModwatched = true;//假设调用了select_modwatch()
#endif

    //
    // The first time this function gets called, we're supposed to
    // call watchevent. Each subsequent time, call modwatch. That's
    // the way the MacOS X event queue works.
	/* 注意在Win32下,watchevent和modwatch没有区别!,参见win32ev.cpp */
    
	/* 首先运用select_modwatch()监控指定窗口sMsgWindow是否有指定掩码的event发生? */
	/* 如果已经调用过select_watchevent()，即不是第一次调用  */
    if (fWatchEventCalled)
    {
		/* 设置网络事件掩码 */
		//注册指定event(默认为EV_RE)
        fEventReq.er_eventbits = theMask;

		/* 请求Windows Socket DLL为指定窗口sMsgWindow发送在指定Socket处由theEvent指明的网络事件的窗口消息theMsg */
		/* 若是EV_RE,将指定eventreq的文件描述符fd加入读集,从写集中删除;
          若是EV_WR,将指定eventreq的文件描述符fd加入写集,从读集中删除.
          更新最大文件描述符sMaxFDPos,记录req->er_data进sCookieArray[],写pipe,总是返回函数值0 */
        if (select_modwatch(&fEventReq, theMask) != 0)
            AssertV(false, OSThread::GetErrno());
    }
	// 如果是第一次申请，需要调用select_watchevent()先分配一个唯一的标识符
	// 并将fRef注册到fRefTable，以供EventThread在后续处理中找到对应的对象
    else /* 否则配置唯一的fEventReq信息后,运用select_watchevent()来监听指定窗口sMsgWindow是否有指定掩码的event发生? */
    {
        //allocate a Unique ID for this socket, and add it to the ref table
		/* 注意上面这句话! 注意fUniqueID将用来配置fEventReq.er_data */
        
		/* 设置Message/event id，当超出一定数值（Windows是8192，Linux是10000000）就从一定数值开始（Windows是0x0400，Linux是1） */
        if (!compare_and_store(10000000, 1, &sUniqueID))
            fUniqueID = (PointerSizedInt)atomic_add(&sUniqueID, 1);
        else
            fUniqueID = 1;

		/* 设置key字符串,该Hash Table元所在类就是当前的类EventContext */
		/* 将指定的event id放入hash table,事实上该行就是配置该Hash Table元 */
        fRef.Set(fUniqueIDStr, this);
		/* 将该Hash Table元加入到当前Ref列表中 */
        fEventThread->fRefTable.Register(&fRef);
            
        //fill out the eventreq data structure
		/* 将该event request struct初始化为0 */
        ::memset( &fEventReq, '\0', sizeof(fEventReq));
		/* 文件描述符类型 */
        fEventReq.er_type = EV_FD;
		/* Socket文件描述符 */
        fEventReq.er_handle = fFileDesc;
		/* 网络事件掩码位,默认EV_RE */
        fEventReq.er_eventbits = theMask;
		/* 用唯一的event ID来设置事件请求的数据 */
        fEventReq.er_data = (void*)fUniqueID;

		/*现在马上要调用select_watchevent(),设置为true */
        fWatchEventCalled = true;

		/* 将指定eventreq的文件描述符fd加入读集,从写集中删除,更新最大文件描述符sMaxFDPos,记录事件ID req->er_data进sCookieArray[],写pipe */
		/* 这里实际上调用的select_modwatch() */
        if (select_watchevent(&fEventReq, theMask) != 0)
            //this should never fail, but if it does, cleanup.
            AssertV(false, OSThread::GetErrno());
            
    }
}

/* 最重要的函数之一。 */
/* 创建窗口,获取Message,当有数据在Socket上等待时,将该数据加入Hash table中,并触发(signal)指定的网络事件,并减少引用计数,通知其它等待线程 */
void EventThread::Entry()
{
	/* 初始化一个结构体的方法 */
    struct eventreq theCurrentEvent;
    ::memset( &theCurrentEvent, '\0', sizeof(theCurrentEvent) );
    
	/* 循环监控所有的Socket 端口是否有数据到来 */
    while (true)
    {
        int theErrno = EINTR;

		/* 反复循环直到theErrno值不是EINTR */
        while (theErrno == EINTR)
        {
			/* 创建窗口,获取Message,注意第一个参数没有用上,返回值只有0,负值和EINTR(4) */
			/* select_waitevent含有select()调用 */
            int theReturnValue = select_waitevent(&theCurrentEvent, NULL);

            //Sort of a hack. In the POSIX version of the server, waitevent can return
            //an actual POSIX errorcode.
            if (theReturnValue >= 0)
                theErrno = theReturnValue;
            else
                theErrno = OSThread::GetErrno();//返回负值,出错
        }
        
		//确保得到theErrno为0
        AssertV(theErrno == 0, theErrno);
        
        //ok, there's data waiting on this socket. Send a wakeup.
		//有事件发生，唤醒相应的Socket 端口
        if (theCurrentEvent.er_data != NULL)
        {
            //The cookie in this event is an ObjectID. Resolve that objectID into
            //a pointer.
			//通过事件中的标识找到相应的对象参考指针
			/* 将该Socket等待的event ID转换成StrPtrLen类型,注意此处是常引用!! */
            StrPtrLen idStr((char*)&theCurrentEvent.er_data, sizeof(theCurrentEvent.er_data));
            /* 通过指定字符串去解析(识别和取得)该event id得到相应的引用(Hash Table元) */
            OSRef* ref = fRefTable.Resolve(&idStr);
			/* 当成功取得该引用(Hash Table元) */
            if (ref != NULL)
            {
                /* 获取引用(Hash Table元)所在的类指针 */
                EventContext* theContext = (EventContext*)ref->GetObject();
#if DEBUG
                theContext->fModwatched = false;
#endif
				/* 对指定的该EventContext对象,若fTask存在,利用Task::Signal(Task::kReadEvent)传信,就触发Task::kReadEvent */
                theContext->ProcessEvent(theCurrentEvent.er_eventbits);
				/* 减少引用计数,通知其它等待线程 */
                fRefTable.Release(ref);
                
                
            }
        }

#if EVENT_CONTEXT_DEBUG
        SInt64  yieldStart = OS::Milliseconds();
#endif

		/* 对Windows平台无用,创建线程已在OSThread::Start()中实现 */
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
