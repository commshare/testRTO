
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
        Float32             fSpeed; /* in essence,note the speed is controlled by client !default 1,max 4,speed related to the normal play speed of media file,���RTSPͷ"Speed: 2.0\r\n" */
        Float64             fStartTime;/* the time of media file begin to play (s) */
        Float64             fStopTime; /* time to stop(s),���RTSPͷ"Range: npt=0.00000-70.00000\r\n" */
		SInt64              fAdjustedPlayTime;/* adjust play time(ms), a relative value,see DoPlay()/SendPackets(),��client�㲥ý���ļ�ʱ��ʱ�����RTSP Rangeͷ"Range: npt=0.00000-70.00000\r\n"�п�ʼ���ŵ�ʱ����ļ��! */
        
        UInt32              fStopTrackID;/* track ID of stopped track,���"x-Packet-Range: pn=4551-4689;url=trackID3"  */
        UInt64              fStopPN; /* packet number of stopped packet, used in QTRTPFile::RTPTrackListEntry::HTCB->fCurrentPacketNumber in QTSSFileModule::SendPackets()  */

        UInt32              fLastRTPTime;
        UInt64              fLastPauseTime; /* �ϴ�PAUSEʱ��ʱ���(��λ��ms,�μ�DoPlay()) */
        SInt64              fTotalPauseTime;/* �ۻ����ж�ʱ�� (��λ��ms,�μ�DoPlay()) */
        Bool16              fPaused; /*��ǰfile session��״̬��PAUSE��? */
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
static  StrPtrLen sEmailHeader;/* ��������BuildPrefBasedHeaders()���� */
static  StrPtrLen sURLHeader;/* ��������BuildPrefBasedHeaders()���� */
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

static QTSS_AttributeID sFileSessionAttr                = qtssIllegalAttrID; /* used frequently here,������������������? */
/* file session */
static QTSS_AttributeID sFileSessionPlayCountAttrID     = qtssIllegalAttrID;/* ���Ŵ��� */
static QTSS_AttributeID sFileSessionBufferDelayAttrID   = qtssIllegalAttrID; /* ������ʱ */
/* RTP Stream send packets */
static QTSS_AttributeID sRTPStreamLastSentPacketSeqNumAttrID   = qtssIllegalAttrID;/* �ϴγɹ����ͳ���RTP�������к�,used by QTSSFileModule::SendPackets() */
static QTSS_AttributeID sRTPStreamLastPacketSeqNumAttrID   = qtssIllegalAttrID;/*�ϴ�Ҫ���͵�RTP�������к�,used by QTSSFileModule::SendPackets() */

// OTHER DATA

/* cf: kQualityCheckIntervalInMsec = 250 in QTSSFileModule::SendPackets() */
static UInt32				sFlowControlProbeInterval	= 10;/* used by block in QTSSFileModule::SendPackets()  */
static UInt32				sDefaultFlowControlProbeInterval= 10;
static Float32              sMaxAllowedSpeed            = 4;/* see API Doc and StreamingServer.xml */
static Float32              sDefaultMaxAllowedSpeed     = 4;

// File Caching Prefs �ļ�����Ԥ��ֵ
/* see QTSS API Doc for details */
static Bool16               sEnableSharedBuffers    = false;//��ʹ�ù�������?
static Bool16               sEnablePrivateBuffers   = false;//��ʹ��˽�л�����?

/* �����浥λ */
static UInt32               sSharedBufferUnitKSize  = 0;
static UInt32               sSharedBufferInc        = 0;
static UInt32               sSharedBufferUnitSize   = 0;
static UInt32               sSharedBufferMaxUnits   = 0;

/* ˽�л��浥λ */
static UInt32               sPrivateBufferUnitKSize = 0;
static UInt32               sPrivateBufferUnitSize  = 0;
static UInt32               sPrivateBufferMaxUnits  = 0;

static Float32              sAddClientBufferDelaySecs = 0;/* used in DoDescribe() */

static Bool16               sRecordMovieFileSDP = false;/* whether record movie file? */
static Bool16               sEnableMovieFileSDP = false;/* if represent separately in a individual file or in a built-in atom ? */

static Bool16               sPlayerCompatibility = true;/* ���ݲ�������? used in DoDescribe() */
static UInt32               sAdjustMediaBandwidthPercent = 50;/* ����ý�����ٷֱ� used in DoDescribe() */

// Server preference we respect
static Bool16               sDisableThinning       = false;/* �ܴ�? may thinning */

static const StrPtrLen              kCacheControlHeader("must-revalidate");
static const QTSS_RTSPStatusCode    kNotModifiedStatus  = qtssRedirectNotModified;



  
// FUNCTIONS
/* ����ע�������⼸������: ProcessRTSPRequest, DoDescribe,DoSetup,DoPlay,SendPackets */

/* ע���������ԭ������ */
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

/* ��μ���pauseTimeStamp? */
/* �����RTPStream�ĵ�ǰ�ж�ʱ���(��λ��s), used by QTSSFileModule::DoPlay()/SendPackets() */
inline UInt32 CalculatePauseTimeStamp(UInt32 timescale, SInt64 totalPauseTime, UInt32 currentTimeStamp)
{
	/* �����RTPStream����Pause time(��λ��s),�Ǹ�ʱ��� */
    SInt64 pauseTime = (SInt64) ( (Float64) timescale * ( ( (Float64) totalPauseTime) / 1000.0)); 
	/* �����RTPStream�ĵ�ǰ�ж�ʱ���(��λ��s) */
    UInt32 pauseTimeStamp = (UInt32) (pauseTime + currentTimeStamp);

    return pauseTimeStamp;
}

/* ����CalculatePauseTimeStamp()/ SetPacketTimeStamp() */
/* used by QTSSFileModule::SendPackets()  */
/* ��ȡ�ļ��Ự�е�fTotalPauseTime,��Ϊ��,ֱ�ӷ��ص�ǰʱ���;����Ϊ��,�����PauseTimeStamp,����Ϊ��ǰ����ʱ���,�ٷ���PauseTimeStamp */
UInt32 SetPausetimeTimeStamp(FileSession *fileSessionPtr, QTSS_Object theRTPStream, UInt32 currentTimeStamp)
{ 
	/* obvious from above CalculatePauseTimeStamp() */
    if (fileSessionPtr->fTotalPauseTime == 0)
        return currentTimeStamp;

    UInt32 timeScale = 0;
    UInt32 theLen = sizeof(timeScale);
	/* obtain trp stream timescale */
	/* ������ֵ������һ����������,��ָ��������ֵ������һ�������ⲿ�ṩ�Ļ������� */
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
/* ��ָ������Ϣ�ȴ��iovec������ʽ,��д��SDP�ļ���,������д�����ݵĳ���.ע��*ioVectorIndex�Զ���1 */
UInt32 WriteSDPHeader(FILE* sdpFile, iovec *theSDPVec, SInt16 *ioVectorIndex, StrPtrLen *sdpHeader)
{

    Assert (ioVectorIndex != NULL);/* д�ڵڼ��� */
    Assert (theSDPVec != NULL);/* д��λ�ô������ݽṹ */
    Assert (sdpHeader != NULL);/* Ҫд��ͷ��Ϣ */
	/* Ҫд�����������ű���С��sdp����(22) */
    Assert (*ioVectorIndex < sNumSDPVectors); // if adding an sdp param you need to increase sNumSDPVectors
    
	/* ��������ֵ���Լ�һ */
    SInt16 theIndex = *ioVectorIndex;
    *ioVectorIndex += 1;/* note index starts from 0 */

	/* iovec see .\APIStubLib\OSHeaders.h */
	/* д��ָ�������źͳ��ȵ�sdp�е�ָ��λ�õĽṹ�� */
    theSDPVec[theIndex].iov_base =  sdpHeader->Ptr;
    theSDPVec[theIndex].iov_len = sdpHeader->Len;
    
	/* ��SDP�ļ���д��ָ�������ŵ�sdp��info */
    if (sdpFile !=NULL)
		/* �μ�̷��ǿC p.339 */
        ::fwrite(theSDPVec[theIndex].iov_base,theSDPVec[theIndex].iov_len,sizeof(char),sdpFile);
    
	/* ����ָ��д�����Ϣ�ĳ��� */
    return theSDPVec[theIndex].iov_len;
}


/* important function */
/* ģ��������,used in QTSServer::LoadCompiledInModules() */
QTSS_Error QTSSFileModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSFileModuleDispatch);
}


/* important function,��RTPSession::Run()����  */
/* ��Բ�ͬ�Ľ�ɫ,���벻ͬ�Ĳ���,���� */
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
        case QTSS_RTSPRequest_Role:/* ����RTSP request��������send packet to use */
            return ProcessRTSPRequest(&inParamBlock->rtspRequestParams);
        case QTSS_RTPSendPackets_Role: /* we are use it in RTPSession.cpp soon */
            return SendPackets(&inParamBlock->rtpSendPacketsParams);
        case QTSS_ClientSessionClosing_Role:
            return DestroySession(&inParamBlock->clientSessionClosingParams);
    }
    return QTSS_NoErr;
}

/* functions used in QTSSFileModuleDispatch() above  */
/* ע��Role,ΪTextMessagesObject\RTPSession\RTPStream��Ӿ�̬�������Ƽ�ID,����ģ������ */
QTSS_Error Register(QTSS_Register_Params* inParams)
{
    // Register for roles
	/* Ϊ��ģ��ע��Role(��ʼ��,��Ԥ��ֵ,����RTSP����,RTP�Ự�ر�),Ϊ��û��QTSS_RTPSendPackets_Role ? */
    (void)QTSS_AddRole(QTSS_Initialize_Role);
    (void)QTSS_AddRole(QTSS_RTSPRequest_Role);
    (void)QTSS_AddRole(QTSS_ClientSessionClosing_Role);
    (void)QTSS_AddRole(QTSS_RereadPrefs_Role);

    // Add text messages attributes
	/* ������Ҫ��Ӿ�̬����(���ƺ�׺ΪName)��qtssTextMessagesObjectType,����������ID(��׺Err) */
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
	/* ��qtssClientSessionObjectType(Ҳ����RTP Session)��Ӿ�̬����(�ļ��Ự��,���Ŵ���,SDP�����ӳ�),ͬʱ��������ID */
    static char*        sFileSessionName    = "QTSSFileModuleSession";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionName, NULL, qtssAttrDataTypeVoidPointer);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionName, &sFileSessionAttr);
    
    static char*        sFileSessionPlayCountName   = "QTSSFileModulePlayCount";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionPlayCountName, NULL, qtssAttrDataTypeUInt32);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionPlayCountName, &sFileSessionPlayCountAttrID);
    
    static char*        sFileSessionBufferDelayName = "QTSSFileModuleSDPBufferDelay";
    (void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sFileSessionBufferDelayName, NULL, qtssAttrDataTypeFloat32);
    (void)QTSS_IDForAttr(qtssClientSessionObjectType, sFileSessionBufferDelayName, &sFileSessionBufferDelayAttrID);
    
	/* ��qtssRTPStreamObjectType(Ҳ����RTP Stream)��Ӿ�̬����(��һ�����ͳ���RTP����seqnum,��һ��Ҫ�ͳ���RTP����seqnum),ͬʱ��������ID  */
     static char*        sRTPStreamLastSentPacketSeqNumName   = "QTSSFileModuleLastSentPacketSeqNum";
    (void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sRTPStreamLastSentPacketSeqNumName, NULL, qtssAttrDataTypeUInt16);
    (void)QTSS_IDForAttr(qtssRTPStreamObjectType, sRTPStreamLastSentPacketSeqNumName, &sRTPStreamLastSentPacketSeqNumAttrID);
   

    static char*        sRTPStreamLastPacketSeqNumName   = "QTSSFileModuleLastPacketSeqNum";
    (void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sRTPStreamLastPacketSeqNumName, NULL, qtssAttrDataTypeUInt16);
    (void)QTSS_IDForAttr(qtssRTPStreamObjectType, sRTPStreamLastPacketSeqNumName, &sRTPStreamLastPacketSeqNumAttrID);

    // Tell the server our name!
	/* ����Server module���� */
    static char* sModuleName = "QTSSFileModule";
    ::strcpy(inParams->outModuleName, sModuleName);

    return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
    QTRTPFile::Initialize();
    QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);//��������̬������ʼ��

	/* �����������������濪ͷ�ж��� */
    sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);/* �õ�ModuleԤ��ֵ */
    sServerPrefs = inParams->inPrefs;/* �õ�������Ԥ��ֵ */
    sServer = inParams->inServer;/* �õ������� */
        
    // Read our preferences
	/* ��������� */
    RereadPrefs();
    
    // Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
	/* see RTSPProtocol.h */
    static QTSS_RTSPMethod sSupportedMethods[] = { qtssDescribeMethod, qtssSetupMethod, qtssTeardownMethod, qtssPlayMethod, qtssPauseMethod };
    /* �������֧�ֵķ����� */
	QTSSModuleUtils::SetupSupportedMethods(inParams->inServer, sSupportedMethods, 5);

    return QTSS_NoErr;
}

/* ����sURLHeader��sEmailHeader */
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
        sdpURLLen = theDefaultDNS.Len + sHTTP.Len + 1;/* '/',���¼��� */
        
    sURLHeader.Delete();
    sURLHeader.Len = sdpURLLen + 10;
	/* ����ָ������ */
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
/* ��ģ��ͷ�����Ԥ��ֵ����ȡ��ص�Ԥ��ֵ */
QTSS_Error RereadPrefs()
{
    //����sFlowControlProbeInterval,sDefaultFlowControlProbeInterval
    QTSSModuleUtils::GetAttribute(sPrefs, "flow_control_probe_interval",    qtssAttrDataTypeUInt32,
                                &sFlowControlProbeInterval, &sDefaultFlowControlProbeInterval, sizeof(sFlowControlProbeInterval));
    //����sMaxAllowedSpeed,sDefaultMaxAllowedSpeed
    QTSSModuleUtils::GetAttribute(sPrefs, "max_allowed_speed",  qtssAttrDataTypeFloat32,
                                &sMaxAllowedSpeed, &sDefaultMaxAllowedSpeed, sizeof(sMaxAllowedSpeed));
                                
// File Cache prefs     
    //����sEnableSharedBuffers                
    sEnableSharedBuffers = true;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_shared_file_buffers", qtssAttrDataTypeBool16, &sEnableSharedBuffers,  sizeof(sEnableSharedBuffers));

	//����sEnablePrivateBuffers
    sEnablePrivateBuffers = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_private_file_buffers", qtssAttrDataTypeBool16, &sEnablePrivateBuffers, sizeof(sEnablePrivateBuffers));

	//����sSharedBufferInc
    sSharedBufferInc = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_shared_buffer_increase_per_session", qtssAttrDataTypeUInt32,&sSharedBufferInc, sizeof(sSharedBufferInc));
      
	//����sSharedBufferUnitKSize
    sSharedBufferUnitKSize = 64;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "shared_buffer_unit_k_size", qtssAttrDataTypeUInt32, &sSharedBufferUnitKSize, sizeof(sSharedBufferUnitKSize));

	//����sPrivateBufferUnitKSize
    sPrivateBufferUnitKSize = 64;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "private_buffer_unit_k_size", qtssAttrDataTypeUInt32, &sPrivateBufferUnitKSize, sizeof(sPrivateBufferUnitKSize));

	//����sSharedBufferUnitSize
    sSharedBufferUnitSize = 1;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_shared_buffer_units_per_buffer", qtssAttrDataTypeUInt32,&sSharedBufferUnitSize, sizeof(sSharedBufferUnitSize));

	//����sPrivateBufferUnitSize
    sPrivateBufferUnitSize = 1;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "num_private_buffer_units_per_buffer", qtssAttrDataTypeUInt32,&sPrivateBufferUnitSize, sizeof(sPrivateBufferUnitSize));
     
	//����sSharedBufferMaxUnits
    sSharedBufferMaxUnits = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "max_shared_buffer_units_per_buffer", qtssAttrDataTypeUInt32, &sSharedBufferMaxUnits, sizeof(sSharedBufferMaxUnits));
      
	//����sPrivateBufferMaxUnits
    sPrivateBufferMaxUnits = 8;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "max_private_buffer_units_per_buffer", qtssAttrDataTypeUInt32, &sPrivateBufferMaxUnits, sizeof(sPrivateBufferMaxUnits));

	//����sAddClientBufferDelaySecs
    sAddClientBufferDelaySecs = 0;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "add_seconds_to_client_buffer_delay", qtssAttrDataTypeFloat32, &sAddClientBufferDelaySecs, sizeof(sAddClientBufferDelaySecs));

	//����sRecordMovieFileSDP
    sRecordMovieFileSDP = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "record_movie_file_sdp", qtssAttrDataTypeBool16, &sRecordMovieFileSDP, sizeof(sRecordMovieFileSDP));

	//����sEnableMovieFileSDP
    sEnableMovieFileSDP = false;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_movie_file_sdp", qtssAttrDataTypeBool16, &sEnableMovieFileSDP, sizeof(sEnableMovieFileSDP));
    
	//����sPlayerCompatibility
    sPlayerCompatibility = true;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "enable_player_compatibility", qtssAttrDataTypeBool16, &sPlayerCompatibility, sizeof(sPlayerCompatibility));

	/* adjust media bandwidth percent */
	//����sAdjustMediaBandwidthPercent
    sAdjustMediaBandwidthPercent = 50;
    QTSSModuleUtils::GetIOAttribute(sPrefs, "compatibility_adjust_sdp_media_bandwidth_percent", qtssAttrDataTypeUInt32, &sAdjustMediaBandwidthPercent, sizeof(sAdjustMediaBandwidthPercent));

    if (sAdjustMediaBandwidthPercent > 100)
        sAdjustMediaBandwidthPercent = 100;
        
    if (sAdjustMediaBandwidthPercent < 1)
        sAdjustMediaBandwidthPercent = 1;
     
	/* permit thinning */
	//�ӷ�������ȡ����sDisableThinning
    UInt32 len = sizeof(sDisableThinning);
    (void) QTSS_GetValue(sServerPrefs, qtssPrefsDisableThinning, 0, (void*)&sDisableThinning, &len);

    BuildPrefBasedHeaders();
    
    return QTSS_NoErr;
}

/* ��RTSP���ӵĽӿڣ����ݻ�õ�RTSP Request���巽���������������� */
QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParamBlock)
{
	/* ��client���͵ı�׼RTSP�Ự�л�ȡRTSP����,���������Ϸ�,��������ʧ�� */
    QTSS_RTSPMethod* theMethod = NULL;
    UInt32 theMethodLen = 0;
    if ((QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqMethod, 0,
            (void**)&theMethod, &theMethodLen) != QTSS_NoErr) || (theMethodLen != sizeof(QTSS_RTSPMethod)))
    {
        Assert(0);
        return QTSS_RequestFailed;
    }
    
	/* ��Ի�ȡ�ķ�������������,���ò�ͬ�ĺ������� */
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
			/* ͬʱ֪ͨ�ͻ���Ҫ�ж����� */
            (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
            break;
        case qtssPauseMethod:
        {    (void)QTSS_Pause(inParamBlock->inClientSession);
		    /* ͬʱ֪ͨ�ͻ���Ҫ��ͣ���� */
            (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
              
			/* ��ȡ�ļ��Ự����,��ʧ��,���ػỰʧ�� */
            FileSession** theFile = NULL;
            UInt32 theLen = 0;
            QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
            if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
                return QTSS_RequestFailed;

			/* ͬʱ��ʱ��¼��ͣʱ����Ϣ/�� */
            (**theFile).fPaused = true;
            (**theFile).fLastPauseTime = OS::Milliseconds();

            break;
        }
        default:
            break;
    }
	/* �Ը��ֽ��������������,���������������жϻỰ */
    if (err != QTSS_NoErr)
        (void)QTSS_Teardown(inParamBlock->inClientSession);

    return QTSS_NoErr;
}

/* �ж�ý���ļ��ı��ص�ַ����󼸸��ַ��Ƿ���".sdp",�Ǿͷ���true,��ͷ���false  */
Bool16 isSDP(QTSS_StandardRTSP_Params* inParamBlock)
{
	Bool16 sdpSuffix = false;
	
    char* path = NULL;
    UInt32 len = 0;
	/* lock QTSS_RTSPRequestObject object */
	QTSS_LockObject(inParamBlock->inRTSPRequest);
	/* ��Client��RTSP Request�л�ȡ����ý���ļ��ı��ص�ַ */
    QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&path, &len);
    Assert(theErr == QTSS_NoErr);
	
    if (sSDPSuffix.Len <= len)
	{
		/*�ж�ý���ļ��ı��ص�ַ����󼸸��ַ��Ƿ���".sdp",�Ǿ�����sdpSuffixΪfalse */
		StrPtrLen thePath(&path[len - sSDPSuffix.Len],sSDPSuffix.Len);
		sdpSuffix = thePath.Equal(sSDPSuffix);
	}
	
	/* unlock QTSS_RTSPRequestObject object */
	QTSS_UnlockObject(inParamBlock->inRTSPRequest);
	
	return sdpSuffix;
}

/* ע���⼸����:thePath,thePathLen, theSDPVec,theSDPData,theFullSDPBuffer,fullSDPBuffSPL */
QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParamBlock)
{
	/* ����ָ����RTSP Request�е�Local file path����ĩ�����ַ�����".sdp",����Client����"ָ��·�����ļ�û��SDP�ļ���"���� */
	/* ע������Ĵ����DoSetup()/DoPlay()����ͬ�ģ��������� */
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
    FileSession*    theFile = NULL; /* �����ļ��Ự����ʹ�� */
    QTSS_Error      theErr = QTSS_NoErr;
    Bool16          pathEndsWithMOV = false;/* ·��ĩβ��MOV����? */
    static StrPtrLen sMOVSuffix(".mov");
    SInt16 vectorIndex = 1; /* ��λ��SDP�ļ��ĵ�һ�� */
    ResizeableStringFormatter theFullSDPBuffer(NULL,0);/* ���ش��SDP�ļ��Ļ��� */
    StrPtrLen bufferDelayStr;/* sdp�ļ��еĻ����ӳ��ַ���,Ӧ�ü����� */
    char tempBufferDelay[64];/* ��Ż����ӳٵ��ַ���"a=x-bufferdelay:0.2254",����Ϊ64���ַ�,Ӧ�ü����� */
    StrPtrLen theSDPData;/* ��Ŵ�ԭSDP�ļ��ж�ȡ������,�����ǵ�����sdp�ļ�,���Ǵ�sdpԭ�ӻ�õ���Ϣ.sdp data which may be a isolated file or a built-in file in udta.hnti.sdp atom */
      
	/*  ��ȡFileSession��ָ�뼰���� */
    (void)QTSS_GetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, (void*)&theFile, &theLen);
    // Generate the complete file path
    UInt32 thePathLen = 0;
	/* thePath������request��ý���ļ��ı��������ļ�·��(��"/sample_300kbit.mp4"),�ļ�·������ΪthePathLen,������׺'.sdp'(��ӵ�) */
    OSCharArrayDeleter thePath(QTSSModuleUtils::GetFullPath(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath,&thePathLen, &sSDPSuffix));
        
    //first locate the target movie
	/* �ض�'.sdp',�ж�RTSP Request��ý���ļ��Ƿ�mov�ļ�? */
    thePath.GetObject()[thePathLen - sSDPSuffix.Len] = '\0';//truncate the .sdp added in the GetFullPath call
    StrPtrLen   requestPath(thePath.GetObject(), ::strlen(thePath.GetObject()));//get request path
	/* ����isSDP()����,ȷ�������·���Ƿ�����'.MOV'��β? �ڸ�·��ĩ��ȡһ����'.mov'�ȳ���һ�ν��бȽ� */
    if (requestPath.Len > sMOVSuffix.Len )
    {   
		StrPtrLen endOfPath(&requestPath.Ptr[requestPath.Len - sMOVSuffix.Len], sMOVSuffix.Len);
        if (endOfPath.Equal(sMOVSuffix)) // it is a .mov
        {  
			pathEndsWithMOV = true;
        }
    }
    
	/* �����Ѵ����ļ��ỰFile Session,�Ƚ�RTSP Request�����ý���ļ���FileSession�е��ļ����Ƿ���ͬ,���粻ͬ,ɾȥ�ɵ�FileSession */
    if ( theFile != NULL )  
    {
        // There is already a file for this session. This can happen if there are multiple DESCRIBES,
        // or a DESCRIBE has been issued with a Session ID, or some such thing.
		/* ��File Session��ȡ��Movie·�����Ǵ�RTSP request��ȡ��Movie·�� */
        StrPtrLen   moviePath( theFile->fFile.GetMoviePath() ); /* get movie path(��"/sample_300kbit.mp4") */
        
        // This describe is for a different file. Delete the old FileSession.
		/* �Ƚ�RTSP Request�����ý���ļ���FileSession�е��ļ����Ƿ���ͬ,���粻ͬ,ɾȥ�ɵ�FileSession */
        if ( !requestPath.Equal( moviePath ) )
        {
            DeleteFileSession(theFile);
            theFile = NULL;
            
            // NULL out the attribute value, just in case(��ո�����ֵ���Է���һ).
            (void)QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
        }
    }

	/* ���粻�����ļ��ỰFile Session,�ʹ���FileSession����,������ָ�����������ļ�·����QTRTPFile,���洢FileSessionAttr��ClientSession(��RTP session)�� */
    if ( theFile == NULL )
    {   
		/* ��̬����FileSession���󲢳�ʼ�������ݳ�ԱQTRTPFile,���ڴ���ʱ,��Client���ʹ���ԭ�� ,��ֱ�ӷ��� */
        theErr = CreateQTRTPFile(inParamBlock, thePath.GetObject(), &theFile);
        if (theErr != QTSS_NoErr)
            return theErr;
    
        // Store this newly created file object in the RTP session.
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
    }
    
    //replace the sacred character we have trodden on in order to truncate the path.
	/* thePath stores full file path with length thePathLen and ended by 'mov' */
	/* �ص�ĩβ�ȳ���,����'.sdp' */
    thePath.GetObject()[thePathLen - sSDPSuffix.Len] = sSDPSuffix.Ptr[0];

    iovec theSDPVec[sNumSDPVectors];//ע��������ǳ���Ҫ,1 for the RTSP header, 6 for the sdp header, 1 for the sdp body,size 22
    ::memset(&theSDPVec[0], 0, sizeof(theSDPVec));/* ��ʼ��SDP�ļ���һ��?? */
    
	/* if permit present the sdp file of movie */
    if (sEnableMovieFileSDP)
    {
        // Check to see if there is an sdp file, if so, return that file instead
        // of the built-in sdp. ReadEntireFile allocates memory but if all goes well theSDPData will be managed by the File Session
		/* ��ָ�����ļ�·����ȡsdp���ݽ�theSDPData(C-String) */
		/********************* ע��˴���theSDPData��������*********************************/
        (void)QTSSModuleUtils::ReadEntireFile(thePath.GetObject(), &theSDPData);
    }

	/***************** ���濪ʼ����ָ����sdp�ļ����� ***************/
    OSCharArrayDeleter sdpDataDeleter(theSDPData.Ptr); // Just in case we fail we know to clean up. But we clear the deleter if we succeed.

	/* ����sdp�ļ�����(���Ǵ�ָ��·����sdp�ļ������),�������ָ������theSDPData,�������Ƿ�Ϸ�?Ȼ�󸽼�ModDateͷ��CacheControlͷ */
    if (theSDPData.Len > 0)
    {  
		/* related defs see SDPUtils.h */
        SDPContainer fileSDPContainer; 
		/* set sdp buffer,parse it and add it as the next sdpline of current line */
        fileSDPContainer.SetSDPBuffer(&theSDPData);
		/* check sdp file buffer if valid ? */
        if (!fileSDPContainer.IsSDPBufferValid())
			/* ��ͻ��˷���Ϣ��������Ӧ"Movie SDP is not valid.",qtssUnsupportedMediaType�Ǵ���״̬����,sSDPNotValidMessage��Message Text String */
        {    return QTSSModuleUtils::SendErrorResponseWithMessage(inParamBlock->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
        }
    
        
		/* if added sdp line is valid */
        // Append the Last Modified header to be a good caching proxy citizen before sending the Describe
		/* QTSS_AppendRTSPHeader�ص����̽���ͷ��Ϣ���ӵ�RTSP��ͷ�С��ڵ���QTSS_AppendRTSPHeader����֮��
		���Խ����ŵ���QTSS_SendRTSPHeaders����������������ͷ�� */
		/*
		   ��ͻ��˻ظ���������:
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
                                                                &theSDPVec[0], 3, theSDPData.Len); //3��iovec 
    }
    else  /* ����theSDPDataû��ֵ */
    {
        // Before generating the SDP and sending it, check to see if there is an If-Modified-Since
        // date. If there is, and the content hasn't been modified, then just return a 304 Not Modified
		// ���RTSP Request���Ƿ���If-Modified-Sinceͷ,������,����QTFile�м�¼��modified date��ͬ,����Client����"RTSP/1.0 304 Not Modified",��ֱ�ӷ���
        QTSS_TimeVal* theTime = NULL;
        (void) QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqIfModSinceDate, 0, (void**)&theTime, &theLen);
        if ((theLen == sizeof(QTSS_TimeVal)) && (*theTime > 0))
        {
            // There is an If-Modified-Since header. Check it vs. the content.
            if (*theTime == theFile->fFile.GetQTFile()->GetModDate())
            {
				/* set RTSP Request status codeΪ:"RTSP/1.0 304 Not Modified" */
                theErr = QTSS_SetValue( inParamBlock->inRTSPRequest, qtssRTSPReqStatusCode, 0,
                                        &kNotModifiedStatus, sizeof(kNotModifiedStatus) );
                Assert(theErr == QTSS_NoErr);
                // Because we are using this call to generate a 304 Not Modified response, we do not need
                // to pass in a RTP Stream /* ��inFlags��Ϊ0 */
                theErr = QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, 0);
                Assert(theErr == QTSS_NoErr);
                return QTSS_NoErr;
            }
        }
        
		/**************** ����SDP�ļ�ָ��!!����������SDP�ļ���������,����MOV�ļ�����ָ��·����sdp�ļ� ********************/
        FILE* sdpFile = NULL;
        if (sRecordMovieFileSDP && pathEndsWithMOV) // don't auto create sdp for a non .mov file because it would look like a broadcast
        {   
			/* open a local sdp file in specific path and in read-only means */
            sdpFile = ::fopen(thePath.GetObject(),"r"); // see if there already is a .sdp for the movie
			/* �����·���Ѿ���һ��SDP�ļ�,��Ҫ�ƻ���,��Ȼ�˴�Ҳ���ö��������� */
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

		/* ��ȡRTSP Request���ļ�·��(��"/sample_300kbit.mp4"),����fileNameStr */
        char* fileNameStr = NULL;
        (void)QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, &fileNameStr);
        QTSSCharArrayDeleter fileNameStrDeleter(fileNameStr);/* �����Ժ�ɾ����׼�� */
        	
        //Get IP addr
		/* ��ȡRTSP Session�ı���IP��ַ(�Դ����ʮ��������ʽ��ʾ) */
        StrPtrLen ipStr;
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPSession, qtssRTSPSesLocalAddrStr, 0, (void**)&ipStr.Ptr, &ipStr.Len);


//      
// *** The order of sdp headers is specified and required by rfc 2327
//
// -------- version header 

		/* �򻺴���д��"v=0\r\n" */
        theFullSDPBuffer.Put(sVersionHeader);
        theFullSDPBuffer.Put(sEOL);
        
// -------- owner header

        const SInt16 sLineSize = 256;
        char ownerLine[sLineSize]="";/* ��������Ϊ256���ַ��Ŀ��� */
        ownerLine[sLineSize - 1] = 0;/* �������һ���ַ�ΪNULL */
        
        char *ipCstr = ipStr.GetAsCString();/* �õ�RTSP Session�ı���IP��ַ(������)��C-string */
        OSCharArrayDeleter ipDeleter(ipCstr);/* ����ɾ��׼�� */
        
        // the first number is the NTP time used for the session identifier (this changes for each request)
        // the second number is the NTP date time of when the file was modified (this changes when the file changes)
        qtss_sprintf(ownerLine, "o=StreamingServer %"_64BITARG_"d %"_64BITARG_"d IN IP4 %s", (SInt64) OS::UnixTime_Secs() + 2208988800LU, (SInt64) theFile->fFile.GetQTFile()->GetModDate(),ipCstr);
        Assert(ownerLine[sLineSize - 1] == 0); /* ȷ���������һ���ַ�û���Ķ�����ʼ������� */

		/* �򻺴���д����Ϊ"o=StreamingServer 3487035788 1259131481000 IN IP4 172.16.34.22\r\n"���� */ 
        StrPtrLen ownerStr(ownerLine);
        theFullSDPBuffer.Put(ownerStr); 
        theFullSDPBuffer.Put(sEOL); 
        
// -------- session header

		/* �򻺴���д������"s=/sample_300kbit.mp4\r\n" */ 
        theFullSDPBuffer.Put(sSessionNameHeader);
        theFullSDPBuffer.Put(fileNameStr);
        theFullSDPBuffer.Put(sEOL);
    
// -------- uri header

		/* �򻺴���д������"u=http:///\r\n" */
        theFullSDPBuffer.Put(sURLHeader);
        theFullSDPBuffer.Put(sEOL);

    
// -------- email header

		/* �򻺴���д������"e=admin@\r\n" */ 
        theFullSDPBuffer.Put(sEmailHeader);
        theFullSDPBuffer.Put(sEOL);

// -------- connection information header
        
		/* �򻺴���д������"c=IN IP4 0.0.0.0\r\n" */
        theFullSDPBuffer.Put(sConnectionHeader); 
        theFullSDPBuffer.Put(sEOL);

// -------- time header

        // t=0 0 is a permanent always available movie (doesn't ever change unless we change the code)
		/* �򻺴���д������"t=0 0\r\n" */
        theFullSDPBuffer.Put(sPermanentTimeHeader);
        theFullSDPBuffer.Put(sEOL);
        
// -------- control header

		/* �򻺴���д������"a=control:*\r\n" */
        theFullSDPBuffer.Put(sStaticControlHeader);
        theFullSDPBuffer.Put(sEOL);
        
                
// -------- add buffer delay:"a=x-bufferdelay:00000.00\r\n" 

		/* ����Client���л�����ʱ,��ѰtheSDPData(���洢��SDP�ļ�������)��ԭ�е�"a=x-bufferdelay:"��ͷ���ַ���,�ټ���Client�˵Ļ�����ʱֵ,ע�������������һ�����Ƕ�״��ݽṹ */
        if (sAddClientBufferDelaySecs > 0) // increase the client buffer delay by the preference amount.
        {
            Float32 bufferDelay = 3.0; // ���ֵ�ǲµ�:the client doesn't advertise it's default value so we guess.
            
            static StrPtrLen sBuffDelayStr("a=x-bufferdelay:");
        
            StrPtrLen delayStr;
			/* ��theSDPData�в�����"a=x-bufferdelay:"��ͷ���ַ�������ֵ��delayStr */
			/* theSDPData�ĸ�ֵ������ */
            theSDPData.FindString(sBuffDelayStr, &delayStr);
			/* �жϻ�ȡ���ַ���delayStr,�ҳ���������bufferDelay */
            if (delayStr.Len > 0)
            {
				/* ����delayStr����ĩ�˶�theSDPData�ײ���ƫ�� */
                UInt32 offset = (delayStr.Ptr - theSDPData.Ptr) + delayStr.Len; // step past the string
				/* ע��delayStr��theSDPData����Խ��ԭdelayStr���ʣ�ಿ�� */
                delayStr.Ptr = theSDPData.Ptr + offset;
                delayStr.Len = theSDPData.Len - offset;
				/* ����StringParser����󲢳�ʼ�� */
                StringParser theBufferSecsParser(&delayStr);
				/* ���յ�ͷ���ո� */
                theBufferSecsParser.ConsumeWhitespace();
				/* ȡ�����еĸ�������bufferDelay,���ʼ��ֵΪ3 */
                bufferDelay = theBufferSecsParser.ConsumeFloat();
            }
            
			/* ���з���������ʱ,���������Ϳͻ���BufferDelay���,����,ֻ���Ͽͻ��˻��� */
            bufferDelay += sAddClientBufferDelaySecs;

            /* ��ָ����ʽ������ʱ���ַ���tempBufferDelay,�������������ͷ���� */
            qtss_sprintf(tempBufferDelay, "a=x-bufferdelay:%.2f",bufferDelay);
            bufferDelayStr.Set(tempBufferDelay);//���û����ӳ��ַ���bufferDelayStr 
             
			/* �򻺴���д������"a=x-bufferdelay:00000.00\r\n" */
            theFullSDPBuffer.Put(bufferDelayStr); 
            theFullSDPBuffer.Put(sEOL);
        }
        
 // -------- Add the movie's sdp headers to our sdp headers

        //now append content-determined sdp ( cached in QTRTPFile ),����
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
		/********************* ע��˴���theSDPData����ý���ļ��е�SDP����!!*********************************/
        theSDPData.Ptr = theFile->fFile.GetSDPFile(&sdpLen);
        theSDPData.Len = sdpLen; 
		/* ��QTRTPFile�е�SDP���ݶ�����,��ÿ��audio/video track,����һ��������Ϣ */
		theFullSDPBuffer.Put(theSDPData);


		/********************* ���濪ʼ��������ĵ�SDP����*************************************************/
		/* ���¶���һ���µ�StrPtrLen�Ķ���,��ǰ����theFullSDPBuffer����ʼָ�����д�����ֽ�����ʼ�� */
        StrPtrLen fullSDPBuffSPL(theFullSDPBuffer.GetBufPtr(),theFullSDPBuffer.GetBytesWritten());

// ------------ Check the headers
		/* ������ͷ�ĺϷ���,���������� */
		/* ע��: SetSDPBuffer()�Ѿ�������Initialize(),Parse(),���Ϸ���sdp��д�뵱ǰ�е���һ��,�����غϷ������Ľ���*/
        SDPContainer rawSDPContainer;
		/* input the given string, store in buffer, then parse its validity and add it to the next sdp line to current line */
        rawSDPContainer.SetSDPBuffer( &fullSDPBuffSPL ); 
		/* ����SDP��Ϣ���Ϸ�,����ͻ��˷���һ����Ϣ"Movie SDP is not valid.",�����Ƿ���QTSS_RequestFailed */
        if (!rawSDPContainer.IsSDPBufferValid())
        {    
			return QTSSModuleUtils::SendErrorResponseWithMessage(inParamBlock->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
        }
		
// ------------ reorder the sdp headers to make them proper.��ǡ��˳������SDPͷ
        Float32 adjustMediaBandwidthPercent = 1.0;
        Bool16 adjustMediaBandwidth = false;
        if (sPlayerCompatibility )//��ֵĬ��Ϊtrue,���ݲ�����
			/* ���ҷ�����Ԥ��ֵ�������Ƿ���AdjustBandwidth����,����boolֵ */
            adjustMediaBandwidth = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParamBlock,QTSSModuleUtils::kAdjustBandwidth);
		    
		if (adjustMediaBandwidth)
		    adjustMediaBandwidthPercent = (Float32) sAdjustMediaBandwidthPercent / 100.0;//���õ�������ٷֱ�Ϊ50%,���������sortedSDP������Ҫ��

// ----------- get session header and media header from sdp cache

		SDPLineSorter sortedSDP(&rawSDPContainer,adjustMediaBandwidthPercent);
		/* ��sSessionOrderedLines[]= "vosiuepcbtrzka"Ϊ�̶�˳��,������ͬ����ĸ��ͷ���з���һ��,�γ�fSDPSessionHeaders */
		StrPtrLen *theSessionHeadersPtr = sortedSDP.GetSessionHeaders();
		/* ��ÿ��auido/video track,�Ӷ�Ӧ��m��ͷ�����Ժ��ҵ����ܵ�b��ͷ����,������MediaBandwidth,����ʼ����Ա����fMediaHeaders */
		StrPtrLen *theMediaHeadersPtr = sortedSDP.GetMediaHeaders();
		
// ----------- write out the sdp

		/* ��theSessionHeadersPtr��theMediaHeadersPtr����д��sdp file,ע��*ioVectorIndex�Զ���1 */
		totalSDPLength += ::WriteSDPHeader(sdpFile, theSDPVec, &vectorIndex, theSessionHeadersPtr);//��1������,��0������������?
        totalSDPLength += ::WriteSDPHeader(sdpFile, theSDPVec, &vectorIndex, theMediaHeadersPtr); //��2������
 

// -------- done with SDP processing
        /* ���ǵùرմ򿪵�SDP�ļ� */
        if (sdpFile !=NULL)
            ::fclose(sdpFile);
            

        Assert(theSDPData.Len > 0);//ע��,��ʱtheSDPData�洢���Ǵ�sdpԭ���л�õ���Ϣ
        Assert(theSDPVec[2].iov_base != NULL);//���ǵ�3��iovec
        //ok, we have a filled out iovec. Let's send the response!
        
        // Append the Last Modified header to be a good caching proxy citizen before sending the Describe
		/*
		   ��ͻ��˻ظ���������:
		   Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
		   Cache-Control: must-revalidate\r\n
		*/
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssLastModifiedHeader,
                                        theFile->fFile.GetQTFile()->GetModDateStr(), DateBuffer::kDateBufferLen);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssCacheControlHeader,
                                        kCacheControlHeader.Ptr, kCacheControlHeader.Len);
		/* ͬ����ͻ��˷���Describe response,ע��vectorIndexֵΪ3 */
        QTSSModuleUtils::SendDescribeResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession,
                                                                        &theSDPVec[0], vectorIndex, totalSDPLength);    
    }
    
	/* Ϊ�����Parse()��׼��,��ʱtheSDPData��ŵ��Ǵ�SDP�ļ����sdpԭ�Ӷ�ȡ����Ϣ */
    Assert(theSDPData.Ptr != NULL);
    Assert(theSDPData.Len > 0);

    //now parse the movie media sdp data. We need to do this in order to extract payload(audio or video) information.
    //The SDP parser object will not take responsibility of the memory (one exception... see above)
	/* ��������������Ǵ����е�sdp��Ϣ����ȡ�������ԱfStreamArray�Ľṹ��StreamInfo����Ϣ,������fStreamArray�ĸ�����Ϣ */
    theFile->fSDPSource.Parse(theSDPData.Ptr, theSDPData.Len);
    sdpDataDeleter.ClearObject(); // don't delete theSDPData, theFile has it now.
    
    return QTSS_NoErr;
}

/* ��̬����FileSession���󲢳�ʼ�������ݳ�ԱQTRTPFile,���ڴ���ʱ,��Client���ʹ���ԭ�� */
/* used in QTSSFileModule::DoSetup()/DoDescribe() */
QTSS_Error CreateQTRTPFile(QTSS_StandardRTSP_Params* inParamBlock, char* inPath, FileSession** outFile)
{   
	/* ��̬����FileSession���󲢳�ʼ�������ݳ�ԱQTRTPFile */
    *outFile = NEW FileSession();
	/* ��ʼ��QTRTPFileʱ����trackȫ��Ϊhint track */
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
		
		/* �Ը��ִ�����������δ��� */
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

/* ����RTSP Request����,�ָ��track id��RTPStream,������payload������,����,������ʱ,timescale,quality level����,��ͬ������QTRTPFile��ָ��track��
SSRC,payload type��RTPMetaInfo,����Client����һ����׼RTSP��Ӧ */
QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParamBlock)
{
    /* ����ָ����RTSP Request�е�Local file path����ĩ�����ַ�����".sdp",����Client����"ָ��·�����ļ�û��SDP�ļ���"���� */
	/* ע������Ĵ����DoDescribe()/DoPlay()����ͬ�ģ��������� */
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
	/* ��ȡFileSession����ָ��,�������,�ͷ���DoDescribe()����Ӧ����׼���� */
    QTSS_Error theErr = QTSS_GetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, (void*)&theFile, &theLen);
	/* �����ȡFile Session�������⣬����������File Session Attr */
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
    {
		//Get the full local path to the file.
        char* theFullPath = NULL;
        //theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&theFullPath, &theLen);
		/* QTSS_GetValueAsString�ص����̻�ȡָ�����Ե�ֵ����ת��ΪC�ַ�����ʽ���洢����theFullPath����ָ�����ڴ�λ���� */
		/* ����������ҪtheFullPathʱ����Ҫ����QTSS_Delete�������ͷ�Ϊ�������ڴ档����QTSS_GetValue�����ǻ�ȡ����ռ���ʰ�ȫ
		   ������ֵ���Ƽ�������������QTSS_GetValuePtr�������ǻ�ȡ��ռ���ʰ�ȫ������ֵ���Ƽ������� */
		theErr = QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqLocalPath, 0, &theFullPath);
        Assert(theErr == QTSS_NoErr);
        // This is possible, as clients are not required to send a DESCRIBE. If we haven't set
        // anything up yet, set everything up
		/* creat QTRTPFile ( belongs to FileSession theFile ) at the specific path   */
        theErr = CreateQTRTPFile(inParamBlock, theFullPath, &theFile);
		/* ����������ҪtheFullPathʱ����Ҫ����QTSS_Delete�������ͷ�Ϊ�������ڴ档 */
		QTSS_Delete(theFullPath);
        if (theErr != QTSS_NoErr)
            return theErr;
 
		/* get sdp data from just created QTRTPFile */
        int theSDPBodyLen = 0;
		/* see QTRTPFile::GetSDPFile(), use it immediately if sdp file already exist,otherwise create a new one  */
		/* �õ�SDP�ļ��Ļ���λ�úͳ��� */
        char* theSDPData = theFile->fFile.GetSDPFile(&theSDPBodyLen);

		
        //now parse the sdp. We need to do this in order to extract payload information(video or audio?).
        //The SDP parser object will not take responsibility of the memory (one exception... see above)
        theFile->fSDPSource.Parse(theSDPData, theSDPBodyLen);

        // Store this newly created file object in the RTP session.
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionAttr, 0, &theFile, sizeof(theFile));
    }

    //unless there is a digit at the end of this path (representing trackID), don't
    //even bother with the request
	/* ��ȡ"SETUP rtsp://172.16.34.22/sample_300kbit.mp4/trackID=3 RTSP/1.0\r\n"��β��һ�����߸������� */
    char* theDigitStr = NULL;
    (void)QTSS_GetValueAsString(inParamBlock->inRTSPRequest, qtssRTSPReqFileDigit, 0, &theDigitStr);
    QTSSCharArrayDeleter theDigitStrDeleter(theDigitStr); /* prepare to delete soon */
	if (theDigitStr == NULL)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssClientBadRequest, sExpectedDigitFilenameErr);
	
	/* ��atoi��atol��strtod��strtol��strtoul������ʵ���ַ�������ת����sprintf����CString���Format()�������Խ���ֵת��Ϊ�ַ��� */
	/* ���庯�� long int strtol(const char *nptr,char **endptr,int base); strtol()�Ὣ����nptr�ַ������ݲ���base��ת���ɳ���������
	����base��Χ��2��36����0������base������õĽ��Ʒ�ʽ���� baseֵΪ10�����10���ƣ���baseֵΪ16�����16���Ƶȡ� */
	/* �˴���theDigitStrת����ʮ�����ַ�����ʽ */
    UInt32 theTrackID = ::strtol(theDigitStr, NULL, 10);
    
//    QTRTPFile::ErrorCode qtfileErr = theFile->fFile.AddTrack(theTrackID, false); //test for 3gpp monotonic wall clocktime and sequence
	/* ����ָ��trackID��track */
    QTRTPFile::ErrorCode qtfileErr = theFile->fFile.AddTrack(theTrackID, true);
    
    //if we get an error back, forward that error to the client
	//�����Ҳ���ָ����track,����Client����"Track������",�����"��֧�ֵ�ý������"�Ĵ�����Ϣ
    if (qtfileErr == QTRTPFile::errTrackIDNotFound)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssClientNotFound, sTrackDoesntExistErr);
    else if (qtfileErr != QTRTPFile::errNoError)
        return QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest,
                                                    qtssUnsupportedMediaType, sBadQTFileErr);

    // Before setting up this track, check to see if there is an If-Modified-Since
    // date. If there is, and the content hasn't been modified, then just return a 304 Not Modified
	// ���RTSP Request���Ƿ���If-Modified-Sinceͷ,������,����QTFile�м�¼��modified date��ͬ,����Client����"RTSP/1.0 304 Not Modified",��ֱ�ӷ���
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

	//Create a new RTP stream Ϊָ����track,�һ��RTP Stream
	/* ֻ�е�����QTSS_AddRTPStream������ģ����ܵ���QTSS_Play����, see DoPlay() 
	   QTSS_AddRTSPStream�ص�����ʹһ��ģ�������ͻ��˷���RTP���ݰ�������ӦRTSP���󡣶�ε���QTSS_AddRTSPStream����
	   ������Ự�м������������ϣ����ʼ����һ���������Ե���QTSS_Play���� */
    QTSS_RTPStreamObject newStream = NULL;
    theErr = QTSS_AddRTPStream(inParamBlock->inClientSession, inParamBlock->inRTSPRequest, &newStream, 0);
    if (theErr != QTSS_NoErr)
        return theErr;

    //find the payload for this track ID (if applicable)
    StrPtrLen* thePayload = NULL;
    UInt32 thePayloadType = qtssUnknownPayloadType; //audio/video
    Float32 bufferDelay = (Float32) 3.0; // FIXME need a constant defined for 3.0 value. It is used multiple places

	/* ��SDP��Ϣ�л�ȡRTPStream�ĸ���,������ЩRTPStream,��StreamInfo����ָ��track id��RTPStream��Payload������,����,������ʱ */
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
    
    // Set the payload type, payload name & timescale of this track���ø�track�Ļ�����ʱ,����,����,ʱ��̶�,trackID,NumQualityLevels

    SInt32 theTimescale = theFile->fFile.GetTrackTimeScale(theTrackID);//��ȡָ��trackID��ʱ��̶�
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
    
    // Get the SSRC of this track,��������SSRC(�ڳɹ�����ʱ����SSRC)
    UInt32* theTrackSSRC = NULL;
    UInt32 theTrackSSRCSize = 0;
    (void)QTSS_GetValuePtr(newStream, qtssRTPStrSSRC, 0, (void**)&theTrackSSRC, &theTrackSSRCSize);
    // The RTP stream should ALWAYS have an SSRC assuming QTSS_AddStream succeeded.
    Assert((theTrackSSRC != NULL) && (theTrackSSRCSize == sizeof(UInt32)));
    
    //give the file some info it needs.����QTRTPFile��ָ��Track��SSRC��payload����
    theFile->fFile.SetTrackSSRC(theTrackID, *theTrackSSRC);
    theFile->fFile.SetTrackCookies(theTrackID, newStream, thePayloadType);
    
	/* ��ȡqtssXRTPMetaInfoHeaderָ��,��֧�ֵ�RTPMetaInfoFields���鸴����Ӧͷ,���ø�track��RTP Meta Info */
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
	/* ��ͻ��˷���һ��RTSP��Ӧ */
    theErr = QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, newStream, 0);
    Assert(theErr == QTSS_NoErr);
    return QTSS_NoErr;
}


/* ���Ȼ�ȡFileSession�еĲ��Ŵ���������(��1),������ʹ�ù�����,�Ҳ��Ŵ���Ϊ1,�ʹ���л���;������ʹ��˽�л���,�ͷ���˽�л���(����������32K��˽�л����) */
QTSS_Error SetupCacheBuffers(QTSS_StandardRTSP_Params* inParamBlock, FileSession** theFile)
{
    
	/* obtain play count and initialize */
    UInt32 playCount = 0;
    UInt32 theLen = sizeof(playCount);
	/* ��ȡ���Ŵ��� */
    QTSS_Error theErr = QTSS_GetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, (void*)&playCount, &theLen);
	/* handle error if necessary */
    if ( (theErr != QTSS_NoErr) || (theLen != sizeof(playCount)) )
    {
        playCount = 1;
		/* ���ò��Ŵ��� */
        theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, &playCount, sizeof(playCount));
        if (theErr != QTSS_NoErr)
            return QTSS_RequestFailed;
    }
    
	/* ������ʹ�ù�����,�Ҳ��Ŵ���Ϊ1 */
    if (sEnableSharedBuffers && playCount == 1) // increments num buffers after initialization so do only once per session
        /* �����ļ��������û�������(���8��),��ý���ļ�����ǡ����С��һ�������ָ�������ṹ,������Щָ������ */
		(*theFile)->fFile.AllocateSharedBuffers(sSharedBufferUnitKSize/*64*/, sSharedBufferInc/*8*/, sSharedBufferUnitSize/*1*/,sSharedBufferMaxUnits/*8*/);
    
	/* ������ʹ��˽�л���,�ͷ���˽�л��� */
    if (sEnablePrivateBuffers) // reinitializes buffers to current location so do every time 
		/* �����ļ������ʶ��,Ϊÿ���ļ���������������32K��˽�л���� */
        (*theFile)->fFile.AllocatePrivateBuffers(sSharedBufferUnitKSize/*64*/, sPrivateBufferUnitSize/*1*/, sPrivateBufferMaxUnits/*8*/);

	/* incerement by 1 and reset playcount attribute */
    playCount ++;
    theErr = QTSS_SetValue(inParamBlock->inClientSession, sFileSessionPlayCountAttrID, 0, &playCount, sizeof(playCount));
    if (theErr != QTSS_NoErr)
        return QTSS_RequestFailed;  
        
    return theErr;

}

/* ����������/˽�ļ�����,���õ�ǰFileSession,RTPSession,RTSP Request���������,��������ý���ļ� */
QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParamBlock)
{
    QTRTPFile::ErrorCode qtFileErr = QTRTPFile::errNoError;

	/* ����ָ����RTSP Request�е�Local file path����ĩ�����ַ�����".sdp",����Client����"ָ��·�����ļ�û��SDP�ļ���"���� */
	/* ע������Ĵ����DoSetup()/DoDescribe()����ͬ��,�������� */
    if (isSDP(inParamBlock))
    {
        StrPtrLen pathStr;
        (void)QTSS_LockObject(inParamBlock->inRTSPRequest);
        (void)QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqFilePath, 0, (void**)&pathStr.Ptr, &pathStr.Len);
        QTSS_Error err = QTSSModuleUtils::SendErrorResponse(inParamBlock->inRTSPRequest, qtssClientNotFound, sNoSDPFileFoundErr, &pathStr);
        (void)QTSS_UnlockObject(inParamBlock->inRTSPRequest);
        return err;
    }

	/* ��ȡFileSession����ָ��,�������,ֱ�ӷ��� */
    FileSession** theFile = NULL;
    UInt32 theLen = 0; /* used by QTSS_GetValuePtr in  many places below */
    QTSS_Error theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(FileSession*)))
        return QTSS_RequestFailed;

	/**************** ����������/˽�ļ����� *****************************/
	/* set cache buffers, def see above  */
    theErr = SetupCacheBuffers(inParamBlock, theFile);  
    if (theErr != QTSS_NoErr)
        return theErr;
    /**************** ����������/˽�ļ����� *****************************/

    // Set the default quality before playing.�������е�RTPStream,����ÿ��track��quality level
    QTRTPFile::RTPTrackListEntry* thePacketTrack;
    for (UInt32 x = 0; x < (*theFile)->fSDPSource.GetNumStreams(); x++)
    {
         SourceInfo::StreamInfo* theStreamInfo = (*theFile)->fSDPSource.GetStreamInfo(x);
		 /* if find no track entry ? */
         if (!(*theFile)->fFile.FindTrackEntry(theStreamInfo->fTrackID,&thePacketTrack))
            break;
		 /* ע��set track quality levelΪ�������еİ� */
         (*theFile)->fFile.SetTrackQualityLevel(thePacketTrack, QTRTPFile::kAllPackets);
    }


	/*********** ����FileSession����ʼʱ��� ******************/
    // How much are we going to tell the client to back up?
    Float32 theBackupTime = 0;

	/* get packet range header field "x-Packet-Range: pn=4551-4689;url=trackID3" */
    char* thePacketRangeHeader = NULL;
    theErr = QTSS_GetValuePtr(inParamBlock->inRTSPHeaders, qtssXPacketRangeHeader, 0, (void**)&thePacketRangeHeader, &theLen);
    if (theErr == QTSS_NoErr)
    {
        StrPtrLen theRangeHdrPtr(thePacketRangeHeader, theLen);
        StringParser theRangeParser(&theRangeHdrPtr);
        
		/* �����ʼ���� */
        theRangeParser.ConsumeUntil(NULL, StringParser::sDigitMask);
        UInt64 theStartPN = theRangeParser.ConsumeInteger();
        
		/* �����ֹ���� */
        theRangeParser.ConsumeUntil(NULL, StringParser::sDigitMask);
        (*theFile)->fStopPN = theRangeParser.ConsumeInteger();

		/* ���track id */
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
		/* ���RTSP request�������ʼʱ�� */
        theErr = QTSS_GetValuePtr(inParamBlock->inRTSPRequest, qtssRTSPReqStartTime, 0, (void**)&theStartTimeP, &theLen);
		/* ����û����ʼʱ��,����׼�����͵���һ������ʱ�� */
        if ((theErr != QTSS_NoErr) || (theLen != sizeof(Float64)))
        {   // No start time so just start at the last packet ready to send
            // This packet could be somewhere out in the middle of the file.
             currentTime =  (*theFile)->fFile.GetFirstPacketTransmitTime(); 
             theStartTimeP = &currentTime;  
             (*theFile)->fStartTime = currentTime;
        }    

          
		/* ��ȡRTSP request�б�ͷ"the x-Prebuffer: 20 "���õ������ʱ�� */
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
    
	/* �����ѯʱ����� */
    if (qtFileErr == QTRTPFile::errCallAgain)
    {
        //
        // If we are doing RTP-Meta-Info stuff, we might be asked to get called again here.
        // This is simply because seeking might be a long operation and we don't want to
        // monopolize(¢��) the CPU, but there is no other reason to wait, so just set a timeout of 0
        theErr = QTSS_SetIdleTimer(1);
        Assert(theErr == QTSS_NoErr);
        return theErr;
    }
    else if (qtFileErr != QTRTPFile::errNoError)
        return QTSSModuleUtils::SendErrorResponse(  inParamBlock->inRTSPRequest,
                                                    qtssClientBadRequest, sSeekToNonexistentTimeErr);
                                                        
    //make sure to clear the next packet the server would have sent!����ѷ��ͳ������ݰ�
    (*theFile)->fPacketStruct.packetData = NULL;
	(**theFile).fPaused = false;/* ��ǰ��FileSession״̬��PAUSE��?����,��PLAY */
	if ((**theFile).fLastPauseTime > 0)//��¼�ϴ��ж�ʱ��ʱ���
		(**theFile).fTotalPauseTime += OS::Milliseconds() - (**theFile).fLastPauseTime;/* �������ж�ʱ��(ms) */

    // Set the movie duration,size parameters,bitsPerSecond
	
    Float64 movieDuration = (*theFile)->fFile.GetMovieDuration();/* ����ý���ļ�����ʱ�� */
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieDurationInSecs, 0, &movieDuration, sizeof(movieDuration));
    UInt64 movieSize = (*theFile)->fFile.GetAddedTracksRTPBytes();/* ����ý���ļ���С */
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieSizeInBytes, 0, &movieSize, sizeof(movieSize));
    UInt32 bitsPerSecond =  (*theFile)->fFile.GetBytesPerSecond() * 8; /* ����ý���ļ���ƽ��������(bits/sec)*/
    (void)QTSS_SetValue(inParamBlock->inClientSession, qtssCliSesMovieAverageBitRate, 0, &bitsPerSecond, sizeof(bitsPerSecond));

    // For the purposes of the speed header, check to make sure all tracks are over a reliable transport
	/* note: UDP is unreliable, but Reliable UDP and TCP reliable */
    Bool16 allTracksReliable = true;
    
    // Set the timestamp & sequence number parameters for each track.
	/* �ڵ���QTSS_Play����֮ǰ,ģ��Ӧ��Ϊÿ��RTP������������ЩQTSS_RTPStreamObject���������:
	qtssRTPStrFirstSeqNumber,qtssRTPStrFirstTimestamp,qtssRTPStrTimescale */
    QTSS_RTPStreamObject* theRef = NULL;
    for (   UInt32 theStreamIndex = 0;
            QTSS_GetValuePtr(inParamBlock->inClientSession, qtssCliSesStreamObjects, theStreamIndex, (void**)&theRef, &theLen) == QTSS_NoErr;
            theStreamIndex++)
    {
        UInt32* theTrackID = NULL;
        theErr = QTSS_GetValuePtr(*theRef, qtssRTPStrTrackID, 0, (void**)&theTrackID, &theLen);//�õ�RTPStream��track ID
        Assert(theErr == QTSS_NoErr);
        Assert(theTrackID != NULL);
        Assert(theLen == sizeof(UInt32));
        
        UInt16 theSeqNum = 0;//�õ�ָ��track ID��RTPStream�ĵ�ǰʱ���
        UInt32 theTimestamp = (*theFile)->fFile.GetSeekTimestamp(*theTrackID); // this is the base timestamp need to add in paused time. 
		
        Assert(theRef != NULL);

		/* get RTPStream timescale to calculate pause timestamp and reset the timestamp */
        UInt32* theTimescale = NULL;/* �õ���RTPStream��timescale */
        QTSS_GetValuePtr(*theRef, qtssRTPStrTimescale, 0,  (void**)&theTimescale, &theLen);
        if (theLen != 0) // adjust the timestamps to reflect paused time else leave it alone we can't calculate the timestamp without a timescale.
        {
			//�����ж�ʱ���
            UInt32 pauseTimeStamp = CalculatePauseTimeStamp( *theTimescale,  (*theFile)->fTotalPauseTime, (UInt32) theTimestamp);
            if (pauseTimeStamp != theTimestamp) //���µ�ǰʱ���
                  theTimestamp = pauseTimeStamp;
       }

       
		/* ���ø�RTP stream�ĵ�һ��SeqNum�͵�һ��ʱ��� */
	    theSeqNum = (*theFile)->fFile.GetNextTrackSequenceNumber(*theTrackID);       
        theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstSeqNumber, 0, &theSeqNum, sizeof(theSeqNum));
        Assert(theErr == QTSS_NoErr);
        theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstTimestamp, 0, &theTimestamp, sizeof(theTimestamp));
        Assert(theErr == QTSS_NoErr);

		/* ͨ����ȡ��RTP stream�Ĵ�������������allTracksReliable */
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
    // we don't care if it doesn't set (i.e. this is a meta info session)//�����Ƿ񶪵��ظ���?TCP��RUDP��Ҫ�����ظ���,UDP����
     (void)  (*theFile)->fFile.SetDropRepeatPackets(allTracksReliable);// if all tracks are reliable then drop repeat packets.

    //Tell the server to start playing this movie. We do want it to send RTCP SRs, but we DON'T want it to write the RTP header
    /* QTSS_Play�ص����̿�ʼ������ָ���ͻ��Ự�����������ֻ�е�����QTSS_AddRTPStream������ģ����ܵ���QTSS_Play����
	   �ڵ���QTSS_Play����֮��ģ���RTP Send Packets��ɫ�ͻᱻ���á���inPlayFlags��������ΪqtssPlaySendRTCP������
	   ʹ�������ڲ��ŵĹ������Զ�����RTCP���ͷ����档����ģ��Ḻ����������������������ķ��ͷ����档*/
    theErr = QTSS_Play(inParamBlock->inClientSession, inParamBlock->inRTSPRequest, qtssPlayFlagsSendRTCP);
    if (theErr != QTSS_NoErr)//�㲥��Ӱ������SR��
        return theErr;

	// Set fAdjustedPlayTime
    SInt64* thePlayTime = 0;
	/* ��ȡý���ļ���PLAYʱ�Ĳ���ʱ���(ms),�μ�RTPSession::Play()�е�fPlayTime */
    theErr = QTSS_GetValuePtr(inParamBlock->inClientSession, qtssCliSesPlayTimeInMsec, 0, (void**)&thePlayTime, &theLen);
    Assert(theErr == QTSS_NoErr);
    Assert(thePlayTime != NULL);
    Assert(theLen == sizeof(SInt64));
	/* set fAdjustedPlayTime(ms) */
	/* fAdjustedPlayTime����client�㲥ý���ļ�ʱ��ʱ�����RTSP Rangeͷ"Range: npt=0.00000-70.00000\r\n"�п�ʼ���ŵ�ʱ����ļ��! */
    (*theFile)->fAdjustedPlayTime = *thePlayTime - ((SInt64)((*theFile)->fStartTime * 1000));
    
	/*************** ����theSpeed,���RTSPͷ"Speed: 2.0\r\n" ************************/
    // This module supports the Speed header if the client wants the stream faster than normal.
    Float32 theSpeed = 1;
    theLen = sizeof(theSpeed);
	/* ��ȡClient��RTSP Request��ͷ"Speed: 2.0"��ֵ,֧�ֿ��/�쵹 */
    theErr = QTSS_GetValue(inParamBlock->inRTSPRequest, qtssRTSPReqSpeed, 0, &theSpeed, &theLen);
    Assert(theErr != QTSS_BadArgument);
    Assert(theErr != QTSS_NotEnoughSpace);
    
	/* �����ٶ�ֵ,��UDP���䷽ʽ,��֧�ֿ������ */
    if (theErr == QTSS_NoErr)
    {
        if (theSpeed > sMaxAllowedSpeed)
            theSpeed = sMaxAllowedSpeed;
        if ((theSpeed <= 0) || (!allTracksReliable))
            theSpeed = 1;
    }
    
	/* ����FileSession�е�Speedֵ������Speedֵ����1,��"Speed: 2.0\r\n"����RTSP��Ӧ�� */
    (*theFile)->fSpeed = theSpeed;
    if (theSpeed != 1)
    {
        // If our speed is not 1, append the RTSP speed header in the response
        char speedBuf[32];
        qtss_sprintf(speedBuf, "%10.5f", theSpeed);
        StrPtrLen speedBufPtr(speedBuf);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssSpeedHeader,speedBufPtr.Ptr, speedBufPtr.Len);
    }
    
    /********** ���RTSPͷ"x-Prebuffer: time=0.2\r\n" *****************/
    // Append x-Prebuffer header "x-Prebuffer: time=0.2" if provided & nonzero prebuffer needed
    if (theBackupTime > 0)
    {
        char prebufferBuf[32];
        qtss_sprintf(prebufferBuf, "time=%.5f", theBackupTime);
        StrPtrLen backupTimePtr(prebufferBuf);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssXPreBufferHeader,
                                    backupTimePtr.Ptr, backupTimePtr.Len);
    
    }
   
	/********** ���RTSPͷ"Range: npt=0.00000-70.00000\r\n" *****************/
	 // Record the requested stop time, if there is one ��¼RTSP Request������ֹͣʱ��
    (*theFile)->fStopTime = -1; /* note: this initialize value in  FileSession(), see above */
    theLen = sizeof((*theFile)->fStopTime);
    theErr = QTSS_GetValue(inParamBlock->inRTSPRequest, qtssRTSPReqStopTime, 0, &(*theFile)->fStopTime, &theLen);

    // add the range header "Range: npt=0.00000-70.00000\r\n".
    {
        char rangeHeader[64];
		/* ����û��ý���ļ���ֹʱ��,���ó���ʱ����� */
        if (-1 == (*theFile)->fStopTime)
           (*theFile)->fStopTime = (*theFile)->fFile.GetMovieDuration();
           
        qtss_snprintf(rangeHeader,sizeof(rangeHeader) -1, "npt=%.5f-%.5f", (*theFile)->fStartTime, (*theFile)->fStopTime);
        rangeHeader[sizeof(rangeHeader) -1] = 0;
        
        StrPtrLen rangeHeaderPtr(rangeHeader);
        (void)QTSS_AppendRTSPHeader(inParamBlock->inRTSPRequest, qtssRangeHeader,rangeHeaderPtr.Ptr, rangeHeaderPtr.Len);
    
    }
	/* �������˸������Ժ�,���������ͳ�RTSP Reponse against Client request */
    (void)QTSS_SendStandardRTSPResponse(inParamBlock->inRTSPRequest, inParamBlock->inClientSession, qtssPlayRespWriteTrackInfo);

    return QTSS_NoErr;
}

 
/* the true function to send packets, note theLastPacketTrack,theTransmitTime  */
/* two struct: QTSS_PacketStruct, QTSS_RTPSendPackets_Params  */
QTSS_Error SendPackets(QTSS_RTPSendPackets_Params* inParams)
{
	/* set quality check interval in ms */
    static const UInt32 kQualityCheckIntervalInMsec = 250;  //�������������250����

    FileSession** theFile = NULL;
    UInt32 theLen = 0;
	/* ��ȡFileSession����,�����������õ�? */
	/* (void**)&theFile should be (void**)theFile? */
    QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sFileSessionAttr, 0, (void**)&theFile, &theLen);
    Assert(theErr == QTSS_NoErr);
    Assert(theLen == sizeof(FileSession*));

	/* QTSS_WriteFlags, see QTSS.h */
	/* set write status code,�����Ƿ���Է���RTP��? */
    bool isBeginningOfWriteBurst = true;

	/* RTPStream def, void * pointer in essence */
    QTSS_Object theStream = NULL;/* RTPStream���� */

	/* GetLastPacketTrack() refer to QTRTPFile.h */
	/* very important ! �õ���һ�������ڵ�track */
    QTRTPFile::RTPTrackListEntry* theLastPacketTrack = (*theFile)->fFile.GetLastPacketTrack();

    /* ��ʼ����ѭ��send Packet */
    while (true)
    {   
		
        /* when we find that the buffer to save packet date is empty */
        if ((*theFile)->fPacketStruct.packetData == NULL)
        {
			/* refer to QTRTPFile::GetNextPacket() */
			/* theTransmitTime is very important, used by much places below ! */
			/* theTransmitTime�ǵõ���һ��packet������ʱ��(���ʱ��) */
            Float64 theTransmitTime = (*theFile)->fFile.GetNextPacket((char**)&(*theFile)->fPacketStruct.packetData, &(*theFile)->fNextPacketLen);
            //ȡ�¸�������, if errors occure in use of QTRTPFile
			if ( QTRTPFile::errNoError != (*theFile)->fFile.Error() )
            {   //�趨����ԭ��Ȼ��ϵ����ӣ�������
                QTSS_CliSesTeardownReason reason = qtssCliSesTearDownUnsupportedMedia;//�ϵ���֧�ֵ�ý��
                (void) QTSS_SetValue(inParams->inClientSession, qtssCliTeardownReason, 0, &reason, sizeof(reason));
				/* tear down */
                (void)QTSS_Teardown(inParams->inClientSession);
                return QTSS_RequestFailed;
            }
			/* �õ���һ�������ڵ�track,��Ϊ�վ��ж�ѭ�� */
            theLastPacketTrack = (*theFile)->fFile.GetLastPacketTrack();
			if (theLastPacketTrack == NULL)
				break;

			/*local var theStream see above def, in QTRTPFile::RTPTrackListEntry.Cookie1*/
			/* ȡ����һ������track���ڵ�RTPStream */
            theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
			Assert(theStream != NULL);
			if (theStream == NULL)
				return 0;


            // Check to see if we should stop playing now
            /* �����ѵ�ֹͣʱ��,�ص�ý���ļ�,������ֹʱ��,���� */
            if (((*theFile)->fStopTime != -1) && (theTransmitTime > (*theFile)->fStopTime))
            {
                // We should indeed stop playing
                (void)QTSS_Pause(inParams->inClientSession);
				/* adjust parameters, explaination see QTSS.h */
                inParams->outNextPacketTime = qtssDontCallSendPacketsAgain;

				/* ��ǰFileSession״̬��PAUSE */
                (**theFile).fPaused = true;
				/* ��ǰPAUSEʱ���,�μ�DoPlay() */
                (**theFile).fLastPauseTime = OS::Milliseconds();
  
                return QTSS_NoErr;
            }

			
			/* �����ѵ��ϸ�����track�趨��ֹ�İ���,�ص�ý���ļ�,������ֹʱ��,����,ע�����"x-Packet-Range: pn=4551-4689;url=trackID3" */
            if (((*theFile)->fStopTrackID != 0) && ((*theFile)->fStopTrackID == theLastPacketTrack->TrackID) && (theLastPacketTrack->HTCB->fCurrentPacketNumber > (*theFile)->fStopPN))
            {
                // We should indeed stop playing
                (void)QTSS_Pause(inParams->inClientSession);
				/* adjust parameters */
                inParams->outNextPacketTime = qtssDontCallSendPacketsAgain;
                (**theFile).fPaused = true;/* ��ǰFileSession״̬��PAUSE */
                (**theFile).fLastPauseTime = OS::Milliseconds();/* ��ǰPAUSEʱ���,�μ�DoPlay() */

                return QTSS_NoErr;
            }
            
            // Find out what our play speed is. Send packets out at the specified rate,
            // and do so by altering the transmit time of the packet based on the Speed rate.
			/* Next Packet������ʱ���File Session��ʼ��ʱ��Ĳ� */
            Float64 theOffsetFromStartTime = theTransmitTime - (*theFile)->fStartTime;
			/* re-adjust the transmit time of packets */
			/* ����File session�Ĳ����ٶȿ���������Next Packet������ʱ�� */
            theTransmitTime = (*theFile)->fStartTime + (theOffsetFromStartTime / (*theFile)->fSpeed);
            
            // correct for first packet xmit times that are < 0
			/* ��File�Ự������ֵ���ּ���ó����ĵ���ʱ��ʱ����ʱ����Next Packet������ʱ�� */
            if (( theTransmitTime < 0.0 ) && ( !(*theFile)->fAllowNegativeTTs ))
                theTransmitTime = 0.0;
            
			/* update the info in FileSession.QTSS_PacketStruct, see QTSS.h */
			/********************* ???????????????? ****************************/
			/* NOTE: theTransmitTime is second, not DSS system time(ms), but packetTransmitTime and fAdjustedPlayTime use ms */
            (*theFile)->fPacketStruct.packetTransmitTime = (*theFile)->fAdjustedPlayTime + ((SInt64)(theTransmitTime * 1000));

        }
        
        //We are done playing all streams!
		//�����Ѿ������������е���,����!
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
		//��������һ��RTP��Ҫ���ͳ�ȥ
		/* ����������������һ�����ϴε�packet����δ��ȫ�ͳ�������ش�����һ�־��Ǵ���һ���� */
        Assert(theLastPacketTrack != NULL);

        //If the stream is video, we need to make sure that QTRTPFile knows what quality level we're at
        /* if able to thin and exceeds quality check time, set the theLastPacketTrack quality level */
		/* ��������Ƶ��,�ܴ�,���ѵ�Quality checkʱ��,Ҫ��ȷ��ǰ��quality level */
        if ( (!sDisableThinning) && (inParams->inCurrentTime > ((*theFile)->fLastQualityCheck + kQualityCheckIntervalInMsec) ) )
        {
			/* see QTRTPFile.RTPTrackListEntry.Cookie2 */
			/* �����һ��track����audio����video? */
            QTSS_RTPPayloadType thePayloadType = (QTSS_RTPPayloadType)theLastPacketTrack->Cookie2;
			/* ��������Ƶ��,�������quality level,����Ϊ��FileSession��quality level */
            if (thePayloadType == qtssVideoPayloadType)
            {
				/* �����ϴ�LastQualityCheckʱ�� */
                (*theFile)->fLastQualityCheck = inParams->inCurrentTime;
                /* ��ȡ�ϸ�track���ڵ�RTPStream */
				theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
				Assert(theStream != NULL);
				if (theStream == NULL)
					return 0;


                // Get the current quality level in the stream, and this stream's TrackID.
                UInt32* theQualityLevel = 0;
				/* ����ϸ�track����Quality level,�������òμ�DoPlay() */
                theErr = QTSS_GetValuePtr(theStream, qtssRTPStrQualityLevel, 0, (void**)&theQualityLevel, &theLen);
                Assert(theErr == QTSS_NoErr);
                Assert(theQualityLevel != NULL);
                Assert(theLen == sizeof(UInt32));
        
				/* see QTRTPFile::SetTrackQualityLevel() */
				/* use *theQualityLevel obtained above to set current track quality level */
                (*theFile)->fFile.SetTrackQualityLevel(theLastPacketTrack, *theQualityLevel);
            }
        }

        // ������RTP��ǰ��׼��!
		/* QTSS_WriteFlags,qtssWriteFlagsWriteBurstBegin,qtssWriteFlagsIsRTP see QTSS.h */
		/* ����д��־(�Ǵ�������RTP��) */
        QTSS_WriteFlags theFlags = qtssWriteFlagsIsRTP;
		/* write status code, see above*/
        if (isBeginningOfWriteBurst)
            theFlags |= qtssWriteFlagsWriteBurstBegin; /* means now begin to write */

		/* �õ���һ��RTP�����ڵ�RTPStream����,����Ϊ��,ֱ�ӷ��� */
        theStream = (QTSS_Object)theLastPacketTrack->Cookie1;
		Assert(theStream != NULL);
		if (theStream == NULL)
			return 0;

        //adjust the timestamp so it reflects paused time.
		/* see FileSession.QTSS_PacketStruct.packatData */
		/* these quantities will use in block-dealing, see below */
		/* ȡ�õ�ǰRTP�������� */
        void* packetDataPtr =  (*theFile)->fPacketStruct.packetData;
		/* get timestamp field from given packet, GetPacketTimeStamp see above  */
        UInt32 currentTimeStamp = GetPacketTimeStamp(packetDataPtr);
		/* obtain the absolute time to pause,SetPausetimeTimeStamp see above */ 
		/* use *theFile to obtain fTotalPauseTime, theStream to obtain timescale */
		/* in general, pauseTimeStamp locate behind currentTimeStamp */
        UInt32 pauseTimeStamp = SetPausetimeTimeStamp(*theFile, theStream, currentTimeStamp); 
		/* get the sequence number from the given packet in RTP stream */
  		UInt16 curSeqNum = GetPacketSequenceNumber(theStream);
		//�õ�ǰ����seqnum��ʽ���ø�����һ��Ҫ����LastPacketSeqNumAttr ��ֵ���Ե�����ֵ
        (void) QTSS_SetValue(theStream, sRTPStreamLastPacketSeqNumAttrID, 0, &curSeqNum, sizeof(curSeqNum));

		// ����һ��RTP��!
		/* write rtp packet from buffer &(*theFile)->fPacketStruct into given stream theStream with specified length bytes NULL */
		// QTSS_Write�ص����̽�һ���������е�����д�뵽һ��RTPStream����,��־��qtssWriteFlagsIsRTP!qtssWriteFlagsWriteBurstBegin��
		theErr = QTSS_Write(theStream, &(*theFile)->fPacketStruct, (*theFile)->fNextPacketLen, NULL, theFlags);
        
		/* change write status code right now after a writing into RTP stream */
        isBeginningOfWriteBurst = false;
        
		/* handle the probable results of QTSS_Write */
		/* deal with blocking:�����RTP������,�����ú�QTSS_PacketStruct,�ر����´η��͸�RTP����Ҫ�ȴ���ʱ�� */
        if ( theErr == QTSS_WouldBlock )
        { 

			/* the two local vars see above def, here means fTotalPauseTime>0 */
			/* ���統ǰʱ��������ж�ʱ���,������Ϊ��RTP����ʱ��� */
            if (currentTimeStamp != pauseTimeStamp) // reset the packet time stamp so we adjust it again when we really do send it
               SetPacketTimeStamp(currentTimeStamp, packetDataPtr);
           
            //
            // In the case of a QTSS_WouldBlock error, the packetTransmitTime field of the packet struct will be set to
            // the time to wakeup, or -1 if not known.
            // If the time to wakeup is not given by the server, just give a fixed guess interval
			/* ����suggestedWakeupTimeû��ָ��,�ͽ�outNextPacketTime��Ϊһ������̽������ */
            if ((*theFile)->fPacketStruct.suggestedWakeupTime == -1)
				/* after default 10 ms continue to send packet */
                inParams->outNextPacketTime = sFlowControlProbeInterval;    //10ms for buffering, try me again in # MSec
            else  /* if suggestedWakeupTime has already be set */
            {   
				/* make sure of suggestedWakeupTime locates behind current time */
                Assert((*theFile)->fPacketStruct.suggestedWakeupTime > inParams->inCurrentTime);
				/* calculate the relative interval to wakeup,������һ�����͵ĵȴ�ʱ�� */
                inParams->outNextPacketTime = (*theFile)->fPacketStruct.suggestedWakeupTime - inParams->inCurrentTime;
            }
            
            //qtss_printf("Call again: %qd\n", inParams->outNextPacketTime);
                
            return QTSS_NoErr;
        }
        else /* if no blocking, this easy! */
        {
          //���ͳ���RTP����seqnum��ʽ���ø�����һ���ͳ���LastSentPacketSeqNumAttr ��ֵ���Ե�����ֵ
          (void) QTSS_SetValue(theStream, sRTPStreamLastSentPacketSeqNumAttrID, 0, &curSeqNum, sizeof(curSeqNum));
		  /* reset the buffer to save packet data and prepare to send packet next time */
          (*theFile)->fPacketStruct.packetData = NULL;
        }
    }
    
    return QTSS_NoErr;
}

/* ��ȡFileSessionʵ������,���л�ȡskip����������,����������е�RTPSession�е�����������qtssCliSesFramesSkipped,���ɾ��FileSessionʵ������ */
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
	/* ��������е�RTPSession�е�����������qtssCliSesFramesSkipped */
    UInt32 theNumSkippedSamples = (*theFile)->fFile.GetNumSkippedSamples();
    (void)QTSS_SetValue(inParams->inClientSession, qtssCliSesFramesSkipped, 0, &theNumSkippedSamples, sizeof(theNumSkippedSamples));
    
	/* ɾ��FileSession����ʵ�� */
    DeleteFileSession(*theFile);
    return QTSS_NoErr;
}

void    DeleteFileSession(FileSession* inFileSession)
{   
    delete inFileSession;
}
