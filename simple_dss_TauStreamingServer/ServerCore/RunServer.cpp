
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


//全局静态变量
QTSServer* sServer = NULL;
int sStatusUpdateInterval = 0;
Bool16 sHasPID = false;

/* 下面这三个值在DebugLevel_1()中设置 */
UInt64 sLastStatusPackets = 0;
UInt64 sLastDebugPackets = 0;
SInt64 sLastDebugTotalQuality = 0;


QTSS_ServerState StartServer(XMLPrefsParser* inPrefsSource,/* PrefsSource* inMessagesSource,*/ UInt16 inPortOverride, int statsUpdateInterval, QTSS_ServerState inInitialState, Bool16 inDontFork, UInt32 debugLevel, UInt32 debugOptions)
{
    //Mark when we are done starting up. If auto-restart is enabled, we want to make sure
    //to always exit with a status of 0 if we encountered a problem WHILE STARTING UP. This
    //will prevent infinite-auto-restart-loop type problems
	/* 设置不自动重启 */
    Bool16 doneStartingUp = false;
	/* 设置服务器为启动状态 */
    QTSS_ServerState theServerState = qtssStartingUpState;
    
	/* 用入参设置状态更新区间 */
    sStatusUpdateInterval = statsUpdateInterval;
    
    //Initialize utility classes
	/* 配置服务器sInitialMsec的值! */
    OS::Initialize();
	/* 分配同一进程中所有线程共享的TLS存储索引,获取thread index */
    OSThread::Initialize();
    /* 创建一个event thread,启动该线程见下面 */
    Socket::Initialize();
	/* 创建数据报类型的Socket,获取它的IP Address List;创建指定大小的IPAddrInfoArray,并配置成以IP address,
	IP address string,DNS为分量的结构体IPAddrInfo的数组.有可能的话,互换第一个和第二个分量的位置 */
    SocketUtils::Initialize(!inDontFork);

    ::select_startevents();//initialize the select() implementation of the event queue
    
    //start the server
    QTSSDictionaryMap::Initialize();
	/* 给出DSS的头信息 */
    QTSServerInterface::Initialize();// this must be called before constructing the server object

	/************NOTE!!******************/
	/* 新建静态QTSServer对象 */
    sServer = NEW QTSServer();
	/************NOTE!!******************/

    sServer->SetDebugLevel(debugLevel);
    sServer->SetDebugOptions(debugOptions);
    
    // re-parse config file
    inPrefsSource->Parse();

	/* 默认创建侦听 */
    Bool16 createListeners = true;
	/* 当入参为服务器关闭状态时,不创建侦听 */
    if (qtssShuttingDownState == inInitialState) 
        createListeners = false;
    
	/* 注意重要!! */
	/* 初始化DSS,包含QTSServer::CreateListeners(),创建TCPListenerSocket去侦听 */
    sServer->Initialize(inPrefsSource,/* inMessagesSource,*/ inPortOverride,createListeners);

	/* 考虑入参inInitialState的值 */
    if (inInitialState == qtssShuttingDownState)
    {  
		/* 初始化所有的modules */
        sServer->InitModules(inInitialState);
        return inInitialState;
    }
    
	/* 初始化两个OSCharArrayDeleter对象,参见OSArrayObjectDeleter.h */
    OSCharArrayDeleter runGroupName(sServer->GetPrefs()->GetRunGroupName());
    OSCharArrayDeleter runUserName(sServer->GetPrefs()->GetRunUserName());
	/* 设置User/Group字符串(皆为128长度的C-String) */
    OSThread::SetPersonality(runUserName.GetObject(), runGroupName.GetObject());

	/* 创建指定大小的任务线程(单核CPU仅启动一个任务线程)和TimeoutTaskThread,并开启 */
    if (sServer->GetServerState() != qtssFatalErrorState)
    {
        UInt32 numThreads = 0;
        
		/* 对非Mac平台都是true */
        if (OS::ThreadSafe())
        {
			/* 获取预设值的线程个数,注意streamingserver.xml中的预设值是0 */
            numThreads = sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
            if (numThreads == 0)
				/* 默认一个processor有一个thread */
                numThreads = OS::GetNumProcessors(); // 1 worker thread per processor
        }
		/* 确保要有一个thread */
        if (numThreads == 0)
            numThreads = 1;

		/* 创建指定大小的任务线程数组并开启,返回true */
        TaskThreadPool::AddThreads(numThreads);

    #if DEBUG
        qtss_printf("Number of task threads: %lu\n",numThreads);
    #endif
    
        // Start up the server's global tasks, and start listening
		/* 创建并启动TimeoutTaskThread（注意它是一个普通的Task实例,跟IdleThread无关）,类似IdleTask::Initialize() */
        TimeoutTask::Initialize();     // The TimeoutTask mechanism is task based, we therefore must do this after adding task threads
                                       // this be done before starting the sockets and server tasks
     }

	/**************************NOTE: 以上是Server启动阶段,对单核CPU创建4个线程.下面转入任务执行阶段 *************************************/

    //Make sure to do this stuff last. Because these are all the threads that
    //do work in the server, this ensures that no work can go on while the server
    //is in the process of staring up
    if (sServer->GetServerState() != qtssFatalErrorState)
    {
		/* 若没有空闲线程时,创建并启动它 */
        IdleTask::Initialize();

		/************* 启动监听Socket过程  ************************/
		/* 启动一个event thread,纯虚函数,由各自的任务线程函数执行 */
        Socket::StartThread();
		/* 主线程休眠1s */
        OSThread::Sleep(1000);
        
        //
        // On Win32, in order to call modwatch, the Socket EventQueue thread must be
        // created first. Modules call modwatch from their initializer, and we don't
        // want to prevent them from doing that, so module initialization is separated
        // out from other initialization, and we start the Socket EventQueue thread first.
        // The server is still prevented from doing anything as of yet, because there
        // aren't any TaskThreads yet.
		/* 首先开启Socket EventQueue thread */

		/* 初始化所有的modules */
        sServer->InitModules(inInitialState);
		/* 创建RTCP Task来处理RTPSocketPool中所有的UDPSocketPair上可能到来的RTCP包数据,RTPStatsUpdaterTask,对每个TCPListenerSocket,向任务线程请求读事件 */
        sServer->StartTasks();
		/* 对服务器上的所有IP地址,每个port pair都创建一对UDP Socket对,统计出对数,启动Socket pair向任务线程请求读事件,并返回true */
        sServer->SetupUDPSockets(); // udp sockets are set up after the rtcp task is instantiated(实例化)
		/* 更新服务器状态 */
        theServerState = sServer->GetServerState();
    }

    if (theServerState != qtssFatalErrorState)
    {
		/* 这两个函数对Windows没用 */
        CleanPid(true);
        WritePid(!inDontFork);

		/* 设置启动状态,初始值见本函数第一行 */
        doneStartingUp = true;
        qtss_printf("Streaming Server done starting up\n");
		/* 设置内存错误为没有足够内存空间 */
        OSMemory::SetMemoryError(ENOMEM);
    }


    // SWITCH TO RUN USER AND GROUP ID
	/* 对Windows无效 */
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

/* 删除指定的pid文件 */
void CleanPid(Bool16 force)
{
    if (sHasPID || force)
    {
        OSCharArrayDeleter thePidFileName(sServer->GetPrefs()->GetPidFilePath());
        unlink(thePidFileName);
    }
}

/* 以指定格式建立服务器stateFile */
void LogStatus(QTSS_ServerState theServerState)
{
    static QTSS_ServerState lastServerState = 0;

	/* 定义表头的指针数组 */
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
    
	/* 设置格式字符串,下面设置属性要用 */
    static char*    sKey    = "     <key>%s</key>\n";
    static char*    sValue  = "     <string>%s</string>\n";
    
	/* 属性指针的数组 */
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
	/* 属性的个数 */
    static int numAttributes = sizeof(sAttributes) / sizeof(char*);
        
    static StrPtrLen statsFileNameStr("server_status");    
    
	/* 假如不能得到Pref中的服务器stateFile就返回 */
    if (false == sServer->GetPrefs()->ServerStatFileEnabled())
        return;
    /* 得到服务器stateFile的时间间隔(s) */    
    UInt32 interval = sServer->GetPrefs()->GetStatFileIntervalSec();
	/* 使服务器stateFile的时间间隔非零且能整除OS::UnixTime_Secs() */
    if (interval == 0 || (OS::UnixTime_Secs() % interval) > 0 ) 
        return;
    
    // If the total number of RTSP sessions is 0  then we 
    // might not need to update the "server_status" file.

    char* thePrefStr = NULL;
    // We start lastRTSPSessionCount off with an impossible value so that
    // we force the "server_status" file to be written at least once.
    static int lastRTSPSessionCount = -1; 
    // Get the RTSP session count from the server.
	/* 获取RTSP Current session计数 */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
	/* 将字符串转换成整型数 */
    int currentRTSPSessionCount = ::atoi(thePrefStr);
	/* 删去以备下次使用 */
    delete [] thePrefStr; thePrefStr = NULL;
	/* 当服务器处于IdleState,就无需更新"server_status" file,参考上面一句话 */
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

	/* 将StatsMonitorFileName放到ErrorLogDir后,末尾再加'\0' */
	/* 这套思路很有典型性! */
    StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
    StrPtrLenDel fileNameStr(sServer->GetPrefs()->GetStatsMonitorFileName());
    ResizeableStringFormatter pathBuffer(NULL,0);
    pathBuffer.PutFilePath(&pathStr,&fileNameStr);
    pathBuffer.PutTerminator();
    
	/* 得到上述StatsMonitorFile路径 */
    char*   filePath = pathBuffer.GetBufPtr();
	/* 以"只写"方式打开一个输出文件,返回该文件句柄 */
    FILE*   statusFile = ::fopen(filePath, "w");
    char*   theAttributeValue = NULL;
    int     i;
    
    if (statusFile != NULL)
    {
		/* 改变文件的访问权限 */
        ::chmod(filePath, 0640);
		/* 将sPLISTHeader逐行加入输出文件statusFile中 */
        for ( i = 0; i < numHeaderLines; i++)
        {    
            qtss_fprintf(statusFile, "%s\n",sPLISTHeader[i]);    
        }

		/* 再依次加入sPlistStart,sDictStart到statusFile文件 */
        qtss_fprintf(statusFile, "%s\n", sPlistStart);
        qtss_fprintf(statusFile, "%s\n", sDictStart);    

         // show each element value
         for ( i = 0; i < numAttributes; i++)
        {
			/* 获取每个属性的值 */
            (void)QTSS_GetValueAsString(sServer, QTSSModuleUtils::GetAttrID(sServer,sAttributes[i]), 0, &theAttributeValue);
            if (theAttributeValue != NULL)
             {
				 /* 将属性字符串和其值按指定格式写入statusFile */
                qtss_fprintf(statusFile, sKey, sAttributes[i]);    
                qtss_fprintf(statusFile, sValue, theAttributeValue);
				/* 同时删去属性值和相应字符串 */
                delete [] theAttributeValue;
                theAttributeValue = NULL;
             }
         }
        
		/* 再依次加入sDictEnd,sPlistEnd到statusFile文件 */
        qtss_fprintf(statusFile, "%s\n", sDictEnd);
        qtss_fprintf(statusFile, "%s\n\n", sPlistEnd);    
         
		/* 关闭状态文件statusFile */
        ::fclose(statusFile);
    }
    lastServerState = theServerState;
}

/* 将字符串以指定格式写入指定的文件或控制台中,注意这个函数在下面DebugLevel_1()应用频繁 */
void print_status(FILE* file, FILE* console, char* format, char* theStr)
{
    if (file) qtss_fprintf(file, format, theStr);
    if (console) qtss_fprintf(console, format, theStr);

}

/* 将指定信息"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time"
   连续输入到文件statusFile或stdOut */
void DebugLevel_1(FILE*   statusFile, FILE*   stdOut,  Bool16 printHeader )
{
	/* 存放Pref String,下面多次使用 */
    char*  thePrefStr = NULL;
	/* 将当前带宽以kBits/Sec为单位等存入该指定字符串,长度不得超过11 */
	/* 注意该量被反复使用,只起临时存储之用 */
    static char numStr[12] ="";
	/* 存放指定格式的时间 */
    static char dateStr[25] ="";
    UInt32 theLen = 0;

	/* 假如打印表头 */
    if ( printHeader )
    {                   
        /* 将字符串以指定格式写入指定的文件或控制台中,定义见上面 */
        print_status(statusFile,stdOut,"%s", "     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time\n");

    }
    
	/* 获取并显示RTP current connection数,并输出 */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
    
	/* 清空以便下次使用 */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取并显示RTSP current connection数,并输出 */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* 清空以便下次使用 */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取并显示RTSP/HTTP current connection数,并输出 */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* 清空以便下次使用 */
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取RTP current Bandwidth */
    UInt32 curBandwidth = 0;
    theLen = sizeof(curBandwidth);
    (void)QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
	/* 将当前带宽以kBits/Sec为单位存入指定字符串,长度不得超过11 */
    qtss_snprintf(numStr, 11, "%lu", curBandwidth/1024);
    /* 将字符串以指定格式写入指定的文件或控制台中 */
    print_status(statusFile, stdOut,"%11s", numStr);

	/* 获取RTP current packets数,并输出 */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
    print_status(statusFile, stdOut,"%11s", thePrefStr);
	/* 清空以便下次使用 */
    delete [] thePrefStr; thePrefStr = NULL;

    /* 得到当前播放的RTP数,存储并显示 */
    UInt32 currentPlaying = sServer->GetNumRTPPlayingSessions();
    qtss_snprintf( numStr, sizeof(numStr) -1, "%lu", currentPlaying);
    print_status(statusFile, stdOut,"%14s", numStr);

   
    //is the server keeping up with the streams?
    //what quality are the streams?
	/* 将下面三个量(它们会稍后删去)暂存在.cpp开头定义的三个变量中:sLastDebugPackets,sLastDebugTotalQuality */
    SInt64 totalRTPPaackets = sServer->GetTotalRTPPackets();
    SInt64 deltaPackets = totalRTPPaackets - sLastDebugPackets;
    sLastDebugPackets = totalRTPPaackets;

    SInt64 totalQuality = sServer->GetTotalQuality();
    SInt64 deltaQuality = totalQuality - sLastDebugTotalQuality;
    sLastDebugTotalQuality = totalQuality;

	//delay
    SInt64 currentMaxLate =  sServer->GetCurrentMaxLate();
    SInt64 totalLate =  sServer->GetTotalLate();

	/* 清除相对应的成员:fTotalLate,fCurrentMaxLate,fTotalQuality */
    sServer->ClearTotalLate();
    sServer->ClearCurrentMaxLate();
    sServer->ClearTotalQuality();
    
	/* 字符串末尾置零 */
    ::qtss_snprintf(numStr, sizeof(numStr) -1, "%s", "0");

	/* 打印输出AvgDelay CurMaxDelay  MaxDelay */
    if (deltaPackets > 0)
        qtss_snprintf(numStr, sizeof(numStr) -1, "%ld", (SInt32) ((SInt64)totalLate /  (SInt64) deltaPackets ));
    print_status(statusFile, stdOut,"%11s", numStr);

    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32) currentMaxLate);
    print_status(statusFile, stdOut,"%11s", numStr);
    
    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32)  sServer->GetMaxLate() );
    print_status(statusFile, stdOut,"%11s", numStr);

	/* 打印输出AvgQuality  NumThinned  Time */
    ::qtss_snprintf(numStr, sizeof(numStr) -1, "%s", "0");
    if (deltaPackets > 0)
        qtss_snprintf(numStr, sizeof(numStr) -1, "%ld", (SInt32) ((SInt64) deltaQuality /  (SInt64) deltaPackets));
    print_status(statusFile, stdOut,"%11s", numStr);

    qtss_snprintf(numStr,sizeof(numStr) -1, "%ld", (SInt32) sServer->GetNumThinned() );
    print_status(statusFile, stdOut,"%11s", numStr);

    
    /* 分配缓存,得到指定格式的时间,亦见PrintStatus() */
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
    (void) QTSSRollingLog::FormatDate(theDateBuffer, false);
    
	/* 存放和显示指定格式的时间 */
    qtss_snprintf(dateStr,sizeof(dateStr) -1, "%s", theDateBuffer );
    print_status(statusFile, stdOut,"%24s\n", dateStr);
}

/* 从服务器获取Debug options,确定是否将Log信息显示输出? 若显示Log信息,则创建文件的指定路径,
   并以"数据追加"方式打开该文件,最后返回打开文件的句柄,否则返回NULL */
FILE* LogDebugEnabled()
{
	/* 当输出Log信息时 */
    if (DebugLogOn(sServer))
    {
		/* 设置状态文件名 */
        static StrPtrLen statsFileNameStr("server_debug_status");    
        /* 得到Error Log dir */
        StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
		/* 初始化 */
        ResizeableStringFormatter pathBuffer(NULL,0);
		/* 将指定文件名放入指定路径 */
        pathBuffer.PutFilePath(&pathStr,&statsFileNameStr);
		/* 末尾放上'\0' */
        pathBuffer.PutTerminator();
        
		/* 得到文件路径 */
        char*   filePath = pathBuffer.GetBufPtr();  
		/* 以"数据追加"方式打开指定路径的文件,返回打开文件的句柄 */
        return ::fopen(filePath, "a");
    }
    
    return NULL;
}

/* 从服务器获取Debug options,确定是否将Debug信息显示输出? */
FILE* DisplayDebugEnabled()
{   
	/* stdout是系统定义的标准输出文件 */
    return ( DebugDisplayOn(sServer) ) ? stdout   : NULL ;
}

/* 默认从屏幕而非日志文件显示Debug相关的指定信息"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time" */
void DebugStatus(UInt32 debugLevel, Bool16 printHeader)
{
	/* 从服务器获取Debug options,确定是否将Log信息显示输出? 若显示Log信息,则创建文件的指定路径,
	并以"数据追加"方式打开该文件,最后返回打开文件的句柄,否则返回NULL */    
    FILE*   statusFile = LogDebugEnabled();
	/* 从服务器获取Debug options,确定是否将Debug信息显示输出? */
    FILE*   stdOut = DisplayDebugEnabled();
    
    if (debugLevel > 0)
		/* 将指定信息"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time"
		连续输入到文件句柄statusFile或stdOut */
        DebugLevel_1(statusFile, stdOut, printHeader);

	/* Debug默认是从屏幕显示而非日志文件 */
    if (statusFile) 
        ::fclose(statusFile);
}

/* 将入参totalBytes(总字节大小)以指定格式(分别以G/M/K/B为单位)存入缓存*outBuffer中 */
void FormattedTotalBytesBuffer(char *outBuffer, int outBufferLen, UInt64 totalBytes)
{
	/* 显示出的字节数 */
    Float32 displayBytes = 0.0;
	/* 容量大小的单位的字符串 */
    char  sizeStr[] = "B";
	/* 格式字符串 */
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

/* 屏幕显示输出9项多的指定信息 */
void PrintStatus(Bool16 printHeader)
{
    char* thePrefStr = NULL;
    UInt32 theLen = 0;
    
	/* 打印下面列出的几行信息,注意总字节数和时间以指定格式输出 */
    if ( printHeader )
    {                       
        qtss_printf("     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec    TotConn     TotBytes   TotPktsLost          Time\n");   
    }

	/* 获取RTP current conn信息,在屏幕上显示,并删去中间变量 */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurConn, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取RTSP current conn信息,在屏幕上显示,并删去中间变量 */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPCurrentSessionCount, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取RTSP-HTTP current conn信息,在屏幕上显示,并删去中间变量 */
    (void)QTSS_GetValueAsString(sServer, qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取RTP current bandwidth信息,在屏幕上显示 */
    UInt32 curBandwidth = 0;
    theLen = sizeof(curBandwidth);
    (void)QTSS_GetValue(sServer, qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
    qtss_printf( "%11lu", curBandwidth/1024);
    
	/* 获取RTP current获取的包数,在屏幕上显示,并删去中间变量 */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrCurPackets, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取RTP Total conn信息,在屏幕上显示,并删去中间变量 */
    (void)QTSS_GetValueAsString(sServer, qtssRTPSvrTotalConn, 0, &thePrefStr);
    qtss_printf( "%11s", thePrefStr);
    delete [] thePrefStr; thePrefStr = NULL;
    
	/* 获取发送的总RTP字节数 */
    UInt64 totalBytes = sServer->GetTotalRTPBytes();
	/* 存放指定格式的总RTP字节数的缓存 */
    char  displayBuff[32] = "";
    /* 将入参totalBytes(总字节大小)以指定格式(分别以G/M/K/B为单位)存入缓存中 */
    FormattedTotalBytesBuffer(displayBuff, sizeof(displayBuff),totalBytes);
    qtss_printf( "%17s", displayBuff);
    
	/* 得到总RTP丢包数 */
    qtss_printf( "%11"_64BITARG_"u", sServer->GetTotalRTPPacketsLost());
    
	/* 创建指定大小的缓存(30个字节) */
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	/* 将当前时间以local时间形式按指定格式存放在上述新建的缓存中 */
    (void) QTSSRollingLog::FormatDate(theDateBuffer, false);
	/* 输出指定格式的当前时间:YYYY-MM-DD HH:MM:SS */
    qtss_printf( "%25s",theDateBuffer);
    
    qtss_printf( "\n");
    
}

/* 每隔sStatusUpdateInterval * 10要打印一个Header行吗? */
Bool16 PrintHeader(UInt32 loopCount)
{
     return ( (loopCount % (sStatusUpdateInterval * 10) ) == 0 ) ? true : false;
}

/* 每隔sStatusUpdateInterval要打印一行吗? */
Bool16 PrintLine(UInt32 loopCount)
{
     return ( (loopCount % sStatusUpdateInterval) == 0 ) ? true : false;
}

/* 在整个运行过程中管理服务器 */
void RunServer()
{   
    Bool16 restartServer = false;
    UInt32 loopCount = 0;
    UInt32 debugLevel = 0;

	/* 分别接受从PrintHeader()和PrintLine()返回的值,见下面 */
    Bool16 printHeader = false;
    Bool16 printStatus = false;


    //just wait until someone stops the server or a fatal error occurs.
	/* 得到服务器的状态 */
    QTSS_ServerState theServerState = sServer->GetServerState();
    while ((theServerState != qtssShuttingDownState) &&
            (theServerState != qtssFatalErrorState))
    {
        OSThread::Sleep(1000);

		/* 记录服务器状态日志 */
        LogStatus(theServerState);

		/* 从第0行开始每10行循环显示/打印Header信息 */
        if (sStatusUpdateInterval)
        {
			/* 获得Debug级别 */
            debugLevel = sServer->GetDebugLevel(); 
			/* 接受下面两个函数的返回值 */
            printHeader = PrintHeader(loopCount);
            printStatus = PrintLine(loopCount);
             
			/* 在显示每行的前提下 */
            if (printStatus)
            {
				/* 是否显示Debug信息?注意Debug信息要么在屏幕上显示,要么记录在log里 */
                if  (DebugOn(sServer) ) // debug level display or logging is on
					/* 默认从屏幕而非日志文件显示Debug相关的指定信息"RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned  Time" */
                    DebugStatus(debugLevel, printHeader);
                
                if (!DebugDisplayOn(sServer))
					/* 屏幕显示输出9项多的指定信息,定义见上 */
                    PrintStatus(printHeader); // default status output
            }
            
            
            loopCount++;

        }
        
		/* 若要Interrupt或Terminate服务器 */
        if ((sServer->SigIntSet()) || (sServer->SigTermSet()))
        {
            //
            // start the shutdown process
			/* 设置服务器状态是qtssShuttingDownState */
            theServerState = qtssShuttingDownState;
            (void)QTSS_SetValue(QTSServerInterface::GetServer(), qtssSvrState, 0, &theServerState, sizeof(theServerState));

			/* 假如现在要Sigal Interrupt DSS,就设置可以重启服务器 */
			/* 注意terminate DSS,就不需这样 */
            if (sServer->SigIntSet())
                restartServer = true;
        }
        
		/* 重新获取服务器状态,继续循环要用 */
        theServerState = sServer->GetServerState();
        if (theServerState == qtssIdleState)
			/* 杀死所有的RTP Session */
            sServer->KillAllRTPSessions();
    }
    
    //
    // Kill all the sessions and wait for them to die,
    // but don't wait more than 5 seconds
    sServer->KillAllRTPSessions();
    for (UInt32 shutdownWaitCount = 0; (sServer->GetNumRTPSessions() > 0) && (shutdownWaitCount < 5); shutdownWaitCount++)
        OSThread::Sleep(1000);
        
    //Now, make sure that the server can't do any work
	/* 逐个发送停止请求,传信,删去各个线程 */
    TaskThreadPool::RemoveThreads();
    
    //now that the server is definitely stopped, it is safe to initate
    //the shutdown process
	/* 删去服务器对象 */
    delete sServer;
    
    CleanPid(false);
    //ok, we're ready to exit. If we're quitting because of some fatal error
    //while running the server, make sure to let the parent process know by
    //exiting with a nonzero status. Otherwise, exit with a 0 status
    if (theServerState == qtssFatalErrorState || restartServer)
        ::exit (-2);//-2 signals parent process to restart server
}
