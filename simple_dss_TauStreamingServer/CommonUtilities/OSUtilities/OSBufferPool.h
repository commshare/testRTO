
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
		/* 得到总的缓存片段总数 */
        UInt32  GetTotalNumBuffers() { return fTotNumBuffers; }
		/* 得到可用的缓存片段总数 */
        UInt32  GetNumAvailableBuffers() { return fQueue.GetLength(); }
        
        // All these functions are thread-safe
        
        // Gets a buffer out of the pool. This buffer must be replaced
        // by calling Put when you are done with it.
		/* 从buffer池中取出一个buffer,它必须是被Put()放入buffer池的 */
        void*   Get();
        
        // Returns a buffer (retreived by Get) back to the pool.
		/* 将指定的buffer(通过Get()检索得到)放回缓存池 */
        void    Put(void* inBuffer);
    
    private:
    
		/* 缓存池互斥锁 */
        OSMutex fMutex;
		/* 所有的缓存片段组成的队列 */
        OSQueue fQueue;
		/* 每片缓存真实长度 */
        UInt32  fBufSize;
		/* 缓存片段总数,赋值参见OSBufferPool::Get(),好像没有多大作用,队列长度就足够了?? */
        UInt32  fTotNumBuffers;
};

#endif //__OS_BUFFER_POOL_H__
