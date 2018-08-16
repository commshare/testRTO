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


/* �ȶԼ������ݳ�Ա��ʼ�� */
void*   OSThread::sMainThreadData = NULL;
pthread_key_t OSThread::gMainKey = 0;
char  OSThread::sUser[128]= "";
char  OSThread::sGroup[128]= "";
Bool16  OSThread::sWrapSleep = true;

/* ����ͬһ�����������̹߳����TLS�洢����,��ȡthread index */
void OSThread::Initialize()
{
    pthread_key_create(&OSThread::gMainKey, NULL);
}

OSThread::OSThread()
:   fStopRequested(false),/* ��δ��ĳ�߳����stop���� */
    fJoined(false),/* ��δ����ĳ�߳� */
    fThreadData(NULL) /*  �߳�����(�洢��TlsAlloc��������Ĵ洢��Ԫ��)Ϊ�� */
{
}

OSThread::~OSThread()
{
	/* ��ǰ����ִ�е���һ�̷߳���stop����,�����ڵȴ����߳�ͣ��,ָ��������̵߳��� */
    this->StopAndWaitForThread();
}


/* ����������Ҫ�ĺ���!! */
/* �������߳�,�����̵߳ľ��fThreadID */
void OSThread::Start()
{
    pthread_attr_t* theAttrP;
	/* �߳����Բ���Ĭ��ֵ */
    theAttrP = NULL;
	/* ����ָ��pid���߳� */
    int err = pthread_create((pthread_t*)&fThreadID, theAttrP, _Entry, (void*)this);
	/* ȷ��ָ���̳߳ɹ� */
    Assert(err == 0);
}

/* ��ǰ����ִ�е���һ�̷߳���stop����,�����ڵȴ����߳�ͣ��,ָ��������̵߳��� */
void OSThread::StopAndWaitForThread()
{
    fStopRequested = true;
    if (!fJoined)
        Join();
}

/* �����ڵȴ���һ���߳̽�����ָ�����̵߳��� */
void OSThread::Join()
{
    // What we're trying to do is allow the thread we want to delete to complete
    // running. So we wait for it to stop.
	/* ��Ҫ��Ŀǰ���߳��������ͣ����,��������Ҫ�ȴ��� */
    Assert(!fJoined);
    fJoined = true;
	/* ����ǰ�̣߳�ֱ��ָ���߳���ֹ */
    void *retVal;
    pthread_join((pthread_t)fThreadID, &retVal);
}

/* ��Windows/Linuxƽ̨����,�����߳�����OSThread::Start()��ʵ�� */
void OSThread::ThreadYield()
{
}

#include "OS.h" /* ע���ͷ�ļ�������ϵͳʱ��Ĵ��� */
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

/* ����������Ҫ��һ������:��ָ��Thread��TLS�д�������ֵ,�����߳����Entry����(��Բ�ͬ���߳�,�в�ͬ��ʵ��ϸ��) */
void* OSThread::_Entry(void *inThread)  //static
{
    OSThread* theThread = (OSThread*)inThread;
	/* ��ѯ������߳�id */
    theThread->fThreadID = (pthread_t)pthread_self();
	/* ����������߳�id */
    pthread_setspecific(OSThread::gMainKey, theThread);

	/* ��Windows��˵,���Ѿ�����true�� */
    theThread->SwitchPersonality();

    // Run the thread
	/* ���麯��,����������,��Բ�ͬ���߳�,�в�ͬ��ʵ��ϸ�� */
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

/* ��ȡ��ǰ�߳���TLS�д�����������ʶ */
OSThread*   OSThread::GetCurrent()
{
    return (OSThread *)pthread_getspecific(OSThread::gMainKey);
}





