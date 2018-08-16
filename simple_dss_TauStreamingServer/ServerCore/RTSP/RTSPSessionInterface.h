
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
	/* 设置是否基于base64解码? */
    static void     SetBase64Decoding(Bool16 newVal) { sDoBase64Decoding = newVal; }
    
    RTSPSessionInterface();
    virtual ~RTSPSessionInterface();
    
    //Is this session alive? If this returns false, clean up and begone as fast as possible
    Bool16 IsLiveSession()      { return fSocket.IsConnected() && fLiveSession; }
    
    // Allows clients to refresh the timeout,这是S/C keep alive的方法
    void RefreshTimeout()       { fTimeoutTask.RefreshTimeout(); }

    // In order to facilitate sending out of band data(带外数据) on the RTSP connection,
    // other objects need to have direct pointer access to this object. But,
    // because this object is a task object it can go away at any time. If #(总数) of
    // object holders is > 0, the RTSPSession will NEVER go away. However,
    // the object managing the session should be aware that if IsLiveSession returns
    // false it may be wise to relinquish(放弃) control of the session
    void IncrementObjectHolderCount() { (void)atomic_add(&fObjectHolders, 1); }/* 增加一个对象计数 */
    void DecrementObjectHolderCount();
    
    // If RTP data is interleaved into the RTSP connection, we need to associate
    // 2 unique channel numbers with each RTP stream, one for RTP and one for RTCP.
    // This function allocates 2 channel numbers, returns the lower one. The other one
    // is implicitly 1 greater.
    // Pass in the RTSP Session ID of the Client session to which these channel numbers will belong.
	/* 为指定的RTSPSession分配一对Channel Number,一个for RTP,另一个for RTCP,返回的是RTP Channel Number(RTCP Channel Number默认大1) */
    UInt8               GetTwoChannelNumbers(StrPtrLen* inRTSPSessionID);

    // Given a channel number, returns the RTSP Session ID to which this channel number refers
	/* 从Channel Number获取相应的RTSPSession ID */
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
    
    // Allows non-buffered writes to the client. These will flow control(流控).
    
    // THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
    virtual QTSS_Error WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten);
    virtual QTSS_Error Write(void* inBuffer, UInt32 inLength, UInt32* outLenWritten, UInt32 inFlags);
    virtual QTSS_Error Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead);
    virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask);

    // performs RTP over RTSP
	/* 利用交替写RTP/RTCP数据进RTPStream */
    QTSS_Error  InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel);

	// OPTIONS request
	void		SaveOutputStream();
	void		RevertOutputStream();
	void		ResetOutputStream() { fOutputStream.Reset(); fOutputStream.ResetBytesWritten();}
	void		SendOptionsRequest();
	Bool16		SentOptionsRequest() { return fSentOptionsRequest; }/* 发送OPTIONS请求吗? */
	SInt32		RoundTripTime() { return fRoundTripTime; }
	

	/*************** 下面是数据成员 ***************************/
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
    /* 每个RTSP会话都有一个唯一编号识别它 */
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
    // for coalescing(拼合) small interleaved writes into a single TCP frame,参见struct RTPInterleaveHeader定义
    enum
    {
          kTCPCoalesceBufferSize = 1450 //1450 is the max data space in an TCP segment over ent
        , kTCPCoalesceDirectWriteSize = 0 // if > this # bytes, bypass(绕开拼合) coalescing and make a direct write
        , kInteleaveHeaderSize = 4  //RTPInterleaveHeader size: '$ '+ 1 byte ch ID + 2 bytes length
    };
    char*       fTCPCoalesceBuffer;
	/* fTCPCoalesceBuffer的长度,参见RTSPSessionInterface::InterleavedWrite() */
    SInt32      fNumInCoalesceBuffer;


    //+rt  socket we get from "accept()",参见TCPListenerSocket::ProcessEvent()
    TCPSocket           fSocket; //得到服务器端的TCPSocket
    TCPSocket*          fOutputSocketP; //下面这两个TCPSocket指针,都指向&fSocket,参见RTSPSessionInterface::RTSPSessionInterface()
    TCPSocket*          fInputSocketP;  // <-- usually same as fSocketP, unless we're HTTP Proxying
    
    void        SnarfInputSocket( RTSPSessionInterface* fromRTSPSession );
    
    // What session type are we? RTSPSession会话类型
	// Is this a normal RTSP session or an RTSP / HTTP session?
	//Is this a normal RTSP session, or is it a HTTP tunnelled RTSP session?
    QTSS_RTSPSessionType    fSessionType;

	/* is live Session?是活跃的会话吗? 服务器和client如何保活? */
    Bool16              fLiveSession;
	/* 这是第几个RTSPSession实例对象,该量能够控制RTSPSession的实例对象持有个数,使之不超过最大连接数 */
    unsigned int        fObjectHolders;
	/* session ID 计数器,从1开始计数,能对所有的RTSPSession对象编号 */
	static unsigned int sSessionIDCounter;

	/* 当前的通道号 */
    UInt8               fCurChannelNum;
	/* Channel Number到RTSPSession ID的映射:索引是ChannelNumber,分量值是RTSPSession ID */
    StrPtrLen*          fChNumToSessIDMap;  
    
	// A QTSS_StreamRef used for sending data to the RTSP client
    QTSS_StreamRef      fStreamRef;
    
	/* RTSPRequest body len,RTSP请求的数据长度 */
    SInt32              fRequestBodyLen;
    
	/* S/C ip&port */
	UInt32              fLocalAddr;
	UInt32              fRemoteAddr;
    UInt16              fLocalPort;
    UInt16              fRemotePort;
    
	
	/* 临时存放fOutputStream的缓存,参见RTSPSessionInterface::SaveOutputStream(),为何需要它?备份,就是为了在"x-Dynamic-Rate"头后插入";rtt=**"字段,其他都不变 */
	StrPtrLen				fOldOutputStreamBuffer;

	// For OPTIONS request, see RTSPSessionInterface::SendOptionsRequest()
	/* 是否发送了OptionsRequest? */
	Bool16					fSentOptionsRequest;
	/* 发送OptionsRequest的时间戳 */
	SInt64					fOptionsRequestSendTime;
	/* 注意基于4字节对齐,指定长度的整数数组,used in RTSPSessionInterface::SendOptionsRequest() */
	static 	UInt32			sOptionsRequestBody[kMaxRandomDataSize / sizeof(UInt32)];//1024*64
	
	/* 通过发送"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n"和接收Options Response来计算往返时间RTT */
	SInt32					fRoundTripTime;
	/* 计算往返时间RTT吗? */
	Bool16					fRoundTripTimeCalculation;
	

	/* base64 解码吗?(true) */
    static Bool16           sDoBase64Decoding;
    

	
    //Dictionary support
    
    // Param retrieval function,use in RTSPSession attr Dictionary
    static void*        SetupParams(QTSSDictionary* inSession, UInt32* outLen);
    
	/* RTSPSession Dictionary attributes */
    static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __RTSPSESSIONINTERFACE_H__

