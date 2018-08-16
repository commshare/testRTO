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



/* 注意两个入参是从父类Socket继承下来的,其中第一个入参是RTCPTask,参见RTPSocketPool::ConstructUDPSocketPair() */
UDPSocket::UDPSocket(Task* inTask, UInt32 inSocketType)
: Socket(inTask, inSocketType), fDemuxer(NULL)
{
	//假如Socket类型是kWantsDemuxer,就创建UDPDemuxer实例
    if (inSocketType & kWantsDemuxer)
        fDemuxer = NEW UDPDemuxer();
        
    //setup msghdr(Message handler)
    ::memset(&fMsgAddr, 0, sizeof(fMsgAddr));
}


/* 面向无连接模式的发送数据  */
OS_Error UDPSocket::SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength)
{
    Assert(inBuffer != NULL);
    
	/* 利用入参配置结构体theRemoteAddr,它是数据接收方的Socket地址 */
    struct sockaddr_in  theRemoteAddr;
    theRemoteAddr.sin_family = AF_INET;
    theRemoteAddr.sin_port = htons(inRemotePort);
    theRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);

    // Win32 says that inBuffer is a char*
	/* 在无连接的UDP Socket上发送数据,成功返回已发送的数据字节数;错误返回SOCKET_ERROR  */
	int theErr = ::sendto(fFileDesc, (char*)inBuffer, inLength, 0, (sockaddr*)&theRemoteAddr, sizeof(theRemoteAddr));


	/* 错误处理 */
    if (theErr == -1)
        return (OS_Error)OSThread::GetErrno();
    return OS_NoErr;
}

/* 以非连接方式(UDP Socket)接收一个数据报并保存源地址和接收数据长度 */
OS_Error UDPSocket::RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort,
                            void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen)
{
    Assert(outRecvLen != NULL);
	/* 断定数据源的IP地址和端口存在 */
    Assert(outRemoteAddr != NULL);
    Assert(outRemotePort != NULL);
    
    socklen_t addrLen = sizeof(fMsgAddr);

    // Win32 says that ioBuffer is a char*
	/* 以非连接方式(UDP Socket)接收一个数据报并保存源地址 */
    SInt32 theRecvLen = ::recvfrom(fFileDesc, (char*)ioBuffer, inBufLen, 0, (sockaddr*)&fMsgAddr, &addrLen);

	/* 假如错误,查找具体出错原因 */
    if (theRecvLen == -1)
        return (OS_Error)OSThread::GetErrno();
    
	/* 记录下数据源的Socket地址和端口 */
    *outRemoteAddr = ntohl(fMsgAddr.sin_addr.s_addr);
    *outRemotePort = ntohs(fMsgAddr.sin_port);
	/* 确保接收到数据,并保存数据长度 */
    Assert(theRecvLen >= 0);
    *outRecvLen = (UInt32)theRecvLen;

    return OS_NoErr;        
}

/* 设置多播结构体和相应Socket属性 */
OS_Error UDPSocket::JoinMulticast(UInt32 inRemoteAddr)
{
	/* Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP */
    struct ip_mreq  theMulti;
    UInt32 localAddr = fLocalAddr.sin_addr.s_addr; // Already in network byte order

	/* 用入参设置group的IP多播地址 */
    theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
	/* 设置接口的本地IP地址 */
    theMulti.imr_interface.s_addr = localAddr;

    /* add an IP group membership,设置多播属性,注意多播结构体theMulti */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
    //AssertV(err == 0, OSThread::GetErrno());

	/* 查找错误原因 */
    if (err == -1)
         return (OS_Error)OSThread::GetErrno();
    else
         return OS_NoErr;
}

/* 设置该Socket的TTL属性 */
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

/* 设置本地Socket上的多播数据报属性 */
OS_Error UDPSocket::SetMulticastInterface(UInt32 inLocalAddr)
{
    // set the outgoing interface for multicast datagrams on this socket
    in_addr theLocalAddr;
    theLocalAddr.s_addr = inLocalAddr;
	/* set/get IP multicast i/f  */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_MULTICAST_IF, (char*)&theLocalAddr, sizeof(theLocalAddr));
	/* 错误处理 */
    AssertV(err == 0, OSThread::GetErrno());
    if (err == -1)
        return (OS_Error)OSThread::GetErrno();
    else
        return OS_NoErr;    
}

/* 除了setsocketopt()中参数IP_DROP_MEMBERSHIP不同外,其它与UDPSocket::JoinMulticast()相同,
   从指定Socket移除多播组 */
OS_Error UDPSocket::LeaveMulticast(UInt32 inRemoteAddr)
{
    struct ip_mreq  theMulti;
    theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
    theMulti.imr_interface.s_addr = htonl(fLocalAddr.sin_addr.s_addr);

	/* drop an IP group membership,设置多播属性,注意多播结构体theMulti */
    int err = setsockopt(fFileDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
    if (err == -1)
        return (OS_Error)OSThread::GetErrno();
    else
        return OS_NoErr;    
}
