
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  QTSServer.cpp
Description: define an object for bringing up & shutting down the RTSP serve, and also loads & initializes all modules.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>


#include "QTSServer.h"  //这是相应的头文件
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"

/* socket/task */
#include "SocketUtils.h"
#include "TCPListenerSocket.h"
#include "Task.h"

/* Callback routines and module utilities */
#include "QTSS_Private.h"
#include "QTSSCallbacks.h"
#include "QTSSModuleUtils.h"
#include "QTSSFile.h"

//Compile time modules
/* DSS中相应的模块 */
#include "QTSSErrorLogModule.h"
#include "QTSSFileModule.h"
#include "QTSSAccessLogModule.h"
#include "QTSSFlowControlModule.h"
//#include "QTSSReflectorModule.h"
//#ifdef PROXYSERVER
//#include "QTSSProxyModule.h"
//#endif
//#include "QTSSRelayModule.h"
#include "QTSSPosixFileSysModule.h"
//#include "QTSSAdminModule.h"
//#include "QTSSAccessModule.h"
//#include "QTSSMP3StreamingModule.h"
//#if MEMORY_DEBUGGING
//#include "QTSSWebDebugModule.h"
//#endif


/* RTP/RTCP/RTSP */
#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTPSessionInterface.h"
#include "RTSPSession.h"
#include "RTPStream.h"
#include "RTCPTask.h"


// CLASS DEFINITIONS

class RTSPListenerSocket : public TCPListenerSocket //RTSP responds to TCP as a common approach
{
    public:
    
        RTSPListenerSocket() {}
        virtual ~RTSPListenerSocket() {}
        
        //sole job of this object is to implement this function
		/* 该类唯一的工作就是从TCP端口获取会话任务,参见TCPListenerSocket::GetSessionTask() */
        virtual Task*   GetSessionTask(TCPSocket** outSocket);
        
        //check whether the Listener should be idling
		/* 检查是否超过最大连接数? */
        Bool16 OverMaxConnections(UInt32 buffer);

};

/* 该类对UDPSocketPool中的几个protected函数进行源码定义 */
/* 注意该类的几个函数在UDPSocketPool中被调用,参见UDPSocketPool::GetUDPSocketPair() */
class RTPSocketPool : public UDPSocketPool //RTP responds to UDP as a common approach
{
    public:
    
        // Pool of UDP sockets for use by the RTP server
        
        RTPSocketPool() {}
        ~RTPSocketPool() {}
        
		/* 建立/销毁UDP Socket 对 */
        virtual UDPSocketPair*  ConstructUDPSocketPair();
        virtual void            DestructUDPSocketPair(UDPSocketPair* inPair);

		/* 设置UDP Socket pair中,向外发送RTP数据的socket buffer大小为256K字节,从预设值开始,以减半策略来动态调整RTCP socket接受buffer的大小 */
        virtual void            SetUDPSocketOptions(UDPSocketPair* inPair);
};



char*           QTSServer::sPortPrefString = "rtsp_port"; 
QTSS_Callbacks  QTSServer::sCallbacks;

/* used in QTSServer::Initialize(),在该函数中会给出其设置 */
XMLPrefsParser* QTSServer::sPrefsSource = NULL;
//PrefsSource*    QTSServer::sMessagesSource = NULL;


QTSServer::~QTSServer()  //destructor
{
    // Grab the server mutex. This is to make sure all gets & set values on this
    // object complete before we start deleting stuff
	/* 得到服务器互斥锁,refer to QTSServerInterface.h */
    OSMutexLocker serverlocker(this->GetServerObjectMutex());
    
    // Grab the prefs mutex. This is to make sure we can't reread prefs
    // WHILE shutting down, which would cause some weirdness for QTSS API
    // (some modules could get QTSS_RereadPrefs_Role after QTSS_Shutdown, which would be bad)
	/* 得到服务器预设值对象互斥锁,使用该互斥锁,防止模块关闭时还预读预设值,参见QTSServer::RereadPrefsService() */
    OSMutexLocker locker(this->GetPrefs()->GetMutex());

	/* 设置module state来设置主线程私有数据 */
    QTSS_ModuleState theModuleState;
    theModuleState.curRole = QTSS_Shutdown_Role;
    theModuleState.curTask = NULL;
    OSThread::SetMainThreadData(&theModuleState);

	/* 调用注册关闭角色的相关模块完成资源释放任务 */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kShutdownRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kShutdownRole, x)->CallDispatch(QTSS_Shutdown_Role, NULL);

	/* 清空主线程私有数据 */
    OSThread::SetMainThreadData(NULL);
}

/* used in StartServer() in RunServer.cpp */
Bool16 QTSServer::Initialize(XMLPrefsParser* inPrefsSource,/* PrefsSource* inMessagesSource,*/ UInt16 inPortOverride, Bool16 createListeners)
{
    static const UInt32 kRTPSessionMapSize = 577;//此处可以修改吧?
	/* 一开始是FatalErrorState,正确配置后就更新为StartingUpState */
    fServerState = qtssFatalErrorState;
	/* 传递in parameters进member Variables */
    sPrefsSource = inPrefsSource;
    //sMessagesSource = inMessagesSource;
	/* initialize QTSS API callback routines,给出源码定义,参见下面的该函数定义 */
    this->InitCallbacks();

    //
    // DICTIONARY INITIALIZATION
    /* 初始化所有的Dictionary类对象 */

	/* 利用QTSSDictionaryMap::SetAttribute()循环设置6个模块参数的属性信息QTSSAttrInfoDict::AttrInfo */
    QTSSModule::Initialize();          //kModuleDictIndex
    QTSServerPrefs::Initialize();      //kPrefsDictIndex
    QTSSMessages::Initialize();        //kTextMessagesDictIndex
	QTSSFile::Initialize();            //kFileDictIndex
	QTSSUserProfile::Initialize();     //kQTSSUserProfileDictIndex
    RTSPRequestInterface::Initialize();//kRTSPRequestDictIndex & kRTSPHeaderDictIndex
    RTSPSessionInterface::Initialize();//kRTSPSessionDictIndex
    RTPSessionInterface::Initialize(); //kClientSessionDictIndex
    RTPStream::Initialize();           //kRTPStreamDictIndex
    
	RTSPSession::Initialize();         //创建http proxy tunnel map,构造sHTTPResponseHeaderBuf/sHTTPResponseNoServerHeaderBuf缓存
    
    //
    //RTP\RTSP STUB SERVER INITIALIZATION
    //
    // Construct stub versions(存根版本) of the prefs and messages dictionaries. We need
    // both of these to initialize the server, but they have to be stubs because
    // their QTSSDictionaryMaps will presumably be modified when modules get loaded.
    
	/* 新建并用入参初始化数据成员 */
    fSrvrPrefs = new QTSServerPrefs(inPrefsSource, false); // First time, don't write changes to the prefs file
    fSrvrMessages = new QTSSMessages(/*inMessagesSource*/); //服务器唯一的TextMessage对象
    QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());//使用上述新建实例对象初始化QTSSModuleUtils静态类对象,便于记录Error Log

    //
    // SETUP ASSERT BEHAVIOR 处理Assert行为
    //
    // Depending on the server preference, we will either break when we hit an
    // assert, or log the assert to the error log
    if (!fSrvrPrefs->ShouldServerBreakOnAssert())
        SetAssertLogger(this->GetErrorLogStream());// the error log stream is our assert logger
        
    //
    // CREATE GLOBAL OBJECTS
	/* 在基类UDPSocketPool上动态创建子类RTPSocketPool,从而方便将动态创建的UDPSocketPair放入UDPSocketPool,进行统一的维护和管理 */
    fSocketPool = new RTPSocketPool();
	/* 创建大小为577的RTP session map(hash table),它会利用唯一的会话ID来标识和管理所有的RTPSession或RTSPSession */
    fRTPMap = new OSRefTable(kRTPSessionMapSize);//577

    //
    // Load ERROR LOG module only. This is good in case there is a startup error.
    /* 动态创建该error log module, 下面在侦听端口时可能要用来记录错误日志 */
    QTSSModule* theLoggingModule = new QTSSModule("QTSSErrorLogModule");
	/* 从磁盘加载dll/.so得到和调用主入口函数QTSS_MainEntryPointPtr(此处即&QTSSErrorLogModule_Main),从而得到分发函数指针并设置为fDispatchFunc */
    (void)theLoggingModule->SetupModule(&sCallbacks, &QTSSErrorLogModule_Main);
	/* 将该模块加入静态sModuleQueue */
    (void)AddModule(theLoggingModule);
    this->BuildModuleRoleArrays(); //重置静态模块角色数组sModuleArray

    //
    // DEFAULT IP ADDRESS & DNS NAME
	/* 从预设值获取RTSP的ip地址,设置第一个绑定的ip为默认的点播IP并配置它相应的DNS域名和ip address string属性,若默认的域名地址为空就将错误记进error log */
    if (!this->SetDefaultIPAddr())
        return false;

    //
    // STARTUP TIME - record it
	/* 记录Server启动的时间和GMT偏移 */
    fStartupTime_UnixMilli = OS::Milliseconds();
    fGMTOffset = OS::GetGMTOffset();//当前时区与GMT相差8小时
        
    //
    // BEGIN LISTENING--erro log
    if (createListeners) //note the difference:in param createListeners (boolean type in parameters) and CreateListeners() 
    {
		/* 注意第一个参数表示现在已经开始监听,但是未向TaskThread请求读事件(它在QTSServer::StartTasks()这样做),QTSServer.CreateListeners()的返回值是 fNumListeners>0 ? */
		// 动态创建RTSP Listeners socket数组,并配置更新数据成员fNumListeners和fListeners
		/****************  NOTE: Important !! **********************************/
        if ( !this->CreateListeners(false, fSrvrPrefs, inPortOverride) ) 
		/****************  NOTE: Important !! **********************************/
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSomePortsFailed, 0);//记录"端口失败"错误日志进Error Log流
    }
    
	/* 当创建了侦听但listeners Number为0就报错并写进error log,其实这步,上面已经做过了 */
    if ( fNumListeners == 0 )
    {   
		if (createListeners)
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoPortsSucceeded, 0);//记录错误日志进Error Log流
        return false;
    }

    /* 及时更新Server state */
    fServerState = qtssStartingUpState;
    return true;
}

/* 加载和初始化所有的Module */
void QTSServer::InitModules(QTSS_ServerState inEndState)
{
    //
    // LOAD AND INITIALIZE ALL MODULES
        
    // temporarily set the verbosity on missing prefs when starting up to debug level
    // This keeps all the pref messages being written to the config file from being logged.
    // don't exit until the verbosity level is reset back to the initial prefs.
     
    LoadModules(fSrvrPrefs);
    LoadCompiledInModules();
    this->BuildModuleRoleArrays();//重置静态sModuleArray

	/* 当初始化兼容模块时,设置Server错误级别为Warning,最后再改回来 */
    fSrvrPrefs->SetErrorLogVerbosity(qtssWarningVerbosity); // turn off info messages while initializing compiled in modules.
    
	// CREATE MODULE OBJECTS AND READ IN MODULE PREFS
    
    // Finish setting up modules. Create our final prefs & messages objects,
    // register all global dictionaries, and invoke the modules in their Init roles.
    fStubSrvrPrefs = fSrvrPrefs;			
    fStubSrvrMessages = fSrvrMessages;

    fSrvrPrefs = new QTSServerPrefs(sPrefsSource, true); // Now write changes to the prefs file. First time, we don't because the error messages won't get printed.
    QTSS_ErrorVerbosity serverLevel = fSrvrPrefs->GetErrorLogVerbosity(); // get the real prefs verbosity and save it.
    fSrvrPrefs->SetErrorLogVerbosity(qtssWarningVerbosity); // turn off info messages while loading dynamic modules.
   
 
    fSrvrMessages = new QTSSMessages(/*sMessagesSource*/);
    QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());//使用上述新建实例对象初始化QTSSModuleUtils静态类对象,便于记录Error Log

	/* set dictionary attr and write into server internally */
	// 用上述新建的对象来设置服务器预设值和消息对象
    this->SetVal(qtssSvrMessages, &fSrvrMessages, sizeof(fSrvrMessages));
    this->SetVal(qtssSvrPreferences, &fSrvrPrefs, sizeof(fSrvrPrefs));

    // 在当前服务器对象中加入读预设值服务的属性
    // ADD REREAD PREFERENCES SERVICE
    (void)QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
         AddAttribute(QTSS_REREAD_PREFS_SERVICE, (QTSS_AttrFunctionPtr)QTSServer::RereadPrefsService, qtssAttrDataTypeUnknown, qtssAttrModeRead);

    //
    // INVOKE INITIALIZE ROLE
	/* 初始化module */
    this->DoInitRole();

	/* 将入参inEndState传递给成员变量 */
    if (fServerState != qtssFatalErrorState)
        fServerState = inEndState; // Server is done starting up!   

    /* 改回Server pref错误级别,serverLevel上面已做了保存 */
    fSrvrPrefs->SetErrorLogVerbosity(serverLevel); // reset the server's verbosity back to the original prefs level. see also line 258.
}

/* 创建RTCP Task来处理RTPSocketPool中所有的UDPSocketPair上可能到来的RTCP包数据,RTPStatsUpdaterTask,
对每个TCPListenerSocket,向任务线程请求读事件 */
void QTSServer::StartTasks()
{
	/* 创建RTP/RTCP Task pair */
    fRTCPTask = new RTCPTask();
	/* 创建RTP stats update Task */
    fStatsTask = new RTPStatsUpdaterTask();

    //
    // Start listening
	/* 对每个TCPListenerSocket,监听指定窗口是否有指定的读请求事件发生? */
    for (UInt32 x = 0; x < fNumListeners; x++)
        fListeners[x]->RequestEvent(EV_RE); // what is EV_RE? event_read
}

/* 从预设值获取RTSP的ip地址,设置第一个绑定的ip为默认的点播IP并配置它相应的DNS域名和ip address string属性,若默认的域名地址为空就将错误记进error log */
Bool16 QTSServer::SetDefaultIPAddr()
{
    //check to make sure there is an available ip interface
	/* 假如没有找到ip address,就将错误qtssMsgNotConfiguredForIP记录进error log,并返回  */
    if (SocketUtils::GetNumIPAddrs() == 0) 
    {
        QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgNotConfiguredForIP, 0);
        return false;
    }

    //find out what our default IP addr is & dns name
	/* 从预设值获取RTSP的ip地址字符串 */
    UInt32 theNumAddrs = 0;
    UInt32* theIPAddrs = this->GetRTSPIPAddrs(fSrvrPrefs, &theNumAddrs);
	/* 设置fDefaultIPAddr */
    if (theNumAddrs == 1) //这表示没有配置绑定的RTSP ip
        fDefaultIPAddr = SocketUtils::GetIPAddr(0);
    else
        fDefaultIPAddr = theIPAddrs[0];//设置第一个绑定的ip为默认的点播IP

    delete [] theIPAddrs;//删除在QTSServer::GetRTSPIPAddrs()中已经分配的ip地址数组
    
    /* 通过循环查找default IP address,并相应设置它默认的DNS域名和ip address字符串属性 */
    for (UInt32 ipAddrIter = 0; ipAddrIter < SocketUtils::GetNumIPAddrs(); ipAddrIter++) // note ipAddrIter is a iterative variable
    {
        if (SocketUtils::GetIPAddr(ipAddrIter) == fDefaultIPAddr)
        {
            this->SetVal(qtssSvrDefaultDNSName, SocketUtils::GetDNSNameStr(ipAddrIter));
            Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != NULL); //make sure whether null pointer
            this->SetVal(qtssSvrDefaultIPAddrStr, SocketUtils::GetIPAddrStr(ipAddrIter));
            Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != NULL);
            break;
        }
    }

	/* 默认的域名地址为空就将错误记进error log */
    if (this->GetValue(qtssSvrDefaultDNSName)->Ptr == NULL) //notified by assert()
    {
        //If we've gotten here, what has probably happened is the IP address (explicitly
        //entered as a preference) doesn't exist
        QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgDefaultRTSPAddrUnavail, 0); //record the errorlog
        return false;   
    }
    return true;
}               

// 注意: 本函数的主要作用就是配置更新fNumListeners和fListeners
/* 依据服务器预设值来得到IP地址数组和端口数组,按照是否使用公用端口,来得到端口追踪结构体数组,再动态创建RTSP Listeners socket数组newListenerArray,
在考虑当前使用的fNumListeners和fListeners后,用newListenerArray更新它们,返回最终该fListeners大小是否>0的bool值 */
Bool16 QTSServer::CreateListeners(Bool16 startListeningNow, QTSServerPrefs* inPrefs, UInt16 inPortOverride)
{
	/* 端口追踪结构体定义 */
    struct PortTracking 
    {
        PortTracking() : fPort(0), fIPAddr(0), fNeedsCreating(true) {}
        
        UInt16 fPort;
        UInt32 fIPAddr;
        Bool16 fNeedsCreating; // 需要生成TCPListenerSocket*吗?
    };
    
	/* 端口追踪结构体数组及数目,这是我们下面要重点研究的! */
    PortTracking* thePortTrackers = NULL;   
    UInt32 theTotalPortTrackers = 0;
    
    // Get the IP addresses from the pref
    UInt32 theNumAddrs = 0;
	/* 获取bind-RTSP-ip-address的数目theNumAddrs和该ip address的数组theIPAddrs */
    UInt32* theIPAddrs = this->GetRTSPIPAddrs(inPrefs, &theNumAddrs);   //theIPAddrs defintion
    UInt32 index = 0;
    
	/* 对第三个入参(端口重叠)分情形讨论,当重叠端口非0时,对每个绑定的IP地址,来创建恰当大小的端口追踪结构体数组并配置它的各项参数,其中端口必须是指定的重叠端口 */
    if ( inPortOverride != 0)
    { 
		/* 配置bind-RTSP-ip-address相应的端口追踪结构体数组的各项参数 */
        theTotalPortTrackers = theNumAddrs; // one port tracking struct for each IP addr
        thePortTrackers = NEW PortTracking[theTotalPortTrackers]; //creat and return the pointer of struct array
        for (index = 0; index < theNumAddrs; index++)
        {
            thePortTrackers[index].fPort = inPortOverride;
            thePortTrackers[index].fIPAddr = theIPAddrs[index];
        }
    }
    else /* 当入参指定的重叠端口为0时(端口不需重叠),这种情形复杂些 */
    {
        UInt32 theNumPorts = 0;
		/* 从预置的RTSP端口获取port的个数theNumPorts,并返回以端口号为分量的数组thePorts */
        UInt16* thePorts = GetRTSPPorts(inPrefs, &theNumPorts);
		/* 要追踪的地址端口数,注意:每个ip address都要遍历一遍端口数组 */
        theTotalPortTrackers = theNumAddrs * theNumPorts; //* means multiplication
		/* 创建新的端口追踪结构体 */
        thePortTrackers = NEW PortTracking[theTotalPortTrackers]; //creat and return the pointer of struct array
        
        UInt32 currentIndex  = 0;
        
		/* 对每个ip address的不同port分别来配置端口追踪结构体中的成员 */
        for (index = 0; index < theNumAddrs; index++)
        {
            for (UInt32 portIndex = 0; portIndex < theNumPorts; portIndex++)
            {
				/* 表示端口追踪结构体中的分量索引 */
                currentIndex = (theNumPorts * index) + portIndex;
                
				/* 区分这三个量:currentIndex(端口追踪结构体中的分量索引),portIndex(端口索引),index(ip address索引) */
                thePortTrackers[currentIndex].fPort = thePorts[portIndex]; //指定端口
                thePortTrackers[currentIndex].fIPAddr = theIPAddrs[index]; //指定ip地址
            }
        }
                
        delete [] thePorts;//删除QTSServer::GetRTSPPorts()中动态分配的端口数组
    }
    
    delete [] theIPAddrs; //删除QTSServer::GetRTSPIPAddrs()中动态分配的IP地址数组
    
    // Now figure out which of these ports we are *already* listening on.
    // If we already are listening on that port, just move the pointer to the
    // listener over to the new array
	/* 首先粗糙地创建一个同大小的TCPListenerSocket数组,在分析当前已做侦听的端口后,针对每个RTSPsocket(IP address+port),创建一个TCP Listen socket,
	注意最后会用其设置数据成员fListeners,fNumListeners */
    TCPListenerSocket** newListenerArray = NEW TCPListenerSocket*[theTotalPortTrackers];//refer to line 367
	/* 用于统计不需修改TCP Listener socket的个数 */
    UInt32 curPortIndex = 0; //curPortIndex definition
    
	// 首先指出哪些端口,我们已经在侦听,将其TCPListenerSocket*放入新建的newListenerArray
	/* 假如当前数据成员fListeners的端口和ip地址和上面的端口追踪结构体数组的某分量相同,就将其添加进newListenerArray  */
    for (UInt32 count = 0; count < theTotalPortTrackers; count++)
    {
        for (UInt32 count2 = 0; count2 < fNumListeners; count2++)
        {
			/* 假如TCP Listener socket和上面的thePortTrackers分量相同,就不需建立该分量 */
            if ((fListeners[count2]->GetLocalPort() == thePortTrackers[count].fPort) && 
                (fListeners[count2]->GetLocalAddr() == thePortTrackers[count].fIPAddr))
            {
                thePortTrackers[count].fNeedsCreating = false;
				/* 将当前该TCP Listener socket移进上面新建的TCP Listener socket数组中 */
                newListenerArray[curPortIndex++] = fListeners[count2];
                Assert(curPortIndex <= theTotalPortTrackers);
                break;
            }
        }
    }
    
    //
    // Create any new listeners we need
	/* 注意是RTSPListenerSocket(TCP listeners socket的派生类),在TCP listeners socket的后面接着创建 */
	/* 将上端口追踪结构体数组中标记为fNeedsCreating=true的分量对应的索引的newListenerArray分量,动态创建后,依次加入newListenerArray,初始化,向TaskThread请求事件;若出错,则记录错误原因进error log */
    for (UInt32 count3 = 0; count3 < theTotalPortTrackers; count3++)
    {
		// 假如确实需要动态创建TCPListenerSocket*
        if (thePortTrackers[count3].fNeedsCreating)
        {
			/* 注意curPortIndex是接上面的TCP listeners socket计数来的,依循环一个一个创建 */
            newListenerArray[curPortIndex] = NEW RTSPListenerSocket(); 
			/* 创建TCP Socket并设置为非阻塞模式;绑定到入参指定的ip和端口;设置大的缓冲大小(96K字节);设置等待队列长度(128)并开始侦听 */
            QTSS_Error err = newListenerArray[curPortIndex]->Initialize(thePortTrackers[count3].fIPAddr, thePortTrackers[count3].fPort);

			/* 给定指定形式的port字符串 */
            char thePortStr[20];
            qtss_sprintf(thePortStr, "%hu", thePortTrackers[count3].fPort);
            
            //
            // If there was an error creating this listener, destroy it and log an error
			/* 假如要现在开始侦听,但是创建这个RTSP Listeners socket出错了,就立即删去该分量 */
            if ((startListeningNow) && (err != QTSS_NoErr))
                delete newListenerArray[curPortIndex];  //refer to line 415

			// 假如在对RTSPListenerSocket进行初始化时出错,记录下错误信息
            if (err == EADDRINUSE) /* 地址已在使用 */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortInUse, 0, thePortStr);
            else if (err == EACCES) /* 访问错误 */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortAccessDenied, 0, thePortStr);
            else if (err != QTSS_NoErr) /* 侦听端口错误 */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortError, 0, thePortStr);
            else
            {
                // 假如RTSPListenerSocket实例成功创建,并初始化成功,且要现在开始侦听,就向TaskThread请求读事件
                // This listener was successfully created.
                if (startListeningNow)
                    newListenerArray[curPortIndex]->RequestEvent(EV_RE); 
                curPortIndex++;
            }
        }
    }
    
    //
    // Kill any listeners that we no longer need
	/* 将数据成员fListeners数组中的每个分量和newListenerArray数组中每个分量逐一对比,相同的保留,不同的发信号让TaskThread删去 */
    for (UInt32 count4 = 0; count4 < fNumListeners; count4++)
    {
        Bool16 deleteThisOne = true;
        
		/* 在现存的总的TCP和RTSP Listener port数组中,若其分量与TCP Listen socket相同,就保留 */
        for (UInt32 count5 = 0; count5 < curPortIndex; count5++)//curPortIndex == theTotalPortTrackers??
        {
            if (newListenerArray[count5] == fListeners[count4])
                deleteThisOne = false;
        }
        
		/* 那些要删去的,就发信号删去 */
        if (deleteThisOne)
            fListeners[count4]->Signal(Task::kKillEvent); //Signal means?
    }
    
    // Finally, make our server attributes and fListener privy to the new...
	/* 最后将调整更新TCP Listen socket数组及其元素个数来设置数据成员fListeners,fNumListeners,这是该函数主要的作用!! */
	/******************* NOTE !! ******************/
    fListeners = newListenerArray;
    fNumListeners = curPortIndex;  //line 392
	/******************* NOTE !! ******************/
	/* 用来记录设置过服务器端口属性的个数 */
    UInt32 portIndex = 0;
          
	/* 重新设置服务器端口属性 */
    for (UInt32 count6 = 0; count6 < fNumListeners; count6++)
    {
        if  (fListeners[count6]->GetLocalAddr() != INADDR_LOOPBACK)
        {
            UInt16 thePort = fListeners[count6]->GetLocalPort();
            (void)this->SetValue(qtssSvrRTSPPorts, portIndex, &thePort, sizeof(thePort), QTSSDictionary::kDontObeyReadOnly);
            portIndex++;
        }
    }
	/* 设置服务器端口个数 */
    this->SetNumValues(qtssSvrRTSPPorts, portIndex);

	/* 删去上面动态创建的端口追踪数组,因为它的作用已经完成了 */
    delete [] thePortTrackers; 

    return (fNumListeners > 0);
}

/* 从绑定的ip地址获取地址的个数*outNumAddrsPtr,并将地址以十进点分格式的数组theIPAddrArray输出 */
UInt32* QTSServer::GetRTSPIPAddrs(QTSServerPrefs* inPrefs, UInt32* outNumAddrsPtr)
{
	 // Returns the number of values associated with a given attribute
	/*  see QTSSDictionary.h  */
	/* 从服务器预设值对象分析绑定了几个ip address? 将它以十进制形式数组输出 */
    UInt32 numAddrs = inPrefs->GetNumValues(qtssPrefsRTSPIPAddr);//"bind_ip_addr" 
    UInt32* theIPAddrArray = NULL;
    
	/* 假如没有"bind_ip_addr",就新建一个四字节,任何ip地址都可 */
    if (numAddrs == 0)
    {
        *outNumAddrsPtr = 1; //note * denote the value not the pointer
        theIPAddrArray = NEW UInt32[1]; //define an array which will be return as function value
        theIPAddrArray[0] = INADDR_ANY; //可以是任何IP地址
    }
    else
    {
		/* 这两个参数都是输出参数,很重要! */
        theIPAddrArray = NEW UInt32[numAddrs + 1]; //创建一个指定个数+1的数组
        UInt32 arrIndex = 0;
        // Get the ip addr out of the prefs dictionary
        for (UInt32 theIndex = 0; theIndex < numAddrs; theIndex++)
        {
            QTSS_Error theErr = QTSS_NoErr;
            
			/* 从"bind_ip_addr"中获取指定索引的IP address string */
            char* theIPAddrStr = NULL;
            theErr = inPrefs->GetValueAsString(qtssPrefsRTSPIPAddr, theIndex, &theIPAddrStr);
            if (theErr != QTSS_NoErr)
            {
                delete [] theIPAddrStr; //假如获取预设值的ip地址失败,删去该IP字符串缓存
                break;
            }

            
			/* 将得到的IP address string转换为十进点分制的IP address,并赋给数组theIPAddrArray的相应分量 */
            UInt32 theIPAddr = 0;
            if (theIPAddrStr != NULL)
            {
                theIPAddr = SocketUtils::ConvertStringToAddr(theIPAddrStr);
                delete [] theIPAddrStr;//删去已经分配的该IP字符串缓存
                
                if (theIPAddr != 0)
                    theIPAddrArray[arrIndex++] = theIPAddr;//给IP地址数组赋值
            }   
        }
        
		/* 假如得到ip address string,但是转换为十进点分制的IP address后为0,则该ip address可为任意ip address */
        if ((numAddrs == 1) && (arrIndex == 0))
            theIPAddrArray[arrIndex++] = INADDR_ANY;
        else
            theIPAddrArray[arrIndex++] = INADDR_LOOPBACK;
    
		/* 该循环就是确定这个值 */
        *outNumAddrsPtr = arrIndex;
    }
    
    return theIPAddrArray;
}

/* 从服务器预设值对象获取预置的rtsp端口数目,并动态创建和返回端口值数组 */
UInt16* QTSServer::GetRTSPPorts(QTSServerPrefs* inPrefs, UInt32* outNumPortsPtr)
{
	/* 从服务器预设值对象,获取RTSP ports数目,默认是4个 */
    *outNumPortsPtr = inPrefs->GetNumValues(qtssPrefsRTSPPorts);//"rtsp_ports"
    
    if (*outNumPortsPtr == 0)
        return NULL;
     
	/* 动态创建指定维数(4)的端口数组 */
    UInt16* thePortArray = NEW UInt16[*outNumPortsPtr];
    
    for (UInt32 theIndex = 0; theIndex < *outNumPortsPtr; theIndex++)
    {
        // Get the ip addr out of the prefs dictionary
		/* 长度为两个字节 */
        UInt32 theLen = sizeof(UInt16);
        QTSS_Error theErr = QTSS_NoErr;
        theErr = inPrefs->GetValue(qtssPrefsRTSPPorts, theIndex, &thePortArray[theIndex], &theLen);
        Assert(theErr == QTSS_NoErr);   
    }
    
    return thePortArray;
}

/* used in  StartServer() in RunServer.cpp */
//function finds all IP addresses on this machine, and binds 1 RTP / RTCP socket pair to a port pair on each address.
/* 对服务器上的所有IP地址和port的配对,在RTPSocketPool中都创建一对UDP Socket对,统计出对数,启动Socket pair向任务线程请求读事件,并返回true */
Bool16  QTSServer::SetupUDPSockets()
{   
	/* 统计服务器上的IP地址个数,分配对应的RTP/RTCP Socket对数 */
    UInt32 theNumAllocatedPairs = 0;
    for (UInt32 theNumPairs = 0; theNumPairs < SocketUtils::GetNumIPAddrs(); theNumPairs++)
    {
        UDPSocketPair* thePair = fSocketPool->CreateUDPSocketPair(SocketUtils::GetIPAddr(theNumPairs), 0);/* default port is 0 */
        if (thePair != NULL)
        {
            theNumAllocatedPairs++; //port pair increments by 1
			/* 启动UDPSocket pair,向任务线程请求读事件 */
            thePair->GetSocketA()->RequestEvent(EV_RE);
            thePair->GetSocketB()->RequestEvent(EV_RE);
        }
     }

    // only return an error if we couldn't allocate ANY pairs of sockets
	// 假如不能分配任何UDPSocketPair,设置服务器状态为Fatal,并返回
    if (theNumAllocatedPairs == 0)
    {
            fServerState = qtssFatalErrorState; // also set the state to fatal error
            return false;
    }

    return true;
}

/* 对Windows无效 */
Bool16  QTSServer::SwitchPersonality()
{
    OSCharArrayDeleter runGroupName(fSrvrPrefs->GetRunGroupName());
    OSCharArrayDeleter runUserName(fSrvrPrefs->GetRunUserName());

    if (::strlen(runGroupName.GetObject()) > 0)
    {
        struct group* gr = ::getgrnam(runGroupName.GetObject());
        if (gr == NULL || ::setgid(gr->gr_gid) == -1)
        {
            char buffer[kErrorStrSize];

            QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgCannotSetRunGroup, 0,
                    runGroupName.GetObject(), qtss_strerror(OSThread::GetErrno(), buffer, sizeof(buffer)));
            return false;
        }
    }
    
    if (::strlen(runUserName.GetObject()) > 0)
    {
        struct passwd* pw = ::getpwnam(runUserName.GetObject());
        if (pw == NULL || ::setuid(pw->pw_uid) == -1)
        {
            QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgCannotSetRunUser, 0,
                    runUserName.GetObject(), strerror(OSThread::GetErrno()));
            return false;
        }
    }

   return true;
}



/* 按照标准步骤static加载服务器compiled的modules */
void    QTSServer::LoadCompiledInModules() //load modules into the server
{
#ifndef DSS_DYNAMIC_MODULES_ONLY  //need to be added by me
    // MODULE DEVELOPERS SHOULD ADD THE FOLLOWING THREE LINES OF CODE TO THIS
    // FUNCTION IF THEIR MODULE IS BEING COMPILED INTO THE SERVER.
    //
	// 参见QTSSModule::QTSSModule(),这里第二个参数为空,说明是static Module
    // QTSSModule* myModule = new QTSSModule("__MODULE_NAME__");   //where __MODULE_NAME__ is just created (static) module name by modules developers
    // (void)myModule->Initialize(&sCallbacks, &__MODULE_MAIN_ROUTINE__);
    // (void)AddModule(myModule);
    //
    // The following modules are all compiled into the server in the same way. 
    
    QTSSModule* theFileModule = new QTSSModule("QTSSFileModule");
    (void)theFileModule->SetupModule(&sCallbacks, &QTSSFileModule_Main);
    (void)AddModule(theFileModule);

    /*QTSSModule* theReflectorModule = new QTSSModule("QTSSReflectorModule");
    (void)theReflectorModule->SetupModule(&sCallbacks, &QTSSReflectorModule_Main);
    (void)AddModule(theReflectorModule);

	QTSSModule* theRelayModule = new QTSSModule("QTSSRelayModule");
    (void)theRelayModule->SetupModule(&sCallbacks, &QTSSRelayModule_Main);
    (void)AddModule(theRelayModule);*/

    QTSSModule* theAccessLog = new QTSSModule("QTSSAccessLogModule");
    (void)theAccessLog->SetupModule(&sCallbacks, &QTSSAccessLogModule_Main);
    (void)AddModule(theAccessLog);

    QTSSModule* theFlowControl = new QTSSModule("QTSSFlowControlModule");
    (void)theFlowControl->SetupModule(&sCallbacks, &QTSSFlowControlModule_Main);
    (void)AddModule(theFlowControl);

    QTSSModule* theFileSysModule = new QTSSModule("QTSSPosixFileSysModule");
    (void)theFileSysModule->SetupModule(&sCallbacks, &QTSSPosixFileSysModule_Main);
    (void)AddModule(theFileSysModule);

//    QTSSModule* theAdminModule = new QTSSModule("QTSSAdminModule");
//    (void)theAdminModule->SetupModule(&sCallbacks, &QTSSAdminModule_Main);
//    (void)AddModule(theAdminModule);
//
//    QTSSModule* theMP3StreamingModule = new QTSSModule("QTSSMP3StreamingModule");
//    (void)theMP3StreamingModule->SetupModule(&sCallbacks, &QTSSMP3StreamingModule_Main);
//    (void)AddModule(theMP3StreamingModule);
//
//#if MEMORY_DEBUGGING
//    QTSSModule* theWebDebug = new QTSSModule("QTSSWebDebugModule");
//    (void)theWebDebug->SetupModule(&sCallbacks, &QTSSWebDebugModule_Main);
//    (void)AddModule(theWebDebug);
//#endif
//
//    QTSSModule* theQTACESSmodule = new QTSSModule("QTSSAccessModule");
//    (void)theQTACESSmodule->SetupModule(&sCallbacks, &QTSSAccessModule_Main);
//    (void)AddModule(theQTACESSmodule);
//
//#endif //DSS_DYNAMIC_MODULES_ONLY
//
//#ifdef PROXYSERVER
//    QTSSModule* theProxyModule = new QTSSModule("QTSSProxyModule");  // add theProxyModule
//    (void)theProxyModule->SetupModule(&sCallbacks, &QTSSProxyModule_Main);
//    (void)AddModule(theProxyModule);
//#endif

}


/* 初始化所有的callback routines(共61个),定义参见QTSS_Private.h/cpp和QTSSCallback.h. 注意: 这些Callback routines的实质
   定义事实上还是在QTSSCallback.cpp中  */
void    QTSServer::InitCallbacks()
{
    sCallbacks.addr[kNewCallback] =                 (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_New;
    sCallbacks.addr[kDeleteCallback] =              (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Delete;
    sCallbacks.addr[kMillisecondsCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Milliseconds;
    sCallbacks.addr[kConvertToUnixTimeCallback] =   (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_ConvertToUnixTime;

    sCallbacks.addr[kAddRoleCallback] =             (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddRole;
    sCallbacks.addr[kCreateObjectTypeCallback] =    (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_CreateObjectType;
    sCallbacks.addr[kAddAttributeCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddAttribute;
    sCallbacks.addr[kIDForTagCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IDForAttr;
    sCallbacks.addr[kGetAttributePtrByIDCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetValuePtr;
    sCallbacks.addr[kGetAttributeByIDCallback] =    (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetValue;
    sCallbacks.addr[kSetAttributeByIDCallback] =    (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetValue;
    sCallbacks.addr[kCreateObjectValueCallback] =   (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_CreateObject;
    sCallbacks.addr[kGetNumValuesCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetNumValues;

    sCallbacks.addr[kWriteCallback] =               (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Write;
    sCallbacks.addr[kWriteVCallback] =              (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_WriteV;
    sCallbacks.addr[kFlushCallback] =               (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Flush;
    sCallbacks.addr[kReadCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Read;
    sCallbacks.addr[kSeekCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Seek;
    sCallbacks.addr[kAdviseCallback] =              (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Advise;

    sCallbacks.addr[kAddServiceCallback] =          (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddService;
    sCallbacks.addr[kIDForServiceCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IDForService;
    sCallbacks.addr[kDoServiceCallback] =           (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_DoService;

    sCallbacks.addr[kSendRTSPHeadersCallback] =     (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SendRTSPHeaders;
    sCallbacks.addr[kAppendRTSPHeadersCallback] =   (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AppendRTSPHeader;
    sCallbacks.addr[kSendStandardRTSPCallback] =    (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SendStandardRTSPResponse;

    sCallbacks.addr[kAddRTPStreamCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddRTPStream;
    sCallbacks.addr[kPlayCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Play;
    sCallbacks.addr[kPauseCallback] =               (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Pause;
    sCallbacks.addr[kTeardownCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Teardown;
    sCallbacks.addr[kRefreshTimeOutCallback] =      (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RefreshTimeOut;

    sCallbacks.addr[kRequestEventCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RequestEvent;
    sCallbacks.addr[kSetIdleTimerCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetIdleTimer;
    sCallbacks.addr[kSignalStreamCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SignalStream;

    sCallbacks.addr[kOpenFileObjectCallback] =      (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_OpenFileObject;
    sCallbacks.addr[kCloseFileObjectCallback] =     (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_CloseFileObject;

    sCallbacks.addr[kCreateSocketStreamCallback] =  (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_CreateStreamFromSocket;
    sCallbacks.addr[kDestroySocketStreamCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_DestroySocketStream;

    sCallbacks.addr[kAddStaticAttributeCallback] =          (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddStaticAttribute;
    sCallbacks.addr[kAddInstanceAttributeCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddInstanceAttribute;
    sCallbacks.addr[kRemoveInstanceAttributeCallback] =     (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RemoveInstanceAttribute;

    sCallbacks.addr[kGetAttrInfoByIndexCallback] =          (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByIndex;
    sCallbacks.addr[kGetAttrInfoByNameCallback] =           (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByName;
    sCallbacks.addr[kGetAttrInfoByIDCallback] =             (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByID;
    sCallbacks.addr[kGetNumAttributesCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetNumAttributes;


    sCallbacks.addr[kGetValueAsStringCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetValueAsString;
    sCallbacks.addr[kTypeToTypeStringCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_TypeToTypeString;
    sCallbacks.addr[kTypeStringToTypeCallback] =            (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_TypeStringToType;
    sCallbacks.addr[kStringToValueCallback] =               (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_StringToValue;
    sCallbacks.addr[kValueToStringCallback] =               (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_ValueToString;

    sCallbacks.addr[kRemoveValueCallback] =                 (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RemoveValue;

    sCallbacks.addr[kRequestGlobalLockCallback] =           (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RequestLockedCallback;
    sCallbacks.addr[kIsGlobalLockedCallback] =              (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IsGlobalLocked;
    sCallbacks.addr[kUnlockGlobalLock] =                    (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_UnlockGlobalLock;

    sCallbacks.addr[kAuthenticateCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Authenticate;
    sCallbacks.addr[kAuthorizeCallback] =                   (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Authorize;
    
    sCallbacks.addr[kLockObjectCallback] =                  (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_LockObject;
    sCallbacks.addr[kUnlockObjectCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_UnlockObject;
    sCallbacks.addr[kSetAttributePtrCallback] =             (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetValuePtr;
    
    sCallbacks.addr[kSetIntervalRoleTimerCallback] =        (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetIdleRoleTimer;
    
    sCallbacks.addr[kLockStdLibCallback] =                  (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_LockStdLib;
    sCallbacks.addr[kUnlockStdLibCallback] =                (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_UnlockStdLib;
}

/* 用指定目录路径和Win32方式加载模块,具体说就是,先从预设值获取module目录,在末尾加上"\\*",在该新路径上查找文件,
   找到后返回文件句柄,并创建该模块,否则记录错误进error log */
void QTSServer::LoadModules(QTSServerPrefs* inPrefs)
{
    // Fetch the name of the module directory and open it.
    OSCharArrayDeleter theModDirName(inPrefs->GetModuleDirectory());
    
    // POSIX version
	// opendir mallocs memory for DIR* so call closedir to free the allocated memory
    DIR* theDir = ::opendir(theModDirName.GetObject());
    if (theDir == NULL)
    {
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoModuleFolder, 0); 
        return;
    }
    
    while (true)
    {
        // Iterate over each file in the directory, attempting to construct
        // a module object from that file.
        
        struct dirent* theFile = ::readdir(theDir);
        if (theFile == NULL)
            break;
        
        this->CreateModule(theModDirName.GetObject(), theFile->d_name);
    }
	
	(void)::closedir(theDir);
	
#endif
}

/* 首先分析module路径的正确性,再创建新的文件路径,并新建QTSSModule实例并初始化一个module,用setup和add加载该module */
void    QTSServer::CreateModule(char* inModuleFolderPath, char* inModuleName) //create and initial Module
{
    // Ignore these silly directory names
    /* 一旦遇到不合法的模块名称,就无条件返回 */
	/* 注意入参inModuleFolderPath是module文件夹的路径,它的设置见QTSServer::LoadModules() */
    if (::strcmp(inModuleName, ".") == 0) //the directory names is .
        return;
    if (::strcmp(inModuleName, "..") == 0)
        return;
    if (::strlen(inModuleName) == 0)
        return;
    if (*inModuleName == '.')//若模块名以.开头,立即返回
        return; // Fix 2572248. Do not attempt to load '.' files as modules at all 

    //
    // Construct a full path to this module, denoted by theModPath.GetObject()
	/* 注意模块的完全路径的长度是module文件夹的路径长度+module名称的长度 */
    UInt32 totPathLen = ::strlen(inModuleFolderPath) + ::strlen(inModuleName); //full path
    OSCharArrayDeleter theModPath(NEW char[totPathLen + 4]);//theModPath definition, why add 4?
    ::strcpy(theModPath.GetObject(), inModuleFolderPath);
    ::strcat(theModPath.GetObject(), kPathDelimiterString);//i.e. "/"
    ::strcat(theModPath.GetObject(), inModuleName);/* 注意此处该theModPath.GetObject()末尾还空3个字符 */
            
    //
    // Construct a QTSSModule object, and attempt to initialize the module
	/* 按照标准的添加module的步骤加入一个模块:先给出module的名称和路径(该路径包含module名称)新建QTSSModule实例并初始化一个module */
    QTSSModule* theNewModule = NEW QTSSModule(inModuleName, theModPath.GetObject());
	/* 搭载静态(代码段)或动态库,将类似字符串"INFO: Module Loaded...QTSSRefMovieModule [dynamic]"写入错误日志 */
    QTSS_Error theErr = theNewModule->SetupModule(&sCallbacks);
    
	/* 若setup module出错,就将错误("错误的Module名称")记录进error log,并删除该新建QTSSModule实例 */
    if (theErr != QTSS_NoErr)
    {
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgBadModule, theErr,inModuleName);
        delete theNewModule; //fail to initialize so delete it
    }
    // 若将该Module加入Module queue出错,就记录错误信息("注册失败")进error log,并删除该新建QTSSModule实例
    // If the module was successfully initialized, add it to our module queue
    else if (!this->AddModule(theNewModule))
    {
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgRegFailed, theErr,inModuleName);                                     
        delete theNewModule;  //fail to add to module queue so delete it
    }
}

/* 调用分发函数注册得到ModuleName,新建Module预设值字典并初始化该Module,再更新服务器模块qtssSvrModuleObjects属性值,最后按指定索引加入模块数组中 */
Bool16 QTSServer::AddModule(QTSSModule* inModule)   // Add this module to the module queue
{
	/* 首先要确保该module被初始化,这就是说,分发函数指针fDispatchFunc一定要非空!但这已在QTSSModule::SetupModule()中得到保证 */
    Assert(inModule->IsInitialized());
    
    // Prepare to invoke the module's Register role. Setup the Register param block
	/* 设置module的状态信息 */
    QTSS_ModuleState theModuleState;

    theModuleState.curModule = inModule;
	/* 现在使用注册角色 */
    theModuleState.curRole = QTSS_Register_Role;
	/* 现在还没有Task */
    theModuleState.curTask = NULL;
	/* 设置主线程私有数据 */
    OSThread::SetMainThreadData(&theModuleState);
    
    // Currently we do nothing with the module name
	/* 设置module的register role参数,清空第一个分量以接受即将到来的值 */
    QTSS_RoleParams theRegParams;
    theRegParams.regParams.outModuleName[0] = 0;
    
    // If the module returns an error from the QTSS_Register role, don't put it anywhere
	/* 若调用分发函数(用QTSSModule::CallDispatch包装)注册QTSS_Register_Role出错,就返回 */
    if (inModule->CallDispatch(QTSS_Register_Role, &theRegParams) != QTSS_NoErr) 
        return false;
        
    OSThread::SetMainThreadData(NULL);
    
    //
    // Update the module name to reflect what was returned from the register role
    theRegParams.regParams.outModuleName[QTSS_MAX_MODULE_NAME_LENGTH - 1] = 0; //the last component of the array is set 0
	/* 假如注册角色返回后,得到的module name非空,就用QTSSDictionary::SetValue()将其设置为qtssModName的值 */
    if (theRegParams.regParams.outModuleName[0] != 0) 
        inModule->SetValue(qtssModName, 0, theRegParams.regParams.outModuleName, ::strlen(theRegParams.regParams.outModuleName), false);

    //
    // Give the module object a prefs dictionary. Instance attributes are allowed for these objects.
	/* 新建QTSSPrefs,并用kModulePrefsDictIndex字典初始化,设置module的预设值,允许实例属性 */
    QTSSPrefs* thePrefs = NEW QTSSPrefs( sPrefsSource, inModule->GetValue(qtssModName), QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModulePrefsDictIndex), true);
    thePrefs->RereadPreferences();//读取Module预设值
    inModule->SetPrefsDict(thePrefs);//设置为Module的预设值
        
    //
    // Add this module to the array of module (dictionaries)
	/* 获取该服务器对象有多少个Module ? */
    UInt32 theNumModules = this->GetNumValues(qtssSvrModuleObjects);
	/* 在module数组末尾加上入参指定的modul,更新模块qtssSvrModuleObjects属性值 */
    QTSS_Error theErr = this->SetValue(qtssSvrModuleObjects, theNumModules, &inModule, sizeof(QTSSModule*), QTSSDictionary::kDontObeyReadOnly);
    Assert(theErr == QTSS_NoErr);
    
    //
    // Add this module to the module queue
	/* 获取该module的队列元,并加入到该Module队列中 */
    sModuleQueue.EnQueue(inModule->GetQueueElem());
    
    return true;
}

/* 利用OSQueue队列工具创建所有角色Role的静态module数组和统计该角色下的module个数 */
void QTSServer::BuildModuleRoleArrays()
{
	/* 参见OSQueue.h/cpp, 阅读本函数时务必要好好读懂这个文件 */
    OSQueueIter theIter(&sModuleQueue);
    QTSSModule* theModule = NULL;
        
    // Make sure these variables are cleaned out in case they've already been invited.
    /* 删去所有role对应的二维指针(QTSSModule**)数组sModuleArray,函数定义见下面 */
    DestroyModuleRoleArrays();

    // Loop through all the roles of all the modules, recording the number of
    // modules in each role, and also recording which modules are doing what.
    /* 对所有的role(定义见QTSSModule.h)作循环 */
    for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++)
    {
		/* 用于统计指定role的module数 */
        sNumModulesInRole[x] = 0; //the x-th component, x denotes the Number of Roles

		/* 对本类中的static模块队列从头至尾循环,统计每个role引用了多少Modules? */
        for (theIter.Reset(); !theIter.IsDone(); theIter.Next()) // note the repression of loop
        {
			/* 先获取当前队列元所在的module */
            theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject();
			/* 若该模块注册了该角色,将sNumModulesInRole的第x个分量加1 */
            if (theModule->RunsInRole(x))
                sNumModulesInRole[x] += 1;
        }
     
		/* 针对该role的module数目,创建module数组 */
        if (sNumModulesInRole[x] > 0) 
        {
			/* 用于索引同一role的模块数组元素 */
            UInt32 moduleIndex = 0;

			/* 新建静态module数组sModuleArray,注意数组元素是QTSSModule* */
            sModuleArray[x] = new QTSSModule*[sNumModulesInRole[x] + 1]; //sModuleArray[x] (pointer array) definition
            /* 对每个模块队列从头至尾循环,来创建一个一维数组 */
			for (theIter.Reset(); !theIter.IsDone(); theIter.Next())
            {
				/* 先获取当前队列元所在的module */
                theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject(); 
                if (theModule->RunsInRole(x))
                {
					/* sModuleArray[x]是个模块数组的分量(拥有共同的role) */
                    sModuleArray[x][moduleIndex] = theModule; //2-dimmion array
                    moduleIndex++;
                }
            }
        }
    }
}

/* 删去所有role对应的二维指针(QTSSModule**)数组sModuleArray */
void QTSServer::DestroyModuleRoleArrays()
{
	/* 对所有的role循环 */
    for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++)
    {
		/* 将每个role对应的module数设置为0 */
        sNumModulesInRole[x] = 0; 
		/* 删去该角色对应的Module数组(二维指针数组QTSSModule**) */
        if (sModuleArray[x] != NULL) 
            delete [] sModuleArray[x];
        sModuleArray[x] = NULL; 
    }
}
 
/* 对注册该QTSS_Initialize_Role的所有modules,循环调用分发函数去完成初始化任务,若调用分发函数出错,就利用QTSSModuleUtils::LogError()将错误原因和module name记录进error日志;
   设置服务器默认处理的RTSP method是OPTIONS,并建立服务器所能处理的RTSP方法的一个字符串sPublicHeaderStr,即"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n" */
void QTSServer::DoInitRole()
{
	/* 配置initialize role参数,就差module分量没有配置(参见下面设置),see QTSS.h */
    QTSS_RoleParams theInitParams;
    theInitParams.initParams.inServer =         this;
    theInitParams.initParams.inPrefs =          fSrvrPrefs;
    theInitParams.initParams.inMessages =       fSrvrMessages;
    theInitParams.initParams.inErrorLogStream = &sErrorLogStream;
     
	/* 配置QTSS_ModuleState */
    QTSS_ModuleState theModuleState;
    theModuleState.curRole = QTSS_Initialize_Role;/* 当前Module的role是QTSS_Initialize_Role */
    theModuleState.curTask = NULL;/* 当前Module并没有Task */
    OSThread::SetMainThreadData(&theModuleState); /* 初始化主线程的私有数据 */

    //
    // Add the OPTIONS method as the one method the server handles by default (it handles
    // it internally). Modules that handle other RTSP methods(see QTSSRTSPProtocol.h) will add 
	/* 设置服务器默认处理的RTSP method是OPTIONS,Modules会用初始化角色在该属性后面加入其它RTSP methods */
    QTSS_RTSPMethod theOptionsMethod = qtssOptionsMethod;
    (void)this->SetValue(qtssSvrHandledMethods, 0, &theOptionsMethod, sizeof(theOptionsMethod));


	// For now just disable the SetParameter to be compatible with Real.  It should really be removed only for clients 
	//	that have problems with their SetParameter implementations like (Real Players).
	// At the moment it isn't necessary to add the option.
	/* 若与Real Player不compatible,就移去该选项 */
	//   QTSS_RTSPMethod	theSetParameterMethod = qtssSetParameterMethod;
	//    (void)this->SetValue(qtssSvrHandledMethods, 0, &theSetParameterMethod, sizeof(theSetParameterMethod));
	
	/* 对注册该QTSS_Initialize_Role的所有modules,循环调用分发函数去完成任务,注意QTSS_Initialize_Role与QTSSModule::kInitializeRole的关系
	   参见QTSSModule::AddRole() */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kInitializeRole); x++)
    {
		/* 得到指定索引和注册QTSS_Initialize_Role的module */
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kInitializeRole, x);
		/* 用该模块去设置初始化角色的module */
        theInitParams.initParams.inModule = theModule;
		/* 用该模块去设置Module state中的module */
        theModuleState.curModule = theModule;
		/* 调用该module的分发函数 */
        QTSS_Error theErr = theModule->CallDispatch(QTSS_Initialize_Role, &theInitParams);

        if (theErr != QTSS_NoErr)
        {
            // If the module reports an error when initializing itself,
            // delete the module and pretend it was never there.
			/* 若调用分发函数出错,就利用QTSSModuleUtils::LogError()将错误原因和module name记录进error日志 */
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgInitFailed, theErr,theModule->GetValue(qtssModName)->Ptr);                                        
            
			/* 从module队列中移去该module */
            sModuleQueue.Remove(theModule->GetQueueElem());
            delete theModule;//删去该module实例
        }
    }

	/* 建立公共头"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n",参见下面 */
    this->SetupPublicHeader();

    OSThread::SetMainThreadData(NULL); /* 清空主线程的私有数据 */
}

/* 建立服务器所能处理的RTSP方法(多值的属性)的一个字符串sPublicHeaderStr和sPublicHeaderFormatter,即"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n" */
void QTSServer::SetupPublicHeader()
{
    //
    // After the Init role, all the modules have reported the methods that they handle.
    // So, we can prune this attribute for duplicates, and construct a string to use in the
    // Public: header of the OPTIONS response
    QTSS_RTSPMethod* theMethod = NULL;
    UInt32 theLen = 0;

	/* 新建一个12个分量的bool数组,记录下每个module的RTSP方法,初始化为0,见QTSSRTSPProtocol.h */
    Bool16 theUniqueMethods[qtssNumMethods + 1]; //12
    ::memset(theUniqueMethods, 0, sizeof(theUniqueMethods)); 

	/* 利用QTSSDictionary::GetValuePtr()循环检查服务器中的方法qtssSvrHandledMethods(是个多值的属性),给上述数组赋值 */
    for (UInt32 y = 0; this->GetValuePtr(qtssSvrHandledMethods, y, (void**)&theMethod, &theLen) == QTSS_NoErr; y++)
		/* 注意在QTSSRTSPProtocol.h中,每个RTSP method对应一个index */
        theUniqueMethods[*theMethod] = true; 

    // Rewrite the qtssSvrHandledMethods, eliminating any duplicates that modules may have introduced
	/* 更新服务器的处理方法,并对修改过的次数计数 */
    UInt32 uniqueMethodCount = 0;
    for (QTSS_RTSPMethod z = 0; z < qtssNumMethods; z++)
    {
        if (theUniqueMethods[z])
            this->SetValue(qtssSvrHandledMethods, uniqueMethodCount++, &z, sizeof(QTSS_RTSPMethod));
    }
	/* 更新服务器真正可以处理的RTSP方法数 */
    this->SetNumValues(qtssSvrHandledMethods, uniqueMethodCount);
    
    // Format a text string for the Public header
    ResizeableStringFormatter theFormatter(NULL, 0);//注意此时还未分配缓存

	/* 通过循环得到服务器所能处理的RTSP方法(多值的属性)的一个字符串 */
    for (UInt32 a = 0; this->GetValuePtr(qtssSvrHandledMethods, a, (void**)&theMethod, &theLen) == QTSS_NoErr; a++)
    {
        sPublicHeaderFormatter.Put(RTSPProtocol::GetMethodString(*theMethod));
        sPublicHeaderFormatter.Put(", ");
    }
    sPublicHeaderStr.Ptr = sPublicHeaderFormatter.GetBufPtr();
    sPublicHeaderStr.Len = sPublicHeaderFormatter.GetBytesWritten() - 2; //trunc the last ", "
}

/**************  以下是RTSPListenerSocket类的成员函数 ***************************/

/* 当侦听的TCP socket接入一个点播后,创建一个RTSP session实例(其有个TCPSocket成员),返回这个RTSPSession实例和其socket描述符,并根据最大连接数控制接入accept点播会话的节奏 */
/* 子类重载函数,used in TCPListenerSocket::ProcessEvent() */
Task*   RTSPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{
	/* 确保TCP socket的两级指针**非空,由父类这可以办到:TCPSocket* theSocket = NULL;用&theSocket作入参 */
    Assert(outSocket != NULL);
    
    // when the server is behing a round robin DNS(轮询域名), the client needs to known the IP address of the server
    // so that it can direct the "POST" half of the connection to the same machine when tunnelling RTSP thru HTTP
	// this pref tells the server to report its IP address in the reply to the HTTP GET request when tunneling RTSP through HTTP
	/* 由服务器预设值,获取是否对Client报告流服务器的ip地址? */
    Bool16  doReportHTTPConnectionAddress = QTSServerInterface::GetServer()->GetPrefs()->GetDoReportHTTPConnectionAddress();
    
	/* 创建一个新点播会话RTSPSession任务并获取socket */
    RTSPSession* theTask = NEW RTSPSession(doReportHTTPConnectionAddress);
	/* 获得该点播会话的TCPSocket的描述符指针 */
    *outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.注意它的配对设置在TCPListenerSocket::ProcessEvent()

	/* 连接数控制,当超过最大连接数(streamingserver.xml设置为1000)时,让侦听的TCPListenerSocket在accept()之间休眠1秒再accept().
	参见TCPListenerSocket::ProcessEvent() */
    if (this->OverMaxConnections(0))
		/* 就是设置TCPListenerSocket::fSleepBetweenAccepts = true */
        this->SlowDown();
    else
		/* 就是设置TCPListenerSocket::fSleepBetweenAccepts = false */
        this->RunNormal();
        
    return theTask;
}

/* 从服务器获取最大连接数的预设值,来判断当前的RTP或RTSP连接数是否超限,返回Boolean结果 */
Bool16 RTSPListenerSocket::OverMaxConnections(UInt32 buffer)
{
	/* 从服务器获取最大连接数的值 */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();
	/* 是否超过最大连接数 */
    Bool16 overLimit = false;
    
    if (maxConns > -1) // limit connections
    { 
        maxConns += buffer;
		/* 只要RTP session数或RTSP session数超过最大连接限制,就是超限 */
        if  ( (theServer->GetNumRTPSessions() > (UInt32) maxConns) 
              ||
              ( theServer->GetNumRTSPSessions() + theServer->GetNumRTSPHTTPSessions() > (UInt32) maxConns ) 
            )
        {
            overLimit = true;          
        }
    } 
	/* 返回最后是否超限的结果 */
    return overLimit;
     
}

/**************  以上是RTSPListenerSocket类的成员函数 ***************************/

/***********************  以下是RTPSocketPool类的成员函数 ***************************/

/* 注意RTPSocketPool是QTSServer类的friend class,该函数作用:从服务器获取RTCP任务实例指针并相应创建一个UDPSocketPair实例 */
UDPSocketPair*  RTPSocketPool::ConstructUDPSocketPair()
{
	/* 从QTSServerInterface类获取RTCP任务指针(注意从父类强制转换为子类) */
    Task* theTask = ((QTSServer*)QTSServerInterface::GetServer())->fRTCPTask;
    
    //construct a pair of UDP sockets, the lower one for RTP data (outgoing only, no demuxer
    //necessary), and one for RTCP data (incoming, so definitely need a demuxer).
    //These are nonblocking sockets that DON'T receive events (we are going to poll for data)
	// They do receive events - we don't poll from them anymore
	/* 逐步创建UDPSocketPair,端口小的用作向外发送RTP数据,不需复用器;端口大的用作向内接收RTCP包,一定需要复用器,都是非阻塞类型的. 参见UDPSocketPool.h和Socket.h的构造函数 */
    return NEW UDPSocketPair(  NEW UDPSocket(theTask, Socket::kNonBlockingSocketType),
                               NEW UDPSocket(theTask, UDPSocket::kWantsDemuxer | Socket::kNonBlockingSocketType)); //会创建UDPDemuxer实例
}

/* 删去入参指定(比如,上面创建)的一个UDPSocketPair实例 */
void RTPSocketPool::DestructUDPSocketPair(UDPSocketPair* inPair)
{
	/* 先删去单个的UDPSocket,再删去UDPSocketPair.注意这里的delete和上面RTPSocketPool::ConstructUDPSocketPair()中的NEW是一一对应的 */
    delete inPair->GetSocketA();
    delete inPair->GetSocketB();
    delete inPair;
}

/* 依次设置UDP socket对的发送buffer或接收缓存大小:第一个设置固定,第二个设置要灵活,即:设置UDPSocketPair中向外发送RTP数据的socket buffer大小为256K字节,
从预设值开始,以减半策略来动态调整RTCP socket接受buffer的大小 */
void RTPSocketPool::SetUDPSocketOptions(UDPSocketPair* inPair)
{
    // Apparently the socket buffer size matters even though this is UDP and being
    // used for sending... on UNIX typically the socket buffer size doesn't matter because the
    // packet goes right down to the driver. On Win32 and linux, unless this is really big, we get packet loss.
	/* 设置UDPSocketPair中向外发送RTP数据的socket buffer大小为256K字节,亦见TCPListenerSocket::Initialize() */
    inPair->GetSocketA()->SetSocketBufSize(256 * 1024);

    // Always set the Rcv buf size for the RTCP sockets. This is important because the
    // server is going to be getting many many acks.
	/* 获取服务器预设的RTCP socket的接收缓冲大小,768K字节  */
    UInt32 theRcvBufSize = QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK();

    // 当有时预设值对操作系统过大时,会持续减半调整,直到系统满意为止
    // In case the rcv buf size is too big for the system, retry, dividing the requested size by 2.
    // Until it works, or until some minimum value is reached.
    OS_Error theErr = EINVAL;
    while ((theErr != OS_NoErr) && (theRcvBufSize > 32))
    {
		/* 从预设值开始,以减半策略来动态调整RTCP socket接受buffer的大小 */
        theErr = inPair->GetSocketB()->SetSocketRcvBufSize(theRcvBufSize * 1024);
        if (theErr != OS_NoErr)
			/* 使缓存大小减为原来的一半 */
            theRcvBufSize >>= 1;
    }

    // 当RTCP socket接受buffer大小的预设值被动态更改后,记录日志
    // Report an error if we couldn't set the socket buffer size the user requested
    if (theRcvBufSize != QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK())
    {
		/* 设置指定形式的theRcvBufSize */
        char theRcvBufSizeStr[20];
        qtss_sprintf(theRcvBufSizeStr, "%lu", theRcvBufSize);
        
        // For now, do not log an error, though we should enable this in the future.
       QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSockBufSizesTooLarge,0, theRcvBufSizeStr);
    }
}

/**************************  以上是RTPSocketPool类的成员函数 ***************************/


/* 读取服务器,各个module,以及注册QTSS_RereadPrefs_Role的module的预设值,更新服务器的侦听,端口和IP地址 */
QTSS_Error QTSServer::RereadPrefsService(QTSS_ServiceFunctionArgsPtr /*inArgs*/)
{
    // This function can only be called safely when the server is completely running.
    // Ensuring this is a bit complicated because of preemption. Here's how it's done...
    // 获取服务器实例指针
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    // This is to make sure this function isn't being called before the server is completely started up.
    // 假如服务器未完全启动,或者不在允许状态,立即返回
    if ((theServer == NULL) || (theServer->GetServerState() != qtssRunningState))
        return QTSS_OutOfState;

    // Because the server must have started up, and because this object always stays
    // around (until the process dies), we can now safely get this object.
	// 获取服务器预设值对象指针(此时服务器已经完全启动,我们总能安全得到该预设值对象)
    QTSServerPrefs* thePrefs = theServer->GetPrefs();
    
    // Grab the prefs mutex. We want to make sure that calls to RereadPrefsService
    // are serialized. This also prevents the server from shutting down while in
    // this function, because the QTSServer destructor grabs this mutex as well.
	// 对服务器预设值对象加锁,这也能防止在执行RereadPrefsService()时,服务器关闭,因为QTSServer析构函数也会用到服务器预设值对象锁
    OSMutexLocker locker(thePrefs->GetMutex());
    
    // Finally, check the server state again. The state may have changed
    // to qtssShuttingDownState or qtssFatalErrorState in this time, though
    // at this point we have the prefs mutex, so we are guarenteed that the
    // server can't actually shut down anymore
	/* 尽管我们已经抓住预设值对象锁,但是还要再次确保服务器当前状态是qtssRunningState,不是立即退出 */
    if (theServer->GetServerState() != qtssRunningState)
        return QTSS_OutOfState;
    
    // Ok, we're ready to reread preferences now.
    // Reread preferences
	/* 解析xml配置文件中各Tag的合法性 */
    sPrefsSource->Parse();
	/* 先遍历服务器的属性列表,逐个用预设值文件设置各指定的属性,若预设值出错,就用默认值代替,然后设置认证格式,
	RTP/RTCP包头打印选项,日志写关闭,最后将调整后的预设值内容写入xml预设值文件 */
    thePrefs->RereadServerPreferences(true);
    
    {
        // 获取服务器对象的互斥锁,Update listeners, ports, and IP addrs.
        OSMutexLocker locker(theServer->GetServerObjectMutex());
		// 从预设值获取RTSP的ip地址,设置第一个绑定的ip为默认的点播IP并配置它相应的DNS域名和ip address string属性
        (void)((QTSServer*)theServer)->SetDefaultIPAddr();
		// 根据预设值,创建侦听Socket数组,并配置更新fNumListeners和fListeners,第一个入参true表示,现在开始侦听并向TaskThread请求读事件
        (void)((QTSServer*)theServer)->CreateListeners(true, thePrefs, 0);
    }
    
    // Delete all the streams
	/* 依次读入服务器中的各个模块的预设值 */
    QTSSModule** theModule = NULL;
    UInt32 theLen = 0;

    // Go through each module's prefs object and have those reread as well
    for (int y = 0; QTSServerInterface::GetServer()->GetValuePtr(qtssSvrModuleObjects, y, (void**)&theModule, &theLen) == QTSS_NoErr; y++)
    {
        Assert(theModule != NULL);
        Assert(theLen == sizeof(QTSSModule*));
        /* 读入每个module的预设值 */
        (*theModule)->GetPrefsDict()->RereadPreferences();

#if DEBUG
        theModule = NULL;
        theLen = 0;
#endif
    }
    
    // Now that we are done rereading the prefs, invoke all modules in the RereadPrefs
    // role so they can update their internal prefs caches.
	/* 使读预设值角色的module依次调用QTSS_RereadPrefs_Role去更新内部的预设值缓存 */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRereadPrefsRole); x++)
    {
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRereadPrefsRole, x);
        (void)theModule->CallDispatch(QTSS_RereadPrefs_Role, NULL);
    }
    return QTSS_NoErr;
}


