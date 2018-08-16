
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TCPListenerSocket.h
Description: Implemention an Idle task class with one exception: when a new connection 
             comes in, the listener socket can assign the new connection to a tcp socket 
			 object and a Task object pair, which implemented by the derived class RTSPListenerSocket.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#ifndef __TCPLISTENERSOCKET_H__
#define __TCPLISTENERSOCKET_H__

#include "TCPSocket.h"
#include "IdleTask.h"

class TCPListenerSocket : public TCPSocket, public IdleTask
{
    public:

        TCPListenerSocket() :   TCPSocket(NULL, Socket::kNonBlockingSocketType), IdleTask(),
                                fAddr(0), fPort(0), fOutOfDescriptors(false), fSleepBetweenAccepts(false) {this->SetTaskName("TCPListenerSocket");}
        virtual ~TCPListenerSocket() {}
        
        //
        // Send a TCPListenerObject a Kill event to delete it.
                
        //addr = listening address. port = listening port. Automatically
        //starts listening
		/* 作以下几件事:创建Socket并设置为非阻塞模式;绑定入参;设置大的缓冲大小;设置等待队列长度 */
        OS_Error        Initialize(UInt32 addr, UInt16 port);

        //You can query(查询) the listener to see if it is failing to accept
        //connections because the OS is out of descriptors.
        Bool16      IsOutOfDescriptors() { return fOutOfDescriptors; }

        void        SlowDown() { fSleepBetweenAccepts = true; }
        void        RunNormal() { fSleepBetweenAccepts = false; }
        //derived object must implement a way of getting tasks & sockets to this object 
		/* 虚函数,注意以后的派生类(当是RTSPListenerSocket,参见RTSPListenerSocket::GetSessionTask())去获取任务,
		   并设置好Task和outSocket的配对 */
        virtual Task*   GetSessionTask(TCPSocket** outSocket) = 0;
        
        virtual SInt64  Run();
            
    private:
    
        enum
        {
			/* 服务器相邻两次accept()之间的时间间隔是1秒 */
            kTimeBetweenAcceptsInMsec = 1000,   //UInt32
			/* 服务器设定的等待连接的队列长度是128 */
            kListenQueueLength = 128            //UInt32
        };

		/* 先利用accept()接受某客户端连接请求,记录下它的Socket描述符和ip地址;然后由派生类去获取Task并动态分配服务器上的对应Socket.
		在成功取得Task后相应设置客户端的Socket的属性,在服务器Socket上指派任务并发送EV_RE事件 */
        virtual void ProcessEvent(int eventBits);
		/* 设置Socket为侦听状态,准备被连接 */
        OS_Error    Listen(UInt32 queueLength);

		/* TCP侦听套接口的IP地址和端口 */
        UInt32          fAddr;
        UInt16          fPort;
        
		/* 描述符用光了吗? */
        Bool16          fOutOfDescriptors;
		/* 相邻两次accept()之间允许休眠吗? */
        Bool16          fSleepBetweenAccepts;
};
#endif // __TCPLISTENERSOCKET_H__

