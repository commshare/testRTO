
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



#ifndef __SOCKET_H__
#define __SOCKET_H__


#include <netinet/in.h>
#include "EventContext.h"
#include "ev.h"

#define SOCKET_DEBUG 0


class Socket : public EventContext
{
    public:
    
		/* 设置Socket是否是阻塞的? */
        enum
        {
            // Pass this in on socket constructors to specify whether the
            // socket should be non-blocking or blocking
            kNonBlockingSocketType = 1
        };

        // This class provides a global event thread.
		/* 创建一个event thread */
        static void Initialize() { sEventThread = new EventThread(); }
		/* 启动一个event thread */
        static void StartThread() { sEventThread->Start(); }
		/* 获取一个event thread */
        static EventThread* GetEventThread() { return sEventThread; }
        
		//绑定/解除绑定到指定的ip地址和端口

        //Binds the socket to the following address.
        //Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
        OS_Error    Bind(UInt32 addr, UInt16 port);
        //The same. but in reverse
        void            Unbind();   
        
		/* 利用::setsockopt()设置指定Socket的相应属性 */

        void            ReuseAddr();
        void            NoDelay();
        void            KeepAlive();
        /* 用入参设置指定的发送缓存大小 */
        void            SetSocketBufSize(UInt32 inNewSize);
        //
        // Returns an error if the socket buffer size is too big
		/* 用入参设置指定的接受缓存大小 */
        OS_Error        SetSocketRcvBufSize(UInt32 inNewSize);
        
        //Send
        //Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		/* 实质上用到WSA函数send() */
        OS_Error    Send(const char* inData, const UInt32 inLength, UInt32* outLengthSent);

        //Read
        //Reads some data.
        //Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		/* 实质上用到WSA函数recv() */
        OS_Error    Read(void *buffer, const UInt32 length, UInt32 *rcvLen);
        
        //WriteV: same as send, but takes an iovec
        //Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
		/* 类似Socket::Send(),但比它功能更强大 */
        OS_Error        WriteV(const struct iovec* iov, const UInt32 numIOvecs, UInt32* outLengthSent);
        
		/*****************************************************************/
		//
		//accessors

        //You can query for the socket's state
		/* used in RTSPRequestStream::ReadRequest() */
        Bool16  IsConnected()   { return (Bool16) (fState & kConnected); }
        Bool16  IsBound()       { return (Bool16) (fState & kBound); }
        
        //If the socket is bound, you may find out to which addr it is bound
        UInt32  GetLocalAddr()  { return ntohl(fLocalAddr.sin_addr.s_addr); }
        UInt16  GetLocalPort()  { return ntohs(fLocalAddr.sin_port); }
        
        StrPtrLen*  GetLocalAddrStr();
        StrPtrLen*  GetLocalPortStr();
        StrPtrLen* GetLocalDNSStr();
      
        enum
        {
            kMaxNumSockets = 4096   //UInt32
        };

    protected:

        //TCPSocket takes an optional task object which will get notified when
        //certain events happen on this socket. Those events are:
        //
        //S_DATA:               Data is currently available on the socket.
        //S_CONNECTIONCLOSING:  Client is closing the connection. No longer necessary
        //                      to call Close or Disconnect, Snd & Rcv will fail.
        
        Socket(Task *notifytask, UInt32 inSocketType);
        virtual ~Socket() {}

        //returns QTSS_NoErr, or appropriate posix error
        OS_Error    Open(int theType);
        
		/* socket state */
        UInt32          fState;
        
        enum
        {
			/* port buffer size */
            kPortBufSizeInBytes = 8,    //UInt32
			/* max IP address size */
            kMaxIPAddrSizeInBytes = 20  //UInt32
        };
        
#if SOCKET_DEBUG
        StrPtrLen       fLocalAddrStr;
        char            fLocalAddrBuffer[kMaxIPAddrSizeInBytes]; 
#endif
        
        //address information (available if bound)
        //these are always stored in network order. Conver
		/* 相互绑定的Socket信息:一个是客户端的,一个是服务器上与客户端相对应的 */
        struct sockaddr_in  fLocalAddr;
        struct sockaddr_in  fDestAddr;
        
        StrPtrLen* fLocalAddrStrPtr;
        StrPtrLen* fLocalDNSStrPtr;
        char fPortBuffer[kPortBufSizeInBytes];
        StrPtrLen fPortStr;
        
        //State flags. Be careful when changing these values, as subclasses add their own
        enum
        {
			/* 是否与ip地址及端口绑定,利用::bind() */
            kBound      = 0x0004,
			/* 是否已经处于连接状态,利用::connect() */
            kConnected  = 0x0008
        };
        
		// This class provides a global event thread.
		/************* NOTE !! *************/
        static EventThread* sEventThread;
        
};

#endif // __SOCKET_H__

