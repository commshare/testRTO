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



/* ���統ǰ����Ϊ��,�½�һ��QueueElem(����������),������ʵ�����ݵ�ַ;����,���ص�ǰ����Ԫ������Ķ��� */
void*   OSBufferPool::Get()
{
    OSMutexLocker locker(&fMutex);
	/* �����Queue����Ϊ0,���½�һ��QueueElem(����������),���ò��������������EnclosingObject */
    if (fQueue.GetLength() == 0)
    {
		/* �ۼƻ���Ƭ������ */
        fTotNumBuffers++;
		/* �½�����Ƭ�� */
        char* theNewBuf = NEW char[fBufSize + sizeof(OSQueueElem)];
        
        // We need to construct a Queue Element, but we don't actually need
        // to use it in this function, so to avoid a compiler warning just
        // don't assign the result to anything.
		/* ����÷�����ֵ�? ���Ǳ�׼C++��new�ĵ������÷�:�����ڸ������ڴ��г�ʼ������*/
		/* ����ָ������Ԫ������EnclosingObject�Ǹö�������,���������Ļ��� */
        (void)new (theNewBuf) OSQueueElem(theNewBuf + sizeof(OSQueueElem));

		/* ����ʵ�ʵĻ���Ƭ�ε�ַ */
        return theNewBuf + sizeof(OSQueueElem);
    }
	/* ����ɾȥ��ǰ����Ԫ��,������������������ */
    return fQueue.DeQueue()->GetEnclosingObject();
}

/* �����ָ��֮ǰ����һ������Ϊsizeof(OSQueueElem)�Ļ�������,�ٲ��뵱ǰ������ */
void OSBufferPool::Put(void* inBuffer)
{
    OSMutexLocker locker(&fMutex);
	/* ����Ƭ����ǰ������sizeof(OSQueueElem),���Զ���Ԫ���� */
    fQueue.EnQueue((OSQueueElem*)((char*)inBuffer - sizeof(OSQueueElem)));
}
