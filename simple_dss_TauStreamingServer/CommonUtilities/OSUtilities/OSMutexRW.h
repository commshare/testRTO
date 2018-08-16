/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMutexRW.h
Description: Provide a common OSMutex class,but use for read/write date casees.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef _OSMUTEXRW_H_
#define _OSMUTEXRW_H_

#include <stdlib.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"
#include "OSThread.h"
#include "OSMutex.h"
#include "OSQueue.h"
#include "MyAssert.h"


#define DEBUGMUTEXRW 0

/* �ǳ���Ҫ��һ������,ע�����õ�OSMutex���OSCond��Ķ�����Ϊ���ݳ�Ա */
class OSMutexRW
{
    public:
        
		/* ע��ֻ�й��캯��,�Ҹ��������Ѹ�ֵ */
        OSMutexRW(): fState(0), fWriteWaiters(0),fReadWaiters(0),fActiveReaders(0) {} ;

		/* ���ļ�������Ҫ��5������,������Ҫ������!����OSMutexRW.cpp�ж��� */
        void LockRead();
        void LockWrite();
        void Unlock();
        
        // Returns 0 on success, EBUSY on failure
        int TryLockWrite();
        int TryLockRead();
    
    private:
        enum {eMaxWait = 0x0FFFFFFF, eMultiThreadCondition = true };
		/* д/��д״̬�� */
        enum {eActiveWriterState = -1, eNoWriterState = 0 };

		/* ���ݳ�Ա */

		/* �ǳ���Ҫ��һ�����ݳ�Ա */
        OSMutex             fInternalLock;   // the internal lock         
        OSCond              fReadersCond;    // the waiting readers             
        OSCond              fWritersCond;    // the waiting writers 

		/* ״̬/��/д/��Ծ�� */
        int                 fState;          // -1:writer,0:free,>0:readers 
        int                 fWriteWaiters;   // number of waiting writers,ע����������-1  
        int                 fReadWaiters;    // number of waiting readers
		/* Ҳ��Ϊ��ֵ,�μ�OSMutexRW::LockWrite() */
        int                 fActiveReaders;  // number of active readers = fState >= 0;

		/* �޸Ļ��ȡ�����Ϣ,�������������� */

		/* �޸�/���ø��� */
        inline void AdjustState(int i) {  fState += i; };
        inline void AdjustWriteWaiters(int i) { fWriteWaiters += i; };
        inline void AdjustReadWaiters(int i) {  fReadWaiters += i; };
        inline void SetState(int i) { fState = i; };
        inline void SetWriteWaiters(int i) {  fWriteWaiters = i; };
        inline void SetReadWaiters(int i) { fReadWaiters = i; };
        
		/* ����/���ٵȴ���/д/��Ծ���� */
        inline void AddWriteWaiter() { AdjustWriteWaiters(1); };
        inline void RemoveWriteWaiter() {  AdjustWriteWaiters(-1); };
        
        inline void AddReadWaiter() { AdjustReadWaiters(1); };
        inline void RemoveReadWaiter() {  AdjustReadWaiters(-1); };
        
        inline void AddActiveReader() { AdjustState(1); };/* ע��ʹ״̬��1 */
        inline void RemoveActiveReader() {  AdjustState(-1); };
        
        
		/* �жϵȴ���/д���� */
        inline Bool16 WaitingWriters()  {return (Bool16) (fWriteWaiters > 0) ; }
        inline Bool16 WaitingReaders()  {return (Bool16) (fReadWaiters > 0) ;}

		/* �ж��Ƿ�Ϊ��Ծ(��/д)״̬? */
        inline Bool16 Active()          {return (Bool16) (fState != 0) ;}
		/* �ж��Ƿ��״̬? */
        inline Bool16 ActiveReaders()   {return (Bool16) (fState > 0) ;}
		/* �ж��Ƿ�д״̬? */
        inline Bool16 ActiveWriter()    {return (Bool16) (fState < 0) ;} // only one
    
    #if DEBUGMUTEXRW
        static int fCount, fMaxCount;
        static OSMutex sCountMutex;
        void CountConflict(int i); 
    #endif

};

/* ע��OSMutexReadWriteLocker����OSMutexRW��ķ�װ,����ֻ������� */
class   OSMutexReadWriteLocker
{
    public:
        OSMutexReadWriteLocker(OSMutexRW *inMutexPtr): fRWMutexPtr(inMutexPtr) {};
        ~OSMutexReadWriteLocker() { if (fRWMutexPtr != NULL) fRWMutexPtr->Unlock(); }


        void UnLock() { if (fRWMutexPtr != NULL) fRWMutexPtr->Unlock(); }
        void SetMutex(OSMutexRW *mutexPtr) {fRWMutexPtr = mutexPtr;}

		/* Ψһ�����ݳ�Ա,��װ��OSMutexRW,����������������Ҫ�� */
        OSMutexRW*  fRWMutexPtr;
};
   
/* �������������ཫ����Ķ�д���������ֿܷ�,��ֻ����Զ�/д����,����������� */

class   OSMutexReadLocker: public OSMutexReadWriteLocker
{
    public:

        OSMutexReadLocker(OSMutexRW *inMutexPtr) : OSMutexReadWriteLocker(inMutexPtr) { if (OSMutexReadWriteLocker::fRWMutexPtr != NULL) OSMutexReadWriteLocker::fRWMutexPtr->LockRead(); }
        void Lock()         { if (OSMutexReadWriteLocker::fRWMutexPtr != NULL) OSMutexReadWriteLocker::fRWMutexPtr->LockRead(); }       
};

class   OSMutexWriteLocker: public OSMutexReadWriteLocker
{
    public:

        OSMutexWriteLocker(OSMutexRW *inMutexPtr) : OSMutexReadWriteLocker(inMutexPtr) { if (OSMutexReadWriteLocker::fRWMutexPtr != NULL) OSMutexReadWriteLocker::fRWMutexPtr->LockWrite(); }       
        void Lock()         { if (OSMutexReadWriteLocker::fRWMutexPtr != NULL) OSMutexReadWriteLocker::fRWMutexPtr->LockWrite(); }

};



#endif //_OSMUTEX_H_
