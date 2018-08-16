
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPSocket.h
Description: Provides a socket derived class to deal with the udp connection from clients.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef __UDPSOCKET_H__
#define __UDPSOCKET_H__


#include <sys/socket.h>
#include <sys/uio.h>
#include "Socket.h"
#include "UDPDemuxer.h"


class   UDPSocket : public Socket
{
    public:
    
        //Another socket type flag (in addition to the ones defined in Socket.h).
        //The value of this can't conflict with those!
		/* 在Socket.h定义的是kNonBlockingSocketType,也见QTSServer.cpp中RTPSocketPool::ConstructUDPSocketPair() */
        enum
        {
            kWantsDemuxer = 0x0100 //UInt32,想复用类型,暗示需要创建UDPDemuxer实例,参见UDPSocket类的构造函数
        };
    
        UDPSocket(Task* inTask, UInt32 inSocketType);
        virtual ~UDPSocket() { if (fDemuxer != NULL) delete fDemuxer; }

        //Open
		/* open数据报类型的Socket(即,创建该类型的Socket并非阻塞) */
        OS_Error    Open() { return Socket::Open(SOCK_DGRAM); }

		//
		//Set Multicast
        OS_Error    JoinMulticast(UInt32 inRemoteAddr);
        OS_Error    LeaveMulticast(UInt32 inRemoteAddr);
        OS_Error    SetTtl(UInt16 timeToLive);
        OS_Error    SetMulticastInterface(UInt32 inLocalAddr);

        //returns an ERRNO
		/* 面向无连接模式的发送数据  */
        OS_Error    SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort,
                                    void* inBuffer, UInt32 inLength);
        /* 以非连接方式(UDP Socket)接收一个数据报并保存源地址和接收数据长度 */                
        OS_Error    RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort,
                     void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen);
        
        //A UDP socket may or may not have a demuxer(复用器) associated with it. The demuxer
        //is a data structure so the socket can associate incoming data with the proper
        //task(RTPStream?) to process that data (based on source IP addr & port)
		/* 注意这里对demuxer的具体定义 */
        UDPDemuxer*  GetDemuxer()    { return fDemuxer; }
        
    private:
    
		/* 将UDPSocket获得的数据定位到恰当的任务? */
        UDPDemuxer* fDemuxer;
		/* 通过UDP Socket发送Message(Message handler)的Socket地址 */
        struct sockaddr_in  fMsgAddr;
};
#endif // __UDPSOCKET_H__

