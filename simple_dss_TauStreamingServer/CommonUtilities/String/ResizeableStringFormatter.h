
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



#ifndef __RESIZEABLE_STRING_FORMATTER_H__
#define __RESIZEABLE_STRING_FORMATTER_H__

#include "StringFormatter.h"

class ResizeableStringFormatter : public StringFormatter
{
    public:
        // Pass in inBuffer=NULL and inBufSize=0 to dynamically allocate the initial buffer.
        ResizeableStringFormatter(char* inBuffer = NULL, UInt32 inBufSize = 0)
            : StringFormatter(inBuffer, inBufSize), fOriginalBuffer(inBuffer) {}
        
        //If we've been forced to increase the buffer size, fStartPut WILL be a dynamically allocated
        //buffer, and it WON'T be equal to fOriginalBuffer (obviously).
        virtual ~ResizeableStringFormatter() {  if (fStartPut != fOriginalBuffer) delete [] fStartPut; }

    private:
        
        // This function will get called by StringFormatter(基类) if the current
        // output buffer is full. This object allocates a buffer that's twice
        // as big as the old one.并把原缓存的数据复制进来
        virtual void    BufferIsFull(char* inBuffer, UInt32 inBufferLen);
        
		/* pointer to original buffer,判断是否是动态分配的 */
        char*           fOriginalBuffer;
        
};

#endif //__RESIZEABLE_STRING_FORMATTER_H__
