/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPSocketPool.cpp
Description: Object that creates & maintains UDP socket pairs in a pool.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "UDPSocketPool.h"


/* ��ԴIP��ַ�Ͷ˿ڷ�����: ������һ������ʱ,ͨ��ѭ�����ҳ�����Ҫ��Ĳ���demuxer�е�UDPSocketPair;�����½�����Ҫ���UDPSocketPair */
UDPSocketPair* UDPSocketPool::GetUDPSocketPair(UInt32 inIPAddr, UInt16 inPort,
                                                UInt32 inSrcIPAddr, UInt16 inSrcPort)
{
    OSMutexLocker locker(&fMutex);
	/* ��source IP address or port����һ������ʱ,����UDPSocketPair��ɵ�pool */
    if ((inSrcIPAddr != 0) || (inSrcPort != 0))
    {
        for (OSQueueIter qIter(&fUDPQueue); !qIter.IsDone(); qIter.Next())
        {
            //If we find a pair that is a) on the right IP address, and b) doesn't
            //have this source IP & port in the demuxer already, we can return this pair
			/* ��ȡ��ǰUDPSocketPair�����е�Queue elem���ڵ������ָ��,�μ�UDPSocketPair�ඨ�� */
            UDPSocketPair* theElem = (UDPSocketPair*)qIter.GetCurrent()->GetEnclosingObject();
			/* ����UDPSocketPair�ı��ص�ַ�������ָ���ĵ�ַ,���Ҷ˿�ҪôΪ0(���������󶨵Ķ˿ں�),Ҫô�������ָ���Ķ˿ں� */
            if ((theElem->fSocketA->GetLocalAddr() == inIPAddr) &&
                ((inPort == 0) || (theElem->fSocketA->GetLocalPort() == inPort)))
            {
                //check to make sure this source IP & port is not already in the demuxer.
                //If not, we can return this socket pair.
				/* ����û�еõ�UDPSocketPair����һUDP Socket(RTCP)��UDPDemuxer�����ָ��(û�и���),�����ҵ������ָ��,����
				   ��ָ��IP��ַ�Ͷ˿�û�з���Task,�����Ӽ���,������ */
                if ((theElem->fSocketB->GetDemuxer() == NULL) ||
                    ((!theElem->fSocketB->GetDemuxer()->AddrInMap(0, 0)) &&
                    (!theElem->fSocketB->GetDemuxer()->AddrInMap(inSrcIPAddr, inSrcPort))))
                {
                    theElem->fRefCount++;
                    return theElem;
                }
                //If port is specified, there is NO WAY a socket pair can exist that matches
                //the criteria (because caller wants a specific ip & port combination)
				/* �������inPort����,ֻ��һ�λ������,��������,��ζ�Ų���ʧ��,����NULL */
                else if (inPort != 0)
                    return NULL;
            }
        }
    }
    //if we get here, there is no UDP Socket pair already in the pool that matches the specified criteria, so we have to create a new pair.
    //�������ǵ���,˵����ǰUDPSocketPair������û�з���ָ����׼��UDPSocketPair,����ֻ���½�һ��,�����pool��
    return this->CreateUDPSocketPair(inIPAddr, inPort);
}

/* ע��ԭ������Critical_Section,����UDPSocketPair���ü���.���ǵ�������������,ֻ�е����ü���Ϊ0ʱ,�ŴӶ�����ɾȥ����Ԫ��,������UDPSocketPair����ʵ�� */
void UDPSocketPool::ReleaseUDPSocketPair(UDPSocketPair* inPair)
{
    OSMutexLocker locker(&fMutex);
    inPair->fRefCount--;
    if (inPair->fRefCount == 0)
    {
        fUDPQueue.Remove(&inPair->fElem);
        this->DestructUDPSocketPair(inPair);
    }
}

/* ����UDP Socket Pair,���󶨵�ָ����IP��ַ�Ͷ˿�.������˿ڷ���ʱ,ֻ��һ�ΰ󶨳ɹ�����;������˿�Ϊ��ʱ,
   ��ѭ�����ҿ��԰��ϵĶ˿�.��󷵻ذ󶨺��UDP Socket Pair,���򷵻�NULL */
UDPSocketPair*  UDPSocketPool::CreateUDPSocketPair(UInt32 inAddr, UInt16 inPort)
{
    OSMutexLocker locker(&fMutex);
	/* ��������RTPSocketPool(�μ�QTSServer.cpp)����UDP Socket Pair,���ӷ�������ȡRTCP����ʵ��ָ�벢��Ӧ����һ��UDPSocketPairʵ�� */
    UDPSocketPair* theElem = ConstructUDPSocketPair();
	/* ȷ�����ɳɹ� */
    Assert(theElem != NULL);
	/* ȷ��UDP Socket Pair���˵�UDPSocket���ɳɹ���,���������� */
    if (theElem->fSocketA->Open() != OS_NoErr)
    {
		/* Դ��ʵ�ֲμ�������RTPSocketPool(�μ�QTSServer.cpp),ɾȥ���ָ��(����,���洴��)��һ��UDPSocketPairʵ�� */
        this->DestructUDPSocketPair(theElem);
        return NULL;
    }
    if (theElem->fSocketB->Open() != OS_NoErr)
    {
        this->DestructUDPSocketPair(theElem);
        return NULL;
    }
    
    // Set socket options on these new sockets
	/* ��������RTPSocketPool(�μ�QTSServer.cpp)����UDP Socket Pair��options,��: ��������UDP socket�Եķ���buffer����ջ����С:
	��һ�����ù̶�,�ڶ�������Ҫ���,��:����UDPSocketPair�����ⷢ��RTP���ݵ�socket buffer��СΪ256K�ֽ�,��Ԥ��ֵ��ʼ,�Լ������
	����̬����RTCP socket����buffer�Ĵ�С */
    this->SetUDPSocketOptions(theElem);
    
    //try to find an open pair of ports to bind these sockets to
	/* ��һ��δʹ�õĶ˿�(��Χ��6970~65536)���󶨵�UDPSocketPair��,��ʼֵ��Ϊfalse,�������úú���������ֵ */
    Bool16 foundPair = false;
    
    //If port is 0, then the caller doesn't care what port # we bind this socket to.
    //Otherwise, ONLY attempt to bind this socket to the specified port
	/* �������ָ���Ķ˿ں�Ϊ0,������߲�������ʵ�ʰ󶨵��ĸ��˿ں���;����,ֻ�ܰ����ָ���Ķ˿ں� */
    UInt16 startIndex = kLowestUDPPort;
    if (inPort != 0)
        startIndex = inPort;
    UInt16 stopIndex = kHighestUDPPort;
    if (inPort != 0)
        stopIndex = inPort;
    
	/* ��Ҫ����һ�Զ˿�,������ʼ�˿ڲ��������ֵʱ,����ѭ�����Ҷ˿ڵ����� */
    while ((!foundPair) && (startIndex <= kHighestUDPPort))
    {
        OS_Error theErr = theElem->fSocketA->Bind(inAddr, startIndex);
		//���󶨳ɹ�RTP�˿ںź�, �ٽ��Ű�RTCP�˿ں�
        if (theErr == OS_NoErr)
        {
            theErr = theElem->fSocketB->Bind(inAddr, startIndex+1);
			/* ������Socket���ɹ���ʱ */
            if (theErr == OS_NoErr)
            {
				/* ��ʾ�ҵ����������˿� */
                foundPair = true;
				/* ���󶨺��UDP Socket Pair����UDPSocketPair�Ķ��� */
                fUDPQueue.EnQueue(&theElem->fElem);
				/* ��UDP Socket Pair��������1 */
                theElem->fRefCount++;
				/* ���ظ�UDP Socket Pair */
                return theElem;
            }
        }
        //If we are looking to bind to a specific port set, and we couldn't then
        //just break it out here.
		/* �����ʱ���inPort����(˵���ǲ��ܰ󶨵�ָ���˿�),������Ψһһ�ΰ󶨻�����û�гɹ�,���ж� */
        if (inPort != 0)
            break;
		/* ��������,�ٴ�ѭ�����Ҷ˿�,ע�������ǳɶԲ���,����������2����1 ! */
        startIndex += 2;
    }
    //if we couldn't find a pair of sockets, make sure to clean up our mess
	/* �����ָ���Ķ˿ں�Ϊ0����,����������ѭ��������û�гɹ�,�����ٸ�UDP Socket Pair */
    this->DestructUDPSocketPair(theElem);
    return NULL;
}
