
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
        
		/* 更新当前RTSPSession的总个数 */
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
            
        /* 增加总延迟 */
        void            IncrementTotalLate(SInt64 milliseconds)
           {    OSMutexLocker locker(&fMutex); 
                fTotalLate += milliseconds;
                if (milliseconds > fCurrentMaxLate) fCurrentMaxLate = milliseconds;
                if (milliseconds > fMaxLate) fMaxLate = milliseconds;
           }
        
		/* 增加总qualitylevel */
        void            IncrementTotalQuality(SInt32 level)
           { OSMutexLocker locker(&fMutex); fTotalQuality += level; }
           
        /* 增加打薄数(thinning) */   
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
        
		//得到/设置服务器Debug信息
        UInt32              GetDebugLevel()                     { return fDebugLevel; }
        UInt32              GetDebugOptions()                   { return fDebugOptions; }
        void                SetDebugLevel(UInt32 debugLevel)    { fDebugLevel = debugLevel; }
        void                SetDebugOptions(UInt32 debugOptions){ fDebugOptions = debugOptions; }
        
        SInt64              GetMaxLate()                { return fMaxLate; };
        SInt64              GetTotalLate()              { return fTotalLate; };
        SInt64              GetCurrentMaxLate()         { return fCurrentMaxLate; };
        SInt64              GetTotalQuality()           { return fTotalQuality; };
        SInt32              GetNumThinned()             { return fNumThinned; };

        // GLOBAL OBJECTS REPOSITORY(全局对象库)
        // This object is in fact global, so there is an accessor for it as well.
		/* 注意下面这几个都非常重要 */
        
		/* needed by RTPSession::run()/Play() */
		/* 获得唯一的Server接口指针,这个非常重要!! */
        static QTSServerInterface*  GetServer()         { return sServer; }
		QTSServerPrefs*     GetPrefs()                  { return fSrvrPrefs; }
		QTSSMessages*       GetMessages()               { return fSrvrMessages; }
        
        //Allows you to map RTP session IDs (strings) to actual RTP session objects
        OSRefTable*         GetRTPSessionMap()          { return fRTPMap; }
    
        //Server provides a statically created & bound UDPSocket / Demuxer pair
        //for each IP address setup to serve RTP. You access those pairs through
        //this function. This returns a pair pre-bound to the IPAddr specified.
		/* 得到UDPSocketPool指针 */
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
		/* needed by RTPSession::run(),得到注册指定Role的Module数目 */
        static UInt32       GetNumModulesInRole(QTSSModule::RoleIndex inRole)
                { Assert(inRole < QTSSModule::kNumRoles); return sNumModulesInRole[inRole]; }
        
        // Allows the caller to iterate over all modules that act in a given role 
		/* needed by RTPSession::run(),得到ModuleArray数组中指定Role的二维指针数组中指定索引的元素(是个 QTSSModule*) */
        static QTSSModule*  GetModule(QTSSModule::RoleIndex inRole, UInt32 inIndex)
                {  
					Assert(inRole < QTSSModule::kNumRoles);
                    Assert(inIndex < sNumModulesInRole[inRole]);
                    return sModuleArray[inRole][inIndex];
                }

        // 调用模块用入参设置服务器新状态
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

        // Setup by the derived RTSPServer object,下面的数据成员和成员函被派生子类RTSPServer类型的QTSServer所创建

        //Sockets are allocated global to the server, and arbitrated through this pool here.
        //RTCP data is processed completely within the following task.
		/* TCP/UDP Socket在服务器内部被全局分配,RTCPTask与UDP Socket上的RTCP数据处理紧密相连 */
        UDPSocketPool*              fSocketPool;

		// Array of pointers to TCPListenerSockets.侦听Socket数组,注意这两个值的设置在QTSServer::CreateListeners()中定义
		TCPListenerSocket**         fListeners;/* 指向指针数组的指针 */
		UInt32                      fNumListeners; // Number of elements in the array
		UInt32                      fDefaultIPAddr; //默认绑定的IP地址,它是预设的IP地址数组的第一个分量,参见QTSServer::SetDefaultIPAddr()
        
        // All RTP sessions are put into this map
		/* 所有的RTP Session组成一个Hash Table，名为RTPSessionMap,该表中每个表元都有一个随机的唯一的Session ID */
        OSRefTable*                 fRTPMap;
        
		// Server prefers,服务器预设值对象
        QTSServerPrefs*             fSrvrPrefs;
        QTSSMessages*               fSrvrMessages;

		// Stub Server prefers(RTSP/RTP Server etc),存根服务器预设值对象
        QTSServerPrefs*				fStubSrvrPrefs;
        QTSSMessages*				fStubSrvrMessages;

		//Server state and IP addr	
        QTSS_ServerState            fServerState;/* QTSS.h中定义了6种服务器状态信息 */
         
        // startup time
        SInt64                      fStartupTime_UnixMilli;//服务器启动的时间
        SInt32                      fGMTOffset; //本地时区与GMT时间的偏移小时数

		/* 对服务器使用的RTSP方法作统计,并给出一个字符串,见QTSServer::SetupPublicHeader() */
        static ResizeableStringFormatter    sPublicHeaderFormatter;
        static StrPtrLen                    sPublicHeaderStr;

        // MODULE DATA    
		/* 每个role对应的module指针的指针,QTSSModule**,作为分量组成的数组,参见QTSServer::BuildModuleRoleArrays() */
        static QTSSModule**             sModuleArray[QTSSModule::kNumRoles];//24
		/* 每个role所关联的module数目为分量组成的数组 */
        static UInt32                   sNumModulesInRole[QTSSModule::kNumRoles];
		/* 由module组成的队列,务必要读懂OSQueue.h/cpp */
        static OSQueue                  sModuleQueue;/* used in QTSServer::AddModule()  */

		
        static QTSSErrorLogStream       sErrorLogStream;/* 处理module过程的错误日志流 */

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
        static char         sServerHeader[kMaxServerHeaderLen]; //1000个字节的字符数组
        static StrPtrLen    sServerHeaderPtr;   //"Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )"

        OSMutex             fMutex;

		//num of current RTSP/HTTP/RTP Session
        UInt32              fNumRTSPSessions;
        UInt32              fNumRTSPHTTPSessions;
        UInt32              fNumRTPSessions; //当前RTPSession数目

        //stores the current number of playing connections.
        UInt32              fNumRTPPlayingSessions;
     
		//下面几个是累计属性
        //stores the total number of connections since startup.自服务器启动至今,总的RTPSession数目
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
		/* for calculate the above global quantities using theses temp variable,参见RTPStatsUpdaterTask::Run() */
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
        
		/************** 下面这几个用Param retrieval functions在服务器字典中设置 *********************/
        // stores # of UDP sockets in the server currently (gets updated lazily via.param retrieval function)
        // 服务器当前的UDP Socket总数,它是RTP/RTCP Socket pair队列长度的2倍
        UInt32              fTotalUDPSockets; 
        // are we out of descriptors? 文件描述符用光了吗?
        Bool16              fIsOutOfDescriptors; 
        // Storage for current time attribute,服务器的当前操作系统时间的Unix时间(ms)
        SInt64              fCurrentTime_UnixMilli;
        // Stats for UDP retransmits
        UInt32              fUDPWastageInBytes; /* 累计缓存池OSBufferPool中未使用的缓存字节总数 */
        UInt32              fNumUDPBuffers;     /* 缓存池OSBufferPool中定长缓存的当前个数 */
		/************** 上面这几个用Param retrieval functions在服务器字典中设置 ********************/
        
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
        UInt32              fDebugLevel;  /* debug 级别 */
        UInt32              fDebugOptions;/* debug 选项 */
        

		//late
        SInt64              fMaxLate;
        SInt64              fTotalLate;
        SInt64              fCurrentMaxLate;
        SInt64              fTotalQuality;
        SInt32              fNumThinned;   //总薄化次数

        // Param retrieval functions for ServerDict, see QTSServerInterface::sAttributes[]赋值
        static void* CurrentUnixTimeMilli(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetTotalUDPSockets(QTSSDictionary* inServer, UInt32* outLen);
        static void* IsOutOfDescriptors(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetNumUDPBuffers(QTSSDictionary* inServer, UInt32* outLen);
        static void* GetNumWastedBytes(QTSSDictionary* inServer, UInt32* outLen);
        
		/* 重要的三个静态参数: */
        static QTSServerInterface*  sServer; /* 指向QTSServerInterface类的指针,注意这种用法很奇特,needed by RTPSession::run() */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];/* 服务器的属性 */
        static QTSSAttrInfoDict::AttrInfo   sConnectedUserAttributes[];/* 服务器的ConnectedUser属性 */
        
        friend class RTPStatsUpdaterTask;/* def see below */
        friend class SessionTimeoutTask;
};

/* 实时更新RTPSession状态的Task类 */
class RTPStatsUpdaterTask : public Task
{
    public:
    
        // This class runs periodically(配置值为1s) to compute current totals & averages
        RTPStatsUpdaterTask();
        virtual ~RTPStatsUpdaterTask() {}
    
    private:
    
		/* 这是主函数,会调用其他两个函数 */
        virtual SInt64 Run();
        RTPSessionInterface* GetNewestSession(OSRefTable* inRTPSessionMap);
        Float32 GetCPUTimeInSeconds();
        
		/* 计算上次带宽的时间 */
        SInt64 fLastBandwidthTime;
		/* 上次计算平均带宽的时间 */
        SInt64 fLastBandwidthAvg;
		/* 上次发出的RTP包字节总数 */
        SInt64 fLastBytesSent;
		/* 上次发出的MP3总字节数  */
        SInt64 fLastTotalMP3Bytes;
};



#endif // __QTSSERVERINTERFACE_H__

