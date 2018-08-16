
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  QTSServerInterface.cpp
Description: define an interface for getting and setting server-wide attributes, and storing global server resources.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef kVersionString
#include "revision.h"
#endif

#include "QTSServerInterface.h"
#include "RTPSessionInterface.h"
#include "RTPPacketResender.h"
#include "RTSPProtocol.h"
#include "OSRef.h"
#include "UDPSocketPool.h"


// STATIC DATA

UInt32                  QTSServerInterface::sServerAPIVersion = QTSS_API_VERSION;
QTSServerInterface*     QTSServerInterface::sServer = NULL;
StrPtrLen               QTSServerInterface::sServerNameStr("DSS");

// kVersionString from revision.h, include with -i at project level,来自revision.h
StrPtrLen               QTSServerInterface::sServerVersionStr(kVersionString);      //"5.5.1"
StrPtrLen               QTSServerInterface::sServerBuildStr(kBuildString);          //"489.8"
StrPtrLen               QTSServerInterface::sServerCommentStr(kCommentString);      //"Release/Darwin; "

StrPtrLen               QTSServerInterface::sServerPlatformStr(kPlatformNameString);//"Linux"
StrPtrLen               QTSServerInterface::sServerBuildDateStr(__DATE__ ", "__TIME__);
char                    QTSServerInterface::sServerHeader[kMaxServerHeaderLen];     //"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"
StrPtrLen               QTSServerInterface::sServerHeaderPtr(sServerHeader, kMaxServerHeaderLen);//设置参见QTSServerInterface::Initialize()

ResizeableStringFormatter       QTSServerInterface::sPublicHeaderFormatter(NULL, 0);
StrPtrLen                       QTSServerInterface::sPublicHeaderStr;//设置参见QTSServer::SetupPublicHeader()

QTSSModule**            QTSServerInterface::sModuleArray[QTSSModule::kNumRoles];/* 创建/删除参见QTSServer::BuildModuleRoleArrays()/DestroyModuleRoleArrays() */
UInt32                  QTSServerInterface::sNumModulesInRole[QTSSModule::kNumRoles];/* 注意该数组内嵌入上一个数组中,参见 QTSServerInterface::GetModule() */
OSQueue                 QTSServerInterface::sModuleQueue;//模块组成的队列,构成参见QTSServer::AddModule()
QTSSErrorLogStream      QTSServerInterface::sErrorLogStream;

/* 服务器的ConnectedUser属性,类似QTSSModule::sAttributes[] */
QTSSAttrInfoDict::AttrInfo  QTSServerInterface::sConnectedUserAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0  */ { "qtssConnectionType",                    NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 1  */ { "qtssConnectionCreateTimeInMsec",        NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 2  */ { "qtssConnectionTimeConnectedInMsec",     TimeConnected,  qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 3  */ { "qtssConnectionBytesSent",               NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 4  */ { "qtssConnectionMountPoint",              NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 5  */ { "qtssConnectionHostName",                NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe } ,

    /* 6  */ { "qtssConnectionSessRemoteAddrStr",       NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 7  */ { "qtssConnectionSessLocalAddrStr",        NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    
    /* 8  */ { "qtssConnectionCurrentBitRate",          NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 9  */ { "qtssConnectionPacketLossPercent",       NULL,   qtssAttrDataTypeFloat32,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    // this last parameter is a workaround for the current dictionary implementation.  For qtssConnectionTimeConnectedInMsec above we have a param
    // retrieval function.  This needs storage to keep the value returned, but if it sets its own param then the function no longer gets called.
    /* 10 */ { "qtssConnectionTimeStorage",             NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
};

/* 服务器的属性,参见QTSS_ServerAttributes in QTSS.h */
QTSSAttrInfoDict::AttrInfo  QTSServerInterface::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0  */ { "qtssServerAPIVersion",          NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1  */ { "qtssSvrDefaultDNSName",         NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 2  */ { "qtssSvrDefaultIPAddr",          NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 3  */ { "qtssSvrServerName",             NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4  */ { "qtssRTSPSvrServerVersion",      NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 5  */ { "qtssRTSPSvrServerBuildDate",    NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 6  */ { "qtssSvrRTSPPorts",              NULL,   qtssAttrDataTypeUInt16,     qtssAttrModeRead },
    /* 7  */ { "qtssSvrRTSPServerHeader",       NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 8  */ { "qtssSvrState",                  NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite  },
    /* 9  */ { "qtssSvrIsOutOfDescriptors",     IsOutOfDescriptors,     qtssAttrDataTypeBool16, qtssAttrModeRead },
    /* 10 */ { "qtssRTSPCurrentSessionCount",   NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 11 */ { "qtssRTSPHTTPCurrentSessionCount",NULL,  qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 12 */ { "qtssRTPSvrNumUDPSockets",       GetTotalUDPSockets,     qtssAttrDataTypeUInt32, qtssAttrModeRead },
    /* 13 */ { "qtssRTPSvrCurConn",             NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 14 */ { "qtssRTPSvrTotalConn",           NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 15 */ { "qtssRTPSvrCurBandwidth",        NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 16 */ { "qtssRTPSvrTotalBytes",          NULL,   qtssAttrDataTypeUInt64,     qtssAttrModeRead },
    /* 17 */ { "qtssRTPSvrAvgBandwidth",        NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 18 */ { "qtssRTPSvrCurPackets",          NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 19 */ { "qtssRTPSvrTotalPackets",        NULL,   qtssAttrDataTypeUInt64,     qtssAttrModeRead },
    /* 20 */ { "qtssSvrHandledMethods",         NULL,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe  },
    /* 21 */ { "qtssSvrModuleObjects",          NULL,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 22 */ { "qtssSvrStartupTime",            NULL,   qtssAttrDataTypeTimeVal,    qtssAttrModeRead },
    /* 23 */ { "qtssSvrGMTOffsetInHrs",         NULL,   qtssAttrDataTypeSInt32,     qtssAttrModeRead },
    /* 24 */ { "qtssSvrDefaultIPAddrStr",       NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead },
    /* 25 */ { "qtssSvrPreferences",            NULL,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModeInstanceAttrAllowed},
    /* 26 */ { "qtssSvrMessages",               NULL,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead },
    /* 27 */ { "qtssSvrClientSessions",         NULL,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead },
    /* 28 */ { "qtssSvrCurrentTimeMilliseconds",CurrentUnixTimeMilli,   qtssAttrDataTypeTimeVal,qtssAttrModeRead},
    /* 29 */ { "qtssSvrCPULoadPercent",         NULL,   qtssAttrDataTypeFloat32,    qtssAttrModeRead},
    /* 30 */ { "qtssSvrNumReliableUDPBuffers",  GetNumUDPBuffers,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
    /* 31 */ { "qtssSvrReliableUDPWastageInBytes",GetNumWastedBytes, qtssAttrDataTypeUInt32,        qtssAttrModeRead },
    /* 32 */ { "qtssSvrConnectedUsers",         NULL, qtssAttrDataTypeQTSS_Object,      qtssAttrModeRead | qtssAttrModeWrite },
    /* 33 */ { "qtssMP3SvrCurConn",             NULL, qtssAttrDataTypeUInt32,       qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 34 */ { "qtssMP3SvrTotalConn",           NULL, qtssAttrDataTypeUInt32,       qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 35 */ { "qtssMP3SvrCurBandwidth",        NULL, qtssAttrDataTypeUInt32,       qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 36 */ { "qtssMP3SvrTotalBytes",          NULL, qtssAttrDataTypeUInt64,       qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 37 */ { "qtssMP3SvrAvgBandwidth",        NULL, qtssAttrDataTypeUInt32,       qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },

    /* 38  */ { "qtssSvrServerBuild",           NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 39  */ { "qtssSvrServerPlatform",        NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 40  */ { "qtssSvrRTSPServerComment",     NULL,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 41  */ { "qtssSvrNumThinned",            NULL,   qtssAttrDataTypeSInt32,     qtssAttrModeRead | qtssAttrModeWrite  }
};

/* 搭建kServerDictIndex和kQTSSConnectedUserDictIndex字典的属性,给出DSS的头信息"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )" */
void    QTSServerInterface::Initialize()
{   //搭建kServerDictIndex字典的属性
    for (UInt32 x = 0; x < qtssSvrNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);

	//搭建kQTSSConnectedUserDictIndex字典的属性
    for (UInt32 y = 0; y < qtssConnectionNumParams; y++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kQTSSConnectedUserDictIndex)->
            SetAttribute(y, sConnectedUserAttributes[y].fAttrName, sConnectedUserAttributes[y].fFuncPtr,
                sConnectedUserAttributes[y].fAttrDataType, sConnectedUserAttributes[y].fAttrPermission);

    //Write out a premade server header "Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"
    StringFormatter serverFormatter(sServerHeaderPtr.Ptr, kMaxServerHeaderLen);//对这1000个字符进行内存管理
    serverFormatter.Put(RTSPProtocol::GetHeaderString(qtssServerHeader));
    serverFormatter.Put(": ");
    serverFormatter.Put(sServerNameStr);
    serverFormatter.PutChar('/');
    serverFormatter.Put(sServerVersionStr);
    serverFormatter.PutChar(' ');

    serverFormatter.PutChar('(');
    serverFormatter.Put("Build/");
    serverFormatter.Put(sServerBuildStr);
    serverFormatter.Put("; ");
    serverFormatter.Put("Platform/");
    serverFormatter.Put(sServerPlatformStr);
    serverFormatter.PutChar(';');
 
    if (sServerCommentStr.Len > 0)
    {
        serverFormatter.PutChar(' ');
        serverFormatter.Put(sServerCommentStr);
    }
    serverFormatter.PutChar(')');


    sServerHeaderPtr.Len = serverFormatter.GetCurrentOffset();//累计存入缓存的字节数
    Assert(sServerHeaderPtr.Len < kMaxServerHeaderLen);
}

QTSServerInterface::QTSServerInterface()
 :  QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex), &fMutex),
    fSocketPool(NULL),
    fRTPMap(NULL),
    fSrvrPrefs(NULL),
    fSrvrMessages(NULL),
    fServerState(qtssStartingUpState),/* 服务器启动状态 */
    fDefaultIPAddr(0),
    fListeners(NULL), /* TCPListenerSocket数组指针 */
    fNumListeners(0), /* 上述数组的分量个数 */
    fStartupTime_UnixMilli(0),
    fGMTOffset(0),
    fNumRTSPSessions(0),
    fNumRTSPHTTPSessions(0),
    fNumRTPSessions(0),
    fNumRTPPlayingSessions(0),
    fTotalRTPSessions(0),
    fTotalRTPBytes(0),
    fTotalRTPPackets(0),
    fTotalRTPPacketsLost(0),
    fPeriodicRTPBytes(0),
    fPeriodicRTPPacketsLost(0),
    fPeriodicRTPPackets(0),
    fCurrentRTPBandwidthInBits(0),
    fAvgRTPBandwidthInBits(0),
    fRTPPacketsPerSecond(0),
    fCPUPercent(0),
    fCPUTimeUsedInSec(0),
    fUDPWastageInBytes(0),/* UDPSocketPair池中的未使用的字节数(也即OSBufferPool中的) */
    fNumUDPBuffers(0), /* UDPSocketPair池中的分量个数 */
    fNumMP3Sessions(0),
    fTotalMP3Sessions(0),
    fCurrentMP3BandwidthInBits(0),
    fTotalMP3Bytes(0),
    fAvgMP3BandwidthInBits(0),
    fSigInt(false),
    fSigTerm(false),
    fDebugLevel(0),   /* 默认都是0级 */
    fDebugOptions(0), /* 默认都是0级 */   
    fMaxLate(0),
    fTotalLate(0),
    fCurrentMaxLate(0),
    fTotalQuality(0),
    fNumThinned(0)
{
	/* 初始化关于Role的module array和NumModulesInRole数组,注意后者嵌套在前者中,两者紧密联系 */
    for (UInt32 y = 0; y < QTSSModule::kNumRoles; y++)
    {
        sModuleArray[y] = NULL;//QTSSModule**
        sNumModulesInRole[y] = 0;
    }

	/* 搭建kServerDictIndex字典属性值,部分被Param retrieval functions设置,此处未被设置 */
    this->SetVal(qtssServerAPIVersion,      &sServerAPIVersion,         sizeof(sServerAPIVersion));         //0
    this->SetVal(qtssSvrDefaultIPAddr,      &fDefaultIPAddr,            sizeof(fDefaultIPAddr));            //2
    this->SetVal(qtssSvrServerName,         sServerNameStr.Ptr,         sServerNameStr.Len);                //3
    this->SetVal(qtssSvrServerVersion,      sServerVersionStr.Ptr,      sServerVersionStr.Len);             //4
    this->SetVal(qtssSvrServerBuildDate,    sServerBuildDateStr.Ptr,    sServerBuildDateStr.Len);           //5
    this->SetVal(qtssSvrRTSPServerHeader,   sServerHeaderPtr.Ptr,       sServerHeaderPtr.Len);              //7
	this->SetVal(qtssSvrState,              &fServerState,              sizeof(fServerState));              //8
    this->SetVal(qtssRTSPCurrentSessionCount, &fNumRTSPSessions,        sizeof(fNumRTSPSessions));          //10
    this->SetVal(qtssRTSPHTTPCurrentSessionCount, &fNumRTSPHTTPSessions,sizeof(fNumRTSPHTTPSessions));      //11
    this->SetVal(qtssRTPSvrCurConn,         &fNumRTPSessions,           sizeof(fNumRTPSessions));           //13
    this->SetVal(qtssRTPSvrTotalConn,       &fTotalRTPSessions,         sizeof(fTotalRTPSessions));         //14
    this->SetVal(qtssRTPSvrCurBandwidth,    &fCurrentRTPBandwidthInBits,sizeof(fCurrentRTPBandwidthInBits));//15
    this->SetVal(qtssRTPSvrTotalBytes,      &fTotalRTPBytes,            sizeof(fTotalRTPBytes));            //16
    this->SetVal(qtssRTPSvrAvgBandwidth,    &fAvgRTPBandwidthInBits,    sizeof(fAvgRTPBandwidthInBits));    //17
    this->SetVal(qtssRTPSvrCurPackets,      &fRTPPacketsPerSecond,      sizeof(fRTPPacketsPerSecond));      //18
    this->SetVal(qtssRTPSvrTotalPackets,    &fTotalRTPPackets,          sizeof(fTotalRTPPackets));          //19
    this->SetVal(qtssSvrStartupTime,        &fStartupTime_UnixMilli,    sizeof(fStartupTime_UnixMilli));    //22
    this->SetVal(qtssSvrGMTOffsetInHrs,     &fGMTOffset,                sizeof(fGMTOffset));                //23
    this->SetVal(qtssSvrCPULoadPercent,     &fCPUPercent,               sizeof(fCPUPercent));               //29
    this->SetVal(qtssMP3SvrCurConn,         &fNumMP3Sessions,           sizeof(fNumMP3Sessions));           //33
    this->SetVal(qtssMP3SvrTotalConn,       &fTotalMP3Sessions,         sizeof(fTotalMP3Sessions));         //34
    this->SetVal(qtssMP3SvrCurBandwidth,    &fCurrentMP3BandwidthInBits,sizeof(fCurrentMP3BandwidthInBits));//35
    this->SetVal(qtssMP3SvrTotalBytes,      &fTotalMP3Bytes,            sizeof(fTotalMP3Bytes));            //36
    this->SetVal(qtssMP3SvrAvgBandwidth,    &fAvgMP3BandwidthInBits,    sizeof(fAvgMP3BandwidthInBits));    //37

    this->SetVal(qtssSvrServerBuild,        sServerBuildStr.Ptr,        sServerBuildStr.Len);               //38
    this->SetVal(qtssSvrServerPlatform,     sServerPlatformStr.Ptr,     sServerPlatformStr.Len);            //39
    this->SetVal(qtssSvrRTSPServerComment,  sServerCommentStr.Ptr,      sServerCommentStr.Len);             //40
    this->SetVal(qtssSvrNumThinned,         &fNumThinned,               sizeof(fNumThinned));               //41
    
    /* 初始化指向QTSServerInterface类的指针,就是本实例 */
    sServer = this;
}

/* 调用注册记录错误日志的Module,去写指定级别的指定缓存的数据入日志,在日志处于Fatal级别时,设置服务器的状态属性值为Fatal */
void QTSServerInterface::LogError(QTSS_ErrorVerbosity inVerbosity, char* inBuffer)
{
    QTSS_RoleParams theParams;
    theParams.errorParams.inVerbosity = inVerbosity;//日志级别
    theParams.errorParams.inBuffer = inBuffer; //要写入日志的缓存的数据

    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kErrorLogRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kErrorLogRole, x)->CallDispatch(QTSS_ErrorLog_Role, &theParams);

    // If this is a fatal error, set the proper attribute in the RTSPServer dictionary
    if ((inVerbosity == qtssFatalVerbosity) && (sServer != NULL))
    {
        QTSS_ServerState theState = qtssFatalErrorState;
        (void)sServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
    }
}

/* 遍历当前RTPSession Map中的所有RTPSession, 发送杀死事件去通知任务线程杀死所有的RTP Session */
void QTSServerInterface::KillAllRTPSessions()
{
	/* 获取Hash Table的互斥锁 */
    OSMutexLocker locker(fRTPMap->GetMutex());
    for (OSRefHashTableIter theIter(fRTPMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
    {
		/* 获取当前的Hash Table Elem */
        OSRef* theRef = theIter.GetCurrent();
		/* 获取当前的Hash Table Elem所在的类对象指针 */
        RTPSessionInterface* theSession = (RTPSessionInterface*)theRef->GetObject();
		/* 发信息去关闭RTPSession */
        theSession->Signal(Task::kKillEvent);
    }   
}

/* 用入参设置服务器新状态,并调用注册状态改变角色的模块去完成任务 */
void QTSServerInterface::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
    if (inAttrIndex == qtssSvrState)
    {
        Assert(inNewValueLen == sizeof(QTSS_ServerState));
        
        //
        // Invoke the server state change role
        QTSS_RoleParams theParams;
        theParams.stateChangeParams.inNewState = *(QTSS_ServerState*)inNewValue; //入参设置服务器新状态
        
		/* 设置线程私有参数 */
        static QTSS_ModuleState sStateChangeState = { NULL, 0, NULL, false };
        if (OSThread::GetCurrent() == NULL)
            OSThread::SetMainThreadData(&sStateChangeState);
        else
            OSThread::GetCurrent()->SetThreadData(&sStateChangeState);

		/* 调用注册状态改变角色的模块去完成任务 */
        UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kStateChangeRole);
        {
            for (UInt32 theCurrentModule = 0; theCurrentModule < numModules; theCurrentModule++)
            {  
                QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kStateChangeRole, theCurrentModule);
                (void)theModule->CallDispatch(QTSS_StateChange_Role, &theParams);
            }
        }

        // 清空线程私有数据
        // Make sure to clear out the thread data
        if (OSThread::GetCurrent() == NULL)
            OSThread::SetMainThreadData(NULL);
        else
            OSThread::GetCurrent()->SetThreadData(NULL);
    }
}


/************************************* 以下是RTPStatsUpdaterTask类 ************************************/

RTPStatsUpdaterTask::RTPStatsUpdaterTask()
:   Task(), fLastBandwidthTime(0), fLastBandwidthAvg(0), fLastBytesSent(0), fLastTotalMP3Bytes(0)
{
    this->SetTaskName("RTPStatsUpdaterTask");
	/* 将RTPStatsUpdaterTask放入指定任务线程的任务队列 */
    this->Signal(Task::kStartEvent);
}

/* 遍历当前RTPSession map中的所有RTPSession,返回最后一个连接上(就是会话生成时间最大)的一个RTPSession(实际上是RTPSessionInterface) */
RTPSessionInterface* RTPStatsUpdaterTask::GetNewestSession(OSRefTable* inRTPSessionMap)
{
	//Caller must lock down the RTP session map
	SInt64 theNewestPlayTime = 0;
	RTPSessionInterface* theNewestSession = NULL;

	//use the session map to iterate through all the sessions, finding the most
	//recently connected client
	/* 迭代RTPSession map,直到遇到一个空的Hash Table Elem才停下 */
	for (OSRefHashTableIter theIter(inRTPSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		/* 得到当前的非空的Hash Table Elem */
		OSRef* theRef = theIter.GetCurrent();
		/* 得到该非空当前元的对象 */
		RTPSessionInterface* theSession = (RTPSessionInterface*)theRef->GetObject();
		/* 获取当前会话的生成时间 */
		Assert(theSession->GetSessionCreateTime() > 0);
		/* 更新当前Session并返回 */
		if (theSession->GetSessionCreateTime() > theNewestPlayTime)
		{
			theNewestPlayTime = theSession->GetSessionCreateTime();
			theNewestSession = theSession;
		}
	}
	return theNewestSession;
}

/* 获取并返回当前进程的cpu 时间 */
Float32 RTPStatsUpdaterTask::GetCPUTimeInSeconds()
{
    // This function returns the total number of seconds that the
    // process running RTPStatsUpdaterTask() has been executing as
    // a user process.
    Float32 cpuTimeInSec = 0.0;

    // The UNIX way of getting the time for this process
    clock_t cpuTime = clock();
    cpuTimeInSec = (Float32) cpuTime / CLOCKS_PER_SEC;

    return cpuTimeInSec;
}

/* 该任务每秒运行1次,注意它会配置4个数据成员的值: */
SInt64 RTPStatsUpdaterTask::Run()
{
    /* 首先获取静态QTSServerInterface类的指针,由它可以查询服务器相关的属性参数 */
    QTSServerInterface* theServer = QTSServerInterface::sServer;
    
    // All of this must happen atomically wrt dictionary values we are manipulating
    OSMutexLocker locker(&theServer->fMutex);
    
    //First update total bytes. This must be done because total bytes is a 64 bit number,
    //so no atomic functions can apply.
    //
    // NOTE: The line below is not thread safe on non-PowerPC platforms. This is
    // because the fPeriodicRTPBytes variable is being manipulated from within an
    // atomic_add. On PowerPC, assignments are atomic, so the assignment below is ok.
    // On a non-PowerPC platform, the following would be thread safe:
    //unsigned int periodicBytes = atomic_add(&theServer->fPeriodicRTPBytes, 0);

	/********************** 注意下面三项的处理是相同的 *********************************************/

    unsigned int periodicBytes = theServer->fPeriodicRTPBytes;
    (void)atomic_sub(&theServer->fPeriodicRTPBytes, periodicBytes);
    theServer->fTotalRTPBytes += periodicBytes;
    
    // Same deal for packet totals
    unsigned int periodicPackets = theServer->fPeriodicRTPPackets;/* 在一个周期(单位待定)内的RTP包数 */
    (void)atomic_sub(&theServer->fPeriodicRTPPackets, periodicPackets);
    theServer->fTotalRTPPackets += periodicPackets;
    
    // ..and for lost packet totals
    unsigned int periodicPacketsLost = theServer->fPeriodicRTPPacketsLost;
    (void)atomic_sub(&theServer->fPeriodicRTPPacketsLost, periodicPacketsLost);
    theServer->fTotalRTPPacketsLost += periodicPacketsLost;

	/********************** 注意下面三项的处理是相同的 ***********************************************/
    
	/* 获得服务器的当前时间(ms),注意是16字节的,这个值非常重要! */
    SInt64 curTime = OS::Milliseconds();
    
    //for cpu percent
	/* 获取当前进程的cpu时间(s) */
        Float32 cpuTimeInSec = GetCPUTimeInSeconds();
    
    //also update current bandwidth statistic
	/* 假如计算上次带宽的时间非零,计算当前带宽比特数fCurrentRTPBandwidthInBits,fCurrentMP3BandwidthInBits,fCPUPercent */
    if (fLastBandwidthTime != 0)
    {
        Assert(curTime > fLastBandwidthTime);
		/* 注意差值是8字节的 */
        UInt32 delta = (UInt32)(curTime - fLastBandwidthTime);/* 当前时间和上次更新时间的差值(ms) */
		/* 超过一秒就给出警告信息:"Timer is off" */
        if (delta < 1000)
            WarnV(delta >= 1000, "Timer is off");
          

        UInt32 packetsPerSecond = periodicPackets;
        UInt32 theTime = delta / 1000;/* 时间差值(s) */
 
		/* 每秒送出的RTP包数 */
        packetsPerSecond /= theTime;
        Assert(packetsPerSecond >= 0);
		/* 每秒的RTP Packet总数 */
        theServer->fRTPPacketsPerSecond = packetsPerSecond;
		/* 每秒每个packet要附加的字节数是28,总共要附加的字节数是? */
        UInt32 additionalBytes = 28 * packetsPerSecond; // IP headers( = 20) + UDP headers( = 8)
        /* 每个packet要附加的比特数是28*8,总共要附加的比特数是? */
        UInt32 headerBits = 8 * additionalBytes;/* 由字节转化为bit */
		/* 每秒要附加的头bit数 */
		/* 为何要再除以theTime */
        headerBits  /= theTime;
      
		/* 计算当前RTP包的带宽比特数 */
        Float32 bits = periodicBytes * 8;
        bits /= theTime;
		/* 配置服务器当前带宽比特数 */
        theServer->fCurrentRTPBandwidthInBits = (UInt32) (bits + headerBits);
         
        // okay let's do it for MP3 bytes now
		/* 计算前后两次MP3比特数差值 */
        bits = (Float32)(((SInt64)theServer->fTotalMP3Bytes - fLastTotalMP3Bytes) * 8);
        bits /= theTime;
		/* 配置服务器MP3带宽比特数 */
        theServer->fCurrentMP3BandwidthInBits = (UInt32)bits;


        //do the computation for cpu percent
		/* 为单核CPU计算CPU使用时间百分比 */
        Float32 diffTime = cpuTimeInSec - theServer->fCPUTimeUsedInSec;
        theServer->fCPUPercent = (diffTime/theTime) * 100;  /* 通过时间计算CPU的使用率 */
		
		/* 得到处理器个数 */
		UInt32 numProcessors = OS::GetNumProcessors();
		
		/* 对多核CPU,计算CPU使用时间百分比 */
		if (numProcessors > 1)
			theServer->fCPUPercent /= numProcessors;
    }

    
	/********** 计算下面这四个量都很重要!! ***********/

	/* 注意这两个是成员变量值 */
    fLastTotalMP3Bytes = (SInt64)theServer->fTotalMP3Bytes;
    fLastBandwidthTime = curTime;


    // We use a running average for avg. bandwidth calculations
	/* 注意计算avg MP3 bandwidth的算法 */
    theServer->fAvgMP3BandwidthInBits = (theServer->fAvgMP3BandwidthInBits
            + theServer->fCurrentMP3BandwidthInBits)/2; 
    
    //for cpu percent
	/* 保存上次上个进程使用CPU的时间 */
    theServer->fCPUTimeUsedInSec    = cpuTimeInSec; 
    
    //also compute average bandwidth, a much more smooth value. This is done with
    //the fLastBandwidthAvg, a timestamp of the last time we did an average, and
    //fLastBytesSent, the number of bytes sent when we last did an average.
	/* 配置值"average_bandwidth_update"是60s,注意fLastBandwidthAvg指上次计算平均带宽的时间 */
	/* 当上次计算平均带宽的时间非零,且当前时间超过上次计算平均带宽的时间+更新间隔60s */
    if ((fLastBandwidthAvg != 0) && (curTime > (fLastBandwidthAvg +
        (theServer->GetPrefs()->GetAvgBandwidthUpdateTimeInSecs() * 1000))))
    {
		/* 计算前后两次更新时间间隔 */
        UInt32 delta = (UInt32)(curTime - fLastBandwidthAvg);
		/* 当前间隔内送出的总字节数(用现在送出的总字节-上次更新时送出的字节数) */
        SInt64 bytesSent = theServer->fTotalRTPBytes - fLastBytesSent;
        Assert(bytesSent >= 0);
        
        //do the bandwidth computation using floating point divides
        //for accuracy and speed.
		/* 当前送出的总比特数 */
        Float32 bits = (Float32)(bytesSent * 8);
        Float32 theAvgTime = (Float32)delta;
		/* 距上次以来的时间间隔(秒) */
        theAvgTime /= 1000;
		/* 当前间隔内每秒送出的总比特数 */
        bits /= theAvgTime;
        Assert(bits >= 0);
		/* 用当前间隔内每秒送出的总比特数配置当前平均RTP Bandwidth(比特) */
		/***********  NOTE:此量非常重要 ***************************/
        theServer->fAvgRTPBandwidthInBits = (UInt32)bits;
		/***********  NOTE:此量非常重要 ***************************/

		/* 为了下次循环使用 */
		/**************** NOTE!! ********************/
		/* 记录当前时间戳 */
        fLastBandwidthAvg = curTime;
		/* 记录当前发出的RTP包字节总数 */
        fLastBytesSent = theServer->fTotalRTPBytes;
		/**************** NOTE!! ********************/
        
        //if the bandwidth is above the bandwidth setting, disconnect 1 user by sending them
        //a BYE RTCP packet.
		//获取maximum_bandwidth配置值(102400Kbit/s,即100M小b)
        SInt32 maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
		/* 假如上面计算的theServer->fAvgRTPBandwidthInBits的值超过配置值(100M小b),就断开该点播连接 */
        if ((maxKBits > -1) && (theServer->fAvgRTPBandwidthInBits > ((UInt32)maxKBits * 1024)))
        {
            //we need to make sure that all of this happens atomically wrt the session map
			/* 锁定该RTPSession Map */
            OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
			/* 获得该RTPSession Map中最近的一个RTPSession */
            RTPSessionInterface* theSession = this->GetNewestSession(theServer->fRTPMap);
            if (theSession != NULL)
				/* 断开最后10分钟内接进来的最近一次的RTPSession */
                if ((curTime - theSession->GetSessionCreateTime()) <
                        theServer->GetPrefs()->GetSafePlayDurationInSecs() * 1000)    //安全播放不会断掉的时间值是10分钟
                    theSession->Signal(Task::kKillEvent);
        }
    }
	/* 当上次计算平均带宽的时间为零,说明是起始情况,仅需记录下面这两个量以循环使用. */
    else if (fLastBandwidthAvg == 0)
    {
		/**************** NOTE!! ********************/
		/* 记录当前时间戳 */
        fLastBandwidthAvg = curTime;
        fLastBytesSent = theServer->fTotalRTPBytes;
		/**************** NOTE!! ********************/
    }
    
    (void)this->GetEvents();//we must clear the event mask!
	/* 配置值"total_bytes_update"为1s  */
    return theServer->GetPrefs()->GetTotalBytesUpdateTimeInSecs() * 1000;
}


/********************************* 以上是RTPStatsUpdaterTask类 ********************************/

/********************************* 以下是Param retrieval functions for ServerDict ********************************/

/* 设置并返回当前操作系统时间的Unix时间(ms) */
void* QTSServerInterface::CurrentUnixTimeMilli(QTSSDictionary* inServer, UInt32* outLen)
{
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    theServer->fCurrentTime_UnixMilli = OS::TimeMilli_To_UnixTimeMilli(OS::Milliseconds()); 
    
    // Return the result
    *outLen = sizeof(theServer->fCurrentTime_UnixMilli);
    return &theServer->fCurrentTime_UnixMilli;
}

/* 设置并返回RTP/RTCP Socket pair队列中的UDP总数 */
void* QTSServerInterface::GetTotalUDPSockets(QTSSDictionary* inServer, UInt32* outLen)
{
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    // Multiply by 2 because this is returning the number of socket *pairs*
    theServer->fTotalUDPSockets = theServer->fSocketPool->GetSocketQueue()->GetLength() * 2;
    
    // Return the result
    *outLen = sizeof(theServer->fTotalUDPSockets);
    return &theServer->fTotalUDPSockets;
}

/* 遍历侦听套接字数组TCPListenerSocke中的每个分量,判断其文件描述符是否用光? 若有一个用光,就设置并返回theServer->fIsOutOfDescriptors */
void* QTSServerInterface::IsOutOfDescriptors(QTSSDictionary* inServer, UInt32* outLen)
{
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    
    theServer->fIsOutOfDescriptors = false;
    for (UInt32 x = 0; x < theServer->fNumListeners; x++)
    {
        if (theServer->fListeners[x]->IsOutOfDescriptors())
        {
            theServer->fIsOutOfDescriptors = true;
            break;
        }
    }
    // Return the result
    *outLen = sizeof(theServer->fIsOutOfDescriptors);
    return &theServer->fIsOutOfDescriptors;
}

/* 通过查询缓存池OSBufferPool中定长缓存的个数,来设置并返回fNumUDPBuffers的个数 */
void* QTSServerInterface::GetNumUDPBuffers(QTSSDictionary* inServer, UInt32* outLen)
{
    // This param retrieval function must be invoked each time it is called,
    // because whether we are out of descriptors or not is continually changing
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    
	//将所有的UDP buffer放入OSBufferPool中进行管理,现在获取该缓存池中定长缓存的个数
    theServer->fNumUDPBuffers = RTPPacketResender::GetNumRetransmitBuffers();

    // Return the result
    *outLen = sizeof(theServer->fNumUDPBuffers);
    return &theServer->fNumUDPBuffers;
}

/* 累计缓存池OSBufferPool中未使用的缓存字节总数,设置并返回fUDPWastageInBytes总数  */
void* QTSServerInterface::GetNumWastedBytes(QTSSDictionary* inServer, UInt32* outLen)
{
    // This param retrieval function must be invoked each time it is called,
    // because whether we are out of descriptors or not is continually changing
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    
	//分析缓存池OSBufferPool中定长缓存的使用情况,累计未使用的缓存字节总数
    theServer->fUDPWastageInBytes = RTPPacketResender::GetWastedBufferBytes();

    // Return the result
    *outLen = sizeof(theServer->fUDPWastageInBytes);
    return &theServer->fUDPWastageInBytes;  
}

/********************************* 以上是Param retrieval functions for ServerDict ********************************/

/* 首先获取连接生成时间戳,并与当前时间戳做差,存为连接时长(ms)属性,再读取该属性值并返回。注意第一个入参是QTSSConnectedUserDict */
void* QTSServerInterface::TimeConnected(QTSSDictionary* inConnection, UInt32* outLen)
{
    SInt64 connectTime;
    void* result;
    UInt32 len = sizeof(connectTime);
    inConnection->GetValue(qtssConnectionCreateTimeInMsec, 0, &connectTime, &len);
    SInt64 timeConnected = OS::Milliseconds() - connectTime;
    *outLen = sizeof(timeConnected);
    inConnection->SetValue(qtssConnectionTimeStorage, 0, &timeConnected, sizeof(connectTime));
    inConnection->GetValuePtr(qtssConnectionTimeStorage, 0, &result, outLen);

    // Return the result
    return result;
}

/* 调用模块将指定缓存的数据,以指定级别记入日志 */
QTSS_Error  QTSSErrorLogStream::Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags)
{
    // For the error log stream, the flags are considered to be the verbosity of the error.
    // 当日志级别超过5时,使它回复到2
    if (inFlags >= qtssIllegalVerbosity)
        inFlags = qtssMessageVerbosity;
        
    QTSServerInterface::LogError(inFlags, (char*)inBuffer);
    if (outLenWritten != NULL)
        *outLenWritten = inLen;
        
    return QTSS_NoErr;
}

/* 调用模块记录指定的Assert级别的日志信息 */
void QTSSErrorLogStream::LogAssert(char* inMessage)
{
    QTSServerInterface::LogError(qtssAssertVerbosity, inMessage);
}


