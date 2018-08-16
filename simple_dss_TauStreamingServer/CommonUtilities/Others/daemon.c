
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 daemon.h
Description: Provide the explicit code definition of daemon function.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <fcntl.h>
#include <unistd.h>
#include "daemon.h"

/* ����һ���ػ����̵ĵ�������:�μ��ʼ� */
int daemon(int nochdir, int noclose)
{
    int fd;

	/* ����fork����һ���ӽ���,ͬʱ�������˳�,�������к������������ӽ�������� */
    switch (fork()) 
	{
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(0);
    }

	/* ����setsidϵͳ����,������������������Ҫ��һ��!����һ���µĻỰ(session),�����θûỰ���鳤(session leader).
	�õ��ý�����ȫ��������,���������������̵Ŀ���
	*/
    if (setsid() == -1)
        return (-1);

	/* �л�����Ŀ¼ */
    if (!nochdir)
        (void)chdir("/");

	/* ʹ��׼����/���/����������ļ��ض���Ϊ�򿪵��ļ�������fd,
	dup()��dup2()������������Ҫ��������һ���ļ���������,���������ض�����̵�stdin��stdout��stderr */
    if (!noclose && (fd = open("/dev/null", O_RDWR, 0)) != -1) 
	{  
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > 2)
            (void)close (fd);
    }
    return (0);
}
