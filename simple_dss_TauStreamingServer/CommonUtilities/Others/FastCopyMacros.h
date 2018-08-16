
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 FastCopyMacros.h
Description: Provide some fast copy macro definition using to operate the memory.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef __FastCopyMacros__
#define __FastCopyMacros__

/* 注意:这里变量dest和src都是不同类型的指针变量 */

#define     COPY_BYTE( dest, src ) ( *((char*)(dest)) = *((char*)(src)) )
#define     COPY_WORD( dest, src ) ( *((SInt16*)(dest)) =  *((SInt16*)(src)) )
#define     COPY_LONG_WORD( dest, src ) ( *((SInt32*)(dest)) =  *((SInt32*)(src)) )
#define     COPY_LONG_LONG_WORD( dest, src ) ( *((SInt64*)(dest)) =  *((SInt64**)(src)) )

#define     MOVE_BYTE( dest, src ) ( dest = *((char*)(src)) )
#define     MOVE_WORD( dest, src ) ( dest =  *((SInt16*)(src)) )
#define     MOVE_LONG_WORD( dest, src ) ( dest =  *((SInt32*)(src)) )
#define     MOVE_LONG_LONG_WORD( dest, src ) ( dest =  *((SInt64**)(src)) )


#endif

