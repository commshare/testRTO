/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TCPSocket.cpp
Description: Provides a socket derived class to deal with the tcp connection from clients.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "TCPSocket.h"
#include "SocketUtils.h"
#include "OS.h"

#ifdef USE_NETLOG
#include <netlog.h>
#endif

/* 利用入参的值配置本类TCPSocket各成员的信息,然后清空入参中的各成员值,类似于复制构造函数 */
void TCPSocket::SnarfSocket( TCPSocket & fromSocket )
{
    // take the connection away from the other socket
    // and use it as our own.
	/* 确保成员是初始状态 */
    Assert(fFileDesc == EventContext::kInvalidFileDesc);
	/* 利用入参设置Socket及调整其状态  */
    this->Set( fromSocket.fFileDesc, &fromSocket.fRemoteAddr );
    
    // clear the old socket so he doesn't close and the like
	/* 新建remoteaddr并零初始化,作下面fromSocket.Set()使用 */
    struct  sockaddr_in  remoteaddr;
    
    ::memset( &remoteaddr, 0, sizeof( remoteaddr ) );

	/* 清除入参的Socket信息 */
    fromSocket.Set( EventContext::kInvalidFileDesc, &remoteaddr );

    // get the event context too
	/* 利用EventContext::SnarfEventContext(), 利用入参来获取数据成员的值,并将数据成员fRef加入Hash Table中,删去入参中同key值的Ref  */
    this->SnarfEventContext( fromSocket );

}

/* 利用入参设置指定Socket的本地fLocalAddr,fRemoteAddr及调整本地Socket状态  */
/* used in TCPListenerSocket::ProcessEvent(),那里第一个入参是服务器接受客户端连接的服务器端的Socket,客户端的ip地址是第二个参数,所以这两个参数联系紧密 */
void TCPSocket::Set(int inSocket, struct sockaddr_in* remoteaddr)
{
	/* 从入参设置本地和远端的Socket */
    fRemoteAddr = *remoteaddr;
    fFileDesc = inSocket;
    
	/* 当服务器接受的客户端的Socket不是初始状态时,一定要成功获取它在服务器端对应的Socket的信息(包括相应的IP地址) */
    if ( inSocket != EventContext::kInvalidFileDesc ) 
    {
        //make sure to find out what IP address this connection is actually occuring on. That
        //way, we can report correct information to clients asking what the connection's IP is
        socklen_t len = sizeof(fLocalAddr);

		/* 获取客户端的Socket的在服务器本地上的Socket信息,详见<<Windows网络编程第二版>>.
		   这里fRemoteAddr和fLocalAddr是已接受的该客户端已经连接的对应的IP地址 */
        int err = ::getsockname(fFileDesc, (struct sockaddr*)&fLocalAddr, &len);
		/* 确保能成功获取本地Socket信息 */
        AssertV(err == 0, OSThread::GetErrno());
		/* 确保该Socket已绑定ip和连接 */
        fState |= kBound;
        fState |= kConnected;
    }
    else
		/* 若服务器接受的客户端的Socket是初始状态时,它的状态只能是0 */
        fState = 0;
}

/* 获取远端Socket的ip地址的字符串 */
StrPtrLen*  TCPSocket::GetRemoteAddrStr()
{
    if (fRemoteStr.Len == kIPAddrBufSize)
        SocketUtils::ConvertAddrToString(fRemoteAddr.sin_addr, &fRemoteStr);
    return &fRemoteStr;
}

/* 利用入参配置远端Socket的IP地址信息,并与本地Socket建立联系  */
OS_Error  TCPSocket::Connect(UInt32 inRemoteAddr, UInt16 inRemotePort)
{
	/* 利用入参配置远端Socket的IP地址信息 */
    ::memset(&fRemoteAddr, 0, sizeof(fRemoteAddr));
    fRemoteAddr.sin_family = AF_INET;        /* host byte order */
    fRemoteAddr.sin_port = htons(inRemotePort); /* short, network byte order */
    fRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);

    /* don't forget to error check the connect()! */
	/* 将两个Socket连接起来准备通信 */
    int err = ::connect(fFileDesc, (sockaddr *)&fRemoteAddr, sizeof(fRemoteAddr));
	/* 设置通信的状态 */
    fState |= kConnected;
    
	/* 进行错误处理 */
    if (err == -1)
    {
        fRemoteAddr.sin_port = 0;
        fRemoteAddr.sin_addr.s_addr = 0;
		/* 获取错误原因 */
        return (OS_Error)OSThread::GetErrno();
    }
    
    return OS_NoErr;

}

