
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
#include "DateTranslator.h"  /* ʱ�����ݸ�ʽ��ת�� */


class OSThread
{
public:
    //
    // Call before calling any other OSThread function
	/* ���ÿ���߳�ȡ��һ���洢��Ԫ */
    static void     Initialize();
                
                    OSThread();
    virtual         ~OSThread();
    
    //
    // Derived classes must implement their own entry function(��ں���)��such as TaskThread::Entry()
	/* �������̵߳�ִ�к���������Ե�ִ�д��� */
    virtual     void            Entry() = 0;
	            /* used in TaskThreadPool::AddThreads() */
	            /* �������߳�,�����̵߳ľ��fThreadID */
                void            Start();
                
				/* used in TaskThread::Entry() */
                static void     ThreadYield();
				/* ��Ҫ�õ�OS.h,�漰ʱ��Ĵ��� */
                static void     Sleep(UInt32 inMsec);
                
				/* �����ڵȴ���һ���߳̽�����ָ�����̵߳��� */
                void            Join();
				/* used in TaskThreadPool::RemoveThreads() */
                void            SendStopRequest() { fStopRequested = true; }
                Bool16          IsStopRequested() { return fStopRequested; }

				/* ��ǰ����ִ�е���һ�̷߳���stop����,�����ڵȴ����߳�ͣ��,ָ��������̵߳��� */
                void            StopAndWaitForThread();

				/* �����߳����� */
                void*           GetThreadData()         { return fThreadData; }
                void            SetThreadData(void* inThreadData) { fThreadData = inThreadData; }
                
                // As a convienence to higher levels, each thread has its own date buffer(ʱ�仺��)
                DateBuffer*     GetDateBuffer()         { return &fDateBuffer; }
                
				/* �������߳����� */
                static void*    GetMainThreadData()     { return sMainThreadData; }
                static void     SetMainThreadData(void* inData) { sMainThreadData = inData; }

				/* ����User/Group�ַ��� */
                static void     SetUser(char *user) {::strncpy(sUser,user, sizeof(sUser) -1); sUser[sizeof(sUser) -1]=0;} 
                static void     SetGroup(char *group) {::strncpy(sGroup,group, sizeof(sGroup) -1); sGroup[sizeof(sGroup) -1]=0;} 

				/* ����User��Group�ַ��� */
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

	/* �õ���ǰ����OSThread��ָ�� */
	/* �ڵ�ǰ�߳��е�TLSȡֵ */
	/* ��ȡ��ǰ�߳���TLS�д�����������ʶ */
    static  OSThread*   GetCurrent();
 
private:

    static pthread_key_t    gMainKey;
    static char sUser[128];
    static char sGroup[128];
    
    /* �߳�״̬:ֹͣ��?������? */

    /* ��ǰ����ִ�е���һ�̷߳���stop������?  */
    Bool16 fStopRequested;
	/* Ҫ����ָ���߳���? */
    Bool16 fJoined;

	/* ָ���̵߳ľ�� */
    pthread_t       fThreadID;

	/* �߳�����(�洢��TlsAlloc��������Ĵ洢��Ԫ��)��ʱ���ʽ���ݻ��� */
    void*           fThreadData;
    DateBuffer      fDateBuffer;
    
	/* ���߳�����,����TlsAlloc�����������̴߳洢����δ��ʹ�ó�Ա���������߳�,�������߳����� */
    static void*    sMainThreadData;
    static Bool16   sWrapSleep;
	/* ��ں���,�ǳ���Ҫ! */
    static void*    _Entry(void* inThread);
};

/* �����߳����ݵ��� */
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

