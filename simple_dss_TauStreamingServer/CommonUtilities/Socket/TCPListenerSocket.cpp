
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 TCPListenerSocket.cpp
Description: Implemention an Idle task class with one exception: when a new connection 
             comes in, the listener socket can assign the new connection to a tcp socket 
			 object and a Task object pair, which implemented by the derived class RTSPListenerSocket.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>

#include "QTSSModuleUtils.h"
#include "TCPListenerSocket.h"
#include "Task.h"


/* ����SocketΪ����״̬,׼�������� */
OS_Error TCPListenerSocket::Listen(UInt32 queueLength)
{
	/* ȷ��Socket��������Ч */
    if (fFileDesc == EventContext::kInvalidFileDesc)
        return EBADF;
    
	/* ����SocketΪ����״̬,׼��������,����ֵΪ0(�ɹ�),����SOCKET_ERROR */
    int err = ::listen(fFileDesc, queueLength);
	/* ��������,��ȡ����ĳ���ԭ�� */
    if (err != 0)
        return (OS_Error)OSThread::GetErrno();
    return OS_NoErr;
}

/* �����¼�����:����TCP Socket������Ϊ������ģʽ;�󶨵����ָ����ip�Ͷ˿�;���ô�Ļ����С(96K�ֽ�);���õȴ����г���(128)����ʼ���� */
/* used in QTSServer::CreateListeners() */
OS_Error TCPListenerSocket::Initialize(UInt32 addr, UInt16 port)
{
	/* ����Socket������Ϊ������ģʽ */
    OS_Error err = this->TCPSocket::Open();
    if (0 == err) do
    {   
        // set SO_REUSEADDR socket option before calling bind.
        // this causes problems on NT (multiple processes can bind simultaneously),
        // so don't do it on NT.
        this->ReuseAddr();

		/* ����κ��½���Socket�� */
        err = this->Bind(addr, port);
        if (err != 0) break; // don't assert this is just a port already in use.

        //
        // Unfortunately we need to advertise a big buffer because our TCP sockets
        // can be used for incoming broadcast data. This could force the server
        // to run out of memory faster(�����ʹ�ڴ����) if it gets bogged down(��������, ͣ��), but it is unavoidable.
		/* ����Socket����buffer��С(������Ҫ��Ļ����Զಥ����) */
        this->SetSocketRcvBufSize(96 * 1024); 
		/* �������������ȴ����ӵĶ��г���Ϊ128 */
        err = this->Listen(kListenQueueLength);
        AssertV(err == 0, OSThread::GetErrno()); 
        if (err != 0) break;
        
    } while (false);
    
    return err;
}

/* ������accept()����ĳ�ͻ�����������,��¼������Socket��������ip��ַ;Ȼ����������ȥ��ȡTask����̬����������ϵĶ�ӦSocket.
   �ڳɹ�ȡ��Task����Ӧ���ÿͻ��˵�Socket������,�ڷ�����Socket��ָ�����񲢷���EV_RE�¼� */
/* ��TCPListenerSocket��,ProcessEvent ����(�̳�EventContext ��ProcessEvent()����)��������������Socket��Task �������� */
void TCPListenerSocket::ProcessEvent(int /*eventBits*/)
{
    //we are executing on the same thread as every other
    //socket, so whatever you do here has to be fast.
	/* �ú���������ϵͳΨһ��EventThread �߳��У�����Ҫ�������٣�����ռ�ù����ϵͳ��Դ */

	/* ��accept()�д�Ž��ܵ�Զ���ͻ��˵�ip��ַ */
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);


	/* ע��theTask(ͨ��������TCPSocket)��theSocket����TCPListenerSocket�Ļ��� */
	/**************** ע��:ͨ����������GetSessionTask()ʹTask(����˵,��RTSPSession)��TCPSocket��� ***********************/
    Task* theTask = NULL;
    TCPSocket* theSocket = NULL;
    
    //fSocket data member of TCPSocket.
	/* �������˵�Socket���ܿͻ��˵�Socket����������,�ɹ��󷵻ط������½��Ľ������ӵ�Socket��������,���򷵻�INVALID_SOCKET */
	int osSocket = accept(fFileDesc, (struct sockaddr*)&addr, &size);

//test osSocket = -1;
	/* ���������,���д�����.����ȫ�Ǵ�����!! */
	if (osSocket == -1)
	{
        //take a look at what this error is.
		/* ��ȡ����ĳ���ԭ�� */
        int acceptError = OSThread::GetErrno();

		/* �Եó��Ĵ������������: */

//test acceptError = EAGAIN;
        if (acceptError == EAGAIN)
        { 
            //If it's EAGAIN, there's nothing on the listen queue right now,
            //so modwatch and return
			/* ��ͬһsocket�˿����������ָ����EV_RE�¼� */
            this->RequestEvent(EV_RE);
            return;
        }
		
//test acceptError = ENFILE;
//test acceptError = EINTR;
//test acceptError = ENOENT;
		 
        //if these error gets returned, we're out of file descriptors, 
        //the server is going to be failing on sockets, logs, qtgroups and qtuser auth file accesses and movie files. The server is not functional.
		// �ļ��������ù�,��ʱ��������ɥʧ����,ֱ���˳�
		if (acceptError == EMFILE || acceptError == ENFILE)
        {           
			QTSSModuleUtils::LogErrorStr(qtssFatalVerbosity,  "Out of File Descriptors. Set max connections lower and check for competing usage from other processes. Exiting.");
			exit (EXIT_FAILURE);	
        }
        else //��������������(��EAGAIN\EMFILE\ENFILE�����),����Ļ����ʾ������Ϣ,ͬʱ����Ե�������½���TCPSocket�ֱ�ɾ���͹ر�,������ 
        {   
            char errStr[256];
			/* ȷ��ĩβnull-terminated */
            errStr[sizeof(errStr) -1] = 0;
			/* �õ�ָ����ʽ��errStr����"accept error = 1 '*****error' on socket. Clean up and continue." */
            qtss_snprintf(errStr, sizeof(errStr) -1, "accept error = %d '%s' on socket. Clean up and continue.", acceptError, strerror(acceptError)); 
            /* ������������ʱ,����Ļ����ʾ������Ϣ */
			WarnV( (acceptError == 0), errStr);
            
			/**************** ע��:ͨ����������GetSessionTask()ʹTask��TCPSocket��� ***********************/
			/* ��RTSPListenerSocket::GetSessionTask()��ȡSession Task��socket,���Խ������ */
            theTask = this->GetSessionTask(&theSocket);
            if (theTask == NULL)
            {   
				/* ��û�л�ȡ����,�͹ر�Socket */
                close(osSocket);
            }
            else
            {  
                theTask->Signal(Task::kKillEvent); // just clean up the task
            }
            
			/* ����RTSPSession�����Ӧ��theSocket�ǿ�,�ͽ���״̬��Ϊ������ */
            if (theSocket)
                theSocket->fState &= ~kConnected; // turn off connected state
            
            return;
        }
	}/* errors handling */
	
	/* �õ�Session Task���������� */
	/* ע���Ժ��������(����RTSPListenerSocket)ȥ��ȡ����,�����ú�Task��outSocket */
	/**************** ע��:ͨ����������GetSessionTask()ʹTask��TCPSocket��� ***********************/
    theTask = this->GetSessionTask(&theSocket);
	/* ���û�л������,���ѽ������ӵķ������˵�osSocket�ر�,����������RTSPSession�����Ӧ��theSocket�ر� */
    if (theTask == NULL)
    {    //this should be a disconnect. do an ioctl call?
        close(osSocket);
        if (theSocket)
            theSocket->fState &= ~kConnected; // turn off connected state
    }
	/* ����ɹ���ȡ������,�ͷֱ����������Ӧ������Socket��������� */
    else//����Task�ɹ�,���Ŵ���Socket ����
    {   
		/* ȷ���������ӵķ�������Socket���ǳ�ʼ״̬ */
        Assert(osSocket != EventContext::kInvalidFileDesc);
        
        //set options on the socket
        //we are a server, always disable NAGLE ALGORITHM
		/* ���ý������ӵķ�������Socket��:���ӳٵ�,���ֻ�Ծ,ָ����С�Ĵ��ͻ��� */
        int one = 1;
        int err = ::setsockopt(osSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
        
        err = ::setsockopt(osSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&one, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
    
		/* ���÷������˴��ͻ����С96K�ֽ� */
        int sndBufSize = 96L * 1024L;
        err = ::setsockopt(osSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
    
        //setup the socket. When there is data on the socket, theTask will get an kReadEvent event
        //
		/* �÷�������������ʱ�½���Socket���������Ŀͻ��˵�IP��ַ�ȳ�ʼ��RTSPSession�е�TCPSocket���ݳ�Ա */
        theSocket->Set(osSocket, &addr);
		/* ����RTSPSession�е�TCPSocketΪ�������� */
        theSocket->InitNonBlocking(osSocket);
		/**************** ע��:ͨ����������GetSessionTask()ʹTask��TCPSocket��� ***********************/
		/* ��RTSPSession�е�TCPSocket���ݳ�Ա�趨����,���������ڵ�RTSPSession����ʵ��,ʹRTSPSession��TCPSocket���������� */
        theSocket->SetTask(theTask);
		// ����������ú�,�ս������ӵ����RTSPSession����ʵ����TCPSocket��TaskThread�������Client���͵�����
        theSocket->RequestEvent(EV_RE);
    }  

	/* ������accept()��������?�����ٶȵ���! */
    if (fSleepBetweenAccepts)
    { 	
        // We are at our maximum supported sockets
        // slow down so we have time to process the active ones (we will respond with errors or service).
        // wake up and execute again after sleeping. The timer must be reset each time through
        //qtss_printf("TCPListenerSocket slowing down\n");
		// �����Ѿ��ù��ļ�������,�˴���Ҫ���ÿ��������ʱ��,�õ�ǰ�߳�����1s
        this->SetIdleTimer(kTimeBetweenAcceptsInMsec); //sleep 1 second
    }
    else
    { 	
        // sleep until there is a read event outstanding (another client wants to connect)
        //qtss_printf("TCPListenerSocket normal speed\n");
		//������һ�����������,�������˵�����TCPListenerSocket����Ҫ���ż���,�ȴ������µ�Client����
        this->RequestEvent(EV_RE);
    }

    fOutOfDescriptors = false; // always false for now  we don't properly handle this elsewhere in the code
}

/* ��Ϊһ��Task(����ȷ��,��IdleTask)����,����Client������,������RTSPSession,�������ڵ�TCPSocket�����,����ѭ��ִ�� */
SInt64 TCPListenerSocket::Run()
{
	//we must clear the event mask!
    EventFlags events = this->GetEvents();
    
    //
    // ProcessEvent cannot be going on when this object gets deleted, because
    // the resolve / release mechanism of EventContext(�¼������ĵĽ���/�ͷŻ���) will ensure this thread
    // will block(�߳�����) before destructing stuff.
    if (events & Task::kKillEvent)
        return -1;
        
        
    //This function will get called when we have run out of file descriptors(�ļ�����������).
    //All we need to do is check the listen queue to see if the situation has
    //cleared up.
    (void)this->GetEvents();
	/* ����Client������,������RTSPSession,�������ڵ�TCPSocket�����,������� */
    this->ProcessEvent(Task::kReadEvent);
    return 0;
}
