
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
        
        //Skanky(���˷���) access to member data
        OSMutex*    GetMutex()          { return &fMutex; }
        OSQueue*    GetSocketQueue()    { return &fUDPQueue; }
        
        //Gets a UDP socket out of the pool. 
        //inIPAddr = IP address you'd like this pair to be bound to.
        //inPort = port you'd like this pair to be bound to, or 0 if you don't care
        //inSrcIPAddr = srcIP address of incoming packets for the demuxer(refer to note in UDPSocket.h).
        //inSrcPort = src port of incoming packets for the demuxer.
        //This may return NULL if no pair is available that meets the criteria.
		/* ��ԴIP��ַ�Ͷ˿ڷ�����: ������һ������ʱ,ͨ��ѭ�����ҳ�����Ҫ��Ĳ���demuxer�е�UDPSocketPair;�����½�����Ҫ���UDPSocketPair */
        UDPSocketPair*  GetUDPSocketPair(UInt32 inIPAddr, UInt16 inPort,
                                            UInt32 inSrcIPAddr, UInt16 inSrcPort);
        
        //When done using a UDP socket pair retrieved via GetUDPSocketPair, you must
        //call this function. Doing so tells the pool which UDP sockets are in use,
        //keeping the number of UDP sockets allocated at a minimum.
		/* ע��ԭ������Critical_Section,����UDPSocketPair���ü���.�����ü���Ϊ0ʱ,�Ӷ�����ɾȥ����Ԫ��,������UDPSocketPair */
        void ReleaseUDPSocketPair(UDPSocketPair* inPair);

		/* ����UDP Socket Pair,���󶨵�ָ����IP��ַ�Ͷ˿�.������˿ڷ���ʱ,ֻ��һ�ΰ󶨳ɹ�����;������˿�Ϊ��ʱ,
		��ѭ�����ҿ��԰��ϵĶ˿�.��󷵻ذ󶨺��UDP Socket Pair,���򷵻�NULL */
        UDPSocketPair*  CreateUDPSocketPair(UInt32 inAddr, UInt16 inPort);
        
    protected:
    
        //Because UDPSocket is a base class, and this pool class is intended to be
        //a general purpose class for all types of UDP sockets (reflector, standard),
        //there must be a virtual fuction for actually constructing the derived UDP sockets
		/* ������������Դ��ʵ�ֲμ�QTSServer.cpp��RTPSocketPool�� */
        virtual UDPSocketPair*  ConstructUDPSocketPair() = 0;
        virtual void            DestructUDPSocketPair(UDPSocketPair* inPair) = 0;
        
        virtual void            SetUDPSocketOptions(UDPSocketPair* /*inPair*/) {}
    
    private:
    
		/* UDP port range:6970~65535,�μ�UDPSocketPool::CreateUDPSocketPair() */
        enum
        {
            kLowestUDPPort = 6970,  //UInt16
            kHighestUDPPort = 65535 //UInt16
        };
    
		/* ��UDPSocketPair��ɵĶ��� */
        OSQueue fUDPQueue;
		/* ��UDPSocketPool��Ӧ�Ļ����� */
        OSMutex fMutex;
};

/* ������UDPSocket��ϳ�UDPsocketPair����,��������Ϊһ������Ԫ����UDPSocket Pool,����UDPSocketPoolͳһά���͹��� */
class UDPSocketPair
{
    public:
        
        UDPSocketPair(UDPSocket* inSocketA, UDPSocket* inSocketB)
            : fSocketA(inSocketA), fSocketB(inSocketB), fRefCount(0), fElem() {fElem.SetEnclosingObject(this);/* ����Queue elem���ڵ������ָ�� */}
        ~UDPSocketPair() {}
    
		//accessors
        UDPSocket*  GetSocketA() { return fSocketA; }
        UDPSocket*  GetSocketB() { return fSocketB; }
        
    private:
    
		/* ���������������ӵ�UDP Socket��� */
        UDPSocket*  fSocketA;
        UDPSocket*  fSocketB;
		/* ���ü���,��Ϊ���ܸ��� */
        UInt32      fRefCount;
		/* ��������Ԫ */
        OSQueueElem fElem;
        
        friend class UDPSocketPool;
};
#endif // __UDPSOCKETPOOL_H__

