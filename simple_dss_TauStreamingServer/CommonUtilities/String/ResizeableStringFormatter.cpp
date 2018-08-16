/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 ResizeableStringFormatter.h
Description: Derived from StringFormatter, this object can grows infinitly by 2 multiple size.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "ResizeableStringFormatter.h"
#include "OSMemory.h"



/* 本派生类中最重要的一个函数,其它都inherited by class StringFormatter */
/* used in StringFormatter::Put() */
void    ResizeableStringFormatter::BufferIsFull(char* inBuffer, UInt32 inBufferLen)
{
    //allocate a buffer twice as big as the old one, and copy over the contents
	/* 首先复制原缓存大小两倍的缓存,GetTotalBufferSize()定义见基类StringFormatter  */
    UInt32 theNewBufferSize = this->GetTotalBufferSize() * 2;
    if (theNewBufferSize == 0)
        theNewBufferSize = 64;
     
	/* 创建原缓存大小两倍的缓存,复制原数据进来 */
    char* theNewBuffer = NEW char[theNewBufferSize];
    ::memcpy(theNewBuffer, inBuffer, inBufferLen);

    //if the old buffer was dynamically allocated also, we'd better delete it.
    if (inBuffer != fOriginalBuffer)
        delete [] inBuffer;
    
	/* 重新设置基类中的成员变量 */
    fStartPut = theNewBuffer;
    fCurrentPut = theNewBuffer + inBufferLen;/* 因为已经复制进这么长的数据 */
    fEndPut = theNewBuffer + theNewBufferSize;
}
