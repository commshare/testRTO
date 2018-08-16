/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSHeap.cpp
Description: Provide a heap abstraction to use as a idle timer.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <string.h>
#include "OSHeap.h"
#include "OSMemory.h"


/* 初始化函数,创建指定大小的二维数组 */
OSHeap::OSHeap(UInt32 inStartSize)
: fFreeIndex(1)/******* NOTE! *********/
{
	/* 注意数据保护,最小值必须为2,因为fFreeIndex最小值为1. */
    if (inStartSize < 2)
        fArraySize = 2;
    else
        fArraySize = inStartSize;
        
    fHeap = NEW OSHeapElem*[fArraySize];
}

/* 使用冒泡法插入入参到当前OSHeap二维数组中的恰当位置(使按OSHeapElem的值从头开始升序排列) */
void OSHeap::Insert(OSHeapElem* inElem)
{
    Assert(inElem != NULL);
    
	/* 创建2倍大小的OSHeap */
    if ((fHeap == NULL) || (fFreeIndex == fArraySize))
    {
        fArraySize *= 2;
        OSHeapElem** tempArray = NEW OSHeapElem*[fArraySize];
		/* 复制进原来的数组 */
        if ((fHeap != NULL) && (fFreeIndex > 1))
            memcpy(tempArray, fHeap, sizeof(OSHeapElem*) * fFreeIndex);
            
        delete [] fHeap;
        fHeap = tempArray;
    }
    
    Assert(fHeap != NULL);
	/* 确保入参没有在其它任何Heap中 */
    Assert(inElem->fCurrentHeap == NULL);
	/* 确保当前Heap未超过最大元素个数 */
    Assert(fArraySize > fFreeIndex);
    
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif

    //insert the element into the last leaf of the tree
	/* 将入参插入OSHeap中的最后元位置 */
    fHeap[fFreeIndex] = inElem;
    
    //bubble(冒泡法) the new element up to its proper place in the heap
    
    //start at the last leaf of the tree
	/* 使用冒泡法逐步轮换,直到OSHeapElem值最恰当的位置 */
    UInt32 swapPos = fFreeIndex;
    while (swapPos > 1)
    {
        //move up the chain until we get to the root, bubbling this new element
        //to its proper place in the tree
		/* 冒泡法 */
        UInt32 nextSwapPos = swapPos >> 1;
        
        //if this child is greater than it's parent, we need to do the old
        //switcheroo
        if (fHeap[swapPos]->fValue < fHeap[nextSwapPos]->fValue)
        {
            OSHeapElem* temp = fHeap[swapPos];
            fHeap[swapPos] = fHeap[nextSwapPos];
            fHeap[nextSwapPos] = temp;
            swapPos = nextSwapPos;
        }
        else
            //if not, we are done!
            break;
    }
    inElem->fCurrentHeap = this;
    fFreeIndex++;
}

/* 该函数是这个OSHeap类中最重要的函数 */
/* 取出入参指定的OSHeapElem,利用the Heapify algorithm重排剩下的数组 */
OSHeapElem* OSHeap::Extract(UInt32 inIndex)
{
	/* 确保OSHeapElement的指针数组非空,和入参的合法性 */
    if ((fHeap == NULL) || (fFreeIndex <= inIndex))
        return NULL;
        
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif
    
    //store a reference to the element we want to extract
	/* 将要找的HeapElement保存起来,并最后以函数值返回 */
    OSHeapElem* victim = fHeap[inIndex];
	/* 确保这个HeapElement所在的Heap就是当前的Heap,这个事显然的 */
    Assert(victim->fCurrentHeap == this);
	/* 使指定索引的OSHeapElem脱离OSHeap */
    victim->fCurrentHeap = NULL;
    
    //but now we need to preserve this heuristic(维持惯例). We do this by taking
    //the last leaf, putting it at the empty position, then heapifying that chain

	/* 将最后一个OSHeapElem*填充取出指定入参后的空位,利用the Heapify algorithm重排该数组 */
    fHeap[inIndex] = fHeap[fFreeIndex - 1];
    fFreeIndex--;
    
    //The following is an implementation of the Heapify algorithm (CLR 7.1 pp 143 )
    //The gist is that this new item at the top of the heap needs to be bubbled down
    //until it is bigger than its two children, therefore maintaining the heap property.
	/* Introduction to Algorithms,by Thomas H. Cormen, Charles E. Leiserson and Ronald L. Rivest,June 1990
	1048 pp., 271 illus */
    
    UInt32 parent = inIndex;
    while (parent < fFreeIndex)
    {
        //which is bigger? parent or left child?
        UInt32 greatest = parent;
        UInt32 leftChild = parent * 2;
        if ((leftChild < fFreeIndex) && (fHeap[leftChild]->fValue < fHeap[parent]->fValue))
            greatest = leftChild;

        //which is bigger? the biggest so far or the right child?
        UInt32 rightChild = (parent * 2) + 1;
        if ((rightChild < fFreeIndex) && (fHeap[rightChild]->fValue < fHeap[greatest]->fValue))
            greatest = rightChild;
         
        //if the parent is in fact bigger than its two children, we have bubbled
        //this element down far enough
        if (greatest == parent)
            break;
            
        //parent is not bigger than at least one of its two children, so swap the parent
        //with the largest item.
        OSHeapElem* temp = fHeap[parent];
        fHeap[parent] = fHeap[greatest];
        fHeap[greatest] = temp;
        
        //now heapify the remaining chain
        parent = greatest;
    }
    
    return victim;
}

/* 先确定入参所在的数组元素索引,再调用OSHeap::Extract()从OSHeap中移除指定的OSHeapElem,利用the Heapify algorithm重排剩下的数组 */
OSHeapElem* OSHeap::Remove(OSHeapElem* elem)
{
	/* 当Heap有元素时才可移去该HeapElem */
    if ((fHeap == NULL) || (fFreeIndex == 1))
        return NULL;
        
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif

    //first attempt to locate this element in the heap
	/* HeapElement索引fFreeIndex总从1开始计数 */
    UInt32 theIndex = 1;
	/* 从头开始寻找指定index的HeapElement,找到就退出循环 */
    for ( ; theIndex < fFreeIndex; theIndex++)
        if (elem == fHeap[theIndex])
            break;
            
    //either we've found it, or this is a bogus element(伪元)
	/* 假如没有找到,就返回 */
    if (theIndex == fFreeIndex)
        return NULL;
        
    return Extract(theIndex);
}


#if _OSHEAP_TESTING_

void OSHeap::SanityCheck(UInt32 root)
{
    //make sure root is greater than both its children. Do so recursively
    if (root < fFreeIndex)
    {
        if ((root * 2) < fFreeIndex)
        {
            Assert(fHeap[root]->fValue <= fHeap[root * 2]->fValue);
            SanityCheck(root * 2);
        }
        if (((root * 2) + 1) < fFreeIndex)
        {
            Assert(fHeap[root]->fValue <= fHeap[(root * 2) + 1]->fValue);
            SanityCheck((root * 2) + 1);
        }
    }
}


Bool16 OSHeap::Test()
{
    OSHeap victim(2);
    OSHeapElem elem1;
    OSHeapElem elem2;
    OSHeapElem elem3;
    OSHeapElem elem4;
    OSHeapElem elem5;
    OSHeapElem elem6;
    OSHeapElem elem7;
    OSHeapElem elem8;
    OSHeapElem elem9;

    OSHeapElem* max = victim.ExtractMin();
    if (max != NULL)
        return false;
        
    elem1.SetValue(100);
    victim.Insert(&elem1);
    
    max = victim.ExtractMin();
    if (max != &elem1)
        return false;
    max = victim.ExtractMin();
    if (max != NULL)
        return false;
    
    elem1.SetValue(100);
    elem2.SetValue(80);
    
    victim.Insert(&elem1);
    victim.Insert(&elem2);
    
    max = victim.ExtractMin();
    if (max != &elem2)
        return false;
    max = victim.ExtractMin();
    if (max != &elem1)
        return false;
    max = victim.ExtractMin();
    if (max != NULL)
        return false;
    
    victim.Insert(&elem2);
    victim.Insert(&elem1);

    max = victim.ExtractMin();
    if (max != &elem2)
        return false;
    max = victim.ExtractMin();
    if (max != &elem1)
        return false;
        
    elem3.SetValue(70);
    elem4.SetValue(60);

    victim.Insert(&elem3);
    victim.Insert(&elem1);
    victim.Insert(&elem2);
    victim.Insert(&elem4);
    
    max = victim.ExtractMin();
    if (max != &elem4)
        return false;
    max = victim.ExtractMin();
    if (max != &elem3)
        return false;
    max = victim.ExtractMin();
    if (max != &elem2)
        return false;
    max = victim.ExtractMin();
    if (max != &elem1)
        return false;

    elem5.SetValue(50);
    elem6.SetValue(40);
    elem7.SetValue(30);
    elem8.SetValue(20);
    elem9.SetValue(10);

    victim.Insert(&elem5);
    victim.Insert(&elem3);
    victim.Insert(&elem1);
    
    max = victim.ExtractMin();
    if (max != &elem5)
        return false;
    
    victim.Insert(&elem4);
    victim.Insert(&elem2);

    max = victim.ExtractMin();
    if (max != &elem4)
        return false;
    max = victim.ExtractMin();
    if (max != &elem3)
        return false;
    
    victim.Insert(&elem2);

    max = victim.ExtractMin();
    if (max != &elem2)
        return false;

    victim.Insert(&elem2);
    victim.Insert(&elem6);

    max = victim.ExtractMin();
    if (max != &elem6)
        return false;

    victim.Insert(&elem6);
    victim.Insert(&elem3);
    victim.Insert(&elem4);
    victim.Insert(&elem5);

    max = victim.ExtractMin();
    if (max != &elem6)
        return false;
    max = victim.ExtractMin();
    if (max != &elem5)
        return false;

    victim.Insert(&elem8);
    max = victim.ExtractMin();
    if (max != &elem8)
        return false;
    max = victim.ExtractMin();
    if (max != &elem4)
        return false;
        
    victim.Insert(&elem5);
    victim.Insert(&elem4);
    victim.Insert(&elem9);
    victim.Insert(&elem7);
    victim.Insert(&elem8);
    victim.Insert(&elem6);

    max = victim.ExtractMin();
    if (max != &elem9)
        return false;
    max = victim.ExtractMin();
    if (max != &elem8)
        return false;
    max = victim.ExtractMin();
    if (max != &elem7)
        return false;
    max = victim.ExtractMin();
    if (max != &elem6)
        return false;
    max = victim.ExtractMin();
    if (max != &elem5)
        return false;
    max = victim.ExtractMin();
    if (max != &elem4)
        return false;
    max = victim.ExtractMin();
    if (max != &elem3)
        return false;
    max = victim.ExtractMin();
    if (max != &elem2)
        return false;
    max = victim.ExtractMin();
    if (max != &elem2)
        return false;
    max = victim.ExtractMin();
    if (max != &elem1)
        return false;
    max = victim.ExtractMin();
    if (max != NULL)
        return false;
        
    victim.Insert(&elem1);
    victim.Insert(&elem2);
    victim.Insert(&elem3);
    victim.Insert(&elem4);
    victim.Insert(&elem5);
    victim.Insert(&elem6);
    victim.Insert(&elem7);
    victim.Insert(&elem8);
    victim.Insert(&elem9);
    
    max = victim.Remove(&elem7);
    if (max != &elem7)
        return false;
    max = victim.Remove(&elem9);
    if (max != &elem9)
        return false;
    max = victim.ExtractMin();
    if (max != &elem8)
        return false;
    max = victim.Remove(&elem2);
    if (max != &elem2)
        return false;
    max = victim.Remove(&elem2);
    if (max != NULL)
        return false;
    max = victim.Remove(&elem8);
    if (max != NULL)
        return false;
    max = victim.Remove(&elem5);
    if (max != &elem5)
        return false;
    max = victim.Remove(&elem6);
    if (max != &elem6)
        return false;
    max = victim.Remove(&elem1);
    if (max != &elem1)
        return false;
    max = victim.ExtractMin();
    if (max != &elem4)
        return false;
    max = victim.Remove(&elem1);
    if (max != NULL)
        return false;
    
    return true;
}
#endif
