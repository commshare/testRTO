
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



unsigned int            RTSPSessionInterface::sSessionIDCounter = kFirstRTSPSessionID;/* 初始化为1 */
Bool16                  RTSPSessionInterface::sDoBase64Decoding = true;/* 默认基于base64解码 */
UInt32					RTSPSessionInterface::sOptionsRequestBody[kMaxRandomDataSize / sizeof(UInt32)];//64K字节

/* RTSPSession Dictionary attributes,参见QTSS_RTSPSessionAttributes in QQTSS.h */
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


/* 设置14个静态属性值QTSS_RTSPSessionAttributes,利用随机数设置RTSPSessionInterface::sOptionsRequestBody[]的所有分量,并使第一个字节为0值 */
void    RTSPSessionInterface::Initialize()
{
	/* 针对14个QTSS_RTSPSessionAttributes,利用上面定义的相关静态属性,先找到RTSPSession对应的Dictionary,循环设置相应的属性,参见QTSSDictionary.h */
    for (UInt32 x = 0; x < qtssRTSPSesNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPSessionDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
	
	// DJM PROTOTYPE
	/* 利用随机数设置RTSPSessionInterface::sOptionsRequestBody[]的所有分量,并使第一个字节为0值 */
	::srand((unsigned int) OS::Microseconds());
	//对64K字节,每个字节设置一个随机数
	for (unsigned int i = 0; i < kMaxRandomDataSize / sizeof(UInt32); i++)
		RTSPSessionInterface::sOptionsRequestBody[i] = ::rand();
	((char *)RTSPSessionInterface::sOptionsRequestBody)[0] = 0; //always set first byte so it doesn't hit any client parser bugs for \r or \n.
	
}


RTSPSessionInterface::RTSPSessionInterface() 
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPSessionDictIndex)),/* 初始化RTSPSession字典 */
    Task(), 
    fTimeoutTask(NULL, QTSServerInterface::GetServer()->GetPrefs()->GetRealRTSPTimeoutInSecs() * 1000),//初始化超时对象,默认60秒超时
    fInputStream(&fSocket),//初始化RTSPRequestStream对象
    fOutputStream(&fSocket, &fTimeoutTask),//初始化RTSPResponseStream对象
    fSessionMutex(),
    fTCPCoalesceBuffer(NULL),
    fNumInCoalesceBuffer(0),
    fSocket(NULL, Socket::kNonBlockingSocketType),//初始化非阻塞类型Socket对象
    fOutputSocketP(&fSocket),//注意它们都是同一个TCPSocket
    fInputSocketP(&fSocket),
    fSessionType(qtssRTSPSession),//默认通常的RTSPSession
    fLiveSession(true),//默认alive RTSP session
    fObjectHolders(0),
    fCurChannelNum(0),
    fChNumToSessIDMap(NULL),
	fRequestBodyLen(-1),
	fSentOptionsRequest(false),//默认不发送OPTIONS
	fOptionsRequestSendTime(-1),
	fRoundTripTime(-1),
	fRoundTripTimeCalculation(true)//默认计算RTT,在RTSPSessionInterface::SendOptionsRequest()中改为false
{
    /* 设置本类对象的属性 */
    fTimeoutTask.SetTask(this);//设置本类对象为超时任务对象
    fSocket.SetTask(this); //设置本类对象的Socket
    fStreamRef = this; //设置本类对象的stream

	/* 在计数器基础上加1,即值为2 */
    fSessionID = (UInt32)atomic_add(&sSessionIDCounter, 1);

	/* 手动设置QTSS_RTSPSessionAttributes的相关属性值 */
    this->SetVal(qtssRTSPSesID, &fSessionID, sizeof(fSessionID));
    this->SetVal(qtssRTSPSesEventCntxt, &fOutputSocketP, sizeof(fOutputSocketP));
    this->SetVal(qtssRTSPSesType, &fSessionType, sizeof(fSessionType));
    this->SetVal(qtssRTSPSesStreamRef, &fStreamRef, sizeof(fStreamRef));

    this->SetEmptyVal(qtssRTSPSesLastUserName, &fUserNameBuf[0], kMaxUserNameLen);
    this->SetEmptyVal(qtssRTSPSesLastUserPassword, &fUserPasswordBuf[0], kMaxUserPasswordLen);
    this->SetEmptyVal(qtssRTSPSesLastURLRealm, &fUserRealmBuf[0], kMaxUserRealmLen);
    
	//打印出RTSP信息吗?
    fInputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
    fOutputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
}


RTSPSessionInterface::~RTSPSessionInterface()
{
    // If the input socket is != output socket, the input socket was created dynamically
    if (fInputSocketP != fOutputSocketP) 
        delete fInputSocketP;
    
	/* 删除拼合缓存，注意它的分配在RTSPSessionInterface::GetTwoChannelNumbers() */
    delete [] fTCPCoalesceBuffer;
    
    for (UInt8 x = 0; x < (fCurChannelNum >> 1); x++)
        delete [] fChNumToSessIDMap[x].Ptr;
    delete [] fChNumToSessIDMap;
}

/* 减少持有对象的计数,且假如对象持有计数为零,就删除该RTSPSession */
void RTSPSessionInterface::DecrementObjectHolderCount()
{

#if __Win32__
//maybe don't need this special case but for now on Win32 we do it the old way since the killEvent code hasn't been verified on Windows.
    this->Signal(Task::kReadEvent);//have the object wakeup in case it can go away.
    atomic_sub(&fObjectHolders, 1);
#else
	/* 假如对象持有计数为零,就删除该RTSPSession */
    if (0 == atomic_sub(&fObjectHolders, 1))
        this->Signal(Task::kKillEvent);
#endif

}

/* 默认使用不缓存方式(除非明确指定),利用RTSPResponseStream::WriteV()向外写出数据 */
QTSS_Error RTSPSessionInterface::Write(void* inBuffer, UInt32 inLength,
                                            UInt32* outLenWritten, UInt32 inFlags)
{
	/* 默认设置sendType的值为不缓存方式 */
    UInt32 sendType = RTSPResponseStream::kDontBuffer;
	/* 若明确指定缓存就使用缓存方式 */
    if ((inFlags & qtssWriteFlagsBufferData) != 0)
        sendType = RTSPResponseStream::kAlwaysBuffer;
    
    iovec theVec[2];
	/* 为何要放在第2个向量? */
    theVec[1].iov_base = (char*)inBuffer;
    theVec[1].iov_len = inLength;
    return fOutputStream.WriteV(theVec, 2, inLength, outLenWritten, sendType);
}

/* 使用不缓存方式的RTSPResponseStream::WriteV() */
QTSS_Error RTSPSessionInterface::WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten)
{
    return fOutputStream.WriteV(inVec, inNumVectors, inTotalLength, outLenWritten, RTSPResponseStream::kDontBuffer);
}

/* 试问:fRequestBodyLen最初在哪里设置? */
/* 利用RTSPRequestStream::Read()来只读入长度为fRequestBodyLen(用它修改第二个参数)的数据,用实际读取的数据长度设置第三个入参 */
QTSS_Error RTSPSessionInterface::Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead)
{
    // Don't let callers of this function accidently creep(意外越过) past the end of the
    // request body.  If the request body size isn't known, fRequestBodyLen will be -1
    
    if (fRequestBodyLen == 0)
        return QTSS_NoMoreData;

    //只读入fRequestBodyLen长度的RTSP request data    
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

/* 根据读或写事件,来调用TCPSocket(它是EventContext的派生类)来读入或写出事件 */
QTSS_Error RTSPSessionInterface::RequestEvent(QTSS_EventType inEventMask)
{
    if (inEventMask & QTSS_ReadableEvent)
        fInputSocketP->RequestEvent(EV_RE);
    if (inEventMask & QTSS_WriteableEvent)
        fOutputSocketP->RequestEvent(EV_WR);
        
    return QTSS_NoErr;
}

/* 新建fChNumToSessIDMap,复制进原来的fChNumToSessIDMap数据,并用入参配置新建fChNumToSessIDMap的最后一个分量,最后返回当前channel号 */
UInt8 RTSPSessionInterface::GetTwoChannelNumbers(StrPtrLen* inRTSPSessionID)
{
    // Allocate a TCP coalesce buffer if still needed
    if (fTCPCoalesceBuffer != NULL)
        fTCPCoalesceBuffer = new char[kTCPCoalesceBufferSize];//1450

    // Allocate 2 channel numbers
	/* 获取当前channel号 */
    UInt8 theChannelNum = fCurChannelNum;
	/* 注意当前Channel数及时更新了!! */
    fCurChannelNum+=2;
    
    // Reallocate the Ch# to Session ID Map
	/* 除2获取Channel pair总数 */
    UInt32 numChannelEntries = fCurChannelNum >> 1;
	/* 创建新的ChNumToSessID Map */
    StrPtrLen* newMap = NEW StrPtrLen[numChannelEntries];
	/* 及时扩容旧的fChNumToSessIDMap */
    if (fChNumToSessIDMap != NULL)
    {
        Assert(numChannelEntries > 1);
		/* 注意这里减一,是将原来的ChNumToSessID Map映射数据复制过来,最后一个分量还需下面接着配置 */
        ::memcpy(newMap, fChNumToSessIDMap, sizeof(StrPtrLen) * (numChannelEntries - 1));
        delete [] fChNumToSessIDMap;
    }
    fChNumToSessIDMap = newMap;
    
    // Put this sessionID to the proper place in the map
	/* 利用入参来更新ChNumToSessID Map的最后一个分量 */
    fChNumToSessIDMap[numChannelEntries-1].Set(inRTSPSessionID->GetAsCString(), inRTSPSessionID->Len);

	/* 返回当前的channel Number */
    return theChannelNum;
}


/* 利用入参inChannelNum获取对应的RTSPSession ID */
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

//紧密联系RTSPSession::HandleIncomingDataPacket()
/* 首先尽力抓住RTSPSession mutex,保证原子操作,对指定的数据,首先判断若其长度inLen+拼合缓存中的原有数据长度fNumInCoalesceBuffer之和,
若该和已超过1450字节,先将拼合缓存fTCPCoalesceBuffer中的数据首先发送出去(清空), 假如inLen超过一定字节数,就绕开拼合,直接写到RTPStream里,
若inLen超过一定字节数没有超过,就拼合进拼合缓存中,等待下次RTSPResponseStream::WriteV(实质用到writev()),最后释放会话锁
*/
QTSS_Error RTSPSessionInterface::InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel)
{
    /* 假如缓存中的数据长度为0 */
    if ( inLen == 0 && fNumInCoalesceBuffer == 0 )
    {   if (outLenWritten != NULL)
			*outLenWritten = 0;
        return QTSS_NoErr;
    }
        
    // First attempt to grab the RTSPSession mutex. This is to prevent writing data to
    // the connection at the same time an RTSPRequest is being processed. We cannot
    // wait for this mutex to be freed (there would be a deadlock possibility), so
    // just try to grab it, and if we can't, then just report it as an EAGAIN
	/* 尽力抓住RTSPSession mutex,若没有成功,就返回EAGAIN */
    if ( this->GetSessionMutex()->TryLock() == false )
    {
        return EAGAIN;
    }

    // DMS - this struct should be packed.
    //rt todo -- is this struct more portable (byte alignment could be a problem)?
    struct  RTPInterleaveHeader //总长为4个字节,即'$ '+ 1 byte ch ID + 2 bytes length
    {
        unsigned char header;
        unsigned char channel;
        UInt16      len;
    };
    
    struct  iovec   iov[3];
    QTSS_Error      err = QTSS_NoErr;
    
    // flush rules清空法则
	/* 假如拼合缓存fTCPCoalesceBuffer已有数据,若再加上其他数据,超过1450个字节,则将拼合缓存fTCPCoalesceBuffer中的数据首先发送出去(清空) */
    if ( ( inLen > kTCPCoalesceDirectWriteSize || inLen == 0 ) && fNumInCoalesceBuffer > 0 
        || ( inLen + fNumInCoalesceBuffer + kInteleaveHeaderSize > kTCPCoalesceBufferSize ) && fNumInCoalesceBuffer > 0
        )
    {
        UInt32      buffLenWritten;
        
        // skip iov[0], WriteV uses it
        iov[1].iov_base = fTCPCoalesceBuffer;
        iov[1].iov_len = fNumInCoalesceBuffer;
        
		/* 注意只有第二个分量的长度 */
        err = this->GetOutputStream()->WriteV( iov, 2, fNumInCoalesceBuffer, &buffLenWritten, RTSPResponseStream::kAllOrNothing );

    #if RTSP_SESSION_INTERFACE_DEBUGGING 
        qtss_printf("InterleavedWrite: flushing %li\n", fNumInCoalesceBuffer );
    #endif
        
		/* 表明已经完全清空了这些数据 */
        if ( err == QTSS_NoErr )
            fNumInCoalesceBuffer = 0;
    }
     
    if ( err == QTSS_NoErr )
    {
        
		/* 假如inLen超过一定字节数,就绕开拼合,直接写到RTPStream里 */
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

			/* 注意只有第2和3个分量的长度 */
            err = this->GetOutputStream()->WriteV( iov, 3, inLen + sizeof(rih), outLenWritten, RTSPResponseStream::kAllOrNothing );

        #if RTSP_SESSION_INTERFACE_DEBUGGING 
            qtss_printf("InterleavedWrite: bypass(绕开) %li\n", inLen );
        #endif
            
        }
        else/* 否则长度不够,不足发送,与其他小的write拼合起来,等待以后发送 */
        {
            // coalesce with other small writes
            /* 首先将RTPInterleaveHeader拼合在fTCPCoalesceBuffer末尾,增加4个字节 */
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
            
			/* 再将实际数据拼合进来 */
            ::memcpy( &fTCPCoalesceBuffer[fNumInCoalesceBuffer], inBuffer, inLen );
            fNumInCoalesceBuffer += inLen;
        
        #if RTSP_SESSION_INTERFACE_DEBUGGING 
            qtss_printf("InterleavedWrite: coalesce(拼合) %li, total bufff %li\n", inLen, fNumInCoalesceBuffer);
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
/* 先将失控字节复制过来,再新建一个新的TCPSocket并设置为fInputSocketP来读进这些数据 */
void    RTSPSessionInterface::SnarfInputSocket( RTSPSessionInterface* fromRTSPSession )
{
    Assert( fromRTSPSession != NULL );
    Assert( fromRTSPSession->fOutputSocketP != NULL );
    
    // grab the unused, but already read fromsocket data
    // this should be the first RTSP request

	/* 要求client发送基于64解密的数据 */
    if (sDoBase64Decoding)
        fInputStream.IsBase64Encoded(true); // client sends all data base64 encoded
	/* 从入参指定的RTSPRequestStream中复制失控字节retreat bytes到fRequestBuffer开头处,设置相应数据成员的值 */
    fInputStream.SnarfRetreat( fromRTSPSession->fInputStream );

    if (fInputSocketP == fOutputSocketP)
        fInputSocketP = NEW TCPSocket( this, Socket::kNonBlockingSocketType );
    else
        fInputSocketP->Cleanup();   // if this is a socket replacing an old socket, we need
                                    // to make sure the file descriptor gets closed

	/* 利用入参的值配置本类TCPSocket各成员的信息,然后清空入参中的各成员值,类似于复制构造函数 */
    fInputSocketP->SnarfSocket( fromRTSPSession->fSocket );
    
    // fInputStream, meet your new input socket
	// Use a different TCPSocket to read request data
    fInputStream.AttachToSocket( fInputSocketP );
}

/* 从入参TCPSocket得到的C/S ip&port和string并设置RTSPSession Dictionary相应属性值 */
void* RTSPSessionInterface::SetupParams(QTSSDictionary* inSession, UInt32* /*outLen*/)
{
    RTSPSessionInterface* theSession = (RTSPSessionInterface*)inSession;
 
	/* 从TCPSocket得到的C/S ip&port */
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

	/* 用上面获得的值设置RTSPSession Dictionary attributes */
    theSession->SetVal(qtssRTSPSesLocalAddr, &theSession->fLocalAddr, sizeof(theSession->fLocalAddr));
    theSession->SetVal(qtssRTSPSesLocalAddrStr, theLocalAddrStr->Ptr, theLocalAddrStr->Len);
    theSession->SetVal(qtssRTSPSesLocalDNS, theLocalDNSStr->Ptr, theLocalDNSStr->Len);
    theSession->SetVal(qtssRTSPSesRemoteAddr, &theSession->fRemoteAddr, sizeof(theSession->fRemoteAddr));
    theSession->SetVal(qtssRTSPSesRemoteAddrStr, theRemoteAddrStr->Ptr, theRemoteAddrStr->Len);
    
    theSession->SetVal(qtssRTSPSesLocalPort, &theSession->fLocalPort, sizeof(theSession->fLocalPort));
    theSession->SetVal(qtssRTSPSesRemotePort, &theSession->fRemotePort, sizeof(theSession->fRemotePort));
    return NULL;
}

/* 从fOutputStream中备份fOutputStream中即将送出的数据,来配置fOldOutputStreamBuffer */
void RTSPSessionInterface::SaveOutputStream()
{
	Assert(fOldOutputStreamBuffer.Ptr == NULL);
	fOldOutputStreamBuffer.Ptr = NEW char[fOutputStream.GetBytesWritten()];
	fOldOutputStreamBuffer.Len = fOutputStream.GetBytesWritten();
	::memcpy(fOldOutputStreamBuffer.Ptr, fOutputStream.GetBufPtr(), fOldOutputStreamBuffer.Len);
}

/* used in RTSPSession::Run():case kSendingResponse */
/* 逐个解析fOldOutputStreamBuffer中的数据,复制进EOL之前的字符串进fOutputStream;若RTSP headers中有"x-Dynamic-Rate"头,
就要在fOutputStream插入";rtt=**".最后在末尾补上原有的"\r\n"重新得到fOutputStream.并删去fOldOutputStreamBuffer. */
void RTSPSessionInterface::RevertOutputStream()
{
	/* 参见RTSPSessionInterface::SaveOutputStream() */
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
		/* 逐个解析fOldOutputStreamBuffer中的RTSP header,并复制进EOL之前的字符串进fOutputStream */
		while(theStreamParser.GetDataRemaining() != 0)
		{
			/* 指针fStartGet从头开始移到,直到遇到'\r\n'停下,将经过的字符串给theHeader */
			theStreamParser.ConsumeUntil(&theHeader, StringParser::sEOLMask);
			if (theHeader.Len != 0)
			{
				/* 将theHeader字符串放入fOutputStream */
				fOutputStream.Put(theHeader);
				
				/* 下面具体分析theHeader这个字符串 */
				StringParser theHeaderParser(&theHeader);
				/* 指针fStartGet从theHeader开头开始移到,直到遇到':'停下,将经过的字符串给theField */
				theHeaderParser.ConsumeUntil(&theField, ':');
				if (theHeaderParser.PeekFast() == ':')
				{
					/* 假如theField为"x-Dynamic-Rate" */
					if(theField.Equal(RTSPProtocol::GetHeaderString(qtssXDynamicRateHeader)))
					{
						/* 放入";rtt="和往返时间值 */
						fOutputStream.Put(theRTTStr);
						fOutputStream.Put(fRoundTripTime);/* 试问:fRoundTripTime在哪里赋值的? */
					}	
				}
			}
			/* 指针fStartGet越过'\r'或'\n'并换行,获得theEOL的值,下面再加上 */
			theStreamParser.ConsumeEOL(&theEOL);
			/* 再将'\r\n'放入fOutputStream */
			fOutputStream.Put(theEOL);
		}
		
		/* 删除fOldOutputStreamBuffer */
		fOldOutputStreamBuffer.Delete();
	}
}

/* used in RTSPSession::SetupRequest() */
// 紧密联系RTSPSession::ParseOptionsResponse()
/* 在fOutputStream中放入"OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n",再放入指定长度的1400个字节的数组; */
void RTSPSessionInterface::SendOptionsRequest()
{
	static StrPtrLen	sOptionsRequestHeader("OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n");	
	
	fOutputStream.Put(sOptionsRequestHeader);
	/* 放入指定长度的1400个字节的数组 */
	fOutputStream.Put((char*)(RTSPSessionInterface::sOptionsRequestBody), 1400);

	/* 记录放入sOptionsRequestHeader的local time */
	fOptionsRequestSendTime = OS::Milliseconds();
	fSentOptionsRequest = true;
	fRoundTripTimeCalculation = false;
}
