
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TCPSocket.h
Description: Provides a socket derived class to deal with the tcp connection from clients.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef __TCPSOCKET_H__
#define __TCPSOCKET_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "SafeStdLib.h"
#include "Socket.h" 
#include "Task.h"
#include "StrPtrLen.h"



class TCPSocket : public Socket
{
    public:

        //TCPSocket takes an optional task object which will get notified when
        //certain events happen on this socket. Those events are:
        //
        //S_DATA:               Data is currently available on the socket.
        //S_CONNECTIONCLOSING:  Client is closing the connection. No longer necessary
        //                      to call Close or Disconnect, Snd & Rcv will fail.
        TCPSocket(Task *notifytask, UInt32 inSocketType)
            :   Socket(notifytask, inSocketType),
                fRemoteStr(fRemoteBuffer, kIPAddrBufSize)  {}
        virtual ~TCPSocket() {}

        //Open
		/* 打开streaming类型的Socket */
        OS_Error    Open() { return Socket::Open(SOCK_STREAM); }

        // Connect. Attempts to connect to the specified remote host. If this
        // is a non-blocking socket, this function may return EINPROGRESS, in which
        // case caller must wait for either an EV_RE or an EV_WR. You may call
        // CheckAsyncConnect at any time, which will return OS_NoErr if the connect
        // has completed, EINPROGRESS if it is still in progress, or an appropriate error
        // if the connect failed.
		/* 利用入参配置远端Socket的IP地址信息,并与本地Socket建立联系  */
        OS_Error    Connect(UInt32 inRemoteAddr, UInt16 inRemotePort);
        //OS_Error  CheckAsyncConnect();

        // Basically a copy constructor for this object, also NULLs out the data
        // in tcpSocket.
		/* 类似TCPSocket的复制构造函数,但会清空入参中的数据 */
        void        SnarfSocket( TCPSocket & tcpSocket );

        //ACCESSORS:
        //Returns NULL if not currently available.
        
        UInt32      GetRemoteAddr() { return ntohl(fRemoteAddr.sin_addr.s_addr); }
        UInt16      GetRemotePort() { return ntohs(fRemoteAddr.sin_port); }
        //This function is NOT thread safe!
        StrPtrLen*  GetRemoteAddrStr();

    protected:

		/* 利用入参设置指定Socket的本地fLocalAddr,fRemoteAddr及调整本地Socket状态  */
        void        Set(int inSocket, struct sockaddr_in* remoteaddr);
                            
        enum
        {
            kIPAddrBufSize = 20 //UInt32
        };

        struct sockaddr_in  fRemoteAddr;
        char fRemoteBuffer[kIPAddrBufSize];
        StrPtrLen fRemoteStr;

        /* 这里是引用它的子类为friend class */
        friend class TCPListenerSocket;
};
#endif // __TCPSOCKET_H__

