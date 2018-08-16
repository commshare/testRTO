/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSRollingLog.cpp
Description: A log toolkit, log can roll either by time or by size, clients
             must derive off of this object to provide configuration information. 
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <time.h> /* 这个很重要!很多处涉及时间的处理 */
#include <math.h> /* 使用::floor() */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>   
#include <sys/stat.h>/* 使用::stat()获取文件信息 */
#include <sys/time.h>
#include <errno.h>

#include "SafeStdLib.h"
#include "QTSSRollingLog.h"
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "ResizeableStringFormatter.h"

// Set this to true to get the log to close the file between writes.
/* 注意函数QTSSRollingLog::SetCloseOnWrite()与此有关,设置参见QTSServerPrefs::RereadServerPreferences() */
/* 对这个量的理解很关键:设置向日志文件中写入数据后关闭日志文件 */
static Bool16 sCloseOnWrite = true;

 QTSSRollingLog::QTSSRollingLog() :     
    fLog(NULL), 
    fLogCreateTime(-1),
    fLogFullPath(NULL),
    fAppendDotLog(true),/* 默认附加后缀".log" */
    fLogging(true)/* 可以写日志 */
{
    this->SetTaskName("QTSSRollingLog");
}

// Task object. Do not delete directly
/* 关闭日志,删除日志目录 */
QTSSRollingLog::~QTSSRollingLog()
{
    //
    // Log should already be closed, but just in case...
    this->CloseLog();
    delete [] fLogFullPath;
}

// Set this to true to get the log to close the file between writes.used in QTSServerPrefs::RereadServerPreferences()/:SetCloseLogsOnWrite()
/* 设置向日志文件中写入数据后是否关闭日志文件? */
void QTSSRollingLog::SetCloseOnWrite(Bool16 closeOnWrite) 
{ 
    sCloseOnWrite = closeOnWrite; 
}

/* 现在可以记录日志吗? 当日志文件关闭或日志文件存在时,就可以记录日志. */
Bool16  QTSSRollingLog::IsLogEnabled() 
{ 
    return sCloseOnWrite || (fLog != NULL); 
}

/* 以追加方式打开日志文件,检查是否滚动日志? 将入参指定数据以字符串形式追加写入日志文件中,关闭日志文件 */
void QTSSRollingLog::WriteToLog(char* inLogData, Bool16 allowLogToRoll)
{
    OSMutexLocker locker(&fMutex);
    
	/* 现在记录Log吗? */
    if (fLogging == false)
        return;
     
	/* 若没有日志文件且写日志文件已关闭 */
    if (sCloseOnWrite && fLog == NULL)
		/* 以追加方式打开日志文件 */
        this->EnableLog(fAppendDotLog ); //re-open log file before we write
    
	/* 当允许日志文件滚动时,检查是否滚动日志? */
    if (allowLogToRoll)
        (void)this->CheckRollLog();
        
    if (fLog != NULL)
    {
		/* 将指定数据以字符串形式追加写入日志文件中 */
        qtss_fprintf(fLog, "%s", inLogData);
		/* 更新buffer */
        ::fflush(fLog);
    }
    
	/* 写完日志关闭日志文件吗? */
    if (sCloseOnWrite)
        this->CloseLog( false );
}

/* 重命名旧日志文件(添加日期和.log),重新打开日志文件,再次配置各数据成员的值 */
Bool16 QTSSRollingLog::RollLog()
{
    OSMutexLocker locker(&fMutex);
    
    //returns false if an error occurred, true otherwise

    //close the old file.
    if (fLog != NULL)
        this->CloseLog();
     
	/* 现在开始记录log吗? */
    if (fLogging == false)
        return false;
 
    //rename the old file
	/* 重新命名old log File,将新的命名赋给入参fLogFullPath */
    Bool16 result = this->RenameLogFile(fLogFullPath);
    if (result)
		/* 若命名成功,重新打开日志文件,再次配置各数据成员的值  */
        this->EnableLog(fAppendDotLog);//re-opens log file

    return result;
}

/* 获取日志文件的路径和文件名,根据入参决定是否附加.log后缀,最后返回该字符串 */
char* QTSSRollingLog::GetLogPath(char *extension)
{
    char *thePath = NULL;
    
	/* 注意由派生类得到GetLogDir()和GetLogName() */
    OSCharArrayDeleter logDir(this->GetLogDir()); //The string passed into this function is a copy
    OSCharArrayDeleter logName(this->GetLogName());  //The string passed into this function is a copy
    
    ResizeableStringFormatter formatPath(NULL,0); //allocate the buffer
	/* 得到文件的路径 */
    formatPath.PutFilePath(logDir, logName);
    
	/* 将非空的扩展名放在文件路径后面 */
    if ( extension != NULL)
        formatPath.Put(extension);
    /* 紧接着附加'\0' */    
    formatPath.PutTerminator();

	/* 获取新的文件路径(带扩展名和'\0') */
    thePath = formatPath.GetBufPtr();
    
	/* 重新初始化formatPath成员变量以备下次使用,但注意不要删去buffer的内容,因为我们正要作为结果返回. */
    formatPath.Set(NULL,0); //don't delete buffer we are returning the path as  a result
    
    return thePath;
}

/* 主要就是打开日志文件 */
/* 类似构造函数,配置各数据成员(fLogging除外),以追加方式打开指定目录的Log文件,配置Log文件的生成日期 */
void QTSSRollingLog::EnableLog( Bool16 appendDotLog )
{
   //
    // Start this object running!
    this->Signal(Task::kStartEvent);

    OSMutexLocker locker(&fMutex);

	/* 由入参确定是否附加后缀.log? */
    fAppendDotLog = appendDotLog;
    
	/* 记录Log吗?不记录,立即返回 */
    if (fLogging == false)
        return;

	/* 准备添加后缀".log" */
	/* 设置extension的值(带".log"或不带) */
    char *extension = ".log";
    if (!appendDotLog)
        extension = NULL;
    
	/* 清空并重新获得Log路径 */
    delete[] fLogFullPath;
	/* 获取日志文件的路径和文件名,根据入参决定是否附加.log后缀,最后返回该字符串 */
    fLogFullPath = this->GetLogPath(extension);

    //we need to make sure that when we create a new log file, we write the
    //log header at the top

	/* 获取入参指定路径的文件信息,根据返回结果判断该文件是否存在 */
    Bool16 logExists = this->DoesFileExist(fLogFullPath);
    
    //create the log directory if it doesn't already exist
    if (!logExists)
    {
       OSCharArrayDeleter tempDir(this->GetLogDir());
	   /* 利用OS::MakeDir()逐步生成指定的文件目录 */
       OS::RecursiveMakeDir(tempDir.GetObject());
    }
 
	/* 以追加方式打开指定目录的Log文件,返回Log文件句柄 */
    fLog = ::fopen(fLogFullPath, "a+");//open for "append"
	/* 配置Log文件的生成日期 */
    if (NULL != fLog)
    { 
        if (!logExists) //the file is new, write a log header with the create time of the file.
        {    /* 对新log,将日志生成时间写入日志头 */
			fLogCreateTime = this->WriteLogHeader(fLog);
        }
        else            //the file is old, read the log header to find the create time of the file.
			/* 从Log文件开头读取theFileCreateTime并作恰当处理后转换成从公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数,也返回该数 */
            fLogCreateTime = this->ReadLogHeader(fLog);/* 这个日志生成时间经过原生成时间的处理 */
    }
}

/* 关闭Log文件 */
void QTSSRollingLog::CloseLog( Bool16 leaveEnabled )
{
    OSMutexLocker locker(&fMutex);
    
	/* 现在能离开吗? */
    if (leaveEnabled)
		/* 暂时关闭日志文件 */
        sCloseOnWrite = true;

	/* 假如Log文件存在,就关闭它 */
    if (fLog != NULL)
    {
        ::fclose(fLog);
        fLog = NULL;
    }
}

//returns false if some error has occurred
/* used in PrintStatus() in RunServer.cpp, QTSSAccessLog::WriteLogHeader*/
/* 获取当前时间,并以指定格式YYYY-MM-DD HH:MM:SS(GMT或local time)存放在指定大小的缓存中,返回true */
Bool16 QTSSRollingLog::FormatDate(char *ioDateBuffer, Bool16 logTimeInGMT)
{
	/* 确保数据缓存非空,以存放指定格式的时间 */
    Assert(NULL != ioDateBuffer);
    
    //use ansi routines for getting the date.
	/* 得到1970-1-1以后的时间(s) */
    time_t calendarTime = ::time(NULL);
    Assert(-1 != calendarTime);
    if (-1 == calendarTime)
        return false;
    
	/* 构造表示时间的结构体,参见time.h */
    struct tm* theTime = NULL;
    struct tm  timeResult;
    
	/* 由第二个入参确定是GMT还是local time?从而获取GMT或local格式的当前时间 */
    if (logTimeInGMT)
        theTime = ::qtss_gmtime(&calendarTime, &timeResult);
    else
        theTime = qtss_localtime(&calendarTime, &timeResult);
    
    Assert(NULL != theTime);
    
    if (NULL == theTime)
        return false;
        
    // date time needs to look like this for extended log file format: 2001-03-16 23:34:54
    // this wonderful ANSI routine just does it for you.
    // the format is YYYY-MM-DD HH:MM:SS
    // the date time is in GMT, unless logTimeInGMT is false, in which case
    // the time logged is local time
    //qtss_strftime(ioDateBuffer, kMaxDateBufferSize, "%d/%b/%Y:%H:%M:%S", theLocalTime);
	/* 按照格式字符串命令将上述时间信息以指定格式存放在指定大小的缓存中 */
    qtss_strftime(ioDateBuffer, kMaxDateBufferSizeInBytes, "%Y-%m-%d %H:%M:%S", theTime);  
    return true;
}

/* 检查是否Roll Log?首先,若当前时间与生成日志文件的时间间隔超过日志滚动间隔,就滚动日志,然后,当日志文件的位置指针超过最大日志字节数,就滚动日志 */
Bool16 QTSSRollingLog::CheckRollLog()
{
    //returns false if an error occurred, true otherwise
    if (fLog == NULL)
        return true;
    
    //first check to see if log rolling should happen because of a date interval.
    //This takes precedence over(比...优先处理) size based log rolling
    // this is only if a client connects just between 00:00:00 and 00:01:00
    // since the task object runs every minute
    
    // when an entry is written to the log file, only the file size must be checked
    // to see if it exceeded the limits
    
    // the roll interval should be monitored in a task object 
    // and rolled at midnight if the creation time has exceeded.
	/* 当前时间间隔若超过日志滚动间隔就滚动日志 */
    if ((-1 != fLogCreateTime) && (0 != this->GetRollIntervalInDays()))
    {   
        /* 将日志生成时间fLogCreateTime调整成midnight后再转换成自1970-1-1 00:00:00.以来的秒数 */
        time_t logCreateTimeMidnight = -1;
        QTSSRollingLog::ResetToMidnight(&fLogCreateTime, &logCreateTimeMidnight);
        Assert(logCreateTimeMidnight != -1);
        
		/* 获取当前时间(s) */
        time_t calendarTime = ::time(NULL);

        Assert(-1 != calendarTime);
        if (-1 != calendarTime)
        {
			/* 计算当前时间和日志创建时间之间的时间差值并取整(s) */
			/* 精确的间隔 */
            double theExactInterval = ::difftime(calendarTime, logCreateTimeMidnight);
			/* 取整后的间隔 */
            SInt32 theCurInterval = (SInt32)::floor(theExactInterval);
            
            //transfer_roll_interval is in days, theCurInterval is in seconds
			/* 统一滚动(Roll)的间隔的时间单位(s) */
            SInt32 theRollInterval = this->GetRollIntervalInDays() * 60 * 60 * 24;
			/* 当前时间间隔若超过日志滚动间隔就滚动日志 */
            if (theCurInterval > theRollInterval)
                return this->RollLog();
        }
    }
    
    
    //now check size based log rolling
	/* 获取日志文件的位置指针 */
    UInt32 theCurrentPos = ::ftell(fLog);
    //max_transfer_log_size being 0 is a signal to ignore the setting.
	/* 当日志文件的位置指针超过最大日志字节数就滚动日志 */
    if ((this->GetMaxLogBytes() != 0) &&
        (theCurrentPos > this->GetMaxLogBytes()))
        return this->RollLog();
    return true;
}

/* 取得并生成Log目录,分配适当大小的缓存存放一个没有重名的新命名(附加日期和.log)后的文件名,并赋给入参.成功返回true,失败返回false */
Bool16 QTSSRollingLog::RenameLogFile(const char* inFileName)
{
    //returns false if an error occurred, true otherwise

    //this function takes care of renaming a log file from "myLogFile.log" to
    //"myLogFile.981217000.log" or if that is already taken, myLogFile.981217001.log", etc 
    
    //fix 2287086. Rolled log name can be different than original log name

    //GetLogDir returns a copy of the log dir
	/* 得到Log Directory */
    OSCharArrayDeleter logDirectory(this->GetLogDir());

    //create the log directory if it doesn't already exist
	/* 生成指定路径(char*)上任何不存在的目录 */
    OS::RecursiveMakeDir(logDirectory.GetObject());
    
    //GetLogName returns a copy of the log name
	/* 得到Log文件名称 */
    OSCharArrayDeleter logBaseName(this->GetLogName());
        
    //QTStreamingServer.981217003.log
    //format the new file name
	/* 分配存放新文件名称(目录\文件名+.+.)的缓存 */
    OSCharArrayDeleter theNewNameBuffer(NEW char[::strlen(logDirectory) + kMaxFilenameLengthInBytes + 3]);
    
    //copy over the directory - append a '/' if it's missing
	/* 将Log文件路径复制到新建的缓存中,若末尾不是'/',就增加一个'/' */
    ::strcpy(theNewNameBuffer, logDirectory);
    if (theNewNameBuffer[::strlen(theNewNameBuffer)-1] != kPathDelimiterChar)
    {
        ::strcat(theNewNameBuffer, kPathDelimiterString);
    }
    
    //copy over the base filename
	/* 再将Log文件名称也复制进来 */
    ::strcat(theNewNameBuffer, logBaseName.GetObject());

    //append the date the file was created
	/* 获取Log文件生成的local time,转换成指定格式,放在buffer中 */
    struct tm  timeResult;
	/* 将数据成员转换成local time */
    struct tm* theLocalTime = qtss_localtime(&fLogCreateTime, &timeResult);
    char timeString[10];
	/* 将local time转换成指定格式(.),放在buffer中 */
    qtss_strftime(timeString,  10, ".%y%m%d", theLocalTime);
	/* 将时间信息附加到缓存中 */
    ::strcat(theNewNameBuffer, timeString);
    
	/* (现在统计)获取缓存中字符串的长度(BaseNameLength) */
    SInt32 theBaseNameLength = ::strlen(theNewNameBuffer);

    //loop until we find a unique name to rename this file
    //and append the log number and suffix
    SInt32 theErr = 0;
	/* 注意只进行最多1000次循环,来找到一个没有重名的(append the log number and suffix命名后的)文件 */
    for (SInt32 x = 0; (theErr == 0) && (x<=1000); x++)
    {
        if (x  == 1000) //we don't have any digits left, so just reuse the "---" until tomorrow...
        {
            //add a bogus(假的,伪造的) log number and exit the loop
			/* 在Base Name后append 一个示例的名字,并退出 */
            qtss_sprintf(theNewNameBuffer + theBaseNameLength, "---.log");
            break;
        }

        //add the log number & suffix
        qtss_sprintf(theNewNameBuffer + theBaseNameLength, "%03ld.log", x);

        //assume that when ::stat returns an error, it is becase
        //the file doesnt exist. Once that happens, we have a unique name
        // csl - shouldn't you watch for a ENOENT result?
		/* 从指定的文件路径获取文件相关信息,若返回error,则说明不存在该文件,我们的目标达到了 */
        struct stat theIdontCare;
        theErr = ::stat(theNewNameBuffer, &theIdontCare);
		/* 当不满足条件时给出错误信息 */
        WarnV((theErr == 0 || OSThread::GetErrno() == ENOENT), "unexpected stat error in RenameLogFile");      
    }
    
    //rename the file. Use posix rename function
	/* 将得到的新的文件名赋给入参inFileName,成功返回0,失败返回-1 */
    int result = ::rename(inFileName, theNewNameBuffer);
    if (result == -1)
        theErr = (SInt32)OSThread::GetErrno();
    else
        theErr = 0;
    
	/* 当不满足条件时给出错误信息 */
    WarnV(theErr == 0 , "unexpected rename error in RenameLogFile");

    
    if (theErr != 0)
        return false;
    else
        return true;    
}

/* 获取入参指定路径的文件信息,根据返回结果判断该文件是否存在 */
Bool16 QTSSRollingLog::DoesFileExist(const char *inPath)
{
    struct stat theStat;
	/* 获取指定路径的文件信息 */
    int theErr = ::stat(inPath, &theStat);

    if (theErr != 0)
        return false;
    else
        return true;
}

/* 将生成日志文件的local时间追加进日志文件中,再从日志文件中读出并返回UTC时间 */
time_t QTSSRollingLog::WriteLogHeader(FILE* inFile)
{
    OSMutexLocker locker(&fMutex);

    //The point of this header is to record the exact time the log file was created,
    //in a format that is easy to parse through whenever we open the file again.
    //This is necessary to support log rolling based on a time interval, and POSIX doesn't
    //support a create date in files.
	/* 获取当前时间(自1970-1-1 00:00:00以来的秒数) */
    time_t calendarTime = ::time(NULL);
    Assert(-1 != calendarTime);
    if (-1 == calendarTime)
        return -1;

	/* 将当前时间转换成local time的tm结构体 */
    struct tm  timeResult;
    struct tm* theLocalTime = qtss_localtime(&calendarTime, &timeResult);
    Assert(NULL != theLocalTime);
    if (NULL == theLocalTime)
        return -1;
    
    //
    // Files are always created at hour 0 (we don't care about the time, we always
    // want them to roll at midnight.
    //theLocalTime->tm_hour = 0;
    //theLocalTime->tm_min = 0;
    //theLocalTime->tm_sec = 0;

	/* 将获得的local time以指定的格式存入缓存 */
    char tempbuf[1024];
    qtss_strftime(tempbuf, sizeof(tempbuf), "#Log File Created On: %m/%d/%Y %H:%M:%S\n", theLocalTime);
    //qtss_sprintf(tempbuf, "#Log File Created On: %d/%d/%d %d:%d:%d %d:%d:%d GMT\n",
    //          theLocalTime->tm_mon, theLocalTime->tm_mday, theLocalTime->tm_year,
    //          theLocalTime->tm_hour, theLocalTime->tm_min, theLocalTime->tm_sec,
    //          theLocalTime->tm_yday, theLocalTime->tm_wday, theLocalTime->tm_isdst);
	/* 以追加方式打开日志文件,检查是否滚动日志? 将tempbuf中的数据以字符串形式追加写入日志文件中,关闭日志文件 */
    this->WriteToLog(tempbuf, !kAllowLogToRoll);
    
	/* 从Log文件开头读取theFileCreateTime并作恰当处理后转换成从公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数,也返回该数 */
    return this->ReadLogHeader(inFile);
}

/* 从Log文件开头读取theFileCreateTime并作恰当处理后转换成从公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数,也返回该数 */
time_t QTSSRollingLog::ReadLogHeader(FILE* inFile)
{
    OSMutexLocker locker(&fMutex);

    //This function reads the header in a log file, returning the time stored
    //at the beginning of this file. This value is used to determine when to
    //roll the log.
    //Returns -1 if the header is bogus(假的). In that case, just ignore time based log rolling

    //first seek to the beginning of the file
	/* 获取打开Log文件的当前位置(相对文件开头的偏移量) */
    SInt32 theCurrentPos = ::ftell(inFile);
    if (theCurrentPos == -1)
        return -1;
	/* 重设文件流的读写位置为文件开头 */
    (void)::rewind(inFile);

	/* 新建指定长度的缓存 */
    const UInt32 kMaxHeaderLength = 500;
    char theFirstLine[kMaxHeaderLength];
    
	/* 从打开文件内读入指定长度(500个字符)到缓存,若失败了返回-1 */
    if (NULL == ::fgets(theFirstLine, kMaxHeaderLength, inFile))
    {
		/* 将读写位置移动到文件尾 */
        ::fseek(inFile, 0, SEEK_END);
        return -1;
    }
    /* 将读写位置移动到文件尾 */
    ::fseek(inFile, 0, SEEK_END);
    
    struct tm theFileCreateTime;
    
    // Zero out fields we will not be using
	/* 将tm结构体中不会用到的9个成员变量中的3个赋值 */
    theFileCreateTime.tm_isdst = -1;/* daylight savings time flag */
    theFileCreateTime.tm_wday = 0;/* days since Sunday - [0,6] */
    theFileCreateTime.tm_yday = 0;/* days since January 1 - [0,365] */
    
    //if (EOF == ::sscanf(theFirstLine, "#Log File Created On: %d/%d/%d %d:%d:%d\n",
    //          &theFileCreateTime.tm_mon, &theFileCreateTime.tm_mday, &theFileCreateTime.tm_year,
    //          &theFileCreateTime.tm_hour, &theFileCreateTime.tm_min, &theFileCreateTime.tm_sec))
    //  return -1;
    
    //
    // We always want to roll at hour 0, so ignore the time of creation
    
	/* 从指定字符串获取指定格式的数据(配置tm结构体中剩下的6个变量),注意::sscanf()功能十分强大 */
    if (EOF == ::sscanf(theFirstLine, "#Log File Created On: %d/%d/%d %d:%d:%d\n",
                &theFileCreateTime.tm_mon, &theFileCreateTime.tm_mday, &theFileCreateTime.tm_year,
                &theFileCreateTime.tm_hour, &theFileCreateTime.tm_min, &theFileCreateTime.tm_sec))
        return -1;

    //
    // It should be like this anyway, but if the log file is legacy, then...
    // No! The log file will have the actual time in it but we shall return the exact time
    //theFileCreateTime.tm_hour = 0;
    //theFileCreateTime.tm_min = 0;
    //theFileCreateTime.tm_sec = 0;
    
    // Actually, it seems like all platforms need this.
//#ifdef __Win32__
    // Win32 has slightly different atime basis than UNIX.
    theFileCreateTime.tm_yday--;
    theFileCreateTime.tm_mon--;
    theFileCreateTime.tm_year -= 1900;
//#endif

#if 0
    //use ansi routines for getting the date.
    time_t calendarTime = ::time(NULL);
    Assert(-1 != calendarTime);
    if (-1 == calendarTime)
        return false;
        
    struct tm  timeResult;
    struct tm* theLocalTime = qtss_localtime(&calendarTime, &timeResult);
    Assert(NULL != theLocalTime);
    if (NULL == theLocalTime)
        return false;
#endif

    //ok, we should have a filled in tm struct. Convert it to a time_t.
    //time_t thePoopTime = ::mktime(theLocalTime);
	/* 将参数所指的tm结构数据转换成从公元1970年1月1日0时0分0秒算起至今的UTC时间所经过的秒数 */
    time_t theTime = ::mktime(&theFileCreateTime);
    return theTime;
}

/* 若当前时间与生成日志文件的时间间隔超过日志滚动间隔,写入日志文件(重命名旧日志文件(添加日期和.log),重新打开日志文件,再次配置各数据成员的值),每分钟滚动一次Log */
SInt64 QTSSRollingLog::Run()
{
    //
    // If we are going away, just return
	//获取该任务的所有event flag bit
    EventFlags events = this->GetEvents();
	//如果有Kill event, 立即返回-1,TaskThread会立即将该任务删去!
    if (events & Task::kKillEvent)
        return -1;
    
    OSMutexLocker locker(&fMutex);
    
	/* 获取Roll的时间间隔(s) */
    UInt32 theRollInterval = (this->GetRollIntervalInDays())  * 60 * 60 * 24;
    
	/* 当Log创建时间和Log文件句柄非空 */
    if((fLogCreateTime != -1) && (fLog != NULL))
    {
		/* 设置logRollTimeMidnight */
        time_t logRollTimeMidnight = -1;
		/* 将日志生成时间fLogCreateTime重置成午夜时间midnight后再转换成自1970-1-1 00:00:00.以来的秒数 */
        this->ResetToMidnight(&fLogCreateTime, &logRollTimeMidnight);
        Assert(logRollTimeMidnight != -1);
        
        if(theRollInterval != 0)
        {
			/* 获取当前时间(自1970-1-1 00:00:00以来的秒数) */
            time_t calendarTime = ::time(NULL);
            Assert(-1 != calendarTime);
			/* 返回当前时间和logRollTimeMidnight之间的差值(s) */
            double theExactInterval = ::difftime(calendarTime, logRollTimeMidnight);/* 精确地间隔 */
            if(theExactInterval > 0) {
                /* 获取不大于精确地间隔theExactInterval的整数 */
                UInt32 theCurInterval = (UInt32)::floor(theExactInterval);
                if (theCurInterval >= theRollInterval)
					/* 重命名旧日志文件(添加日期和.log),再次配置各数据成员的值 */
                    this->RollLog();
            }
        }
    }
    return 60 * 1000;
}

/* 将给定时间设置成午夜时间midnight后再转换成自1970-1-1 00:00:00.以来的秒数 */
void QTSSRollingLog::ResetToMidnight(time_t* inTimePtr, time_t* outTimePtr) 
{
    if(*inTimePtr == -1)
    {
        *outTimePtr = -1;
        return;
    }
    
	/* 获取local time */
    struct tm  timeResult;
    struct tm* theLocalTime = qtss_localtime(inTimePtr, &timeResult);
    Assert(theLocalTime != NULL);

	/* reset to midnight */
	/* 重置为午夜时间,使时间延后了 */
    theLocalTime->tm_hour = 0;
    theLocalTime->tm_min = 0;
    theLocalTime->tm_sec = 0;
    
    // some weird stuff
    //theLocalTime->tm_yday--;
    //theLocalTime->tm_mon--;
    //theLocalTime->tm_year -= 1900;

	/* 将设置成midnight的local时间再次转换成自1970-1-1 00:00:00.以来的秒数 */
    *outTimePtr = ::mktime(theLocalTime);

}
