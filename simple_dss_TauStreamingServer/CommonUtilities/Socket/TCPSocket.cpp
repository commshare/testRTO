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

/* ������ε�ֵ���ñ���TCPSocket����Ա����Ϣ,Ȼ���������еĸ���Աֵ,�����ڸ��ƹ��캯�� */
void TCPSocket::SnarfSocket( TCPSocket & fromSocket )
{
    // take the connection away from the other socket
    // and use it as our own.
	/* ȷ����Ա�ǳ�ʼ״̬ */
    Assert(fFileDesc == EventContext::kInvalidFileDesc);
	/* �����������Socket��������״̬  */
    this->Set( fromSocket.fFileDesc, &fromSocket.fRemoteAddr );
    
    // clear the old socket so he doesn't close and the like
	/* �½�remoteaddr�����ʼ��,������fromSocket.Set()ʹ�� */
    struct  sockaddr_in  remoteaddr;
    
    ::memset( &remoteaddr, 0, sizeof( remoteaddr ) );

	/* �����ε�Socket��Ϣ */
    fromSocket.Set( EventContext::kInvalidFileDesc, &remoteaddr );

    // get the event context too
	/* ����EventContext::SnarfEventContext(), �����������ȡ���ݳ�Ա��ֵ,�������ݳ�ԱfRef����Hash Table��,ɾȥ�����ͬkeyֵ��Ref  */
    this->SnarfEventContext( fromSocket );

}

/* �����������ָ��Socket�ı���fLocalAddr,fRemoteAddr����������Socket״̬  */
/* used in TCPListenerSocket::ProcessEvent(),�����һ������Ƿ��������ܿͻ������ӵķ������˵�Socket,�ͻ��˵�ip��ַ�ǵڶ�������,����������������ϵ���� */
void TCPSocket::Set(int inSocket, struct sockaddr_in* remoteaddr)
{
	/* ��������ñ��غ�Զ�˵�Socket */
    fRemoteAddr = *remoteaddr;
    fFileDesc = inSocket;
    
	/* �����������ܵĿͻ��˵�Socket���ǳ�ʼ״̬ʱ,һ��Ҫ�ɹ���ȡ���ڷ������˶�Ӧ��Socket����Ϣ(������Ӧ��IP��ַ) */
    if ( inSocket != EventContext::kInvalidFileDesc ) 
    {
        //make sure to find out what IP address this connection is actually occuring on. That
        //way, we can report correct information to clients asking what the connection's IP is
        socklen_t len = sizeof(fLocalAddr);

		/* ��ȡ�ͻ��˵�Socket���ڷ����������ϵ�Socket��Ϣ,���<<Windows�����̵ڶ���>>.
		   ����fRemoteAddr��fLocalAddr���ѽ��ܵĸÿͻ����Ѿ����ӵĶ�Ӧ��IP��ַ */
        int err = ::getsockname(fFileDesc, (struct sockaddr*)&fLocalAddr, &len);
		/* ȷ���ܳɹ���ȡ����Socket��Ϣ */
        AssertV(err == 0, OSThread::GetErrno());
		/* ȷ����Socket�Ѱ�ip������ */
        fState |= kBound;
        fState |= kConnected;
    }
    else
		/* �����������ܵĿͻ��˵�Socket�ǳ�ʼ״̬ʱ,����״ֻ̬����0 */
        fState = 0;
}

/* ��ȡԶ��Socket��ip��ַ���ַ��� */
StrPtrLen*  TCPSocket::GetRemoteAddrStr()
{
    if (fRemoteStr.Len == kIPAddrBufSize)
        SocketUtils::ConvertAddrToString(fRemoteAddr.sin_addr, &fRemoteStr);
    return &fRemoteStr;
}

/* �����������Զ��Socket��IP��ַ��Ϣ,���뱾��Socket������ϵ  */
OS_Error  TCPSocket::Connect(UInt32 inRemoteAddr, UInt16 inRemotePort)
{
	/* �����������Զ��Socket��IP��ַ��Ϣ */
    ::memset(&fRemoteAddr, 0, sizeof(fRemoteAddr));
    fRemoteAddr.sin_family = AF_INET;        /* host byte order */
    fRemoteAddr.sin_port = htons(inRemotePort); /* short, network byte order */
    fRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);

    /* don't forget to error check the connect()! */
	/* ������Socket��������׼��ͨ�� */
    int err = ::connect(fFileDesc, (sockaddr *)&fRemoteAddr, sizeof(fRemoteAddr));
	/* ����ͨ�ŵ�״̬ */
    fState |= kConnected;
    
	/* ���д����� */
    if (err == -1)
    {
        fRemoteAddr.sin_port = 0;
        fRemoteAddr.sin_addr.s_addr = 0;
		/* ��ȡ����ԭ�� */
        return (OS_Error)OSThread::GetErrno();
    }
    
    return OS_NoErr;

}

