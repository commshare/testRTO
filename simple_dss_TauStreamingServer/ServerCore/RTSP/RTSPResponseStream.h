
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPResponseStream.h
Description: Object that provides a "buffered WriteV" service. Clients
			 can call this function to write to a socket, and buffer flow
			 controlled data in different ways.
			 It is derived from StringFormatter, which it uses as an output
			 stream buffer. The buffer may grow infinitely.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __RTSP_RESPONSE_STREAM_H__
#define __RTSP_RESPONSE_STREAM_H__

#include "ResizeableStringFormatter.h"
#include "TCPSocket.h"
#include "TimeoutTask.h"
#include "QTSS.h"



class RTSPResponseStream : public ResizeableStringFormatter
{
    public:
    
        // This object provides some flow control buffering services.
        // It also refreshes the timeout whenever there is a successful write
        // on the socket.
		//参见RTSPSessionInterface::RTSPSessionInterface()
        RTSPResponseStream(TCPSocket* inSocket, TimeoutTask* inTimeoutTask)
            :   ResizeableStringFormatter(fOutputBuf, kOutputBufferSizeInBytes),
                fSocket(inSocket), fBytesSentInBuffer(0), fTimeoutTask(inTimeoutTask),fPrintRTSP(false) {}
        
        virtual ~RTSPResponseStream() {}

        // WriteV
        //
        // This function takes an input ioVec and writes it to the socket. If any
        // data has been written to this stream via Put, that data gets written first.
        //
        // In the event of flow control on the socket, less data than what was
        // requested, or no data at all, may be sent. Specify what you want this
        // function to do with the unsent data via inSendType.
        //
        // kAlwaysBuffer:   Buffer any unsent data internally.
        // kAllOrNothing:   If no data could be sent, return EWOULDBLOCK. Otherwise,
        //                  buffer any unsent data.
        // kDontBuffer:     Never buffer any data.
        //
        // If some data ends up being buffered(假如某些数据终止缓存), outLengthSent will = inTotalLength,
        // and the return value will be QTSS_NoErr 
        
		/* 入参inSendType的取值 */
        enum
        {
            kDontBuffer     = 0,
            kAllOrNothing   = 1,
            kAlwaysBuffer   = 2
        };
        QTSS_Error WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength,
                                UInt32* outLengthSent, UInt32 inSendType);

        // Flushes(刷新) any buffered data to the socket. If all data could be sent,
        // this returns QTSS_NoErr, otherwise, it returns EWOULDBLOCK
        QTSS_Error Flush();
        
		/* 是否打印RTSP信息? */
        void        ShowRTSP(Bool16 enable) {fPrintRTSP = enable; }     

        
    private:
    
        enum
        {
            kOutputBufferSizeInBytes = 512  //UInt32
        };
        
        //The default buffer size is allocated inline as part of the object. Because this size
        //is good enough for 99.9% of all requests, we avoid the dynamic memory allocation in most
        //cases. But if the response is too big for this buffer, the ResizeableStringFormatter::BufferIsFull() function will
        //allocate a larger buffer.
		/* output buffer size 是512K,这个缓存对99.9%的RTSP request都适用,在绝大多数情况下,避免动态内存分配 */
        char                    fOutputBuf[kOutputBufferSizeInBytes];
        TCPSocket*              fSocket;
        UInt32                  fBytesSentInBuffer;
        TimeoutTask*            fTimeoutTask;
        Bool16                  fPrintRTSP;     // debugging printfs
        
        friend class RTSPRequestInterface;
};


#endif // __RTSP_RESPONSE_STREAM_H__
