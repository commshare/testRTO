
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RunServer.cpp
Description: define the mian entry functions to run the Streaming Server on Linux platform.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "RunServer.h"
#include "OS.h"
#include "OSMemory.h"
#include "OSThread.h"
#include "OSArrayObjectDeleter.h"
#include "SafeStdLib.h"
#include "Socket.h"
#include "SocketUtils.h"
#include "ev.h"
#include "Task.h"
#include "IdleTask.h"
#include "TimeoutTask.h"
#include "DateTranslator.h"
#include "QTSServerInterface.h"
#include "QTSServer.h"
#include "QTSSRollingLog.h"


//ȫ�־�̬����
QTSServer* sServer = NULL;
int sStatusUpdateInterval = 0;
Bool16 sHasPID = false;

/* ����������ֵ��DebugLevel_1()������ */
UInt64 sLastStatusPackets = 0;
UInt64 sLastDebugPackets = 0;
SInt64 sLastDebugTotalQuality = 0;


QTSS_ServerState StartServer(XMLPrefsParser* inPrefsSource,/* PrefsSource* inMessagesSource,*/ UInt16 inPortOverride, int statsUpdateInterval, QTSS_ServerState inInitialState, Bool16 inDontFork, UInt32 debugLevel, UInt32 debugOptions)
{
    //Mark when we are done starting up. If auto-restart is enabled, we want to make sure
    //to always exit with a status of 0 if we encountered a problem WHILE STARTING UP. This
    //will prevent infinite-auto-restart-loop type problems
	/* ���ò��Զ����� */
    Bool16 doneStartingUp = false;
	/* ���÷�����Ϊ����״̬ */
    QTSS_ServerState theServerState = qtssStartingUpState;
    
	/* ���������״̬�������� */
    sStatusUpdateInterval = statsUpdateInterval;
    
    //Initialize utility classes
	/* ���÷�����sInitialMsec��ֵ! */
    OS::Initialize();
	/* ����ͬһ�����������̹߳����TLS�洢����,��ȡthread index */
    OSThread::Initialize();
    /* ����һ��event thread,�������̼߳����� */
    Socket::Initialize();
	/* �������ݱ����͵�Socket,��ȡ����IP Address List;����ָ����С��IPAddrInfoArray,�����ó���IP address,
	IP address string,DNSΪ�����Ľṹ��IPAddrInfo������.�п��ܵĻ�,������һ���͵ڶ���������λ�� */
    SocketUtils::Initialize(!inDontFork);

    ::select_startevents();//initialize the select() implementation of the event queue
    
    //start the server
    QTSSDictionaryMap::Initialize();
	/* ����DSS��ͷ��Ϣ */
    QTSServerInterface::Initialize();// this must be called before constructing the server object

	/************NOTE!!******************/
	/* �½���̬QTSServer���� */
    sServer = NEW QTSServer();
	/************NOTE!!******************/

    sServer->SetDebugLevel(debugLevel);
    sServer->SetDebugOptions(debugOptions);
    
    // re-parse config file
    inPrefsSource->Parse();

	/* Ĭ�ϴ������� */
    Bool16 createListeners = true;
	/* �����Ϊ�������ر�״̬ʱ,���������� */
    if (qtssShuttingDownState == inInitialState) 
        createListeners = false;
    
	/* ע����Ҫ!! */
	/* ��ʼ��DSS,����QTSServer::CreateListeners(),����TCPListenerSocketȥ���� */
    sServer->Initialize(inPrefsSource,/* inMessagesSource,*/ inPortOverride,createListeners);

	/* �������inInitialState��ֵ */
    if (inInitialState == qtssShuttingDownState)
    {  
		/* ��ʼ�����е�modules */
        sServer->InitModules(inInitialState);
        return inInitialState;
    }
    
	/* ��ʼ������OSCharArrayDeleter����,�μ�OSArrayObjectDeleter.h */
    OSCharArrayDeleter runGroupName(sServer->GetPrefs()->GetRunGroupName());
    OSCharArrayDeleter runUserName(sServer->GetPrefs()->GetRunUserName());
	/* ����User/Group�ַ���(��Ϊ128���ȵ�C-String) */
    OSThread::SetPersonality(runUserName.GetObject(), runGroupName.GetObject());

	/* ����ָ����С�������߳�(����CPU������һ�������߳�)��TimeoutTaskThread,������ */
    if (sServer->GetServerState() != qtssFatalErrorState)
    {
        UInt32 numThreads = 0;
        
		/* �Է�Macƽ̨����true */
        if (OS::ThreadSafe())
        {
			/* ��ȡԤ��ֵ���̸߳���,ע��streamingserver.xml�е�Ԥ��ֵ��0 */
            numThreads = sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
            if (numThreads == 0)
				/* Ĭ��һ��processor��һ��thread */
                numThreads = OS::GetNumProcessors(); // 1 worker thread per processor
        }
		/* ȷ��Ҫ��һ��thread */
        if (numThreads == 0)
            numThreads = 1;

		/* ����ָ����С�������߳����鲢����,����true */
        TaskThreadPool::AddThreads(numThreads);

    #if DEBUG
        qtss_printf("Number of task threads: %lu\n",numThreads);
    #endif
    
        // Start up the server's global tasks, and start listening
		/* ����������TimeoutTaskThread��ע������һ����ͨ��Taskʵ��,��IdleThread�޹أ�,����IdleTask::Initialize() */
        TimeoutTask::Initialize();     // The TimeoutTask mechanism is task based, we therefore must do this after adding task threads
                                       // this be done before starting the sockets and server tasks
     }

	/**************************NOTE: ������Server�����׶�,�Ե���CPU����4���߳�.����ת������ִ�н׶� *************************************/

    //Make sure to do this stuff last. Because these are all the threads that
    //do work in the server, this ensures that no work can go on while the server
    //is in the process of staring up
    if (sServer->GetServerState() != qtssFatalErrorState)
    {
		/* ��û�п����߳�ʱ,������������ */
        IdleTask::Initialize();

		/************* ��������Socket����  ************************/
		/* ����һ��event thread,���麯��,�ɸ��Ե������̺߳���ִ�� */
        Socket::StartThread();
		/* ���߳�����1s */
        OSThread::Sleep(1000);
        
        //
        // On Win32, in order to call modwatch, the Socket EventQueue thread must be
        // created first. Modules call modwatch from their initializer, and we don't
        // want to prevent them from doing that, so module initialization is separated
        // out from other initialization, and we start the Socket EventQueue thread first.
        // The server is still prevented from doing anything as of yet, because there
        // aren't any TaskThreads yet.
		/* ���ȿ���Socket EventQueue thread */

		/* ��ʼ�����е�modules */
        sServer->InitModules(inInitialState);
		/* ����RTCP Task������RTPSocketPool�����е�UDPSocketPair�Ͽ��ܵ�����RTCP������,RTPStatsUpdaterTask,��ÿ��TCPListenerSocket,�������߳�������¼� */
        sServer->StartTasks();
		/* �Է������ϵ�����IP��ַ,ÿ��port pair������һ��UDP Socket��,ͳ�Ƴ�����,����Socket pair�������߳�������¼�,������true */
        sServer->SetupUDPSockets(); // udp sockets are set up after the rtcp task is instantiated(ʵ����)
		/* ���·�����״̬ */
        theServerState = sServer->GetServerState();
    }

    if (theServerState != qtssFatalErrorState)
    {
		/* ������������Windowsû�� */
        CleanPid(true);
        WritePid(!inDontFork);

		/* ��������״̬,��ʼֵ����������һ�� */
        doneStartingUp = true;
        qtss_printf("Streaming Server done starting up\n");
		/* �����ڴ����Ϊû���㹻�ڴ�ռ� */
        OSMemory::SetMemoryError(ENOMEM);
    }


    // SWITCH TO RUN USER AND GROUP ID
	/* ��Windows��Ч */
    if (!sServer->SwitchPersonality())
        theServerState = qtssFatalErrorState;

   //
    // Tell the caller whether the server started up or not
    return theServerState;
}

void WritePid(Bool16 forked)
{
	// WRITE PID TO FILE
	OSCharArrayDeleter thePidFileName(sServer->GetPrefs()->GetPidFilePath());
	FILE *thePidFile = fopen(thePidFileName, "w");
	if(thePidFile)
	{
		if (!forked)
			fprintf(thePidFile,"%d\n",getpid());    // write own pid
		else
		{
			fprintf(thePidFile,"%d\n",getppid());    // write parent pid
			fprintf(thePidFile,"%d\n",getpid());    // and our own pid in the next line
		}                
		fclose(thePidFile);
		sHasPID = true;
	}
}

/* ɾ��ָ����pid�ļ� */
void CleanPid(Bool16 force)
{
    if (sHasPID || force)
    {
        OSCharArrayDeleter thePidFileName(sServer->GetPrefs()->GetPidFilePath());
        unlink(thePidFileName);
    }
}

/* ��ָ����ʽ����������stateFile */
void LogStatus(QTSS_ServerState theServerState)
{
    static QTSS_ServerState lastServerState = 0;

	/* �����ͷ��ָ������ */
    static char *sPLISTHeader[] =
    {   
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
        "<!ENTITY % plistObject \"(array | data | date | dict | real | integer | string | true | false )\">",
        "<!ELEMENT plist %plistObject;>",
        "<!ATTLIST plist version CDATA \"0.9\">",
        "",
        "<!-- Collections -->",
        "<!ELEMENT array (%plistObject;)*>",
        "<!ELEMENT dict (key, %plistObject;)*>",
        "<!ELEMENT key (#PCDATA)>",
        "",
        "<!--- Primitive types -->",
        "<!ELEMENT string (#PCDATA)>",
        "<!ELEMENT data (#PCDATA)> <!-- Contents interpreted as Base-64 encoded -->",
        "<!ELEMENT date (#PCDATA)> <!-- Contents should conform to a subset of ISO 8601 (in particular, YYYY '-' MM '-' DD 'T' HH ':' MM ':' SS 'Z'.  Smaller units may be omitted with a loss of precision) -->",
        "",
        "<!-- Numerical primitives -->",
        "<!ELEMENT true EMPTY>  <!-- Boolean constant true -->",
        "<!ELEMENT false EMPTY> <!-- Boolean constant false -->",
        "<!ELEMENT real (#PCDATA)> <!-- Contents should represent a floating point number matching (\"+\" | \"-\")? d+ (\".\"d*)? (\"E\" (\"+\" | \"-\") d+)? where d is a digit 0-9.  -->",
        "<!ELEMENT integer (#PCDATA)> <!-- Contents should represent a (possibly signed) integer number in base 10 -->",
        "]>",
    };

	/* Header Lines Number */
    static int numHeaderLines = sizeof(sPLISTHeader) / sizeof(char*);

    static char*    sPlistStart = "<plist version=\"0.9\">";
    static char*    sPlistEnd = "</plist>";
    static char*    sDictStart = "<dict>";
    static char*    sDictEnd = "</dict>";
    
	/* ���ø�ʽ�ַ���,������������Ҫ�� */
    static char*    sKey    = "     <key>%s</key>\n";
    static char*    sValue  = "     <string>%s</string>\n";
    
	/* ����ָ������� */
    static char *sAttributes[] =
    {
        "qtssSvrServerName",
        "qtssSvrServerVersion",
        "qtssSvrServerBuild",
        "qtssSvrServerPlatform",
        "qtssSvrRTSPServerComment",
        "qtssSvrServerBuildDate",
        "qtssSvrStartupTime",
        "qtssSvrCurrentTimeMilliseconds",
        "qtssSvrCPULoadPercent",
        "qtssSvrState",
        "qtssRTPSvrCurConn",
        "qtssRTSPCurrentSessionCount",
        "qtssRTSPHTTPCurrentSessionCount",
        "qtssRTPSvrCurBandwidth",
        "qtssRTPSvrCurPackets",
        "qtssRTPSvrTotalConn",
        "qtssRTPSvrTotalBytes",
        "qtssMP3SvrCurConn",
        "qtssMP3SvrTotalConn",
        "qtssMP3SvrCurBandwidth",
        "qtssMP3SvrTotalBytes"
    };
	/* ���Եĸ��� */
    static int numAttributes = sizeof(sAttributes) / sizeof(char*);
        
    static StrPtrLen statsFileNameStr("server_status");    
    
	/* ���粻�ܵõ�Pref�еķ�����stateFile�ͷ��� */
    if (false == sServer->GetPrefs()->ServerStatFileEnabled())
        return;
    /* �õ�������stateFile��ʱ����(s) */    
    UInt32 interval = sServer->GetPrefs()->GetStatFileIntervalSec();
	/* ʹ������stateFile��ʱ����������������OS::UnixTime_Secs() */
    if (interval == 0 || (OS::UnixTime_Secs() % interval) > 0 ) 
        return;
    
    // If the total number of RTSP sessions is 0  then we 
    // might not need to update the "server_status" file.

    char* thePrefStr = NULL;
    // We start lastRTSPSessionCount off with an impossible value so that
    // we force the "server_status" file to be written at least once.
    static int lastRTSPSessionCount = -1; 
    // Get the RTSP session count from the server.
	/* ��ȡRTSP Current session���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
	/* ���ַ���ת���������� */
    int currentRTSPSessionCount = ::atoi(thePrefStr);
	/* ɾȥ�Ա��´�ʹ�� */
    delete [] thePrefStr; thePrefStr = NULL;
	/* ������������IdleState,���������"server_status" file,�ο�����һ�仰 */
    if (currentRTSPSessionCount == 0 && currentRTSPSessionCount == lastRTSPSessionCount)
    {
        // we don't need to update the "server_status" file except the
        // first time we are in the idle state.
        if (theServerState == qtssIdleState && lastServerState == qtssIdleState)
        {
            lastRTSPSessionCount = currentRTSPSessionCount; //0
            lastServerState = theServerState; //qtssIdleState
            return;
        }
    }
    else
    {
        // save the RTSP session count for the next time we execute.
        lastRTSPSessionCount = currentRTSPSessionCount;
    }

	/* ��StatsMonitorFileName�ŵ�ErrorLogDir��,ĩβ�ټ�'\0' */
	/* ����˼·���е�����! */
    StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
    StrPtrLenDel fileNameStr(sServer->GetPrefs()->GetStatsMonitorFileName());
    ResizeableStringFormatter pathBuffer(NULL,0);
    pathBuffer.PutFilePath(&pathStr,&fileNameStr);
    pathBuffer.PutTerminator();
    
	/* �õ�����StatsMonitorFile·�� */
    char*   filePath = pathBuffer.GetBufPtr();
	/* ��"ֻд"��ʽ��һ������ļ�,���ظ��ļ���� */
    FILE*   statusFile = ::fopen(filePath, "w");
    char*   theAttributeValue = NULL;
    int     i;
    
    if (statusFile != NULL)
    {
		/* �ı��ļ��ķ���Ȩ�� */
        ::chmod(filePath, 0640);
		/* ��sPLISTHeader���м�������ļ�statusFile�� */
        for ( i = 0; i < numHeaderLines; i++)
        {    
            qtss_fprintf(statusFile, "%s\n",sPLISTHeader[i]);    
        }

		/* �����μ���sPlistStart,sDictStart��statusFile�ļ� */
        qtss_fprintf(statusFile, "%s\n", sPlistStart);
        qtss_fprintf(statusFile, "%s\n", sDictStart);    

         // show each element value
         for ( i = 0; i < numAttributes; i++)
        {
			/* ��ȡÿ�����Ե�ֵ */
            (void)QTSS_GetValueAsString(sServer, QTSSModuleUtils::GetAttrID(sServer,sAttributes[i]), 0, &theAttributeValue);
            if (theAttributeValue != NULL)
             {
				 /* �������ַ�������ֵ��ָ����ʽд��statusFile */
                qtss_fprintf(statusFile, sKey, sAttributes[i]);    
                qtss_fprintf(statusFile, sValue, theAttributeValue);
				/* ͬʱɾȥ����ֵ����Ӧ�ַ��� */
                delete [] theAttributeValue;
                theAttributeValue = NULL;
             }
         }
        
		/* �����μ���sDictEnd,sPlistEnd��statusFile�ļ� */
        qtss_fprintf(statusFile, "%s\n", sDictEnd);
        qtss_fprintf(statusFile, "%s\n\n", sPlistEnd);    
         
		/* �ر�״̬�ļ�statusFile */
        ::fclose(statusFile);
    }
    lastServerState = theServerState;
}

/* ���ַ�����ָ����ʽд��ָ�����ļ������̨��,ע���������������DebugLevel_1()Ӧ��Ƶ�� */
void print_status(FILE* file, FILE* console, char* format, char* theStr)
{
    if (file) qtss_fprintf(file, format, theStr);
    if (console) qtss_fprintf(console, format, theStr);

}

/* ��ָ����Ϣ"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time"
   �������뵽�ļ�statusFile��stdOut */
void DebugLevel_1(FILE*   statusFile, FILE*   stdOut,  Bool16 printHeader )
{
	/* ���Pref String,������ʹ�� */
    char*  thePrefStr = NULL;
	/* ����ǰ������kBits/SecΪ��λ�ȴ����ָ���ַ���,���Ȳ��ó���11 */
	/* ע�����������ʹ��,ֻ����ʱ�洢֮�� */
    static char numStr[12] ="";
	/* ���ָ����ʽ��ʱ�� */
    static char dateStr[25] ="";
    UInt32 theLen = 0;

	/* �����ӡ��ͷ */
    if ( printHeader )
    {                   
        /* ���ַ�����ָ����ʽд��ָ�����ļ������̨��,��������� */
        print_status(statusFile,stdOut,"%s", "     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time\n");

    }
    
	/* ��ȡ����ʾRTP current connection��,����� */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
    
	/* ����Ա��´�ʹ�� */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡ����ʾRTSP current connection��,����� */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* ����Ա��´�ʹ�� */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡ����ʾRTSP/HTTP current connection��,����� */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* ����Ա��´�ʹ�� */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡRTP current Bandwidth */
    UInt32 curBandwidth = 0;
    theLen = sizeof(curBandwidth);
    (void)QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
	/* ����ǰ������kBits/SecΪ��λ����ָ���ַ���,���Ȳ��ó���11 */
    qtss_snprintf(numStr, 11, "%lu", curBandwidth/1024);
    /* ���ַ�����ָ����ʽд��ָ�����ļ������̨�� */
    print_status(statusFile, stdOut,"%11s", numStr);

	/* ��ȡRTP current packets��,����� */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* ����Ա��´�ʹ�� */
    delete [] thePrefStr; thePrefStr = NULL;

    /* �õ���ǰ���ŵ�RTP��,�洢����ʾ */
    UInt32 currentPlaying = sServer->GetNumRTPPlayingSessions();
    qtss_snprintf( numStr, sizeof(numStr) -1, "%lu", currentPlaying);
    print_status(statusFile, stdOut,"%14s", numStr);

   
    //is the server keeping up with the streams?
    //what quality are the streams?
	/* ������������(���ǻ��Ժ�ɾȥ)�ݴ���.cpp��ͷ���������������:sLastDebugPackets,sLastDebugTotalQuality */
    SInt64 totalRTPPaackets = sServer->GetTotalRTPPackets();
    SInt64 deltaPackets = totalRTPPaackets - sLastDebugPackets;
    sLastDebugPackets = totalRTPPaackets;

    SInt64 totalQuality = sServer->GetTotalQuality();
    SInt64 deltaQuality = totalQuality - sLastDebugTotalQuality;
    sLastDebugTotalQuality = totalQuality;

	//delay
    SInt64 currentMaxLate =  sServer->GetCurrentMaxLate();
    SInt64 totalLate =  sServer->GetTotalLate();

	/* ������Ӧ�ĳ�Ա:fTotalLate,fCurrentMaxLate,fTotalQuality */
    sServer->ClearTotalLate();
    sServer->ClearCurrentMaxLate();
    sServer->ClearTotalQuality();
    
	/* �ַ���ĩβ���� */
    ::qtss_snprintf(numStr, sizeof(numStr) -1, "%s", "0");

	/* ��ӡ���AvgDelay CurMaxDelay  MaxDelay */
    if (deltaPackets > 0)
        qtss_snprintf(numStr, sizeof(numStr) -1, "%ld", (SInt32) ((SInt64)totalLate /  (SInt64) deltaPackets ));
    print_status(statusFile, stdOut,"%11s", numStr);

    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32) currentMaxLate);
    print_status(statusFile, stdOut,"%11s", numStr);
    
    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32)  sServer->GetMaxLate() );
    print_status(statusFile, stdOut,"%11s", numStr);

	/* ��ӡ���AvgQuality  NumThinned  Time */
    ::qtss_snprintf(numStr, sizeof(numStr) -1, "%s", "0");
    if (deltaPackets > 0)
        qtss_snprintf(numStr, sizeof(numStr) -1, "%ld", (SInt32) ((SInt64) deltaQuality /  (SInt64) deltaPackets));
    print_status(statusFile, stdOut,"%11s", numStr);

    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32) sServer->GetNumThinned() );
    print_status(statusFile, stdOut,"%11s", numStr);

    
    /* ���仺��,�õ�ָ����ʽ��ʱ��,���PrintStatus() */
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
    (void) QTSSRollingLog::FormatDate(theDateBuffer, false);
    
	/* ��ź���ʾָ����ʽ��ʱ�� */
    qtss_snprintf(dateStr,sizeof(dateStr) -1, "%s", theDateBuffer );
    print_status(statusFile, stdOut,"%24s\n", dateStr);
}

/* �ӷ�������ȡDebug options,ȷ���Ƿ�Log��Ϣ��ʾ���? ����ʾLog��Ϣ,�򴴽��ļ���ָ��·��,
   ����"����׷��"��ʽ�򿪸��ļ�,��󷵻ش��ļ��ľ��,���򷵻�NULL */
FILE* LogDebugEnabled()
{
	/* �����Log��Ϣʱ */
    if (DebugLogOn(sServer))
    {
		/* ����״̬�ļ��� */
        static StrPtrLen statsFileNameStr("server_debug_status");    
        /* �õ�Error Log dir */
        StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
		/* ��ʼ�� */
        ResizeableStringFormatter pathBuffer(NULL,0);
		/* ��ָ���ļ�������ָ��·�� */
        pathBuffer.PutFilePath(&pathStr,&statsFileNameStr);
		/* ĩβ����'\0' */
        pathBuffer.PutTerminator();
        
		/* �õ��ļ�·�� */
        char*   filePath = pathBuffer.GetBufPtr();  
		/* ��"����׷��"��ʽ��ָ��·�����ļ�,���ش��ļ��ľ�� */
        return ::fopen(filePath, "a");
    }
    
    return NULL;
}

/* �ӷ�������ȡDebug options,ȷ���Ƿ�Debug��Ϣ��ʾ���? */
FILE* DisplayDebugEnabled()
{   
	/* stdout��ϵͳ����ı�׼����ļ� */
    return ( DebugDisplayOn(sServer) ) ? stdout   : NULL ;
}

/* Ĭ�ϴ���Ļ������־�ļ���ʾDebug��ص�ָ����Ϣ"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time" */
void DebugStatus(UInt32 debugLevel, Bool16 printHeader)
{
	/* �ӷ�������ȡDebug options,ȷ���Ƿ�Log��Ϣ��ʾ���? ����ʾLog��Ϣ,�򴴽��ļ���ָ��·��,
	����"����׷��"��ʽ�򿪸��ļ�,��󷵻ش��ļ��ľ��,���򷵻�NULL */    
    FILE*   statusFile = LogDebugEnabled();
	/* �ӷ�������ȡDebug options,ȷ���Ƿ�Debug��Ϣ��ʾ���? */
    FILE*   stdOut = DisplayDebugEnabled();
    
    if (debugLevel > 0)
		/* ��ָ����Ϣ"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time"
		�������뵽�ļ����statusFile��stdOut */
        DebugLevel_1(statusFile, stdOut, printHeader);

	/* DebugĬ���Ǵ���Ļ��ʾ������־�ļ� */
    if (statusFile) 
        ::fclose(statusFile);
}

/* �����totalBytes(���ֽڴ�С)��ָ����ʽ(�ֱ���G/M/K/BΪ��λ)���뻺��*outBuffer�� */
void FormattedTotalBytesBuffer(char *outBuffer, int outBufferLen, UInt64 totalBytes)
{
	/* ��ʾ�����ֽ��� */
    Float32 displayBytes = 0.0;
	/* ������С�ĵ�λ���ַ��� */
    char  sizeStr[] = "B";
	/* ��ʽ�ַ��� */
    char* format = NULL;
        
    if (totalBytes > 1073741824 ) //2^30=1GBytes
    {   displayBytes = (Float32) ( (Float64) (SInt64) totalBytes /  (Float64) (SInt64) 1073741824 );
        sizeStr[0] = 'G';
        format = "%.4f%s ";
     }
    else if (totalBytes > 1048576 ) //2^20=1MBytes
    {   displayBytes = (Float32) (SInt32) totalBytes /  (Float32) (SInt32) 1048576;
        sizeStr[0] = 'M';
        format = "%.3f%s ";
     }
    else if (totalBytes > 1024 ) //2^10=1KBytes
    {    displayBytes = (Float32) (SInt32) totalBytes /  (Float32) (SInt32) 1024;
         sizeStr[0] = 'K';
         format = "%.2f%s ";
    }
    else
    {    displayBytes = (Float32) (SInt32) totalBytes;  //Bytes
         sizeStr[0] = 'B';
         format = "%4.0f%s ";
    }
    
    outBuffer[outBufferLen -1] = 0;
    qtss_snprintf(outBuffer, outBufferLen -1,  format , displayBytes, sizeStr);
}

/* ��Ļ��ʾ���9����ָ����Ϣ */
void PrintStatus(Bool16 printHeader)
{
    char* thePrefStr = NULL;
    UInt32 theLen = 0;
    
	/* ��ӡ�����г��ļ�����Ϣ,ע�����ֽ�����ʱ����ָ����ʽ��� */
    if ( printHeader )
    {                       
        qtss_printf("     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec    TotConn     TotBytes   TotPktsLost          Time\n");   
    }

	/* ��ȡRTP current conn��Ϣ,����Ļ����ʾ,��ɾȥ�м���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡRTSP current conn��Ϣ,����Ļ����ʾ,��ɾȥ�м���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡRTSP-HTTP current conn��Ϣ,����Ļ����ʾ,��ɾȥ�м���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡRTP current bandwidth��Ϣ,����Ļ����ʾ */
    UInt32 curBandwidth = 0;
    theLen = sizeof(curBandwidth);
    (void)QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
    qtss_printf( "%11lu", curBandwidth/1024);
    
	/* ��ȡRTP current��ȡ�İ���,����Ļ����ʾ,��ɾȥ�м���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡRTP Total conn��Ϣ,����Ļ����ʾ,��ɾȥ�м���� */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrTotalConn, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* ��ȡ���͵���RTP�ֽ��� */
    UInt64 totalBytes = sServer->GetTotalRTPBytes();
	/* ���ָ����ʽ����RTP�ֽ����Ļ��� */
    char  displayBuff[32] = "";
    /* �����totalBytes(���ֽڴ�С)��ָ����ʽ(�ֱ���G/M/K/BΪ��λ)���뻺���� */
    FormattedTotalBytesBuffer(displayBuff, sizeof(displayBuff),totalBytes);
    qtss_printf( "%17s", displayBuff);
    
	/* �õ���RTP������ */
    qtss_printf( "%11"_64BITARG_"u", sServer->GetTotalRTPPacketsLost());
    
	/* ����ָ����С�Ļ���(30���ֽ�) */
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	/* ����ǰʱ����localʱ����ʽ��ָ����ʽ����������½��Ļ����� */
    (void) QTSSRollingLog::FormatDate(theDateBuffer, false);
	/* ���ָ����ʽ�ĵ�ǰʱ��:YYYY-MM-DD HH:MM:SS */
    qtss_printf( "%25s",theDateBuffer);
    
    qtss_printf( "\n");
    
}

/* ÿ��sStatusUpdateInterval * 10Ҫ��ӡһ��Header����? */
Bool16 PrintHeader(UInt32 loopCount)
{
     return ( (loopCount % (sStatusUpdateInterval * 10) ) == 0 ) ? true : false;
}

/* ÿ��sStatusUpdateIntervalҪ��ӡһ����? */
Bool16 PrintLine(UInt32 loopCount)
{
     return ( (loopCount % sStatusUpdateInterval) == 0 ) ? true : false;
}

/* ���������й����й�������� */
void RunServer()
{   
    Bool16 restartServer = false;
    UInt32 loopCount = 0;
    UInt32 debugLevel = 0;

	/* �ֱ���ܴ�PrintHeader()��PrintLine()���ص�ֵ,������ */
    Bool16 printHeader = false;
    Bool16 printStatus = false;


    //just wait until someone stops the server or a fatal error occurs.
	/* �õ���������״̬ */
    QTSS_ServerState theServerState = sServer->GetServerState();
    while ((theServerState != qtssShuttingDownState) &&
            (theServerState != qtssFatalErrorState))
    {
        OSThread::Sleep(1000);

		/* ��¼������״̬��־ */
        LogStatus(theServerState);

		/* �ӵ�0�п�ʼÿ10��ѭ����ʾ/��ӡHeader��Ϣ */
        if (sStatusUpdateInterval)
        {
			/* ���Debug���� */
            debugLevel = sServer->GetDebugLevel(); 
			/* �����������������ķ���ֵ */
            printHeader = PrintHeader(loopCount);
            printStatus = PrintLine(loopCount);
             
			/* ����ʾÿ�е�ǰ���� */
            if (printStatus)
            {
				/* �Ƿ���ʾDebug��Ϣ?ע��Debug��ϢҪô����Ļ����ʾ,Ҫô��¼��log�� */
                if  (DebugOn(sServer) ) // debug level display or logging is on
					/* Ĭ�ϴ���Ļ������־�ļ���ʾDebug��ص�ָ����Ϣ"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time" */
                    DebugStatus(debugLevel, printHeader);
                
                if (!DebugDisplayOn(sServer))
					/* ��Ļ��ʾ���9����ָ����Ϣ,������� */
                    PrintStatus(printHeader); // default status output
            }
            
            
            loopCount++;

        }
        
		/* ��ҪInterrupt��Terminate������ */
        if ((sServer->SigIntSet()) || (sServer->SigTermSet()))
        {
            //
            // start the shutdown process
			/* ���÷�����״̬��qtssShuttingDownState */
            theServerState = qtssShuttingDownState;
            (void)QTSS_SetValue(QTSServerInterface::GetServer(), qtssSvrState, 0, &theServerState, sizeof(theServerState));

			/* ��������ҪSigal Interrupt DSS,�����ÿ������������� */
			/* ע��terminate DSS,�Ͳ������� */
            if (sServer->SigIntSet())
                restartServer = true;
        }
        
		/* ���»�ȡ������״̬,����ѭ��Ҫ�� */
        theServerState = sServer->GetServerState();
        if (theServerState == qtssIdleState)
			/* ɱ�����е�RTP Session */
            sServer->KillAllRTPSessions();
    }
    
    //
    // Kill all the sessions and wait for them to die,
    // but don't wait more than 5 seconds
    sServer->KillAllRTPSessions();
    for (UInt32 shutdownWaitCount = 0; (sServer->GetNumRTPSessions() > 0) && (shutdownWaitCount < 5); shutdownWaitCount++)
        OSThread::Sleep(1000);
        
    //Now, make sure that the server can't do any work
	/* �������ֹͣ����,����,ɾȥ�����߳� */
    TaskThreadPool::RemoveThreads();
    
    //now that the server is definitely stopped, it is safe to initate
    //the shutdown process
	/* ɾȥ���������� */
    delete sServer;
    
    CleanPid(false);
    //ok, we're ready to exit. If we're quitting because of some fatal error
    //while running the server, make sure to let the parent process know by
    //exiting with a nonzero status. Otherwise, exit with a 0 status
    if (theServerState == qtssFatalErrorState || restartServer)
        ::exit (-2);//-2 signals parent process to restart server
}
