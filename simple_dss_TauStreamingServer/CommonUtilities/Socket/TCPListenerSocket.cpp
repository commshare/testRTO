
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


/* 设置Socket为侦听状态,准备被连接 */
OS_Error TCPListenerSocket::Listen(UInt32 queueLength)
{
	/* 确保Socket描述符有效 */
    if (fFileDesc == EventContext::kInvalidFileDesc)
        return EBADF;
    
	/* 设置Socket为侦听状态,准备被连接,返回值为0(成功),或者SOCKET_ERROR */
    int err = ::listen(fFileDesc, queueLength);
	/* 若错误发生,获取具体的出错原因 */
    if (err != 0)
        return (OS_Error)OSThread::GetErrno();
    return OS_NoErr;
}

/* 作以下几件事:创建TCP Socket并设置为非阻塞模式;绑定到入参指定的ip和端口;设置大的缓冲大小(96K字节);设置等待队列长度(128)并开始侦听 */
/* used in QTSServer::CreateListeners() */
OS_Error TCPListenerSocket::Initialize(UInt32 addr, UInt16 port)
{
	/* 创建Socket并设置为非阻塞模式 */
    OS_Error err = this->TCPSocket::Open();
    if (0 == err) do
    {   
        // set SO_REUSEADDR socket option before calling bind.
        // this causes problems on NT (multiple processes can bind simultaneously),
        // so don't do it on NT.
        this->ReuseAddr();

		/* 将入参和新建的Socket绑定 */
        err = this->Bind(addr, port);
        if (err != 0) break; // don't assert this is just a port already in use.

        //
        // Unfortunately we need to advertise a big buffer because our TCP sockets
        // can be used for incoming broadcast data. This could force the server
        // to run out of memory faster(更快地使内存溢出) if it gets bogged down(陷入困境, 停顿), but it is unavoidable.
		/* 设置Socket接受buffer大小(我们需要大的缓存以多播数据) */
        this->SetSocketRcvBufSize(96 * 1024); 
		/* 设置侦听的最大等待连接的队列长度为128 */
        err = this->Listen(kListenQueueLength);
        AssertV(err == 0, OSThread::GetErrno()); 
        if (err != 0) break;
        
    } while (false);
    
    return err;
}

/* 先利用accept()接受某客户端连接请求,记录下它的Socket描述符和ip地址;然后由派生类去获取Task并动态分配服务器上的对应Socket.
   在成功取得Task后相应设置客户端的Socket的属性,在服务器Socket上指派任务并发送EV_RE事件 */
/* 在TCPListenerSocket中,ProcessEvent 函数(继承EventContext 的ProcessEvent()函数)被重载用来创建Socket和Task 对象得配对 */
void TCPListenerSocket::ProcessEvent(int /*eventBits*/)
{
    //we are executing on the same thread as every other
    //socket, so whatever you do here has to be fast.
	/* 该函数运行于系统唯一的EventThread 线程中，所以要尽量快速，以免占用过多的系统资源 */

	/* 在accept()中存放接受的远处客户端的ip地址 */
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);


	/* 注意theTask(通过派生类TCPSocket)和theSocket都是TCPListenerSocket的基类 */
	/**************** 注意:通过子类重载GetSessionTask()使Task(具体说,是RTSPSession)和TCPSocket配对 ***********************/
    Task* theTask = NULL;
    TCPSocket* theSocket = NULL;
    
    //fSocket data member of TCPSocket.
	/* 服务器端的Socket接受客户端的Socket的连接请求,成功后返回服务器新建的接受连接的Socket的描述符,否则返回INVALID_SOCKET */
	int osSocket = accept(fFileDesc, (struct sockaddr*)&addr, &size);

//test osSocket = -1;
	/* 假如出错了,进行错误处理.以下全是错误处理!! */
	if (osSocket == -1)
	{
        //take a look at what this error is.
		/* 获取具体的出错原因 */
        int acceptError = OSThread::GetErrno();

		/* 对得出的错误分情形讨论: */

//test acceptError = EAGAIN;
        if (acceptError == EAGAIN)
        { 
            //If it's EAGAIN, there's nothing on the listen queue right now,
            //so modwatch and return
			/* 在同一socket端口上请求监听指定的EV_RE事件 */
            this->RequestEvent(EV_RE);
            return;
        }
		
//test acceptError = ENFILE;
//test acceptError = EINTR;
//test acceptError = ENOENT;
		 
        //if these error gets returned, we're out of file descriptors, 
        //the server is going to be failing on sockets, logs, qtgroups and qtuser auth file accesses and movie files. The server is not functional.
		// 文件描述符用光,这时服务器会丧失功能,直接退出
		if (acceptError == EMFILE || acceptError == ENFILE)
        {           
			QTSSModuleUtils::LogErrorStr(qtssFatalVerbosity,  "Out of File Descriptors. Set max connections lower and check for competing usage from other processes. Exiting.");
			exit (EXIT_FAILURE);	
        }
        else //假如是其它错误(除EAGAIN\EMFILE\ENFILE以外的),在屏幕上显示错误信息,同时将配对的任务和新建的TCPSocket分别删除和关闭,并返回 
        {   
            char errStr[256];
			/* 确保末尾null-terminated */
            errStr[sizeof(errStr) -1] = 0;
			/* 得到指定格式的errStr形如"accept error = 1 '*****error' on socket. Clean up and continue." */
            qtss_snprintf(errStr, sizeof(errStr) -1, "accept error = %d '%s' on socket. Clean up and continue.", acceptError, strerror(acceptError)); 
            /* 当条件不成立时,在屏幕上显示错误信息 */
			WarnV( (acceptError == 0), errStr);
            
			/**************** 注意:通过子类重载GetSessionTask()使Task和TCPSocket配对 ***********************/
			/* 用RTSPListenerSocket::GetSessionTask()获取Session Task和socket,并对结果分析 */
            theTask = this->GetSessionTask(&theSocket);
            if (theTask == NULL)
            {   
				/* 若没有获取任务,就关闭Socket */
                close(osSocket);
            }
            else
            {  
                theTask->Signal(Task::kKillEvent); // just clean up the task
            }
            
			/* 假如RTSPSession中相对应的theSocket非空,就将其状态设为非连接 */
            if (theSocket)
                theSocket->fState &= ~kConnected; // turn off connected state
            
            return;
        }
	}/* errors handling */
	
	/* 得到Session Task并作错误处理 */
	/* 注意以后的派生类(当是RTSPListenerSocket)去获取任务,并设置好Task和outSocket */
	/**************** 注意:通过子类重载GetSessionTask()使Task和TCPSocket配对 ***********************/
    theTask = this->GetSessionTask(&theSocket);
	/* 如果没有获得任务,将已接受连接的服务器端的osSocket关闭,将服务器上RTSPSession中相对应的theSocket关闭 */
    if (theTask == NULL)
    {    //this should be a disconnect. do an ioctl call?
        close(osSocket);
        if (theSocket)
            theSocket->fState &= ~kConnected; // turn off connected state
    }
	/* 假如成功获取到任务,就分别设置这相对应的两个Socket的相关属性 */
    else//创建Task成功,接着创建Socket 对象
    {   
		/* 确保接受连接的服务器端Socket不是初始状态 */
        Assert(osSocket != EventContext::kInvalidFileDesc);
        
        //set options on the socket
        //we are a server, always disable NAGLE ALGORITHM
		/* 设置接受连接的服务器端Socket是:非延迟的,保持活跃,指定大小的传送缓存 */
        int one = 1;
        int err = ::setsockopt(osSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
        
        err = ::setsockopt(osSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&one, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
    
		/* 设置服务器端传送缓存大小96K字节 */
        int sndBufSize = 96L * 1024L;
        err = ::setsockopt(osSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sndBufSize, sizeof(int));
        AssertV(err == 0, OSThread::GetErrno());
    
        //setup the socket. When there is data on the socket, theTask will get an kReadEvent event
        //
		/* 用服务器接受连接时新建的Socket和连接它的客户端的IP地址等初始化RTSPSession中的TCPSocket数据成员 */
        theSocket->Set(osSocket, &addr);
		/* 设置RTSPSession中的TCPSocket为非阻塞的 */
        theSocket->InitNonBlocking(osSocket);
		/**************** 注意:通过子类重载GetSessionTask()使Task和TCPSocket配对 ***********************/
		/* 给RTSPSession中的TCPSocket数据成员设定任务,就是它所在的RTSPSession对象实例,使RTSPSession和TCPSocket紧密配对配对 */
        theSocket->SetTask(theTask);
		// 完成上述设置后,刚建立连接的这个RTSPSession对象实例的TCPSocket向TaskThread请求读入Client发送的数据
        theSocket->RequestEvent(EV_RE);
    }  

	/* 在两次accept()间休眠吗?进行速度调整! */
    if (fSleepBetweenAccepts)
    { 	
        // We are at our maximum supported sockets
        // slow down so we have time to process the active ones (we will respond with errors or service).
        // wake up and execute again after sleeping. The timer must be reset each time through
        //qtss_printf("TCPListenerSocket slowing down\n");
		// 我们已经用光文件描述符,此处需要设置空闲任务计时器,让当前线程休眠1s
        this->SetIdleTimer(kTimeBetweenAcceptsInMsec); //sleep 1 second
    }
    else
    { 	
        // sleep until there is a read event outstanding (another client wants to connect)
        //qtss_printf("TCPListenerSocket normal speed\n");
		//处理完一次连接请求后,服务器端的侦听TCPListenerSocket对象还要接着监听,等待接入新的Client连接
        this->RequestEvent(EV_RE);
    }

    fOutOfDescriptors = false; // always false for now  we don't properly handle this elsewhere in the code
}

/* 作为一个Task(更精确的,是IdleTask)对象,接收Client的连接,并创建RTSPSession,并和其内的TCPSocket的配对,反复循环执行 */
SInt64 TCPListenerSocket::Run()
{
	//we must clear the event mask!
    EventFlags events = this->GetEvents();
    
    //
    // ProcessEvent cannot be going on when this object gets deleted, because
    // the resolve / release mechanism of EventContext(事件上下文的解析/释放机制) will ensure this thread
    // will block(线程阻塞) before destructing stuff.
    if (events & Task::kKillEvent)
        return -1;
        
        
    //This function will get called when we have run out of file descriptors(文件描述符用满).
    //All we need to do is check the listen queue to see if the situation has
    //cleared up.
    (void)this->GetEvents();
	/* 接收Client的连接,并创建RTSPSession,并和其内的TCPSocket的配对,定义见上 */
    this->ProcessEvent(Task::kReadEvent);
    return 0;
}
