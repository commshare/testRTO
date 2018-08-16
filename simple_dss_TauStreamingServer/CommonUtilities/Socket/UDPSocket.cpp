/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPSocket.cpp
Description: Provides a socket derived class to deal with the udp connection from clients.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "UDPSocket.h"
#include "OSMemory.h"

#ifdef USE_NETLOG
#include <netlog.h>
#endif



/* ע����������ǴӸ���Socket�̳�������,���е�һ�������RTCPTask,�μ�RTPSocketPool::ConstructUDPSocketPair() */
UDPSocket::UDPSocket(Task* inTask, UInt32 inSocketType)
: Socket(inTask, inSocketType), fDemuxer(NULL)
{
	//����Socket������kWantsDemuxer,�ʹ���UDPDemuxerʵ��
    if (inSocketType & kWantsDemuxer)
        fDemuxer = NEW UDPDemuxer();
        
    //setup msghdr(Message handler)
    ::memset(&fMsgAddr, 0, sizeof(fMsgAddr));
}


/* ����������ģʽ�ķ�������  */
OS_Error UDPSocket::SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength)
{
    Assert(inBuffer != NULL);
    
	/* ����������ýṹ��theRemoteAddr,�������ݽ��շ���Socket��ַ */
    struct sockaddr_in  theRemoteAddr;
    theRemoteAddr.sin_family = AF_INET;
    theRemoteAddr.sin_port = htons(inRemotePort);
    theRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);

    // Win32 says that inBuffer is a char*
	/* �������ӵ�UDP Socket�Ϸ�������,�ɹ������ѷ��͵������ֽ���;���󷵻�SOCKET_ERROR  */
	int theErr = ::sendto(fFileDesc, (char*)inBuffer, inLength, 0, (sockaddr*)&theRemoteAddr, sizeof(theRemoteAddr));


	/* ������ */
    if (theErr == -1)
        return (OS_Error)OSThread::GetErrno();
    return OS_NoErr;
}

/* �Է����ӷ�ʽ(UDP Socket)����һ�����ݱ�������Դ��ַ�ͽ������ݳ��� */
OS_Error UDPSocket::RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort,
                            void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen)
{
    Assert(outRecvLen != NULL);
	/* �϶�����Դ��IP��ַ�Ͷ˿ڴ��� */
    Assert(outRemoteAddr != NULL);
    Assert(outRemotePort != NULL);
    
    socklen_t addrLen = sizeof(fMsgAddr);

    // Win32 says that ioBuffer is a char*
	/* �Է����ӷ�ʽ(UDP Socket)����һ�����ݱ�������Դ��ַ */
    SInt32 theRecvLen = ::recvfrom(fFileDesc, (char*)ioBuffer, inBufLen, 0, (sockaddr*)&fMsgAddr, &addrLen);

	/* �������,���Ҿ������ԭ�� */
    if (theRecvLen == -1)
        return (OS_Error)OSThread::GetErrno();
    
	/* ��¼������Դ��Socket��ַ�Ͷ˿� */
    *outRemoteAddr = ntohl(fMsgAddr.sin_addr.s_addr);
    *outRemotePort = ntohs(fMsgAddr.sin_port);
	/* ȷ�����յ�����,���������ݳ��� */
    Assert(theRecvLen >= 0);
    *outRecvLen = (UInt32)theRecvLen;

    return OS_NoErr;        
}

/* ���öಥ�ṹ�����ӦSocket���� */
OS_Error UDPSocket::JoinMulticast(UInt32 inRemoteAddr)
{
	/* Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP */
    struct ip_mreq  theMulti;
    UInt32 localAddr = fLocalAddr.sin_addr.s_addr; // Already in network byte order

	/* ���������group��IP�ಥ��ַ */
    theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
	/* ���ýӿڵı���IP��ַ */
    theMulti.imr_interface.s_addr = localAddr;

    /* add an IP group membership,���öಥ����,ע��ಥ�ṹ��theMulti */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
    //AssertV(err == 0, OSThread::GetErrno());

	/* ���Ҵ���ԭ�� */
    if (err == -1)
         return (OS_Error)OSThread::GetErrno();
    else
         return OS_NoErr;
}

/* ���ø�Socket��TTL���� */
OS_Error UDPSocket::SetTtl(UInt16 timeToLive)
{
    // set the ttl
    u_char  nOptVal = (u_char)timeToLive;//dms - stevens pp. 496. bsd implementations barf
                                            //unless this is a u_char
	/* set/get IP multicast ttl */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&nOptVal, sizeof(nOptVal));
    if (err == -1)
        return (OS_Error)OSThread::GetErrno();
    else
        return OS_NoErr;    
}

/* ���ñ���Socket�ϵĶಥ���ݱ����� */
OS_Error UDPSocket::SetMulticastInterface(UInt32 inLocalAddr)
{
    // set the outgoing interface for multicast datagrams on this socket
    in_addr theLocalAddr;
    theLocalAddr.s_addr = inLocalAddr;
	/* set/get IP multicast i/f  */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_MULTICAST_IF, (char*)&theLocalAddr, sizeof(theLocalAddr));
	/* ������ */
    AssertV(err == 0, OSThread::GetErrno());
    if (err == -1)
        return (OS_Error)OSThread::GetErrno();
    else
        return OS_NoErr;    
}

/* ����setsocketopt()�в���IP_DROP_MEMBERSHIP��ͬ��,������UDPSocket::JoinMulticast()��ͬ,
   ��ָ��Socket�Ƴ��ಥ�� */
OS_Error UDPSocket::LeaveMulticast(UInt32 inRemoteAddr)
{
    struct ip_mreq  theMulti;
    theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
    theMulti.imr_interface.s_addr = htonl(fLocalAddr.sin_addr.s_addr);

	/* drop an IP group membership,���öಥ����,ע��ಥ�ṹ��theMulti */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
    if (err == -1)
        return (OS_Error)OSThread::GetErrno();
    else
        return OS_NoErr;    
}
