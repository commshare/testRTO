
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
/* ʹ��writev(),��RTSP Response���Ļ��������д�������,���Ƚ��䷢�ͳ�ȥ,�������������������ݷ��ͳ�,
��ˢ�³�ʱ����,���ȽϷ��͵��ܳ���outLengthSent��Ҫ���������ܳ���inTotalLength�Ĳ�ֵ,���ö�ʣ������
�Լ�����iovec���뻺����������,���ݵ�5�����������Ƿ񻺴�(����StringFormatter::Put())û�з��ͳ���iovec data?
*/
QTSS_Error RTSPResponseStream::WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength,
                                            UInt32* outLengthSent, UInt32 inSendType)
{
    QTSS_Error theErr = QTSS_NoErr;
	/* ��TCPSocket�Ѿ��ͳ����ݵĳ���,ע�������ʮ����Ҫ */
    UInt32 theLengthSent = 0;
	/* ������ʣ��data���ֽ��� */
    UInt32 amtInBuffer = this->GetCurrentOffset() - fBytesSentInBuffer;
    
	/* ����buffer�л���ʣ������,�Ͱ�����Socketд��RTSP Response���� */
    if (amtInBuffer > 0)
    {

        // There is some data in the output buffer. Make sure to send that
        // first, using the empty space in the ioVec.
         
		/* ȷ��buffer��ʣ�����ݵ����ͳ���,����ΪinVec�ĵ�һ������ */
        inVec[0].iov_base = this->GetBufPtr() + fBytesSentInBuffer;
        inVec[0].iov_len = amtInBuffer;

		/* ��Socket::Writev()д������,�ͳ������ݳ�����theLengthSent */
        theErr = fSocket->WriteV(inVec, inNumVectors, &theLengthSent);
        
		/* �����ӡRTSP��Ϣ,�����RTSP Response��ָ����ʽ����Ϣ */
		if (fPrintRTSP)
		{
			/* ����RTSPRequestStream::ReadRequest() */
			/* �õ���ǰ��GTMʱ��,��ʽΪ"Mon, 04 Nov 1996 21:42:17 GMT",����ӡ��Щ��Ϣ */
            DateBuffer theDate;			
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time
 			qtss_printf("\n#S->C:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer() );
 			
			/* ��ÿ������,���д�ӡ�䱻'\n'��'\r'�ָ������(���C-String)��'\n'��'\r' */
			for (UInt32 i =0; i < inNumVectors; i++)
			{  
				StrPtrLen str((char*)inVec[i].iov_base, (UInt32) inVec[i].iov_len);
				str.PrintStrEOL();
  			}
 		}

		/* ����TCPSocket�ͳ������ݳ�����ʣ��Ҫ�͵�����,����ǰָ��fCurrentPut�ƶ���ͷ,������Ӧ�� */
        if (theLengthSent >= amtInBuffer)
        {
            // We were able to send all the data in the buffer. Great(�ü���). Flush it.
			/* �������0����fCurrentPut��λ��ΪfStartPut,��,����ǰָ���ƶ���ͷ */
            this->Reset();
			/* ����,��ʱbuffer��û������Ҫ�� */
            fBytesSentInBuffer = 0;
            
            // Make theLengthSent reflect the amount of data sent in the ioVec
			/* ��ʱ����theLengthSent */
            theLengthSent -= amtInBuffer;
        }
        else /* ��������û������ */
        {
            // Uh oh. Not all the data in the buffer was sent. Update the
            // fBytesSentInBuffer count, and set theLengthSent to 0.
            
			/* �ۼ�buffer��TCPSocket�ͳ����ݵ����ֽ��� */
            fBytesSentInBuffer += theLengthSent;
            Assert(fBytesSentInBuffer < this->GetCurrentOffset());
			/* �Ա��´���fSocket->WriteV() */
            theLengthSent = 0;
        }
        // theLengthSent now represents how much data in the ioVec was sent
    }/* ���統ǰbuffer��û������,�ʹӵڶ���������ʼ�������������е�����,��ʵ�ʷ������ݵĳ�����theLengthSent */
    else if (inNumVectors > 1)
    {
        theErr = fSocket->WriteV(&inVec[1], inNumVectors - 1, &theLengthSent);
    }

    // We are supposed to refresh the timeout if there is a successful write.
    if (theErr == QTSS_NoErr)
        fTimeoutTask->RefreshTimeout();
        
    // If there was an error, don't alter anything, just bail
	/* ����Socket::WriteV()�������������߻�Ҫ��,�ͷ���ʵ�ʴ��� */
    if ((theErr != QTSS_NoErr) && (theErr != EAGAIN))
        return theErr;
    
    // theLengthSent at this point is the amount of data passed into
    // this function that was sent.
	/* ��¼ʹ��fSocket->WriteV()ʱ������ݵ��ܳ���,���������ֵ���� */
    if (outLengthSent != NULL)
        *outLengthSent = theLengthSent;

    // Update the StringFormatter fBytesWritten variable... this data
    // wasn't buffered in the output buffer at any time, so if we
    // don't do this, this amount would get lost
	// A way of keeping count of how many bytes have been written total,see StringFormatter.h
	/* ���㹲�ж����ֽڱ�д��,��ͳ����Ϣ����fBytesWritten,�Է���ʧ */
    fBytesWritten += theLengthSent;
    
    // All of the data was sent... whew!
	/* �������е����ݱ����ͳ�,��ȷ���� */
    if (theLengthSent == inTotalLength)
        return QTSS_NoErr;
    
	/* ������ݵ�5�����������Ƿ񻺴�ʣ��û�з��ͳ���iovec data,����ʹ�û���
	(����StringFormatter::Put()),����Щ���ݽ����´�WriteVʱ���ȷ��͸�client */
    // We need to determine now whether to copy the remaining unsent
    // iovec data into the buffer. This is determined based on
    // the inSendType parameter passed in.

	/* case 1:������������kDontBuffer,������remaining unsent iovec data */
    if (inSendType == kDontBuffer)
        return theErr;

	/* case 2:������������kAllOrNothing,�һ������ݴ�����ʱ,����EAGAIN */
	/* If no data could be sent, return EWOULDBLOCK. Otherwise,buffer any unsent data. */
    if ((inSendType == kAllOrNothing) && (theLengthSent == 0))
        return EAGAIN;
     
    /* case 3:������������kAlwaysBuffer,��ȷ��δ�������ݳ���,�ٻ���remaining unsent iovec data */

    // Some or none of the iovec data was sent. Copy the remainder into the output
    // buffer.
	/* ע����������һ�ε�˼· */
    
    // The caller should consider this data sent.
	/* һ��Ҫʹ���е�inVec[]�е�����(����ΪinTotalLength)ȫ�����ͳ� */
    if (outLengthSent != NULL)
        *outLengthSent = inTotalLength;
     
	/* ������Щ�Ѿ��ͳ�������,���»�Ҫ�ͳ����ݵ���theLengthSent,���Ѳ���ĳ������curVec��iov�ĳ��� */
	/* ע�������ȡֵ��1,û��0��ʼ,��Ϊ���Ѿ����� */
    UInt32 curVec = 1;
    while (theLengthSent >= inVec[curVec].iov_len)
    {
        // Skip over the vectors that were in fact sent.
        Assert(curVec < inNumVectors);
        theLengthSent -= inVec[curVec].iov_len;
        curVec++;
    }
    
	/* ��ʣ�µĲ���һ��inVec[curVec].iov_len���ȵ�����(ʵ�ʴ�СΪinVec[curVec].iov_len - theLengthSent)����ָ������,����������� */
    while (curVec < inNumVectors)
    {
        // Copy the remaining vectors into the buffer
        this->Put(  ((char*)inVec[curVec].iov_base) + theLengthSent,
                    inVec[curVec].iov_len - theLengthSent);
        theLengthSent = 0;//�Ժ��iovec��������������
        curVec++;       
    }
    return QTSS_NoErr;
}

/* ��ͳ��buffer�л��ж��ٴ��ͳ�������,����Socket::Send()���ⷢ������.ֻҪ�ܹ��ͳ�����,��refresh timeout.����һ��ȫ���ͳ�����,��flush�����,�����ۼ����ͳ�������,����EAGAIN */
QTSS_Error RTSPResponseStream::Flush()
{
	/* �õ�ʣ�����ݵĳ��� */
    UInt32 amtInBuffer = this->GetCurrentOffset() - fBytesSentInBuffer;
    if (amtInBuffer > 0)
    {
		/*********** ���д�ӡ��ʣ�����ݵ���Ϣ,ע���ӡ��Ϣ���Ǵ���������� **************/
        if (fPrintRTSP)
        {
            DateBuffer theDate;
            DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time

 			qtss_printf("\n#S->C:\n#time: ms=%lu date=%s\n", (UInt32) OS::StartTimeMilli_Int(), theDate.GetDateBuffer() );
			StrPtrLen str(this->GetBufPtr() + fBytesSentInBuffer, amtInBuffer);
			str.PrintStrEOL();
        }

        UInt32 theLengthSent = 0;
		/* ʹ��TCPSocket���ͳ�RTSP Response��������ʣ�������,ʵ�ʷ������ݳ�ΪtheLengthSent */
        (void)fSocket->Send(this->GetBufPtr() + fBytesSentInBuffer, amtInBuffer, &theLengthSent);
       
        // Refresh the timeout if we were able to send any data
		/* ֻҪ�ܷ��ͳ�����,��ˢ�³�ʱ */
        if (theLengthSent > 0)
            fTimeoutTask->RefreshTimeout();
         
		/* ����ɹ����ͳ���������,�����û�����ָ����ѷ������ݳ��� */
        if (theLengthSent == amtInBuffer)
        {
            // We were able to send all the data in the buffer. Great. Flush it.
            this->Reset();
            fBytesSentInBuffer = 0;
        }
        else
        {
            // Not all the data was sent, so report an EAGAIN
            /* ����,�ۼƷ��ͳ�������,����EAGAIN */
            fBytesSentInBuffer += theLengthSent;
            Assert(fBytesSentInBuffer < this->GetCurrentOffset());
            return EAGAIN;
        }
    }
    return QTSS_NoErr;
}
