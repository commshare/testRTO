
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

/* �������������Ҷ���Ԫ�ص�ǰһ���ͺ�һ��Ԫ��,���ڶ���,�ⲿ�������ָ����� */
class OSQueueElem {
    public:
        OSQueueElem(void* enclosingObject = NULL) : fNext(NULL), fPrev(NULL), fQueue(NULL),
                                                    fEnclosingObject(enclosingObject) {}
        virtual ~OSQueueElem() { Assert(fQueue == NULL); }

		/* �˴�ʹ�ó�����,�жϸ����Ķ��ж����Ƿ��ǵ�ǰ����Ԫ�صĶ��ж���? */
        Bool16 IsMember(const OSQueue& queue) { return (&queue == fQueue); }
		/* �ж϶���ָ���Ƿ�Ϊ��? */
        Bool16 IsMemberOfAnyQueue()     { return fQueue != NULL; }

		/* ��ȡ�����ð���������Ϊ���ݳ�Ա���ⲿ�����ָ�� */
        void* GetEnclosingObject()  { return fEnclosingObject; }
        void SetEnclosingObject(void* obj) { fEnclosingObject = obj; }

		/* ���ض����е�ǰһ��Ԫ�غͺ�һ��Ԫ�� */
        OSQueueElem* Next() { return fNext; }
        OSQueueElem* Prev() { return fPrev; }
		/* ���ص�ǰ����ָ�� */
        OSQueue* InQueue()  { return fQueue; }
		/* ���������ĩ�� */
		/* ������ǰ����,��ȥ��ǰ����Ԫ��,ʵ�ʵ�ͬOSQueue::Remove() */

		/** ������Ψһһ����Ҫ�ĺ��� **/
        inline void Remove();

    private:

		/* ����������������? */
        OSQueueElem*    fNext; /* ǰһ������Ԫ�� */
        OSQueueElem*    fPrev; /* ��һ������Ԫ�� */
		/* ����ָ�� */
        OSQueue *       fQueue;
		/* ������?���������ָ��!�������(����OSQueue)��ָ�����ص��ַ���,���ڻ����ж���OSQueueElem��Ϊһ�����ݳ�ԱҪ�õ�,�μ�Task::Task(),OSFileSource.h/cpp.
		   �����÷��Ƚ���� */
        void*           fEnclosingObject;

		/* �ö���Ԫ�ع����Ķ��� */
        friend class    OSQueue;
};

/* ���������˶��м����ϵ�Ԫ����Ӻ�ɾ������ */
class OSQueue {
    public:
        OSQueue();
        ~OSQueue() {}

		/************* ����������ʮ����Ҫ!! ************/
		/* ����β��ڵ�ǰ���е�ê�����Ԫ�غ��� */ 
        void            EnQueue(OSQueueElem* object);
		/* ɾ��������ê�����Ԫ�ص�ǰһ��Ԫ�� */
        OSQueueElem*    DeQueue();
		/************* ����������ʮ����Ҫ!! ************/

		/* �õ�ê��Ԫ�ص�ǰһ��Ԫ�غͺ�һ��Ԫ�� */
        OSQueueElem*    GetHead() { if (fLength > 0) return fSentinel.fPrev; return NULL; }
        OSQueueElem*    GetTail() { if (fLength > 0) return fSentinel.fNext; return NULL; }
		/* �õ����г���  */
        UInt32          GetLength() { return fLength; }

        /* �ӵ�ǰ��������ȥ��� */
		/* ע��Ƚ��� */
        void            Remove(OSQueueElem* object);

#if OSQUEUETESTING
        static Bool16       Test();
#endif

    protected:

		/* �����е�ê��Ԫ��(ע���ʱ���Ƕ���Ԫ��ָ��) */
		/* ע�����������? */
        OSQueueElem     fSentinel;
		/* ���г��� */
        UInt32          fLength;
};

/*************  �������������ʵ���о����� ****************/

/* ���������˶��б�������Ԫ�ص���Ϊ */
/* ���������˶�����Ԫ�ص�ǰ�Ʋ�����ָ���Ƿ�Ϊ�յ��ж�,���д����� */
class OSQueueIter
{
    public:
		/* ����˽�г�Ա��ֵ,Ĭ�ϵ�ǰ����Ԫ�ش���Ԫ�ؿ�ʼ */
        OSQueueIter(OSQueue* inQueue) : fQueueP(inQueue), fCurrentElemP(inQueue->GetHead()) {}
		/* ָ�����ж�������ʼԪ�� */
        OSQueueIter(OSQueue* inQueue, OSQueueElem* startElemP ) : fQueueP(inQueue)
            {
                if ( startElemP )
                {   /* һ��Ҫȷ�������ĵڶ�������ǵ�ǰ����(��һ�����)�еĳ�Ա */
					Assert( startElemP->IsMember(*inQueue ) );
					/* ��ָ������Ԫ����Ϊ������ʼԪ�� */
                    fCurrentElemP = startElemP;  
                }
                else
					/* ���ڶ����������ǵ�һ�������ĳ�Ա,���������ݳ�Ա(ָ��)Ϊ��.��ʱ���һ�����캯����ͬ */
                    fCurrentElemP = NULL;
            }
        ~OSQueueIter() {}
        
		/* ʹ��ǰ����Queue�еĵ�ǰԪ��ָ��ָ��Queue��ͷԪ�� */
        void            Reset() { fCurrentElemP = fQueueP->GetHead(); }
        
		/* ���ص�ǰ����Queue�еĵ�ǰԪ��ָ�� */
        OSQueueElem*    GetCurrent() { return fCurrentElemP; }
		/* �ú�������Queue element��ǰ������,��Ҫ��Ҫʱ���ж��Ƿ�λ�ڶ���ĩβ? ���OSQueue.cpp */
        void            Next();
        
		/* �жϵ�ǰ����Queue�еĵ�ǰԪ��ָ���Ƿ�Ϊ��?�ҵ��ն���Ԫ����? */
        Bool16          IsDone() { return fCurrentElemP == NULL; }
        
    private:
    
		/* ָ��ǰ����Queue��ָ�� */
        OSQueue*        fQueueP;
		/* ָ��ǰ������Ԫ�ص�ָ�� */
        OSQueueElem*    fCurrentElemP;
};

/* ��ʵ��,����ʹ�õø���!�μ�Task.h */
/* ���������˶�������(��δ����������):����һ������Ԫ��,����;ɾȥһ������Ԫ��,��ʱ�ȴ��� */
/* used in Task.cpp�е�TaskThread class */
class OSQueue_Blocking
{
    public:
        OSQueue_Blocking() {}
        ~OSQueue_Blocking() {}
        
		/* �����г���Ϊ0ʱ,��ʱ�ȴ�,����NULL;����,ɾȥ�����ص�ǰ������ê��Ԫ�ص�ǰһ������Ԫ�� */
        OSQueueElem*    DeQueueBlocking(OSThread* inCurThread, SInt32 inTimeoutInMilSecs);//would block
		/* ɾȥ�����ص�ǰ������ê��Ԫ�ص�ǰһ������Ԫ�� */
        OSQueueElem*    DeQueue();//will not block
		/* ����μ��뵱ǰ����,������ */
        void            EnQueue(OSQueueElem* obj);
        
		/* used in TaskThreadPool::RemoveThreads() */
		/* �õ���������,Ҫ�õ����е�Signal(),wait()�� */
		/* ����TaskThreadPool::RemoveThreads() */
        OSCond*         GetCond()   { return &fCond; }
		/* �õ���ǰ����  */
        OSQueue*        GetQueue()  { return &fQueue; }
        
    private:

		/* ��������������ͻ������Ǵ�����������ıر����� */
		/* ��������,���ⲿ�����ı�ʱ,signal(),wait(),broadcast() */
        OSCond              fCond;
        OSMutex             fMutex;
		/* ��ǰ���� */
        OSQueue             fQueue;
};

/* ������ǰ���ж���,��ȥ��ǰ����Ԫ��(����),ʵ�ʵ�ͬOSQueue::Remove() */
void    OSQueueElem::Remove()
{
    if (fQueue != NULL)
        fQueue->Remove(this);
}

#endif //_OSQUEUE_H_


