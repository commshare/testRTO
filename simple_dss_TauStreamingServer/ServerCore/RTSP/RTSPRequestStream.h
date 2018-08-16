
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequestStream.h
Description: Provides a stream abstraction for RTSP. Given a socket, this object
			 can read in data until an entire RTSP request header is available.
			 (do this by calling ReadRequest). It handles RTSP pipelining (request
			 headers are produced serially even if multiple headers arrive simultaneously),
			 & RTSP request data.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 




#ifndef __RTSPREQUESTSTREAM_H__
#define __RTSPREQUESTSTREAM_H__

#include "StrPtrLen.h"
#include "TCPSocket.h"
#include "QTSS.h"



class RTSPRequestStream
{
public:

    //CONSTRUCTOR / DESTRUCTOR
	//�μ�RTSPSessionInterface::RTSPSessionInterface()
    RTSPRequestStream(TCPSocket* sock);
    
    // We may have to delete this memory if it was allocated due to base64 decoding
    ~RTSPRequestStream() { if (fRequest.Ptr != &fRequestBuffer[0]) delete [] fRequest.Ptr; }

    //ReadRequest
    //This function will not block.
    //Attempts to read data into the stream, stopping when we hit the EOL - EOL that
    //ends an RTSP header.
    //
    //Returns:          QTSS_NoErr:     Out of data, haven't hit EOL - EOL yet û�����ݿɶ�,���в���һ��������RTSP����
    //                  QTSS_RequestArrived: full request has arrived
    //                  E2BIG: ran out of buffer space
    //                  QTSS_RequestFailed: if the client has disconnected
    //                  EINVAL: if we are base64 decoding and the stream is corrupt
    //                  QTSS_OutOfState:
	/* read in data until an entire RTSP request header is available. */
	/******************* ������Ҫ��һ������ ***************/
    QTSS_Error      ReadRequest();
    
    // Read
    //
    // This function reads data off of the stream, and places it into the buffer provided
    // Returns: QTSS_NoErr, EAGAIN if it will block, or another socket error.
	/* ������ݵ�ָ�����ȵĻ���,������:������ʧ������ʱ,���ȶ�ȡ��ֱ���������inBufLen;����û����������,�ʹ�Socketֱ��
	��ȡ����������ָ���ĳ���,���������ָ�������һ�ζ�ȡ���ݵ��ܳ���(Ӧ����inBufLen) */
    QTSS_Error      Read(void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead);
    
    // Use a different TCPSocket to read request data 
    // this will be used by RTSPSessionInterface::SnarfInputSocket()
    void            AttachToSocket(TCPSocket* sock) { fSocket = sock; }
    
    // Tell the request stream whether or not to decode from base64.
    void            IsBase64Encoded(Bool16 isDataEncoded) { fDecode = isDataEncoded; }
    
    //GetRequestBuffer
    //This returns a buffer containing the full client request. The length is set to
    //the exact length of the request headers. This will return NULL UNLESS this object
    //is in the proper state (has been initialized, ReadRequest() has been called until it returns
        //RequestArrived).
    StrPtrLen*  GetRequestBuffer()  { return fRequestPtr; }
    Bool16      IsDataPacket()      { return fIsDataPacket; }

	/* �ܷ�print RTSP info? */
    void        ShowRTSP(Bool16 enable) {fPrintRTSP = enable; }     
    void        SnarfRetreat( RTSPRequestStream &fromRequest );
        
private:

        
    //CONSTANTS:
    enum
    {
        kRequestBufferSizeInBytes = 2048        //UInt32
    };
    
    // Base64 decodes into fRequest.Ptr, updates fRequest.Len, and returns the amount
    // of data left undecoded in inSrcData
	/* ����Base64decode()��decode�����ָ��������,�����������bit�ϵ�ֵ��decode */
    QTSS_Error              DecodeIncomingData(char* inSrcData, UInt32 inSrcDataLen);

    TCPSocket*              fSocket;
    UInt32                  fRetreatBytes;
    UInt32                  fRetreatBytesRead; // Used by RTSPRequestStream::Read() when it is reading RetreatBytes
    
	/* ��2048�ֽ���bufferһ��RTSP Request��������ǳ���Ҫ�� */
    char                    fRequestBuffer[kRequestBufferSizeInBytes];
    UInt32                  fCurOffset; // tracks how much valid data is in the above buffer
    UInt32                  fEncodedBytesRemaining; // If we are decoding, tracks(׷��) how many encoded bytes are in the buffer?
    
    StrPtrLen               fRequest;  // fRequest.Len always refers to the length of the request header
    StrPtrLen*              fRequestPtr;    // pointer to a request header

    Bool16                  fDecode;        // should we base 64 decode?
    Bool16                  fIsDataPacket;  // is this a data packet? Like for a record?
    Bool16                  fPrintRTSP;     // debugging printfs
    
};

#endif
