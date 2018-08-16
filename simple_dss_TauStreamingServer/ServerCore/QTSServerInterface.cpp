
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

// kVersionString from revision.h, include with -i at project level,����revision.h
StrPtrLen               QTSServerInterface::sServerVersionStr(kVersionString);      //"5.5.1"
StrPtrLen               QTSServerInterface::sServerBuildStr(kBuildString);          //"489.8"
StrPtrLen               QTSServerInterface::sServerCommentStr(kCommentString);      //"Release/Darwin; "

StrPtrLen               QTSServerInterface::sServerPlatformStr(kPlatformNameString);//"Linux"
StrPtrLen               QTSServerInterface::sServerBuildDateStr(__DATE__ ", "__TIME__);
char                    QTSServerInterface::sServerHeader[kMaxServerHeaderLen];     //"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"
StrPtrLen               QTSServerInterface::sServerHeaderPtr(sServerHeader, kMaxServerHeaderLen);//���òμ�QTSServerInterface::Initialize()

ResizeableStringFormatter       QTSServerInterface::sPublicHeaderFormatter(NULL, 0);
StrPtrLen                       QTSServerInterface::sPublicHeaderStr;//���òμ�QTSServer::SetupPublicHeader()

QTSSModule**            QTSServerInterface::sModuleArray[QTSSModule::kNumRoles];/* ����/ɾ���μ�QTSServer::BuildModuleRoleArrays()/DestroyModuleRoleArrays() */
UInt32                  QTSServerInterface::sNumModulesInRole[QTSSModule::kNumRoles];/* ע���������Ƕ����һ��������,�μ� QTSServerInterface::GetModule() */
OSQueue                 QTSServerInterface::sModuleQueue;//ģ����ɵĶ���,���ɲμ�QTSServer::AddModule()
QTSSErrorLogStream      QTSServerInterface::sErrorLogStream;

/* ��������ConnectedUser����,����QTSSModule::sAttributes[] */
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

/* ������������,�μ�QTSS_ServerAttributes in QTSS.h */
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

/* �kServerDictIndex��kQTSSConnectedUserDictIndex�ֵ������,����DSS��ͷ��Ϣ"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )" */
void    QTSServerInterface::Initialize()
{   //�kServerDictIndex�ֵ������
    for (UInt32 x = 0; x < qtssSvrNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);

	//�kQTSSConnectedUserDictIndex�ֵ������
    for (UInt32 y = 0; y < qtssConnectionNumParams; y++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kQTSSConnectedUserDictIndex)->
            SetAttribute(y, sConnectedUserAttributes[y].fAttrName, sConnectedUserAttributes[y].fFuncPtr,
                sConnectedUserAttributes[y].fAttrDataType, sConnectedUserAttributes[y].fAttrPermission);

    //Write out a premade server header "Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"
    StringFormatter serverFormatter(sServerHeaderPtr.Ptr, kMaxServerHeaderLen);//����1000���ַ������ڴ����
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


    sServerHeaderPtr.Len = serverFormatter.GetCurrentOffset();//�ۼƴ��뻺����ֽ���
    Assert(sServerHeaderPtr.Len < kMaxServerHeaderLen);
}

QTSServerInterface::QTSServerInterface()
 :  QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex), &fMutex),
    fSocketPool(NULL),
    fRTPMap(NULL),
    fSrvrPrefs(NULL),
    fSrvrMessages(NULL),
    fServerState(qtssStartingUpState),/* ����������״̬ */
    fDefaultIPAddr(0),
    fListeners(NULL), /* TCPListenerSocket����ָ�� */
    fNumListeners(0), /* ��������ķ������� */
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
    fUDPWastageInBytes(0),/* UDPSocketPair���е�δʹ�õ��ֽ���(Ҳ��OSBufferPool�е�) */
    fNumUDPBuffers(0), /* UDPSocketPair���еķ������� */
    fNumMP3Sessions(0),
    fTotalMP3Sessions(0),
    fCurrentMP3BandwidthInBits(0),
    fTotalMP3Bytes(0),
    fAvgMP3BandwidthInBits(0),
    fSigInt(false),
    fSigTerm(false),
    fDebugLevel(0),   /* Ĭ�϶���0�� */
    fDebugOptions(0), /* Ĭ�϶���0�� */   
    fMaxLate(0),
    fTotalLate(0),
    fCurrentMaxLate(0),
    fTotalQuality(0),
    fNumThinned(0)
{
	/* ��ʼ������Role��module array��NumModulesInRole����,ע�����Ƕ����ǰ����,���߽�����ϵ */
    for (UInt32 y = 0; y < QTSSModule::kNumRoles; y++)
    {
        sModuleArray[y] = NULL;//QTSSModule**
        sNumModulesInRole[y] = 0;
    }

	/* �kServerDictIndex�ֵ�����ֵ,���ֱ�Param retrieval functions����,�˴�δ������ */
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
    
    /* ��ʼ��ָ��QTSServerInterface���ָ��,���Ǳ�ʵ�� */
    sServer = this;
}

/* ����ע���¼������־��Module,ȥдָ�������ָ���������������־,����־����Fatal����ʱ,���÷�������״̬����ֵΪFatal */
void QTSServerInterface::LogError(QTSS_ErrorVerbosity inVerbosity, char* inBuffer)
{
    QTSS_RoleParams theParams;
    theParams.errorParams.inVerbosity = inVerbosity;//��־����
    theParams.errorParams.inBuffer = inBuffer; //Ҫд����־�Ļ��������

    for (UInt32 x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kErrorLogRole); x++)
        (void)QTSServerInterface::GetModule(QTSSModule::kErrorLogRole, x)->CallDispatch(QTSS_ErrorLog_Role, &theParams);

    // If this is a fatal error, set the proper attribute in the RTSPServer dictionary
    if ((inVerbosity == qtssFatalVerbosity) && (sServer != NULL))
    {
        QTSS_ServerState theState = qtssFatalErrorState;
        (void)sServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
    }
}

/* ������ǰRTPSession Map�е�����RTPSession, ����ɱ���¼�ȥ֪ͨ�����߳�ɱ�����е�RTP Session */
void QTSServerInterface::KillAllRTPSessions()
{
	/* ��ȡHash Table�Ļ����� */
    OSMutexLocker locker(fRTPMap->GetMutex());
    for (OSRefHashTableIter theIter(fRTPMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
    {
		/* ��ȡ��ǰ��Hash Table Elem */
        OSRef* theRef = theIter.GetCurrent();
		/* ��ȡ��ǰ��Hash Table Elem���ڵ������ָ�� */
        RTPSessionInterface* theSession = (RTPSessionInterface*)theRef->GetObject();
		/* ����Ϣȥ�ر�RTPSession */
        theSession->Signal(Task::kKillEvent);
    }   
}

/* ��������÷�������״̬,������ע��״̬�ı��ɫ��ģ��ȥ������� */
void QTSServerInterface::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
    if (inAttrIndex == qtssSvrState)
    {
        Assert(inNewValueLen == sizeof(QTSS_ServerState));
        
        //
        // Invoke the server state change role
        QTSS_RoleParams theParams;
        theParams.stateChangeParams.inNewState = *(QTSS_ServerState*)inNewValue; //������÷�������״̬
        
		/* �����߳�˽�в��� */
        static QTSS_ModuleState sStateChangeState = { NULL, 0, NULL, false };
        if (OSThread::GetCurrent() == NULL)
            OSThread::SetMainThreadData(&sStateChangeState);
        else
            OSThread::GetCurrent()->SetThreadData(&sStateChangeState);

		/* ����ע��״̬�ı��ɫ��ģ��ȥ������� */
        UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kStateChangeRole);
        {
            for (UInt32 theCurrentModule = 0; theCurrentModule < numModules; theCurrentModule++)
            {  
                QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kStateChangeRole, theCurrentModule);
                (void)theModule->CallDispatch(QTSS_StateChange_Role, &theParams);
            }
        }

        // ����߳�˽������
        // Make sure to clear out the thread data
        if (OSThread::GetCurrent() == NULL)
            OSThread::SetMainThreadData(NULL);
        else
            OSThread::GetCurrent()->SetThreadData(NULL);
    }
}


/************************************* ������RTPStatsUpdaterTask�� ************************************/

RTPStatsUpdaterTask::RTPStatsUpdaterTask()
:   Task(), fLastBandwidthTime(0), fLastBandwidthAvg(0), fLastBytesSent(0), fLastTotalMP3Bytes(0)
{
    this->SetTaskName("RTPStatsUpdaterTask");
	/* ��RTPStatsUpdaterTask����ָ�������̵߳�������� */
    this->Signal(Task::kStartEvent);
}

/* ������ǰRTPSession map�е�����RTPSession,�������һ��������(���ǻỰ����ʱ�����)��һ��RTPSession(ʵ������RTPSessionInterface) */
RTPSessionInterface* RTPStatsUpdaterTask::GetNewestSession(OSRefTable* inRTPSessionMap)
{
	//Caller must lock down the RTP session map
	SInt64 theNewestPlayTime = 0;
	RTPSessionInterface* theNewestSession = NULL;

	//use the session map to iterate through all the sessions, finding the most
	//recently connected client
	/* ����RTPSession map,ֱ������һ���յ�Hash Table Elem��ͣ�� */
	for (OSRefHashTableIter theIter(inRTPSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		/* �õ���ǰ�ķǿյ�Hash Table Elem */
		OSRef* theRef = theIter.GetCurrent();
		/* �õ��÷ǿյ�ǰԪ�Ķ��� */
		RTPSessionInterface* theSession = (RTPSessionInterface*)theRef->GetObject();
		/* ��ȡ��ǰ�Ự������ʱ�� */
		Assert(theSession->GetSessionCreateTime() > 0);
		/* ���µ�ǰSession������ */
		if (theSession->GetSessionCreateTime() > theNewestPlayTime)
		{
			theNewestPlayTime = theSession->GetSessionCreateTime();
			theNewestSession = theSession;
		}
	}
	return theNewestSession;
}

/* ��ȡ�����ص�ǰ���̵�cpu ʱ�� */
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

/* ������ÿ������1��,ע����������4�����ݳ�Ա��ֵ: */
SInt64 RTPStatsUpdaterTask::Run()
{
    /* ���Ȼ�ȡ��̬QTSServerInterface���ָ��,�������Բ�ѯ��������ص����Բ��� */
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

	/********************** ע����������Ĵ�������ͬ�� *********************************************/

    unsigned int periodicBytes = theServer->fPeriodicRTPBytes;
    (void)atomic_sub(&theServer->fPeriodicRTPBytes, periodicBytes);
    theServer->fTotalRTPBytes += periodicBytes;
    
    // Same deal for packet totals
    unsigned int periodicPackets = theServer->fPeriodicRTPPackets;/* ��һ������(��λ����)�ڵ�RTP���� */
    (void)atomic_sub(&theServer->fPeriodicRTPPackets, periodicPackets);
    theServer->fTotalRTPPackets += periodicPackets;
    
    // ..and for lost packet totals
    unsigned int periodicPacketsLost = theServer->fPeriodicRTPPacketsLost;
    (void)atomic_sub(&theServer->fPeriodicRTPPacketsLost, periodicPacketsLost);
    theServer->fTotalRTPPacketsLost += periodicPacketsLost;

	/********************** ע����������Ĵ�������ͬ�� ***********************************************/
    
	/* ��÷������ĵ�ǰʱ��(ms),ע����16�ֽڵ�,���ֵ�ǳ���Ҫ! */
    SInt64 curTime = OS::Milliseconds();
    
    //for cpu percent
	/* ��ȡ��ǰ���̵�cpuʱ��(s) */
        Float32 cpuTimeInSec = GetCPUTimeInSeconds();
    
    //also update current bandwidth statistic
	/* ��������ϴδ����ʱ�����,���㵱ǰ���������fCurrentRTPBandwidthInBits,fCurrentMP3BandwidthInBits,fCPUPercent */
    if (fLastBandwidthTime != 0)
    {
        Assert(curTime > fLastBandwidthTime);
		/* ע���ֵ��8�ֽڵ� */
        UInt32 delta = (UInt32)(curTime - fLastBandwidthTime);/* ��ǰʱ����ϴθ���ʱ��Ĳ�ֵ(ms) */
		/* ����һ��͸���������Ϣ:"Timer is off" */
        if (delta < 1000)
            WarnV(delta >= 1000, "Timer is off");
          

        UInt32 packetsPerSecond = periodicPackets;
        UInt32 theTime = delta / 1000;/* ʱ���ֵ(s) */
 
		/* ÿ���ͳ���RTP���� */
        packetsPerSecond /= theTime;
        Assert(packetsPerSecond >= 0);
		/* ÿ���RTP Packet���� */
        theServer->fRTPPacketsPerSecond = packetsPerSecond;
		/* ÿ��ÿ��packetҪ���ӵ��ֽ�����28,�ܹ�Ҫ���ӵ��ֽ�����? */
        UInt32 additionalBytes = 28 * packetsPerSecond; // IP headers( = 20) + UDP headers( = 8)
        /* ÿ��packetҪ���ӵı�������28*8,�ܹ�Ҫ���ӵı�������? */
        UInt32 headerBits = 8 * additionalBytes;/* ���ֽ�ת��Ϊbit */
		/* ÿ��Ҫ���ӵ�ͷbit�� */
		/* Ϊ��Ҫ�ٳ���theTime */
        headerBits  /= theTime;
      
		/* ���㵱ǰRTP���Ĵ�������� */
        Float32 bits = periodicBytes * 8;
        bits /= theTime;
		/* ���÷�������ǰ��������� */
        theServer->fCurrentRTPBandwidthInBits = (UInt32) (bits + headerBits);
         
        // okay let's do it for MP3 bytes now
		/* ����ǰ������MP3��������ֵ */
        bits = (Float32)(((SInt64)theServer->fTotalMP3Bytes - fLastTotalMP3Bytes) * 8);
        bits /= theTime;
		/* ���÷�����MP3��������� */
        theServer->fCurrentMP3BandwidthInBits = (UInt32)bits;


        //do the computation for cpu percent
		/* Ϊ����CPU����CPUʹ��ʱ��ٷֱ� */
        Float32 diffTime = cpuTimeInSec - theServer->fCPUTimeUsedInSec;
        theServer->fCPUPercent = (diffTime/theTime) * 100;  /* ͨ��ʱ�����CPU��ʹ���� */
		
		/* �õ����������� */
		UInt32 numProcessors = OS::GetNumProcessors();
		
		/* �Զ��CPU,����CPUʹ��ʱ��ٷֱ� */
		if (numProcessors > 1)
			theServer->fCPUPercent /= numProcessors;
    }

    
	/********** �����������ĸ���������Ҫ!! ***********/

	/* ע���������ǳ�Ա����ֵ */
    fLastTotalMP3Bytes = (SInt64)theServer->fTotalMP3Bytes;
    fLastBandwidthTime = curTime;


    // We use a running average for avg. bandwidth calculations
	/* ע�����avg MP3 bandwidth���㷨 */
    theServer->fAvgMP3BandwidthInBits = (theServer->fAvgMP3BandwidthInBits
            + theServer->fCurrentMP3BandwidthInBits)/2; 
    
    //for cpu percent
	/* �����ϴ��ϸ�����ʹ��CPU��ʱ�� */
    theServer->fCPUTimeUsedInSec    = cpuTimeInSec; 
    
    //also compute average bandwidth, a much more smooth value. This is done with
    //the fLastBandwidthAvg, a timestamp of the last time we did an average, and
    //fLastBytesSent, the number of bytes sent when we last did an average.
	/* ����ֵ"average_bandwidth_update"��60s,ע��fLastBandwidthAvgָ�ϴμ���ƽ�������ʱ�� */
	/* ���ϴμ���ƽ�������ʱ�����,�ҵ�ǰʱ�䳬���ϴμ���ƽ�������ʱ��+���¼��60s */
    if ((fLastBandwidthAvg != 0) && (curTime > (fLastBandwidthAvg +
        (theServer->GetPrefs()->GetAvgBandwidthUpdateTimeInSecs() * 1000))))
    {
		/* ����ǰ�����θ���ʱ���� */
        UInt32 delta = (UInt32)(curTime - fLastBandwidthAvg);
		/* ��ǰ������ͳ������ֽ���(�������ͳ������ֽ�-�ϴθ���ʱ�ͳ����ֽ���) */
        SInt64 bytesSent = theServer->fTotalRTPBytes - fLastBytesSent;
        Assert(bytesSent >= 0);
        
        //do the bandwidth computation using floating point divides
        //for accuracy and speed.
		/* ��ǰ�ͳ����ܱ����� */
        Float32 bits = (Float32)(bytesSent * 8);
        Float32 theAvgTime = (Float32)delta;
		/* ���ϴ�������ʱ����(��) */
        theAvgTime /= 1000;
		/* ��ǰ�����ÿ���ͳ����ܱ����� */
        bits /= theAvgTime;
        Assert(bits >= 0);
		/* �õ�ǰ�����ÿ���ͳ����ܱ��������õ�ǰƽ��RTP Bandwidth(����) */
		/***********  NOTE:�����ǳ���Ҫ ***************************/
        theServer->fAvgRTPBandwidthInBits = (UInt32)bits;
		/***********  NOTE:�����ǳ���Ҫ ***************************/

		/* Ϊ���´�ѭ��ʹ�� */
		/**************** NOTE!! ********************/
		/* ��¼��ǰʱ��� */
        fLastBandwidthAvg = curTime;
		/* ��¼��ǰ������RTP���ֽ����� */
        fLastBytesSent = theServer->fTotalRTPBytes;
		/**************** NOTE!! ********************/
        
        //if the bandwidth is above the bandwidth setting, disconnect 1 user by sending them
        //a BYE RTCP packet.
		//��ȡmaximum_bandwidth����ֵ(102400Kbit/s,��100MСb)
        SInt32 maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
		/* ������������theServer->fAvgRTPBandwidthInBits��ֵ��������ֵ(100MСb),�ͶϿ��õ㲥���� */
        if ((maxKBits > -1) && (theServer->fAvgRTPBandwidthInBits > ((UInt32)maxKBits * 1024)))
        {
            //we need to make sure that all of this happens atomically wrt the session map
			/* ������RTPSession Map */
            OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
			/* ��ø�RTPSession Map�������һ��RTPSession */
            RTPSessionInterface* theSession = this->GetNewestSession(theServer->fRTPMap);
            if (theSession != NULL)
				/* �Ͽ����10�����ڽӽ��������һ�ε�RTPSession */
                if ((curTime - theSession->GetSessionCreateTime()) <
                        theServer->GetPrefs()->GetSafePlayDurationInSecs() * 1000)    //��ȫ���Ų���ϵ���ʱ��ֵ��10����
                    theSession->Signal(Task::kKillEvent);
        }
    }
	/* ���ϴμ���ƽ�������ʱ��Ϊ��,˵������ʼ���,�����¼��������������ѭ��ʹ��. */
    else if (fLastBandwidthAvg == 0)
    {
		/**************** NOTE!! ********************/
		/* ��¼��ǰʱ��� */
        fLastBandwidthAvg = curTime;
        fLastBytesSent = theServer->fTotalRTPBytes;
		/**************** NOTE!! ********************/
    }
    
    (void)this->GetEvents();//we must clear the event mask!
	/* ����ֵ"total_bytes_update"Ϊ1s  */
    return theServer->GetPrefs()->GetTotalBytesUpdateTimeInSecs() * 1000;
}


/********************************* ������RTPStatsUpdaterTask�� ********************************/

/********************************* ������Param retrieval functions for ServerDict ********************************/

/* ���ò����ص�ǰ����ϵͳʱ���Unixʱ��(ms) */
void* QTSServerInterface::CurrentUnixTimeMilli(QTSSDictionary* inServer, UInt32* outLen)
{
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    theServer->fCurrentTime_UnixMilli = OS::TimeMilli_To_UnixTimeMilli(OS::Milliseconds()); 
    
    // Return the result
    *outLen = sizeof(theServer->fCurrentTime_UnixMilli);
    return &theServer->fCurrentTime_UnixMilli;
}

/* ���ò�����RTP/RTCP Socket pair�����е�UDP���� */
void* QTSServerInterface::GetTotalUDPSockets(QTSSDictionary* inServer, UInt32* outLen)
{
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    // Multiply by 2 because this is returning the number of socket *pairs*
    theServer->fTotalUDPSockets = theServer->fSocketPool->GetSocketQueue()->GetLength() * 2;
    
    // Return the result
    *outLen = sizeof(theServer->fTotalUDPSockets);
    return &theServer->fTotalUDPSockets;
}

/* ���������׽�������TCPListenerSocke�е�ÿ������,�ж����ļ��������Ƿ��ù�? ����һ���ù�,�����ò�����theServer->fIsOutOfDescriptors */
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

/* ͨ����ѯ�����OSBufferPool�ж�������ĸ���,�����ò�����fNumUDPBuffers�ĸ��� */
void* QTSServerInterface::GetNumUDPBuffers(QTSSDictionary* inServer, UInt32* outLen)
{
    // This param retrieval function must be invoked each time it is called,
    // because whether we are out of descriptors or not is continually changing
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    
	//�����е�UDP buffer����OSBufferPool�н��й���,���ڻ�ȡ�û�����ж�������ĸ���
    theServer->fNumUDPBuffers = RTPPacketResender::GetNumRetransmitBuffers();

    // Return the result
    *outLen = sizeof(theServer->fNumUDPBuffers);
    return &theServer->fNumUDPBuffers;
}

/* �ۼƻ����OSBufferPool��δʹ�õĻ����ֽ�����,���ò�����fUDPWastageInBytes����  */
void* QTSServerInterface::GetNumWastedBytes(QTSSDictionary* inServer, UInt32* outLen)
{
    // This param retrieval function must be invoked each time it is called,
    // because whether we are out of descriptors or not is continually changing
    QTSServerInterface* theServer = (QTSServerInterface*)inServer;
    
	//���������OSBufferPool�ж��������ʹ�����,�ۼ�δʹ�õĻ����ֽ�����
    theServer->fUDPWastageInBytes = RTPPacketResender::GetWastedBufferBytes();

    // Return the result
    *outLen = sizeof(theServer->fUDPWastageInBytes);
    return &theServer->fUDPWastageInBytes;  
}

/********************************* ������Param retrieval functions for ServerDict ********************************/

/* ���Ȼ�ȡ��������ʱ���,���뵱ǰʱ�������,��Ϊ����ʱ��(ms)����,�ٶ�ȡ������ֵ�����ء�ע���һ�������QTSSConnectedUserDict */
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

/* ����ģ�齫ָ�����������,��ָ�����������־ */
QTSS_Error  QTSSErrorLogStream::Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags)
{
    // For the error log stream, the flags are considered to be the verbosity of the error.
    // ����־���𳬹�5ʱ,ʹ���ظ���2
    if (inFlags >= qtssIllegalVerbosity)
        inFlags = qtssMessageVerbosity;
        
    QTSServerInterface::LogError(inFlags, (char*)inBuffer);
    if (outLenWritten != NULL)
        *outLenWritten = inLen;
        
    return QTSS_NoErr;
}

/* ����ģ���¼ָ����Assert�������־��Ϣ */
void QTSSErrorLogStream::LogAssert(char* inMessage)
{
    QTSServerInterface::LogError(qtssAssertVerbosity, inMessage);
}


