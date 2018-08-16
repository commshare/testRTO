
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

		/* ע�⹹�캯��������������OSCond.cpp�ж��� */
        OSCond();
        ~OSCond();
        
        inline void     Signal();//���ź���
        inline void     Wait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs = 0);//�ȴ����ź���
        inline void     Broadcast(); //�㲥���ź���

    private:

        pthread_cond_t      fCondition;/*��������*/
        void                TimedWait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs);/* ����ֱ��ָ��ʱ�� */
};

/* ����ȴ��ٽ�����������,�ȵ������,�����ٽ���,��ȡ�߳�ID,��ʹ�̼߳�������1 */
inline void OSCond::Wait(OSMutex* inMutex, SInt32 inTimeoutInMilSecs)
{ 
    this->TimedWait(inMutex, inTimeoutInMilSecs);
}

/* ��ָ�������������֪ͨ״̬ */
inline void OSCond::Signal()
{
    pthread_cond_signal(&fCondition);
}

/* ͨ��ѭ��ʹ����ָ�������������֪ͨ״̬ */
inline void OSCond::Broadcast()
{
    pthread_cond_broadcast(&fCondition);
}

#endif //_OSCOND_H_
