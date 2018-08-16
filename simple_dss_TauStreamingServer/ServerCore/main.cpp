
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 main.cpp
Description: main function to drive streaming server on Linux platform.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2011-07-02

****************************************************************************/  
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include "daemon.h"
#include "defaultPaths.h"
#include "SafeStdLib.h"
#include "QTSSExpirationDate.h"
#include "QTSServer.h"
#include "RunServer.h"
#include "FilePrefsSource.h"
#include "GenerateXMLPrefs.h"
static int sSigIntCount = 0;
static int sSigTermCount = 0;
static pid_t sChildPID = 0;		/* 子进程的识别码 */

void usage ();
Bool16 sendtochild (int sig, pid_t myPID);
void sigcatcher (int sig, int /*sinfo */ , struct sigcontext * /*sctxt */ );
Bool16 RunInForeground ();
Bool16 CheckConfig (char *inPath);
Bool16 RestartServer (char *theXMLFilePath);

void usage ()
{
	const char *usage_name = PLATFORM_SERVER_BIN_NAME;

	qtss_printf ("%s/%s ( Build/%s; Platform/%s; %s) Built on: %s\n", QTSServerInterface::GetServerName ().Ptr,
		QTSServerInterface::GetServerVersion ().Ptr,
		QTSServerInterface::GetServerBuild ().Ptr, QTSServerInterface::GetServerPlatform ().Ptr, QTSServerInterface::GetServerComment ().Ptr, QTSServerInterface::GetServerBuildDate ().Ptr);
	qtss_printf ("usage: %s [ -v | -d | -f /myconfigpath.xml | -p port | -x | -D | -Z debugLevel | -S numseconds | -I ]\n", usage_name);
	qtss_printf ("-v: Prints usage\n");
	qtss_printf ("-d: Run in the foreground\n");
	qtss_printf ("-f /myconfigpath.xml: Specify a config file\n");
	qtss_printf ("-p XXX: Specify the default RTSP listening port of the server\n");
	qtss_printf ("-x: Force create new .xml config file and exit.\n");
	qtss_printf ("-D: Display performance data\n");
	qtss_printf ("-Z n: set debug level\n");
	qtss_printf ("-S n: Display server stats in the console every \"n\" seconds\n");
	qtss_printf ("-I: Start the server in the idle state\n");
}

/* 在父进程myPID中,发送指定信号sig给子进程sChildPID>0,并返回true, 否则,返回false */
Bool16 sendtochild (int sig, pid_t myPID)
{
	if (sChildPID != 0 && sChildPID != myPID)	// this is the parent
	{							// Send signal to child,发送指定的信号给指定的进程
		::kill (sChildPID, sig);
		return true;
	}

	return false;
}

/* 获取当前进程的PID,发送指定信号sig给子进程sChildPID,同时依据信号类型,在子进程中触发读预设值的事件,或将子进程中的服务器的Signal Interrupt/Terminate状态为true */
void sigcatcher (int sig, int /*sinfo */ , struct sigcontext * /*sctxt */ )
{
#if DEBUG
	qtss_printf ("Signal %d caught\n", sig);
#endif
	pid_t myPID = getpid ();	//取得目前进程的识别码

	// SIGHUP means we should reread our preferences
	if (sig == SIGHUP)
	{
		if (sendtochild (sig, myPID))
		{
			return;
		}
		else
		{
			// This is the child process.
			// Re-read our preferences.在子进程中触发读预设值的事件
			RereadPrefsTask *task = new RereadPrefsTask;
			task->Signal (Task::kStartEvent);

		}
	}

	//Try to shut down gracefully the first time, shutdown forcefully the next time
	if (sig == SIGINT)			// kill the child only
	{
		if (sendtochild (sig, myPID))
		{
			return;				// ok we're done 
		}
		else
		{
			// Tell the server that there has been a SigInt, the main thread will start
			// the shutdown process because of this. The parent and child processes will quit.
			if (sSigIntCount == 0)
				QTSServerInterface::GetServer ()->SetSigInt ();	/* 设置服务器的Signal Interrupt状态为true */
			sSigIntCount++;		/* 更新信号中断计数 */
		}
	}

	if (sig == SIGTERM || sig == SIGQUIT)	// kill child then quit
	{
		if (sendtochild (sig, myPID))
		{
			return;				// ok we're done 
		}
		else
		{
			// Tell the server that there has been a SigTerm, the main thread will start
			// the shutdown process because of this only the child will quit
			if (sSigTermCount == 0)
				QTSServerInterface::GetServer ()->SetSigTerm ();	/* 设置服务器的Signal Terminate状态为true */
			sSigTermCount++;	/* 更新信号终止计数 */
		}
	}
}

extern "C"
{
	typedef int (*EntryFunction) (int input);
}

/* 在前台运行吗?设置没有缓存的标准输出,总返回true */
Bool16 RunInForeground ()
{
	/* 设置标准输出,没有缓存 */
	(void) setvbuf (stdout, NULL, _IOLBF, 0);
	OSThread::WrapSleep (true);

	return true;
}

Bool16 CheckConfig (char *inPath)
{
	struct stat filestat;
	::stat (inPath, &filestat);

	if (filestat.st_size < 0)
		return false;

	if (S_ISDIR (filestat.st_mode))
		return false;

	if (::access (inPath, F_OK) == -1)
		return false;

	if (::chmod (inPath, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) == -1)
	{
		printf ("set config mode failed! \n");
		return false;
	}

	return true;
}

/* 从指定路径的XML文件中获取"auto_restart"的设定值,据此决定是否重启服务器,返回ture(默认值)或false */
Bool16 RestartServer (char *theXMLFilePath)
{
	Bool16 autoRestart = true;
	XMLPrefsParser theXMLParser (theXMLFilePath);
	theXMLParser.Parse ();		/* 解析xml文件中各Tag的合法性 */

	ContainerRef server = theXMLParser.GetRefForServer ();	/* 在fRootTag下获取子Tag SERVER并返回,没有就生成它 */
	ContainerRef pref = theXMLParser.GetPrefRefByName (server, "auto_restart");	/* 返回子Tag SERVER下PrefName为"auto_restart"和index为0的子Tag */
	char *autoStartSetting = NULL;

	if (pref != NULL)
		autoStartSetting = theXMLParser.GetPrefValueByRef (pref, 0, NULL, NULL);	//得到"auto_restart"的设定值,是true或false

	if ((autoStartSetting != NULL) && (::strcmp (autoStartSetting, "false") == 0))	//假如预设值是false,就设置autoRestart为false
		autoRestart = false;

	return autoRestart;
}

int main (int argc, char *argv[])
{
	extern char *optarg;
	struct sigaction act;

	/* 设置信号处理方式 */
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = (void (*)(int)) &sigcatcher;

	(void)::sigaction (SIGPIPE, &act, NULL);
	(void)::sigaction (SIGHUP, &act, NULL);
	(void)::sigaction (SIGINT, &act, NULL);
	(void)::sigaction (SIGTERM, &act, NULL);
	(void)::sigaction (SIGQUIT, &act, NULL);
	(void)::sigaction (SIGALRM, &act, NULL);

	//grow our pool of file descriptors to the max!
	/* 设置每个进程允许打开的最大文件描述符的个数,使其不受系统资源限制 */
	struct rlimit rl;

	// set it to the absolute maximum of resource limit(rlim) that the operating system allows - have to be superuser(超级用户) to do this
	rl.rlim_cur = RLIM_INFINITY;	//软限制
	rl.rlim_max = RLIM_INFINITY;	//硬限制

	setrlimit (RLIMIT_NOFILE, &rl);	//指定进程可以打开文件的最大个数,此处是去除对打开文件的最大个数的限制

#if 0							// testing
	getrlimit (RLIMIT_NOFILE, &rl);
	printf ("current open file limit =%lu\n", (long unsigned) rl.rlim_cur);
	printf ("current open file max =%lu\n", (long unsigned) rl.rlim_max);
#endif

	/**********下面的几个参数将作为入参传送给StartServer()**********************/
	int thePort = 0;			//port can be set on the command line
	int statsUpdateInterval = 0;	//服务器状态更新间隔
	QTSS_ServerState theInitialState = qtssRunningState;	//服务器初始状态值
	Bool16 dontFork = false;	//是否不生成子进程(并使它作为守护进程)? 
	UInt32 debugLevel = 0;		//注意这两个参数将被传送给StartServer()
	UInt32 debugOptions = kRunServerDebug_Off;
	/**********上面的几个参数将作为入参传送给StartServer()**********************/

	//First thing to do is to read command-line arguments.首先读取命令行参数并解析
	int ch;						//character for getopt()
	Bool16 theXMLPrefsExist = true;	//存在xml预设值文件吗?
	static char *sDefaultConfigFilePath = DEFAULTPATHS_ETC_DIR_OLD "streamingserver.conf";
	static char *sDefaultXMLFilePath = DEFAULTPATHS_ETC_DIR "streamingserver.xml";
	char *theXMLFilePath = sDefaultXMLFilePath;
	char *theConfigFilePath = sDefaultConfigFilePath;

	while ((ch = getopt (argc, argv, "vdf:xp:DZ:S:I")) != EOF)	// opt: means requires option arg
	{
		switch (ch)
		{
		case 'v':
			usage ();
			::exit (0);
		case 'd':
			dontFork = RunInForeground ();	//因为在前台输出打印信息,不生成守护进程         
			break;
		case 'D':				/* 手动设置调试级别,调试选项,状态更新间隔,这些值都可以手动改变的 */
			dontFork = RunInForeground ();
			debugOptions |= kRunServerDebugDisplay_On;
			if (debugLevel == 0)
				debugLevel = 1;
			if (statsUpdateInterval == 0)
				statsUpdateInterval = 3;
			break;
		case 'Z':				/* 手动设置调试级别 */
			Assert (optarg != NULL);	// this means we didn't declare getopt options correctly or there is a bug in getopt.
			debugLevel = (UInt32)::atoi (optarg);
			break;
		case 'f':				/* 手动设置xml文件路径 */
			Assert (optarg != NULL);	// this means we didn't declare getopt options correctly or there is a bug in getopt.
			theXMLFilePath = optarg;
			break;
		case 'p':				/* 手动设置端口值 */
			Assert (optarg != NULL);	// this means we didn't declare getopt options correctly or there is a bug in getopt.
			thePort =::atoi (optarg);
			break;
		case 'S':				/* 手动设置服务器状态更新间隔 */
			dontFork = RunInForeground ();
			Assert (optarg != NULL);	// this means we didn't declare getopt options correctly or there is a bug in getopt.
			statsUpdateInterval =::atoi (optarg);
			break;
		case 'x':
			theXMLPrefsExist = false;	// Force us to generate a new XML prefs file
			theInitialState = qtssShuttingDownState;
			dontFork = true;
			break;
		case 'I':
			theInitialState = qtssIdleState;
			break;
		default:
			break;
		}
	}

	// Check port 检查手动输入的监听端口号是否合法?
	if (thePort < 0 || thePort > 65535)
	{
		qtss_printf ("Invalid port value = %d max value = 65535\n", thePort);
		exit (-1);
	}

	// Check expiration date 检查软件使用是否过期了?
	QTSSExpirationDate::PrintExpirationDate ();
	if (QTSSExpirationDate::IsSoftwareExpired ())
	{
		qtss_printf ("Streaming Server has expired\n");
		::exit (0);
	}

	XMLPrefsParser theXMLParser (theXMLFilePath);

	//check the validity of the xml config file......
	theXMLPrefsExist = CheckConfig (theXMLFilePath);
	/*if (!theXMLPrefsExist)
	   {
	   qtss_printf("Invalid streaming server prefs file.\n");
	   exit(-1);
	   } */

	//
	// Check to see if we can write to the file
	/*if (!theXMLParser.CanWriteFile())
	   {
	   qtss_printf("Cannot write to the streaming server prefs file.\n");
	   exit(-1);
	   } */

	// If we aren't forced to create a new XML prefs file, whether
	// we do or not depends solely on whether the XML prefs file exists currently.
	/*if (theXMLPrefsExist)
	   theXMLPrefsExist = theXMLParser.DoesFileExist(); */

	if (!theXMLPrefsExist)
	{
		// The XML prefs file doesn't exist, so let's create an old-style
		// prefs source in order to generate a fresh XML prefs file.

		if (theConfigFilePath != NULL)
		{
			FilePrefsSource *filePrefsSource = new FilePrefsSource (true);	// Allow dups

			if (filePrefsSource->InitFromConfigFile (theConfigFilePath))
			{
				qtss_printf ("Generating a new prefs file at %s\n", theXMLFilePath);
			}

			if (GenerateAllXMLPrefs (filePrefsSource, &theXMLParser))
			{
				qtss_printf ("Fatal Error: Could not create new prefs file at: %s. (%d)\n", theXMLFilePath, OSThread::GetErrno ());
				::exit (-1);
			}
		}
	}

	// 解析XML文件中各Tag的合法性,若不合法,就直接返回
	// Parse the configs from the XML file
	int xmlParseErr = theXMLParser.Parse ();
	if (xmlParseErr)
	{
		qtss_printf ("Fatal Error: Could not load configuration file at %s. (%d)\n", theXMLFilePath, OSThread::GetErrno ());
		::exit (-1);
	}

	//除非在命令行中使用-x选项强制生成新的XML文件并将dontFork设为true,否则在此处派生出一个子进程并使它成为守护进程
	//Unless the command line option is set, fork & daemonize the process at this point
	if (!dontFork)
	{
		if (daemon (0, 0) != 0)
		{
#if DEBUG
			qtss_printf ("Failed to daemonize process. Error = %d\n", OSThread::GetErrno ());
#endif
			exit (-1);
		}
	}

	int status = 0;
	int pid = 0;
	pid_t processID = 0;

	//假如要派生出子进程,就由守护进程派生出一个子进程,并由它驱动流媒体服务器,而守护进程只负责管理和维护,等待子进程的退出,必要时重新派生出子进程
	if (!dontFork)				// if (fork) 
	{
		//loop until the server exits normally. If the server doesn't exit normally, then restart it.
		// normal exit means the following the child quit 
		// 
		do						// fork at least once but stop on the status conditions returned by wait or if autoStart pref is false
		{
			processID = fork ();	//派生出一个子进程
			Assert (processID >= 0);	//确保子进程识别码没错
			if (processID > 0)	// this is the parent and we have a child
			{
				sChildPID = processID;	//设置子进程识别码
				status = 0;
				while (status == 0)	//loop on wait until status is != 0;
				{
					pid =::wait (&status);	//暂停执行目前进程,并等待子进程结束
					SInt8 exitStatus = (SInt8) WEXITSTATUS (status);	//获取子进程结束时的状态码,status和exitstatus相同                                     
					qtss_printf ("Child Process %d wait exited with pid=%d status=%d exit status=%d\n", processID, pid, status, exitStatus);

					if (WIFEXITED (status) && pid > 0 && status != 0)	// 假如子进程正常退出,其状态码为-1/-2: child exited with status -2 restart or -1 don't restart 
					{
						qtss_printf ("child exited with status=%d\n", exitStatus);

						if (exitStatus == -1)	// child couldn't run don't try again
						{
							qtss_printf ("child exited with -1 fatal error so parent is exiting too.\n");
							exit (EXIT_FAILURE);	//让当前父进程也退出,EXIT_FAILURE宏定义参见stdlib.h
						}
						break;	// restart the child     
					}

					if (WIFSIGNALED (status))	// 假如子进程因信号中断退出: child exited on an unhandled signal (maybe a bug error or segment fault)
					{
						qtss_printf ("child was signalled\n");
						break;	// restart the child
					}

					/* 当前父进程因外部信号唤醒,没有等待子进程退出 */
					if (pid == -1 && status == 0)	// parent woken up by a handled signal
					{
						qtss_printf ("handled signal continue waiting\n");
						continue;
					}

					/* 当前子进程正常退出,父进程也正常退出 */
					if (pid > 0 && status == 0)
					{
						qtss_printf ("child exited cleanly so parent is exiting\n");
						exit (EXIT_SUCCESS);	//EXIT_SUCCESS宏定义参见stdlib.h                       
					}

					qtss_printf ("child died for unknown reasons parent is exiting\n");
					exit (EXIT_FAILURE);	//当前父进程退出
				}
			}
			else if (processID == 0)	// must be the child
				break;
			else
				exit (EXIT_FAILURE);

			//eek. If you auto-restart too fast, you might start the new one before the OS has
			//cleaned up from the old one, resulting in startup errors when you create the new
			//one. Waiting for a second seems to work
			sleep (1);			/* 避免重启过快 */
		}
		while (RestartServer (theXMLFilePath));	// fork again based on pref if server dies

		if (processID != 0)		//the parent is quitting,让没有退出的父进程退出
			exit (EXIT_SUCCESS);
	}

	sChildPID = 0;
	//we have to do this again for the child process, because sigaction states do not span multiple processes.
	//为守护进程的子进程也设置信号处理方式
	(void)::sigaction (SIGPIPE, &act, NULL);
	(void)::sigaction (SIGHUP, &act, NULL);
	(void)::sigaction (SIGINT, &act, NULL);
	(void)::sigaction (SIGTERM, &act, NULL);
	(void)::sigaction (SIGQUIT, &act, NULL);

	//This function starts, runs, and shuts down the server
	if (::StartServer (&theXMLParser, /* &theMessagesSource, */ thePort, statsUpdateInterval, theInitialState, dontFork, debugLevel, debugOptions) != qtssFatalErrorState)
	{
		::RunServer ();			/* 这个函数在整个运行过程中管理服务器,最后退出时关闭该子进程 */
		CleanPid (false);		/* 删除预设路径的pid文件 */
		exit (EXIT_SUCCESS);
	}
	else
		exit (-1);				//Cant start server don't try again 假如不能启动服务器,别再试了
}
