/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 MyAssert.h
Description: Provide a assert class in C++ or some assert Macroes in C.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 

#ifndef _MYASSERT_H_
#define _MYASSERT_H_

#include <stdio.h>
#include "SafeStdLib.h"

#ifdef __cplusplus
class AssertLogger
{
    public:
        // An interface so the MyAssert function can write a message
        virtual void LogAssert(char* inMessage) = 0;
		virtual ~AssertLogger(){}
};

// If a logger is provided, asserts will be logged. Otherwise, asserts will cause a bus error
void SetAssertLogger(AssertLogger* theLogger);
#endif

#if ASSERT  
    void MyAssert(char *s);

    #define kAssertBuffSize 256
    
    #define Assert(condition)    {                              \
                                                                \
        if (!(condition))                                       \
        {                                                       \
            char s[kAssertBuffSize];                            \
            s[kAssertBuffSize -1] = 0;                          \
            qtss_snprintf (s,kAssertBuffSize -1, "_Assert: %s, %d",__FILE__, __LINE__ ); \
            MyAssert(s);                                        \
        }   }


    #define AssertV(condition,errNo)    {                                   \
        if (!(condition))                                                   \
        {                                                                   \
            char s[kAssertBuffSize];                                        \
            s[kAssertBuffSize -1] = 0;                                      \
            qtss_snprintf( s,kAssertBuffSize -1, "_AssertV: %s, %d (%d)",__FILE__, __LINE__, errNo );    \
            MyAssert(s);                                                    \
        }   }
                                     
                                         
    #define Warn(condition) {                                       \
            if (!(condition))                                       \
                qtss_printf( "_Warn: %s, %d\n",__FILE__, __LINE__ );     }                                                           
                                         
    #define WarnV(condition,msg)        {                               \
        if (!(condition))                                               \
            qtss_printf ("_WarnV: %s, %d (%s)\n",__FILE__, __LINE__, msg );  }                                                   
                                         
    #define WarnVE(condition,msg,err)  {                           		\
        if (!(condition))                                               \
        {   char buffer[kAssertBuffSize];								\
            buffer[kAssertBuffSize -1] = 0;                              \
            qtss_printf ("_WarnV: %s, %d (%s, %s [err=%d])\n",__FILE__, __LINE__, msg, qtss_strerror(err,buffer,sizeof(buffer) -1), err );  \
        }	}

#else
            
    #define Assert(condition) ((void) 0)
    #define AssertV(condition,errNo) ((void) 0)
    #define Warn(condition) ((void) 0)
    #define WarnV(condition,msg) ((void) 0)

#endif
#endif //_MY_ASSERT_H_
