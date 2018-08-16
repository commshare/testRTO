
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  QTSServerInterface.h
Description: define an interface for getting and setting server-wide attributes, and storing global server resources.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSSERVERINTERFACE_H__
#define __QTSSERVERINTERFACE_H__

#include "QTSS.h"
#include "QTSSDictionary.h"
#include "QTSServerPrefs.h"
#include "QTSSMessages.h"
#include "QTSSModule.h"
#include "atomic.h"

#include "OSMutex.h"
#include "Task.h"
#include "TCPListenerSocket.h"
#include "ResizeableStringFormatter.h"

class UDPSocketPool;
class QTSServerPrefs;
class QTSSMessages;
class RTPSessionInterface;

// This object also functions as our assert logger
// This QTSSStream is used by modules to write to the error log
class QTSSErrorLogStream : public QTSSStream, public AssertLogger
{
    public:
    
        QTSSErrorLogStream() {}
        virtual ~QTSSErrorLogStream() {}
        
        virtual QTSS_Error  Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags);
        virtual void        LogAssert(char* inMessage);
};

class QTSServerInterface : public QTSSDictionary
{
    public:
    
        //Initialize must be called right off the bat to initialize dictionary resources
        static void     Initialize();

        // CONSTRUCTOR / DESTRUCTOR
        QTSServerInterface();
        virtual ~QTSServerInterface() {}
        
        // STATISTICS MANIPULATION
        // These functions are how the server keeps its statistics current
        
		/* ���µ�ǰRTSPSession���ܸ��� */
        void             AlterCurrentRTSPSessionCount(SInt32 inDifference)
            { OSMutexLocker locker(&fMutex); fNumRTSPSessions += inDifference; }
        void             AlterCurrentRTSPHTTPSessionCount(SInt32 inDifference)
            { OSMutexLocker locker(&fMutex); fNumRTSPHTTPSessions += inDifference; }
        void             SwapFromRTSPToHTTP()
            { OSMutexLocker locker(&fMutex); fNumRTSPSessions--; fNumRTSPHTTPSessions++; }
            
        //total rtp bytes sent by the server
        void            IncrementTotalRTPBytes(UInt32 bytes)
            { (void)atomic_add(&fPeriodicRTPBytes, bytes); }
        //total rtp packets sent by the server
        void            IncrementTotalPackets()
            { (void)atomic_add(&fPeriodicRTPPackets, 1); }
        //total rtp bytes reported as lost by the clients
        void            IncrementTotalRTPPacketsLost(UInt32 packets)
            { (void)atomic_add(&fPeriodicRTPPacketsLost, packets); }
                                        
        // Also increments current RTP session count
		/* used in RTPSession::Activate() */
        void            IncrementTotalRTPSessions()
            { OSMutexLocker locker(&fMutex); fNumRTPSessions++; fTotalRTPSessions++; }            
        void            AlterCurrentRTPSessionCount(SInt32 inDifference)
            { OSMutexLocker locker(&fMutex); fNumRTPSessions += inDifference; }

        //track how many sessions are playing
		/* used by RTPSession::Play() */
        void            AlterRTPPlayingSessions(SInt32 inDifference)
            { OSMutexLocker locker(&fMutex); fNumRTPPlayingSessions += inDifference; }
            
        /* �������ӳ� */
        void            IncrementTotalLate(SInt64 milliseconds)
           {    OSMutexLocker locker(&fMutex); 
                fTotalLate += milliseconds;
                if (milliseconds > fCurrentMaxLate) fCurrentMaxLate = milliseconds;
                if (milliseconds > fMaxLate) fMaxLate = milliseconds;
           }
        
		/* ������qualitylevel */
        void            IncrementTotalQuality(SInt32 level)
           { OSMutexLocker locker(&fMutex); fTotalQuality += level; }
           
        /* ���Ӵ���(thinning) */   
        void            IncrementNumThinned(SInt32 inDifference)
           { OSMutexLocker locker(&fMutex); fNumThinned += inDifference; }
       
		// clear
        void            ClearTotalLate()
           { OSMutexLocker locker(&fMutex); fTotalLate = 0;  }
        void            ClearCurrentMaxLate()
           { OSMutexLocker locker(&fMutex); fCurrentMaxLate = 0;  }
        void            ClearTotalQuality()
           { OSMutexLocker locker(&fMutex); fTotalQuality = 0;  }
     

        // ACCESSORS
        
        QTSS_ServerState    GetServerState()        { return fServerState; }
        UInt32              GetNumRTPSessions()     { return fNumRTPSessions; }
        UInt32              GetNumRTSPSessions()    { return fNumRTSPSessions; }
        UInt32              GetNumRTSPHTTPSessions(){ return fNumRTSPHTTPSessions; }
        
        UInt32              GetTotalRTPSessions()   { return fTotalRTPSessions; }
        UInt32              GetNumRTPPlayingSessions()   { return fNumRTPPlayingSessions; }
        
        UInt32              GetCurBandwidthInBits() { return fCurrentRTPBandwidthInBits; }
        UInt32              GetAvgBandwidthInBits() { return fAvgRTPBandwidthInBits; }
        UInt32              GetRTPPacketsPerSec()   { return fRTPPacketsPerSecond; }
        UInt64              GetTotalRTPBytes()      { return fTotalRTPBytes; }
        UInt64              GetTotalRTPPacketsLost(){ return fTotalRTPPacketsLost; }
        UInt64              GetTotalRTPPackets()    { return fTotalRTPPackets; }
        Float32             GetCPUPercent()         { return fCPUPercent; }

        Bool16              SigIntSet()             { return fSigInt; }
        Bool16				SigTermSet()			{ return fSigTerm; }
		
        UInt32              GetNumMP3Sessions()     { return fNumMP3Sessions; }
        UInt32              GetTotalMP3Sessions()   { return fTotalMP3Sessions; }
        UInt64              GetTotalMP3Bytes()      { return fTotalMP3Bytes; }
        
		//�õ�/���÷�����Debug��Ϣ
        UInt32              GetDebugLevel()                     { return fDebugLevel; }
        UInt32              GetDebugOptions()                   { return fDebugOptions; }
        void                SetDebugLevel(UInt32 debugLevel)    { fDebugLevel = debugLevel; }
        void                SetDebugOptions(UInt32 debugOptions){ fDebugOptions = debugOptions; }
        
        SInt64              GetMaxLate()                { return fMaxLate; };
        SInt64              GetTotalLate()              { return fTotalLate; };
        SInt64              GetCurrentMaxLate()         { return fCurrentMaxLate; };
        SInt64              GetTotalQuality()           { return fTotalQuality; };
        SInt32              GetNumThinned()             { return fNumThinned; };

        // GLOBAL OBJECTS REPOSITORY(ȫ�ֶ����)
        // This object is in fact global, so there is an accessor for it as well.
		/* ע�������⼸�����ǳ���Ҫ */
        
		/* needed by RTPSession::run()/Play() */
		/* ���Ψһ��Server�ӿ�ָ��,����ǳ���Ҫ!! */
        static QTSServerInterface*  GetServer()         { return sServer; }
		QTSServerPrefs*     GetPrefs()                  { return fSrvrPrefs; }
		QTSSMessages*       GetMessages()               { return fSrvrMessages; }
        
        //Allows you to map RTP session IDs (strings) to actual RTP session objects
        OSRefTable*         GetRTPSessionMap()          { return fRTPMap; }
    
        //Server provides a statically created & bound UDPSocket / Demuxer pair
        //for each IP address setup to serve RTP. You access those pairs through
        //this function. This returns a pair pre-bound to the IPAddr specified.
		/* �õ�UDPSocketPoolָ�� */
        UDPSocketPool*      GetSocketPool()             { return fSocketPool; }
       
        
        // SERVER NAME & VERSION
        
        static StrPtrLen&   GetServerName()             { return sServerNameStr; }
        static StrPtrLen&   GetServerVersion()          { return sServerVersionStr; }
        static StrPtrLen&   GetServerPlatform()         { return sServerPlatformStr; }
        static StrPtrLen&   GetServerBuildDate()        { return sServerBuildDateStr; }
        static StrPtrLen&   GetServerHeader()           { return sServerHeaderPtr; }
        static StrPtrLen&   GetServerBuild()            { return sServerBuildStr; }
        static StrPtrLen&   GetServerComment()          { return sServerCommentStr; }
        
        // PUBLIC HEADER
        static StrPtrLen*   GetPublicHeader()           { return &sPublicHeaderStr; }
        
        // KILL ALL
        void                KillAllRTPSessions();
        
        // SIGINT - to interrupt the server, set this flag and the server will shut down, SigInt - Signal Interrupt
        void                SetSigInt()                 { fSigInt = true; }

        // SIGTERM - to kill(terminate) the server, set this flag and the server will shut down, SigTerm - Signal Terminate
        void                SetSigTerm()                { fSigTerm = true; }
        
        // MODULE STORAGE
        // All module objects are stored here, and are accessable through these routines.
        
        // Returns the number of modules that act in a given role
		/* needed by RTPSession::run(),�õ�ע��ָ��Role��Module��Ŀ */
        static UInt32       GetNumModulesInRole(QTSSModule::RoleIndex inRole)
                { Assert(inRole < QTSSModule::kNumRoles); return sNumModulesInRole[inRole]; }
        
        // Allows the caller to iterate over all modules that act in a given role 
		/* needed by RTPSession::run(),�õ�ModuleArray������ָ��Role�Ķ�άָ��������ָ��������Ԫ��(�Ǹ� QTSSModule*) */
        static QTSSModule*  GetModule(QTSSModule::RoleIndex inRole, UInt32 inIndex)
                {  
					Assert(inRole < QTSSModule::kNumRoles);
                    Assert(inIndex < sNumModulesInRole[inRole]);
                    return sModuleArray[inRole][inIndex];
                }

        // ����ģ����������÷�������״̬
        // We need to override this. This is how we implement the QTSS_StateChange_Role
        virtual void    SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen);								
        
        // ERROR LOGGING
        // Invokes the error logging modules with some data
        static void     LogError(QTSS_ErrorVerbosity inVerbosity, char* inBuffer);
        
        // Returns the error log stream
        static QTSSErrorLogStream* GetErrorLogStream() { return &sErrorLogStream; }
        
        // LOCKING DOWN THE SERVER OBJECT
        OSMutex*        GetServerObjectMutex() { return &fMutex; }
        

        
    protected:

        // Setup by the derived RTSPServer object,��������ݳ�Ա�ͳ�Ա������������RTSPServer���͵�QTSServer������

        //Sockets are allocated global to the server, and arbitrated through this pool here.
        //RTCP data is processed completely within the following task.
		/* TCP/UDP Socket�ڷ������ڲ���ȫ�ַ���,RTCPTask��UDP Socket�ϵ�RTCP���ݴ���������� */
        UDPSocketPool*              fSocketPool;

		// Array of pointers to TCPListenerSockets.����Socket����,ע��������ֵ��������QTSServer::CreateListeners()�ж���
		TCPListenerSocket**         fListeners;/* ָ��ָ�������ָ�� */
		UInt32                      fNumListeners; // Number of elements in the array
		UInt32                      fDefaultIPAddr; //Ĭ�ϰ󶨵�IP��ַ,����Ԥ���IP��ַ����ĵ�һ������,�μ�QTSServer::SetDefaultIPAddr()
        
        // All RTP sessions are put into this map
		/* ���е�RTP Session���һ��Hash Table����ΪRTPSessionMap,�ñ���ÿ����Ԫ����һ�������Ψһ��Session ID */
        OSRefTable*                 fRTPMap;
        
		// Server prefers,������Ԥ��ֵ����
        QTSServerPrefs*             fSrvrPrefs;
        QTSSMessages*               fSrvrMessages;

		// Stub Server prefers(RTSP/RTP Server etc),���������Ԥ��ֵ����
        QTSServerPrefs*				fStubSrvrPrefs;
        QTSSMessages*				fStubSrvrMessages;

		//Server state and IP addr	
        QTSS_ServerState            fServerState;/* QTSS.h�ж�����6�ַ�����״̬��Ϣ */
         
        // startup time
        SInt64                      fStartupTime_UnixMilli;//������������ʱ��
        SInt32                      fGMTOffset; //����ʱ����GMTʱ���ƫ��Сʱ��

		/* �Է�����ʹ�õ�RTSP������ͳ��,������һ���ַ���,��QTSServer::SetupPublicHeader() */
        static ResizeableStringFormatter    sPublicHeaderFormatter;
        static StrPtrLen                    sPublicHeaderStr;

        // MODULE DATA    
		/* ÿ��role��Ӧ��moduleָ���ָ��,QTSSModule**,��Ϊ������ɵ�����,�μ�QTSServer::BuildModuleRoleArrays() */
        static QTSSModule**             sModuleArray[QTSSModule::kNumRoles];//24
		/* ÿ��role��������module��ĿΪ������ɵ����� */
        static UInt32                   sNumModulesInRole[QTSSModule::kNumRoles];
		/* ��module��ɵĶ���,���Ҫ����OSQueue.h/cpp */
        static OSQueue                  sModuleQueue;/* used in QTSServer::AddModule()  */

		
        static QTSSErrorLogStream       sErrorLogStream;/* ����module���̵Ĵ�����־�� */

    private:
    
        enum
        {
            kMaxServerHeaderLen = 1000
        };

        static void* TimeConnected(QTSSDictionary* inConnection, UInt32* outLen);

        static UInt32       sServerAPIVersion;  //0x00040000
        static StrPtrLen    sServerNameStr;     //"DSS"
        static StrPtrLen    sServerVersionStr;  //"5.5.1"
        static StrPtrLen    sServerBuildStr;    //"489.8"
        static StrPtrLen    sServerCommentStr;  //"Release/Darwin; "
        static StrPtrLen    sServerPlatformStr; //"Linux"
        static StrPtrLen    sServerBuildDateStr;//__DATE__ ", "__TIME__
        static char         sServerHeader[kMaxServerHeaderLen]; //1000���ֽڵ��ַ�����
        static StrPtrLen    sServerHeaderPtr;   //"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"

        OSMutex             fMutex;

		//num of current RTSP/HTTP/RTP Session
        UInt32              fNumRTSPSessions;
        UInt32              fNumRTSPHTTPSessions;
        UInt32              fNumRTPSessions; //��ǰRTPSession��Ŀ

        //stores the current number of playing connections.
        UInt32              fNumRTPPlayingSessions;
     
		//���漸�����ۼ�����
        //stores the total number of connections since startup.�Է�������������,�ܵ�RTPSession��Ŀ
        UInt32              fTotalRTPSessions;
        //stores the total number of bytes served since startup
        UInt64              fTotalRTPBytes;
        //total number of rtp packets sent since startup
        UInt64              fTotalRTPPackets;
        //stores the total number of bytes lost (as reported by clients) since startup
        UInt64              fTotalRTPPacketsLost;

        //because there is no 64 bit atomic add (for obvious reasons), we efficiently
        //implement total byte counting by atomic adding to this variable, then every
        //once in a while updating the sTotalBytes.
		/* for calculate the above global quantities using theses temp variable,�μ�RTPStatsUpdaterTask::Run() */
        unsigned int        fPeriodicRTPBytes;
        unsigned int        fPeriodicRTPPacketsLost;
        unsigned int        fPeriodicRTPPackets;
        
        //stores the current served bandwidth in BITS per second
        UInt32              fCurrentRTPBandwidthInBits;
        UInt32              fAvgRTPBandwidthInBits;
        UInt32              fRTPPacketsPerSecond;
        
		// CPU
        Float32             fCPUPercent;
        Float32             fCPUTimeUsedInSec;              
        
		/************** �����⼸����Param retrieval functions�ڷ������ֵ������� *********************/
        // stores # of UDP sockets in the server currently (gets updated lazily via.param retrieval function)
        // ��������ǰ��UDP Socket����,����RTP/RTCP Socket pair���г��ȵ�2��
        UInt32              fTotalUDPSockets; 
        // are we out of descriptors? �ļ��������ù�����?
        Bool16              fIsOutOfDescriptors; 
        // Storage for current time attribute,�������ĵ�ǰ����ϵͳʱ���Unixʱ��(ms)
        SInt64              fCurrentTime_UnixMilli;
        // Stats for UDP retransmits
        UInt32              fUDPWastageInBytes; /* �ۼƻ����OSBufferPool��δʹ�õĻ����ֽ����� */
        UInt32              fNumUDPBuffers;     /* �����OSBufferPool�ж�������ĵ�ǰ���� */
		/************** �����⼸����Param retrieval functions�ڷ������ֵ������� ********************/
        
        // MP3 Client Session params
        UInt32              fNumMP3Sessions;
        UInt32              fTotalMP3Sessions;
        UInt32              fCurrentMP3BandwidthInBits;
        UInt64              fTotalMP3Bytes;
        UInt32              fAvgMP3BandwidthInBits;
        
		//interrupt server signals	
        Bool16              fSigInt; /* Signal Interrupt server? */
        Bool16              fSigTerm;/* Signal Terminate server? */

		//Debug params level/options	
        UInt32              fDebugLevel;  /* debug ���� */
        UInt32              fDebugOptions;/* debug ѡ�� */
        

		//late
        SInt64              fMaxLate;
        SInt64              fTotalLate;
        SInt64              fCurrentMaxLate;
        SInt64              fTotalQuality;
        SInt32              fNumThinned;   //�ܱ�������

        // Param retrieval functions for ServerDict, see QTSServerInterface::sAttributes[]��ֵ
        static void* CurrentUnixTimeMilli(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetTotalUDPSockets(QTSSDictionary* inServer, UInt32* outLen);
        static void* IsOutOfDescriptors(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetNumUDPBuffers(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetNumWastedBytes(QTSSDictionary* inServer, UInt32* outLen);
        
		/* ��Ҫ��������̬����: */
        static QTSServerInterface*  sServer; /* ָ��QTSServerInterface���ָ��,ע�������÷�������,needed by RTPSession::run() */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];/* ������������ */
        static QTSSAttrInfoDict::AttrInfo   sConnectedUserAttributes[];/* ��������ConnectedUser���� */
        
        friend class RTPStatsUpdaterTask;/* def see below */
        friend class SessionTimeoutTask;
};

/* ʵʱ����RTPSession״̬��Task�� */
class RTPStatsUpdaterTask : public Task
{
    public:
    
        // This class runs periodically(����ֵΪ1s) to compute current totals & averages
        RTPStatsUpdaterTask();
        virtual ~RTPStatsUpdaterTask() {}
    
    private:
    
		/* ����������,����������������� */
        virtual SInt64 Run();
        RTPSessionInterface* GetNewestSession(OSRefTable* inRTPSessionMap);
        Float32 GetCPUTimeInSeconds();
        
		/* �����ϴδ����ʱ�� */
        SInt64 fLastBandwidthTime;
		/* �ϴμ���ƽ�������ʱ�� */
        SInt64 fLastBandwidthAvg;
		/* �ϴη�����RTP���ֽ����� */
        SInt64 fLastBytesSent;
		/* �ϴη�����MP3���ֽ���  */
        SInt64 fLastTotalMP3Bytes;
};



#endif // __QTSSERVERINTERFACE_H__

