/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSCodeFragment.cpp
Description: Provide OS abstraction for loading code fragments(.so/dll).
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> //加载动态库的头文件,用到dlopen()/dlclose()/dlerror()/dlsym()
#include "SafeStdLib.h"
#include "MyAssert.h"
#include "OSCodeFragment.h"



void OSCodeFragment::Initialize()
{
// does nothing...should do any CFM initialization here
}

//加载指定路径的.so,返回库文件描述符
OSCodeFragment::OSCodeFragment(const char* inPath)
: fFragmentP(NULL)
{
    fFragmentP = dlopen(inPath, RTLD_NOW | RTLD_GLOBAL); //加载.so,返回库文件描述符
    fprintf (stderr, "%s\n", dlerror());
}

OSCodeFragment::~OSCodeFragment()
{
    if (fFragmentP == NULL)
        return;
        
    dlclose(fFragmentP); //卸载打开的.so库文件
}


/* used in QTSSModule::LoadFromDisk() */
/* 获取当前动态库中指定符号inSymbolName对应的函数入口地址 */
void*   OSCodeFragment::GetSymbol(const char* inSymbolName)
{
    if (fFragmentP == NULL)
        return NULL;
        
    return dlsym(fFragmentP, inSymbolName);//返回指定的.so的库文件描述符fFragmentP中,指定符号inSymbolName对应的函数入口地址
}
