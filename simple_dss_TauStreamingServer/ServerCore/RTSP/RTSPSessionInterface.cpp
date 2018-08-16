
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPSessionInterface.cpp
Description: Presents an API for session-wide resources for modules to use.
             Implements the RTSP Session dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "atomic.h"
#include "RTSPProtocol.h"
#include "RTSPSessionInterface.h"
#include "QTSServerInterface.h"
#include "OSMemory.h"

#include <errno.h>


#if DEBUG
	#define RTSP_SESSION_INTERFACE_DEBUGGING 1
#else
    #define RTSP_SESSION_INTERFACE_DEBUGGING 0
#endif



unsigned int            RTSPSessionInterface::sSessionIDCounter = kFirstRTSPSessionID;/* ��ʼ��Ϊ1 */
Bool16                  RTSPSessionInterface::sDoBase64Decoding = true;/* Ĭ�ϻ���base64���� */
UInt32					RTSPSessionInterface::sOptionsRequestBody[kMaxRandomDataSize / sizeof(UInt32)];//64K�ֽ�

/* RTSPSession Dictionary attributes,�μ�QTSS_RTSPSessionAttributes in QQTSS.h */
QTSSAttrInfoDict::AttrInfo  RTSPSessionInterface::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "qtssRTSPSesID",              NULL,           qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssRTSPSesLocalAddr",       SetupParams,    qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 2 */ { "qtssRTSPSesLocalAddrStr",    SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 3 */ { "qtssRTSPSesLocalDNS",        SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 4 */ { "qtssRTSPSesRemoteAddr",      SetupParams,    qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 5 */ { "qtssRTSPSesRemoteAddrStr",   SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 6 */ { "qtssRTSPSesEventCntxt",      NULL,           qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 7 */ { "qtssRTSPSesType",            NULL,           qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 8 */ { "qtssRTSPSesStreamRef",       NULL,           qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },
    
    /* 9 */ { "qtssRTSPSesLastUserName",    NULL,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 10 */{ "qtssRTSPSesLastUserPassword",NULL,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
    /* 11 */{ "qtssRTSPSesLastURLRealm",    NULL,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
    
    /* 12 */{ "qtssRTSPSesLocalPort",       SetupParams,    qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
    /* 13 */{ "qtssRTSPSesRemotePort",      SetupParams,    qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable }
};


/* ����14����̬����ֵQTSS_RTSPSessionAttributes,�������������RTSPSessionInterface::sOptionsRequestBody[]�����з���,��ʹ��һ���ֽ�Ϊ0ֵ */
void    RTSPSessionInterface::Initialize()
{
	/* ���14��QTSS_RTSPSessionAttributes,�������涨�����ؾ�̬����,���ҵ�RTSPSession��Ӧ��Dictionary,ѭ��������Ӧ������,�μ�QTSSDictionary.h */
    for (UInt32 x = 0; x < qtssRTSPSesNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPSessionDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
	
	// DJM PROTOTYPE
	/* �������������RTSPSessionInterface::sOptionsRequestBody[]�����з���,��ʹ��һ���ֽ�Ϊ0ֵ */
	::srand((unsigned int) OS::Microseconds());
	//��64K�ֽ�,ÿ���ֽ�����һ�������
	for (unsigned int i = 0; i < kMaxRandomDataSize / sizeof(UInt32); i++)
		RTSPSessionInterface::sOptionsRequestBody[i] = ::rand();
	((char *)RTSPSessionInterface::sOptionsRequestBody)[0] = 0; //always set first byte so it doesn't hit any client parser bugs for \r or \n.
	
}


RTSPSessionInterface::RTSPSessionInterface() 
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPSessionDictIndex)),/* ��ʼ��RTSPSession�ֵ� */
    Task(), 
    fTimeoutTask(NULL, QTSServerInterface::GetServer()->GetPrefs()->GetRealRTSPTimeoutInSecs() * 1000),//��ʼ����ʱ����,Ĭ��60�볬ʱ
    fInputStream(&fSocket),//��ʼ��RTSPRequestStream����
    fOutputStream(&fSocket, &fTimeoutTask),//��ʼ��RTSPResponseStream����
    fSessionMutex(),
    fTCPCoalesceBuffer(NULL),
    fNumInCoalesceBuffer(0),
    fSocket(NULL, Socket::kNonBlockingSocketType),//��ʼ������������Socket����
    fOutputSocketP(&fSocket),//ע�����Ƕ���ͬһ��TCPSocket
    fInputSocketP(&fSocket),
    fSessionType(qtssRTSPSession),//Ĭ��ͨ����RTSPSession
    fLiveSession(true),//Ĭ��alive RTSP session
    fObjectHolders(0),
    fCurChannelNum(0),
    fChNumToSessIDMap(NULL),
	fRequestBodyLen(-1),
	fSentOptionsRequest(false),//Ĭ�ϲ�����OPTIONS
	fOptionsRequestSendTime(-1),
	fRoundTripTime(-1),
	fRoundTripTimeCalculation(true)//Ĭ�ϼ���RTT,��RTSPSessionInterface::SendOptionsRequest()�и�Ϊfalse
{
    /* ���ñ����������� */
    fTimeoutTask.SetTask(this);//���ñ������Ϊ��ʱ�������
    fSocket.SetTask(this); //���ñ�������Socket
    fStreamRef = this; //���ñ�������stream

	/* �ڼ����������ϼ�1,��ֵΪ2 */
    fSessionID = (UInt32)atomic_add(&sSessionIDCounter, 1);

	/* �ֶ�����QTSS_RTSPSessionAttributes���������ֵ */
    this->SetVal(qtssRTSPSesID, &fSessionID, sizeof(fSessionID));
    this->SetVal(qtssRTSPSesEventCntxt, &fOutputSocketP, sizeof(fOutputSocketP));
    this->SetVal(qtssRTSPSesType, &fSessionType, sizeof(fSessionType));
    this->SetVal(qtssRTSPSesStreamRef, &fStreamRef, sizeof(fStreamRef));

    this->SetEmptyVal(qtssRTSPSesLastUserName, &fUserNameBuf[0], kMaxUserNameLen);
    this->SetEmptyVal(qtssRTSPSesLastUserPassword, &fUserPasswordBuf[0], kMaxUserPasswordLen);
    this->SetEmptyVal(qtssRTSPSesLastURLRealm, &fUserRealmBuf[0], kMaxUserRealmLen);
    
	//��ӡ��RTSP��Ϣ��?
    fInputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
    fOutputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
}


RTSPSessionInterface::~RTSPSessionInterface()
{
    // If the input socket is != output socket, the input socket was created dynamically
    if (fInputSocketP != fOutputSocketP) 
        delete fInputSocketP;
    
	/* ɾ��ƴ�ϻ��棬ע�����ķ�����RTSPSessionInterface::GetTwoChannelNumbers() */
    delete [] fTCPCoalesceBuffer;
    
    for (UInt8 x = 0; x < (fCurChannelNum >> 1); x++)
        delete [] fChNumToSessIDMap[x].Ptr;
    delete [] fChNumToSessIDMap;
}

/* ���ٳ��ж���ļ���,�Ҽ��������м���Ϊ��,��ɾ����RTSPSession */
void RTSPSessionInterface::DecrementObjectHolderCount()
{

#if __Win32__
//maybe don't need this special case but for now on Win32 we do it the old way since the killEvent code hasn't been verified on Windows.
    this->Signal(Task::kReadEvent);//have the object wakeup in case it can go away.
    atomic_sub(&fObjectHolders, 1);
#else
	/* ���������м���Ϊ��,��ɾ����RTSPSession */
    if (0 == atomic_sub(&fObjectHolders, 1))
        this->Signal(Task::kKillEvent);
#endif

}

/* Ĭ��ʹ�ò����淽ʽ(������ȷָ��),����RTSPResponseStream::WriteV()����д������ */
QTSS_Error RTSPSessionInterface::Write(void* inBuffer, UInt32 inLength,
                                            UInt32* outLenWritten, UInt32 inFlags)
{
	/* Ĭ������sendType��ֵΪ�����淽ʽ */
    UInt32 sendType = RTSPResponseStream::kDontBuffer;
	/* ����ȷָ�������ʹ�û��淽ʽ */
    if ((inFlags & qtssWriteFlagsBufferData) != 0)
        sendType = RTSPResponseStream::kAlwaysBuffer;
    
    iovec theVec[2];
	/* Ϊ��Ҫ���ڵ�2������? */
    theVec[1].iov_base = (char*)inBuffer;
    theVec[1].iov_len = inLength;
    return fOutputStream.WriteV(theVec, 2, inLength, outLenWritten, sendType);
}

/* ʹ�ò����淽ʽ��RTSPResponseStream::WriteV() */
QTSS_Error RTSPSessionInterface::WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten)
{
    return fOutputStream.WriteV(inVec, inNumVectors, inTotalLength, outLenWritten, RTSPResponseStream::kDontBuffer);
}

/* ����:fRequestBodyLen�������������? */
/* ����RTSPRequestStream::Read()��ֻ���볤��ΪfRequestBodyLen(�����޸ĵڶ�������)������,��ʵ�ʶ�ȡ�����ݳ������õ�������� */
QTSS_Error RTSPSessionInterface::Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead)
{
    // Don't let callers of this function accidently creep(����Խ��) past the end of the
    // request body.  If the request body size isn't known, fRequestBodyLen will be -1
    
    if (fRequestBodyLen == 0)
        return QTSS_NoMoreData;

    //ֻ����fRequestBodyLen���ȵ�RTSP request data    
    if ((fRequestBodyLen > 0) && ((SInt32)inLength > fRequestBodyLen))
        inLength = fRequestBodyLen;
    
    UInt32 theLenRead = 0;
    QTSS_Error theErr = fInputStream.Read(ioBuffer, inLength, &theLenRead);
    
    if (fRequestBodyLen >= 0)
        fRequestBodyLen -= theLenRead;

    if (outLenRead != NULL)
        *outLenRead = theLenRead;
    
    return theErr;
}

/* ���ݶ���д�¼�,������TCPSocket(����EventContext��������)�������д���¼� */
QTSS_Error RTSPSessionInterface::RequestEvent(QTSS_EventType inEventMask)
{
    if (inEventMask & QTSS_ReadableEvent)
        fInputSocketP->RequestEvent(EV_RE);
    if (inEventMask & QTSS_WriteableEvent)
        fOutputSocketP->RequestEvent(EV_WR);
        
    return QTSS_NoErr;
}

/* �½�fChNumToSessIDMap,���ƽ�ԭ����fChNumToSessIDMap����,������������½�fChNumToSessIDMap�����һ������,��󷵻ص�ǰchannel�� */
UInt8 RTSPSessionInterface::GetTwoChannelNumbers(StrPtrLen* inRTSPSessionID)
{
    // Allocate a TCP coalesce buffer if still needed
    if (fTCPCoalesceBuffer != NULL)
        fTCPCoalesceBuffer = new char[kTCPCoalesceBufferSize];//1450

    // Allocate 2 channel numbers
	/* ��ȡ��ǰchannel�� */
    UInt8 theChannelNum = fCurChannelNum;
	/* ע�⵱ǰChannel����ʱ������!! */
    fCurChannelNum+=2;
    
    // Reallocate the Ch# to Session ID Map
	/* ��2��ȡChannel pair���� */
    UInt32 numChannelEntries = fCurChannelNum >> 1;
	/* �����µ�ChNumToSessID Map */
    StrPtrLen* newMap = NEW StrPtrLen[numChannelEntries];
	/* ��ʱ���ݾɵ�fChNumToSessIDMap */
    if (fChNumToSessIDMap != NULL)
    {
        Assert(numChannelEntries > 1);
		/* ע�������һ,�ǽ�ԭ����ChNumToSessID Mapӳ�����ݸ��ƹ���,���һ��������������������� */
        ::memcpy(newMap, fChNumToSessIDMap, sizeof(StrPtrLen) * (numChannelEntries - 1));
        delete [] fChNumToSessIDMap;
    }
    fChNumToSessIDMap = newMap;
    
    // Put this sessionID to the proper place in the map
	/* �������������ChNumToSessID Map�����һ������ */
    fChNumToSessIDMap[numChannelEntries-1].Set(inRTSPSessionID->GetAsCString(), inRTSPSessionID->Len);

	/* ���ص�ǰ��channel Number */
    return theChannelNum;
}


/* �������inChannelNum��ȡ��Ӧ��RTSPSession ID */
StrPtrLen*  RTSPSessionInterface::GetSessionIDForChannelNum(UInt8 inChannelNum)
{
    if (inChannelNum < fCurChannelNum)
        return &fChNumToSessIDMap[inChannelNum >> 1];
    else
        return NULL;
}

/*********************************
/   InterleavedWrite
/   Write the given RTP packet out on the RTSP channel in interleaved format.
*/

//������ϵRTSPSession::HandleIncomingDataPacket()
/* ���Ⱦ���ץסRTSPSession mutex,��֤ԭ�Ӳ���,��ָ��������,�����ж����䳤��inLen+ƴ�ϻ����е�ԭ�����ݳ���fNumInCoalesceBuffer֮��,
���ú��ѳ���1450�ֽ�,�Ƚ�ƴ�ϻ���fTCPCoalesceBuffer�е��������ȷ��ͳ�ȥ(���), ����inLen����һ���ֽ���,���ƿ�ƴ��,ֱ��д��RTPStream��,
��inLen����һ���ֽ���û�г���,��ƴ�Ͻ�ƴ�ϻ�����,�ȴ��´�RTSPResponseStream::WriteV(ʵ���õ�writev()),����ͷŻỰ��
*/
QTSS_Error RTSPSessionInterface::InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel)
{
    /* ���绺���е����ݳ���Ϊ0 */
    if ( inLen == 0 && fNumInCoalesceBuffer == 0 )
    {   if (outLenWritten != NULL)
			*outLenWritten = 0;
        return QTSS_NoErr;
    }
        
    // First attempt to grab the RTSPSession mutex. This is to prevent writing data to
    // the connection at the same time an RTSPRequest is being processed. We cannot
    // wait for this mutex to be freed (there would be a deadlock possibility), so
    // just try to grab it, and if we can't, then just report it as an EAGAIN
	/* ����ץסRTSPSession mutex,��û�гɹ�,�ͷ���EAGAIN */
    if ( this->GetSessionMutex()->TryLock() == false )
    {
        return EAGAIN;
    }

    // DMS - this struct should be packed.
    //rt todo -- is this struct more portable (byte alignment could be a problem)?
    struct  RTPInterleaveHeader //�ܳ�Ϊ4���ֽ�,��'$ '+ 1 byte ch ID + 2 bytes length
    {
        unsigned char header;
        unsigned char channel;
        UInt16      len;
    };
    
    struct  iovec   iov[3];
    QTSS_Error      err = QTSS_NoErr;
    
    // flush rules��շ���
	/* ����ƴ�ϻ���fTCPCoalesceBuffer��������,���ټ�����������,����1450���ֽ�,��ƴ�ϻ���fTCPCoalesceBuffer�е��������ȷ��ͳ�ȥ(���) */
    if ( ( inLen > kTCPCoalesceDirectWriteSize || inLen == 0 ) && fNumInCoalesceBuffer > 0 
        || ( inLen + fNumInCoalesceBuffer + kInteleaveHeaderSize > kTCPCoalesceBufferSize ) && fNumInCoalesceBuffer > 0
        )
    {
        UInt32      buffLenWritten;
        
        // skip iov[0], WriteV uses it
        iov[1].iov_base = fTCPCoalesceBuffer;
        iov[1].iov_len = fNumInCoalesceBuffer;
        
		/* ע��ֻ�еڶ��������ĳ��� */
        err = this->GetOutputStream()->WriteV( iov, 2, fNumInCoalesceBuffer, &buffLenWritten, RTSPResponseStream::kAllOrNothing );

    #if RTSP_SESSION_INTERFACE_DEBUGGING 
        qtss_printf("InterleavedWrite: flushing %li\n", fNumInCoalesceBuffer );
    #endif
        
		/* �����Ѿ���ȫ�������Щ���� */
        if ( err == QTSS_NoErr )
            fNumInCoalesceBuffer = 0;
    }
     
    if ( err == QTSS_NoErr )
    {
        
		/* ����inLen����һ���ֽ���,���ƿ�ƴ��,ֱ��д��RTPStream�� */
        if ( inLen > kTCPCoalesceDirectWriteSize )  
        {
            struct RTPInterleaveHeader  rih;
            
            // write direct to stream
            rih.header = '$';
            rih.channel = channel;
            rih.len = htons( (UInt16)inLen);
            
            iov[1].iov_base = (char*)&rih;
            iov[1].iov_len = sizeof(rih);
            
            iov[2].iov_base = (char*)inBuffer;
            iov[2].iov_len = inLen;

			/* ע��ֻ�е�2��3�������ĳ��� */
            err = this->GetOutputStream()->WriteV( iov, 3, inLen + sizeof(rih), outLenWritten, RTSPResponseStream::kAllOrNothing );

        #if RTSP_SESSION_INTERFACE_DEBUGGING 
            qtss_printf("InterleavedWrite: bypass(�ƿ�) %li\n", inLen );
        #endif
            
        }
        else/* ���򳤶Ȳ���,���㷢��,������С��writeƴ������,�ȴ��Ժ��� */
        {
            // coalesce with other small writes
            /* ���Ƚ�RTPInterleaveHeaderƴ����fTCPCoalesceBufferĩβ,����4���ֽ� */
            fTCPCoalesceBuffer[fNumInCoalesceBuffer] = '$';
            fNumInCoalesceBuffer++;;
            
            fTCPCoalesceBuffer[fNumInCoalesceBuffer] = channel;
            fNumInCoalesceBuffer++;
            
            //*((short*)&fTCPCoalesceBuffer[fNumInCoalesceBuffer]) = htons(inLen);
            // if we ever turn TCPCoalesce back on, this should be optimized
            // for processors w/o alignment restrictions as above.
            
            SInt16  pcketLen = htons( (UInt16) inLen);
            ::memcpy( &fTCPCoalesceBuffer[fNumInCoalesceBuffer], &pcketLen, 2 );
            fNumInCoalesceBuffer += 2;
            
			/* �ٽ�ʵ������ƴ�Ͻ��� */
            ::memcpy( &fTCPCoalesceBuffer[fNumInCoalesceBuffer], inBuffer, inLen );
            fNumInCoalesceBuffer += inLen;
        
        #if RTSP_SESSION_INTERFACE_DEBUGGING 
            qtss_printf("InterleavedWrite: coalesce(ƴ��) %li, total bufff %li\n", inLen, fNumInCoalesceBuffer);
        #endif
        }
    }
    
    if ( err == QTSS_NoErr )
    {   
        /*  if no error sure to correct outLenWritten, cuz WriteV above includes the interleave header count
        
             GetOutputStream()->WriteV guarantees all or nothing for writes
             if no error, then all was written.
        */
        if ( outLenWritten != NULL )
            *outLenWritten = inLen;
    }

    this->GetSessionMutex()->Unlock();

    return err;  
}

/*
    take the TCP socket away from a RTSP session that's
    waiting to be snarfed.
    
*/
/* �Ƚ�ʧ���ֽڸ��ƹ���,���½�һ���µ�TCPSocket������ΪfInputSocketP��������Щ���� */
void    RTSPSessionInterface::SnarfInputSocket( RTSPSessionInterface* fromRTSPSession )
{
    Assert( fromRTSPSession != NULL );
    Assert( fromRTSPSession->fOutputSocketP != NULL );
    
    // grab the unused, but already read fromsocket data
    // this should be the first RTSP request

	/* Ҫ��client���ͻ���64���ܵ����� */
    if (sDoBase64Decoding)
        fInputStream.IsBase64Encoded(true); // client sends all data base64 encoded
	/* �����ָ����RTSPRequestStream�и���ʧ���ֽ�retreat bytes��fRequestBuffer��ͷ��,������Ӧ���ݳ�Ա��ֵ */
    fInputStream.SnarfRetreat( fromRTSPSession->fInputStream );

    if (fInputSocketP == fOutputSocketP)
        fInputSocketP = NEW TCPSocket( this, Socket::kNonBlockingSocketType );
    else
        fInputSocketP->Cleanup();   // if this is a socket replacing an old socket, we need
                                    // to make sure the file descriptor gets closed

	/* ������ε�ֵ���ñ���TCPSocket����Ա����Ϣ,Ȼ���������еĸ���Աֵ,�����ڸ��ƹ��캯�� */
    fInputSocketP->SnarfSocket( fromRTSPSession->fSocket );
    
    // fInputStream, meet your new input socket
	// Use a different TCPSocket to read request data
    fInputStream.AttachToSocket( fInputSocketP );
}

/* �����TCPSocket�õ���C/S ip&port��string������RTSPSession Dictionary��Ӧ����ֵ */
void* RTSPSessionInterface::SetupParams(QTSSDictionary* inSession, UInt32* /*outLen*/)
{
    RTSPSessionInterface* theSession = (RTSPSessionInterface*)inSession;
 
	/* ��TCPSocket�õ���C/S ip&port */
    theSession->fLocalAddr = theSession->fSocket.GetLocalAddr();
    theSession->fRemoteAddr = theSession->fSocket.GetRemoteAddr();
    
    theSession->fLocalPort = theSession->fSocket.GetLocalPort();
    theSession->fRemotePort = theSession->fSocket.GetRemotePort();
    
	/* C/S ip&port$dns string */
    StrPtrLen* theLocalAddrStr = theSession->fSocket.GetLocalAddrStr();
    StrPtrLen* theLocalDNSStr = theSession->fSocket.GetLocalDNSStr();
    StrPtrLen* theRemoteAddrStr = theSession->fSocket.GetRemoteAddrStr();
    if (theLocalAddrStr == NULL || theLocalDNSStr == NULL || theRemoteAddrStr == NULL)
    {    //the socket is bad most likely values are all 0. If the socket had an error we shouldn't even be here.
         //theLocalDNSStr is set to localAddr if it is unavailable, so it should be present at this point as well.
         Assert(0);   //for debugging
         return NULL; //nothing to set
    }

	/* �������õ�ֵ����RTSPSession Dictionary attributes */
    theSession->SetVal(qtssRTSPSesLocalAddr, &theSession->fLocalAddr, sizeof(theSession->fLocalAddr));
    theSession->SetVal(qtssRTSPSesLocalAddrStr, theLocalAddrStr->Ptr, theLocalAddrStr->Len);
    theSession->SetVal(qtssRTSPSesLocalDNS, theLocalDNSStr->Ptr, theLocalDNSStr->Len);
    theSession->SetVal(qtssRTSPSesRemoteAddr, &theSession->fRemoteAddr, sizeof(theSession->fRemoteAddr));
    theSession->SetVal(qtssRTSPSesRemoteAddrStr, theRemoteAddrStr->Ptr, theRemoteAddrStr->Len);
    
    theSession->SetVal(qtssRTSPSesLocalPort, &theSession->fLocalPort, sizeof(theSession->fLocalPort));
    theSession->SetVal(qtssRTSPSesRemotePort, &theSession->fRemotePort, sizeof(theSession->fRemotePort));
    return NULL;
}

/* ��fOutputStream�б���fOutputStream�м����ͳ�������,������fOldOutputStreamBuffer */
void RTSPSessionInterface::SaveOutputStream()
{
	Assert(fOldOutputStreamBuffer.Ptr == NULL);
	fOldOutputStreamBuffer.Ptr = NEW char[fOutputStream.GetBytesWritten()];
	fOldOutputStreamBuffer.Len = fOutputStream.GetBytesWritten();
	::memcpy(fOldOutputStreamBuffer.Ptr, fOutputStream.GetBufPtr(), fOldOutputStreamBuffer.Len);
}

/* used in RTSPSession::Run():case kSendingResponse */
/* �������fOldOutputStreamBuffer�е�����,���ƽ�EOL֮ǰ���ַ�����fOutputStream;��RTSP headers����"x-Dynamic-Rate"ͷ,
��Ҫ��fOutputStream����";rtt=**".�����ĩβ����ԭ�е�"\r\n"���µõ�fOutputStream.��ɾȥfOldOutputStreamBuffer. */
void RTSPSessionInterface::RevertOutputStream()
{
	/* �μ�RTSPSessionInterface::SaveOutputStream() */
	Assert(fOldOutputStreamBuffer.Ptr != NULL);
	Assert(fOldOutputStreamBuffer.Len != 0);
	static StrPtrLen theRTTStr(";rtt=", 5);/* rtt- round trip time */
	
	if (fOldOutputStreamBuffer.Ptr != NULL)
	{
		//fOutputStream.Put(fOldOutputStreamBuffer);		
		StringParser theStreamParser(&fOldOutputStreamBuffer);
		StrPtrLen theHeader;
		StrPtrLen theEOL;
		StrPtrLen theField;
		StrPtrLen theValue;
		/* �������fOldOutputStreamBuffer�е�RTSP header,�����ƽ�EOL֮ǰ���ַ�����fOutputStream */
		while(theStreamParser.GetDataRemaining() != 0)
		{
			/* ָ��fStartGet��ͷ��ʼ�Ƶ�,ֱ������'\r\n'ͣ��,���������ַ�����theHeader */
			theStreamParser.ConsumeUntil(&theHeader, StringParser::sEOLMask);
			if (theHeader.Len != 0)
			{
				/* ��theHeader�ַ�������fOutputStream */
				fOutputStream.Put(theHeader);
				
				/* ����������theHeader����ַ��� */
				StringParser theHeaderParser(&theHeader);
				/* ָ��fStartGet��theHeader��ͷ��ʼ�Ƶ�,ֱ������':'ͣ��,���������ַ�����theField */
				theHeaderParser.ConsumeUntil(&theField, ':');
				if (theHeaderParser.PeekFast() == ':')
				{
					/* ����theFieldΪ"x-Dynamic-Rate" */
					if(theField.Equal(RTSPProtocol::GetHeaderString(qtssXDynamicRateHeader)))
					{
						/* ����";rtt="������ʱ��ֵ */
						fOutputStream.Put(theRTTStr);
						fOutputStream.Put(fRoundTripTime);/* ����:fRoundTripTime�����︳ֵ��? */
					}	
				}
			}
			/* ָ��fStartGetԽ��'\r'��'\n'������,���theEOL��ֵ,�����ټ��� */
			theStreamParser.ConsumeEOL(&theEOL);
			/* �ٽ�'\r\n'����fOutputStream */
			fOutputStream.Put(theEOL);
		}
		
		/* ɾ��fOldOutputStreamBuffer */
		fOldOutputStreamBuffer.Delete();
	}
}

/* used in RTSPSession::SetupRequest() */
// ������ϵRTSPSession::ParseOptionsResponse()
/* ��fOutputStream�з���"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n",�ٷ���ָ�����ȵ�1400���ֽڵ�����; */
void RTSPSessionInterface::SendOptionsRequest()
{
	static StrPtrLen	sOptionsRequestHeader("OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n");	
	
	fOutputStream.Put(sOptionsRequestHeader);
	/* ����ָ�����ȵ�1400���ֽڵ����� */
	fOutputStream.Put((char*)(RTSPSessionInterface::sOptionsRequestBody), 1400);

	/* ��¼����sOptionsRequestHeader��local time */
	fOptionsRequestSendTime = OS::Milliseconds();
	fSentOptionsRequest = true;
	fRoundTripTimeCalculation = false;
}
