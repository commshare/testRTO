/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 MyAssert.cpp
Description: Provide a assert class in C++ or some assert Macroes in C.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "MyAssert.h"
#include "OSHeaders.h"

static AssertLogger* sLogger = NULL;

/* 用入参设置AssertLogger类指针 */
void SetAssertLogger(AssertLogger* theLogger)
{
    sLogger = theLogger;
}

void MyAssert(char *inMessage)
{
    if (sLogger != NULL)
        sLogger->LogAssert(inMessage);
    else
    {
        (*(long*)0) = 0;
    }
}
