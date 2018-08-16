
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSCond.h
Description: Provide a simple condition variable abstraction.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef _OSCOND_H_
#define _OSCOND_H_

#include <pthread.h>
#include "OSMutex.h"
#include "MyAssert.h"

class OSCond 
{
    public:

		/* 注意构造函数和析构函数在OSCond.cpp中定义 */
        OSCond();
        ~OSCond();
        
        inline void     Signal();//传信函数
        inline void     Wait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs = 0);//等待传信函数
        inline void     Broadcast(); //广播传信函数

    private:

        pthread_cond_t      fCondition;/*条件变量*/
        void                TimedWait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs);/* 阻塞直到指定时间 */
};

/* 加入等待临界区返回行列,等到结果后,进入临界区,获取线程ID,并使线程计数增加1 */
inline void OSCond::Wait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs)
{ 
    this->TimedWait(inMutex, inTimeoutInMilSecs);
}

/* 将指定句柄对象置于通知状态 */
inline void OSCond::Signal()
{
    pthread_cond_signal(&fCondition);
}

/* 通过循环使所有指定句柄对象置于通知状态 */
inline void OSCond::Broadcast()
{
    pthread_cond_broadcast(&fCondition);
}

#endif //_OSCOND_H_
