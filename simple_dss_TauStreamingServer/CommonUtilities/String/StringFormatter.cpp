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



char*   StringFormatter::sEOL = "\r\n";//回车换行
UInt32  StringFormatter::sEOLLen = 2;



/* 将数字以指定格式存入缓存 */
void StringFormatter::Put(const SInt32 num)
{
    char buff[32];
    qtss_sprintf(buff, "%ld", num);
    Put(buff);/* 使用Put(char* str)即可,见头文件
			  */
}

/* 类中最基本最重要的一个函数 */
void StringFormatter::Put(char* buffer, UInt32 bufferSize)
{
	/* 假如只有一个字符要放入 */
    if((bufferSize == 1) && (fCurrentPut != fEndPut)) {
        *(fCurrentPut++) = *buffer;
        fBytesWritten++;
        return;
    }       
        
    //loop until the input buffer size is smaller than the space in the output
    //buffer. Call BufferIsFull at each pass through the loop
	/* 检查还剩多少空间 */
    UInt32 spaceLeft = this->GetSpaceLeft();
	/* 保留最后一个字符 */
    UInt32 spaceInBuffer =  spaceLeft - 1;
	/* 新增缓存部分的大小,定义见下 */
    UInt32 resizedSpaceLeft = 0;
    
	/* 当剩下的空间不足以容纳要放入的数据,反复循环直至创建合适大小的缓存(大于或等于原缓存). */
    while ( (spaceInBuffer < bufferSize) || (spaceLeft == 0) ) // too big for destination
    {
        if (spaceLeft > 0)
        {
			/* 先复制入参buffer中的部分数据,数据长度为spaceInBuffer到当前,用完不足的空间后再创建新的空间 */
            ::memcpy(fCurrentPut, buffer, spaceInBuffer);
			/* 同时更新各量大小 */
            fCurrentPut += spaceInBuffer;
            fBytesWritten += spaceInBuffer;
			/* 剩下还要复制的数据地址和长度 */
            buffer += spaceInBuffer;
            bufferSize -= spaceInBuffer;
        }
		/* 创建原缓存大小两倍的缓存并同时复制进原数据 */
        this->BufferIsFull(fStartPut, this->GetCurrentOffset()); // resize buffer
		/* 得到在缓存基础上新增部分，注意fCurrentPut指向新增部分缓存的开头 */
        resizedSpaceLeft = this->GetSpaceLeft();
		/* 将剩余空间与新增空间做比较,如果相等就退出循环 */
        if (spaceLeft == resizedSpaceLeft) // couldn't resize, nothing left to do
        {  
           return; // done. There is either nothing to do or nothing we can do because the BufferIsFull
        }
        spaceLeft = resizedSpaceLeft;
        spaceInBuffer =  spaceLeft - 1;
    }
    
    //copy the remaining chunk into the buffer
	/* 再将buffer中剩余部分放入缓存 */
    ::memcpy(fCurrentPut, buffer, bufferSize);
    fCurrentPut += bufferSize;
    fBytesWritten += bufferSize;
    
}

