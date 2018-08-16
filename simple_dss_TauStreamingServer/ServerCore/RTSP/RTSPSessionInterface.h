
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPSessionInterface.h
Description: Presents an API for session-wide resources for modules to use.
             Implements the RTSP Session dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#ifndef __RTSPSESSIONINTERFACE_H__
#define __RTSPSESSIONINTERFACE_H__

#include "RTSPRequestStream.h"
#include "RTSPResponseStream.h"
#include "Task.h"
#include "QTSS.h"
#include "QTSSDictionary.h"
#include "atomic.h"

class RTSPSessionInterface : public QTSSDictionary, public Task
{
public:

    //Initialize must be called right off the bat to initialize dictionary resources
    static void     Initialize();
	/* �����Ƿ����base64����? */
    static void     SetBase64Decoding(Bool16 newVal) { sDoBase64Decoding = newVal; }
    
    RTSPSessionInterface();
    virtual ~RTSPSessionInterface();
    
    //Is this session alive? If this returns false, clean up and begone as fast as possible
    Bool16 IsLiveSession()      { return fSocket.IsConnected() && fLiveSession; }
    
    // Allows clients to refresh the timeout,����S/C keep alive�ķ���
    void RefreshTimeout()       { fTimeoutTask.RefreshTimeout(); }

    // In order to facilitate sending out of band data(��������) on the RTSP connection,
    // other objects need to have direct pointer access to this object. But,
    // because this object is a task object it can go away at any time. If #(����) of
    // object holders is > 0, the RTSPSession will NEVER go away. However,
    // the object managing the session should be aware that if IsLiveSession returns
    // false it may be wise to relinquish(����) control of the session
    void IncrementObjectHolderCount() { (void)atomic_add(&fObjectHolders, 1); }/* ����һ��������� */
    void DecrementObjectHolderCount();
    
    // If RTP data is interleaved into the RTSP connection, we need to associate
    // 2 unique channel numbers with each RTP stream, one for RTP and one for RTCP.
    // This function allocates 2 channel numbers, returns the lower one. The other one
    // is implicitly 1 greater.
    // Pass in the RTSP Session ID of the Client session to which these channel numbers will belong.
	/* Ϊָ����RTSPSession����һ��Channel Number,һ��for RTP,��һ��for RTCP,���ص���RTP Channel Number(RTCP Channel NumberĬ�ϴ�1) */
    UInt8               GetTwoChannelNumbers(StrPtrLen* inRTSPSessionID);

    // Given a channel number, returns the RTSP Session ID to which this channel number refers
	/* ��Channel Number��ȡ��Ӧ��RTSPSession ID */
    StrPtrLen*          GetSessionIDForChannelNum(UInt8 inChannelNum);
    
    //Two main things are persistent through the course of a session, not
    //associated with any one request. The RequestStream (which can be used for
    //getting data from the client), and the socket. OOps, and the ResponseStream
    RTSPRequestStream*  GetInputStream()    { return &fInputStream; }
    RTSPResponseStream* GetOutputStream()   { return &fOutputStream; }
    TCPSocket*          GetSocket()         { return &fSocket; }
    OSMutex*            GetSessionMutex()   { return &fSessionMutex; }
    
    UInt32              GetSessionID()      { return fSessionID; }
    
    // RTSP Request Body Length
    // This object can enforce a length of the request body to prevent callers
    // of Read() from overrunning the request body and going into the next request.
    // -1 is an unknown request body length. If the body length is unknown,
    // this object will do no length enforcement.
	/* used in RTSPRequest::ParseHeaders() */
    void                SetRequestBodyLength(SInt32 inLength)   { fRequestBodyLen = inLength; }
    SInt32              GetRemainingReqBodyLen()                { return fRequestBodyLen; }
    
    // QTSS STREAM FUNCTIONS
    
    // Allows non-buffered writes to the client. These will flow control(����).
    
    // THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
    virtual QTSS_Error WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten);
    virtual QTSS_Error Write(void* inBuffer, UInt32 inLength, UInt32* outLenWritten, UInt32 inFlags);
    virtual QTSS_Error Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead);
    virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask);

    // performs RTP over RTSP
	/* ���ý���дRTP/RTCP���ݽ�RTPStream */
    QTSS_Error  InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel);

	// OPTIONS request
	void		SaveOutputStream();
	void		RevertOutputStream();
	void		ResetOutputStream() { fOutputStream.Reset(); fOutputStream.ResetBytesWritten();}
	void		SendOptionsRequest();
	Bool16		SentOptionsRequest() { return fSentOptionsRequest; }/* ����OPTIONS������? */
	SInt32		RoundTripTime() { return fRoundTripTime; }
	

	/*************** ���������ݳ�Ա ***************************/
    enum
    {
        kMaxUserNameLen         = 32,
        kMaxUserPasswordLen     = 32,
        kMaxUserRealmLen        = 64
    };

    enum                        // Quality of protection(Qop)
    {
        kNoQop          = 0,    // No Quality of protection
        kAuthQop        = 1,    // Authentication
        kAuthIntQop     = 2     // Authentication with Integrity        
    };
        
	// DJM PROTOTYPE
	enum
    {
        kMaxRandomDataSize		= 256 * 1024,
    };
        
protected:
	/* used in sSessionIDCounter */
    enum
    {
        kFirstRTSPSessionID     = 1,    //UInt32
    };

    //Each RTSP session has a unique number that identifies it.
    /* ÿ��RTSP�Ự����һ��Ψһ���ʶ���� */
	UInt32              fSessionID;

    char                fUserNameBuf[kMaxUserNameLen];
    char                fUserPasswordBuf[kMaxUserPasswordLen];
    char                fUserRealmBuf[kMaxUserRealmLen];

    TimeoutTask         fTimeoutTask;//allows the session to be timed out
    
	/******************* two important classes **************************/
	//The RequestStream (which can be used for getting data from the client)
    RTSPRequestStream   fInputStream;
    RTSPResponseStream  fOutputStream;
	/******************* two important classes **************************/
    
    // Any RTP session sending interleaved data on this RTSP session must
    // be prevented from writing while an RTSP request is in progress
    OSMutex             fSessionMutex;
    
	// used in RTSPSessionInterface::InterleavedWrite()
    // for coalescing(ƴ��) small interleaved writes into a single TCP frame,�μ�struct RTPInterleaveHeader����
    enum
    {
          kTCPCoalesceBufferSize = 1450 //1450 is the max data space in an TCP segment over ent
        , kTCPCoalesceDirectWriteSize = 0 // if > this # bytes, bypass(�ƿ�ƴ��) coalescing and make a direct write
        , kInteleaveHeaderSize = 4  //RTPInterleaveHeader size: '$ '+ 1 byte ch ID + 2 bytes length
    };
    char*       fTCPCoalesceBuffer;
	/* fTCPCoalesceBuffer�ĳ���,�μ�RTSPSessionInterface::InterleavedWrite() */
    SInt32      fNumInCoalesceBuffer;


    //+rt  socket we get from "accept()",�μ�TCPListenerSocket::ProcessEvent()
    TCPSocket           fSocket; //�õ��������˵�TCPSocket
    TCPSocket*          fOutputSocketP; //����������TCPSocketָ��,��ָ��&fSocket,�μ�RTSPSessionInterface::RTSPSessionInterface()
    TCPSocket*          fInputSocketP;  // <-- usually same as fSocketP, unless we're HTTP Proxying
    
    void        SnarfInputSocket( RTSPSessionInterface* fromRTSPSession );
    
    // What session type are we? RTSPSession�Ự����
	// Is this a normal RTSP session or an RTSP / HTTP session?
	//Is this a normal RTSP session, or is it a HTTP tunnelled RTSP session?
    QTSS_RTSPSessionType    fSessionType;

	/* is live Session?�ǻ�Ծ�ĻỰ��? ��������client��α���? */
    Bool16              fLiveSession;
	/* ���ǵڼ���RTSPSessionʵ������,�����ܹ�����RTSPSession��ʵ��������и���,ʹ֮��������������� */
    unsigned int        fObjectHolders;
	/* session ID ������,��1��ʼ����,�ܶ����е�RTSPSession������ */
	static unsigned int sSessionIDCounter;

	/* ��ǰ��ͨ���� */
    UInt8               fCurChannelNum;
	/* Channel Number��RTSPSession ID��ӳ��:������ChannelNumber,����ֵ��RTSPSession ID */
    StrPtrLen*          fChNumToSessIDMap;  
    
	// A QTSS_StreamRef used for sending data to the RTSP client
    QTSS_StreamRef      fStreamRef;
    
	/* RTSPRequest body len,RTSP��������ݳ��� */
    SInt32              fRequestBodyLen;
    
	/* S/C ip&port */
	UInt32              fLocalAddr;
	UInt32              fRemoteAddr;
    UInt16              fLocalPort;
    UInt16              fRemotePort;
    
	
	/* ��ʱ���fOutputStream�Ļ���,�μ�RTSPSessionInterface::SaveOutputStream(),Ϊ����Ҫ��?����,����Ϊ����"x-Dynamic-Rate"ͷ�����";rtt=**"�ֶ�,���������� */
	StrPtrLen				fOldOutputStreamBuffer;

	// For OPTIONS request, see RTSPSessionInterface::SendOptionsRequest()
	/* �Ƿ�����OptionsRequest? */
	Bool16					fSentOptionsRequest;
	/* ����OptionsRequest��ʱ��� */
	SInt64					fOptionsRequestSendTime;
	/* ע�����4�ֽڶ���,ָ�����ȵ���������,used in RTSPSessionInterface::SendOptionsRequest() */
	static 	UInt32			sOptionsRequestBody[kMaxRandomDataSize / sizeof(UInt32)];//1024*64
	
	/* ͨ������"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"�ͽ���Options Response����������ʱ��RTT */
	SInt32					fRoundTripTime;
	/* ��������ʱ��RTT��? */
	Bool16					fRoundTripTimeCalculation;
	

	/* base64 ������?(true) */
    static Bool16           sDoBase64Decoding;
    

	
    //Dictionary support
    
    // Param retrieval function,use in RTSPSession attr Dictionary
    static void*        SetupParams(QTSSDictionary* inSession, UInt32* outLen);
    
	/* RTSPSession Dictionary attributes */
    static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __RTSPSESSIONINTERFACE_H__

