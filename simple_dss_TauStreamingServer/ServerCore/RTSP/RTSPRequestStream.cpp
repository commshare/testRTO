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
    fRequest(fRequestBuffer, 0),/* ʹ����ָ����ͬ�ĵ�ַ */
    fRequestPtr(NULL),
    fDecode(false),/* ��base64 decode */
    fPrintRTSP(false)/* ����ӡRTSP info */
{}

/* �����ָ����RTSPRequestStream�и���ʧ���ֽ�retreat bytes��fRequestBuffer��ͷ��,������Ӧ���ݳ�Ա��ֵ */
void RTSPRequestStream::SnarfRetreat( RTSPRequestStream &fromRequest )
{
    // Simplest thing to do is to just completely blow away everything in this current
    // stream, and replace it with the retreat bytes from the other stream.
    fRequestPtr = NULL;
    Assert(fRetreatBytes < kRequestBufferSizeInBytes);//2048 bytes
	/* ׷�����ʧ���ֽ��� */
    fRetreatBytes = fromRequest.fRetreatBytes;
    fEncodedBytesRemaining = fCurOffset = fRequest.Len = 0;
    ::memcpy(&fRequestBuffer[0], fromRequest.fRequest.Ptr + fromRequest.fRequest.Len, fromRequest.fRetreatBytes);
}

/* �ǳ���Ҫ�ĺ��� */
/* ��RTSP Request stream�л�ȡfull RTSP Request Header,��ӡfRequest�����Ϣ,�����õ�����β��,��������Ӧ���ݳ�Ա��ֵ */
QTSS_Error RTSPRequestStream::ReadRequest()
{
    while (true)
    {
		/* ע��������ǳ���Ҫ */
        UInt32 newOffset = 0;
        
        //If this is the case, we already HAVE a request on this session, and we now are done
        //with the request and want to move onto the next one. The first thing we should do
        //is check whether there is any lingering(�ӳٵ�) data in the stream. If there is, the parent
        //session believes that is part of a new request
        if (fRequestPtr != NULL)
        {
			/* ��ǲ�����complete RTSP client Request  */
            fRequestPtr = NULL;//flag that we no longer have a complete request
            
            // Take all the retreated leftover(ʣ���) data and move it to the beginning of the buffer
            if ((fRetreatBytes > 0) && (fRequest.Len > 0))
                ::memmove(fRequest.Ptr, fRequest.Ptr + fRequest.Len + fRetreatBytesRead/* NOTE! */, fRetreatBytes);

            // if we are decoding, we need to also move over the remaining encoded bytes
            // to the right position in the fRequestBuffer
            if (fEncodedBytesRemaining > 0)
            {
				/* �μ�RTSPRequestStream::DecodeIncomingData() */
                //Assert(fEncodedBytesRemaining < 4);
                
                // The right position is at fRetreatBytes offset in the request buffer. The reason for this is:
                //  1) We need to find a place in the request buffer where we know we have enough space to store
                //  fEncodedBytesRemaining. fRetreatBytes + fEncodedBytesRemaining will always be less than
                //  kRequestBufferSize because all this data must have been in the same request buffer, together, at one point.
                //
                //  2) We need to make sure that there is always more data in the RequestBuffer than in the decoded
                //  request buffer, otherwise we could overrun(�����޶�) the decoded request buffer (we bounds check on the encoded
                //  buffer, not the decoded buffer). Leaving fRetreatBytes as empty space in the request buffer ensures
                //  that this principle is maintained. 
                ::memmove(&fRequestBuffer[fRetreatBytes], &fRequestBuffer[fCurOffset - fEncodedBytesRemaining], fEncodedBytesRemaining);
                fCurOffset = fRetreatBytes + fEncodedBytesRemaining;
                Assert(fCurOffset < kRequestBufferSizeInBytes);
            }
            else
                fCurOffset = fRetreatBytes;
                
            newOffset = fRequest.Len = fRetreatBytes;
			/* ��Ϊ�ѽ�Retreat data��λ�� */
            fRetreatBytes = fRetreatBytesRead = 0;
        }

        // We don't have any new data, so try and get some
		/* ���²�����ȷ��newOffset>0 */
        if (newOffset == 0)
        {
            if (fRetreatBytes > 0)
            {
                // This will be true if we've just snarfed(����) another input stream, in which case the encoded data
                // is copied into our request buffer, and its length is tracked(׷��) in fRetreatBytes.
                // If this is true, just fall through(������ȥ) and decode the data.
                newOffset = fRetreatBytes;
                fRetreatBytes = 0;
                Assert(fEncodedBytesRemaining == 0);
            }
            else
            {
                // We don't have any new data, get some from the socket...
				/* ����TCPSocket��::recv()����������,��һ,���������ǻ����ַ�ͳ���,�����������ǽ������ݵĳ��� */
                QTSS_Error sockErr = fSocket->Read(&fRequestBuffer[fCurOffset], 
                                                    (kRequestBufferSizeInBytes - fCurOffset) - 1, &newOffset);
                //assume the client is dead if we get an error back
				/* Ŀǰ�����������ݿɶ� */
				/* ��recvϵͳ���÷������ֵʱ��ʾrecv������ʱ���Է�û�з������ݹ����� */
                if (sockErr == EAGAIN)
                    return QTSS_NoErr;
				/* ���緵�س���,һ�������ӶϿ� */
                if (sockErr != QTSS_NoErr)
                {
                    Assert(!fSocket->IsConnected());
                    return sockErr;
                }
            }   

			/* ��Ҫbase64 decodeʱ */
            if (fDecode)
            {
                // If we need to decode this data, do it now.
				/* ȷ������ƫ���������ڴ�����Ҫdecode������ */
                Assert(fCurOffset >= fEncodedBytesRemaining);
				/* ���ܶ����������� */
                QTSS_Error decodeErr = this->DecodeIncomingData(&fRequestBuffer[fCurOffset - fEncodedBytesRemaining],
                                                                    newOffset + fEncodedBytesRemaining);
                // If the above function returns an error, it is because we've
                // encountered some non-base64 data in the stream. We can process
                // everything up until that point, but all data after this point will
                // be ignored.
				/* ����ɹ�ʱ,ȷ�����ʣ�µ�δ�������������4���ֽ�,�μ������RTSPRequestStream::DecodeIncomingData() */
                if (decodeErr == QTSS_NoErr)
                    Assert(fEncodedBytesRemaining < 4);
            }
            else
                fRequest.Len += newOffset;

            Assert(fRequest.Len < kRequestBufferSizeInBytes);
            fCurOffset += newOffset;
        }
		/* ���һ��Ҫ�ﵽ�������! */
        Assert(newOffset > 0);

        // See if this is an interleaved data packet
		/* ����data packet,����fIsDataPacket��ֵ */
		/* �ҵ�data packet */
        if ('$' == *(fRequest.Ptr))
        {   
            if (fRequest.Len < 4)
                continue;
            UInt16* dataLenP = (UInt16*)fRequest.Ptr;
			/* ��ȡ��������� */
            UInt32 interleavedPacketLen = ntohs(dataLenP[1]) + 4;
            if (interleavedPacketLen > fRequest.Len)
                continue;
                
            //put back any data that is not part of the header
			/* ������header���ֵ����ݷŻ�ʧ�����ݴ� */
            fRetreatBytes += fRequest.Len - interleavedPacketLen;
            fRequest.Len = interleavedPacketLen;
        
            fRequestPtr = &fRequest;
            fIsDataPacket = true;
            return QTSS_RequestArrived;
        }
        fIsDataPacket = false;

		/* ����ӡRTSP infoʱ,��ӡ��������Ϣ:
		
		   (������)
		   #C->S;
		   #time: ms=***  data=Mon,29 July 2009 15:17:17 GTM
		   #server: ip=172.16.32.37 port=***
	     ��#server: ip=NULL port=***
		   #client: ip=172.16.39.30 port=***
	     ��#client: ip=NULL port=***
		   *********fRequest info***************
		*/
        if (fPrintRTSP)
        {
			/* ����RTSPResponseStream::WriteV() */
			/* �õ���ǰ��GTMʱ��,��ʽΪ"Mon, 04 Nov 1996 21:42:17 GMT" */
            DateBuffer theDate;
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time
			/* OS::StartTimeMilli_Int()��ʾ���������ж೤ʱ��? */
			qtss_printf("\n\n#C->S:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer());

			/* ������TCPSocket����,�ͻ�ȡ����ӡ�������Ϣ */
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

			/* ��ӡfRequest�ַ��� */
			StrPtrLen str(fRequest);
			str.PrintStrEOL("\n\r\n", "\n");// print the request but stop on \n\r\n and add a \n afterwards.
        }
        
        //use a StringParser object to search for a double EOL, which signifies the end of
        //the header.
		/* ����Ҫ����client������RTSP Request Header��ĩβ�� */

		/* �ҵ�fRequestĩ������? */
        Bool16 weAreDone = false;
        StringParser headerParser(&fRequest);
        
        UInt16 lcount = 0;
		/* ������eolʱ,fStartGetָ��Խ���� */
        while (headerParser.GetThruEOL(NULL))
        {
            lcount++;
			/* ����ǰ��fStartGetָ��ָ��EOLʱ */
            if (headerParser.ExpectEOL())
            {
                //The legal end-of-header sequences are \r\r, \r\n\r\n, & \n\n. NOT \r\n\r!
                //If the packets arrive just a certain way, we could get here with the latter
                //combo(���,��\r\n\r), and not wait for a final \n.
				/* ������ҵ�"\r\n\r",�ͼ�������,ֱ���ҵ�����������ĩ�� */
                if ((headerParser.GetDataParsedLen() > 2) &&
                    (memcmp(headerParser.GetCurrentPosition() - 3, "\r\n\r", 3) == 0))
                    continue;
				/* ����ҵ��� */
                weAreDone = true;
                break;
            }
			/* ������"xxxxxx\r" */
            else if (lcount == 1) {
                // if this request is actually a ShoutCast password it will be 
                // in the form of "xxxxxx\r" where "xxxxx" is the password.
                // If we get a 1st request line ending in \r with no blanks we will
                // assume that this is the end of the request.
                UInt16 flag = 0;
                UInt16 i = 0;
				/* ���"xxxxxx\r"�����޿ո�?û�о�˵�������ҵ��� */
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
		/* �����ҵ���һ��Full RTSP Request Header? */
        if (weAreDone)
        {
            //put back any data that is not part of the header
            fRequest.Len -= headerParser.GetDataRemaining();
            fRetreatBytes += headerParser.GetDataRemaining();
            
            fRequestPtr = &fRequest;
            return QTSS_RequestArrived;
        }
        
        //check for a full buffer
		/* ������Request bufferĩ��ʱ,������ʾ��ϢE2BIG */
        if (fCurOffset == kRequestBufferSizeInBytes - 1)
        {
            fRequestPtr = &fRequest;
            return E2BIG;
        }
    }
}

/* ������ݵ�ָ�����ȵĻ���,������:������ʧ������ʱ,���ȶ�ȡ��ֱ���������inBufLen;����û����������,�ʹ�Socketֱ�Ӷ�ȡ����������ָ���ĳ���,���������ָ�������һ�ζ�ȡ���ݵ��ܳ���(Ӧ����inBufLen) */
QTSS_Error RTSPRequestStream::Read(void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead)
{
	/* ע���������¼�����һ�ζ�������Ĳ�����inBufLen�ĳ��� */
    UInt32 theLengthRead = 0;
	/* ��¼��ת����ε����� */
    UInt8* theIoBuffer = (UInt8*)ioBuffer;
    
    //
    // If there are retreat bytes available, read them first.
	/* ������ʧ������ʱ,���ȶ�ȡ��ֱ���������inBufLen,����ʱ����fRetreatBytes��fRetreatBytesRead */
    if (fRetreatBytes > 0)
    {
        theLengthRead = fRetreatBytes;
		/* ע�������������,���theIoBuffer�ڵ�ֵ�ᱻ��θ���!! */
        if (inBufLen < theLengthRead)
            theLengthRead = inBufLen;
        
		/* ע��,���ǵ�ѭ��,���������fRetreatBytesRead */
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
			/* �����������inBufLen */
            *outLengthRead = theLengthRead;
        return QTSS_NoErr;
    }
    
    //
    // Read data directly from the socket and place it in our buffer
    UInt32 theNewOffset = 0;
	/* �����ioBuffer���пռ�,��ֱ�Ӵ�Socket��ȡ����������ָ���ĳ���,���������ָ���˴�Socket��ȡ���ݵĳ��� */
    QTSS_Error theErr = fSocket->Read(&theIoBuffer[theLengthRead], inBufLen - theLengthRead, &theNewOffset);
#if READ_DEBUGGING
    qtss_printf("In RTSPRequestStream::Read: Got %d bytes off Socket\n",theNewOffset);
#endif  
	/* ���µ��������� */
    if (outLengthRead != NULL)
        *outLengthRead = theNewOffset + theLengthRead;
        
    return theErr;
}

/* ����Base64decode()��decode�����ָ��������,�����������bit�ϵ�ֵ��decode */
QTSS_Error RTSPRequestStream::DecodeIncomingData(char* inSrcData, UInt32 inSrcDataLen)
{
	/* ȷ��û��ʧ������ */
    Assert(fRetreatBytes == 0);
    
    if (fRequest.Ptr == &fRequestBuffer[0])
    {
        fRequest.Ptr = NEW char[kRequestBufferSizeInBytes];
        fRequest.Len = 0;
    }
    
    // We always decode up through the last chunk of 4.
	/* �����������bit�ϵ�ֵ(<=3) */
    fEncodedBytesRemaining = inSrcDataLen & 3;
    
    // Let our friendly Base64Decode function know this by NULL terminating at that point
	/* Ҫ������ֽ��� */
    UInt32 bytesToDecode = inSrcDataLen - fEncodedBytesRemaining;
	/* ������������Ҫ�õ� */
    char endChar = inSrcData[bytesToDecode];
	/* ��"\0"���Ϊ����Base64decode()֪�� */
    inSrcData[bytesToDecode] = '\0';
    
	/* ����ͳ�ƽ����ı����ֽ���(ʹ�������bitΪ0) */
    UInt32 encodedBytesConsumed = 0;
    
    // Loop until the whole load is decoded
    while (encodedBytesConsumed < bytesToDecode)
    {
		/* �ɺ��������,����4�ı���,��ȻҲ������� */
        Assert((encodedBytesConsumed & 3) == 0);
		/* �������Ȼ����! */
        Assert((bytesToDecode & 3) == 0);

		/* ���ڶ��������е�����base64 decode,�ٽ����ݴ浽��һ��������,��������decode����ֽ��� */
        UInt32 bytesDecoded = Base64decode(fRequest.Ptr + fRequest.Len, inSrcData + encodedBytesConsumed);

        // If bytesDecoded is 0, we will end up being in an endless loop. The
        // base64 must be corrupt, so let's just return an error and abort
		/* ���Ϻ�������0,һ�������ƻ���,���뷵��error���˳�ѭ�� */
        if (bytesDecoded == 0)
        {
            //Assert(0);
            return QTSS_BadArgument;
        }
        
		/** ��ʱ���³��� **/
        fRequest.Len += bytesDecoded;

        // Assuming the stream is valid, the # of encoded bytes we just consumed is
        // 4/3rds of the number of decoded bytes returned by the decode function Base64decode(),
        // rounded up(��������) to the nearest multiple of 4.
        encodedBytesConsumed += (bytesDecoded / 3) * 4;
		/* �����ܱ�3���� */
        if ((bytesDecoded % 3) > 0)
            encodedBytesConsumed += 4;
        
    }
    
    // Make sure to replace the sacred endChar
    inSrcData[bytesToDecode] = endChar;

    Assert(fRequest.Len < kRequestBufferSizeInBytes);
	/* ȷ�Ž�������Ҫ������ֽڴ����� */
    Assert(encodedBytesConsumed == bytesToDecode);
    
    return QTSS_NoErr;
}

