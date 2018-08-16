
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSHeap.h
Description: Provide a heap abstraction to use as a idle timer.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 

#ifndef _OSHEAP_H_
#define _OSHEAP_H_

#define _OSHEAP_TESTING_ 0

#include "OSCond.h"

class OSHeapElem;

class OSHeap
{
    public:
    
		/* ע��OSHeap��ʼĬ�ϴ�С����1024���Ժ���2������ */
        enum
        {
            kDefaultStartSize = 1024 //UInt32
        };
        
		/* ��ʼ������,����ָ����С�Ķ�ά���� */
        OSHeap(UInt32 inStartSize = kDefaultStartSize);
        ~OSHeap() { if (fHeap != NULL) delete fHeap; }
        
        //ACCESSORS

		/* �õ���ǰHeap��ά�����Ԫ�ظ��� */
        UInt32      CurrentHeapSize() { return fFreeIndex - 1; }
		/* used in TaskThread::WaitForTask() */
		/* �õ���ά�����еĵ�2��OSHeapElem*,�����Ӷ�ά������ȡ�� */
        OSHeapElem* PeekMin() { if (CurrentHeapSize() > 0) return fHeap[1]; return NULL; }
        
        //MODIFIERS
        
        //These are the two primary operations supported by the heap

        //abstract data type. both run in log(n) time.

		/* ʹ��ð�ݷ�������ε���ǰOSHeap��ά�����е�ǡ��λ��(ʹ��OSHeapElem��ֵ��ͷ��ʼ��������) */
        void            Insert(OSHeapElem*  inElem);
		/* used in TaskThread::WaitForTask() */
		/* ��ȡ��ά�����еĵ�2��OSHeapElem*,ע��ʣ�µĶ�ά����Ҫ���� */
        OSHeapElem*     ExtractMin() { return Extract(1); }


        //removes specified element from the heap* array
		/* ��ȷ��������ڵ�����Ԫ������,�ٵ���OSHeap::Extract��OSHeap���Ƴ�ָ����SHeapElem,����the Heapify algorithm����ʣ�µ����� */
        OSHeapElem*     Remove(OSHeapElem* elem);
        
#if _OSHEAP_TESTING_
        //returns true if it passed the test, false otherwise
        static Bool16       Test();
#endif
    
    private:
    
		/* ��ȡ��ά������ָ��������OSHeapElemָ��, �ú��������OSHeap��������Ҫ�ĺ��� */ 
		/* ȡ�����ָ����OSHeapElem,����the Heapify algorithm����ʣ�µ����� */
        OSHeapElem*     Extract(UInt32 index);
    
#if _OSHEAP_TESTING_
        //verifies that the heap is in fact a heap
        void            SanityCheck(UInt32 root);
#endif
    
		/* ����HeapԪ�Ķ�ά���� */
        OSHeapElem**    fHeap;
		/* ����ǰHeapԪ�����Ԫ�ظ���,��1��ʼ���� */
        UInt32          fFreeIndex;
		/* HeapԪ�Ķ�ά����Ĵ�С,�μ�OSHeap::OSHeap() */
        UInt32          fArraySize;
};

class OSHeapElem
{
    public:
        OSHeapElem(void* enclosingObject = NULL)
            : fValue(0), fEnclosingObject(enclosingObject), fCurrentHeap(NULL) {}
        ~OSHeapElem() {}
        
        //This data structure emphasizes performance over extensibility
        //If it were properly object-oriented, the compare routine would
        //be virtual. However, to avoid the use of v-functions in this data
        //structure, I am assuming that the objects are compared using a 64 bit number.
        //

		//accessors and modifiers
        void    SetValue(SInt64 newValue) { fValue = newValue; }
        SInt64  GetValue()              { return fValue; }
        void*   GetEnclosingObject()    { return fEnclosingObject; }
		void	SetEnclosingObject(void* obj) { fEnclosingObject = obj; }
		/* ����OSQueue,�жϵ�ǰ������OSHeap�Ƿ�Ϊ��?���ȷ����ĳ��Heap��Ԫ��!(��������֪��) */
        Bool16  IsMemberOfAnyHeap()     { return fCurrentHeap != NULL; }
        
    private:
    
		/* ��ʾHeapElement��ֵ��С */
        SInt64  fValue;
		/* ����OSQueue,ָ����Ҫ�ĸ���İ������ָ�� */
        void* fEnclosingObject;
		/* ��OSHeapElem������OSHeap */
        OSHeap* fCurrentHeap;
        
        friend class OSHeap;
};
#endif //_OSHEAP_H_
