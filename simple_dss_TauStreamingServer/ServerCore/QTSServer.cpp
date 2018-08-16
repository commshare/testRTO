
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


#include "QTSServer.h"  //������Ӧ��ͷ�ļ�
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
/* DSS����Ӧ��ģ�� */
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
		/* ����Ψһ�Ĺ������Ǵ�TCP�˿ڻ�ȡ�Ự����,�μ�TCPListenerSocket::GetSessionTask() */
        virtual Task*   GetSessionTask(TCPSocket** outSocket);
        
        //check whether the Listener should be idling
		/* ����Ƿ񳬹����������? */
        Bool16 OverMaxConnections(UInt32 buffer);

};

/* �����UDPSocketPool�еļ���protected��������Դ�붨�� */
/* ע�����ļ���������UDPSocketPool�б�����,�μ�UDPSocketPool::GetUDPSocketPair() */
class RTPSocketPool : public UDPSocketPool //RTP responds to UDP as a common approach
{
    public:
    
        // Pool of UDP sockets for use by the RTP server
        
        RTPSocketPool() {}
        ~RTPSocketPool() {}
        
		/* ����/����UDP Socket �� */
        virtual UDPSocketPair*  ConstructUDPSocketPair();
        virtual void            DestructUDPSocketPair(UDPSocketPair* inPair);

		/* ����UDP Socket pair��,���ⷢ��RTP���ݵ�socket buffer��СΪ256K�ֽ�,��Ԥ��ֵ��ʼ,�Լ����������̬����RTCP socket����buffer�Ĵ�С */
        virtual void            SetUDPSocketOptions(UDPSocketPair* inPair);
};



char*           QTSServer::sPortPrefString = "rtsp_port"; 
QTSS_Callbacks  QTSServer::sCallbacks;

/* used in QTSServer::Initialize(),�ڸú����л���������� */
XMLPrefsParser* QTSServer::sPrefsSource = NULL;
//PrefsSource*    QTSServer::sMessagesSource = NULL;


QTSServer::~QTSServer()  //destructor
{
    // Grab the server mutex. This is to make sure all gets & set values on this
    // object complete before we start deleting stuff
	/* �õ�������������,refer to QTSServerInterface.h */
    OSMutexLocker serverlocker(this->GetServerObjectMutex());
    
    // Grab the prefs mutex. This is to make sure we can't reread prefs
    // WHILE shutting down, which would cause some weirdness for QTSS API
    // (some modules could get QTSS_RereadPrefs_Role after QTSS_Shutdown, which would be bad)
	/* �õ�������Ԥ��ֵ���󻥳���,ʹ�øû�����,��ֹģ��ر�ʱ��Ԥ��Ԥ��ֵ,�μ�QTSServer::RereadPrefsService() */
    OSMutexLocker locker(this->GetPrefs()->GetMutex());

	/* ����module state���������߳�˽������ */
    QTSS_ModuleState theModuleState;
    theModuleState.curRole = QTSS_Shutdown_Role;
    theModuleState.curTask = NULL;
    OSThread::SetMainThreadData(&theModuleState);

	/* ����ע��رս�ɫ�����ģ�������Դ�ͷ����� */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kShutdownRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kShutdownRole, x)->CallDispatch(QTSS_Shutdown_Role, NULL);

	/* ������߳�˽������ */
    OSThread::SetMainThreadData(NULL);
}

/* used in StartServer() in RunServer.cpp */
Bool16 QTSServer::Initialize(XMLPrefsParser* inPrefsSource,/* PrefsSource* inMessagesSource,*/ UInt16 inPortOverride, Bool16 createListeners)
{
    static const UInt32 kRTPSessionMapSize = 577;//�˴������޸İ�?
	/* һ��ʼ��FatalErrorState,��ȷ���ú�͸���ΪStartingUpState */
    fServerState = qtssFatalErrorState;
	/* ����in parameters��member Variables */
    sPrefsSource = inPrefsSource;
    //sMessagesSource = inMessagesSource;
	/* initialize QTSS API callback routines,����Դ�붨��,�μ�����ĸú������� */
    this->InitCallbacks();

    //
    // DICTIONARY INITIALIZATION
    /* ��ʼ�����е�Dictionary����� */

	/* ����QTSSDictionaryMap::SetAttribute()ѭ������6��ģ�������������ϢQTSSAttrInfoDict::AttrInfo */
    QTSSModule::Initialize();          //kModuleDictIndex
    QTSServerPrefs::Initialize();      //kPrefsDictIndex
    QTSSMessages::Initialize();        //kTextMessagesDictIndex
	QTSSFile::Initialize();            //kFileDictIndex
	QTSSUserProfile::Initialize();     //kQTSSUserProfileDictIndex
    RTSPRequestInterface::Initialize();//kRTSPRequestDictIndex & kRTSPHeaderDictIndex
    RTSPSessionInterface::Initialize();//kRTSPSessionDictIndex
    RTPSessionInterface::Initialize(); //kClientSessionDictIndex
    RTPStream::Initialize();           //kRTPStreamDictIndex
    
	RTSPSession::Initialize();         //����http proxy tunnel map,����sHTTPResponseHeaderBuf/sHTTPResponseNoServerHeaderBuf����
    
    //
    //RTP\RTSP STUB SERVER INITIALIZATION
    //
    // Construct stub versions(����汾) of the prefs and messages dictionaries. We need
    // both of these to initialize the server, but they have to be stubs because
    // their QTSSDictionaryMaps will presumably be modified when modules get loaded.
    
	/* �½�������γ�ʼ�����ݳ�Ա */
    fSrvrPrefs = new QTSServerPrefs(inPrefsSource, false); // First time, don't write changes to the prefs file
    fSrvrMessages = new QTSSMessages(/*inMessagesSource*/); //������Ψһ��TextMessage����
    QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());//ʹ�������½�ʵ�������ʼ��QTSSModuleUtils��̬�����,���ڼ�¼Error Log

    //
    // SETUP ASSERT BEHAVIOR ����Assert��Ϊ
    //
    // Depending on the server preference, we will either break when we hit an
    // assert, or log the assert to the error log
    if (!fSrvrPrefs->ShouldServerBreakOnAssert())
        SetAssertLogger(this->GetErrorLogStream());// the error log stream is our assert logger
        
    //
    // CREATE GLOBAL OBJECTS
	/* �ڻ���UDPSocketPool�϶�̬��������RTPSocketPool,�Ӷ����㽫��̬������UDPSocketPair����UDPSocketPool,����ͳһ��ά���͹��� */
    fSocketPool = new RTPSocketPool();
	/* ������СΪ577��RTP session map(hash table),��������Ψһ�ĻỰID����ʶ�͹������е�RTPSession��RTSPSession */
    fRTPMap = new OSRefTable(kRTPSessionMapSize);//577

    //
    // Load ERROR LOG module only. This is good in case there is a startup error.
    /* ��̬������error log module, �����������˿�ʱ����Ҫ������¼������־ */
    QTSSModule* theLoggingModule = new QTSSModule("QTSSErrorLogModule");
	/* �Ӵ��̼���dll/.so�õ��͵�������ں���QTSS_MainEntryPointPtr(�˴���&QTSSErrorLogModule_Main),�Ӷ��õ��ַ�����ָ�벢����ΪfDispatchFunc */
    (void)theLoggingModule->SetupModule(&sCallbacks, &QTSSErrorLogModule_Main);
	/* ����ģ����뾲̬sModuleQueue */
    (void)AddModule(theLoggingModule);
    this->BuildModuleRoleArrays(); //���þ�̬ģ���ɫ����sModuleArray

    //
    // DEFAULT IP ADDRESS & DNS NAME
	/* ��Ԥ��ֵ��ȡRTSP��ip��ַ,���õ�һ���󶨵�ipΪĬ�ϵĵ㲥IP����������Ӧ��DNS������ip address string����,��Ĭ�ϵ�������ַΪ�վͽ�����ǽ�error log */
    if (!this->SetDefaultIPAddr())
        return false;

    //
    // STARTUP TIME - record it
	/* ��¼Server������ʱ���GMTƫ�� */
    fStartupTime_UnixMilli = OS::Milliseconds();
    fGMTOffset = OS::GetGMTOffset();//��ǰʱ����GMT���8Сʱ
        
    //
    // BEGIN LISTENING--erro log
    if (createListeners) //note the difference:in param createListeners (boolean type in parameters) and CreateListeners() 
    {
		/* ע���һ��������ʾ�����Ѿ���ʼ����,����δ��TaskThread������¼�(����QTSServer::StartTasks()������),QTSServer.CreateListeners()�ķ���ֵ�� fNumListeners>0 ? */
		// ��̬����RTSP Listeners socket����,�����ø������ݳ�ԱfNumListeners��fListeners
		/****************  NOTE: Important !! **********************************/
        if ( !this->CreateListeners(false, fSrvrPrefs, inPortOverride) ) 
		/****************  NOTE: Important !! **********************************/
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSomePortsFailed, 0);//��¼"�˿�ʧ��"������־��Error Log��
    }
    
	/* ��������������listeners NumberΪ0�ͱ���д��error log,��ʵ�ⲽ,�����Ѿ������� */
    if ( fNumListeners == 0 )
    {   
		if (createListeners)
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoPortsSucceeded, 0);//��¼������־��Error Log��
        return false;
    }

    /* ��ʱ����Server state */
    fServerState = qtssStartingUpState;
    return true;
}

/* ���غͳ�ʼ�����е�Module */
void QTSServer::InitModules(QTSS_ServerState inEndState)
{
    //
    // LOAD AND INITIALIZE ALL MODULES
        
    // temporarily set the verbosity on missing prefs when starting up to debug level
    // This keeps all the pref messages being written to the config file from being logged.
    // don't exit until the verbosity level is reset back to the initial prefs.
     
    LoadModules(fSrvrPrefs);
    LoadCompiledInModules();
    this->BuildModuleRoleArrays();//���þ�̬sModuleArray

	/* ����ʼ������ģ��ʱ,����Server���󼶱�ΪWarning,����ٸĻ��� */
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
    QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());//ʹ�������½�ʵ�������ʼ��QTSSModuleUtils��̬�����,���ڼ�¼Error Log

	/* set dictionary attr and write into server internally */
	// �������½��Ķ��������÷�����Ԥ��ֵ����Ϣ����
    this->SetVal(qtssSvrMessages, &fSrvrMessages, sizeof(fSrvrMessages));
    this->SetVal(qtssSvrPreferences, &fSrvrPrefs, sizeof(fSrvrPrefs));

    // �ڵ�ǰ�����������м����Ԥ��ֵ���������
    // ADD REREAD PREFERENCES SERVICE
    (void)QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
         AddAttribute(QTSS_REREAD_PREFS_SERVICE, (QTSS_AttrFunctionPtr)QTSServer::RereadPrefsService, qtssAttrDataTypeUnknown, qtssAttrModeRead);

    //
    // INVOKE INITIALIZE ROLE
	/* ��ʼ��module */
    this->DoInitRole();

	/* �����inEndState���ݸ���Ա���� */
    if (fServerState != qtssFatalErrorState)
        fServerState = inEndState; // Server is done starting up!   

    /* �Ļ�Server pref���󼶱�,serverLevel���������˱��� */
    fSrvrPrefs->SetErrorLogVerbosity(serverLevel); // reset the server's verbosity back to the original prefs level. see also line 258.
}

/* ����RTCP Task������RTPSocketPool�����е�UDPSocketPair�Ͽ��ܵ�����RTCP������,RTPStatsUpdaterTask,
��ÿ��TCPListenerSocket,�������߳�������¼� */
void QTSServer::StartTasks()
{
	/* ����RTP/RTCP Task pair */
    fRTCPTask = new RTCPTask();
	/* ����RTP stats update Task */
    fStatsTask = new RTPStatsUpdaterTask();

    //
    // Start listening
	/* ��ÿ��TCPListenerSocket,����ָ�������Ƿ���ָ���Ķ������¼�����? */
    for (UInt32 x = 0; x < fNumListeners; x++)
        fListeners[x]->RequestEvent(EV_RE); // what is EV_RE? event_read
}

/* ��Ԥ��ֵ��ȡRTSP��ip��ַ,���õ�һ���󶨵�ipΪĬ�ϵĵ㲥IP����������Ӧ��DNS������ip address string����,��Ĭ�ϵ�������ַΪ�վͽ�����ǽ�error log */
Bool16 QTSServer::SetDefaultIPAddr()
{
    //check to make sure there is an available ip interface
	/* ����û���ҵ�ip address,�ͽ�����qtssMsgNotConfiguredForIP��¼��error log,������  */
    if (SocketUtils::GetNumIPAddrs() == 0) 
    {
        QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgNotConfiguredForIP, 0);
        return false;
    }

    //find out what our default IP addr is & dns name
	/* ��Ԥ��ֵ��ȡRTSP��ip��ַ�ַ��� */
    UInt32 theNumAddrs = 0;
    UInt32* theIPAddrs = this->GetRTSPIPAddrs(fSrvrPrefs, &theNumAddrs);
	/* ����fDefaultIPAddr */
    if (theNumAddrs == 1) //���ʾû�����ð󶨵�RTSP ip
        fDefaultIPAddr = SocketUtils::GetIPAddr(0);
    else
        fDefaultIPAddr = theIPAddrs[0];//���õ�һ���󶨵�ipΪĬ�ϵĵ㲥IP

    delete [] theIPAddrs;//ɾ����QTSServer::GetRTSPIPAddrs()���Ѿ������ip��ַ����
    
    /* ͨ��ѭ������default IP address,����Ӧ������Ĭ�ϵ�DNS������ip address�ַ������� */
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

	/* Ĭ�ϵ�������ַΪ�վͽ�����ǽ�error log */
    if (this->GetValue(qtssSvrDefaultDNSName)->Ptr == NULL) //notified by assert()
    {
        //If we've gotten here, what has probably happened is the IP address (explicitly
        //entered as a preference) doesn't exist
        QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgDefaultRTSPAddrUnavail, 0); //record the errorlog
        return false;   
    }
    return true;
}               

// ע��: ����������Ҫ���þ������ø���fNumListeners��fListeners
/* ���ݷ�����Ԥ��ֵ���õ�IP��ַ����Ͷ˿�����,�����Ƿ�ʹ�ù��ö˿�,���õ��˿�׷�ٽṹ������,�ٶ�̬����RTSP Listeners socket����newListenerArray,
�ڿ��ǵ�ǰʹ�õ�fNumListeners��fListeners��,��newListenerArray��������,�������ո�fListeners��С�Ƿ�>0��boolֵ */
Bool16 QTSServer::CreateListeners(Bool16 startListeningNow, QTSServerPrefs* inPrefs, UInt16 inPortOverride)
{
	/* �˿�׷�ٽṹ�嶨�� */
    struct PortTracking 
    {
        PortTracking() : fPort(0), fIPAddr(0), fNeedsCreating(true) {}
        
        UInt16 fPort;
        UInt32 fIPAddr;
        Bool16 fNeedsCreating; // ��Ҫ����TCPListenerSocket*��?
    };
    
	/* �˿�׷�ٽṹ�����鼰��Ŀ,������������Ҫ�ص��о���! */
    PortTracking* thePortTrackers = NULL;   
    UInt32 theTotalPortTrackers = 0;
    
    // Get the IP addresses from the pref
    UInt32 theNumAddrs = 0;
	/* ��ȡbind-RTSP-ip-address����ĿtheNumAddrs�͸�ip address������theIPAddrs */
    UInt32* theIPAddrs = this->GetRTSPIPAddrs(inPrefs, &theNumAddrs);   //theIPAddrs defintion
    UInt32 index = 0;
    
	/* �Ե��������(�˿��ص�)����������,���ص��˿ڷ�0ʱ,��ÿ���󶨵�IP��ַ,������ǡ����С�Ķ˿�׷�ٽṹ�����鲢�������ĸ������,���ж˿ڱ�����ָ�����ص��˿� */
    if ( inPortOverride != 0)
    { 
		/* ����bind-RTSP-ip-address��Ӧ�Ķ˿�׷�ٽṹ������ĸ������ */
        theTotalPortTrackers = theNumAddrs; // one port tracking struct for each IP addr
        thePortTrackers = NEW PortTracking[theTotalPortTrackers]; //creat and return the pointer of struct array
        for (index = 0; index < theNumAddrs; index++)
        {
            thePortTrackers[index].fPort = inPortOverride;
            thePortTrackers[index].fIPAddr = theIPAddrs[index];
        }
    }
    else /* �����ָ�����ص��˿�Ϊ0ʱ(�˿ڲ����ص�),�������θ���Щ */
    {
        UInt32 theNumPorts = 0;
		/* ��Ԥ�õ�RTSP�˿ڻ�ȡport�ĸ���theNumPorts,�������Զ˿ں�Ϊ����������thePorts */
        UInt16* thePorts = GetRTSPPorts(inPrefs, &theNumPorts);
		/* Ҫ׷�ٵĵ�ַ�˿���,ע��:ÿ��ip address��Ҫ����һ��˿����� */
        theTotalPortTrackers = theNumAddrs * theNumPorts; //* means multiplication
		/* �����µĶ˿�׷�ٽṹ�� */
        thePortTrackers = NEW PortTracking[theTotalPortTrackers]; //creat and return the pointer of struct array
        
        UInt32 currentIndex  = 0;
        
		/* ��ÿ��ip address�Ĳ�ͬport�ֱ������ö˿�׷�ٽṹ���еĳ�Ա */
        for (index = 0; index < theNumAddrs; index++)
        {
            for (UInt32 portIndex = 0; portIndex < theNumPorts; portIndex++)
            {
				/* ��ʾ�˿�׷�ٽṹ���еķ������� */
                currentIndex = (theNumPorts * index) + portIndex;
                
				/* ������������:currentIndex(�˿�׷�ٽṹ���еķ�������),portIndex(�˿�����),index(ip address����) */
                thePortTrackers[currentIndex].fPort = thePorts[portIndex]; //ָ���˿�
                thePortTrackers[currentIndex].fIPAddr = theIPAddrs[index]; //ָ��ip��ַ
            }
        }
                
        delete [] thePorts;//ɾ��QTSServer::GetRTSPPorts()�ж�̬����Ķ˿�����
    }
    
    delete [] theIPAddrs; //ɾ��QTSServer::GetRTSPIPAddrs()�ж�̬�����IP��ַ����
    
    // Now figure out which of these ports we are *already* listening on.
    // If we already are listening on that port, just move the pointer to the
    // listener over to the new array
	/* ���ȴֲڵش���һ��ͬ��С��TCPListenerSocket����,�ڷ�����ǰ���������Ķ˿ں�,���ÿ��RTSPsocket(IP address+port),����һ��TCP Listen socket,
	ע�����������������ݳ�ԱfListeners,fNumListeners */
    TCPListenerSocket** newListenerArray = NEW TCPListenerSocket*[theTotalPortTrackers];//refer to line 367
	/* ����ͳ�Ʋ����޸�TCP Listener socket�ĸ��� */
    UInt32 curPortIndex = 0; //curPortIndex definition
    
	// ����ָ����Щ�˿�,�����Ѿ�������,����TCPListenerSocket*�����½���newListenerArray
	/* ���統ǰ���ݳ�ԱfListeners�Ķ˿ں�ip��ַ������Ķ˿�׷�ٽṹ�������ĳ������ͬ,�ͽ�����ӽ�newListenerArray  */
    for (UInt32 count = 0; count < theTotalPortTrackers; count++)
    {
        for (UInt32 count2 = 0; count2 < fNumListeners; count2++)
        {
			/* ����TCP Listener socket�������thePortTrackers������ͬ,�Ͳ��轨���÷��� */
            if ((fListeners[count2]->GetLocalPort() == thePortTrackers[count].fPort) && 
                (fListeners[count2]->GetLocalAddr() == thePortTrackers[count].fIPAddr))
            {
                thePortTrackers[count].fNeedsCreating = false;
				/* ����ǰ��TCP Listener socket�ƽ������½���TCP Listener socket������ */
                newListenerArray[curPortIndex++] = fListeners[count2];
                Assert(curPortIndex <= theTotalPortTrackers);
                break;
            }
        }
    }
    
    //
    // Create any new listeners we need
	/* ע����RTSPListenerSocket(TCP listeners socket��������),��TCP listeners socket�ĺ�����Ŵ��� */
	/* ���϶˿�׷�ٽṹ�������б��ΪfNeedsCreating=true�ķ�����Ӧ��������newListenerArray����,��̬������,���μ���newListenerArray,��ʼ��,��TaskThread�����¼�;������,���¼����ԭ���error log */
    for (UInt32 count3 = 0; count3 < theTotalPortTrackers; count3++)
    {
		// ����ȷʵ��Ҫ��̬����TCPListenerSocket*
        if (thePortTrackers[count3].fNeedsCreating)
        {
			/* ע��curPortIndex�ǽ������TCP listeners socket��������,��ѭ��һ��һ������ */
            newListenerArray[curPortIndex] = NEW RTSPListenerSocket(); 
			/* ����TCP Socket������Ϊ������ģʽ;�󶨵����ָ����ip�Ͷ˿�;���ô�Ļ����С(96K�ֽ�);���õȴ����г���(128)����ʼ���� */
            QTSS_Error err = newListenerArray[curPortIndex]->Initialize(thePortTrackers[count3].fIPAddr, thePortTrackers[count3].fPort);

			/* ����ָ����ʽ��port�ַ��� */
            char thePortStr[20];
            qtss_sprintf(thePortStr, "%hu", thePortTrackers[count3].fPort);
            
            //
            // If there was an error creating this listener, destroy it and log an error
			/* ����Ҫ���ڿ�ʼ����,���Ǵ������RTSP Listeners socket������,������ɾȥ�÷��� */
            if ((startListeningNow) && (err != QTSS_NoErr))
                delete newListenerArray[curPortIndex];  //refer to line 415

			// �����ڶ�RTSPListenerSocket���г�ʼ��ʱ����,��¼�´�����Ϣ
            if (err == EADDRINUSE) /* ��ַ����ʹ�� */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortInUse, 0, thePortStr);
            else if (err == EACCES) /* ���ʴ��� */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortAccessDenied, 0, thePortStr);
            else if (err != QTSS_NoErr) /* �����˿ڴ��� */
                QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortError, 0, thePortStr);
            else
            {
                // ����RTSPListenerSocketʵ���ɹ�����,����ʼ���ɹ�,��Ҫ���ڿ�ʼ����,����TaskThread������¼�
                // This listener was successfully created.
                if (startListeningNow)
                    newListenerArray[curPortIndex]->RequestEvent(EV_RE); 
                curPortIndex++;
            }
        }
    }
    
    //
    // Kill any listeners that we no longer need
	/* �����ݳ�ԱfListeners�����е�ÿ��������newListenerArray������ÿ��������һ�Ա�,��ͬ�ı���,��ͬ�ķ��ź���TaskThreadɾȥ */
    for (UInt32 count4 = 0; count4 < fNumListeners; count4++)
    {
        Bool16 deleteThisOne = true;
        
		/* ���ִ���ܵ�TCP��RTSP Listener port������,���������TCP Listen socket��ͬ,�ͱ��� */
        for (UInt32 count5 = 0; count5 < curPortIndex; count5++)//curPortIndex == theTotalPortTrackers??
        {
            if (newListenerArray[count5] == fListeners[count4])
                deleteThisOne = false;
        }
        
		/* ��ЩҪɾȥ��,�ͷ��ź�ɾȥ */
        if (deleteThisOne)
            fListeners[count4]->Signal(Task::kKillEvent); //Signal means?
    }
    
    // Finally, make our server attributes and fListener privy to the new...
	/* ��󽫵�������TCP Listen socket���鼰��Ԫ�ظ������������ݳ�ԱfListeners,fNumListeners,���Ǹú�����Ҫ������!! */
	/******************* NOTE !! ******************/
    fListeners = newListenerArray;
    fNumListeners = curPortIndex;  //line 392
	/******************* NOTE !! ******************/
	/* ������¼���ù��������˿����Եĸ��� */
    UInt32 portIndex = 0;
          
	/* �������÷������˿����� */
    for (UInt32 count6 = 0; count6 < fNumListeners; count6++)
    {
        if  (fListeners[count6]->GetLocalAddr() != INADDR_LOOPBACK)
        {
            UInt16 thePort = fListeners[count6]->GetLocalPort();
            (void)this->SetValue(qtssSvrRTSPPorts, portIndex, &thePort, sizeof(thePort), QTSSDictionary::kDontObeyReadOnly);
            portIndex++;
        }
    }
	/* ���÷������˿ڸ��� */
    this->SetNumValues(qtssSvrRTSPPorts, portIndex);

	/* ɾȥ���涯̬�����Ķ˿�׷������,��Ϊ���������Ѿ������ */
    delete [] thePortTrackers; 

    return (fNumListeners > 0);
}

/* �Ӱ󶨵�ip��ַ��ȡ��ַ�ĸ���*outNumAddrsPtr,������ַ��ʮ����ָ�ʽ������theIPAddrArray��� */
UInt32* QTSServer::GetRTSPIPAddrs(QTSServerPrefs* inPrefs, UInt32* outNumAddrsPtr)
{
	 // Returns the number of values associated with a given attribute
	/*  see QTSSDictionary.h  */
	/* �ӷ�����Ԥ��ֵ����������˼���ip address? ������ʮ������ʽ������� */
    UInt32 numAddrs = inPrefs->GetNumValues(qtssPrefsRTSPIPAddr);//"bind_ip_addr" 
    UInt32* theIPAddrArray = NULL;
    
	/* ����û��"bind_ip_addr",���½�һ�����ֽ�,�κ�ip��ַ���� */
    if (numAddrs == 0)
    {
        *outNumAddrsPtr = 1; //note * denote the value not the pointer
        theIPAddrArray = NEW UInt32[1]; //define an array which will be return as function value
        theIPAddrArray[0] = INADDR_ANY; //�������κ�IP��ַ
    }
    else
    {
		/* ���������������������,����Ҫ! */
        theIPAddrArray = NEW UInt32[numAddrs + 1]; //����һ��ָ������+1������
        UInt32 arrIndex = 0;
        // Get the ip addr out of the prefs dictionary
        for (UInt32 theIndex = 0; theIndex < numAddrs; theIndex++)
        {
            QTSS_Error theErr = QTSS_NoErr;
            
			/* ��"bind_ip_addr"�л�ȡָ��������IP address string */
            char* theIPAddrStr = NULL;
            theErr = inPrefs->GetValueAsString(qtssPrefsRTSPIPAddr, theIndex, &theIPAddrStr);
            if (theErr != QTSS_NoErr)
            {
                delete [] theIPAddrStr; //�����ȡԤ��ֵ��ip��ַʧ��,ɾȥ��IP�ַ�������
                break;
            }

            
			/* ���õ���IP address stringת��Ϊʮ������Ƶ�IP address,����������theIPAddrArray����Ӧ���� */
            UInt32 theIPAddr = 0;
            if (theIPAddrStr != NULL)
            {
                theIPAddr = SocketUtils::ConvertStringToAddr(theIPAddrStr);
                delete [] theIPAddrStr;//ɾȥ�Ѿ�����ĸ�IP�ַ�������
                
                if (theIPAddr != 0)
                    theIPAddrArray[arrIndex++] = theIPAddr;//��IP��ַ���鸳ֵ
            }   
        }
        
		/* ����õ�ip address string,����ת��Ϊʮ������Ƶ�IP address��Ϊ0,���ip address��Ϊ����ip address */
        if ((numAddrs == 1) && (arrIndex == 0))
            theIPAddrArray[arrIndex++] = INADDR_ANY;
        else
            theIPAddrArray[arrIndex++] = INADDR_LOOPBACK;
    
		/* ��ѭ������ȷ�����ֵ */
        *outNumAddrsPtr = arrIndex;
    }
    
    return theIPAddrArray;
}

/* �ӷ�����Ԥ��ֵ�����ȡԤ�õ�rtsp�˿���Ŀ,����̬�����ͷ��ض˿�ֵ���� */
UInt16* QTSServer::GetRTSPPorts(QTSServerPrefs* inPrefs, UInt32* outNumPortsPtr)
{
	/* �ӷ�����Ԥ��ֵ����,��ȡRTSP ports��Ŀ,Ĭ����4�� */
    *outNumPortsPtr = inPrefs->GetNumValues(qtssPrefsRTSPPorts);//"rtsp_ports"
    
    if (*outNumPortsPtr == 0)
        return NULL;
     
	/* ��̬����ָ��ά��(4)�Ķ˿����� */
    UInt16* thePortArray = NEW UInt16[*outNumPortsPtr];
    
    for (UInt32 theIndex = 0; theIndex < *outNumPortsPtr; theIndex++)
    {
        // Get the ip addr out of the prefs dictionary
		/* ����Ϊ�����ֽ� */
        UInt32 theLen = sizeof(UInt16);
        QTSS_Error theErr = QTSS_NoErr;
        theErr = inPrefs->GetValue(qtssPrefsRTSPPorts, theIndex, &thePortArray[theIndex], &theLen);
        Assert(theErr == QTSS_NoErr);   
    }
    
    return thePortArray;
}

/* used in  StartServer() in RunServer.cpp */
//function finds all IP addresses on this machine, and binds 1 RTP / RTCP socket pair to a port pair on each address.
/* �Է������ϵ�����IP��ַ��port�����,��RTPSocketPool�ж�����һ��UDP Socket��,ͳ�Ƴ�����,����Socket pair�������߳�������¼�,������true */
Bool16  QTSServer::SetupUDPSockets()
{   
	/* ͳ�Ʒ������ϵ�IP��ַ����,�����Ӧ��RTP/RTCP Socket���� */
    UInt32 theNumAllocatedPairs = 0;
    for (UInt32 theNumPairs = 0; theNumPairs < SocketUtils::GetNumIPAddrs(); theNumPairs++)
    {
        UDPSocketPair* thePair = fSocketPool->CreateUDPSocketPair(SocketUtils::GetIPAddr(theNumPairs), 0);/* default port is 0 */
        if (thePair != NULL)
        {
            theNumAllocatedPairs++; //port pair increments by 1
			/* ����UDPSocket pair,�������߳�������¼� */
            thePair->GetSocketA()->RequestEvent(EV_RE);
            thePair->GetSocketB()->RequestEvent(EV_RE);
        }
     }

    // only return an error if we couldn't allocate ANY pairs of sockets
	// ���粻�ܷ����κ�UDPSocketPair,���÷�����״̬ΪFatal,������
    if (theNumAllocatedPairs == 0)
    {
            fServerState = qtssFatalErrorState; // also set the state to fatal error
            return false;
    }

    return true;
}

/* ��Windows��Ч */
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



/* ���ձ�׼����static���ط�����compiled��modules */
void    QTSServer::LoadCompiledInModules() //load modules into the server
{
#ifndef DSS_DYNAMIC_MODULES_ONLY  //need to be added by me
    // MODULE DEVELOPERS SHOULD ADD THE FOLLOWING THREE LINES OF CODE TO THIS
    // FUNCTION IF THEIR MODULE IS BEING COMPILED INTO THE SERVER.
    //
	// �μ�QTSSModule::QTSSModule(),����ڶ�������Ϊ��,˵����static Module
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


/* ��ʼ�����е�callback routines(��61��),����μ�QTSS_Private.h/cpp��QTSSCallback.h. ע��: ��ЩCallback routines��ʵ��
   ������ʵ�ϻ�����QTSSCallback.cpp��  */
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

/* ��ָ��Ŀ¼·����Win32��ʽ����ģ��,����˵����,�ȴ�Ԥ��ֵ��ȡmoduleĿ¼,��ĩβ����"\\*",�ڸ���·���ϲ����ļ�,
   �ҵ��󷵻��ļ����,��������ģ��,�����¼�����error log */
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

/* ���ȷ���module·������ȷ��,�ٴ����µ��ļ�·��,���½�QTSSModuleʵ������ʼ��һ��module,��setup��add���ظ�module */
void    QTSServer::CreateModule(char* inModuleFolderPath, char* inModuleName) //create and initial Module
{
    // Ignore these silly directory names
    /* һ���������Ϸ���ģ������,������������ */
	/* ע�����inModuleFolderPath��module�ļ��е�·��,�������ü�QTSServer::LoadModules() */
    if (::strcmp(inModuleName, ".") == 0) //the directory names is .
        return;
    if (::strcmp(inModuleName, "..") == 0)
        return;
    if (::strlen(inModuleName) == 0)
        return;
    if (*inModuleName == '.')//��ģ������.��ͷ,��������
        return; // Fix 2572248. Do not attempt to load '.' files as modules at all 

    //
    // Construct a full path to this module, denoted by theModPath.GetObject()
	/* ע��ģ�����ȫ·���ĳ�����module�ļ��е�·������+module���Ƶĳ��� */
    UInt32 totPathLen = ::strlen(inModuleFolderPath) + ::strlen(inModuleName); //full path
    OSCharArrayDeleter theModPath(NEW char[totPathLen + 4]);//theModPath definition, why add 4?
    ::strcpy(theModPath.GetObject(), inModuleFolderPath);
    ::strcat(theModPath.GetObject(), kPathDelimiterString);//i.e. "/"
    ::strcat(theModPath.GetObject(), inModuleName);/* ע��˴���theModPath.GetObject()ĩβ����3���ַ� */
            
    //
    // Construct a QTSSModule object, and attempt to initialize the module
	/* ���ձ�׼�����module�Ĳ������һ��ģ��:�ȸ���module�����ƺ�·��(��·������module����)�½�QTSSModuleʵ������ʼ��һ��module */
    QTSSModule* theNewModule = NEW QTSSModule(inModuleName, theModPath.GetObject());
	/* ���ؾ�̬(�����)��̬��,�������ַ���"INFO: Module Loaded...QTSSRefMovieModule [dynamic]"д�������־ */
    QTSS_Error theErr = theNewModule->SetupModule(&sCallbacks);
    
	/* ��setup module����,�ͽ�����("�����Module����")��¼��error log,��ɾ�����½�QTSSModuleʵ�� */
    if (theErr != QTSS_NoErr)
    {
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgBadModule, theErr,inModuleName);
        delete theNewModule; //fail to initialize so delete it
    }
    // ������Module����Module queue����,�ͼ�¼������Ϣ("ע��ʧ��")��error log,��ɾ�����½�QTSSModuleʵ��
    // If the module was successfully initialized, add it to our module queue
    else if (!this->AddModule(theNewModule))
    {
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgRegFailed, theErr,inModuleName);                                     
        delete theNewModule;  //fail to add to module queue so delete it
    }
}

/* ���÷ַ�����ע��õ�ModuleName,�½�ModuleԤ��ֵ�ֵ䲢��ʼ����Module,�ٸ��·�����ģ��qtssSvrModuleObjects����ֵ,���ָ����������ģ�������� */
Bool16 QTSServer::AddModule(QTSSModule* inModule)   // Add this module to the module queue
{
	/* ����Ҫȷ����module����ʼ��,�����˵,�ַ�����ָ��fDispatchFuncһ��Ҫ�ǿ�!��������QTSSModule::SetupModule()�еõ���֤ */
    Assert(inModule->IsInitialized());
    
    // Prepare to invoke the module's Register role. Setup the Register param block
	/* ����module��״̬��Ϣ */
    QTSS_ModuleState theModuleState;

    theModuleState.curModule = inModule;
	/* ����ʹ��ע���ɫ */
    theModuleState.curRole = QTSS_Register_Role;
	/* ���ڻ�û��Task */
    theModuleState.curTask = NULL;
	/* �������߳�˽������ */
    OSThread::SetMainThreadData(&theModuleState);
    
    // Currently we do nothing with the module name
	/* ����module��register role����,��յ�һ�������Խ��ܼ���������ֵ */
    QTSS_RoleParams theRegParams;
    theRegParams.regParams.outModuleName[0] = 0;
    
    // If the module returns an error from the QTSS_Register role, don't put it anywhere
	/* �����÷ַ�����(��QTSSModule::CallDispatch��װ)ע��QTSS_Register_Role����,�ͷ��� */
    if (inModule->CallDispatch(QTSS_Register_Role, &theRegParams) != QTSS_NoErr) 
        return false;
        
    OSThread::SetMainThreadData(NULL);
    
    //
    // Update the module name to reflect what was returned from the register role
    theRegParams.regParams.outModuleName[QTSS_MAX_MODULE_NAME_LENGTH - 1] = 0; //the last component of the array is set 0
	/* ����ע���ɫ���غ�,�õ���module name�ǿ�,����QTSSDictionary::SetValue()��������ΪqtssModName��ֵ */
    if (theRegParams.regParams.outModuleName[0] != 0) 
        inModule->SetValue(qtssModName, 0, theRegParams.regParams.outModuleName, ::strlen(theRegParams.regParams.outModuleName), false);

    //
    // Give the module object a prefs dictionary. Instance attributes are allowed for these objects.
	/* �½�QTSSPrefs,����kModulePrefsDictIndex�ֵ��ʼ��,����module��Ԥ��ֵ,����ʵ������ */
    QTSSPrefs* thePrefs = NEW QTSSPrefs( sPrefsSource, inModule->GetValue(qtssModName), QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModulePrefsDictIndex), true);
    thePrefs->RereadPreferences();//��ȡModuleԤ��ֵ
    inModule->SetPrefsDict(thePrefs);//����ΪModule��Ԥ��ֵ
        
    //
    // Add this module to the array of module (dictionaries)
	/* ��ȡ�÷����������ж��ٸ�Module ? */
    UInt32 theNumModules = this->GetNumValues(qtssSvrModuleObjects);
	/* ��module����ĩβ�������ָ����modul,����ģ��qtssSvrModuleObjects����ֵ */
    QTSS_Error theErr = this->SetValue(qtssSvrModuleObjects, theNumModules, &inModule, sizeof(QTSSModule*), QTSSDictionary::kDontObeyReadOnly);
    Assert(theErr == QTSS_NoErr);
    
    //
    // Add this module to the module queue
	/* ��ȡ��module�Ķ���Ԫ,�����뵽��Module������ */
    sModuleQueue.EnQueue(inModule->GetQueueElem());
    
    return true;
}

/* ����OSQueue���й��ߴ������н�ɫRole�ľ�̬module�����ͳ�Ƹý�ɫ�µ�module���� */
void QTSServer::BuildModuleRoleArrays()
{
	/* �μ�OSQueue.h/cpp, �Ķ�������ʱ���Ҫ�úö�������ļ� */
    OSQueueIter theIter(&sModuleQueue);
    QTSSModule* theModule = NULL;
        
    // Make sure these variables are cleaned out in case they've already been invited.
    /* ɾȥ����role��Ӧ�Ķ�άָ��(QTSSModule**)����sModuleArray,������������� */
    DestroyModuleRoleArrays();

    // Loop through all the roles of all the modules, recording the number of
    // modules in each role, and also recording which modules are doing what.
    /* �����е�role(�����QTSSModule.h)��ѭ�� */
    for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++)
    {
		/* ����ͳ��ָ��role��module�� */
        sNumModulesInRole[x] = 0; //the x-th component, x denotes the Number of Roles

		/* �Ա����е�staticģ����д�ͷ��βѭ��,ͳ��ÿ��role�����˶���Modules? */
        for (theIter.Reset(); !theIter.IsDone(); theIter.Next()) // note the repression of loop
        {
			/* �Ȼ�ȡ��ǰ����Ԫ���ڵ�module */
            theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject();
			/* ����ģ��ע���˸ý�ɫ,��sNumModulesInRole�ĵ�x��������1 */
            if (theModule->RunsInRole(x))
                sNumModulesInRole[x] += 1;
        }
     
		/* ��Ը�role��module��Ŀ,����module���� */
        if (sNumModulesInRole[x] > 0) 
        {
			/* ��������ͬһrole��ģ������Ԫ�� */
            UInt32 moduleIndex = 0;

			/* �½���̬module����sModuleArray,ע������Ԫ����QTSSModule* */
            sModuleArray[x] = new QTSSModule*[sNumModulesInRole[x] + 1]; //sModuleArray[x] (pointer array) definition
            /* ��ÿ��ģ����д�ͷ��βѭ��,������һ��һά���� */
			for (theIter.Reset(); !theIter.IsDone(); theIter.Next())
            {
				/* �Ȼ�ȡ��ǰ����Ԫ���ڵ�module */
                theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject(); 
                if (theModule->RunsInRole(x))
                {
					/* sModuleArray[x]�Ǹ�ģ������ķ���(ӵ�й�ͬ��role) */
                    sModuleArray[x][moduleIndex] = theModule; //2-dimmion array
                    moduleIndex++;
                }
            }
        }
    }
}

/* ɾȥ����role��Ӧ�Ķ�άָ��(QTSSModule**)����sModuleArray */
void QTSServer::DestroyModuleRoleArrays()
{
	/* �����е�roleѭ�� */
    for (UInt32 x = 0; x < QTSSModule::kNumRoles; x++)
    {
		/* ��ÿ��role��Ӧ��module������Ϊ0 */
        sNumModulesInRole[x] = 0; 
		/* ɾȥ�ý�ɫ��Ӧ��Module����(��άָ������QTSSModule**) */
        if (sModuleArray[x] != NULL) 
            delete [] sModuleArray[x];
        sModuleArray[x] = NULL; 
    }
}
 
/* ��ע���QTSS_Initialize_Role������modules,ѭ�����÷ַ�����ȥ��ɳ�ʼ������,�����÷ַ���������,������QTSSModuleUtils::LogError()������ԭ���module name��¼��error��־;
   ���÷�����Ĭ�ϴ����RTSP method��OPTIONS,���������������ܴ����RTSP������һ���ַ���sPublicHeaderStr,��"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n" */
void QTSServer::DoInitRole()
{
	/* ����initialize role����,�Ͳ�module����û������(�μ���������),see QTSS.h */
    QTSS_RoleParams theInitParams;
    theInitParams.initParams.inServer =         this;
    theInitParams.initParams.inPrefs =          fSrvrPrefs;
    theInitParams.initParams.inMessages =       fSrvrMessages;
    theInitParams.initParams.inErrorLogStream = &sErrorLogStream;
     
	/* ����QTSS_ModuleState */
    QTSS_ModuleState theModuleState;
    theModuleState.curRole = QTSS_Initialize_Role;/* ��ǰModule��role��QTSS_Initialize_Role */
    theModuleState.curTask = NULL;/* ��ǰModule��û��Task */
    OSThread::SetMainThreadData(&theModuleState); /* ��ʼ�����̵߳�˽������ */

    //
    // Add the OPTIONS method as the one method the server handles by default (it handles
    // it internally). Modules that handle other RTSP methods(see QTSSRTSPProtocol.h) will add 
	/* ���÷�����Ĭ�ϴ����RTSP method��OPTIONS,Modules���ó�ʼ����ɫ�ڸ����Ժ����������RTSP methods */
    QTSS_RTSPMethod theOptionsMethod = qtssOptionsMethod;
    (void)this->SetValue(qtssSvrHandledMethods, 0, &theOptionsMethod, sizeof(theOptionsMethod));


	// For now just disable the SetParameter to be compatible with Real.  It should really be removed only for clients 
	//	that have problems with their SetParameter implementations like (Real Players).
	// At the moment it isn't necessary to add the option.
	/* ����Real Player��compatible,����ȥ��ѡ�� */
	//   QTSS_RTSPMethod	theSetParameterMethod = qtssSetParameterMethod;
	//    (void)this->SetValue(qtssSvrHandledMethods, 0, &theSetParameterMethod, sizeof(theSetParameterMethod));
	
	/* ��ע���QTSS_Initialize_Role������modules,ѭ�����÷ַ�����ȥ�������,ע��QTSS_Initialize_Role��QTSSModule::kInitializeRole�Ĺ�ϵ
	   �μ�QTSSModule::AddRole() */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kInitializeRole); x++)
    {
		/* �õ�ָ��������ע��QTSS_Initialize_Role��module */
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kInitializeRole, x);
		/* �ø�ģ��ȥ���ó�ʼ����ɫ��module */
        theInitParams.initParams.inModule = theModule;
		/* �ø�ģ��ȥ����Module state�е�module */
        theModuleState.curModule = theModule;
		/* ���ø�module�ķַ����� */
        QTSS_Error theErr = theModule->CallDispatch(QTSS_Initialize_Role, &theInitParams);

        if (theErr != QTSS_NoErr)
        {
            // If the module reports an error when initializing itself,
            // delete the module and pretend it was never there.
			/* �����÷ַ���������,������QTSSModuleUtils::LogError()������ԭ���module name��¼��error��־ */
            QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgInitFailed, theErr,theModule->GetValue(qtssModName)->Ptr);                                        
            
			/* ��module��������ȥ��module */
            sModuleQueue.Remove(theModule->GetQueueElem());
            delete theModule;//ɾȥ��moduleʵ��
        }
    }

	/* ��������ͷ"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n",�μ����� */
    this->SetupPublicHeader();

    OSThread::SetMainThreadData(NULL); /* ������̵߳�˽������ */
}

/* �������������ܴ����RTSP����(��ֵ������)��һ���ַ���sPublicHeaderStr��sPublicHeaderFormatter,��"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n" */
void QTSServer::SetupPublicHeader()
{
    //
    // After the Init role, all the modules have reported the methods that they handle.
    // So, we can prune this attribute for duplicates, and construct a string to use in the
    // Public: header of the OPTIONS response
    QTSS_RTSPMethod* theMethod = NULL;
    UInt32 theLen = 0;

	/* �½�һ��12��������bool����,��¼��ÿ��module��RTSP����,��ʼ��Ϊ0,��QTSSRTSPProtocol.h */
    Bool16 theUniqueMethods[qtssNumMethods + 1]; //12
    ::memset(theUniqueMethods, 0, sizeof(theUniqueMethods)); 

	/* ����QTSSDictionary::GetValuePtr()ѭ�����������еķ���qtssSvrHandledMethods(�Ǹ���ֵ������),���������鸳ֵ */
    for (UInt32 y = 0; this->GetValuePtr(qtssSvrHandledMethods, y, (void**)&theMethod, &theLen) == QTSS_NoErr; y++)
		/* ע����QTSSRTSPProtocol.h��,ÿ��RTSP method��Ӧһ��index */
        theUniqueMethods[*theMethod] = true; 

    // Rewrite the qtssSvrHandledMethods, eliminating any duplicates that modules may have introduced
	/* ���·������Ĵ�����,�����޸Ĺ��Ĵ������� */
    UInt32 uniqueMethodCount = 0;
    for (QTSS_RTSPMethod z = 0; z < qtssNumMethods; z++)
    {
        if (theUniqueMethods[z])
            this->SetValue(qtssSvrHandledMethods, uniqueMethodCount++, &z, sizeof(QTSS_RTSPMethod));
    }
	/* ���·������������Դ����RTSP������ */
    this->SetNumValues(qtssSvrHandledMethods, uniqueMethodCount);
    
    // Format a text string for the Public header
    ResizeableStringFormatter theFormatter(NULL, 0);//ע���ʱ��δ���仺��

	/* ͨ��ѭ���õ����������ܴ����RTSP����(��ֵ������)��һ���ַ��� */
    for (UInt32 a = 0; this->GetValuePtr(qtssSvrHandledMethods, a, (void**)&theMethod, &theLen) == QTSS_NoErr; a++)
    {
        sPublicHeaderFormatter.Put(RTSPProtocol::GetMethodString(*theMethod));
        sPublicHeaderFormatter.Put(", ");
    }
    sPublicHeaderStr.Ptr = sPublicHeaderFormatter.GetBufPtr();
    sPublicHeaderStr.Len = sPublicHeaderFormatter.GetBytesWritten() - 2; //trunc the last ", "
}

/**************  ������RTSPListenerSocket��ĳ�Ա���� ***************************/

/* ��������TCP socket����һ���㲥��,����һ��RTSP sessionʵ��(���и�TCPSocket��Ա),�������RTSPSessionʵ������socket������,������������������ƽ���accept�㲥�Ự�Ľ��� */
/* �������غ���,used in TCPListenerSocket::ProcessEvent() */
Task*   RTSPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{
	/* ȷ��TCP socket������ָ��**�ǿ�,�ɸ�������԰쵽:TCPSocket* theSocket = NULL;��&theSocket����� */
    Assert(outSocket != NULL);
    
    // when the server is behing a round robin DNS(��ѯ����), the client needs to known the IP address of the server
    // so that it can direct the "POST" half of the connection to the same machine when tunnelling RTSP thru HTTP
	// this pref tells the server to report its IP address in the reply to the HTTP GET request when tunneling RTSP through HTTP
	/* �ɷ�����Ԥ��ֵ,��ȡ�Ƿ��Client��������������ip��ַ? */
    Bool16  doReportHTTPConnectionAddress = QTSServerInterface::GetServer()->GetPrefs()->GetDoReportHTTPConnectionAddress();
    
	/* ����һ���µ㲥�ỰRTSPSession���񲢻�ȡsocket */
    RTSPSession* theTask = NEW RTSPSession(doReportHTTPConnectionAddress);
	/* ��øõ㲥�Ự��TCPSocket��������ָ�� */
    *outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.ע���������������TCPListenerSocket::ProcessEvent()

	/* ����������,���������������(streamingserver.xml����Ϊ1000)ʱ,��������TCPListenerSocket��accept()֮������1����accept().
	�μ�TCPListenerSocket::ProcessEvent() */
    if (this->OverMaxConnections(0))
		/* ��������TCPListenerSocket::fSleepBetweenAccepts = true */
        this->SlowDown();
    else
		/* ��������TCPListenerSocket::fSleepBetweenAccepts = false */
        this->RunNormal();
        
    return theTask;
}

/* �ӷ�������ȡ�����������Ԥ��ֵ,���жϵ�ǰ��RTP��RTSP�������Ƿ���,����Boolean��� */
Bool16 RTSPListenerSocket::OverMaxConnections(UInt32 buffer)
{
	/* �ӷ�������ȡ�����������ֵ */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();
	/* �Ƿ񳬹���������� */
    Bool16 overLimit = false;
    
    if (maxConns > -1) // limit connections
    { 
        maxConns += buffer;
		/* ֻҪRTP session����RTSP session�����������������,���ǳ��� */
        if  ( (theServer->GetNumRTPSessions() > (UInt32) maxConns) 
              ||
              ( theServer->GetNumRTSPSessions() + theServer->GetNumRTSPHTTPSessions() > (UInt32) maxConns ) 
            )
        {
            overLimit = true;          
        }
    } 
	/* ��������Ƿ��޵Ľ�� */
    return overLimit;
     
}

/**************  ������RTSPListenerSocket��ĳ�Ա���� ***************************/

/***********************  ������RTPSocketPool��ĳ�Ա���� ***************************/

/* ע��RTPSocketPool��QTSServer���friend class,�ú�������:�ӷ�������ȡRTCP����ʵ��ָ�벢��Ӧ����һ��UDPSocketPairʵ�� */
UDPSocketPair*  RTPSocketPool::ConstructUDPSocketPair()
{
	/* ��QTSServerInterface���ȡRTCP����ָ��(ע��Ӹ���ǿ��ת��Ϊ����) */
    Task* theTask = ((QTSServer*)QTSServerInterface::GetServer())->fRTCPTask;
    
    //construct a pair of UDP sockets, the lower one for RTP data (outgoing only, no demuxer
    //necessary), and one for RTCP data (incoming, so definitely need a demuxer).
    //These are nonblocking sockets that DON'T receive events (we are going to poll for data)
	// They do receive events - we don't poll from them anymore
	/* �𲽴���UDPSocketPair,�˿�С���������ⷢ��RTP����,���踴����;�˿ڴ���������ڽ���RTCP��,һ����Ҫ������,���Ƿ��������͵�. �μ�UDPSocketPool.h��Socket.h�Ĺ��캯�� */
    return NEW UDPSocketPair(  NEW UDPSocket(theTask, Socket::kNonBlockingSocketType),
                               NEW UDPSocket(theTask, UDPSocket::kWantsDemuxer | Socket::kNonBlockingSocketType)); //�ᴴ��UDPDemuxerʵ��
}

/* ɾȥ���ָ��(����,���洴��)��һ��UDPSocketPairʵ�� */
void RTPSocketPool::DestructUDPSocketPair(UDPSocketPair* inPair)
{
	/* ��ɾȥ������UDPSocket,��ɾȥUDPSocketPair.ע�������delete������RTPSocketPool::ConstructUDPSocketPair()�е�NEW��һһ��Ӧ�� */
    delete inPair->GetSocketA();
    delete inPair->GetSocketB();
    delete inPair;
}

/* ��������UDP socket�Եķ���buffer����ջ����С:��һ�����ù̶�,�ڶ�������Ҫ���,��:����UDPSocketPair�����ⷢ��RTP���ݵ�socket buffer��СΪ256K�ֽ�,
��Ԥ��ֵ��ʼ,�Լ����������̬����RTCP socket����buffer�Ĵ�С */
void RTPSocketPool::SetUDPSocketOptions(UDPSocketPair* inPair)
{
    // Apparently the socket buffer size matters even though this is UDP and being
    // used for sending... on UNIX typically the socket buffer size doesn't matter because the
    // packet goes right down to the driver. On Win32 and linux, unless this is really big, we get packet loss.
	/* ����UDPSocketPair�����ⷢ��RTP���ݵ�socket buffer��СΪ256K�ֽ�,���TCPListenerSocket::Initialize() */
    inPair->GetSocketA()->SetSocketBufSize(256 * 1024);

    // Always set the Rcv buf size for the RTCP sockets. This is important because the
    // server is going to be getting many many acks.
	/* ��ȡ������Ԥ���RTCP socket�Ľ��ջ����С,768K�ֽ�  */
    UInt32 theRcvBufSize = QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK();

    // ����ʱԤ��ֵ�Բ���ϵͳ����ʱ,������������,ֱ��ϵͳ����Ϊֹ
    // In case the rcv buf size is too big for the system, retry, dividing the requested size by 2.
    // Until it works, or until some minimum value is reached.
    OS_Error theErr = EINVAL;
    while ((theErr != OS_NoErr) && (theRcvBufSize > 32))
    {
		/* ��Ԥ��ֵ��ʼ,�Լ����������̬����RTCP socket����buffer�Ĵ�С */
        theErr = inPair->GetSocketB()->SetSocketRcvBufSize(theRcvBufSize * 1024);
        if (theErr != OS_NoErr)
			/* ʹ�����С��Ϊԭ����һ�� */
            theRcvBufSize >>= 1;
    }

    // ��RTCP socket����buffer��С��Ԥ��ֵ����̬���ĺ�,��¼��־
    // Report an error if we couldn't set the socket buffer size the user requested
    if (theRcvBufSize != QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK())
    {
		/* ����ָ����ʽ��theRcvBufSize */
        char theRcvBufSizeStr[20];
        qtss_sprintf(theRcvBufSizeStr, "%lu", theRcvBufSize);
        
        // For now, do not log an error, though we should enable this in the future.
       QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSockBufSizesTooLarge,0, theRcvBufSizeStr);
    }
}

/**************************  ������RTPSocketPool��ĳ�Ա���� ***************************/


/* ��ȡ������,����module,�Լ�ע��QTSS_RereadPrefs_Role��module��Ԥ��ֵ,���·�����������,�˿ں�IP��ַ */
QTSS_Error QTSServer::RereadPrefsService(QTSS_ServiceFunctionArgsPtr /*inArgs*/)
{
    // This function can only be called safely when the server is completely running.
    // Ensuring this is a bit complicated because of preemption. Here's how it's done...
    // ��ȡ������ʵ��ָ��
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    // This is to make sure this function isn't being called before the server is completely started up.
    // ���������δ��ȫ����,���߲�������״̬,��������
    if ((theServer == NULL) || (theServer->GetServerState() != qtssRunningState))
        return QTSS_OutOfState;

    // Because the server must have started up, and because this object always stays
    // around (until the process dies), we can now safely get this object.
	// ��ȡ������Ԥ��ֵ����ָ��(��ʱ�������Ѿ���ȫ����,�������ܰ�ȫ�õ���Ԥ��ֵ����)
    QTSServerPrefs* thePrefs = theServer->GetPrefs();
    
    // Grab the prefs mutex. We want to make sure that calls to RereadPrefsService
    // are serialized. This also prevents the server from shutting down while in
    // this function, because the QTSServer destructor grabs this mutex as well.
	// �Է�����Ԥ��ֵ�������,��Ҳ�ܷ�ֹ��ִ��RereadPrefsService()ʱ,�������ر�,��ΪQTSServer��������Ҳ���õ�������Ԥ��ֵ������
    OSMutexLocker locker(thePrefs->GetMutex());
    
    // Finally, check the server state again. The state may have changed
    // to qtssShuttingDownState or qtssFatalErrorState in this time, though
    // at this point we have the prefs mutex, so we are guarenteed that the
    // server can't actually shut down anymore
	/* ���������Ѿ�ץסԤ��ֵ������,���ǻ�Ҫ�ٴ�ȷ����������ǰ״̬��qtssRunningState,���������˳� */
    if (theServer->GetServerState() != qtssRunningState)
        return QTSS_OutOfState;
    
    // Ok, we're ready to reread preferences now.
    // Reread preferences
	/* ����xml�����ļ��и�Tag�ĺϷ��� */
    sPrefsSource->Parse();
	/* �ȱ����������������б�,�����Ԥ��ֵ�ļ����ø�ָ��������,��Ԥ��ֵ����,����Ĭ��ֵ����,Ȼ��������֤��ʽ,
	RTP/RTCP��ͷ��ӡѡ��,��־д�ر�,��󽫵������Ԥ��ֵ����д��xmlԤ��ֵ�ļ� */
    thePrefs->RereadServerPreferences(true);
    
    {
        // ��ȡ����������Ļ�����,Update listeners, ports, and IP addrs.
        OSMutexLocker locker(theServer->GetServerObjectMutex());
		// ��Ԥ��ֵ��ȡRTSP��ip��ַ,���õ�һ���󶨵�ipΪĬ�ϵĵ㲥IP����������Ӧ��DNS������ip address string����
        (void)((QTSServer*)theServer)->SetDefaultIPAddr();
		// ����Ԥ��ֵ,��������Socket����,�����ø���fNumListeners��fListeners,��һ�����true��ʾ,���ڿ�ʼ��������TaskThread������¼�
        (void)((QTSServer*)theServer)->CreateListeners(true, thePrefs, 0);
    }
    
    // Delete all the streams
	/* ���ζ���������еĸ���ģ���Ԥ��ֵ */
    QTSSModule** theModule = NULL;
    UInt32 theLen = 0;

    // Go through each module's prefs object and have those reread as well
    for (int y = 0; QTSServerInterface::GetServer()->GetValuePtr(qtssSvrModuleObjects, y, (void**)&theModule, &theLen) == QTSS_NoErr; y++)
    {
        Assert(theModule != NULL);
        Assert(theLen == sizeof(QTSSModule*));
        /* ����ÿ��module��Ԥ��ֵ */
        (*theModule)->GetPrefsDict()->RereadPreferences();

#if DEBUG
        theModule = NULL;
        theLen = 0;
#endif
    }
    
    // Now that we are done rereading the prefs, invoke all modules in the RereadPrefs
    // role so they can update their internal prefs caches.
	/* ʹ��Ԥ��ֵ��ɫ��module���ε���QTSS_RereadPrefs_Roleȥ�����ڲ���Ԥ��ֵ���� */
    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRereadPrefsRole); x++)
    {
        QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRereadPrefsRole, x);
        (void)theModule->CallDispatch(QTSS_RereadPrefs_Role, NULL);
    }
    return QTSS_NoErr;
}


