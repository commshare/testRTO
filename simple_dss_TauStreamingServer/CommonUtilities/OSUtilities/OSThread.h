
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSThread.h
Description: Provide a thread abstraction class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 

#ifndef __OSTHREAD__
#define __OSTHREAD__

#include <sys/errno.h>
#include <pthread.h>
#include "OSHeaders.h"
#include "DateTranslator.h"  /* 时间数据格式的转换 */


class OSThread
{
public:
    //
    // Call before calling any other OSThread function
	/* 针对每个线程取得一个存储单元 */
    static void     Initialize();
                
                    OSThread();
    virtual         ~OSThread();
    
    //
    // Derived classes must implement their own entry function(入口函数)，such as TaskThread::Entry()
	/* 派生类线程的执行函数有其各自的执行代码 */
    virtual     void            Entry() = 0;
	            /* used in TaskThreadPool::AddThreads() */
	            /* 创建新线程,设置线程的句柄fThreadID */
                void            Start();
                
				/* used in TaskThread::Entry() */
                static void     ThreadYield();
				/* 需要用到OS.h,涉及时间的处理 */
                static void     Sleep(UInt32 inMsec);
                
				/* 无限期等待另一个线程结束，指定的线程到来 */
                void            Join();
				/* used in TaskThreadPool::RemoveThreads() */
                void            SendStopRequest() { fStopRequested = true; }
                Bool16          IsStopRequested() { return fStopRequested; }

				/* 向当前正在执行的另一线程发出stop请求,无限期等待该线程停下,指定句柄的线程到来 */
                void            StopAndWaitForThread();

				/* 设置线程数据 */
                void*           GetThreadData()         { return fThreadData; }
                void            SetThreadData(void* inThreadData) { fThreadData = inThreadData; }
                
                // As a convienence to higher levels, each thread has its own date buffer(时间缓存)
                DateBuffer*     GetDateBuffer()         { return &fDateBuffer; }
                
				/* 设置主线程数据 */
                static void*    GetMainThreadData()     { return sMainThreadData; }
                static void     SetMainThreadData(void* inData) { sMainThreadData = inData; }

				/* 设置User/Group字符串 */
                static void     SetUser(char *user) {::strncpy(sUser,user, sizeof(sUser) -1); sUser[sizeof(sUser) -1]=0;} 
                static void     SetGroup(char *group) {::strncpy(sGroup,group, sizeof(sGroup) -1); sGroup[sizeof(sGroup) -1]=0;} 

				/* 设置User和Group字符串 */
                static void     SetPersonality(char *user, char* group) { SetUser(user); SetGroup(group); };
                Bool16          SwitchPersonality();
#if DEBUG
                UInt32          GetNumLocksHeld() { return 0; }
                void            IncrementLocksHeld() {}
                void            DecrementLocksHeld() {}
#endif

                static void     WrapSleep( Bool16 wrapSleep) {sWrapSleep = wrapSleep; }

    static  int         GetErrno() { return errno; }
    static  pthread_t   GetCurrentThreadID() { return ::pthread_self(); }

	/* 得到当前对象OSThread的指针 */
	/* 在当前线程中的TLS取值 */
	/* 获取当前线程在TLS中存贮的索引标识 */
    static  OSThread*   GetCurrent();
 
private:

    static pthread_key_t    gMainKey;
    static char sUser[128];
    static char sGroup[128];
    
    /* 线程状态:停止吗?加入吗? */

    /* 向当前正在执行的另一线程发出stop请求吗?  */
    Bool16 fStopRequested;
	/* 要加入指定线程吗? */
    Bool16 fJoined;

	/* 指定线程的句柄 */
    pthread_t       fThreadID;

	/* 线程数据(存储在TlsAlloc函数分配的存储单元中)和时间格式数据缓冲 */
    void*           fThreadData;
    DateBuffer      fDateBuffer;
    
	/* 主线程数据,调用TlsAlloc函数分配标记线程存储数组未被使用成员的索引的线程,跟其它线程区别开 */
    static void*    sMainThreadData;
    static Bool16   sWrapSleep;
	/* 入口函数,非常重要! */
    static void*    _Entry(void* inThread);
};

/* 设置线程数据的类 */
class OSThreadDataSetter
{
    public:
    
        OSThreadDataSetter(void* inInitialValue, void* inFinalValue) : fFinalValue(inFinalValue)
            { OSThread::GetCurrent()->SetThreadData(inInitialValue); }
            
        ~OSThreadDataSetter() { OSThread::GetCurrent()->SetThreadData(fFinalValue); }
        
    private:
    
        void*   fFinalValue;
};


#endif

