
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSBufferPool.h
Description: Provide fast access to fixed size buffers, used in RTPPacketResender.cpp.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef __OS_BUFFER_POOL_H__
#define __OS_BUFFER_POOL_H__

#include "OSQueue.h"
#include "OSMutex.h"

class OSBufferPool
{
    public:
    
        OSBufferPool(UInt32 inBufferSize) : fBufSize(inBufferSize), fTotNumBuffers(0) {}
        
        // This object currently *does not* clean up for itself when you destruct it!
        ~OSBufferPool() {}
        
        // ACCESSORS
		/* �õ��ܵĻ���Ƭ������ */
        UInt32  GetTotalNumBuffers() { return fTotNumBuffers; }
		/* �õ����õĻ���Ƭ������ */
        UInt32  GetNumAvailableBuffers() { return fQueue.GetLength(); }
        
        // All these functions are thread-safe
        
        // Gets a buffer out of the pool. This buffer must be replaced
        // by calling Put when you are done with it.
		/* ��buffer����ȡ��һ��buffer,�������Ǳ�Put()����buffer�ص� */
        void*   Get();
        
        // Returns a buffer (retreived by Get) back to the pool.
		/* ��ָ����buffer(ͨ��Get()�����õ�)�Żػ���� */
        void    Put(void* inBuffer);
    
    private:
    
		/* ����ػ����� */
        OSMutex fMutex;
		/* ���еĻ���Ƭ����ɵĶ��� */
        OSQueue fQueue;
		/* ÿƬ������ʵ���� */
        UInt32  fBufSize;
		/* ����Ƭ������,��ֵ�μ�OSBufferPool::Get(),����û�ж������,���г��Ⱦ��㹻��?? */
        UInt32  fTotNumBuffers;
};

#endif //__OS_BUFFER_POOL_H__
