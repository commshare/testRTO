
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSCodeFragment.h
Description: Provide OS abstraction for loading code fragments(.so/dll).
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _OS_CODEFRAGMENT_H_
#define _OS_CODEFRAGMENT_H_

#include <stdlib.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"


class OSCodeFragment
{
    public:
    
        static void Initialize();/* 没有源码实现 */
    
        OSCodeFragment(const char* inPath);
        ~OSCodeFragment();
        
        Bool16  IsValid() { return (fFragmentP != NULL); }/* 判断.so的库文件描述符是否存在? */
        void*   GetSymbol(const char* inSymbolName);
        
    private:
    
        void*   fFragmentP; //.so的库文件描述符
};

#endif//_OS_CODEFRAGMENT_H_
