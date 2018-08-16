
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

/* 创建一个守护进程的典型做法:参见笔记 */
int daemon(int nochdir, int noclose)
{
    int fd;

	/* 调用fork产生一个子进程,同时父进程退出,我们所有后续工作都在子进程中完成 */
    switch (fork()) 
	{
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(0);
    }

	/* 调用setsid系统调用,这是整个过程中最重要的一步!创建一个新的会话(session),并自任该会话的组长(session leader).
	让调用进程完全独立出来,脱离所有其他进程的控制
	*/
    if (setsid() == -1)
        return (-1);

	/* 切换到根目录 */
    if (!nochdir)
        (void)chdir("/");

	/* 使标准输入/输出/错误输出等文件重定向为打开的文件描述符fd,
	dup()和dup2()这两个函数主要用来复制一个文件的描述符,经常用作重定向进程的stdin、stdout、stderr */
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
