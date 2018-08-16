
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
		/* ��Socket.h�������kNonBlockingSocketType,Ҳ��QTSServer.cpp��RTPSocketPool::ConstructUDPSocketPair() */
        enum
        {
            kWantsDemuxer = 0x0100 //UInt32,�븴������,��ʾ��Ҫ����UDPDemuxerʵ��,�μ�UDPSocket��Ĺ��캯��
        };
    
        UDPSocket(Task* inTask, UInt32 inSocketType);
        virtual ~UDPSocket() { if (fDemuxer != NULL) delete fDemuxer; }

        //Open
		/* open���ݱ����͵�Socket(��,���������͵�Socket��������) */
        OS_Error    Open() { return Socket::Open(SOCK_DGRAM); }

		//
		//Set Multicast
        OS_Error    JoinMulticast(UInt32 inRemoteAddr);
        OS_Error    LeaveMulticast(UInt32 inRemoteAddr);
        OS_Error    SetTtl(UInt16 timeToLive);
        OS_Error    SetMulticastInterface(UInt32 inLocalAddr);

        //returns an ERRNO
		/* ����������ģʽ�ķ�������  */
        OS_Error    SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort,
                                    void* inBuffer, UInt32 inLength);
        /* �Է����ӷ�ʽ(UDP Socket)����һ�����ݱ�������Դ��ַ�ͽ������ݳ��� */                
        OS_Error    RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort,
                     void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen);
        
        //A UDP socket may or may not have a demuxer(������) associated with it. The demuxer
        //is a data structure so the socket can associate incoming data with the proper
        //task(RTPStream?) to process that data (based on source IP addr & port)
		/* ע�������demuxer�ľ��嶨�� */
        UDPDemuxer*  GetDemuxer()    { return fDemuxer; }
        
    private:
    
		/* ��UDPSocket��õ����ݶ�λ��ǡ��������? */
        UDPDemuxer* fDemuxer;
		/* ͨ��UDP Socket����Message(Message handler)��Socket��ַ */
        struct sockaddr_in  fMsgAddr;
};
#endif // __UDPSOCKET_H__

