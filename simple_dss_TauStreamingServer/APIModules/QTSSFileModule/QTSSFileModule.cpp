
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSFileModule.cpp
Description: Content source module that uses the QTFileLib to packetizer a RTP 
             packet and send to clients, also check the sdp file info.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


#include <string.h>
#include <errno.h>

#include "QTSSFileModule.h"
#include "QTSSModuleUtils.h"
#include "QTSSMemoryDeleter.h"
#include "QTRTPFile.h"
#include "QTFile.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "SDPSourceInfo.h"
#include "SDPUtils.h"
#include "StringParser.h"
#include "StringFormatter.h"
#include "ResizeableStringFormatter.h"



/* this class similar to a struct, very important */
/* describe file specific attributes */
class FileSession
{
    public:
    
        FileSession() : fAdjustedPlayTime(0), fNextPacketLen(0), fLastQualityCheck(0),
                        fAllowNegativeTTs(false), fSpeed(1),
                        fStartTime(-1), fStopTime(-1), fStopTrackID(0), fStopPN(0),
                        fLastRTPTime(0), fLastPauseTime(0),fTotalPauseTime(0), fPaused(false)
        {}
        
        ~FileSession() {}
        
        QTRTPFile           fFile; /* specific file to send RTP packets which close related to QTHintTrack */
        QTSS_PacketStruct   fPacketStruct;/* store rtp packet data to write into rtp stream, used in RTPSession::run() */
        int                 fNextPacketLen;/* length of the above packet data, used in QTRTPFile::GetNextPacket() in QTSSFileModule::SendPackets() */
        SInt64              fLastQualityCheck; /* time of last quality check(ms),see QTSSFileModule::SendPackets() */
        SDPSourceInfo       fSDPSource; /* sdp file/atom of QTFile */
        Bool16              fAllowNegativeTTs;/* allow negative Transmit time of the next packet? default no, see QTSSFileModule::SendPackets() */
        Float32             fSpeed; /* in essence,note the speed is controlled by client !default 1,max 4,speed related to the normal play speed of media file,针对RTSP头"Speed: 2.0\r\n" */
        Float64             fStartTime;/* the time of media file begin to play (s) */
        Float64             fStopTime; /* time to stop(s),针对RTSP头"Range: npt=0.00000-70.00000\r\n" */
		SInt64              fAdjustedPlayTime;/* adjust play time(ms), a relative value,see DoPlay()/SendPackets(),是client点播媒体文件时的时间戳和RTSP Range头"Range: npt=0.00000-70.00000\r\n"中开始播放的时间戳的间隔! */
        
        UInt32              fStopTrackID;/* track ID of stopped track,针对"x-Packet-Range: pn=4551-4689;url=trackID3"  */
        UInt64              fStopPN; /* packet number of stopped packet, used in QTRTPFile::RTPTrackListEntry::HTCB->fCurrentPacketNumber in QTSSFileModule::SendPackets()  */

        UInt32              fLastRTPTime;
        UInt64              fLastPauseTime; /* 上次PAUSE时的时间戳(单位是ms,参见DoPlay()) */
        SInt64              fTotalPauseTime;/* 累积的中断时间 (单位是ms,参见DoPlay()) */
        Bool16              fPaused; /*当前file session的状态是PAUSE吗? */
};

// ref to the prefs dictionary object
/* Initial() to use */
static QTSS_ModulePrefsObject       sPrefs;  /* QTSS Module pref object */
static QTSS_PrefsObject             sServerPrefs; /* server pref object */
static QTSS_Object                  sServer; /* server object */

/* SDP related info   */
static  StrPtrLen sSDPSuffix(".sdp");
static  StrPtrLen sVersionHeader("v=0");
static  StrPtrLen sSessionNameHeader("s=");
static  StrPtrLen sPermanentTimeHeader("t=0 0");
static  StrPtrLen sConnectionHeader("c=IN IP4 0.0.0.0");
static  StrPtrLen sStaticControlHeader("a=control:*"); /* include many details */
static  StrPtrLen sEmailHeader;/* 这两项在BuildPrefBasedHeaders()配置 */
static  StrPtrLen sURLHeader;/* 这两项在BuildPrefBasedHeaders()配置 */
static  StrPtrLen sEOL("\r\n");
static  StrPtrLen sSDPNotValidMessage("Movie SDP is not valid.");

const   SInt16    sNumSDPVectors = 22;/* important param */

// ATTRIBUTES IDs,see QTSS.h,used in Register()
/* both initialize -1 */


//error code
static QTSS_AttributeID sSeekToNonexistentTimeErr       = qtssIllegalAttrID;
static QTSS_AttributeID sNoSDPFileFoundErr              = qtssIllegalAttrID;
static QTSS_AttributeID sBadQTFileErr                   = qtssIllegalAttrID;
static QTSS_AttributeID sFileIsNotHintedErr             = qtssIllegalAttrID;
static QTSS_AttributeID sExpectedDigitFilenameErr       = qtssIllegalAttrID;
static QTSS_AttributeID sTrackDoesntExistErr            = qtssIllegalAttrID;

static QTSS_AttributeID sFileSessionAttr                = qtssIllegalAttrID; /* used frequently here,在哪里设置它的属性? */
/* file session */
static QTSS_AttributeID sFileSessionPlayCountAttrID     = qtssIllegalAttrID;/* 播放次数 */
static QTSS_AttributeID sFileSessionBufferDelayAttrID   = qtssIllegalAttrID; /* 缓冲延时 */
/* RTP Stream send packets */
static QTSS_AttributeID sRTPStreamLastSentPacketSeqNumAttrID   = qtssIllegalAttrID;/* 上次成功发送出的RTP包的序列号,used by QTSSFileModule::SendPackets() */
static QTSS_AttributeID sRTPStreamLastPacketSeqNumAttrID   = qtssIllegalAttrID;/*上次要发送的RTP包的序列号,used by QTSSFileModule::SendPackets() */

// OTHER DATA

/* cf: kQualityCheckIntervalInMsec = 250 in QTSSFileModule::SendPackets() */
static UInt32				sFlowControlProbeInterval	= 10;/* used by block in QTSSFileModule::SendPackets()  */
static UInt32				sDefaultFlowControlProbeInterval= 10;
static Float32              sMaxAllowedSpeed            = 4;/* see API Doc and StreamingServer.xml */
static Float32              sDefaultMaxAllowedSpeed     = 4;

// File Caching Prefs 文件缓存预设值
/* see QTSS API Doc for details */
static Bool16               sEnableSharedBuffers    = false;//能使用共享缓存吗?
static Bool16               sEnablePrivateBuffers   = false;//能使用私有缓存吗?

/* 共享缓存单位 */
static UInt32               sSharedBufferUnitKSize  = 0;
static UInt32               sSharedBufferInc        = 0;
static UInt32               sSharedBufferUnitSize   = 0;
static UInt32               sSharedBufferMaxUnits   = 0;

/* 私有缓存单位 */
static UInt32               sPrivateBufferUnitKSize = 0;
static UInt32               sPrivateBufferUnitSize  = 0;
static UInt32               sPrivateBufferMaxUnits  = 0;

static Float32              sAddClientBufferDelaySecs = 0;/* used in DoDescribe() */

static Bool16               sRecordMovieFileSDP = false;/* whether record movie file? */
static Bool16               sEnableMovieFileSDP = false;/* if represent separately in a individual file or in a built-in atom ? */

static Bool16               sPlayerCompatibility = true;/* 兼容播放器吗? used in DoDescribe() */
static UInt32               sAdjustMediaBandwidthPercent = 50;/* 调整媒体带宽百分比 used in DoDescribe() */

// Server preference we respect
static Bool16               sDisableThinning       = false;/* 能打薄? may thinning */

static const StrPtrLen              kCacheControlHeader("must-revalidate");
static const QTSS_RTSPStatusCode    kNotModifiedStatus  = qtssRedirectNotModified;



  
// FUNCTIONS
/* 着重注意下面这几个函数: ProcessRTSPRequest, DoDescribe,DoSetup,DoPlay,SendPackets */

/* 注意这个函数原型声明 */
static QTSS_Error QTSSFileModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock);


static QTSS_Error Register(QTSS_Register_Params* inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params* inParamBlock);
static QTSS_Error RereadPrefs();
static QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParamBlock);
static QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParamBlock);
static QTSS_Error CreateQTRTPFile(QTSS_StandardRTSP_Params* inParamBlock, char* inPath, FileSession** outFile);/* important tie */
static QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParamBlock);
static QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParamBlock);
static QTSS_Error SendPackets(QTSS_RTPSendPackets_Params* inParams); //note important!
static QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams);
/* additional functions comparing with QTSSRTPFileModule.cpp */
static void       DeleteFileSession(FileSession* inFileSession);
static UInt32   WriteSDPHeader(FILE* sdpFile, iovec *theSDPVec, SInt16 *ioVectorIndex, StrPtrLen *sdpHeader);
static void     BuildPrefBasedHeaders();



/* get the sequence number from the given rtp packet */
inline UInt16 GetPacketSequenceNumber(void * packetDataPtr)
{
    return ntohs( ((UInt16*)packetDataPtr)[1]);
}

/* obtain sRTPStreamLastPacketSeqNum attribute from the given rtp stream */
inline UInt16 GetLastPacketSeqNum(QTSS_Object stream)
{

    UInt16 lastSeqNum = 0;
    UInt32  theLen = sizeof(lastSeqNum);
    (void) QTSS_GetValue(stream, sRTPStreamLastPacketSeqNumAttrID, 0, (void*)&lastSeqNum, &theLen);

    return lastSeqNum;
}

/* obtain the last sent-packet sequence number from current RTP Stream */
inline SInt32 GetLastSentSeqNumber(QTSS_Object stream)
{
    UInt16 lastSeqNum = 0;
    UInt32  theLen = sizeof(lastSeqNum);
    QTSS_Error error = QTSS_GetValue(stream, sRTPStreamLastSentPacketSeqNumAttrID, 0, (void*)&lastSeqNum, &theLen);
    if (error == QTSS_ValueNotFound) // first packet
    {    return -1;
    }

    return (SInt32)lastSeqNum; // return UInt16 seq num value or -1.
} 

/* set seqnumber field of rtp packet data using given seqnumber  */
inline void SetPacketSequenceNumber(UInt16 newSequenceNumber, void * packetDataPtr)
{
    ((UInt16*)packetDataPtr)[1] = htons(newSequenceNumber);
}


/* get timestamp field from given packet  */
/* used by QTSSFileModule::SendPackets()  */
inline UInt32 GetPacketTimeStamp(void * packetDataPtr)
{
    return ntohl( ((UInt32*)packetDataPtr)[1]);
}

/* set timestamp field in the given packet struct with the specific value */
inline void SetPacketTimeStamp(UInt32 newTimeStamp, void * packetDataPtr)
{
    ((UInt32*)packetDataPtr)[1] = htonl(newTimeStamp);
}

/* 如何计算pauseTimeStamp? */
/* 计算该RTPStream的当前中断时间戳(单位是s), used by QTSSFileModule::DoPlay()/SendPackets() */
inline UInt32 CalculatePauseTimeStamp(UInt32 timescale, SInt64 totalPauseTime, UInt32 currentTimeStamp)
{
	/* 计算该RTPStream的总Pause time(单位是s),是个时间段 */
    SInt64 pauseTime = (SInt64) ( (Float64) timescale * ( ( (Float64) totalPauseTime) / 1000.0)); 
	/* 计算该RTPStream的当前中断时间戳(单位是s) */
    UInt32 pauseTimeStamp = (UInt32) (pauseTime + currentTimeStamp);

    return pauseTimeStamp;
}

/* 引用CalculatePauseTimeStamp()/ SetPacketTimeStamp() */
/* used by QTSSFileModule::SendPackets()  */
/* 获取文件会话中的fTotalPauseTime,若为零,直接返回当前时间戳;若不为零,计算出PauseTimeStamp,并设为当前包的时间戳,再返回PauseTimeStamp */
UInt32 SetPausetimeTimeStamp(FileSession *fileSessionPtr, QTSS_Object theRTPStream, UInt32 currentTimeStamp)
{ 
	/* obvious from above CalculatePauseTimeStamp() */
    if (fileSessionPtr->fTotalPauseTime == 0)
        return currentTimeStamp;

    UInt32 timeScale = 0;
    UInt32 theLen = sizeof(timeScale);
	/* obtain trp stream timescale */
	/* 将属性值拷贝到一个缓冲区中,将指定的属性值拷贝到一个例程外部提供的缓冲区中 */
    (void) QTSS_GetValue(theRTPStream, qtssRTPStrTimescale, 0, (void*)&timeScale, &theLen);  
	/* obvious from above CalculatePauseTimeStamp() */
    if (theLen != sizeof(timeScale) || timeScale == 0)
        return currentTimeStamp;

	/* invoke above function to calculate pause timestamp */
    UInt32 pauseTimeStamp = CalculatePauseTimeStamp( timeScale,  fileSessionPtr->fTotalPauseTime, currentTimeStamp);
    if (pauseTimeStamp != currentTimeStamp)
		/* set pause timestamp as packet timestamp */
        SetPacketTimeStamp(pauseTimeStamp, fileSessionPtr->fPacketStruct.packetData);

    return pauseTimeStamp;
}

/* iovec see .\APIStubLib\OSHeaders.h */
/* write sdp header into the given sdp file in vector format and return the length of written info */
/* 将指定的信息先存成iovec向量形式,再写入SDP文件中,并返回写入数据的长度.注意*ioVectorIndex自动加1 */
UInt32 WriteSDPHeader(FILE* sdpFile, iovec *theSDPVec, SInt16 *ioVectorIndex, StrPtrLen *sdpHeader)
{

    Assert (ioVectorIndex != NULL);/* 写在第几行 */
    Assert (theSDPVec != NULL);/* 写的位置处的数据结构 */
    Assert (sdpHeader != NULL);/* 要写的头信息 */
	/* 要写的向量索引号必须小于sdp行数(22) */
    Assert (*ioVectorIndex < sNumSDPVectors); // if adding an sdp param you need to increase sNumSDPVectors
    
	/* 传递行数值并自加一 */
    SInt16 theIndex = *ioVectorIndex;
    *ioVectorIndex += 1;/* note index starts from 0 */

	/* iovec see .\APIStubLib\OSHeaders.h */
	/* 写入指定索引号和长度的sdp行到指定位置的结构体 */
    theSDPVec[theIndex].iov_base =  sdpHeader->Ptr;
    theSDPVec[theIndex].iov_len = sdpHeader->Len;
    
	/* 向SDP文件中写入指定索引号的sdp行info */
    if (sdpFile !=NULL)
		/* 参见谭浩强C p.339 */
        ::fwrite(theSDPVec[theIndex].iov_base,theSDPVec[theIndex].iov_len,sizeof(char),sdpFile);
    
	/* 返回指定写入的信息的长度 */
    return theSDPVec[theIndex].iov_len;
}


/* important function */
/* 模块主函数,used in QTSServer::LoadCompiledInModules() */
QTSS_Error QTSSFileModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSFileModuleDispatch);
}


/* important function,被RTPSession::Run()调用  */
/* 针对不同的角色,输入不同的参数,处理 */
QTSS_Error  QTSSFileModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock)
{
    switch (inRole)
    {
        case QTSS_Register_Role:
            return Register(&inParamBlock->regParams);
        case QTSS_Initialize_Role:
            return Initialize(&inParamBlock->initParams);
        case QTSS_RereadPrefs_Role:
            return RereadPrefs();
        case QTSS_RTSPRequest_Role:/* 处理RTSP request，我们在send packet to use */
            return ProcessRTSPRequest(&inParamBlock->rtspRequestParams);
        case QTSS_RTPSendPackets_Role: /* we are use it in RTPSession.cpp soon */
            return SendPackets(&inParamBlock->rtpSendPacketsParams);
        case QTSS_ClientSessionClosing_Role:
            return DestroySession(&inParamBlock->clientSessionClosingParams);
    }
    return QTSS_NoErr;
}

/* functions used in QTSSFileModuleDispatch() above  */
/* 注册Role,为TextMessagesObject\RTPSession\RTPStream添加静态属性名称及ID,设置模块名称 */
QTSS_Error Register(QTSS_Register_Params* inParams)
{
    // Register for roles
	/* 为本模块注册Role(初始化,读预设值,处理RTSP请求,RTP会话关闭),为何没有QTSS_RTPSendPackets_Role ? */
    (void)QTSS_AddRole(QTSS_Initialize_Role);
    (void)QTSS_AddRole(QTSS_RTSPRequest_Role);
    (void)QTSS_AddRole(QTSS_ClientSessionClosing_Role);
    (void)QTSS_AddRole(QTSS_RereadPrefs_Role);

    // Add text messages attributes
	/* 根据需要添加静态属性(名称后缀为Name)进qtssTextMessagesObjectType,并设置属性ID(后缀Err) */
    static char*        sSeekToNonexistentTimeName  = "QTSSFileModuleSeekToNonExistentTime";
    static char*        sNoSDPFileFoundName         = "QTSSFileModuleNoSDPFileFound";
    static char*        sBadQTFileName              = "QTSSFileModuleBadQTFile";
    static char*        sFileIsNotHintedName        = "QTSSFileModuleFileIsNotHinted";
    static char*        sExpectedDigitFilenameName  = "QTSSFileModuleExpectedDigitFilename";
    static char*        sTrackDoesntExistName       = "QTSSFileModuleTrackDoesntExist";
    
    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSeekToNonexistentTimeName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sSeekToNonexistentTimeName, &sSeekToNonexistentTimeErr);

    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sNoSDPFileFoundName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sNoSDPFileFoundName, &sNoSDPFileFoundErr);

    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sBadQTFileName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sBadQTFileName, &sBadQTFileErr);

    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sFileIsNotHintedName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sFileIsNotHintedName, &sFileIsNotHintedErr);

    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sExpectedDigitFilenameName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sExpectedDigitFilenameName, &sExpectedDigitFilenameErr);

    (void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sTrackDoesntExistName, NULL, qtssAttrDataTypeCharArray);
    (void)QTSS_IDForAttr(qtssTextMessagesObjectType, sTrackDoesntExistName, &sTrackDoesntExistErr);
    
    // Add an RTP session attribute for tracking FileSession objects
	/* 向qtssClientSessionObjectType(也就是RTP Session)添加静态属性(文件会话名,播放次数,SDP缓冲延迟),同时设置属性ID */
    static char*        sFileSessionName    = "QTSSFileModuleSession";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionName, NULL, qtssAttrDataTypeVoidPointer);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionName, &sFileSessionAttr);
    
    static char*        sFileSessionPlayCountName   = "QTSSFileModulePlayCount";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionPlayCountName, NULL, qtssAttrDataTypeUInt32);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionPlayCountName, &sFileSessionPlayCountAttrID);
    
    static char*        sFileSessionBufferDelayName = "QTSSFileModuleSDPBufferDelay";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionBufferDelayName, NULL, qtssAttrDataTypeFloat32);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionBufferDelayName, &sFileSessionBufferDelayAttrID);
    
	/* 向qtssRTPStreamObjectType(也就是RTP Stream)添加静态属性(上一个已送出的RTP包的seqnum,上一个要送出的RTP包的seqnum),同时设置属性ID  */
     static char*        sRTPStreamLastSentPacketSeqNumName   = "QTSSFileModuleLastSentPacketSeqNum";
    (void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sRTPStreamLastSentPacketSeqNumName, NULL, qtssAttrDataTypeUInt16);
    (void)QTSS_IDForAttr(qtssRTPStreamObjectType, sRTPStreamLastSentPacketSeqNumName, &sRTPStreamLastSentPacketSeqNumAttrID);
   

    static char*        sRTPStreamLastPacketSeqNumName   = "QTSSFileModuleLastPacketSeqNum";
    (void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sRTPStreamLastPacketSeqNumName, NULL, qtssAttrDataTypeUInt16);
    (void)QTSS_IDForAttr(qtssRTPStreamObjectType, sRTPStreamLastPacketSeqNumName, &sRTPStreamLastPacketSeqNumAttrID);

    // Tell the server our name!
	/* 告诉Server module名字 */
    static char* sModuleName = "QTSSFileModule";
    ::strcpy(inParams->outModuleName, sModuleName);

    return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
    QTRTPFile::Initialize();
    QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);//对三个静态变量初始化

	/* 下面这三个参数上面开头有定义 */
    sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);/* 得到Module预设值 */
    sServerPrefs = inParams->inPrefs;/* 得到服务器预设值 */
    sServer = inParams->inServer;/* 得到服务器 */
        
    // Read our preferences
	/* 定义见下面 */
    RereadPrefs();
    
    // Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
	/* see RTSPProtocol.h */
    static QTSS_RTSPMethod sSupportedMethods[] = { qtssDescribeMethod, qtssSetupMethod, qtssTeardownMethod, qtssPlayMethod, qtssPauseMethod };
    /* 搭建服务器支持的方法数 */
	QTSSModuleUtils::SetupSupportedMethods(inParams->inServer, sSupportedMethods, 5);

    return QTSS_NoErr;
}

/* 生成sURLHeader和sEmailHeader */
void BuildPrefBasedHeaders()
{
    //build the sdp that looks like: \r\nu=http://streaming.apple.com\r\ne=qts@apple.com.
    static StrPtrLen sUHeader("u=");
    static StrPtrLen sEHeader("e=");
    static StrPtrLen sHTTP("http://");
    static StrPtrLen sAdmin("admin@");

    // Get the default DNS name of the server
    StrPtrLen theDefaultDNS;
    (void)QTSS_GetValuePtr(sServer, qtssSvrDefaultDNSName, 0, (void**)&theDefaultDNS.Ptr, &theDefaultDNS.Len);
    
    //-------- URL Header
    StrPtrLen sdpURL;
    sdpURL.Ptr = QTSSModuleUtils::GetStringAttribute(sPrefs, "sdp_url", "");
    sdpURL.Len = ::strlen(sdpURL.Ptr);
    
    UInt32 sdpURLLen = sdpURL.Len;
    if (sdpURLLen == 0)
        sdpURLLen = theDefaultDNS.Len + sHTTP.Len + 1;/* '/',见下几行 */
        
    sURLHeader.Delete();
    sURLHeader.Len = sdpURLLen + 10;
	/* 生成指定长度 */
    sURLHeader.Ptr = NEW char[sURLHeader.Len];
    StringFormatter urlFormatter(sURLHeader);
    urlFormatter.Put(sUHeader);
    if (sdpURL.Len == 0)
    {
        urlFormatter.Put(sHTTP);
        urlFormatter.Put(theDefaultDNS);
        urlFormatter.PutChar('/');
    }
    else
        urlFormatter.Put(sdpURL);
    
    sURLHeader.Len = (UInt32)urlFormatter.GetCurrentOffset();


    //-------- Email Header
    StrPtrLen adminEmail;
    adminEmail.Ptr = QTSSModuleUtils::GetStringAttribute(sPrefs, "admin_email", "");
    adminEmail.Len = ::strlen(adminEmail.Ptr);
    
    UInt32 adminEmailLen = adminEmail.Len;
    if (adminEmailLen == 0)
        adminEmailLen = theDefaultDNS.Len + sAdmin.Len; 
        
    sEmailHeader.Delete();
    sEmailHeader.Len = (sEHeader.Len * 2) + adminEmailLen + 10;
    sEmailHeader.Ptr = NEW char[sEmailHeader.Len];
    StringFormatter sdpFormatter(sEmailHeader);
    sdpFormatter.Put(sEHeader);
    
    if (adminEmail.Len == 0)
    {
        sdpFormatter.Put(sAdmin);
        sdpFormatter.Put(theDefaultDNS);
    }
    else
        sdpFormatter.Put(adminEmail);
        
    sEmailHeader.Len = (UInt32)sdpFormatter.GetCurrentOffset();
    
    
    sdpURL.Delete();
    adminEmail.Delete();
}

/* used in Initialize() */
/* 从模块和服务器预设值来读取相关的预设值 */
QTSS_Error RereadPrefs()
{
    //设置sFlowControlProbeInterval,sDefaultFlowControlProbeInterval
    QTSSModuleUtils::GetAttribute(sPrefs, "flow_control_probe_interval",    qtssAttrDataTypeUInt32,
                                &sFlowControlProbeInterval, &sDefaultFlowControlProbeInterval, sizeof(sFlowControlProbeInterval));
    //设置sMaxAllowedSpeed,sDefaultMaxAllowedSpeed
    QTSSModuleUtils::GetAttribute(sPrefs, "max_allowed_speed",  qtssAttrDataTypeFloat32,
                                &sMaxAllowedSpeed, &sDefaultMaxAllowedSpeed, sizeof(sMaxAllowedSpeed));
                                
// File Cache prefs     
    //设置sEnableSharedBuffers                
    sEnableSharedBuffers = true;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_shared_file_buffers", qtssAttrDataTypeBool16, &sEnableSharedBuffers,  sizeof(sEnableSharedBuffers));

	//设置sEnablePrivateBuffers
    sEnablePrivateBuffers = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_private_file_buffers", qtssAttrDataTypeBool16, &sEnablePrivateBuffers, sizeof(sEnablePrivateBuffers));

	//设置sSharedBufferInc
    sSharedBufferInc = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_shared_buffer_increase_per_session", qtssAttrDataTypeUInt32,&sSharedBufferInc, sizeof(sSharedBufferInc));
      
	//设置sSharedBufferUnitKSize
    sSharedBufferUnitKSize = 64;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "shared_buffer_unit_k_size", qtssAttrDataTypeUInt32, &sSharedBufferUnitKSize, sizeof(sSharedBufferUnitKSize));

	//设置sPrivateBufferUnitKSize
    sPrivateBufferUnitKSize = 64;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "private_buffer_unit_k_size", qtssAttrDataTypeUInt32, &sPrivateBufferUnitKSize, sizeof(sPrivateBufferUnitKSize));

	//设置sSharedBufferUnitSize
    sSharedBufferUnitSize = 1;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_shared_buffer_units_per_buffer", qtssAttrDataTypeUInt32,&sSharedBufferUnitSize, sizeof(sSharedBufferUnitSize));

	//设置sPrivateBufferUnitSize
    sPrivateBufferUnitSize = 1;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_private_buffer_units_per_buffer", qtssAttrDataTypeUInt32,&sPrivateBufferUnitSize, sizeof(sPrivateBufferUnitSize));
     
	//设置sSharedBufferMaxUnits
    sSharedBufferMaxUnits = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "max_shared_buffer_units_per_buffer", qtssAttrDataTypeUInt32, &sSharedBufferMaxUnits, sizeof(sSharedBufferMaxUnits));
      
	//设置sPrivateBufferMaxUnits
    sPrivateBufferMaxUnits = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "max_private_buffer_units_per_buffer", qtssAttrDataTypeUInt32, &sPrivateBufferMaxUnits, sizeof(sPrivateBufferMaxUnits));

	//设置sAddClientBufferDelaySecs
    sAddClientBufferDelaySecs = 0;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "add_seconds_to_client_buffer_delay", qtssAttrDataTypeFloat32, &sAddClientBufferDelaySecs, sizeof(sAddClientBufferDelaySecs));

	//设置sRecordMovieFileSDP
    sRecordMovieFileSDP = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "record_movie_file_sdp", qtssAttrDataTypeBool16, &sRecordMovieFileSDP, sizeof(sRecordMovieFileSDP));

	//设置sEnableMovieFileSDP
    sEnableMovieFileSDP = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_movie_file_sdp", qtssAttrDataTypeBool16, &sEnableMovieFileSDP, sizeof(sEnableMovieFileSDP));
    
	//设置sPlayerCompatibility
    sPlayerCompatibility = true;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_player_compatibility", qtssAttrDataTypeBool16, &sPlayerCompatibility, sizeof(sPlayerCompatibility));

	/* adjust media bandwidth percent */
	//设置sAdjustMediaBandwidthPercent
    sAdjustMediaBandwidthPercent = 50;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "compatibility_adjust_sdp_media_bandwidth_percent", qtssAttrDataTypeUInt32, &sAdjustMediaBandwidthPercent, sizeof(sAdjustMediaBandwidthPercent));

    if (sAdjustMediaBandwidthPercent > 100)
        sAdjustMediaBandwidthPercent = 100;
        
    if (sAdjustMediaBandwidthPercent < 1)
        sAdjustMediaBandwidthPercent = 1;
     
	/* permit thinning */
	//从服务器获取参数sDisableThinning
    UInt32 len = sizeof(sDisableThinning);
    (void) QTSS_GetValue(sServerPrefs, qtssPrefsDisableThinning, 0, (void*)&sDisableThinning, &len);

    BuildPrefBasedHeaders();
    
    return QTSS_NoErr;
}

/* 与RTSP连接的接口，依据获得的RTSP Request具体方法分情形作出处理 */
QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParamBlock)
{
	/* 从client发送的标准RTSP会话中获取RTSP方法,若方法不合法,返回请求失败 */
    QTSS_RTSPMethod* theMethod = NULL;
    UInt32 theMethodLen = 0;
    if ((QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqMethod, 0,
            (void**)&theMethod, &theMethodLen) != QTSS_NoErr) || (theMethodLen != sizeof(QTSS_RTSPMethod)))
    {
        Assert(0);
        return QTSS_RequestFailed;
    }
    
	/* 针对获取的方法分情形讨论,调用不同的函数处理 */
    QTSS_Error err = QTSS_NoErr;
    switch (*theMethod)
    {
        case qtssDescribeMethod:
            err = DoDescribe(inParamBlock);
            break;
        case qtssSetupMethod:
            err = DoSetup(inParamBlock);
            break;
        case qtssPlayMethod:
            err = DoPlay(inParamBlock);
            break;
        case qtssTeardownMethod:
            (void)QTSS_Teardown(inParamBlock->inClientSession);
			/* 同时通知客户端要中断连接 */
            (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
            break;
        case qtssPauseMethod:
        {    (void)QTSS_Pause(inParamBlock->inClientSession);
		    /* 同时通知客户端要暂停连接 */
            (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
              
			/* 获取文件会话属性,若失败,返回会话失败 */
            FileSession** theFile = NULL;
            UInt32 theLen = 0;
            QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
            if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
                return QTSS_RequestFailed;

			/* 同时及时记录暂停时间信息/戳 */
            (**theFile).fPaused = true;
            (**theFile).fLastPauseTime = OS::Milliseconds();

            break;
        }
        default:
            break;
    }
	/* 对各种结果的情形作处理,如果结果不正常就中断会话 */
    if (err != QTSS_NoErr)
        (void)QTSS_Teardown(inParamBlock->inClientSession);

    return QTSS_NoErr;
}

/* 判断媒体文件的本地地址的最后几个字符是否是".sdp",是就返回true,否就返回false  */
Bool16 isSDP(QTSS_StandardRTSP_Params* inParamBlock)
{
	Bool16 sdpSuffix = false;
	
    char* path = NULL;
    UInt32 len = 0;
	/* lock QTSS_RTSPRequestObject object */
	QTSS_LockObject(inParamBlock->inRTSPRequest);
	/* 从Client的RTSP Request中获取请求媒体文件的本地地址 */
    QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&path, &len);
    Assert(theErr == QTSS_NoErr);
	
    if (sSDPSuffix.Len <= len)
	{
		/*判断媒体文件的本地地址的最后几个字符是否是".sdp",是就设置sdpSuffix为false */
		StrPtrLen thePath(&path[len - sSDPSuffix.Len],sSDPSuffix.Len);
		sdpSuffix = thePath.Equal(sSDPSuffix);
	}
	
	/* unlock QTSS_RTSPRequestObject object */
	QTSS_UnlockObject(inParamBlock->inRTSPRequest);
	
	return sdpSuffix;
}

/* 注意这几个量:thePath,thePathLen, theSDPVec,theSDPData,theFullSDPBuffer,fullSDPBuffSPL */
QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParamBlock)
{
	/* 假如指定的RTSP Request中的Local file path的最末几个字符不是".sdp",就向Client发送"指定路径的文件没有SDP文件的"错误 */
	/* 注意这里的处理和DoSetup()/DoPlay()是相同的，参照那里 */
    if (isSDP(inParamBlock))
    {
        StrPtrLen pathStr;
        (void)QTSS_LockObject(inParamBlock->inRTSPRequest);
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, (void**)&pathStr.Ptr, &pathStr.Len);
        QTSS_Error err = QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest, qtssClientNotFound, sNoSDPFileFoundErr, &pathStr);
        (void)QTSS_UnlockObject(inParamBlock->inRTSPRequest);
        return err;
    }
    
    // Get the FileSession for this DESCRIBE, if any.
    UInt32 theLen = sizeof(FileSession*);
    FileSession*    theFile = NULL; /* 定义文件会话下面使用 */
    QTSS_Error      theErr = QTSS_NoErr;
    Bool16          pathEndsWithMOV = false;/* 路径末尾以MOV结束? */
    static StrPtrLen sMOVSuffix(".mov");
    SInt16 vectorIndex = 1; /* 定位到SDP文件的第一行 */
    ResizeableStringFormatter theFullSDPBuffer(NULL,0);/* 本地存放SDP文件的缓存 */
    StrPtrLen bufferDelayStr;/* sdp文件中的缓冲延迟字符串,应用见下面 */
    char tempBufferDelay[64];/* 存放缓冲延迟的字符串"a=x-bufferdelay:0.2254",长度为64个字符,应用见下面 */
    StrPtrLen theSDPData;/* 存放从原SDP文件中读取的数据,不论是单独的sdp文件,还是从sdp原子获得的信息.sdp data which may be a isolated file or a built-in file in udta.hnti.sdp atom */
      
	/*  获取FileSession的指针及长度 */
    (void)QTSS_GetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, (void*)&theFile, &theLen);
    // Generate the complete file path
    UInt32 thePathLen = 0;
	/* thePath对象是request的媒体文件的本地完整文件路径(如"/sample_300kbit.mp4"),文件路径长度为thePathLen,包含后缀'.sdp'(后加的) */
    OSCharArrayDeleter thePath(QTSSModuleUtils::GetFullPath(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath,&thePathLen, &sSDPSuffix));
        
    //first locate the target movie
	/* 截断'.sdp',判断RTSP Request的媒体文件是否mov文件? */
    thePath.GetObject()[thePathLen - sSDPSuffix.Len] = '\0';//truncate the .sdp added in the GetFullPath call
    StrPtrLen   requestPath(thePath.GetObject(), ::strlen(thePath.GetObject()));//get request path
	/* 类似isSDP()处理,确定请求的路径是否是以'.MOV'结尾? 在该路径末端取一段与'.mov'等长的一段进行比较 */
    if (requestPath.Len > sMOVSuffix.Len )
    {   
		StrPtrLen endOfPath(&requestPath.Ptr[requestPath.Len - sMOVSuffix.Len], sMOVSuffix.Len);
        if (endOfPath.Equal(sMOVSuffix)) // it is a .mov
        {  
			pathEndsWithMOV = true;
        }
    }
    
	/* 假如已存在文件会话File Session,比较RTSP Request请求的媒体文件和FileSession中的文件名是否相同,假如不同,删去旧的FileSession */
    if ( theFile != NULL )  
    {
        // There is already a file for this session. This can happen if there are multiple DESCRIBES,
        // or a DESCRIBE has been issued with a Session ID, or some such thing.
		/* 从File Session中取出Movie路径而非从RTSP request中取出Movie路径 */
        StrPtrLen   moviePath( theFile->fFile.GetMoviePath() ); /* get movie path(如"/sample_300kbit.mp4") */
        
        // This describe is for a different file. Delete the old FileSession.
		/* 比较RTSP Request请求的媒体文件和FileSession中的文件名是否相同,假如不同,删去旧的FileSession */
        if ( !requestPath.Equal( moviePath ) )
        {
            DeleteFileSession(theFile);
            theFile = NULL;
            
            // NULL out the attribute value, just in case(清空该属性值仅以防万一).
            (void)QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
        }
    }

	/* 假如不存在文件会话File Session,就创建FileSession对象,创建其指定本地完整文件路径的QTRTPFile,并存储FileSessionAttr在ClientSession(即RTP session)中 */
    if ( theFile == NULL )
    {   
		/* 动态创建FileSession对象并初始化其数据成员QTRTPFile,并在错误时,向Client回送错误原因 ,并直接返回 */
        theErr = CreateQTRTPFile(inParamBlock, thePath.GetObject(), &theFile);
        if (theErr != QTSS_NoErr)
            return theErr;
    
        // Store this newly created file object in the RTP session.
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
    }
    
    //replace the sacred character we have trodden on in order to truncate the path.
	/* thePath stores full file path with length thePathLen and ended by 'mov' */
	/* 截掉末尾等长段,换上'.sdp' */
    thePath.GetObject()[thePathLen - sSDPSuffix.Len] = sSDPSuffix.Ptr[0];

    iovec theSDPVec[sNumSDPVectors];//注意这个量非常重要,1 for the RTSP header, 6 for the sdp header, 1 for the sdp body,size 22
    ::memset(&theSDPVec[0], 0, sizeof(theSDPVec));/* 初始化SDP文件第一行?? */
    
	/* if permit present the sdp file of movie */
    if (sEnableMovieFileSDP)
    {
        // Check to see if there is an sdp file, if so, return that file instead
        // of the built-in sdp. ReadEntireFile allocates memory but if all goes well theSDPData will be managed by the File Session
		/* 从指定的文件路径读取sdp数据进theSDPData(C-String) */
		/********************* 注意此处向theSDPData读入数据*********************************/
        (void)QTSSModuleUtils::ReadEntireFile(thePath.GetObject(), &theSDPData);
    }

	/***************** 下面开始解析指定的sdp文件数据 ***************/
    OSCharArrayDeleter sdpDataDeleter(theSDPData.Ptr); // Just in case we fail we know to clean up. But we clear the deleter if we succeed.

	/* 假如sdp文件存在(它是从指定路径的sdp文件读入的),将其存入指定缓存theSDPData,解析其是否合法?然后附加ModDate头和CacheControl头 */
    if (theSDPData.Len > 0)
    {  
		/* related defs see SDPUtils.h */
        SDPContainer fileSDPContainer; 
		/* set sdp buffer,parse it and add it as the next sdpline of current line */
        fileSDPContainer.SetSDPBuffer(&theSDPData);
		/* check sdp file buffer if valid ? */
        if (!fileSDPContainer.IsSDPBufferValid())
			/* 向客户端发消息作错误响应"Movie SDP is not valid.",qtssUnsupportedMediaType是错误状态代码,sSDPNotValidMessage是Message Text String */
        {    return QTSSModuleUtils::SendErrorResponseWithMessage(inParamBlock->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
        }
    
        
		/* if added sdp line is valid */
        // Append the Last Modified header to be a good caching proxy citizen before sending the Describe
		/* QTSS_AppendRTSPHeader回调例程将报头信息附加到RTSP报头中。在调用QTSS_AppendRTSPHeader函数之后，
		可以紧接着调用QTSS_SendRTSPHeaders函数来发送整个报头。 */
		/*
		   向客户端回复如下内容:
		   Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
		   Cache-Control: must-revalidate\r\n
		*/
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssLastModifiedHeader,
                                        theFile->fFile.GetQTFile()->GetModDateStr(), DateBuffer::kDateBufferLen);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssCacheControlHeader,
                                        kCacheControlHeader.Ptr, kCacheControlHeader.Len);

        //Now that we have the SDP file data, send an appropriate describe response to the client
		//NOTE: THE FIRST ENTRY OF THE IOVEC MUST BE EMPTY!!!!
		
        theSDPVec[1].iov_base = theSDPData.Ptr;
        theSDPVec[1].iov_len = theSDPData.Len;

		/* Using the SDP data provided in the iovec, this function sends a standard describe response.
		//NOTE: THE FIRST ENTRY OF THE IOVEC MUST BE EMPTY!!!! */
        QTSSModuleUtils::SendDescribeResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession,
                                                                &theSDPVec[0], 3, theSDPData.Len); //3个iovec 
    }
    else  /* 假如theSDPData没有值 */
    {
        // Before generating the SDP and sending it, check to see if there is an If-Modified-Since
        // date. If there is, and the content hasn't been modified, then just return a 304 Not Modified
		// 检查RTSP Request中是否有If-Modified-Since头,假如有,并与QTFile中记录的modified date相同,就向Client反馈"RTSP/1.0 304 Not Modified",并直接返回
        QTSS_TimeVal* theTime = NULL;
        (void) QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqIfModSinceDate, 0, (void**)&theTime, &theLen);
        if ((theLen == sizeof(QTSS_TimeVal)) && (*theTime > 0))
        {
            // There is an If-Modified-Since header. Check it vs. the content.
            if (*theTime == theFile->fFile.GetQTFile()->GetModDate())
            {
				/* set RTSP Request status code为:"RTSP/1.0 304 Not Modified" */
                theErr = QTSS_SetValue( inParamBlock->inRTSPRequest, qtssRTSPReqStatusCode, 0,
                                        &kNotModifiedStatus, sizeof(kNotModifiedStatus) );
                Assert(theErr == QTSS_NoErr);
                // Because we are using this call to generate a 304 Not Modified response, we do not need
                // to pass in a RTP Stream /* 故inFlags域为0 */
                theErr = QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
                Assert(theErr == QTSS_NoErr);
                return QTSS_NoErr;
            }
        }
        
		/**************** 本地SDP文件指针!!在允许生成SDP文件的条件下,仅对MOV文件生成指定路径的sdp文件 ********************/
        FILE* sdpFile = NULL;
        if (sRecordMovieFileSDP && pathEndsWithMOV) // don't auto create sdp for a non .mov file because it would look like a broadcast
        {   
			/* open a local sdp file in specific path and in read-only means */
            sdpFile = ::fopen(thePath.GetObject(),"r"); // see if there already is a .sdp for the movie
			/* 假如该路径已经有一个SDP文件,不要破坏它,当然此处也不用读入数据了 */
            if (sdpFile != NULL) // one already exists don't mess with it
            {   ::fclose(sdpFile);
                sdpFile = NULL;
            }
            else
                sdpFile = ::fopen(thePath.GetObject(),"w"); // create the .sdp
        }
        
        UInt32 totalSDPLength = 0;
        
        //Get filename 
        //StrPtrLen fileNameStr;
        //(void)QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, (void**)&fileNameStr.Ptr, (UInt32*)&fileNameStr.Len);

		/* 获取RTSP Request的文件路径(如"/sample_300kbit.mp4"),存入fileNameStr */
        char* fileNameStr = NULL;
        (void)QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, &fileNameStr);
        QTSSCharArrayDeleter fileNameStrDeleter(fileNameStr);/* 做好以后删除的准备 */
        	
        //Get IP addr
		/* 获取RTSP Session的本地IP地址(以带点的十进制数格式表示) */
        StrPtrLen ipStr;
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPSession, qtssRTSPSesLocalAddrStr, 0, (void**)&ipStr.Ptr, &ipStr.Len);


//      
// *** The order of sdp headers is specified and required by rfc 2327
//
// -------- version header 

		/* 向缓存中写入"v=0\r\n" */
        theFullSDPBuffer.Put(sVersionHeader);
        theFullSDPBuffer.Put(sEOL);
        
// -------- owner header

        const SInt16 sLineSize = 256;
        char ownerLine[sLineSize]="";/* 创建长度为256个字符的空行 */
        ownerLine[sLineSize - 1] = 0;/* 该行最后一个字符为NULL */
        
        char *ipCstr = ipStr.GetAsCString();/* 得到RTSP Session的本地IP地址(见上面)的C-string */
        OSCharArrayDeleter ipDeleter(ipCstr);/* 做好删除准备 */
        
        // the first number is the NTP time used for the session identifier (this changes for each request)
        // the second number is the NTP date time of when the file was modified (this changes when the file changes)
        qtss_sprintf(ownerLine, "o=StreamingServer %"_64BITARG_"d %"_64BITARG_"d IN IP4 %s", (SInt64) OS::UnixTime_Secs() + 2208988800LU, (SInt64) theFile->fFile.GetQTFile()->GetModDate(),ipCstr);
        Assert(ownerLine[sLineSize - 1] == 0); /* 确保该行最后一个字符没被改动，初始定义见上 */

		/* 向缓存中写入形为"o=StreamingServer 3487035788 1259131481000 IN IP4 172.16.34.22\r\n"的行 */ 
        StrPtrLen ownerStr(ownerLine);
        theFullSDPBuffer.Put(ownerStr); 
        theFullSDPBuffer.Put(sEOL); 
        
// -------- session header

		/* 向缓存中写入诸如"s=/sample_300kbit.mp4\r\n" */ 
        theFullSDPBuffer.Put(sSessionNameHeader);
        theFullSDPBuffer.Put(fileNameStr);
        theFullSDPBuffer.Put(sEOL);
    
// -------- uri header

		/* 向缓存中写入诸如"u=http:///\r\n" */
        theFullSDPBuffer.Put(sURLHeader);
        theFullSDPBuffer.Put(sEOL);

    
// -------- email header

		/* 向缓存中写入诸如"e=admin@\r\n" */ 
        theFullSDPBuffer.Put(sEmailHeader);
        theFullSDPBuffer.Put(sEOL);

// -------- connection information header
        
		/* 向缓存中写入诸如"c=IN IP4 0.0.0.0\r\n" */
        theFullSDPBuffer.Put(sConnectionHeader); 
        theFullSDPBuffer.Put(sEOL);

// -------- time header

        // t=0 0 is a permanent always available movie (doesn't ever change unless we change the code)
		/* 向缓存中写入诸如"t=0 0\r\n" */
        theFullSDPBuffer.Put(sPermanentTimeHeader);
        theFullSDPBuffer.Put(sEOL);
        
// -------- control header

		/* 向缓存中写入诸如"a=control:*\r\n" */
        theFullSDPBuffer.Put(sStaticControlHeader);
        theFullSDPBuffer.Put(sEOL);
        
                
// -------- add buffer delay:"a=x-bufferdelay:00000.00\r\n" 

		/* 假如Client端有缓冲延时,搜寻theSDPData(它存储旧SDP文件的数据)中原有的"a=x-bufferdelay:"开头的字符串,再加入Client端的缓冲延时值,注意下面的设置是一个层层嵌套传递结构 */
        if (sAddClientBufferDelaySecs > 0) // increase the client buffer delay by the preference amount.
        {
            Float32 bufferDelay = 3.0; // 这个值是猜的:the client doesn't advertise it's default value so we guess.
            
            static StrPtrLen sBuffDelayStr("a=x-bufferdelay:");
        
            StrPtrLen delayStr;
			/* 从theSDPData中查找以"a=x-bufferdelay:"开头的字符串并赋值给delayStr */
			/* theSDPData的赋值在上面 */
            theSDPData.FindString(sBuffDelayStr, &delayStr);
			/* 判断获取的字符串delayStr,找出浮点数给bufferDelay */
            if (delayStr.Len > 0)
            {
				/* 加入delayStr后其末端对theSDPData首部的偏移 */
                UInt32 offset = (delayStr.Ptr - theSDPData.Ptr) + delayStr.Len; // step past the string
				/* 注意delayStr是theSDPData数据越过原delayStr后的剩余部分 */
                delayStr.Ptr = theSDPData.Ptr + offset;
                delayStr.Len = theSDPData.Len - offset;
				/* 生成StringParser类对象并初始化 */
                StringParser theBufferSecsParser(&delayStr);
				/* 吸收掉头部空格 */
                theBufferSecsParser.ConsumeWhitespace();
				/* 取出其中的浮点数给bufferDelay,其初始化值为3 */
                bufferDelay = theBufferSecsParser.ConsumeFloat();
            }
            
			/* 在有服务器缓冲时,将服务器和客户端BufferDelay相加,否则,只加上客户端缓冲 */
            bufferDelay += sAddClientBufferDelaySecs;

            /* 以指定格式存入暂时的字符串tempBufferDelay,定义见本函数开头几行 */
            qtss_sprintf(tempBufferDelay, "a=x-bufferdelay:%.2f",bufferDelay);
            bufferDelayStr.Set(tempBufferDelay);//设置缓冲延迟字符串bufferDelayStr 
             
			/* 向缓存中写入诸如"a=x-bufferdelay:00000.00\r\n" */
            theFullSDPBuffer.Put(bufferDelayStr); 
            theFullSDPBuffer.Put(sEOL);
        }
        
 // -------- Add the movie's sdp headers to our sdp headers

        //now append content-determined sdp ( cached in QTRTPFile ),形如
		/*
		a=mpeg4-iod:"data:application/mpeg4-iod;base64,AoJrAE///w/z/wOBdgABQNhkYXRhOmFwcGxpY2F0aW9uL21wZWc0LW9kLWF1O2Jhc2U2NCxBWUVDQVV3Rkh3TklBTWtnQUdVRUx5QVJBVUpFQUFOYllBQURXMkFGSUFBQUFiRHpBQUFCdFE3Z1FNRFBBQUFCQUFBQUFTQUFoRUQ2S0Y4aDRLSWZCaEFBUkFBQUFCNEFBQUFBSUFBQUFBQURBVElDbndNdUFHVUFCSUNBZ0JSQUZRQVlBQUFBdTRBQUFMdUFCWUNBZ0FJVGtBWVFBRVFBQUZZaUFBQldJaUFnQUFBQUF3PT0EDQEFAADIAAAAAAAAAAAGCQEAAAAAAAAAAANpAAJARmRhdGE6YXBwbGljYXRpb24vbXBlZzQtYmlmcy1hdTtiYXNlNjQsd0JBU2daTUNvRmNtRUVIOEFBQUIvQUFBQkVLQ0tDbjQEEgINAABkAAAAAAAAAAAFAwAAYAYJAQAAAAAAAAAA"\r\n
		a=isma-compliance:1,1.0,1\r\n
		a=range:npt=0-  70.00000\r\n
		m=video 0 RTP/AVP 96\r\n
		b=AS:209\r\n
		a=rtpmap:96 MP4V-ES/90000\r\n
		a=control:trackID=3\r\n
		a=cliprect:0,0,480,380\r\n
		a=framesize:96 380-480\r\n
		a=fmtp:96 profile-level-id=1;config=000001B0F3000001B50EE040C0CF0000010000000120008440FA285F21E0A21F\r\n
		a=mpeg4-esid:201\r\n
		m=audio 0 RTP/AVP 97\r\n
		b=AS:48\r\n
		a=rtpmap:97 mpeg4-generic/22050/2\r\n
		a=control:trackID=4\r\n
		a=fmtp:97 profile-level-id=15;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1390\r\n
		a=mpeg4-esid:101\r\n
		*/
        int sdpLen = 0;
		/* obtain SDP file in QTRTPFile and give it to theSDPData whose length is sdpLen */
		/********************* 注意此处向theSDPData读入媒体文件中的SDP数据!!*********************************/
        theSDPData.Ptr = theFile->fFile.GetSDPFile(&sdpLen);
        theSDPData.Len = sdpLen; 
		/* 将QTRTPFile中的SDP数据读进来,对每个audio/video track,各有一段描述信息 */
		theFullSDPBuffer.Put(theSDPData);


		/********************* 下面开始解析缓存的的SDP数据*************************************************/
		/* 重新定义一个新的StrPtrLen的对象,用前对象theFullSDPBuffer的起始指针和已写长度字节来初始化 */
        StrPtrLen fullSDPBuffSPL(theFullSDPBuffer.GetBufPtr(),theFullSDPBuffer.GetBytesWritten());

// ------------ Check the headers
		/* 检查各种头的合法性,并作错误处理 */
		/* 注意: SetSDPBuffer()已经包含了Initialize(),Parse(),将合法的sdp行写入当前行的下一行,并返回合法性与否的结论*/
        SDPContainer rawSDPContainer;
		/* input the given string, store in buffer, then parse its validity and add it to the next sdp line to current line */
        rawSDPContainer.SetSDPBuffer( &fullSDPBuffSPL ); 
		/* 假如SDP信息不合法,就向客户端发送一条消息"Movie SDP is not valid.",并总是返回QTSS_RequestFailed */
        if (!rawSDPContainer.IsSDPBufferValid())
        {    
			return QTSSModuleUtils::SendErrorResponseWithMessage(inParamBlock->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
        }
		
// ------------ reorder the sdp headers to make them proper.以恰当顺序重排SDP头
        Float32 adjustMediaBandwidthPercent = 1.0;
        Bool16 adjustMediaBandwidth = false;
        if (sPlayerCompatibility )//该值默认为true,兼容播放器
			/* 查找服务器预设值对象中是否有AdjustBandwidth这项,返回bool值 */
            adjustMediaBandwidth = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParamBlock,QTSSModuleUtils::kAdjustBandwidth);
		    
		if (adjustMediaBandwidth)
		    adjustMediaBandwidthPercent = (Float32) sAdjustMediaBandwidthPercent / 100.0;//设置调整带宽百分比为50%,它在下面的sortedSDP对象中要用

// ----------- get session header and media header from sdp cache

		SDPLineSorter sortedSDP(&rawSDPContainer,adjustMediaBandwidthPercent);
		/* 以sSessionOrderedLines[]= "vosiuepcbtrzka"为固定顺序,将以相同的字母开头的行放在一起,形成fSDPSessionHeaders */
		StrPtrLen *theSessionHeadersPtr = sortedSDP.GetSessionHeaders();
		/* 对每个auido/video track,从对应的m开头的行以后找到可能的b开头的行,调整其MediaBandwidth,并初始化成员变量fMediaHeaders */
		StrPtrLen *theMediaHeadersPtr = sortedSDP.GetMediaHeaders();
		
// ----------- write out the sdp

		/* 将theSessionHeadersPtr和theMediaHeadersPtr依次写入sdp file,注意*ioVectorIndex自动加1 */
		totalSDPLength += ::WriteSDPHeader(sdpFile, theSDPVec, &vectorIndex, theSessionHeadersPtr);//第1个向量,第0个向量空着吗?
        totalSDPLength += ::WriteSDPHeader(sdpFile, theSDPVec, &vectorIndex, theMediaHeadersPtr); //第2个向量
 

// -------- done with SDP processing
        /* 最后记得关闭打开的SDP文件 */
        if (sdpFile !=NULL)
            ::fclose(sdpFile);
            

        Assert(theSDPData.Len > 0);//注意,此时theSDPData存储的是从sdp原子中获得的信息
        Assert(theSDPVec[2].iov_base != NULL);//这是第3个iovec
        //ok, we have a filled out iovec. Let's send the response!
        
        // Append the Last Modified header to be a good caching proxy citizen before sending the Describe
		/*
		   向客户端回复如下内容:
		   Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
		   Cache-Control: must-revalidate\r\n
		*/
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssLastModifiedHeader,
                                        theFile->fFile.GetQTFile()->GetModDateStr(), DateBuffer::kDateBufferLen);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssCacheControlHeader,
                                        kCacheControlHeader.Ptr, kCacheControlHeader.Len);
		/* 同理向客户端发送Describe response,注意vectorIndex值为3 */
        QTSSModuleUtils::SendDescribeResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession,
                                                                        &theSDPVec[0], vectorIndex, totalSDPLength);    
    }
    
	/* 为下面的Parse()做准备,此时theSDPData存放的是从SDP文件或从sdp原子读取的信息 */
    Assert(theSDPData.Ptr != NULL);
    Assert(theSDPData.Len > 0);

    //now parse the movie media sdp data. We need to do this in order to extract payload(audio or video) information.
    //The SDP parser object will not take responsibility of the memory (one exception... see above)
	/* 这个函数的作用是从已有的sdp信息中提取出基类成员fStreamArray的结构体StreamInfo的信息,并配置fStreamArray的各项信息 */
    theFile->fSDPSource.Parse(theSDPData.Ptr, theSDPData.Len);
    sdpDataDeleter.ClearObject(); // don't delete theSDPData, theFile has it now.
    
    return QTSS_NoErr;
}

/* 动态创建FileSession对象并初始化其数据成员QTRTPFile,并在错误时,向Client回送错误原因 */
/* used in QTSSFileModule::DoSetup()/DoDescribe() */
QTSS_Error CreateQTRTPFile(QTSS_StandardRTSP_Params* inParamBlock, char* inPath, FileSession** outFile)
{   
	/* 动态创建FileSession对象并初始化其数据成员QTRTPFile */
    *outFile = NEW FileSession();
	/* 初始化QTRTPFile时将其track全变为hint track */
    QTRTPFile::ErrorCode theErr = (*outFile)->fFile.Initialize(inPath);
	/* erro handling */
    if (theErr != QTRTPFile::errNoError)
    {
        delete *outFile;
        *outFile = NULL;

		/* acquire rtsp file path string */
        char* thePathStr = NULL;
        (void)QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, &thePathStr);
        QTSSCharArrayDeleter thePathStrDeleter(thePathStr); /* creat to prepare to delete soon */
        StrPtrLen thePath(thePathStr);        
		
		/* 对各种错误情况分情形处理 */
        if (theErr == QTRTPFile::errFileNotFound)
            return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                        qtssClientNotFound,
                                                        sNoSDPFileFoundErr,&thePath);
        if (theErr == QTRTPFile::errInvalidQuickTimeFile)
            return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                        qtssUnsupportedMediaType,
                                                        sBadQTFileErr,&thePath);
        if (theErr == QTRTPFile::errNoHintTracks)
            return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                        qtssUnsupportedMediaType,
                                                        sFileIsNotHintedErr,&thePath);
        if (theErr == QTRTPFile::errInternalError)
            return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                        qtssServerInternal,
                                                        sBadQTFileErr,&thePath);

        AssertV(0, theErr);
    }
    
    return QTSS_NoErr;
}

/* 根据RTSP Request请求,搭建指定track id的RTPStream,设置其payload的名称,类型,缓冲延时,timescale,quality level个数,并同步设置QTRTPFile中指定track的
SSRC,payload type及RTPMetaInfo,并向Client发送一个标准RTSP响应 */
QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParamBlock)
{
    /* 假如指定的RTSP Request中的Local file path的最末几个字符不是".sdp",就向Client发送"指定路径的文件没有SDP文件的"错误 */
	/* 注意这里的处理和DoDescribe()/DoPlay()是相同的，参照那里 */
    if (isSDP(inParamBlock))
    {
        StrPtrLen pathStr;
        (void)QTSS_LockObject(inParamBlock->inRTSPRequest);
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, (void**)&pathStr.Ptr, &pathStr.Len);
        QTSS_Error err = QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest, qtssClientNotFound, sNoSDPFileFoundErr, &pathStr);
        (void)QTSS_UnlockObject(inParamBlock->inRTSPRequest);
        return err;
    }
    
    //setup this track in the file object 
    FileSession* theFile = NULL;
    UInt32 theLen = sizeof(FileSession*);
	/* 获取FileSession对象指针,假如出错,就仿照DoDescribe()将相应工作准备好 */
    QTSS_Error theErr = QTSS_GetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, (void*)&theFile, &theLen);
	/* 假如获取File Session出了问题，就重新设置File Session Attr */
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
    {
		//Get the full local path to the file.
        char* theFullPath = NULL;
        //theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&theFullPath, &theLen);
		/* QTSS_GetValueAsString回调例程获取指定属性的值，并转换为C字符串格式，存储在由theFullPath参数指定的内存位置上 */
		/* 当您不再需要theFullPath时，需要调用QTSS_Delete函数来释放为其分配的内存。调用QTSS_GetValue函数是获取非抢占访问安全
		   的属性值的推荐方法，而调用QTSS_GetValuePtr函数则是获取抢占访问安全的属性值的推荐方法。 */
		theErr = QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, &theFullPath);
        Assert(theErr == QTSS_NoErr);
        // This is possible, as clients are not required to send a DESCRIBE. If we haven't set
        // anything up yet, set everything up
		/* creat QTRTPFile ( belongs to FileSession theFile ) at the specific path   */
        theErr = CreateQTRTPFile(inParamBlock, theFullPath, &theFile);
		/* 当您不再需要theFullPath时，需要调用QTSS_Delete函数来释放为其分配的内存。 */
		QTSS_Delete(theFullPath);
        if (theErr != QTSS_NoErr)
            return theErr;
 
		/* get sdp data from just created QTRTPFile */
        int theSDPBodyLen = 0;
		/* see QTRTPFile::GetSDPFile(), use it immediately if sdp file already exist,otherwise create a new one  */
		/* 得到SDP文件的缓存位置和长度 */
        char* theSDPData = theFile->fFile.GetSDPFile(&theSDPBodyLen);

		
        //now parse the sdp. We need to do this in order to extract payload information(video or audio?).
        //The SDP parser object will not take responsibility of the memory (one exception... see above)
        theFile->fSDPSource.Parse(theSDPData, theSDPBodyLen);

        // Store this newly created file object in the RTP session.
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
    }

    //unless there is a digit at the end of this path (representing trackID), don't
    //even bother with the request
	/* 获取"SETUP rtsp://172.16.34.22/sample_300kbit.mp4/trackID=3 RTSP/1.0\r\n"结尾的一个或者更多数字 */
    char* theDigitStr = NULL;
    (void)QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqFileDigit, 0, &theDigitStr);
    QTSSCharArrayDeleter theDigitStrDeleter(theDigitStr); /* prepare to delete soon */
	if (theDigitStr == NULL)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssClientBadRequest, sExpectedDigitFilenameErr);
	
	/* 用atoi，atol，strtod，strtol，strtoul都可以实现字符串类型转换，sprintf，或CString类的Format()函数可以将数值转换为字符串 */
	/* 定义函数 long int strtol(const char *nptr,char **endptr,int base); strtol()会将参数nptr字符串根据参数base来转换成长整型数。
	参数base范围从2至36，或0。参数base代表采用的进制方式，如 base值为10则采用10进制，若base值为16则采用16进制等。 */
	/* 此处将theDigitStr转换成十进制字符串形式 */
    UInt32 theTrackID = ::strtol(theDigitStr, NULL, 10);
    
//    QTRTPFile::ErrorCode qtfileErr = theFile->fFile.AddTrack(theTrackID, false); //test for 3gpp monotonic wall clocktime and sequence
	/* 加入指定trackID的track */
    QTRTPFile::ErrorCode qtfileErr = theFile->fFile.AddTrack(theTrackID, true);
    
    //if we get an error back, forward that error to the client
	//假如找不到指定的track,就向Client回送"Track不存在",或回送"不支持的媒体类型"的错误信息
    if (qtfileErr == QTRTPFile::errTrackIDNotFound)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssClientNotFound, sTrackDoesntExistErr);
    else if (qtfileErr != QTRTPFile::errNoError)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssUnsupportedMediaType, sBadQTFileErr);

    // Before setting up this track, check to see if there is an If-Modified-Since
    // date. If there is, and the content hasn't been modified, then just return a 304 Not Modified
	// 检查RTSP Request中是否有If-Modified-Since头,假如有,并与QTFile中记录的modified date相同,就向Client反馈"RTSP/1.0 304 Not Modified",并直接返回
    QTSS_TimeVal* theTime = NULL;
    (void) QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqIfModSinceDate, 0, (void**)&theTime, &theLen);
    if ((theLen == sizeof(QTSS_TimeVal)) && (*theTime > 0))
    {
        // There is an If-Modified-Since header. Check it vs. the content.
        if (*theTime == theFile->fFile.GetQTFile()->GetModDate())
        {
			/* set RTSP status code */
            theErr = QTSS_SetValue( inParamBlock->inRTSPRequest, qtssRTSPReqStatusCode, 0,
                                            &kNotModifiedStatus, sizeof(kNotModifiedStatus) );
            Assert(theErr == QTSS_NoErr);
            // Because we are using this call to generate a 304 Not Modified response, we do not need
            // to pass in a RTP Stream
            theErr = QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
            Assert(theErr == QTSS_NoErr);
            return QTSS_NoErr;
        }
    }

	//Create a new RTP stream 为指定的track,搭建一个RTP Stream
	/* 只有调用了QTSS_AddRTPStream函数的模块才能调用QTSS_Play函数, see DoPlay() 
	   QTSS_AddRTSPStream回调例程使一个模块可以向客户端发送RTP数据包，以响应RTSP请求。多次调用QTSS_AddRTSPStream函数
	   可以向会话中加入多个流。如果希望开始播放一个流，可以调用QTSS_Play函数 */
    QTSS_RTPStreamObject newStream = NULL;
    theErr = QTSS_AddRTPStream(inParamBlock->inClientSession, inParamBlock->inRTSPRequest, &newStream, 0);
    if (theErr != QTSS_NoErr)
        return theErr;

    //find the payload for this track ID (if applicable)
    StrPtrLen* thePayload = NULL;
    UInt32 thePayloadType = qtssUnknownPayloadType; //audio/video
    Float32 bufferDelay = (Float32) 3.0; // FIXME need a constant defined for 3.0 value. It is used multiple places

	/* 从SDP信息中获取RTPStream的个数,遍历这些RTPStream,从StreamInfo查找指定track id的RTPStream的Payload的名称,类型,缓冲延时 */
    for (UInt32 x = 0; x < theFile->fSDPSource.GetNumStreams(); x++)
    {
		/* see SourceInfo.h */
        SourceInfo::StreamInfo* theStreamInfo = theFile->fSDPSource.GetStreamInfo(x);
        if (theStreamInfo->fTrackID == theTrackID)
        {
            thePayload = &theStreamInfo->fPayloadName;
            thePayloadType = theStreamInfo->fPayloadType;
            bufferDelay = theStreamInfo->fBufferDelay;
            break;
        }   
    }
    
    // Set the payload type, payload name & timescale of this track设置该track的缓冲延时,名称,类型,时间刻度,trackID,NumQualityLevels

    SInt32 theTimescale = theFile->fFile.GetTrackTimeScale(theTrackID);//获取指定trackID的时间刻度
    static UInt32 sNumQualityLevels = 5;// Set the number of quality levels. Allow up to 6

    theErr = QTSS_SetValue(newStream, qtssRTPStrBufferDelayInSecs, 0, &bufferDelay, sizeof(bufferDelay));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadName, 0, thePayload->Ptr, thePayload->Len);
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadType, 0, &thePayloadType, sizeof(thePayloadType));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrTimescale, 0, &theTimescale, sizeof(theTimescale));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrTrackID, 0, &theTrackID, sizeof(theTrackID));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrNumQualityLevels, 0, &sNumQualityLevels, sizeof(sNumQualityLevels));
    Assert(theErr == QTSS_NoErr);
    
    // Get the SSRC of this track,假设总有SSRC(在成功加入时就有SSRC)
    UInt32* theTrackSSRC = NULL;
    UInt32 theTrackSSRCSize = 0;
    (void)QTSS_GetValuePtr(newStream, qtssRTPStrSSRC, 0, (void**)&theTrackSSRC, &theTrackSSRCSize);
    // The RTP stream should ALWAYS have an SSRC assuming QTSS_AddStream succeeded.
    Assert((theTrackSSRC != NULL) && (theTrackSSRCSize == sizeof(UInt32)));
    
    //give the file some info it needs.设置QTRTPFile中指定Track的SSRC和payload类型
    theFile->fFile.SetTrackSSRC(theTrackID, *theTrackSSRC);
    theFile->fFile.SetTrackCookies(theTrackID, newStream, thePayloadType);
    
	/* 获取qtssXRTPMetaInfoHeader指针,和支持的RTPMetaInfoFields数组复制响应头,设置该track的RTP Meta Info */
    StrPtrLen theHeader;
    theErr = QTSS_GetValuePtr(inParamBlock->inRTSPHeaders, qtssXRTPMetaInfoHeader, 0, (void**)&theHeader.Ptr, &theHeader.Len);
    if (theErr == QTSS_NoErr)
    {
        // If there is an x-RTP-Meta-Info header in the request, mirror that header in the
        // response. We will support any fields supported by the QTFileLib.
		/* increase a new field in Class RTPMetaInfoPacket and initialize */
        RTPMetaInfoPacket::FieldID* theFields = NEW RTPMetaInfoPacket::FieldID[RTPMetaInfoPacket::kNumFields];//6
        ::memcpy(theFields, QTRTPFile::GetSupportedRTPMetaInfoFields(), sizeof(RTPMetaInfoPacket::FieldID) * RTPMetaInfoPacket::kNumFields);

        // This function does the work of appending the response header based on the fields we support and the requested fields.
        // 
        theErr = QTSSModuleUtils::AppendRTPMetaInfoHeader(inParamBlock->inRTSPRequest, &theHeader, theFields);

        // This returns QTSS_NoErr only if there are some valid, useful fields
        Bool16 isVideo = false;
        if (thePayloadType == qtssVideoPayloadType)
            isVideo = true;
        if (theErr == QTSS_NoErr)
            theFile->fFile.SetTrackRTPMetaInfo(theTrackID, theFields, isVideo);
    }
    
    // Our array has now been updated to reflect the fields requested by the client, send the setup response.
	//Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
	//Cache-Control: must-revalidate\r\n
    (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssLastModifiedHeader,
                                theFile->fFile.GetQTFile()->GetModDateStr(), DateBuffer::kDateBufferLen);
    (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssCacheControlHeader,
                                kCacheControlHeader.Ptr, kCacheControlHeader.Len);
	/* 向客户端发送一个RTSP响应 */
    theErr = QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, newStream, 0);
    Assert(theErr == QTSS_NoErr);
    return QTSS_NoErr;
}


/* 首先获取FileSession中的播放次数并更新(加1),假如能使用共享缓存,且播放次数为1,就搭建公有缓存;假如能使用私有缓存,就分配私有缓存(两个连续的32K的私有缓存块) */
QTSS_Error SetupCacheBuffers(QTSS_StandardRTSP_Params* inParamBlock, FileSession** theFile)
{
    
	/* obtain play count and initialize */
    UInt32 playCount = 0;
    UInt32 theLen = sizeof(playCount);
	/* 获取播放次数 */
    QTSS_Error theErr = QTSS_GetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, (void*)&playCount, &theLen);
	/* handle error if necessary */
    if ( (theErr != QTSS_NoErr) || (theLen != sizeof(playCount)) )
    {
        playCount = 1;
		/* 设置播放次数 */
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, &playCount, sizeof(playCount));
        if (theErr != QTSS_NoErr)
            return QTSS_RequestFailed;
    }
    
	/* 假如能使用共享缓存,且播放次数为1 */
    if (sEnableSharedBuffers && playCount == 1) // increments num buffers after initialization so do only once per session
        /* 根据文件长度设置缓存块个数(最大8个),给媒体文件分配恰当大小的一个缓存块指针的数组结构,并将这些指针清零 */
		(*theFile)->fFile.AllocateSharedBuffers(sSharedBufferUnitKSize/*64*/, sSharedBufferInc/*8*/, sSharedBufferUnitSize/*1*/,sSharedBufferMaxUnits/*8*/);
    
	/* 假如能使用私有缓存,就分配私有缓存 */
    if (sEnablePrivateBuffers) // reinitializes buffers to current location so do every time 
		/* 不论文件比特率多大,为每个文件分配两个连续的32K的私有缓存块 */
        (*theFile)->fFile.AllocatePrivateBuffers(sSharedBufferUnitKSize/*64*/, sPrivateBufferUnitSize/*1*/, sPrivateBufferMaxUnits/*8*/);

	/* incerement by 1 and reset playcount attribute */
    playCount ++;
    theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, &playCount, sizeof(playCount));
    if (theErr != QTSS_NoErr)
        return QTSS_RequestFailed;  
        
    return theErr;

}

/* 创建两级公/私文件缓存,设置当前FileSession,RTPSession,RTSP Request的相关属性,启动播放媒体文件 */
QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParamBlock)
{
    QTRTPFile::ErrorCode qtFileErr = QTRTPFile::errNoError;

	/* 假如指定的RTSP Request中的Local file path的最末几个字符不是".sdp",就向Client发送"指定路径的文件没有SDP文件的"错误 */
	/* 注意这里的处理和DoSetup()/DoDescribe()是相同的,参照那里 */
    if (isSDP(inParamBlock))
    {
        StrPtrLen pathStr;
        (void)QTSS_LockObject(inParamBlock->inRTSPRequest);
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, (void**)&pathStr.Ptr, &pathStr.Len);
        QTSS_Error err = QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest, qtssClientNotFound, sNoSDPFileFoundErr, &pathStr);
        (void)QTSS_UnlockObject(inParamBlock->inRTSPRequest);
        return err;
    }

	/* 获取FileSession对象指针,假如出错,直接返回 */
    FileSession** theFile = NULL;
    UInt32 theLen = 0; /* used by QTSS_GetValuePtr in  many places below */
    QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
        return QTSS_RequestFailed;

	/**************** 创建两级公/私文件缓存 *****************************/
	/* set cache buffers, def see above  */
    theErr = SetupCacheBuffers(inParamBlock, theFile);  
    if (theErr != QTSS_NoErr)
        return theErr;
    /**************** 创建两级公/私文件缓存 *****************************/

    // Set the default quality before playing.遍历所有的RTPStream,设置每个track的quality level
    QTRTPFile::RTPTrackListEntry* thePacketTrack;
    for (UInt32 x = 0; x < (*theFile)->fSDPSource.GetNumStreams(); x++)
    {
         SourceInfo::StreamInfo* theStreamInfo = (*theFile)->fSDPSource.GetStreamInfo(x);
		 /* if find no track entry ? */
         if (!(*theFile)->fFile.FindTrackEntry(theStreamInfo->fTrackID,&thePacketTrack))
            break;
		 /* 注意set track quality level为传输所有的包 */
         (*theFile)->fFile.SetTrackQualityLevel(thePacketTrack, QTRTPFile::kAllPackets);
    }


	/*********** 计算FileSession的起始时间戳 ******************/
    // How much are we going to tell the client to back up?
    Float32 theBackupTime = 0;

	/* get packet range header field "x-Packet-Range: pn=4551-4689;url=trackID3" */
    char* thePacketRangeHeader = NULL;
    theErr = QTSS_GetValuePtr(inParamBlock->inRTSPHeaders, qtssXPacketRangeHeader, 0, (void**)&thePacketRangeHeader, &theLen);
    if (theErr == QTSS_NoErr)
    {
        StrPtrLen theRangeHdrPtr(thePacketRangeHeader, theLen);
        StringParser theRangeParser(&theRangeHdrPtr);
        
		/* 获得起始包号 */
        theRangeParser.ConsumeUntil(NULL, StringParser::sDigitMask);
        UInt64 theStartPN = theRangeParser.ConsumeInteger();
        
		/* 获得终止包号 */
        theRangeParser.ConsumeUntil(NULL, StringParser::sDigitMask);
        (*theFile)->fStopPN = theRangeParser.ConsumeInteger();

		/* 获得track id */
        theRangeParser.ConsumeUntil(NULL, StringParser::sDigitMask);
        (*theFile)->fStopTrackID = theRangeParser.ConsumeInteger();

		/* acquire start packet number from the specific track id and get seek time */
        qtFileErr = (*theFile)->fFile.SeekToPacketNumber((*theFile)->fStopTrackID, theStartPN);
        (*theFile)->fStartTime = (*theFile)->fFile.GetRequestedSeekTime();
    }
    else
    {
        Float64* theStartTimeP = NULL;
        Float64 currentTime = 0;
		/* 获得RTSP request请求的起始时间 */
        theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqStartTime, 0, (void**)&theStartTimeP, &theLen);
		/* 假如没有起始时间,就用准备发送的上一个包的时间 */
        if ((theErr != QTSS_NoErr) || (theLen != sizeof(Float64)))
        {   // No start time so just start at the last packet ready to send
            // This packet could be somewhere out in the middle of the file.
             currentTime =  (*theFile)->fFile.GetFirstPacketTransmitTime(); 
             theStartTimeP = &currentTime;  
             (*theFile)->fStartTime = currentTime;
        }    

          
		/* 获取RTSP request中报头"the x-Prebuffer: 20 "来得到最大倒退时间 */
        Float32* theMaxBackupTime = NULL;
        theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqPrebufferMaxTime, 0, (void**)&theMaxBackupTime, &theLen);
        Assert(theMaxBackupTime != NULL);
    
        if (*theMaxBackupTime == -1)
        {
            //
            // If this is an old client (doesn't send the x-prebuffer header) or an mp4 client, 
            // - don't back up to a key frame, and do not adjust the buffer time
            qtFileErr = (*theFile)->fFile.Seek(*theStartTimeP, 0);
            (*theFile)->fStartTime = *theStartTimeP;
           
            //
            // burst out -transmit time packets
			/* permit no negative transmit time */
            (*theFile)->fAllowNegativeTTs = false;
        }
        else
        {
            qtFileErr = (*theFile)->fFile.Seek(*theStartTimeP, *theMaxBackupTime);
            Float64 theFirstPacketTransmitTime = (*theFile)->fFile.GetFirstPacketTransmitTime();
            theBackupTime = (Float32) ( *theStartTimeP - theFirstPacketTransmitTime);
            
            //
            // For oddly authored movies, there are situations in which the packet
            // transmit time can be before the sample time. In that case, the backup
            // time may exceed the max backup time. In that case, just make the backup
            // time the max backup time.
            if (theBackupTime > *theMaxBackupTime)
                theBackupTime = *theMaxBackupTime;
            //
            // If client specifies that it can do extra buffering (new client), use the first
            // packet transmit time as the start time for this play burst. We don't need to
            // burst any packets because the client can do the extra buffering
			Bool16* overBufferEnabledPtr = NULL;
			theLen = 0;
			theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, qtssCliSesOverBufferEnabled, 0, (void**)&overBufferEnabledPtr, &theLen);	
			if ((theErr == QTSS_NoErr) && (theLen == sizeof(Bool16)) && *overBufferEnabledPtr)
				(*theFile)->fStartTime = *theStartTimeP;
			else
                (*theFile)->fStartTime = *theStartTimeP - theBackupTime;            

            /* permit negative transmit time */
            (*theFile)->fAllowNegativeTTs = true;
        }
    }
    
	/* 假如查询时间出错 */
    if (qtFileErr == QTRTPFile::errCallAgain)
    {
        //
        // If we are doing RTP-Meta-Info stuff, we might be asked to get called again here.
        // This is simply because seeking might be a long operation and we don't want to
        // monopolize(垄断) the CPU, but there is no other reason to wait, so just set a timeout of 0
        theErr = QTSS_SetIdleTimer(1);
        Assert(theErr == QTSS_NoErr);
        return theErr;
    }
    else if (qtFileErr != QTRTPFile::errNoError)
        return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                    qtssClientBadRequest, sSeekToNonexistentTimeErr);
                                                        
    //make sure to clear the next packet the server would have sent!清空已发送出的数据包
    (*theFile)->fPacketStruct.packetData = NULL;
	(**theFile).fPaused = false;/* 当前的FileSession状态是PAUSE吗?不是,是PLAY */
	if ((**theFile).fLastPauseTime > 0)//记录上次中断时的时间戳
		(**theFile).fTotalPauseTime += OS::Milliseconds() - (**theFile).fLastPauseTime;/* 更新总中断时间(ms) */

    // Set the movie duration,size parameters,bitsPerSecond
	
    Float64 movieDuration = (*theFile)->fFile.GetMovieDuration();/* 设置媒体文件持续时间 */
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieDurationInSecs, 0, &movieDuration, sizeof(movieDuration));
    UInt64 movieSize = (*theFile)->fFile.GetAddedTracksRTPBytes();/* 设置媒体文件大小 */
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieSizeInBytes, 0, &movieSize, sizeof(movieSize));
    UInt32 bitsPerSecond =  (*theFile)->fFile.GetBytesPerSecond() * 8; /* 设置媒体文件的平均比特率(bits/sec)*/
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieAverageBitRate, 0, &bitsPerSecond, sizeof(bitsPerSecond));

    // For the purposes of the speed header, check to make sure all tracks are over a reliable transport
	/* note: UDP is unreliable, but Reliable UDP and TCP reliable */
    Bool16 allTracksReliable = true;
    
    // Set the timestamp & sequence number parameters for each track.
	/* 在调用QTSS_Play函数之前,模块应该为每个RTP流设置下面这些QTSS_RTPStreamObject对象的属性:
	qtssRTPStrFirstSeqNumber,qtssRTPStrFirstTimestamp,qtssRTPStrTimescale */
    QTSS_RTPStreamObject* theRef = NULL;
    for (   UInt32 theStreamIndex = 0;
            QTSS_GetValuePtr(inParamBlock->inClientSession, qtssCliSesStreamObjects, theStreamIndex, (void**)&theRef, &theLen) == QTSS_NoErr;
            theStreamIndex++)
    {
        UInt32* theTrackID = NULL;
        theErr = QTSS_GetValuePtr(*theRef, qtssRTPStrTrackID, 0, (void**)&theTrackID, &theLen);//得到RTPStream的track ID
        Assert(theErr == QTSS_NoErr);
        Assert(theTrackID != NULL);
        Assert(theLen == sizeof(UInt32));
        
        UInt16 theSeqNum = 0;//得到指定track ID的RTPStream的当前时间戳
        UInt32 theTimestamp = (*theFile)->fFile.GetSeekTimestamp(*theTrackID); // this is the base timestamp need to add in paused time. 
		
        Assert(theRef != NULL);

		/* get RTPStream timescale to calculate pause timestamp and reset the timestamp */
        UInt32* theTimescale = NULL;/* 得到该RTPStream的timescale */
        QTSS_GetValuePtr(*theRef, qtssRTPStrTimescale, 0,  (void**)&theTimescale, &theLen);
        if (theLen != 0) // adjust the timestamps to reflect paused time else leave it alone we can't calculate the timestamp without a timescale.
        {
			//计算中断时间戳
            UInt32 pauseTimeStamp = CalculatePauseTimeStamp( *theTimescale,  (*theFile)->fTotalPauseTime, (UInt32) theTimestamp);
            if (pauseTimeStamp != theTimestamp) //更新当前时间戳
                  theTimestamp = pauseTimeStamp;
       }

       
		/* 设置该RTP stream的第一个SeqNum和第一个时间戳 */
	    theSeqNum = (*theFile)->fFile.GetNextTrackSequenceNumber(*theTrackID);       
        theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstSeqNumber, 0, &theSeqNum, sizeof(theSeqNum));
        Assert(theErr == QTSS_NoErr);
        theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstTimestamp, 0, &theTimestamp, sizeof(theTimestamp));
        Assert(theErr == QTSS_NoErr);

		/* 通过获取该RTP stream的传输类型来设置allTracksReliable */
        if (allTracksReliable)
        {
			/* enum QTSS_RTPTransportType def see QTSS.h */
            QTSS_RTPTransportType theTransportType = qtssRTPTransportTypeUDP;
            theLen = sizeof(theTransportType);
			/* obtain RTP transport type */
            theErr = QTSS_GetValue(*theRef, qtssRTPStrTransportType, 0, &theTransportType, &theLen);
            Assert(theErr == QTSS_NoErr);
            
			/* UDP transport is not relialbe */
            if (theTransportType == qtssRTPTransportTypeUDP)
                allTracksReliable = false;
        }
    }
    
    //Tell the QTRTPFile whether repeat packets are wanted based on the transport type
    // we don't care if it doesn't set (i.e. this is a meta info session)//决定是否丢掉重复包?TCP和RUDP需要丢掉重复包,UDP不用
     (void)  (*theFile)->fFile.SetDropRepeatPackets(allTracksReliable);// if all tracks are reliable then drop repeat packets.

    //Tell the server to start playing this movie. We do want it to send RTCP SRs, but we DON'T want it to write the RTP header
    /* QTSS_Play回调例程开始播放与指定客户会话相关联的流。只有调用了QTSS_AddRTPStream函数的模块才能调用QTSS_Play函数
	   在调用QTSS_Play函数之后，模块的RTP Send Packets角色就会被调用。将inPlayFlags参数设置为qtssPlaySendRTCP常数会
	   使服务器在播放的过程中自动产生RTCP发送方报告。否则，模块会负责产生具体描述播放特征的发送方报告。*/
    theErr = QTSS_Play(inParamBlock->inClientSession, inParamBlock->inRTSPRequest, qtssPlayFlagsSendRTCP);
    if (theErr != QTSS_NoErr)//点播电影并发送SR包
        return theErr;

	// Set fAdjustedPlayTime
    SInt64* thePlayTime = 0;
	/* 获取媒体文件被PLAY时的播放时间戳(ms),参见RTPSession::Play()中的fPlayTime */
    theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, qtssCliSesPlayTimeInMsec, 0, (void**)&thePlayTime, &theLen);
    Assert(theErr == QTSS_NoErr);
    Assert(thePlayTime != NULL);
    Assert(theLen == sizeof(SInt64));
	/* set fAdjustedPlayTime(ms) */
	/* fAdjustedPlayTime就是client点播媒体文件时的时间戳和RTSP Range头"Range: npt=0.00000-70.00000\r\n"中开始播放的时间戳的间隔! */
    (*theFile)->fAdjustedPlayTime = *thePlayTime - ((SInt64)((*theFile)->fStartTime * 1000));
    
	/*************** 设置theSpeed,添加RTSP头"Speed: 2.0\r\n" ************************/
    // This module supports the Speed header if the client wants the stream faster than normal.
    Float32 theSpeed = 1;
    theLen = sizeof(theSpeed);
	/* 获取Client的RTSP Request中头"Speed: 2.0"的值,支持快进/快倒 */
    theErr = QTSS_GetValue(inParamBlock->inRTSPRequest, qtssRTSPReqSpeed, 0, &theSpeed, &theLen);
    Assert(theErr != QTSS_BadArgument);
    Assert(theErr != QTSS_NotEnoughSpace);
    
	/* 调整速度值,对UDP传输方式,不支持快进快退 */
    if (theErr == QTSS_NoErr)
    {
        if (theSpeed > sMaxAllowedSpeed)
            theSpeed = sMaxAllowedSpeed;
        if ((theSpeed <= 0) || (!allTracksReliable))
            theSpeed = 1;
    }
    
	/* 设置FileSession中的Speed值，假如Speed值不是1,将"Speed: 2.0\r\n"放入RTSP响应中 */
    (*theFile)->fSpeed = theSpeed;
    if (theSpeed != 1)
    {
        // If our speed is not 1, append the RTSP speed header in the response
        char speedBuf[32];
        qtss_sprintf(speedBuf, "%10.5f", theSpeed);
        StrPtrLen speedBufPtr(speedBuf);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssSpeedHeader,speedBufPtr.Ptr, speedBufPtr.Len);
    }
    
    /********** 添加RTSP头"x-Prebuffer: time=0.2\r\n" *****************/
    // Append x-Prebuffer header "x-Prebuffer: time=0.2" if provided & nonzero prebuffer needed
    if (theBackupTime > 0)
    {
        char prebufferBuf[32];
        qtss_sprintf(prebufferBuf, "time=%.5f", theBackupTime);
        StrPtrLen backupTimePtr(prebufferBuf);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssXPreBufferHeader,
                                    backupTimePtr.Ptr, backupTimePtr.Len);
    
    }
   
	/********** 添加RTSP头"Range: npt=0.00000-70.00000\r\n" *****************/
	 // Record the requested stop time, if there is one 记录RTSP Request的请求停止时间
    (*theFile)->fStopTime = -1; /* note: this initialize value in  FileSession(), see above */
    theLen = sizeof((*theFile)->fStopTime);
    theErr = QTSS_GetValue(inParamBlock->inRTSPRequest, qtssRTSPReqStopTime, 0, &(*theFile)->fStopTime, &theLen);

    // add the range header "Range: npt=0.00000-70.00000\r\n".
    {
        char rangeHeader[64];
		/* 假如没有媒体文件终止时间,就用持续时间代替 */
        if (-1 == (*theFile)->fStopTime)
           (*theFile)->fStopTime = (*theFile)->fFile.GetMovieDuration();
           
        qtss_snprintf(rangeHeader,sizeof(rangeHeader) -1, "npt=%.5f-%.5f", (*theFile)->fStartTime, (*theFile)->fStopTime);
        rangeHeader[sizeof(rangeHeader) -1] = 0;
        
        StrPtrLen rangeHeaderPtr(rangeHeader);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssRangeHeader,rangeHeaderPtr.Ptr, rangeHeaderPtr.Len);
    
    }
	/* 在设置了各种属性后,最终我们送出RTSP Reponse against Client request */
    (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, qtssPlayRespWriteTrackInfo);

    return QTSS_NoErr;
}

 
/* the true function to send packets, note theLastPacketTrack,theTransmitTime  */
/* two struct: QTSS_PacketStruct, QTSS_RTPSendPackets_Params  */
QTSS_Error SendPackets(QTSS_RTPSendPackets_Params* inParams)
{
	/* set quality check interval in ms */
    static const UInt32 kQualityCheckIntervalInMsec = 250;  //流质量检查间隔是250毫秒

    FileSession** theFile = NULL;
    UInt32 theLen = 0;
	/* 获取FileSession属性,它在哪里设置的? */
	/* (void**)&theFile should be (void**)theFile? */
    QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
    Assert(theErr == QTSS_NoErr);
    Assert(theLen == sizeof(FileSession*));

	/* QTSS_WriteFlags, see QTSS.h */
	/* set write status code,决定是否可以发送RTP包? */
    bool isBeginningOfWriteBurst = true;

	/* RTPStream def, void * pointer in essence */
    QTSS_Object theStream = NULL;/* RTPStream对象 */

	/* GetLastPacketTrack() refer to QTRTPFile.h */
	/* very important ! 得到上一个包所在的track */
    QTRTPFile::RTPTrackListEntry* theLastPacketTrack = (*theFile)->fFile.GetLastPacketTrack();

    /* 开始反复循环send Packet */
    while (true)
    {   
		
        /* when we find that the buffer to save packet date is empty */
        if ((*theFile)->fPacketStruct.packetData == NULL)
        {
			/* refer to QTRTPFile::GetNextPacket() */
			/* theTransmitTime is very important, used by much places below ! */
			/* theTransmitTime是得到下一个packet所花的时间(相对时间) */
            Float64 theTransmitTime = (*theFile)->fFile.GetNextPacket((char**)&(*theFile)->fPacketStruct.packetData, &(*theFile)->fNextPacketLen);
            //取下个包出错, if errors occure in use of QTRTPFile
			if ( QTRTPFile::errNoError != (*theFile)->fFile.Error() )
            {   //设定出错原因，然后断掉连接，并返回
                QTSS_CliSesTeardownReason reason = qtssCliSesTearDownUnsupportedMedia;//断掉不支持的媒体
                (void) QTSS_SetValue(inParams->inClientSession, qtssCliTeardownReason, 0, &reason, sizeof(reason));
				/* tear down */
                (void)QTSS_Teardown(inParams->inClientSession);
                return QTSS_RequestFailed;
            }
			/* 得到上一个包所在的track,若为空就中断循环 */
            theLastPacketTrack = (*theFile)->fFile.GetLastPacketTrack();
			if (theLastPacketTrack == NULL)
				break;

			/*local var theStream see above def, in QTRTPFile::RTPTrackListEntry.Cookie1*/
			/* 取得上一个包的track所在的RTPStream */
            theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
			Assert(theStream != NULL);
			if (theStream == NULL)
				return 0;


            // Check to see if we should stop playing now
            /* 假如已到停止时间,关掉媒体文件,更新终止时间,返回 */
            if (((*theFile)->fStopTime != -1) && (theTransmitTime > (*theFile)->fStopTime))
            {
                // We should indeed stop playing
                (void)QTSS_Pause(inParams->inClientSession);
				/* adjust parameters, explaination see QTSS.h */
                inParams->outNextPacketTime = qtssDontCallSendPacketsAgain;

				/* 当前FileSession状态是PAUSE */
                (**theFile).fPaused = true;
				/* 当前PAUSE时间戳,参见DoPlay() */
                (**theFile).fLastPauseTime = OS::Milliseconds();
  
                return QTSS_NoErr;
            }

			
			/* 假如已到上个包的track设定终止的包号,关掉媒体文件,更新终止时间,返回,注意针对"x-Packet-Range: pn=4551-4689;url=trackID3" */
            if (((*theFile)->fStopTrackID != 0) && ((*theFile)->fStopTrackID == theLastPacketTrack->TrackID) && (theLastPacketTrack->HTCB->fCurrentPacketNumber > (*theFile)->fStopPN))
            {
                // We should indeed stop playing
                (void)QTSS_Pause(inParams->inClientSession);
				/* adjust parameters */
                inParams->outNextPacketTime = qtssDontCallSendPacketsAgain;
                (**theFile).fPaused = true;/* 当前FileSession状态是PAUSE */
                (**theFile).fLastPauseTime = OS::Milliseconds();/* 当前PAUSE时间戳,参见DoPlay() */

                return QTSS_NoErr;
            }
            
            // Find out what our play speed is. Send packets out at the specified rate,
            // and do so by altering the transmit time of the packet based on the Speed rate.
			/* Next Packet到来的时间和File Session开始的时间的差 */
            Float64 theOffsetFromStartTime = theTransmitTime - (*theFile)->fStartTime;
			/* re-adjust the transmit time of packets */
			/* 根据File session的播放速度快慢灵活调整Next Packet到来的时间 */
            theTransmitTime = (*theFile)->fStartTime + (theOffsetFromStartTime / (*theFile)->fSpeed);
            
            // correct for first packet xmit times that are < 0
			/* 在File会话不允许负值但又计算得出负的到来时间时，及时更正Next Packet到来的时间 */
            if (( theTransmitTime < 0.0 ) && ( !(*theFile)->fAllowNegativeTTs ))
                theTransmitTime = 0.0;
            
			/* update the info in FileSession.QTSS_PacketStruct, see QTSS.h */
			/********************* ???????????????? ****************************/
			/* NOTE: theTransmitTime is second, not DSS system time(ms), but packetTransmitTime and fAdjustedPlayTime use ms */
            (*theFile)->fPacketStruct.packetTransmitTime = (*theFile)->fAdjustedPlayTime + ((SInt64)(theTransmitTime * 1000));

        }
        
        //We are done playing all streams!
		//我们已经播放完了所有的流,返回!
        if ((*theFile)->fPacketStruct.packetData == NULL)
        {
            //TODO not quite good to the last drop -- we -really- should guarantee this, also reflector
            // a write of 0 len to QTSS_Write will flush any buffered data if we're sending over tcp
            //(void)QTSS_Write((QTSS_Object)(*theFile)->fFile.GetLastPacketTrack()->Cookie1, NULL, 0, NULL, qtssWriteFlagsIsRTP);
            inParams->outNextPacketTime = qtssDontCallSendPacketsAgain;
            return QTSS_NoErr;
        }
        
        //we have a packet that needs to be sent now
		/* now (*theFile)->fPacketStruct.packetData != NULL */
		//现在已有一个RTP包要发送出去
		/* 这可能有两种情况：一种是上次的packet阻塞未完全送出，这次重传；另一种就是传下一个包 */
        Assert(theLastPacketTrack != NULL);

        //If the stream is video, we need to make sure that QTRTPFile knows what quality level we're at
        /* if able to thin and exceeds quality check time, set the theLastPacketTrack quality level */
		/* 假如是视频流,能打薄,且已到Quality check时间,要明确当前的quality level */
        if ( (!sDisableThinning) && (inParams->inCurrentTime > ((*theFile)->fLastQualityCheck + kQualityCheckIntervalInMsec) ) )
        {
			/* see QTRTPFile.RTPTrackListEntry.Cookie2 */
			/* 检查上一个track流是audio还是video? */
            QTSS_RTPPayloadType thePayloadType = (QTSS_RTPPayloadType)theLastPacketTrack->Cookie2;
			/* 假如是视频流,获得它的quality level,设置为该FileSession的quality level */
            if (thePayloadType == qtssVideoPayloadType)
            {
				/* 更新上次LastQualityCheck时间 */
                (*theFile)->fLastQualityCheck = inParams->inCurrentTime;
                /* 获取上个track所在的RTPStream */
				theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
				Assert(theStream != NULL);
				if (theStream == NULL)
					return 0;


                // Get the current quality level in the stream, and this stream's TrackID.
                UInt32* theQualityLevel = 0;
				/* 获得上个track流的Quality level,它的设置参见DoPlay() */
                theErr = QTSS_GetValuePtr(theStream, qtssRTPStrQualityLevel, 0, (void**)&theQualityLevel, &theLen);
                Assert(theErr == QTSS_NoErr);
                Assert(theQualityLevel != NULL);
                Assert(theLen == sizeof(UInt32));
        
				/* see QTRTPFile::SetTrackQualityLevel() */
				/* use *theQualityLevel obtained above to set current track quality level */
                (*theFile)->fFile.SetTrackQualityLevel(theLastPacketTrack, *theQualityLevel);
            }
        }

        // 做发送RTP包前的准备!
		/* QTSS_WriteFlags,qtssWriteFlagsWriteBurstBegin,qtssWriteFlagsIsRTP see QTSS.h */
		/* 设置写标志(是大量发送RTP包) */
        QTSS_WriteFlags theFlags = qtssWriteFlagsIsRTP;
		/* write status code, see above*/
        if (isBeginningOfWriteBurst)
            theFlags |= qtssWriteFlagsWriteBurstBegin; /* means now begin to write */

		/* 得到上一个RTP包所在的RTPStream对象,假如为空,直接返回 */
        theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
		Assert(theStream != NULL);
		if (theStream == NULL)
			return 0;

        //adjust the timestamp so it reflects paused time.
		/* see FileSession.QTSS_PacketStruct.packatData */
		/* these quantities will use in block-dealing, see below */
		/* 取得当前RTP包的数据 */
        void* packetDataPtr =  (*theFile)->fPacketStruct.packetData;
		/* get timestamp field from given packet, GetPacketTimeStamp see above  */
        UInt32 currentTimeStamp = GetPacketTimeStamp(packetDataPtr);
		/* obtain the absolute time to pause,SetPausetimeTimeStamp see above */ 
		/* use *theFile to obtain fTotalPauseTime, theStream to obtain timescale */
		/* in general, pauseTimeStamp locate behind currentTimeStamp */
        UInt32 pauseTimeStamp = SetPausetimeTimeStamp(*theFile, theStream, currentTimeStamp); 
		/* get the sequence number from the given packet in RTP stream */
  		UInt16 curSeqNum = GetPacketSequenceNumber(theStream);
		//用当前包的seqnum显式设置给定上一个要发送LastPacketSeqNumAttr 单值属性的属性值
        (void) QTSS_SetValue(theStream, sRTPStreamLastPacketSeqNumAttrID, 0, &curSeqNum, sizeof(curSeqNum));

		// 发送一个RTP包!
		/* write rtp packet from buffer &(*theFile)->fPacketStruct into given stream theStream with specified length bytes NULL */
		// QTSS_Write回调例程将一个缓冲区中的数据写入到一个RTPStream流中,标志是qtssWriteFlagsIsRTP!qtssWriteFlagsWriteBurstBegin。
		theErr = QTSS_Write(theStream, &(*theFile)->fPacketStruct, (*theFile)->fNextPacketLen, NULL, theFlags);
        
		/* change write status code right now after a writing into RTP stream */
        isBeginningOfWriteBurst = false;
        
		/* handle the probable results of QTSS_Write */
		/* deal with blocking:假如该RTP包阻塞,则设置好QTSS_PacketStruct,特别是下次发送该RTP包需要等待的时间 */
        if ( theErr == QTSS_WouldBlock )
        { 

			/* the two local vars see above def, here means fTotalPauseTime>0 */
			/* 假如当前时间戳不是中断时间戳,就重设为该RTP包的时间戳 */
            if (currentTimeStamp != pauseTimeStamp) // reset the packet time stamp so we adjust it again when we really do send it
               SetPacketTimeStamp(currentTimeStamp, packetDataPtr);
           
            //
            // In the case of a QTSS_WouldBlock error, the packetTransmitTime field of the packet struct will be set to
            // the time to wakeup, or -1 if not known.
            // If the time to wakeup is not given by the server, just give a fixed guess interval
			/* 假如suggestedWakeupTime没有指定,就将outNextPacketTime设为一个流控探测区间 */
            if ((*theFile)->fPacketStruct.suggestedWakeupTime == -1)
				/* after default 10 ms continue to send packet */
                inParams->outNextPacketTime = sFlowControlProbeInterval;    //10ms for buffering, try me again in # MSec
            else  /* if suggestedWakeupTime has already be set */
            {   
				/* make sure of suggestedWakeupTime locates behind current time */
                Assert((*theFile)->fPacketStruct.suggestedWakeupTime > inParams->inCurrentTime);
				/* calculate the relative interval to wakeup,计算下一包发送的等待时间 */
                inParams->outNextPacketTime = (*theFile)->fPacketStruct.suggestedWakeupTime - inParams->inCurrentTime;
            }
            
            //qtss_printf("Call again: %qd\n", inParams->outNextPacketTime);
                
            return QTSS_NoErr;
        }
        else /* if no blocking, this easy! */
        {
          //用送出的RTP包的seqnum显式设置给定上一个送出的LastSentPacketSeqNumAttr 单值属性的属性值
          (void) QTSS_SetValue(theStream, sRTPStreamLastSentPacketSeqNumAttrID, 0, &curSeqNum, sizeof(curSeqNum));
		  /* reset the buffer to save packet data and prepare to send packet next time */
          (*theFile)->fPacketStruct.packetData = NULL;
        }
    }
    
    return QTSS_NoErr;
}

/* 获取FileSession实例对象,从中获取skip过的样本数,并设置入参中的RTPSession中的跳包数属性qtssCliSesFramesSkipped,最后删除FileSession实例对象 */
QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams)
{
	//acquire file session attributes
    FileSession** theFile = NULL;
    UInt32 theLen = 0;
    QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)) || (theFile == NULL))
        return QTSS_RequestFailed;

    //
    // Tell the ClientSession how many samples we skipped because of stream thinning
	/* 设置入参中的RTPSession中的跳包数属性qtssCliSesFramesSkipped */
    UInt32 theNumSkippedSamples = (*theFile)->fFile.GetNumSkippedSamples();
    (void)QTSS_SetValue(inParams->inClientSession, qtssCliSesFramesSkipped, 0, &theNumSkippedSamples, sizeof(theNumSkippedSamples));
    
	/* 删除FileSession对象实例 */
    DeleteFileSession(*theFile);
    return QTSS_NoErr;
}

void    DeleteFileSession(FileSession* inFileSession)
{   
    delete inFileSession;
}
