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
#include <dlfcn.h> //���ض�̬���ͷ�ļ�,�õ�dlopen()/dlclose()/dlerror()/dlsym()
#include "SafeStdLib.h"
#include "MyAssert.h"
#include "OSCodeFragment.h"



void OSCodeFragment::Initialize()
{
// does nothing...should do any CFM initialization here
}

//����ָ��·����.so,���ؿ��ļ�������
OSCodeFragment::OSCodeFragment(const char* inPath)
: fFragmentP(NULL)
{
    fFragmentP = dlopen(inPath, RTLD_NOW | RTLD_GLOBAL); //����.so,���ؿ��ļ�������
    fprintf (stderr, "%s\n", dlerror());
}

OSCodeFragment::~OSCodeFragment()
{
    if (fFragmentP == NULL)
        return;
        
    dlclose(fFragmentP); //ж�ش򿪵�.so���ļ�
}


/* used in QTSSModule::LoadFromDisk() */
/* ��ȡ��ǰ��̬����ָ������inSymbolName��Ӧ�ĺ�����ڵ�ַ */
void*   OSCodeFragment::GetSymbol(const char* inSymbolName)
{
    if (fFragmentP == NULL)
        return NULL;
        
    return dlsym(fFragmentP, inSymbolName);//����ָ����.so�Ŀ��ļ�������fFragmentP��,ָ������inSymbolName��Ӧ�ĺ�����ڵ�ַ
}
