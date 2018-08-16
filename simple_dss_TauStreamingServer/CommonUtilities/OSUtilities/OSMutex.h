
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


#ifndef _OSMUTEX_H_
#define _OSMUTEX_H_

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"
#include "OSThread.h" /* ����Ǳ��������,Thread��Mutex������� */
#include "MyAssert.h"

class OSCond;

/* OSMutex����������ִ�к�����RecursiveLock()/RecursiveUnlock()/RecursiveTryLock() */
class OSMutex
{
    public:

        OSMutex();
        ~OSMutex();

		/* ������������ OSMutexLocker�з�װ */
        inline void Lock();
        inline void Unlock();
        
        // Returns true on successful grab of the lock, false on failure
		/* �ж��Ƿ�ɹ���ȡ����? */
        inline Bool16 TryLock();

    private:

        pthread_mutex_t fMutex;
        // These two platforms don't implement pthreads recursive mutexes, so
        // we have to do it manually
        pthread_t   fHolder;
        UInt32      fHolderCount;//�����ٽ����߳���

		/* �ݹ���/����/����? */

		/* ���������Ѿ����˻�����(�ϴθ��õ�),�ٴ���ռ����ʱ�����¼���������;�������Ƚ����ٽ���,��ȡ��ǰThread ID��ʹ�̼߳�������1 */
        void        RecursiveLock();

		/* ���粻�ǵ�ǰ�߳�,�������,����;����ȷ����ǰ�߳����ü���>0,��ε���ʹ���Ϊ0.���߳����ü���Ϊ0ʱ,ʹ�߳�IDΪ0���뿪�ٽ��� */
        void        RecursiveUnlock();

		/* ���˺������ص���Boolֵ����void��,������OSMutex::RecursiveLock()��ͬ */
        Bool16      RecursiveTryLock();

        friend class OSCond;
};

/* ע��OSMutexLocker����OSMutex��ķ�װ,����Lock()��Unlock(),��ר��ִ������/�����Ļ�������,��ʵ��ִ����,ֻʹ�ø��༴��! */
class   OSMutexLocker
{
    public:

		/* ע����������ǳ���Ҫ,��ʹ��Mutex�ĵط�����ʹ�� */
        OSMutexLocker(OSMutex *inMutexP) : fMutex(inMutexP) { if (fMutex != NULL) fMutex->Lock(); }
        ~OSMutexLocker() {  if (fMutex != NULL) fMutex->Unlock(); }
        
        void Lock()         { if (fMutex != NULL) fMutex->Lock(); }
        void Unlock()       { if (fMutex != NULL) fMutex->Unlock(); }
        
    private:
        /* define the critical region variable */
		/* Ψһ�����ݳ�Ա,���ദ���õ� */
        OSMutex*    fMutex;
};

//member functions definitions of class OSMutex

void OSMutex::Lock()
{
    this->RecursiveLock();
}

void OSMutex::Unlock()
{
    this->RecursiveUnlock();
}

Bool16 OSMutex::TryLock()
{
    return this->RecursiveTryLock();
}

#endif //_OSMUTEX_H_
