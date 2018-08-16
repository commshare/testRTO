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

/* ��ʼ����ȫ�־�̬���� */
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
    
	/* ���������ָ�������� */
    this->SetTask(notifytask);

#if SOCKET_DEBUG
   fLocalAddrStr.Set(fLocalAddrBuffer,sizeof(fLocalAddrBuffer));
#endif

}

/*  ����Socket�����óɷ�����ģʽ,ע�����ΪSocket��5������֮һ.  */
OS_Error Socket::Open(int theType)
{
	/* ȷ��socket�������ĳ�ʼֵΪ�Ƿ���(���ǳ�ʼ��״̬) */
    Assert(fFileDesc == EventContext::kInvalidFileDesc);
	/* ����Socket(Ϊ�������Ϳͻ���),���������� */
    fFileDesc = ::socket(PF_INET, theType, 0);
	/* ������ȡ����ĳ���ԭ�� */
    if (fFileDesc == EventContext::kInvalidFileDesc)
        return (OS_Error)OSThread::GetErrno();
            
    //
    // Setup this socket's event context
	/* ��ָ��Socket���óɷ�����ģʽ */
    if (fState & kNonBlockingSocketType)
        this->InitNonBlocking(fFileDesc);   

    return OS_NoErr;
}

/* ����ָ��Socket������IP��ַ���� */
void Socket::ReuseAddr()
{
    int one = 1;
    int err = ::setsockopt(fFileDesc, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(int));
    Assert(err == 0);   
}

/* ����ָ��Socket��TCP���ӳ����� */
void Socket::NoDelay()
{
    int one = 1;
    int err = ::setsockopt(fFileDesc, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
    Assert(err == 0);   
}

/* ����ָ��Socket�ı���alive���� */
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

	/* ���������ָ��Socket��SO_SNDBUF(send buffer size)���� */
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

	/* ���������ָ��Socket��SO_RCVBUF(receive buffer size)���� */
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


/* ������ip��ַ�Ͷ˿ڰ󶨸�Socket */
OS_Error Socket::Bind(UInt32 addr, UInt16 port)
{
    socklen_t/*int*/ len = sizeof(fLocalAddr);

	/* ����fLocalAddr */
    ::memset(&fLocalAddr, 0, sizeof(fLocalAddr));
    fLocalAddr.sin_family = AF_INET;
    fLocalAddr.sin_port = htons(port);
    fLocalAddr.sin_addr.s_addr = htonl(addr);
    
	/* �����ص�ַ�󶨵�Socket�� */
    int err = ::bind(fFileDesc, (sockaddr *)&fLocalAddr, sizeof(fLocalAddr));
    
	/* �����ؽ����SOCKET_ERROR(-1) */
    if (err == -1)
    {
        fLocalAddr.sin_port = 0;
        fLocalAddr.sin_addr.s_addr = 0;
        return (OS_Error)OSThread::GetErrno();
    }
	/* ��ȡSocket���趨ֵ */
    else ::getsockname(fFileDesc, (sockaddr *)&fLocalAddr, &len); // get the kernel to fill in unspecified values
    fState |= kBound;
    return OS_NoErr;
}

/* ��fLocalAddrStrPtrΪ��ʱ,����SocketUtils���еĳ�Ա������IP��ַ�ַ��������в���fLocalAddr.sin_addr.s_addr,��ʹ
   fLocalAddrStrPtr�ǿ�;����,ֱ��ȡ��fLocalAddrStrPtr */
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

/* ��ȡ����DNS���� */
StrPtrLen*  Socket::GetLocalDNSStr()
{
    //Do the same thing as the above function, but for DNS names
	/* �����Ǹ������ip��ַ */
    Assert(fLocalAddr.sin_addr.s_addr != INADDR_ANY);
    if (fLocalDNSStrPtr == NULL)
    {
		/* ����ָ����ip��ַfLocalAddr.sin_addr.s_addr,����fLocalDNSStrPtr */
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
	/* ʵ���Ҳ���DNS,����ip��ַ�ַ������� */
    if (fLocalDNSStrPtr == NULL)
        fLocalDNSStrPtr = this->GetLocalAddrStr();

    Assert(fLocalDNSStrPtr != NULL);
    return fLocalDNSStrPtr;
}

/* ��ȡ���ض˿��ַ��� */
StrPtrLen*  Socket::GetLocalPortStr()
{
    if (fPortStr.Len == kPortBufSizeInBytes)
    {
        int temp = ntohs(fLocalAddr.sin_port);
		/* ����ȡ�Ķ˿����ó�ָ����ʽ */
        qtss_sprintf(fPortBuffer, "%d", temp);
        fPortStr.Len = ::strlen(fPortBuffer);
    }
    return &fPortStr;
}

/* ��Socket��������,���ĸ�������ʾ���ⷢ���˶����ֽڵ����� */
OS_Error Socket::Send(const char* inData, const UInt32 inLength, UInt32* outLengthSent)
{
    Assert(inData != NULL);
    
	/* ȷ��Socket��������״̬ */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
        
    int err;
    do {
		/* ��ָ����Socket����������,���ĸ�������flag,��������ֵ�Ƿ������ݵ��ֽ���(�ɹ�ʱ),��SOCKET_ERROR(-1) */
       err = ::send(fFileDesc, inData, inLength, 0);//flags??
    } while((err == -1) && (OSThread::GetErrno() == EINTR));
	/* ������ */
    if (err == -1)
    {
        //Are there any errors that can happen if the client is connected?
        //Yes... EAGAIN. Means the socket is now flow-controlled
		/* ��ȡ����ԭ�� */
        int theErr = OSThread::GetErrno();
		/* ��������ϵ��Ǵ�����EAGAIN,�ͶϿ����� */
        if ((theErr != EAGAIN) && (this->IsConnected()))
            fState ^= kConnected;//turn off connected state flag
        return (OS_Error)theErr;
    }
    
	/* �����˶������� */
    *outLengthSent = err;
    return OS_NoErr;
}

/* ����Socket::Send(),���������ܸ�ǿ�� */
/* ������������е�����һ���Ը��Ƶ�socket�Ļ�������,��֤ԭ�Ӳ���,����ʵ�ʷ��͵����� */
OS_Error Socket::WriteV(const struct iovec* iov, const UInt32 numIOvecs, UInt32* outLenSent)
{
    Assert(iov != NULL);

	/* ȷ��������,���򷵻����Ӵ��� */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
        
    int err;
    do {
#ifdef __Win32__
        DWORD theBytesSent = 0;
		/* ��һ�������ӵķ��ص���Socket���ⷢ������,��send()���ܸ�ǿ,����ֵΪ0��SOCKET_ERROR(-1) */
        err = ::WSASend(fFileDesc, (LPWSABUF)iov, numIOvecs, &theBytesSent, 0, NULL, NULL);
        if (err == 0)
            err = theBytesSent;
#else
       err = ::writev(fFileDesc, iov, numIOvecs);//flags??
#endif
    } while((err == -1) && (OSThread::GetErrno() == EINTR));
	/* ������ */
    if (err == -1)
    {
        // Are there any errors that can happen if the client is connected?
        // Yes... EAGAIN. Means the socket is now flow-controleld
        int theErr = OSThread::GetErrno();
        if ((theErr != EAGAIN) && (this->IsConnected()))
            fState ^= kConnected;//turn off connected state flag
        return (OS_Error)theErr;
    }
	/* ����ʵ�ʷ��͵����� */
    if (outLenSent != NULL)
        *outLenSent = (UInt32)err;
        
    return OS_NoErr;
}

/* �ǳ�����Socket::Send(),��ָ��Socket�������� */
OS_Error Socket::Read(void *buffer, const UInt32 length, UInt32 *outRecvLenP)
{
    Assert(outRecvLenP != NULL);
    Assert(buffer != NULL);

	/* ȷ����������״̬ */
    if (!(fState & kConnected))
        return (OS_Error)ENOTCONN;
            
    //int theRecvLen = ::recv(fFileDesc, buffer, length, 0);//flags??
    int theRecvLen;
    do {
		/* ��Socket��������,,����::send(fFileDesc, inData, inLength, 0) */
       theRecvLen = ::recv(fFileDesc, (char*)buffer, length, 0);//flags??
    } while((theRecvLen == -1) && (OSThread::GetErrno() == EINTR));

	/* ���д����� */
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
	/* ������յ����ֽ���Ϊ0,�ͶϿ����� */
    else if (theRecvLen == 0)
    {
        fState ^= kConnected;
        return (OS_Error)ENOTCONN;
    }
	/* ȷ�����յ����� */
    Assert(theRecvLen > 0);
    *outRecvLenP = (UInt32)theRecvLen;
    return OS_NoErr;
}
