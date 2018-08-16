
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequestInterface.h
Description: Provides a simple API for modules to access request information and
             manipulate (and possibly send) the client response.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#ifndef __RTSPREQUESTINTERFACE_H__
#define __RTSPREQUESTINTERFACE_H__

#include "QTSS.h"
#include "QTSSDictionary.h"
#include "QTSSMessages.h"
#include "QTSSUserProfile.h"

#include "StrPtrLen.h"
#include "RTSPSessionInterface.h"
#include "RTSPResponseStream.h"
#include "RTSPProtocol.h"


class RTSPRequestInterface : public QTSSDictionary
{
    public:

        //Initialize
        //Call initialize before instantiating this class. For maximum performance, this class builds
        //any response header it can at startup time.
        static void         Initialize();
        
        //CONSTRUCTOR:
        RTSPRequestInterface(RTSPSessionInterface *session);
        virtual ~RTSPRequestInterface()
            { if (fMovieFolderPtr != &fMovieFolderPath[0]) delete [] fMovieFolderPtr; }
        
        //FUNCTIONS FOR SENDING OUTPUT:
        
        //Adds a new header to this object's list of headers to be sent out.
        //Note that this is only needed for "special purpose" headers. The Server,
        //CSeq, SessionID, and Connection headers are taken care of automatically
        void    AppendHeader(QTSS_RTSPHeader inHeader, StrPtrLen* inValue);

        
        // The transport header constructed by this function mimics the one sent
        // by the client, with the addition of server port & interleaved sub headers
        void    AppendTransportHeader(StrPtrLen* serverPortA,
                                        StrPtrLen* serverPortB,
                                        StrPtrLen* channelA,
                                        StrPtrLen* channelB,
                                        StrPtrLen* serverIPAddr = NULL,
                                        StrPtrLen* ssrc = NULL);
        void    AppendContentBaseHeader(StrPtrLen* theURL);
        void    AppendRTPInfoHeader(QTSS_RTSPHeader inHeader,
                                    StrPtrLen* url, StrPtrLen* seqNumber,
                                    StrPtrLen* ssrc, StrPtrLen* rtpTime, Bool16 lastRTPInfo);

        void    AppendContentLength(UInt32 contentLength);
        void    AppendDateAndExpires();
        void    AppendSessionHeaderWithTimeout( StrPtrLen* inSessionID, StrPtrLen* inTimeout );
        void    AppendRetransmitHeader(UInt32 inAckTimeout);

        // MODIFIERS
        /* 设置是否保持此连接? */
        void SetKeepAlive(Bool16 newVal)                { fResponseKeepAlive = newVal; }
        
        //SendHeader:
        //Sends the RTSP headers, in their current state, to the client.
        void SendHeader();
        
        // QTSS STREAM FUNCTIONS
        
        // THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
        virtual QTSS_Error WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten);
        
        //Write
        //A "buffered send" that can be used for sending small chunks of data at a time.
        virtual QTSS_Error Write(void* inBuffer, UInt32 inLength, UInt32* outLenWritten, UInt32 inFlags);
        
        // Flushes all currently buffered data to the network. This either returns
        // QTSS_NoErr or EWOULDBLOCK. If it returns EWOULDBLOCK, you should wait for
        // a EV_WR on the socket, and call flush again.
        virtual QTSS_Error  Flush() { return fOutputStream->Flush(); }

        // Reads data off the stream. Same behavior as calling RTSPSessionInterface::Read
        virtual QTSS_Error Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead)
            { return fSession->Read(ioBuffer, inLength, outLenRead); }
            
        // Requests an event. Same behavior as calling RTSPSessionInterface::RequestEvent
        virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask)
            { return fSession->RequestEvent(inEventMask); }
        
        
        //ACCESS FUNCTIONS:
        
        // These functions are shortcuts that objects internal to the server
        // use to get access to RTSP request information. Pretty much all
        // of this stuff is also available as QTSS API attributes.
        
        QTSS_RTSPMethod             GetMethod() const       { return fMethod; }
        QTSS_RTSPStatusCode         GetStatus() const       { return fStatus; }
        Bool16                      GetResponseKeepAlive() const { return fResponseKeepAlive; }
        void                        SetResponseKeepAlive(Bool16 keepAlive)  { fResponseKeepAlive = keepAlive; }
        
        //will be -1 unless there was a Range header. May have one or two values
        Float64                     GetStartTime()      { return fStartTime; }
        Float64                     GetStopTime()       { return fStopTime; }
        
        //
        // Value of Speed: header in request
        Float32                     GetSpeed()          { return fSpeed; }
        
        //
        // Value of late-tolerance field of x-RTP-Options header
        Float32                     GetLateToleranceInSec(){ return fLateTolerance; }
        StrPtrLen*                  GetLateToleranceStr(){ return &fLateToleranceStr; }
        
        // these get set if there is a transport header
        UInt16                      GetClientPortA()    { return fClientPortA; }
        UInt16                      GetClientPortB()    { return fClientPortB; }
        UInt32                      GetDestAddr()       { return fDestinationAddr; }
        UInt32                      GetSourceAddr()     { return fSourceAddr; }
        UInt16                      GetTtl()            { return fTtl; }
        QTSS_RTPTransportType       GetTransportType()  { return fTransportType; }
        QTSS_RTPNetworkMode         GetNetworkMode()    { return fNetworkMode; }
        UInt32                      GetWindowSize()     { return fWindowSize; }
        
            
        Bool16                      HasResponseBeenSent()
                                        { return fOutputStream->GetBytesWritten() > 0; }
            
        RTSPSessionInterface*       GetSession()         { return fSession; }
        QTSSDictionary*             GetHeaderDictionary(){ return &fHeaderDictionary; }
        
        Bool16                      GetAllowed()                { return fAllowed; }
        void                        SetAllowed(Bool16 allowed)  { fAllowed = allowed;}
        
        QTSS_ActionFlags            GetAction()             { return fAction; }
        void                        SetAction(QTSS_ActionFlags action)  { fAction = action;}
  
		/* 是Setup中的Record mode吗? */
		Bool16						IsPushRequest()				{ return (fTransportMode == qtssRTPTransportModeRecord) ? true : false; }
        UInt16                      GetSetUpServerPort()        { return fSetUpServerPort;}
        QTSS_RTPTransportMode       GetTransportMode()          { return fTransportMode; }
        
        QTSS_AuthScheme             GetAuthScheme()             {  return fAuthScheme; }
        void                        SetAuthScheme(QTSS_AuthScheme scheme)   { fAuthScheme = scheme;}
        StrPtrLen*                  GetAuthRealm()              { return &fAuthRealm; }
        StrPtrLen*                  GetAuthNonce()              { return &fAuthNonce; }
        StrPtrLen*                  GetAuthUri()                { return &fAuthUri; }
        UInt32                      GetAuthQop()                { return fAuthQop; }
        StrPtrLen*                  GetAuthNonceCount()         { return &fAuthNonceCount; }
        StrPtrLen*                  GetAuthCNonce()             { return &fAuthCNonce; }
        StrPtrLen*                  GetAuthResponse()           { return &fAuthResponse; }                          
        StrPtrLen*                  GetAuthOpaque()             { return &fAuthOpaque; }
        QTSSUserProfile*            GetUserProfile()            { return fUserProfilePtr; }
        
        Bool16                      GetStale()                  { return fStale; }
        void                        SetStale(Bool16 stale)      { fStale = stale; }
        
        Bool16                      SkipAuthorization()         {  return fSkipAuthorization; }

		SInt32                      GetDynamicRateState()       { return fEnableDynamicRateState; }
        
		// DJM PROTOTYPE
		UInt32						GetRandomDataSize()			{ return fRandomDataSize; }
        
    protected:

        //ALL THIS STUFF HERE IS SETUP BY RTSPRequest object (derived)
        
        //REQUEST HEADER DATA
        enum
        {
            kMovieFolderBufSizeInBytes = 256,   //Uint32
            kMaxFilePathSizeInBytes = 256       //Uint32
        };
        
        QTSS_RTSPMethod             fMethod;            //Method of this request
        QTSS_RTSPStatusCode         fStatus;            //Current Response status of this request
        UInt32                      fRealStatusCode;    //Current RTSP status num of this request
        Bool16                      fRequestKeepAlive;  //Does the client want keep-alive?保持心跳吗?(默认true)
        Bool16                      fResponseKeepAlive; //Are we going to keep-alive?
        RTSPProtocol::RTSPVersion   fVersion;           /* RTSP Protocol version,0/1 */

		/* used in RTSPRequest::ParseRangeHeader() */
        Float64                     fStartTime;         //Range header info: start time
        Float64                     fStopTime;          //Range header info: stop time

        UInt16                      fClientPortA;       //This is all info that comes out
        UInt16                      fClientPortB;       //of the Transport: header
        
		/* used in RTSPRequest::ParseTimeToLiveSubHeader() */
		UInt16                      fTtl;

		/* used in RTSPRequest::ParseAddrSubHeader() */
        UInt32                      fDestinationAddr;
        UInt32                      fSourceAddr;

		/* used in RTSPRequest::ParseRetransmitHeader() */
        QTSS_RTPTransportType       fTransportType;
		/* used in RTSPRequest::ParseNetworkModeSubHeader() */
        QTSS_RTPNetworkMode         fNetworkMode;
    
		/* used in RTSPRequest::ParseContentLengthHeader() */
        UInt32                      fContentLength;
        /* used in RTSPRequest::ParseIfModSinceHeader() */
        SInt64                      fIfModSinceDate;
		/* used in RTSPRequest::ParseSpeedHeader() */
        Float32                     fSpeed;
		/* used in RTSPRequest::ParseTransportOptionsHeader() */
        Float32                     fLateTolerance;
        StrPtrLen                   fLateToleranceStr;
		/* used in RTSPRequest::ParsePrebufferHeader(),该PrebufferHeader头域对应的值 */
        Float32                     fPrebufferAmt;
        
		/* used in RTSPRequest::ParseTransportHeader(),client发送Server的众多个transport header(用','分隔)中的第一个transport */
        StrPtrLen                   fFirstTransport;
        
		/* QTSSStream父类结构 */
		//A QTSS_StreamRef for sending data to the RTSP client.
        QTSS_StreamRef              fStreamRef;
        
        //
        // For reliable UDP
		/* OverbufferWindow size,used in RTSPRequest::ParseRetransmitHeader() */
        UInt32                      fWindowSize;
        StrPtrLen                   fWindowSizeStr;

        //Because of URL decoding issues, we need to make a copy of the file path.
        //Here is a buffer for it.
		/* 因为qtssRTSPReqURI的数据是encoded的,现在decode需要额外的缓存存放数据,参见RTSPRequest::ParseURI() */
        char                        fFilePath[kMaxFilePathSizeInBytes];
        char                        fMovieFolderPath[kMovieFolderBufSizeInBytes];
        char*                       fMovieFolderPtr;
        
		/* RTSPHeader字典,参见构造函数,RTSPRequest::ParseURI(),RTSPSession::Run()/SetupClientSessionAttrs() */
        QTSSDictionary              fHeaderDictionary;
        
		/* 允许Authorize吗?参见RTSPSession::Run()中的case kAuthorizingRequest */
        Bool16                      fAllowed;
		/* used in RTSPRequest::ParseModeSubHeader() */
        QTSS_RTPTransportMode       fTransportMode;
		/* used in RTPStream::AppendTransport()/SendSetupResponse() */
        UInt16                      fSetUpServerPort; //send this back as the server_port if is SETUP request
    
		/* R/W */
        QTSS_ActionFlags            fAction;    // The action that will be performed for this request
                                                // Set to a combination of QTSS_ActionFlags 
        
		/* 有关authorization */
        QTSS_AuthScheme             fAuthScheme;/* 认证格式 */
        StrPtrLen                   fAuthRealm;
        StrPtrLen                   fAuthNonce;
        StrPtrLen                   fAuthUri;
        UInt32                      fAuthQop;
        StrPtrLen                   fAuthNonceCount;
        StrPtrLen                   fAuthCNonce;
        StrPtrLen                   fAuthResponse;                          
        StrPtrLen                   fAuthOpaque;

        QTSSUserProfile             fUserProfile;
        QTSSUserProfile*            fUserProfilePtr;
        Bool16                      fStale;/* 认证是否过期? */
        
        Bool16                      fSkipAuthorization;

		/* used in RTSPRequest::ParseDynamicRateHeader(),能否开启Dynamic Rate? */
		SInt32                      fEnableDynamicRateState;
        
		// DJM PROTOTYPE
		/* used in RTSPRequest::ParseRandomDataSizeHeader() */
		UInt32						fRandomDataSize;
        
    private:

		/****** 注意这两个类超级重要! *************/
        RTSPSessionInterface*   fSession;
        RTSPResponseStream*     fOutputStream;
        /******** 注意这两个类超级重要!**********/

        enum
        {
            kStaticHeaderSizeInBytes = 512  //UInt32
        };
        
		/* 写了标准RSTP Headers了吗? used in RTSPRequestInterface::WriteStandardHeaders() */
        Bool16                  fStandardHeadersWritten;
        
        void                    PutTransportStripped(StrPtrLen &outFirstTransport, StrPtrLen &outResultStr);
        void                    WriteStandardHeaders();
        static void             PutStatusLine(  StringFormatter* putStream,
                                                QTSS_RTSPStatusCode status,
                                                RTSPProtocol::RTSPVersion version);

        //Individual param retrieval functions
		/* 单个参数检索函数,RTSPRequestInterface::sAttributes[] */
        static void*        GetAbsTruncatedPath(QTSSDictionary* inRequest, UInt32* outLen);
        static void*        GetTruncatedPath(QTSSDictionary* inRequest, UInt32* outLen);
        static void*        GetFileName(QTSSDictionary* inRequest, UInt32* outLen);
        static void*        GetFileDigit(QTSSDictionary* inRequest, UInt32* outLen);
        static void*        GetRealStatusCode(QTSSDictionary* inRequest, UInt32* outLen);
		static void*		GetLocalPath(QTSSDictionary* inRequest, UInt32* outLen);

        //optimized performatted response header strings
		/* 预生成的RTSP头(包含服务器信息),参见RTSPRequestInterface::Initialize()
		RTSP/1.0 200 OK\r\n
		Server: QTSS/4.1.3.x (Build/425; Platform/MacOSX; Release/Development;)\r\n
		Cseq: */
        static char             sPremadeHeader[kStaticHeaderSizeInBytes];//512byte
        static StrPtrLen        sPremadeHeaderPtr;
        
		/* 预生成的RTSP头(不包含服务器信息),参见RTSPRequestInterface::Initialize()
		RTSP/1.0 200 OK\r\n
		Cseq: */
        static char             sPremadeNoHeader[kStaticHeaderSizeInBytes];//512byte
        static StrPtrLen        sPremadeNoHeaderPtr;
        
        static StrPtrLen        sColonSpace;
        
        //Dictionary support
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __RTSPREQUESTINTERFACE_H__

