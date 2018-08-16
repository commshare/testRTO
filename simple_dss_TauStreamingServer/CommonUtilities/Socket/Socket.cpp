/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 Socket.h
Description: Provides a simple, object oriented socket abstraction, also
			 hides the details of socket event handling. Sockets can post
			 events (such as S_DATA, S_CONNECTIONCLOSED) to Tasks.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h> /* used for readv()/writev() */
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "Socket.h"
#include "SocketUtils.h"
#include "OSMemory.h"

#ifdef USE_NETLOG
	#include <netlog.h>
#endif

/* 初始化该全局静态变量 */
EventThread* Socket::sEventThread = NULL;

Socket::Socket(Task *notifytask, UInt32 inSocketType)
:   EventContext(EventContext::kInvalidFileDesc, sEventThread),
    fState(inSocketType),
    fLocalAddrStrPtr(NULL),
    fLocalDNSStrPtr(NULL),
    fPortStr(fPortBuffer, kPortBufSizeInBytes)
{
    fLocalAddr.sin_addr.s_addr = 0;/* unsigned long s_addr, see winsock2.h */
    fLocalAddr.sin_port = 0;
    
    fDestAddr.sin_addr.s_addr = 0;
    fDestAddr.sin_port = 0;
    
	/* 用入参设置指定的任务 */
    this->SetTask(notifytask);

#if SOCKET_DEBUG
   fLocalAddrStr.Set(fLocalAddrBuffer,sizeof(fLocalAddrBuffer));
#endif

}

/*  创建Socket并设置成非阻塞模式,注意入参为Socket的5种类型之一.  */
OS_Error Socket::Open(int theType)
{
	/* 确信socket描述符的初始值为非法的(这是初始化状态) */
    Assert(fFileDesc == EventContext::kInvalidFileDesc);
	/* 创建Socket(为服务器和客户端),并作错误处理 */
    fFileDesc = ::socket(PF_INET, theType, 0);
	/* 出错后获取具体的出错原因 */
    if (fFileDesc == EventContext::kInvalidFileDesc)
        return (OS_Error)OSThread::GetErrno();
            
    //
    // Setup this socket's event context
	/* 将指定Socket设置成非阻塞模式 */
    if (fState & kNonBlockingSocketType)
        this->InitNonBlocking(fFileDesc);   

    return OS_NoErr;
}

/* 设置指定Socket的重用IP地址属性 */
void Socket::ReuseAddr()
{
    int one = 1;
    int err = ::setsockopt(fFileDesc, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(int));
    Assert(err == 0);   
}

/* 设置指定Socket的TCP非延迟属性 */
void Socket::NoDelay()
{
    int one = 1;
    int err = ::setsockopt(fFileDesc, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
    Assert(err == 0);   
}

/* 设置指定Socket的保持alive属性 */
void Socket::KeepAlive()
{
    int one = 1;
    int err = ::setsockopt(fFileDesc, SOL_SOCKET, SO_KEEPALIVE, (char*)&one, sizeof(int));
    Assert(err == 0);   
}

void    Socket::SetSocketBufSize(UInt32 inNewSize)
{

#if SOCKET_DEBUG
	int value;
	int buffSize = sizeof(value);
	int error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_SNDBUF, (void*)&value,  (socklen_t*)&buffSize);
#endif

	/* 用入参设置指定Socket的SO_SNDBUF(send buffer size)属性 */
    int bufSize = inNewSize;
    int err = ::setsockopt(fFileDesc, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(int));
    AssertV(err == 0, OSThread::GetErrno());
    
#if SOCKET_DEBUG
	int setValue;
	error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_SNDBUF, (void*)&setValue,  (socklen_t*)&buffSize);
	qtss_printf("Socket::SetSocketBufSize ");
	if (fState & kBound)
	{	if (NULL != this->GetLocalAddrStr())
			this->GetLocalAddrStr()->PrintStr(":");
		if (NULL != this->GetLocalPortStr())
			this->GetLocalPortStr()->PrintStr(" ");
	}
	else
		qtss_printf("unbound ");
	qtss_printf("socket=%d old SO_SNDBUF =%d inNewSize=%d setValue=%d\n", (int) fFileDesc, value, bufSize, setValue);
#endif

}

/* used in TCPListenerSocket::Initialize() */
OS_Error    Socket::SetSocketRcvBufSize(UInt32 inNewSize)
{
#if SOCKET_DEBUG
	int value;
	int buffSize = sizeof(value);
	int error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_RCVBUF, (void*)&value,  (socklen_t*)&buffSize);
#endif

	/* 用入参设置指定Socket的SO_RCVBUF(receive buffer size)属性 */
    int bufSize = inNewSize;
    int err = ::setsockopt(fFileDesc, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, sizeof(int));

#if SOCKET_DEBUG
	int setValue;
	error = ::getsockopt(fFileDesc, SOL_SOCKET, SO_RCVBUF, (void*)&setValue,  (socklen_t*)&buffSize);
	qtss_printf("Socket::SetSocketRcvBufSize ");
	if (fState & kBound)
	{	if (NULL != this->GetLocalAddrStr())
			this->GetLocalAddrStr()->PrintStr(":");
		if (NULL != this->GetLocalPortStr())
			this->GetLocalPortStr()->PrintStr(" ");
	}
	else
		qtss_printf("unbound ");
	qtss_printf("socket=%d old SO_RCVBUF =%d inNewSize=%d setValue=%d\n",(int) fFileDesc, value, bufSize, setValue);
#endif


    if (err == -1)
        return OSThread::GetErrno();
        
    return OS_NoErr;
}


/* 将本地ip地址和端口绑定给Socket */
OS_Error Socket::Bind(UInt32 addr, UInt16 port)
{
    socklen_t/*int*/ len = sizeof(fLocalAddr);

	/* 设置fLocalAddr */
    ::memset(&fLocalAddr, 0, sizeof(fLocalAddr));
    fLocalAddr.sin_family = AF_INET;
    fLocalAddr.sin_port = htons(port);
    fLocalAddr.sin_addr.s_addr = htonl(addr);
    
	/* 将本地地址绑定到Socket上 */
    int err = ::bind(fFileDesc, (sockaddr *)&fLocalAddr, sizeof(fLocalAddr));
    
	/* 若返回结果是SOCKET_ERROR(-1) */
    if (err == -1)
    {
        fLocalAddr.sin_port = 0;
        fLocalAddr.sin_addr.s_addr = 0;
        return (OS_Error)OSThread::GetErrno();
    }
	/* 获取Socket的设定值 */
    else ::getsockname(fFileDesc, (sockaddr *)&fLocalAddr, &len); // get the kernel to fill in unspecified values
    fState |= kBound;
    return OS_NoErr;
}

/* 当fLocalAddrStrPtr为空时,利用SocketUtils类中的成员函数从IP地址字符串数组中查找fLocalAddr.sin_addr.s_addr,来使
   fLocalAddrStrPtr非空;否则,直接取出fLocalAddrStrPtr */
StrPtrLen*  Socket::GetLocalAddrStr()
{
    //Use the array of IP addr strings to locate the string formatted version
    //of this IP address.
    if (fLocalAddrStrPtr == NULL)
    {
        for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++)
        {
            if (SocketUtils::GetIPAddr(x) == ntohl(fLocalAddr.sin_addr.s_addr))
            {
                fLocalAddrStrPtr = SocketUtils::GetIPAddrStr(x);
                break;
            }
        }
    }

#if SOCKET_DEBUG    
    if (fLocalAddrStrPtr == NULL) 
    {   // shouldn't happen but no match so it was probably a failed socket connection or accept. addr is probably 0.

        fLocalAddrBuffer[0]=0;
        fLocalAddrStrPtr = &fLocalAddrStr;
        struct in_addr theAddr;
        theAddr.s_addr =ntohl(fLocalAddr.sin_addr.s_addr);
        SocketUtils::ConvertAddrToString(theAddr, &fLocalAddrStr);

        printf("Socket::GetLocalAddrStr Search IPs failed, numIPs=%d\n",SocketUtils::GetNumIPAddrs());
        for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++)
        {    printf("ip[%lu]=",x); SocketUtils::GetIPAddrStr(x)->PrintStr("\n");
        }
        printf("this ip = %d = ",theAddr.s_addr); fLocalAddrStrPtr->PrintStr("\n");

       if (theAddr.s_addr == 0 || fLocalAddrBuffer[0] == 0)
           fLocalAddrStrPtr = NULL; // so the caller can test for failure
    }
#endif 

    Assert(fLocalAddrStrPtr != NULL);
    return fLocalAddrStrPtr;
}

/* 获取本地DNS域名 */
StrPtrLen*  Socket::GetLocalDNSStr()
{
    //Do the same thing as the above function, but for DNS names
	/* 断言是个特殊的ip地址 */
    Assert(fLocalAddr.sin_addr.s_addr != INADDR_ANY);
    if (fLocalDNSStrPtr == NULL)
    {
		/* 查找指定的ip地址fLocalAddr.sin_addr.s_addr,设置fLocalDNSStrPtr */
        for (UInt32 x = 0; x < SocketUtils::GetNumIPAddrs(); x++)
        {
            if (SocketUtils::GetIPAddr(x) == ntohl(fLocalAddr.sin_addr.s_addr))
            {
                fLocalDNSStrPtr = SocketUtils::GetDNSNameStr(x);
                break;
            }
        }
    }

    //if we weren't able to get this DNS name, make the DNS name the same as the IP addr str.
	/* 实在找不到DNS,就拿ip地址字符串代替 */
    if (fLocalDNSStrPtr == NULL)
        fLocalDNSStrPtr = this->GetLocalAddrStr();

    Assert(fLocalDNSStrPtr != NULL);
    return fLocalDNSStrPtr;
}

/* 获取本地端口字符串 */
StrPtrLen*  Socket::GetLocalPortStr()
{
    if (fPortStr.Len == kPortBufSizeInBytes)
    {
        int temp = ntohs(fLocalAddr.sin_port);
		/* 将获取的端口设置成指定格式 */
        qtss_sprintf(fPortBuffer, "%d", temp);
        fPortStr.Len = ::strlen(fPortBuffer);
    }
    return &fPortStr;
}

/* 用Socket发送数据,第四个参数表示向外发送了多少字节的数据 */
OS_Error Socket::Send(const char* inData, const UInt32 inLength, UInt32* outLengthSent)
{
    Assert(inData != NULL);
    
	/* 确保Socket处于连接状态 */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
        
    int err;
    do {
		/* 在指定的Socket处发送数据,第四个参数是flag,函数返回值是发送数据的字节数(成功时),或SOCKET_ERROR(-1) */
       err = ::send(fFileDesc, inData, inLength, 0);//flags??
    } while((err == -1) && (OSThread::GetErrno() == EINTR));
	/* 错误处理 */
    if (err == -1)
    {
        //Are there any errors that can happen if the client is connected?
        //Yes... EAGAIN. Means the socket is now flow-controlled
		/* 获取错误原因 */
        int theErr = OSThread::GetErrno();
		/* 如果连接上但是错误不是EAGAIN,就断开连接 */
        if ((theErr != EAGAIN) && (this->IsConnected()))
            fState ^= kConnected;//turn off connected state flag
        return (OS_Error)theErr;
    }
    
	/* 发送了多少数据 */
    *outLengthSent = err;
    return OS_NoErr;
}

/* 类似Socket::Send(),但比它功能更强大 */
/* 将多个缓冲区中的数据一次性复制到socket的缓冲区中,保证原子操作,返回实际发送的数据 */
OS_Error Socket::WriteV(const struct iovec* iov, const UInt32 numIOvecs, UInt32* outLenSent)
{
    Assert(iov != NULL);

	/* 确保连接上,否则返回连接错误 */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
        
    int err;
    do {
#ifdef __Win32__
        DWORD theBytesSent = 0;
		/* 在一个已连接的非重叠的Socket向外发送数据,比send()功能更强,返回值为0或SOCKET_ERROR(-1) */
        err = ::WSASend(fFileDesc, (LPWSABUF)iov, numIOvecs, &theBytesSent, 0, NULL, NULL);
        if (err == 0)
            err = theBytesSent;
#else
       err = ::writev(fFileDesc, iov, numIOvecs);//flags??
#endif
    } while((err == -1) && (OSThread::GetErrno() == EINTR));
	/* 错误处理 */
    if (err == -1)
    {
        // Are there any errors that can happen if the client is connected?
        // Yes... EAGAIN. Means the socket is now flow-controleld
        int theErr = OSThread::GetErrno();
        if ((theErr != EAGAIN) && (this->IsConnected()))
            fState ^= kConnected;//turn off connected state flag
        return (OS_Error)theErr;
    }
	/* 更新实际发送的数据 */
    if (outLenSent != NULL)
        *outLenSent = (UInt32)err;
        
    return OS_NoErr;
}

/* 非常类似Socket::Send(),从指定Socket接受数据 */
OS_Error Socket::Read(void *buffer, const UInt32 length, UInt32 *outRecvLenP)
{
    Assert(outRecvLenP != NULL);
    Assert(buffer != NULL);

	/* 确保是在连接状态 */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
            
    //int theRecvLen = ::recv(fFileDesc, buffer, length, 0);//flags??
    int theRecvLen;
    do {
		/* 从Socket接收数据,,类似::send(fFileDesc, inData, inLength, 0) */
       theRecvLen = ::recv(fFileDesc, (char*)buffer, length, 0);//flags??
    } while((theRecvLen == -1) && (OSThread::GetErrno() == EINTR));

	/* 进行错误处理 */
    if (theRecvLen == -1)
    {
        // Are there any errors that can happen if the client is connected?
        // Yes... EAGAIN. Means the socket is now flow-controleld
        int theErr = OSThread::GetErrno();
        if ((theErr != EAGAIN) && (this->IsConnected()))
            fState ^= kConnected;//turn off connected state flag
        return (OS_Error)theErr;
    }
    //if we get 0 bytes back from read, that means the client has disconnected.
    //Note that and return the proper error to the caller
	/* 假如接收到的字节数为0,就断开连接 */
    else if (theRecvLen == 0)
    {
        fState ^= kConnected;
        return (OS_Error)ENOTCONN;
    }
	/* 确保接收到数据 */
    Assert(theRecvLen > 0);
    *outRecvLenP = (UInt32)theRecvLen;
    return OS_NoErr;
}
