
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPSocketPool.h
Description: Object that creates & maintains UDP socket pairs in a pool.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef __UDPSOCKETPOOL_H__
#define __UDPSOCKETPOOL_H__


#include "UDPDemuxer.h"
#include "UDPSocket.h"
#include "OSMutex.h"
#include "OSQueue.h"


class UDPSocketPair;

class UDPSocketPool
{
    public:
    
        UDPSocketPool() : fMutex() {}
        virtual ~UDPSocketPool() {}
        
        //Skanky(招人烦的) access to member data
        OSMutex*    GetMutex()          { return &fMutex; }
        OSQueue*    GetSocketQueue()    { return &fUDPQueue; }
        
        //Gets a UDP socket out of the pool. 
        //inIPAddr = IP address you'd like this pair to be bound to.
        //inPort = port you'd like this pair to be bound to, or 0 if you don't care
        //inSrcIPAddr = srcIP address of incoming packets for the demuxer(refer to note in UDPSocket.h).
        //inSrcPort = src port of incoming packets for the demuxer.
        //This may return NULL if no pair is available that meets the criteria.
		/* 对源IP地址和端口分情形: 当至少一个非零时,通过循环查找出符合要求的不在demuxer中的UDPSocketPair;否则新建符合要求的UDPSocketPair */
        UDPSocketPair*  GetUDPSocketPair(UInt32 inIPAddr, UInt16 inPort,
                                            UInt32 inSrcIPAddr, UInt16 inSrcPort);
        
        //When done using a UDP socket pair retrieved via GetUDPSocketPair, you must
        //call this function. Doing so tells the pool which UDP sockets are in use,
        //keeping the number of UDP sockets allocated at a minimum.
		/* 注意原理类似Critical_Section,减少UDPSocketPair引用计数.当引用计数为0时,从队列中删去队列元素,并销毁UDPSocketPair */
        void ReleaseUDPSocketPair(UDPSocketPair* inPair);

		/* 构造UDP Socket Pair,并绑定到指定的IP地址和端口.当输入端口非零时,只有一次绑定成功机会;当输入端口为零时,
		可循环查找可以绑定上的端口.最后返回绑定后的UDP Socket Pair,否则返回NULL */
        UDPSocketPair*  CreateUDPSocketPair(UInt32 inAddr, UInt16 inPort);
        
    protected:
    
        //Because UDPSocket is a base class, and this pool class is intended to be
        //a general purpose class for all types of UDP sockets (reflector, standard),
        //there must be a virtual fuction for actually constructing the derived UDP sockets
		/* 这三个函数的源码实现参见QTSServer.cpp中RTPSocketPool类 */
        virtual UDPSocketPair*  ConstructUDPSocketPair() = 0;
        virtual void            DestructUDPSocketPair(UDPSocketPair* inPair) = 0;
        
        virtual void            SetUDPSocketOptions(UDPSocketPair* /*inPair*/) {}
    
    private:
    
		/* UDP port range:6970~65535,参见UDPSocketPool::CreateUDPSocketPair() */
        enum
        {
            kLowestUDPPort = 6970,  //UInt16
            kHighestUDPPort = 65535 //UInt16
        };
    
		/* 由UDPSocketPair组成的队列 */
        OSQueue fUDPQueue;
		/* 该UDPSocketPool对应的互斥锁 */
        OSMutex fMutex;
};

/* 将两个UDPSocket组合成UDPsocketPair的类,它将被作为一个队列元放入UDPSocket Pool,并被UDPSocketPool统一维护和管理 */
class UDPSocketPair
{
    public:
        
        UDPSocketPair(UDPSocket* inSocketA, UDPSocket* inSocketB)
            : fSocketA(inSocketA), fSocketB(inSocketB), fRefCount(0), fElem() {fElem.SetEnclosingObject(this);/* 设置Queue elem所在的类对象指针 */}
        ~UDPSocketPair() {}
    
		//accessors
        UDPSocket*  GetSocketA() { return fSocketA; }
        UDPSocket*  GetSocketB() { return fSocketB; }
        
    private:
    
		/* 将两个面向无连接的UDP Socket配对 */
        UDPSocket*  fSocketA;
        UDPSocket*  fSocketB;
		/* 引用计数,因为可能复用 */
        UInt32      fRefCount;
		/* 充作队列元 */
        OSQueueElem fElem;
        
        friend class UDPSocketPool;
};
#endif // __UDPSOCKETPOOL_H__

