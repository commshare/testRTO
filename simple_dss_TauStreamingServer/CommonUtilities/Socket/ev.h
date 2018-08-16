
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


/* 有关socket event的信息 */
/* event request structure */
/* used in EventThread::Entry() */
struct eventreq 
{
  int      er_type; /* 事件请求类型,只有文件描述符类型EV_FD */
#define EV_FD 1    // file descriptor
  int      er_handle;/* socket描述符,参见select_modwatch() in win32ev.cpp */
  void    *er_data; /* 唯一的event ID,Windows窗口消息,参见select_modwatch()/EventContext::RequestEvent() */
  int      er_rcnt; /* read count */
  int      er_wcnt; /* write count */
  int      er_ecnt; /* execute count */
  int      er_eventbits; /* 事件标志位,只有EV_RE/EV_WR */
#define EV_RE  1
#define EV_WR  2
#define EV_EX  4 /* execute */
#define EV_RM  8
};

typedef struct eventreq *er_t;



int select_watchevent(struct eventreq *req, int which);
int select_modwatch(struct eventreq *req, int which);/* 最核心的函数 */
int select_waitevent(struct eventreq *req, void* onlyForMOSX);//阻塞直至在该文件描述符上等到event发生,对Linux平台没用
void select_startevents();
int select_removeevent(int which);


#endif /* _SYS_EV_H_ */


