/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSThread.cpp
Description: Provide a thread abstraction class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

#include "OSThread.h"
#include "MyAssert.h"
#include "SafeStdLib.h"


/* 先对几个数据成员初始化 */
void*   OSThread::sMainThreadData = NULL;
pthread_key_t OSThread::gMainKey = 0;
char  OSThread::sUser[128]= "";
char  OSThread::sGroup[128]= "";
Bool16  OSThread::sWrapSleep = true;

/* 分配同一进程中所有线程共享的TLS存储索引,获取thread index */
void OSThread::Initialize()
{
    pthread_key_create(&OSThread::gMainKey, NULL);
}

OSThread::OSThread()
:   fStopRequested(false),/* 还未对某线程提出stop请求 */
    fJoined(false),/* 还未加入某线程 */
    fThreadData(NULL) /*  线程数据(存储在TlsAlloc函数分配的存储单元中)为空 */
{
}

OSThread::~OSThread()
{
	/* 向当前正在执行的另一线程发出stop请求,无限期等待该线程停下,指定句柄的线程到来 */
    this->StopAndWaitForThread();
}


/* 本类中最重要的函数!! */
/* 创建新线程,设置线程的句柄fThreadID */
void OSThread::Start()
{
    pthread_attr_t* theAttrP;
	/* 线程属性采用默认值 */
    theAttrP = NULL;
	/* 创建指定pid的线程 */
    int err = pthread_create((pthread_t*)&fThreadID, theAttrP, _Entry, (void*)this);
	/* 确保指定线程成功 */
    Assert(err == 0);
}

/* 向当前正在执行的另一线程发出stop请求,无限期等待该线程停下,指定句柄的线程到来 */
void OSThread::StopAndWaitForThread()
{
    fStopRequested = true;
    if (!fJoined)
        Join();
}

/* 无限期等待另一个线程结束，指定的线程到来 */
void OSThread::Join()
{
    // What we're trying to do is allow the thread we want to delete to complete
    // running. So we wait for it to stop.
	/* 先要让目前的线程运行完后停下来,所以我们要等待它 */
    Assert(!fJoined);
    fJoined = true;
	/* 挂起当前线程，直至指定线程终止 */
    void *retVal;
    pthread_join((pthread_t)fThreadID, &retVal);
}

/* 对Windows/Linux平台无用,创建线程已在OSThread::Start()中实现 */
void OSThread::ThreadYield()
{
}

#include "OS.h" /* 注意该头文件包含对系统时间的处理 */
/* used in StartServer() in RunServer.cpp */
void OSThread::Sleep(UInt32 inMsec)
{
    if (inMsec == 0)
        return;
        
    SInt64 startTime = OS::Milliseconds();
    SInt64 timeLeft = inMsec;
    SInt64 timeSlept = 0;

    do {
        //qtss_printf("OSThread::Sleep = %qd request sleep=%qd\n",timeSlept, timeLeft);
        timeLeft = inMsec - timeSlept;
        if (timeLeft < 1)
            break;
            
        ::usleep(timeLeft * 1000);

        timeSlept = (OS::Milliseconds() - startTime);
        if (timeSlept < 0) // system time set backwards
            break;
            
    } while (timeSlept < inMsec);

	//qtss_printf("total sleep = %qd request sleep=%lu\n", timeSlept,inMsec);

}

/* 本类中最重要的一个函数:在指定Thread的TLS中存贮索引值,启动线程入口Entry函数(针对不同的线程,有不同的实现细节) */
void* OSThread::_Entry(void *inThread)  //static
{
    OSThread* theThread = (OSThread*)inThread;
	/* 查询自身的线程id */
    theThread->fThreadID = (pthread_t)pthread_self();
	/* 设置自身的线程id */
    pthread_setspecific(OSThread::gMainKey, theThread);

	/* 对Windows来说,它已经返回true了 */
    theThread->SwitchPersonality();

    // Run the thread
	/* 纯虚函数,由子类派生,针对不同的线程,有不同的实现细节 */
    theThread->Entry();
    return NULL;
}


Bool16  OSThread::SwitchPersonality()
{
   if (::strlen(sGroup) > 0)
    {
        struct group* gr = ::getgrnam(sGroup);
        if (gr == NULL || ::setgid(gr->gr_gid) == -1)
        {
            //qtss_printf("thread %lu setgid  to group=%s FAILED \n", (UInt32) this, sGroup);
            return false;
        }
        
        //qtss_printf("thread %lu setgid  to group=%s \n", (UInt32) this, sGroup);
    }
    
        
    if (::strlen(sUser) > 0)
    {
        struct passwd* pw = ::getpwnam(sUser);
        if (pw == NULL || ::setuid(pw->pw_uid) == -1)
        {
            //qtss_printf("thread %lu setuid  to user=%s FAILED \n", (UInt32) this, sUser);
            return false;
        }

        //qtss_printf("thread %lu setuid  to user=%s \n", (UInt32) this, sUser);
   }

   return true;
}

/* 获取当前线程在TLS中存贮的索引标识 */
OSThread*   OSThread::GetCurrent()
{
    return (OSThread *)pthread_getspecific(OSThread::gMainKey);
}





