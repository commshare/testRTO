
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
	/* ����RTCP���Ļ����С�����2048�ֽ� */
    char thePacketBuffer[kMaxRTCPPacketSize];
	/* ���ڴ��UDPSocket�ϵõ���һ��RTCP������,�䳤��Ϊ0 */
    StrPtrLen thePacket(thePacketBuffer, 0);

	/* ��ȡ�������ӿ� */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    //This task goes through all the UDPSockets in the RTPSocketPool, checking to see
    //if they have data. If they do, it demuxes(����) the packets and sends the packet onto
    //the proper RTP session.
    EventFlags events = this->GetEvents(); // get and clear events
    
	//Must be done atomically wrt the socket pool.
    if ( (events & Task::kReadEvent) || (events & Task::kIdleEvent) )
    {  
	    /* �õ�UDPSocketPool�Ļ����� */
        OSMutexLocker locker(theServer->GetSocketPool()->GetMutex());
		/* ����UDPSocketPool�����е�UDPSocketPair��ɵĶ���,ֱ�������յĶ���Ԫ��ͣ�� */
        for (OSQueueIter iter(theServer->GetSocketPool()->GetSocketQueue()); !iter.IsDone(); iter.Next())                 
        {
            UInt32 theRemoteAddr = 0; //Client�˵�IP
            UInt16 theRemotePort = 0; //Client�˵Ķ˿�

			/* �õ���ǰ����Ԫ���ڵĶ���UDPSocketPai */
            UDPSocketPair* thePair = (UDPSocketPair*)iter.GetCurrent()->GetEnclosingObject();
            Assert(thePair != NULL);
            
			/* ��ÿ��UDPSocketPair,ȡ����������UDPSocket,��һ��ȡ������RTCP�����Ǹ�Socket��UDPDemuxer(����һ��HashTable) */
            for (UInt32 x = 0; x < 2; x++)
            {
				UDPSocket* theSocket = NULL;
				if (x == 0)
					theSocket = thePair->GetSocketA(); //����RTP����UDPSocket
				else
					theSocket = thePair->GetSocketB(); //����RTCP����UDPSocket
	             
				/* ��ȡ��UDPSocket��ص�UDPDemuxer */
				UDPDemuxer* theDemuxer = theSocket->GetDemuxer();
				if (theDemuxer == NULL) 
					continue; //����ض��Ƿ���RTP����UDPSocket,��û�и�����UDPDemuxer,����
				else
				{
					/* ��ȡUDPDemuxer�Ļ�����,�������� */
					theDemuxer->GetMutex()->Lock();
					while (true) //get all the outstanding packets for this socket
					{
						thePacket.Len = 0;
						//����Client�˷��ͻ�����RTCP������,����ָ����RTCP������,��С��2048�ֽ�
						theSocket->RecvFrom(&theRemoteAddr, &theRemotePort, thePacket.Ptr,  kMaxRTCPPacketSize, &thePacket.Len);
						/* ���û������,���Ͷ��¼�����TaskThread�������� */
						if (thePacket.Len == 0) //no more packets on this socket!
						{
							theSocket->RequestEvent(EV_RE);   
							break; //�ж�whileѭ��
						}
                        
						//if this socket has a demuxer(������), find the target RTPStream
						if (theDemuxer != NULL)
						{
							//�������ݣ����临������ȡ��Ӧ��RTPSession����
							RTPStream* theStream = (RTPStream*)theDemuxer->GetTask(theRemoteAddr, theRemotePort);
							if (theStream != NULL)
								// �����յ���RTCP������
								theStream->ProcessIncomingRTCPPacket(&thePacket);
						}
					}
					/* �ͷŻ����� */
					theDemuxer->GetMutex()->Unlock();
				}
           }
       }
    }
     
    return 0; 
    
}
