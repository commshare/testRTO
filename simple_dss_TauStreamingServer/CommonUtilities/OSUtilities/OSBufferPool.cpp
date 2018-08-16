/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSBufferPool.cpp
Description: Provide fast access to fixed size buffers, used in RTPPacketResender.cpp.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "OSBufferPool.h"
#include "OSMemory.h"



/* 假如当前队列为空,新建一个QueueElem(包含其数据),并返回实际数据地址;否则,返回当前队列元素依存的对象 */
void*   OSBufferPool::Get()
{
    OSMutexLocker locker(&fMutex);
	/* 假如该Queue长度为0,进新建一个QueueElem(包含其数据),设置并返回其关联对象EnclosingObject */
    if (fQueue.GetLength() == 0)
    {
		/* 累计缓存片段总数 */
        fTotNumBuffers++;
		/* 新建缓存片段 */
        char* theNewBuf = NEW char[fBufSize + sizeof(OSQueueElem)];
        
        // We need to construct a Queue Element, but we don't actually need
        // to use it in this function, so to avoid a compiler warning just
        // don't assign the result to anything.
		/* 这个用法好奇怪的? 这是标准C++中new的第三种用法:用于在给定的内存中初始化对象*/
		/* 创建指定队列元（它的EnclosingObject是该定长缓存,用上面分配的缓存 */
        (void)new (theNewBuf) OSQueueElem(theNewBuf + sizeof(OSQueueElem));

		/* 返回实际的缓存片段地址 */
        return theNewBuf + sizeof(OSQueueElem);
    }
	/* 否则删去当前队列元素,返回它所依存的类对象 */
    return fQueue.DeQueue()->GetEnclosingObject();
}

/* 将入参指针之前加上一个长度为sizeof(OSQueueElem)的缓存内容,再插入当前队列中 */
void OSBufferPool::Put(void* inBuffer)
{
    OSMutexLocker locker(&fMutex);
	/* 将该片缓存前部加上sizeof(OSQueueElem),再以队列元加入 */
    fQueue.EnQueue((OSQueueElem*)((char*)inBuffer - sizeof(OSQueueElem)));
}
