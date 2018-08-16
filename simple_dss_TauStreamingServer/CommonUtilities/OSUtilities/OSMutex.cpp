/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMutex.h
Description: Provide a abstraction mutex class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include "OSMutex.h"
#include "SafeStdLib.h"
#include <stdlib.h>
#include <string.h>


static pthread_mutexattr_t  *sMutexAttr=NULL;
static void MutexAttrInit();
static pthread_once_t sMutexAttrInit = PTHREAD_ONCE_INIT;
 
    

OSMutex::OSMutex()
{
    (void)pthread_once(&sMutexAttrInit, MutexAttrInit);
    (void)pthread_mutex_init(&fMutex, sMutexAttr);
    
    fHolder = 0;
    fHolderCount = 0;
}

void MutexAttrInit()
{
    sMutexAttr = (pthread_mutexattr_t*)malloc(sizeof(pthread_mutexattr_t));
    ::memset(sMutexAttr, 0, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(sMutexAttr);
}

OSMutex::~OSMutex()
{
    pthread_mutex_destroy(&fMutex);
}


/* ���������Ѿ����˻�����(�ϴθ��õ�),�ٴ���ռ����ʱ�����¼���������;�������Ƚ����ٽ���,��ȡ��ǰThread ID��ʹ�̼߳�������1 */
void        OSMutex::RecursiveLock()
{
    // We already have this mutex. Just refcount and return
	/* ���������Ѿ����˻�����(�ϴθ��õ�),�ٴ���ռ����ʱ�����¼��������� */
    if (OSThread::GetCurrentThreadID() == fHolder)
    {  /* increment ref count */
		/* �߳�id�ĵݹ��������1 */
        fHolderCount++;
        return;
    }
    (void)pthread_mutex_lock(&fMutex);

	/* �ս���ʱfHolder��fHolderCountһ��Ҫ��Ϊ0 */
    Assert(fHolder == 0);
	/* update the flag of holder of Critical Section */
	/* ��ȡ��ǰThread ID */
    fHolder = OSThread::GetCurrentThreadID();
	/* �̼߳�������1 */
    fHolderCount++;
    Assert(fHolderCount == 1);
}

/* ���粻�ǵ�ǰ�߳�,�������,����;����ȷ����ǰ�߳����ü���>0,��ε���ʹ���Ϊ0.���߳����ü���Ϊ0ʱ,ʹ�߳�IDΪ0���뿪�ٽ��� */
void        OSMutex::RecursiveUnlock()
{
	/* ���粻�ǵ�ǰ�߳�,�������,���� */
    if (OSThread::GetCurrentThreadID() != fHolder)
        return;
    
	/* ȷ����ǰ���̴߳���,�̼߳�����0,��ε���ʹ���Ϊ0 */
    Assert(fHolderCount > 0);
    fHolderCount--;
	/* �����̼߳���Ϊ0ʱ,�뿪�ٽ��� */
    if (fHolderCount == 0)
    {
		/* ���߳�id��0,�Ա��´ν���ʱ���� */
        fHolder = 0;
        pthread_mutex_unlock(&fMutex);
    }
}

/* compare RecursiveTryLock() with RecursiveLock()  */
/* ���˺������ص���Boolֵ����void��,������OSMutex::RecursiveLock()��ͬ */
Bool16      OSMutex::RecursiveTryLock()
{
    // We already have this mutex. Just refcount and return
    /* ���������Ѿ����˻�����(�ϴθ��õ�),�ٴ���ռ����ʱ�����¼���������true,˵�������Ѿ�������� */
    if (OSThread::GetCurrentThreadID() == fHolder)
    {
        fHolderCount++;
        return true;
    }

    int theErr = pthread_mutex_trylock(&fMutex);
    if (theErr != 0)
    {
        Assert(theErr == EBUSY);
        return false;
    }

	/* �����OSMutex::RecursiveLock()��ͬ */
    Assert(fHolder == 0);
    fHolder = OSThread::GetCurrentThreadID();
    fHolderCount++;
    Assert(fHolderCount == 1);
	/* always return true  */
    return true;
}

