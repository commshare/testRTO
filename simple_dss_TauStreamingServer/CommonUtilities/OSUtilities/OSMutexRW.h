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

/* 非常重要的一个基类,注意它用到OSMutex类和OSCond类的对象作为数据成员 */
class OSMutexRW
{
    public:
        
		/* 注意只有构造函数,且各整形量已赋值 */
        OSMutexRW(): fState(0), fWriteWaiters(0),fReadWaiters(0),fActiveReaders(0) {} ;

		/* 该文件中最重要的5个函数,起着重要的作用!将在OSMutexRW.cpp中定义 */
        void LockRead();
        void LockWrite();
        void Unlock();
        
        // Returns 0 on success, EBUSY on failure
        int TryLockWrite();
        int TryLockRead();
    
    private:
        enum {eMaxWait = 0x0FFFFFFF, eMultiThreadCondition = true };
		/* 写/非写状态码 */
        enum {eActiveWriterState = -1, eNoWriterState = 0 };

		/* 数据成员 */

		/* 非常重要的一个数据成员 */
        OSMutex             fInternalLock;   // the internal lock         
        OSCond              fReadersCond;    // the waiting readers             
        OSCond              fWritersCond;    // the waiting writers 

		/* 状态/读/写/活跃数 */
        int                 fState;          // -1:writer,0:free,>0:readers 
        int                 fWriteWaiters;   // number of waiting writers,注意它绝不是-1  
        int                 fReadWaiters;    // number of waiting readers
		/* 也可为负值,参见OSMutexRW::LockWrite() */
        int                 fActiveReaders;  // number of active readers = fState >= 0;

		/* 修改或获取相关信息,各函数已作定义 */

		/* 修改/设置个数 */
        inline void AdjustState(int i) {  fState += i; };
        inline void AdjustWriteWaiters(int i) { fWriteWaiters += i; };
        inline void AdjustReadWaiters(int i) {  fReadWaiters += i; };
        inline void SetState(int i) { fState = i; };
        inline void SetWriteWaiters(int i) {  fWriteWaiters = i; };
        inline void SetReadWaiters(int i) { fReadWaiters = i; };
        
		/* 增加/减少等待读/写/活跃个数 */
        inline void AddWriteWaiter() { AdjustWriteWaiters(1); };
        inline void RemoveWriteWaiter() {  AdjustWriteWaiters(-1); };
        
        inline void AddReadWaiter() { AdjustReadWaiters(1); };
        inline void RemoveReadWaiter() {  AdjustReadWaiters(-1); };
        
        inline void AddActiveReader() { AdjustState(1); };/* 注意使状态加1 */
        inline void RemoveActiveReader() {  AdjustState(-1); };
        
        
		/* 判断等待读/写个数 */
        inline Bool16 WaitingWriters()  {return (Bool16) (fWriteWaiters > 0) ; }
        inline Bool16 WaitingReaders()  {return (Bool16) (fReadWaiters > 0) ;}

		/* 判断是否为活跃(读/写)状态? */
        inline Bool16 Active()          {return (Bool16) (fState != 0) ;}
		/* 判断是否读状态? */
        inline Bool16 ActiveReaders()   {return (Bool16) (fState > 0) ;}
		/* 判断是否写状态? */
        inline Bool16 ActiveWriter()    {return (Bool16) (fState < 0) ;} // only one
    
    #if DEBUGMUTEXRW
        static int fCount, fMaxCount;
        static OSMutex sCountMutex;
        void CountConflict(int i); 
    #endif

};

/* 注意OSMutexReadWriteLocker类是OSMutexRW类的封装,但它只负责解锁 */
class   OSMutexReadWriteLocker
{
    public:
        OSMutexReadWriteLocker(OSMutexRW *inMutexPtr): fRWMutexPtr(inMutexPtr) {};
        ~OSMutexReadWriteLocker() { if (fRWMutexPtr != NULL) fRWMutexPtr->Unlock(); }


        void UnLock() { if (fRWMutexPtr != NULL) fRWMutexPtr->Unlock(); }
        void SetMutex(OSMutexRW *mutexPtr) {fRWMutexPtr = mutexPtr;}

		/* 唯一的数据成员,包装了OSMutexRW,下面两个派生类中要用 */
        OSMutexRW*  fRWMutexPtr;
};
   
/* 下面两个派生类将具体的读写互斥锁功能分开,且只负责对读/写加锁,而不负责解锁 */

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
