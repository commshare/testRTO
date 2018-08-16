/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 StringFormatter.h
Description: Utility class for formatting text to a buffer.Construct object with 
a buffer, then call one of many Put methods to write into that buffer.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include <string.h>
#include "StringFormatter.h"
#include "MyAssert.h"



char*   StringFormatter::sEOL = "\r\n";//�س�����
UInt32  StringFormatter::sEOLLen = 2;



/* ��������ָ����ʽ���뻺�� */
void StringFormatter::Put(const SInt32 num)
{
    char buff[32];
    qtss_sprintf(buff, "%ld", num);
    Put(buff);/* ʹ��Put(char* str)����,��ͷ�ļ�
			  */
}

/* �������������Ҫ��һ������ */
void StringFormatter::Put(char* buffer, UInt32 bufferSize)
{
	/* ����ֻ��һ���ַ�Ҫ���� */
    if((bufferSize == 1) && (fCurrentPut != fEndPut)) {
        *(fCurrentPut++) = *buffer;
        fBytesWritten++;
        return;
    }       
        
    //loop until the input buffer size is smaller than the space in the output
    //buffer. Call BufferIsFull at each pass through the loop
	/* ��黹ʣ���ٿռ� */
    UInt32 spaceLeft = this->GetSpaceLeft();
	/* �������һ���ַ� */
    UInt32 spaceInBuffer =  spaceLeft - 1;
	/* �������沿�ֵĴ�С,������� */
    UInt32 resizedSpaceLeft = 0;
    
	/* ��ʣ�µĿռ䲻��������Ҫ���������,����ѭ��ֱ���������ʴ�С�Ļ���(���ڻ����ԭ����). */
    while ( (spaceInBuffer < bufferSize) || (spaceLeft == 0) ) // too big for destination
    {
        if (spaceLeft > 0)
        {
			/* �ȸ������buffer�еĲ�������,���ݳ���ΪspaceInBuffer����ǰ,���겻��Ŀռ���ٴ����µĿռ� */
            ::memcpy(fCurrentPut, buffer, spaceInBuffer);
			/* ͬʱ���¸�����С */
            fCurrentPut += spaceInBuffer;
            fBytesWritten += spaceInBuffer;
			/* ʣ�»�Ҫ���Ƶ����ݵ�ַ�ͳ��� */
            buffer += spaceInBuffer;
            bufferSize -= spaceInBuffer;
        }
		/* ����ԭ�����С�����Ļ��沢ͬʱ���ƽ�ԭ���� */
        this->BufferIsFull(fStartPut, this->GetCurrentOffset()); // resize buffer
		/* �õ��ڻ���������������֣�ע��fCurrentPutָ���������ֻ���Ŀ�ͷ */
        resizedSpaceLeft = this->GetSpaceLeft();
		/* ��ʣ��ռ��������ռ����Ƚ�,�����Ⱦ��˳�ѭ�� */
        if (spaceLeft == resizedSpaceLeft) // couldn't resize, nothing left to do
        {  
           return; // done. There is either nothing to do or nothing we can do because the BufferIsFull
        }
        spaceLeft = resizedSpaceLeft;
        spaceInBuffer =  spaceLeft - 1;
    }
    
    //copy the remaining chunk into the buffer
	/* �ٽ�buffer��ʣ�ಿ�ַ��뻺�� */
    ::memcpy(fCurrentPut, buffer, bufferSize);
    fCurrentPut += bufferSize;
    fBytesWritten += bufferSize;
    
}

