/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTSPRequestStream.cpp
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



#include "RTSPRequestStream.h"
#include "StringParser.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "OS.h"
#include "base64.h"

#include <errno.h>

#define READ_DEBUGGING 1



RTSPRequestStream::RTSPRequestStream(TCPSocket* sock)
:   fSocket(sock),
    fRetreatBytes(0), 
    fRetreatBytesRead(0),
    fCurOffset(0),
    fEncodedBytesRemaining(0),
    fRequest(fRequestBuffer, 0),/* 使两者指向相同的地址 */
    fRequestPtr(NULL),
    fDecode(false),/* 非base64 decode */
    fPrintRTSP(false)/* 不打印RTSP info */
{}

/* 从入参指定的RTSPRequestStream中复制失控字节retreat bytes到fRequestBuffer开头处,设置相应数据成员的值 */
void RTSPRequestStream::SnarfRetreat( RTSPRequestStream &fromRequest )
{
    // Simplest thing to do is to just completely blow away everything in this current
    // stream, and replace it with the retreat bytes from the other stream.
    fRequestPtr = NULL;
    Assert(fRetreatBytes < kRequestBufferSizeInBytes);//2048 bytes
	/* 追踪入参失控字节数 */
    fRetreatBytes = fromRequest.fRetreatBytes;
    fEncodedBytesRemaining = fCurOffset = fRequest.Len = 0;
    ::memcpy(&fRequestBuffer[0], fromRequest.fRequest.Ptr + fromRequest.fRequest.Len, fromRequest.fRetreatBytes);
}

/* 非常重要的函数 */
/* 从RTSP Request stream中获取full RTSP Request Header,打印fRequest相关信息,解析得到它的尾部,并配置相应数据成员的值 */
QTSS_Error RTSPRequestStream::ReadRequest()
{
    while (true)
    {
		/* 注意这个量非常重要 */
        UInt32 newOffset = 0;
        
        //If this is the case, we already HAVE a request on this session, and we now are done
        //with the request and want to move onto the next one. The first thing we should do
        //is check whether there is any lingering(延迟的) data in the stream. If there is, the parent
        //session believes that is part of a new request
        if (fRequestPtr != NULL)
        {
			/* 标记不再有complete RTSP client Request  */
            fRequestPtr = NULL;//flag that we no longer have a complete request
            
            // Take all the retreated leftover(剩余的) data and move it to the beginning of the buffer
            if ((fRetreatBytes > 0) && (fRequest.Len > 0))
                ::memmove(fRequest.Ptr, fRequest.Ptr + fRequest.Len + fRetreatBytesRead/* NOTE! */, fRetreatBytes);

            // if we are decoding, we need to also move over the remaining encoded bytes
            // to the right position in the fRequestBuffer
            if (fEncodedBytesRemaining > 0)
            {
				/* 参见RTSPRequestStream::DecodeIncomingData() */
                //Assert(fEncodedBytesRemaining < 4);
                
                // The right position is at fRetreatBytes offset in the request buffer. The reason for this is:
                //  1) We need to find a place in the request buffer where we know we have enough space to store
                //  fEncodedBytesRemaining. fRetreatBytes + fEncodedBytesRemaining will always be less than
                //  kRequestBufferSize because all this data must have been in the same request buffer, together, at one point.
                //
                //  2) We need to make sure that there is always more data in the RequestBuffer than in the decoded
                //  request buffer, otherwise we could overrun(超出限度) the decoded request buffer (we bounds check on the encoded
                //  buffer, not the decoded buffer). Leaving fRetreatBytes as empty space in the request buffer ensures
                //  that this principle is maintained. 
                ::memmove(&fRequestBuffer[fRetreatBytes], &fRequestBuffer[fCurOffset - fEncodedBytesRemaining], fEncodedBytesRemaining);
                fCurOffset = fRetreatBytes + fEncodedBytesRemaining;
                Assert(fCurOffset < kRequestBufferSizeInBytes);
            }
            else
                fCurOffset = fRetreatBytes;
                
            newOffset = fRequest.Len = fRetreatBytes;
			/* 因为已将Retreat data移位了 */
            fRetreatBytes = fRetreatBytesRead = 0;
        }

        // We don't have any new data, so try and get some
		/* 以下操作是确保newOffset>0 */
        if (newOffset == 0)
        {
            if (fRetreatBytes > 0)
            {
                // This will be true if we've just snarfed(复制) another input stream, in which case the encoded data
                // is copied into our request buffer, and its length is tracked(追踪) in fRetreatBytes.
                // If this is true, just fall through(进行下去) and decode the data.
                newOffset = fRetreatBytes;
                fRetreatBytes = 0;
                Assert(fEncodedBytesRemaining == 0);
            }
            else
            {
                // We don't have any new data, get some from the socket...
				/* 利用TCPSocket的::recv()来接收数据,第一,二个参数是缓存地址和长度,第三个参数是接收数据的长度 */
                QTSS_Error sockErr = fSocket->Read(&fRequestBuffer[fCurOffset], 
                                                    (kRequestBufferSizeInBytes - fCurOffset) - 1, &newOffset);
                //assume the client is dead if we get an error back
				/* 目前缓冲区无数据可读 */
				/* 当recv系统调用返回这个值时表示recv读数据时，对方没有发送数据过来。 */
                if (sockErr == EAGAIN)
                    return QTSS_NoErr;
				/* 假如返回出错,一定是连接断开 */
                if (sockErr != QTSS_NoErr)
                {
                    Assert(!fSocket->IsConnected());
                    return sockErr;
                }
            }   

			/* 当要base64 decode时 */
            if (fDecode)
            {
                // If we need to decode this data, do it now.
				/* 确保数据偏移量大于内存中需要decode的数据 */
                Assert(fCurOffset >= fEncodedBytesRemaining);
				/* 解密读进来的数据 */
                QTSS_Error decodeErr = this->DecodeIncomingData(&fRequestBuffer[fCurOffset - fEncodedBytesRemaining],
                                                                    newOffset + fEncodedBytesRemaining);
                // If the above function returns an error, it is because we've
                // encountered some non-base64 data in the stream. We can process
                // everything up until that point, but all data after this point will
                // be ignored.
				/* 解码成功时,确保最后剩下的未解码的数据少于4个字节,参见下面的RTSPRequestStream::DecodeIncomingData() */
                if (decodeErr == QTSS_NoErr)
                    Assert(fEncodedBytesRemaining < 4);
            }
            else
                fRequest.Len += newOffset;

            Assert(fRequest.Len < kRequestBufferSizeInBytes);
            fCurOffset += newOffset;
        }
		/* 最后一定要达到这个结论! */
        Assert(newOffset > 0);

        // See if this is an interleaved data packet
		/* 查找data packet,配置fIsDataPacket的值 */
		/* 找到data packet */
        if ('$' == *(fRequest.Ptr))
        {   
            if (fRequest.Len < 4)
                continue;
            UInt16* dataLenP = (UInt16*)fRequest.Ptr;
			/* 获取混叠包长度 */
            UInt32 interleavedPacketLen = ntohs(dataLenP[1]) + 4;
            if (interleavedPacketLen > fRequest.Len)
                continue;
                
            //put back any data that is not part of the header
			/* 将不是header部分的数据放回失控数据处 */
            fRetreatBytes += fRequest.Len - interleavedPacketLen;
            fRequest.Len = interleavedPacketLen;
        
            fRequestPtr = &fRequest;
            fIsDataPacket = true;
            return QTSS_RequestArrived;
        }
        fIsDataPacket = false;

		/* 当打印RTSP info时,打印出如下信息:
		
		   (空两行)
		   #C->S;
		   #time: ms=***  data=Mon,29 July 2009 15:17:17 GTM
		   #server: ip=172.16.32.37 port=***
	     或#server: ip=NULL port=***
		   #client: ip=172.16.39.30 port=***
	     或#client: ip=NULL port=***
		   *********fRequest info***************
		*/
        if (fPrintRTSP)
        {
			/* 类似RTSPResponseStream::WriteV() */
			/* 得到当前的GTM时间,格式为"Mon, 04 Nov 1996 21:42:17 GMT" */
            DateBuffer theDate;
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time
			/* OS::StartTimeMilli_Int()表示服务器运行多长时间? */
			qtss_printf("\n\n#C->S:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer());

			/* 假如有TCPSocket存在,就获取并打印其相关信息 */
            if (fSocket != NULL)    
            {
                UInt16 serverPort = fSocket->GetLocalPort();
                UInt16 clientPort = fSocket->GetRemotePort();    
                StrPtrLen* theLocalAddrStr = fSocket->GetLocalAddrStr();
                StrPtrLen* theRemoteAddrStr = fSocket->GetRemoteAddrStr();
                if (theLocalAddrStr != NULL)
                {	qtss_printf("#server: ip="); theLocalAddrStr->PrintStr(); qtss_printf(" port=%u\n" , serverPort );
                }
                else
              	{	qtss_printf("#server: ip=NULL port=%u\n" , serverPort );
              	}
               	
                if (theRemoteAddrStr != NULL)
                {	qtss_printf("#client: ip="); theRemoteAddrStr->PrintStr(); qtss_printf(" port=%u\n" , clientPort );
                }
            	else
            	{	qtss_printf("#client: ip=NULL port=%u\n" , clientPort );
            	}

            }

			/* 打印fRequest字符串 */
			StrPtrLen str(fRequest);
			str.PrintStrEOL("\n\r\n", "\n");// print the request but stop on \n\r\n and add a \n afterwards.
        }
        
        //use a StringParser object to search for a double EOL, which signifies the end of
        //the header.
		/* 下面要查找client发出的RTSP Request Header的末尾处 */

		/* 找到fRequest末端了吗? */
        Bool16 weAreDone = false;
        StringParser headerParser(&fRequest);
        
        UInt16 lcount = 0;
		/* 当遇到eol时,fStartGet指针越过它 */
        while (headerParser.GetThruEOL(NULL))
        {
            lcount++;
			/* 当当前的fStartGet指针指向EOL时 */
            if (headerParser.ExpectEOL())
            {
                //The legal end-of-header sequences are \r\r, \r\n\r\n, & \n\n. NOT \r\n\r!
                //If the packets arrive just a certain way, we could get here with the latter
                //combo(组合,即\r\n\r), and not wait for a final \n.
				/* 假如查找到"\r\n\r",就继续查找,直至找到符合条件的末端 */
                if ((headerParser.GetDataParsedLen() > 2) &&
                    (memcmp(headerParser.GetCurrentPosition() - 3, "\r\n\r", 3) == 0))
                    continue;
				/* 标记找到了 */
                weAreDone = true;
                break;
            }
			/* 假如是"xxxxxx\r" */
            else if (lcount == 1) {
                // if this request is actually a ShoutCast password it will be 
                // in the form of "xxxxxx\r" where "xxxxx" is the password.
                // If we get a 1st request line ending in \r with no blanks we will
                // assume that this is the end of the request.
                UInt16 flag = 0;
                UInt16 i = 0;
				/* 检查"xxxxxx\r"中有无空格?没有就说明我们找到了 */
                for (i=0; i<fRequest.Len; i++)
                {
                    if (fRequest.Ptr[i] == ' ')
                        flag++;
                }
                if (flag == 0)
                {
                    weAreDone = true;
                    break;
                }
            }
        }
        
        //weAreDone means we have gotten a full request
		/* 我们找到了一个Full RTSP Request Header? */
        if (weAreDone)
        {
            //put back any data that is not part of the header
            fRequest.Len -= headerParser.GetDataRemaining();
            fRetreatBytes += headerParser.GetDataRemaining();
            
            fRequestPtr = &fRequest;
            return QTSS_RequestArrived;
        }
        
        //check for a full buffer
		/* 当到达Request buffer末端时,给出提示信息E2BIG */
        if (fCurOffset == kRequestBufferSizeInBytes - 1)
        {
            fRequestPtr = &fRequest;
            return E2BIG;
        }
    }
}

/* 存放数据到指定长度的缓存,分两步:若存在失控数据时,首先读取它直至填满入参inBufLen;若还没有填满缓存,就从Socket直接读取数据来填满指定的长度,第三个入参指出了最后一次读取数据的总长度(应该是inBufLen) */
QTSS_Error RTSPRequestStream::Read(void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead)
{
	/* 注意这个量记录了最后一次读进缓存的不超过inBufLen的长度 */
    UInt32 theLengthRead = 0;
	/* 记录并转换入参的类型 */
    UInt8* theIoBuffer = (UInt8*)ioBuffer;
    
    //
    // If there are retreat bytes available, read them first.
	/* 当存在失控数据时,首先读取它直至填满入参inBufLen,并及时更新fRetreatBytes和fRetreatBytesRead */
    if (fRetreatBytes > 0)
    {
        theLengthRead = fRetreatBytes;
		/* 注意在这种情况下,入参theIoBuffer内的值会被多次覆盖!! */
        if (inBufLen < theLengthRead)
            theLengthRead = inBufLen;
        
		/* 注意,考虑到循环,这里加上了fRetreatBytesRead */
        ::memcpy(theIoBuffer, fRequest.Ptr + fRequest.Len + fRetreatBytesRead, theLengthRead);
        
        //
        // We should not update fRequest.Len even though we've read some of the retreat bytes.
        // fRequest.Len always refers to the length of the request header. Instead, we
        // have a separate variable, fRetreatBytesRead
        fRetreatBytes -= theLengthRead;
        fRetreatBytesRead += theLengthRead;
#if READ_DEBUGGING
        qtss_printf("In RTSPRequestStream::Read: Got %d Retreat Bytes\n",theLengthRead);
#endif  
    }

    //
    // If there is still space available in ioBuffer, continue. Otherwise, we can return now
    if (theLengthRead == inBufLen)
    {
        if (outLengthRead != NULL)
			/* 第三个入参是inBufLen */
            *outLengthRead = theLengthRead;
        return QTSS_NoErr;
    }
    
    //
    // Read data directly from the socket and place it in our buffer
    UInt32 theNewOffset = 0;
	/* 若入参ioBuffer还有空间,就直接从Socket读取数据来填满指定的长度,第三个入参指出了从Socket读取数据的长度 */
    QTSS_Error theErr = fSocket->Read(&theIoBuffer[theLengthRead], inBufLen - theLengthRead, &theNewOffset);
#if READ_DEBUGGING
    qtss_printf("In RTSPRequestStream::Read: Got %d bytes off Socket\n",theNewOffset);
#endif  
	/* 更新第三个参数 */
    if (outLengthRead != NULL)
        *outLengthRead = theNewOffset + theLengthRead;
        
    return theErr;
}

/* 利用Base64decode()来decode入参中指定的数据,保留最后两个bit上的值不decode */
QTSS_Error RTSPRequestStream::DecodeIncomingData(char* inSrcData, UInt32 inSrcDataLen)
{
	/* 确保没有失控数据 */
    Assert(fRetreatBytes == 0);
    
    if (fRequest.Ptr == &fRequestBuffer[0])
    {
        fRequest.Ptr = NEW char[kRequestBufferSizeInBytes];
        fRequest.Len = 0;
    }
    
    // We always decode up through the last chunk of 4.
	/* 保留最后两个bit上的值(<=3) */
    fEncodedBytesRemaining = inSrcDataLen & 3;
    
    // Let our friendly Base64Decode function know this by NULL terminating at that point
	/* 要解码的字节数 */
    UInt32 bytesToDecode = inSrcDataLen - fEncodedBytesRemaining;
	/* 保存下来后面要用到 */
    char endChar = inSrcData[bytesToDecode];
	/* 用"\0"标记为了让Base64decode()知道 */
    inSrcData[bytesToDecode] = '\0';
    
	/* 用于统计解码后的编码字节数(使最后两个bit为0) */
    UInt32 encodedBytesConsumed = 0;
    
    // Loop until the whole load is decoded
    while (encodedBytesConsumed < bytesToDecode)
    {
		/* 由后面的设置,都是4的倍数,显然也是满足的 */
        Assert((encodedBytesConsumed & 3) == 0);
		/* 这个量显然满足! */
        Assert((bytesToDecode & 3) == 0);

		/* 将第二个参数中的数据base64 decode,再将数据存到第一个参数上,函数返回decode后的字节数 */
        UInt32 bytesDecoded = Base64decode(fRequest.Ptr + fRequest.Len, inSrcData + encodedBytesConsumed);

        // If bytesDecoded is 0, we will end up being in an endless loop. The
        // base64 must be corrupt, so let's just return an error and abort
		/* 若上函数返回0,一定是造破坏了,必须返回error并退出循环 */
        if (bytesDecoded == 0)
        {
            //Assert(0);
            return QTSS_BadArgument;
        }
        
		/** 及时更新长度 **/
        fRequest.Len += bytesDecoded;

        // Assuming the stream is valid, the # of encoded bytes we just consumed is
        // 4/3rds of the number of decoded bytes returned by the decode function Base64decode(),
        // rounded up(四舍五入) to the nearest multiple of 4.
        encodedBytesConsumed += (bytesDecoded / 3) * 4;
		/* 若不能被3整除 */
        if ((bytesDecoded % 3) > 0)
            encodedBytesConsumed += 4;
        
    }
    
    // Make sure to replace the sacred endChar
    inSrcData[bytesToDecode] = endChar;

    Assert(fRequest.Len < kRequestBufferSizeInBytes);
	/* 确信将所有需要解码的字节处理完 */
    Assert(encodedBytesConsumed == bytesToDecode);
    
    return QTSS_NoErr;
}

