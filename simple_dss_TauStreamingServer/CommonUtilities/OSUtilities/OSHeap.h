
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
    
		/* 注意OSHeap起始默认大小就是1024，以后以2倍增加 */
        enum
        {
            kDefaultStartSize = 1024 //UInt32
        };
        
		/* 初始化函数,创建指定大小的二维数组 */
        OSHeap(UInt32 inStartSize = kDefaultStartSize);
        ~OSHeap() { if (fHeap != NULL) delete fHeap; }
        
        //ACCESSORS

		/* 得到当前Heap二维数组的元素个数 */
        UInt32      CurrentHeapSize() { return fFreeIndex - 1; }
		/* used in TaskThread::WaitForTask() */
		/* 得到二维数组中的第2个OSHeapElem*,但不从二维数组中取出 */
        OSHeapElem* PeekMin() { if (CurrentHeapSize() > 0) return fHeap[1]; return NULL; }
        
        //MODIFIERS
        
        //These are the two primary operations supported by the heap

        //abstract data type. both run in log(n) time.

		/* 使用冒泡法插入入参到当前OSHeap二维数组中的恰当位置(使按OSHeapElem的值从头开始升序排列) */
        void            Insert(OSHeapElem*  inElem);
		/* used in TaskThread::WaitForTask() */
		/* 提取二维数组中的第2个OSHeapElem*,注意剩下的二维数组要重排 */
        OSHeapElem*     ExtractMin() { return Extract(1); }


        //removes specified element from the heap* array
		/* 先确定入参所在的数组元素索引,再调用OSHeap::Extract从OSHeap中移除指定的SHeapElem,利用the Heapify algorithm重排剩下的数组 */
        OSHeapElem*     Remove(OSHeapElem* elem);
        
#if _OSHEAP_TESTING_
        //returns true if it passed the test, false otherwise
        static Bool16       Test();
#endif
    
    private:
    
		/* 抽取二维数组中指定索引的OSHeapElem指针, 该函数是这个OSHeap类中最重要的函数 */ 
		/* 取出入参指定的OSHeapElem,利用the Heapify algorithm重排剩下的数组 */
        OSHeapElem*     Extract(UInt32 index);
    
#if _OSHEAP_TESTING_
        //verifies that the heap is in fact a heap
        void            SanityCheck(UInt32 root);
#endif
    
		/* 代表Heap元的二维数组 */
        OSHeapElem**    fHeap;
		/* 代表当前Heap元数组的元素个数,从1开始计数 */
        UInt32          fFreeIndex;
		/* Heap元的二维数组的大小,参见OSHeap::OSHeap() */
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
		/* 类似OSQueue,判断当前依赖的OSHeap是否为空?务必确保是某个Heap的元素!(可以望名知义) */
        Bool16  IsMemberOfAnyHeap()     { return fCurrentHeap != NULL; }
        
    private:
    
		/* 表示HeapElement的值大小 */
        SInt64  fValue;
		/* 类似OSQueue,指向需要的更大的包含类的指针 */
        void* fEnclosingObject;
		/* 该OSHeapElem所属的OSHeap */
        OSHeap* fCurrentHeap;
        
        friend class OSHeap;
};
#endif //_OSHEAP_H_
