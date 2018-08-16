
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSQueue.h
Description: Provide a queue operation class.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _OSQUEUE_H_
#define _OSQUEUE_H_

#include "MyAssert.h"
#include "OSHeaders.h"
#include "OSMutex.h"
#include "OSCond.h"
#include "OSThread.h"

#define OSQUEUETESTING 0

class OSQueue;

/* 该类描述的是找队列元素的前一个和后一个元素,所在队列,外部依存类的指针操作 */
class OSQueueElem {
    public:
        OSQueueElem(void* enclosingObject = NULL) : fNext(NULL), fPrev(NULL), fQueue(NULL),
                                                    fEnclosingObject(enclosingObject) {}
        virtual ~OSQueueElem() { Assert(fQueue == NULL); }

		/* 此处使用常引用,判断给出的队列对象是否是当前队列元素的队列对象? */
        Bool16 IsMember(const OSQueue& queue) { return (&queue == fQueue); }
		/* 判断队列指针是否为空? */
        Bool16 IsMemberOfAnyQueue()     { return fQueue != NULL; }

		/* 获取和设置包含该类作为数据成员的外部大类的指针 */
        void* GetEnclosingObject()  { return fEnclosingObject; }
        void SetEnclosingObject(void* obj) { fEnclosingObject = obj; }

		/* 返回队列中的前一个元素和后一个元素 */
        OSQueueElem* Next() { return fNext; }
        OSQueueElem* Prev() { return fPrev; }
		/* 返回当前队列指针 */
        OSQueue* InQueue()  { return fQueue; }
		/* 定义见下面末端 */
		/* 借助当前队列,移去当前队列元素,实质等同OSQueue::Remove() */

		/** 本类中唯一一个重要的函数 **/
        inline void Remove();

    private:

		/* 如何理解这两个变量? */
        OSQueueElem*    fNext; /* 前一个队列元素 */
        OSQueueElem*    fPrev; /* 后一个队列元素 */
		/* 队列指针 */
        OSQueue *       fQueue;
		/* 如何理解?关联对象的指针!外面基类(不是OSQueue)的指针或相关的字符串,当在基类中定义OSQueueElem作为一个数据成员要用到,参见Task::Task(),OSFileSource.h/cpp.
		   这种用法比较灵活 */
        void*           fEnclosingObject;

		/* 该队列元素关联的队列 */
        friend class    OSQueue;
};

/* 该类描述了队列级别上的元素添加和删除操作 */
class OSQueue {
    public:
        OSQueue();
        ~OSQueue() {}

		/************* 这两个函数十分重要!! ************/
		/* 将入参补在当前队列的锚点队列元素后面 */ 
        void            EnQueue(OSQueueElem* object);
		/* 删除并返回锚点队列元素的前一个元素 */
        OSQueueElem*    DeQueue();
		/************* 这两个函数十分重要!! ************/

		/* 得到锚点元素的前一个元素和后一个元素 */
        OSQueueElem*    GetHead() { if (fLength > 0) return fSentinel.fPrev; return NULL; }
        OSQueueElem*    GetTail() { if (fLength > 0) return fSentinel.fNext; return NULL; }
		/* 得到队列长度  */
        UInt32          GetLength() { return fLength; }

        /* 从当前队列中移去入参 */
		/* 注意比较与 */
        void            Remove(OSQueueElem* object);

#if OSQUEUETESTING
        static Bool16       Test();
#endif

    protected:

		/* 队列中的锚点元素(注意此时不是队列元素指针) */
		/* 注意该量如何理解? */
        OSQueueElem     fSentinel;
		/* 队列长度 */
        UInt32          fLength;
};

/*************  下面的两个类在实际中经常用 ****************/

/* 该类描述了队列遍历它的元素的行为 */
/* 该类描述了队列类元素的前移操作和指针是否为空的判断,很有代表性 */
class OSQueueIter
{
    public:
		/* 设置私有成员的值,默认当前队列元素从首元素开始 */
        OSQueueIter(OSQueue* inQueue) : fQueueP(inQueue), fCurrentElemP(inQueue->GetHead()) {}
		/* 指定队列对象及其起始元素 */
        OSQueueIter(OSQueue* inQueue, OSQueueElem* startElemP ) : fQueueP(inQueue)
            {
                if ( startElemP )
                {   /* 一定要确保给出的第二个入参是当前队列(第一个入参)中的成员 */
					Assert( startElemP->IsMember(*inQueue ) );
					/* 将指定队列元素设为队列起始元素 */
                    fCurrentElemP = startElemP;  
                }
                else
					/* 若第二个参数不是第一个参数的成员,就设置数据成员(指针)为空.此时与第一个构造函数相同 */
                    fCurrentElemP = NULL;
            }
        ~OSQueueIter() {}
        
		/* 使当前队列Queue中的当前元素指针指向Queue开头元素 */
        void            Reset() { fCurrentElemP = fQueueP->GetHead(); }
        
		/* 返回当前队列Queue中的当前元素指针 */
        OSQueueElem*    GetCurrent() { return fCurrentElemP; }
		/* 该函数处理Queue element的前移问题,主要是要时刻判断是否位于队列末尾? 详见OSQueue.cpp */
        void            Next();
        
		/* 判断当前队列Queue中的当前元素指针是否为空?找到空队列元素吗? */
        Bool16          IsDone() { return fCurrentElemP == NULL; }
        
    private:
    
		/* 指向当前队列Queue的指针 */
        OSQueue*        fQueueP;
		/* 指向当前队列中元素的指针 */
        OSQueueElem*    fCurrentElemP;
};

/* 事实上,该类使用得更多!参见Task.h */
/* 该类描述了队列阻塞(如何处理队列阻塞):加入一个队列元素,传信;删去一个队列元素,超时等待等 */
/* used in Task.cpp中的TaskThread class */
class OSQueue_Blocking
{
    public:
        OSQueue_Blocking() {}
        ~OSQueue_Blocking() {}
        
		/* 当队列长度为0时,超时等待,返回NULL;否则,删去并返回当前队列中锚点元素的前一个队列元素 */
        OSQueueElem*    DeQueueBlocking(OSThread* inCurThread, SInt32 inTimeoutInMilSecs);//would block
		/* 删去并返回当前队列中锚点元素的前一个队列元素 */
        OSQueueElem*    DeQueue();//will not block
		/* 将入参加入当前队列,并传信 */
        void            EnQueue(OSQueueElem* obj);
        
		/* used in TaskThreadPool::RemoveThreads() */
		/* 得到条件变量,要用到其中的Signal(),wait()等 */
		/* 用于TaskThreadPool::RemoveThreads() */
        OSCond*         GetCond()   { return &fCond; }
		/* 得到当前队列  */
        OSQueue*        GetQueue()  { return &fQueue; }
        
    private:

		/* 下面的条件变量和互斥锁是处理阻塞问题的必备工具 */
		/* 条件变量,当外部条件改变时,signal(),wait(),broadcast() */
        OSCond              fCond;
        OSMutex             fMutex;
		/* 当前队列 */
        OSQueue             fQueue;
};

/* 借助当前队列对象,移去当前队列元素(自身),实质等同OSQueue::Remove() */
void    OSQueueElem::Remove()
{
    if (fQueue != NULL)
        fQueue->Remove(this);
}

#endif //_OSQUEUE_H_


