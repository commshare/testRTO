
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 ev.h
Description: Describe the event request structure of socket using in the event driven task scheme.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#ifndef _SYS_EV_H_
#define _SYS_EV_H_


#include <sys/queue.h>


/* �й�socket event����Ϣ */
/* event request structure */
/* used in EventThread::Entry() */
struct eventreq 
{
  int      er_type; /* �¼���������,ֻ���ļ�����������EV_FD */
#define EV_FD 1    // file descriptor
  int      er_handle;/* socket������,�μ�select_modwatch() in win32ev.cpp */
  void    *er_data; /* Ψһ��event ID,Windows������Ϣ,�μ�select_modwatch()/EventContext::RequestEvent() */
  int      er_rcnt; /* read count */
  int      er_wcnt; /* write count */
  int      er_ecnt; /* execute count */
  int      er_eventbits; /* �¼���־λ,ֻ��EV_RE/EV_WR */
#define EV_RE  1
#define EV_WR  2
#define EV_EX  4 /* execute */
#define EV_RM  8
};

typedef struct eventreq *er_t;



int select_watchevent(struct eventreq *req, int which);
int select_modwatch(struct eventreq *req, int which);/* ����ĵĺ��� */
int select_waitevent(struct eventreq *req, void* onlyForMOSX);//����ֱ���ڸ��ļ��������ϵȵ�event����,��Linuxƽ̨û��
void select_startevents();
int select_removeevent(int which);


#endif /* _SYS_EV_H_ */


