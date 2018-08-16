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


/* ��ʼ������,����ָ����С�Ķ�ά���� */
OSHeap::OSHeap(UInt32 inStartSize)
: fFreeIndex(1)/******* NOTE! *********/
{
	/* ע�����ݱ���,��Сֵ����Ϊ2,��ΪfFreeIndex��СֵΪ1. */
    if (inStartSize < 2)
        fArraySize = 2;
    else
        fArraySize = inStartSize;
        
    fHeap = NEW OSHeapElem*[fArraySize];
}

/* ʹ��ð�ݷ�������ε���ǰOSHeap��ά�����е�ǡ��λ��(ʹ��OSHeapElem��ֵ��ͷ��ʼ��������) */
void OSHeap::Insert(OSHeapElem* inElem)
{
    Assert(inElem != NULL);
    
	/* ����2����С��OSHeap */
    if ((fHeap == NULL) || (fFreeIndex == fArraySize))
    {
        fArraySize *= 2;
        OSHeapElem** tempArray = NEW OSHeapElem*[fArraySize];
		/* ���ƽ�ԭ�������� */
        if ((fHeap != NULL) && (fFreeIndex > 1))
            memcpy(tempArray, fHeap, sizeof(OSHeapElem*) * fFreeIndex);
            
        delete [] fHeap;
        fHeap = tempArray;
    }
    
    Assert(fHeap != NULL);
	/* ȷ�����û���������κ�Heap�� */
    Assert(inElem->fCurrentHeap == NULL);
	/* ȷ����ǰHeapδ�������Ԫ�ظ��� */
    Assert(fArraySize > fFreeIndex);
    
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif

    //insert the element into the last leaf of the tree
	/* ����β���OSHeap�е����Ԫλ�� */
    fHeap[fFreeIndex] = inElem;
    
    //bubble(ð�ݷ�) the new element up to its proper place in the heap
    
    //start at the last leaf of the tree
	/* ʹ��ð�ݷ����ֻ�,ֱ��OSHeapElemֵ��ǡ����λ�� */
    UInt32 swapPos = fFreeIndex;
    while (swapPos > 1)
    {
        //move up the chain until we get to the root, bubbling this new element
        //to its proper place in the tree
		/* ð�ݷ� */
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

/* �ú��������OSHeap��������Ҫ�ĺ��� */
/* ȡ�����ָ����OSHeapElem,����the Heapify algorithm����ʣ�µ����� */
OSHeapElem* OSHeap::Extract(UInt32 inIndex)
{
	/* ȷ��OSHeapElement��ָ������ǿ�,����εĺϷ��� */
    if ((fHeap == NULL) || (fFreeIndex <= inIndex))
        return NULL;
        
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif
    
    //store a reference to the element we want to extract
	/* ��Ҫ�ҵ�HeapElement��������,������Ժ���ֵ���� */
    OSHeapElem* victim = fHeap[inIndex];
	/* ȷ�����HeapElement���ڵ�Heap���ǵ�ǰ��Heap,�������Ȼ�� */
    Assert(victim->fCurrentHeap == this);
	/* ʹָ��������OSHeapElem����OSHeap */
    victim->fCurrentHeap = NULL;
    
    //but now we need to preserve this heuristic(ά�ֹ���). We do this by taking
    //the last leaf, putting it at the empty position, then heapifying that chain

	/* �����һ��OSHeapElem*���ȡ��ָ����κ�Ŀ�λ,����the Heapify algorithm���Ÿ����� */
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

/* ��ȷ��������ڵ�����Ԫ������,�ٵ���OSHeap::Extract()��OSHeap���Ƴ�ָ����OSHeapElem,����the Heapify algorithm����ʣ�µ����� */
OSHeapElem* OSHeap::Remove(OSHeapElem* elem)
{
	/* ��Heap��Ԫ��ʱ�ſ���ȥ��HeapElem */
    if ((fHeap == NULL) || (fFreeIndex == 1))
        return NULL;
        
#if _OSHEAP_TESTING_
    SanityCheck(1);
#endif

    //first attempt to locate this element in the heap
	/* HeapElement����fFreeIndex�ܴ�1��ʼ���� */
    UInt32 theIndex = 1;
	/* ��ͷ��ʼѰ��ָ��index��HeapElement,�ҵ����˳�ѭ�� */
    for ( ; theIndex < fFreeIndex; theIndex++)
        if (elem == fHeap[theIndex])
            break;
            
    //either we've found it, or this is a bogus element(αԪ)
	/* ����û���ҵ�,�ͷ��� */
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
