
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
		/* �����¼�����:����Socket������Ϊ������ģʽ;�����;���ô�Ļ����С;���õȴ����г��� */
        OS_Error        Initialize(UInt32 addr, UInt16 port);

        //You can query(��ѯ) the listener to see if it is failing to accept
        //connections because the OS is out of descriptors.
        Bool16      IsOutOfDescriptors() { return fOutOfDescriptors; }

        void        SlowDown() { fSleepBetweenAccepts = true; }
        void        RunNormal() { fSleepBetweenAccepts = false; }
        //derived object must implement a way of getting tasks & sockets to this object 
		/* �麯��,ע���Ժ��������(����RTSPListenerSocket,�μ�RTSPListenerSocket::GetSessionTask())ȥ��ȡ����,
		   �����ú�Task��outSocket����� */
        virtual Task*   GetSessionTask(TCPSocket** outSocket) = 0;
        
        virtual SInt64  Run();
            
    private:
    
        enum
        {
			/* ��������������accept()֮���ʱ������1�� */
            kTimeBetweenAcceptsInMsec = 1000,   //UInt32
			/* �������趨�ĵȴ����ӵĶ��г�����128 */
            kListenQueueLength = 128            //UInt32
        };

		/* ������accept()����ĳ�ͻ�����������,��¼������Socket��������ip��ַ;Ȼ����������ȥ��ȡTask����̬����������ϵĶ�ӦSocket.
		�ڳɹ�ȡ��Task����Ӧ���ÿͻ��˵�Socket������,�ڷ�����Socket��ָ�����񲢷���EV_RE�¼� */
        virtual void ProcessEvent(int eventBits);
		/* ����SocketΪ����״̬,׼�������� */
        OS_Error    Listen(UInt32 queueLength);

		/* TCP�����׽ӿڵ�IP��ַ�Ͷ˿� */
        UInt32          fAddr;
        UInt16          fPort;
        
		/* �������ù�����? */
        Bool16          fOutOfDescriptors;
		/* ��������accept()֮������������? */
        Bool16          fSleepBetweenAccepts;
};
#endif // __TCPLISTENERSOCKET_H__

