
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTCPTask.cpp
Description: A task object that processes all incoming RTCP packets for the server, 
             and passes each one onto the task for which it belongs, by goes through 
			 all the UDPSockets in the RTPSocketPool.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 




#include "RTCPTask.h"
#include "RTPStream.h"
#include "QTSServerInterface.h"
#include "UDPSocketPool.h"


SInt64 RTCPTask::Run()
{
    const UInt32 kMaxRTCPPacketSize = 2048;
	/* 接收RTCP包的缓存大小最大是2048字节 */
    char thePacketBuffer[kMaxRTCPPacketSize];
	/* 用于存放UDPSocket上得到的一个RTCP包数据,其长度为0 */
    StrPtrLen thePacket(thePacketBuffer, 0);

	/* 获取服务器接口 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    //This task goes through all the UDPSockets in the RTPSocketPool, checking to see
    //if they have data. If they do, it demuxes(复用) the packets and sends the packet onto
    //the proper RTP session.
    EventFlags events = this->GetEvents(); // get and clear events
    
	//Must be done atomically wrt the socket pool.
    if ( (events & Task::kReadEvent) || (events & Task::kIdleEvent) )
    {  
	    /* 得到UDPSocketPool的互斥锁 */
        OSMutexLocker locker(theServer->GetSocketPool()->GetMutex());
		/* 遍历UDPSocketPool中所有的UDPSocketPair组成的队列,直到遇到空的队列元就停下 */
        for (OSQueueIter iter(theServer->GetSocketPool()->GetSocketQueue()); !iter.IsDone(); iter.Next())                 
        {
            UInt32 theRemoteAddr = 0; //Client端的IP
            UInt16 theRemotePort = 0; //Client端的端口

			/* 得到当前队列元所在的对象UDPSocketPai */
            UDPSocketPair* thePair = (UDPSocketPair*)iter.GetCurrent()->GetEnclosingObject();
            Assert(thePair != NULL);
            
			/* 对每个UDPSocketPair,取出它的两个UDPSocket,进一步取出接收RTCP包的那个Socket的UDPDemuxer(它是一个HashTable) */
            for (UInt32 x = 0; x < 2; x++)
            {
				UDPSocket* theSocket = NULL;
				if (x == 0)
					theSocket = thePair->GetSocketA(); //发送RTP包的UDPSocket
				else
					theSocket = thePair->GetSocketB(); //接收RTCP包的UDPSocket
	             
				/* 获取该UDPSocket相关的UDPDemuxer */
				UDPDemuxer* theDemuxer = theSocket->GetDemuxer();
				if (theDemuxer == NULL) 
					continue; //这个必定是发送RTP包的UDPSocket,它没有复用器UDPDemuxer,跳过
				else
				{
					/* 获取UDPDemuxer的互斥锁,并锁定它 */
					theDemuxer->GetMutex()->Lock();
					while (true) //get all the outstanding packets for this socket
					{
						thePacket.Len = 0;
						//接收Client端发送回来的RTCP包数据,存入指定的RTCP包缓存,大小是2048字节
						theSocket->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket.Ptr,  kMaxRTCPPacketSize, &thePacket.Len);
						/* 如果没有数据,发送读事件请求TaskThread继续监听 */
						if (thePacket.Len == 0) //no more packets on this socket!
						{
							theSocket->RequestEvent(EV_RE);   
							break; //中断while循环
						}
                        
						//if this socket has a demuxer(复用器), find the target RTPStream
						if (theDemuxer != NULL)
						{
							//当有数据，从其复用器获取对应的RTPSession任务
							RTPStream* theStream = (RTPStream*)theDemuxer->GetTask(theRemoteAddr, theRemotePort);
							if (theStream != NULL)
								// 处理收到的RTCP包数据
								theStream->ProcessIncomingRTCPPacket(&thePacket);
						}
					}
					/* 释放互斥锁 */
					theDemuxer->GetMutex()->Unlock();
				}
           }
       }
    }
     
    return 0; 
    
}
