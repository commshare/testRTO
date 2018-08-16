
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSRollingLog.h
Description: A log toolkit, log can roll either by time or by size, clients
             must derive off of this object to provide configuration information. 
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef __QTSS_ROLLINGLOG_H__
#define __QTSS_ROLLINGLOG_H__

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "OSHeaders.h"
#include "OSMutex.h"
#include "Task.h"

const Bool16 kAllowLogToRoll = true;//允许日志滚动吗?

class QTSSRollingLog : public Task
{
    public:
    
        //pass in whether you'd like the log roller to log errors.
        QTSSRollingLog();
        
        //
        // Call this to delete. Closes the log and sends a kill event
        void    Delete()
            { CloseLog(false); this->Signal(Task::kKillEvent); }
        
        //
        // Write a log message
		/* 以追加方式打开日志文件,检查是否滚动日志? 将入参指定数据以字符串形式追加写入日志文件中,关闭日志文件 */
        void    WriteToLog(char* inLogData, Bool16 allowLogToRoll);
        
        //log rolls automatically based on the configuration criteria,
        //but you may roll the log manually by calling this function.
        //Returns true if no error, false otherwise
		/* 重命名旧日志文件(添加日期和.log),重新打开日志文件,再次配置各数据成员的值 */
        Bool16  RollLog();

        //
        // Call this to open the log file and begin logging 
		/* 类似构造函数,配置各数据成员(fLogging除外),以追加方式打开指定目录的Log文件,配置Log文件的生成日期 */
        void EnableLog( Bool16 appendDotLog = true);
        
        //
        // Call this to close the log
        // (pass leaveEnabled as true when we are temporarily closing.)
		/* 关闭Log文件 */
        void CloseLog( Bool16 leaveEnabled = false);

        //
        //mainly to check and see if errors occurred
		/* 现在可以记录日志吗? 当日志文件关闭或日志文件存在时,就可以记录日志. */
        Bool16  IsLogEnabled();
        
        //master switch
		/* 获取或设置日志可否记录状态 */
        Bool16  IsLogging() { return fLogging; }
        void    SetLoggingEnabled( Bool16 logState ) { fLogging = logState; }
        
        //General purpose utility function
        //returns false if some error has occurred
		/* 获取当前时间,并以指定格式YYYY-MM-DD HH:MM:SS存放在指定大小的缓存中,返回true */
        static Bool16   FormatDate(char *ioDateBuffer, Bool16 logTimeInGMT);
        
        // Check the log to see if it needs to roll
        // (rolls the log if necessary)
		/* 检查是否Roll Log?首先,若当前时间与生成日志文件的时间间隔超过日志滚动间隔,就滚动日志,然后,当日志文件的位置指针超过最大日志字节数,就滚动日志 */
        Bool16          CheckRollLog();
        
        // Set this to true to get the log to close the file between writes.
		/* 设置向日志文件中写入数据后是否关闭日志文件? */
        static void		SetCloseOnWrite(Bool16 closeOnWrite);

        enum
        {
            kMaxDateBufferSizeInBytes = 30, //UInt32
            kMaxFilenameLengthInBytes = 31  //UInt32
        };
    
    protected:

        //
        // Task object. Do not delete directly
		/* 关闭日志,删除日志目录 */
        virtual ~QTSSRollingLog();

		/* 注意这种处理方法:派生类对这些函数作具体定义! */
        //Derived class must provide a way to get the log & rolled log name
        virtual char*  GetLogName() = 0;
        virtual char*  GetLogDir() = 0;
        virtual UInt32 GetRollIntervalInDays() = 0;//0 means no interval
        virtual UInt32 GetMaxLogBytes() = 0;//0 means unlimited
                    
        //to record the time the file was created (for time based rolling基于时间的日志滚动,而非基于文件大小的日志滚动)
		/* 将生成日志文件的local时间追加进日志文件中,再从日志文件中读出并返回UTC时间 */
        virtual time_t  WriteLogHeader(FILE *inFile);
		/* 从Log文件开头读取theFileCreateTime并作恰当处理后转换成从公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数,也返回该数 */
        time_t          ReadLogHeader(FILE* inFile);

    private:
    
        //
        // Run function to roll log right at midnight 
		/* 若当前时间与生成日志文件的时间间隔超过日志滚动间隔,写入日志文件(重命名旧日志文件(添加日期和.log),重新打开日志文件,再次配置各数据成员的值),每分钟滚动一次Log */
        virtual SInt64  Run();

		/* 日志文件句柄 */
        FILE*           fLog;
		/* Log文件的创建时间 */
        time_t          fLogCreateTime;
		/* Log文件的完全路径 */
        char*           fLogFullPath;

		/* 在文件末尾附加后缀.log吗? */
        Bool16          fAppendDotLog;
		/* 记录Log吗? */
        Bool16          fLogging;

		/* 取得并生成Log目录,分配适当大小的缓存存放一个没有重名的新命名(附加日期和.log)后的文件名,并赋给入参.成功返回true,失败返回false */
        Bool16          RenameLogFile(const char* inFileName);
		/* 获取入参指定路径的文件信息,根据返回结果判断该文件是否存在 */
        Bool16          DoesFileExist(const char *inPath);
		/* 将给定时间设置成midnight后再转换成自1970-1-1 00:00:00.以来的秒数 */
        static void     ResetToMidnight(time_t* inTimePtr, time_t* outTimePtr);
		/* 获取日志文件的路径和文件名,根据入参决定是否附加.log后缀,最后返回该字符串 */
        char*           GetLogPath(char *extension);
        
        // To make sure what happens in Run doesn't also happen at the same time in the public functions.
        OSMutex         fMutex;
};

#endif // __QTSS_ROLLINGLOG_H__

