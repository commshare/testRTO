
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPResponseStream.cpp
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



#include "RTSPResponseStream.h"
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "StringTranslator.h"

#include <errno.h>

/* used in RTSPSessionInterface::WriteV()/InterleavedWrite()/Write() */
/* 使用writev(),若RTSP Response流的缓冲区中有待发数据,首先将其发送出去,否则将其他缓冲区的数据发送出,
并刷新超时任务,最后比较发送的总长度outLengthSent与要发送数据总长度inTotalLength的差值,将该段剩余数据
以及随后的iovec放入缓冲区待发送,依据第5个参数决定是否缓存(利用StringFormatter::Put())没有发送出的iovec data?
*/
QTSS_Error RTSPResponseStream::WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength,
                                            UInt32* outLengthSent, UInt32 inSendType)
{
    QTSS_Error theErr = QTSS_NoErr;
	/* 用TCPSocket已经送出数据的长度,注意这个量十分重要 */
    UInt32 theLengthSent = 0;
	/* 缓存中剩余data的字节数 */
    UInt32 amtInBuffer = this->GetCurrentOffset() - fBytesSentInBuffer;
    
	/* 假如buffer中还有剩余数据,就把它用Socket写入RTSP Response流中 */
    if (amtInBuffer > 0)
    {

        // There is some data in the output buffer. Make sure to send that
        // first, using the empty space in the ioVec.
         
		/* 确定buffer中剩余数据的起点和长度,配置为inVec的第一个分量 */
        inVec[0].iov_base = this->GetBufPtr() + fBytesSentInBuffer;
        inVec[0].iov_len = amtInBuffer;

		/* 用Socket::Writev()写入数据,送出的数据长度是theLengthSent */
        theErr = fSocket->WriteV(inVec, inNumVectors, &theLengthSent);
        
		/* 假如打印RTSP信息,就输出RTSP Response的指定格式的信息 */
		if (fPrintRTSP)
		{
			/* 类似RTSPRequestStream::ReadRequest() */
			/* 得到当前的GTM时间,格式为"Mon, 04 Nov 1996 21:42:17 GMT",并打印这些信息 */
            DateBuffer theDate;			
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time
 			qtss_printf("\n#S->C:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer() );
 			
			/* 对每个向量,逐行打印其被'\n'或'\r'分割的内容(多个C-String)和'\n'或'\r' */
			for (UInt32 i =0; i < inNumVectors; i++)
			{  
				StrPtrLen str((char*)inVec[i].iov_base, (UInt32) inVec[i].iov_len);
				str.PrintStrEOL();
  			}
 		}

		/* 假如TCPSocket送出的数据超过了剩余要送的数据,将当前指针fCurrentPut移动开头,更新相应量 */
        if (theLengthSent >= amtInBuffer)
        {
            // We were able to send all the data in the buffer. Great(好极了). Flush it.
			/* 根据入参0重置fCurrentPut的位置为fStartPut,即,将当前指针移动开头 */
            this->Reset();
			/* 更新,此时buffer中没有数据要送 */
            fBytesSentInBuffer = 0;
            
            // Make theLengthSent reflect the amount of data sent in the ioVec
			/* 及时更新theLengthSent */
            theLengthSent -= amtInBuffer;
        }
        else /* 还有数据没有送完 */
        {
            // Uh oh. Not all the data in the buffer was sent. Update the
            // fBytesSentInBuffer count, and set theLengthSent to 0.
            
			/* 累计buffer中TCPSocket送出数据的总字节数 */
            fBytesSentInBuffer += theLengthSent;
            Assert(fBytesSentInBuffer < this->GetCurrentOffset());
			/* 以便下次用fSocket->WriteV() */
            theLengthSent = 0;
        }
        // theLengthSent now represents how much data in the ioVec was sent
    }/* 假如当前buffer中没有数据,就从第二个向量开始发送其它向量中的数据,其实际发送数据的长度是theLengthSent */
    else if (inNumVectors > 1)
    {
        theErr = fSocket->WriteV(&inVec[1], inNumVectors - 1, &theLengthSent);
    }

    // We are supposed to refresh the timeout if there is a successful write.
    if (theErr == QTSS_NoErr)
        fTimeoutTask->RefreshTimeout();
        
    // If there was an error, don't alter anything, just bail
	/* 假如Socket::WriteV()结果不是送完或者还要送,就返回实际错误 */
    if ((theErr != QTSS_NoErr) && (theErr != EAGAIN))
        return theErr;
    
    // theLengthSent at this point is the amount of data passed into
    // this function that was sent.
	/* 记录使用fSocket->WriteV()时输出数据的总长度,后面会对这个值更新 */
    if (outLengthSent != NULL)
        *outLengthSent = theLengthSent;

    // Update the StringFormatter fBytesWritten variable... this data
    // wasn't buffered in the output buffer at any time, so if we
    // don't do this, this amount would get lost
	// A way of keeping count of how many bytes have been written total,see StringFormatter.h
	/* 计算共有多少字节被写入,将统计信息加入fBytesWritten,以防丢失 */
    fBytesWritten += theLengthSent;
    
    // All of the data was sent... whew!
	/* 假如所有的数据被发送出,正确返回 */
    if (theLengthSent == inTotalLength)
        return QTSS_NoErr;
    
	/* 下面根据第5个参数决定是否缓存剩余没有发送出的iovec data,假如使用缓存
	(利用StringFormatter::Put()),则这些数据将在下次WriteV时首先发送给client */
    // We need to determine now whether to copy the remaining unsent
    // iovec data into the buffer. This is determined based on
    // the inSendType parameter passed in.

	/* case 1:当发送类型是kDontBuffer,不缓存remaining unsent iovec data */
    if (inSendType == kDontBuffer)
        return theErr;

	/* case 2:当发送类型是kAllOrNothing,且还有数据待发送时,返回EAGAIN */
	/* If no data could be sent, return EWOULDBLOCK. Otherwise,buffer any unsent data. */
    if ((inSendType == kAllOrNothing) && (theLengthSent == 0))
        return EAGAIN;
     
    /* case 3:当发送类型是kAlwaysBuffer,先确定未发送数据长度,再缓存remaining unsent iovec data */

    // Some or none of the iovec data was sent. Copy the remainder into the output
    // buffer.
	/* 注意这是下面一段的思路 */
    
    // The caller should consider this data sent.
	/* 一定要使所有的inVec[]中的数据(长度为inTotalLength)全部发送出 */
    if (outLengthSent != NULL)
        *outLengthSent = inTotalLength;
     
	/* 跳过那些已经送出的数据,更新还要送出数据的量theLengthSent,它已不足某个索引curVec的iov的长度 */
	/* 注意这里的取值是1,没从0开始,因为它已经读了 */
    UInt32 curVec = 1;
    while (theLengthSent >= inVec[curVec].iov_len)
    {
        // Skip over the vectors that were in fact sent.
        Assert(curVec < inNumVectors);
        theLengthSent -= inVec[curVec].iov_len;
        curVec++;
    }
    
	/* 将剩下的不够一个inVec[curVec].iov_len长度的数据(实际大小为inVec[curVec].iov_len - theLengthSent)放入指定缓存,并更新相关量 */
    while (curVec < inNumVectors)
    {
        // Copy the remaining vectors into the buffer
        this->Put(  ((char*)inVec[curVec].iov_base) + theLengthSent,
                    inVec[curVec].iov_len - theLengthSent);
        theLengthSent = 0;//以后的iovec都放入整个长度
        curVec++;       
    }
    return QTSS_NoErr;
}

/* 先统计buffer中还有多少待送出的数据,再用Socket::Send()向外发送数据.只要能够送出数据,就refresh timeout.假如一次全部送出数据,就flush清空它,否则累计已送出的数据,返回EAGAIN */
QTSS_Error RTSPResponseStream::Flush()
{
	/* 得到剩余数据的长度 */
    UInt32 amtInBuffer = this->GetCurrentOffset() - fBytesSentInBuffer;
    if (amtInBuffer > 0)
    {
		/*********** 逐行打印出剩余数据的信息,注意打印信息就是从这里出来的 **************/
        if (fPrintRTSP)
        {
            DateBuffer theDate;
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time

 			qtss_printf("\n#S->C:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer() );
			StrPtrLen str(this->GetBufPtr() + fBytesSentInBuffer, amtInBuffer);
			str.PrintStrEOL();
        }

        UInt32 theLengthSent = 0;
		/* 使用TCPSocket发送出RTSP Response流缓存中剩余的数据,实际发送数据长为theLengthSent */
        (void)fSocket->Send(this->GetBufPtr() + fBytesSentInBuffer, amtInBuffer, &theLengthSent);
       
        // Refresh the timeout if we were able to send any data
		/* 只要能发送出数据,就刷新超时 */
        if (theLengthSent > 0)
            fTimeoutTask->RefreshTimeout();
         
		/* 假如成功发送出所有数据,就重置缓冲区指针和已发送数据长度 */
        if (theLengthSent == amtInBuffer)
        {
            // We were able to send all the data in the buffer. Great. Flush it.
            this->Reset();
            fBytesSentInBuffer = 0;
        }
        else
        {
            // Not all the data was sent, so report an EAGAIN
            /* 否则,累计发送出的数据,返回EAGAIN */
            fBytesSentInBuffer += theLengthSent;
            Assert(fBytesSentInBuffer < this->GetCurrentOffset());
            return EAGAIN;
        }
    }
    return QTSS_NoErr;
}
