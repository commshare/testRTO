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


#include <time.h> /* �������Ҫ!�ܶദ�漰ʱ��Ĵ��� */
#include <math.h> /* ʹ��::floor() */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>   
#include <sys/stat.h>/* ʹ��::stat()��ȡ�ļ���Ϣ */
#include <sys/time.h>
#include <errno.h>

#include "SafeStdLib.h"
#include "QTSSRollingLog.h"
#include "OS.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"
#include "ResizeableStringFormatter.h"

// Set this to true to get the log to close the file between writes.
/* ע�⺯��QTSSRollingLog::SetCloseOnWrite()����й�,���òμ�QTSServerPrefs::RereadServerPreferences() */
/* ������������ܹؼ�:��������־�ļ���д�����ݺ�ر���־�ļ� */
static Bool16 sCloseOnWrite = true;

 QTSSRollingLog::QTSSRollingLog() :     
    fLog(NULL), 
    fLogCreateTime(-1),
    fLogFullPath(NULL),
    fAppendDotLog(true),/* Ĭ�ϸ��Ӻ�׺".log" */
    fLogging(true)/* ����д��־ */
{
    this->SetTaskName("QTSSRollingLog");
}

// Task object. Do not delete directly
/* �ر���־,ɾ����־Ŀ¼ */
QTSSRollingLog::~QTSSRollingLog()
{
    //
    // Log should already be closed, but just in case...
    this->CloseLog();
    delete [] fLogFullPath;
}

// Set this to true to get the log to close the file between writes.used in QTSServerPrefs::RereadServerPreferences()/:SetCloseLogsOnWrite()
/* ��������־�ļ���д�����ݺ��Ƿ�ر���־�ļ�? */
void QTSSRollingLog::SetCloseOnWrite(Bool16 closeOnWrite) 
{ 
    sCloseOnWrite = closeOnWrite; 
}

/* ���ڿ��Լ�¼��־��? ����־�ļ��رջ���־�ļ�����ʱ,�Ϳ��Լ�¼��־. */
Bool16  QTSSRollingLog::IsLogEnabled() 
{ 
    return sCloseOnWrite || (fLog != NULL); 
}

/* ��׷�ӷ�ʽ����־�ļ�,����Ƿ������־? �����ָ���������ַ�����ʽ׷��д����־�ļ���,�ر���־�ļ� */
void QTSSRollingLog::WriteToLog(char* inLogData, Bool16 allowLogToRoll)
{
    OSMutexLocker locker(&fMutex);
    
	/* ���ڼ�¼Log��? */
    if (fLogging == false)
        return;
     
	/* ��û����־�ļ���д��־�ļ��ѹر� */
    if (sCloseOnWrite && fLog == NULL)
		/* ��׷�ӷ�ʽ����־�ļ� */
        this->EnableLog(fAppendDotLog ); //re-open log file before we write
    
	/* ��������־�ļ�����ʱ,����Ƿ������־? */
    if (allowLogToRoll)
        (void)this->CheckRollLog();
        
    if (fLog != NULL)
    {
		/* ��ָ���������ַ�����ʽ׷��д����־�ļ��� */
        qtss_fprintf(fLog, "%s", inLogData);
		/* ����buffer */
        ::fflush(fLog);
    }
    
	/* д����־�ر���־�ļ���? */
    if (sCloseOnWrite)
        this->CloseLog( false );
}

/* ����������־�ļ�(������ں�.log),���´���־�ļ�,�ٴ����ø����ݳ�Ա��ֵ */
Bool16 QTSSRollingLog::RollLog()
{
    OSMutexLocker locker(&fMutex);
    
    //returns false if an error occurred, true otherwise

    //close the old file.
    if (fLog != NULL)
        this->CloseLog();
     
	/* ���ڿ�ʼ��¼log��? */
    if (fLogging == false)
        return false;
 
    //rename the old file
	/* ��������old log File,���µ������������fLogFullPath */
    Bool16 result = this->RenameLogFile(fLogFullPath);
    if (result)
		/* �������ɹ�,���´���־�ļ�,�ٴ����ø����ݳ�Ա��ֵ  */
        this->EnableLog(fAppendDotLog);//re-opens log file

    return result;
}

/* ��ȡ��־�ļ���·�����ļ���,������ξ����Ƿ񸽼�.log��׺,��󷵻ظ��ַ��� */
char* QTSSRollingLog::GetLogPath(char *extension)
{
    char *thePath = NULL;
    
	/* ע����������õ�GetLogDir()��GetLogName() */
    OSCharArrayDeleter logDir(this->GetLogDir()); //The string passed into this function is a copy
    OSCharArrayDeleter logName(this->GetLogName());  //The string passed into this function is a copy
    
    ResizeableStringFormatter formatPath(NULL,0); //allocate the buffer
	/* �õ��ļ���·�� */
    formatPath.PutFilePath(logDir, logName);
    
	/* ���ǿյ���չ�������ļ�·������ */
    if ( extension != NULL)
        formatPath.Put(extension);
    /* �����Ÿ���'\0' */    
    formatPath.PutTerminator();

	/* ��ȡ�µ��ļ�·��(����չ����'\0') */
    thePath = formatPath.GetBufPtr();
    
	/* ���³�ʼ��formatPath��Ա�����Ա��´�ʹ��,��ע�ⲻҪɾȥbuffer������,��Ϊ������Ҫ��Ϊ�������. */
    formatPath.Set(NULL,0); //don't delete buffer we are returning the path as  a result
    
    return thePath;
}

/* ��Ҫ���Ǵ���־�ļ� */
/* ���ƹ��캯��,���ø����ݳ�Ա(fLogging����),��׷�ӷ�ʽ��ָ��Ŀ¼��Log�ļ�,����Log�ļ����������� */
void QTSSRollingLog::EnableLog( Bool16 appendDotLog )
{
   //
    // Start this object running!
    this->Signal(Task::kStartEvent);

    OSMutexLocker locker(&fMutex);

	/* �����ȷ���Ƿ񸽼Ӻ�׺.log? */
    fAppendDotLog = appendDotLog;
    
	/* ��¼Log��?����¼,�������� */
    if (fLogging == false)
        return;

	/* ׼����Ӻ�׺".log" */
	/* ����extension��ֵ(��".log"�򲻴�) */
    char *extension = ".log";
    if (!appendDotLog)
        extension = NULL;
    
	/* ��ղ����»��Log·�� */
    delete[] fLogFullPath;
	/* ��ȡ��־�ļ���·�����ļ���,������ξ����Ƿ񸽼�.log��׺,��󷵻ظ��ַ��� */
    fLogFullPath = this->GetLogPath(extension);

    //we need to make sure that when we create a new log file, we write the
    //log header at the top

	/* ��ȡ���ָ��·�����ļ���Ϣ,���ݷ��ؽ���жϸ��ļ��Ƿ���� */
    Bool16 logExists = this->DoesFileExist(fLogFullPath);
    
    //create the log directory if it doesn't already exist
    if (!logExists)
    {
       OSCharArrayDeleter tempDir(this->GetLogDir());
	   /* ����OS::MakeDir()������ָ�����ļ�Ŀ¼ */
       OS::RecursiveMakeDir(tempDir.GetObject());
    }
 
	/* ��׷�ӷ�ʽ��ָ��Ŀ¼��Log�ļ�,����Log�ļ���� */
    fLog = ::fopen(fLogFullPath, "a+");//open for "append"
	/* ����Log�ļ����������� */
    if (NULL != fLog)
    { 
        if (!logExists) //the file is new, write a log header with the create time of the file.
        {    /* ����log,����־����ʱ��д����־ͷ */
			fLogCreateTime = this->WriteLogHeader(fLog);
        }
        else            //the file is old, read the log header to find the create time of the file.
			/* ��Log�ļ���ͷ��ȡtheFileCreateTime����ǡ�������ת���ɴӹ�Ԫ1970��1��1��0ʱ0��0�����������UTCʱ��������������,Ҳ���ظ��� */
            fLogCreateTime = this->ReadLogHeader(fLog);/* �����־����ʱ�侭��ԭ����ʱ��Ĵ��� */
    }
}

/* �ر�Log�ļ� */
void QTSSRollingLog::CloseLog( Bool16 leaveEnabled )
{
    OSMutexLocker locker(&fMutex);
    
	/* �������뿪��? */
    if (leaveEnabled)
		/* ��ʱ�ر���־�ļ� */
        sCloseOnWrite = true;

	/* ����Log�ļ�����,�͹ر��� */
    if (fLog != NULL)
    {
        ::fclose(fLog);
        fLog = NULL;
    }
}

//returns false if some error has occurred
/* used in PrintStatus() in RunServer.cpp, QTSSAccessLog::WriteLogHeader*/
/* ��ȡ��ǰʱ��,����ָ����ʽYYYY-MM-DD HH:MM:SS(GMT��local time)�����ָ����С�Ļ�����,����true */
Bool16 QTSSRollingLog::FormatDate(char *ioDateBuffer, Bool16 logTimeInGMT)
{
	/* ȷ�����ݻ���ǿ�,�Դ��ָ����ʽ��ʱ�� */
    Assert(NULL != ioDateBuffer);
    
    //use ansi routines for getting the date.
	/* �õ�1970-1-1�Ժ��ʱ��(s) */
    time_t calendarTime = ::time(NULL);
    Assert(-1 != calendarTime);
    if (-1 == calendarTime)
        return false;
    
	/* �����ʾʱ��Ľṹ��,�μ�time.h */
    struct tm* theTime = NULL;
    struct tm  timeResult;
    
	/* �ɵڶ������ȷ����GMT����local time?�Ӷ���ȡGMT��local��ʽ�ĵ�ǰʱ�� */
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
	/* ���ո�ʽ�ַ����������ʱ����Ϣ��ָ����ʽ�����ָ����С�Ļ����� */
    qtss_strftime(ioDateBuffer, kMaxDateBufferSizeInBytes, "%Y-%m-%d %H:%M:%S", theTime);  
    return true;
}

/* ����Ƿ�Roll Log?����,����ǰʱ����������־�ļ���ʱ����������־�������,�͹�����־,Ȼ��,����־�ļ���λ��ָ�볬�������־�ֽ���,�͹�����־ */
Bool16 QTSSRollingLog::CheckRollLog()
{
    //returns false if an error occurred, true otherwise
    if (fLog == NULL)
        return true;
    
    //first check to see if log rolling should happen because of a date interval.
    //This takes precedence over(��...���ȴ���) size based log rolling
    // this is only if a client connects just between 00:00:00 and 00:01:00
    // since the task object runs every minute
    
    // when an entry is written to the log file, only the file size must be checked
    // to see if it exceeded the limits
    
    // the roll interval should be monitored in a task object 
    // and rolled at midnight if the creation time has exceeded.
	/* ��ǰʱ������������־��������͹�����־ */
    if ((-1 != fLogCreateTime) && (0 != this->GetRollIntervalInDays()))
    {   
        /* ����־����ʱ��fLogCreateTime������midnight����ת������1970-1-1 00:00:00.���������� */
        time_t logCreateTimeMidnight = -1;
        QTSSRollingLog::ResetToMidnight(&fLogCreateTime, &logCreateTimeMidnight);
        Assert(logCreateTimeMidnight != -1);
        
		/* ��ȡ��ǰʱ��(s) */
        time_t calendarTime = ::time(NULL);

        Assert(-1 != calendarTime);
        if (-1 != calendarTime)
        {
			/* ���㵱ǰʱ�����־����ʱ��֮���ʱ���ֵ��ȡ��(s) */
			/* ��ȷ�ļ�� */
            double theExactInterval = ::difftime(calendarTime, logCreateTimeMidnight);
			/* ȡ����ļ�� */
            SInt32 theCurInterval = (SInt32)::floor(theExactInterval);
            
            //transfer_roll_interval is in days, theCurInterval is in seconds
			/* ͳһ����(Roll)�ļ����ʱ�䵥λ(s) */
            SInt32 theRollInterval = this->GetRollIntervalInDays() * 60 * 60 * 24;
			/* ��ǰʱ������������־��������͹�����־ */
            if (theCurInterval > theRollInterval)
                return this->RollLog();
        }
    }
    
    
    //now check size based log rolling
	/* ��ȡ��־�ļ���λ��ָ�� */
    UInt32 theCurrentPos = ::ftell(fLog);
    //max_transfer_log_size being 0 is a signal to ignore the setting.
	/* ����־�ļ���λ��ָ�볬�������־�ֽ����͹�����־ */
    if ((this->GetMaxLogBytes() != 0) &&
        (theCurrentPos > this->GetMaxLogBytes()))
        return this->RollLog();
    return true;
}

/* ȡ�ò�����LogĿ¼,�����ʵ���С�Ļ�����һ��û��������������(�������ں�.log)����ļ���,���������.�ɹ�����true,ʧ�ܷ���false */
Bool16 QTSSRollingLog::RenameLogFile(const char* inFileName)
{
    //returns false if an error occurred, true otherwise

    //this function takes care of renaming a log file from "myLogFile.log" to
    //"myLogFile.981217000.log" or if that is already taken, myLogFile.981217001.log", etc 
    
    //fix 2287086. Rolled log name can be different than original log name

    //GetLogDir returns a copy of the log dir
	/* �õ�Log Directory */
    OSCharArrayDeleter logDirectory(this->GetLogDir());

    //create the log directory if it doesn't already exist
	/* ����ָ��·��(char*)���κβ����ڵ�Ŀ¼ */
    OS::RecursiveMakeDir(logDirectory.GetObject());
    
    //GetLogName returns a copy of the log name
	/* �õ�Log�ļ����� */
    OSCharArrayDeleter logBaseName(this->GetLogName());
        
    //QTStreamingServer.981217003.log
    //format the new file name
	/* ���������ļ�����(Ŀ¼\�ļ���+.+.)�Ļ��� */
    OSCharArrayDeleter theNewNameBuffer(NEW char[::strlen(logDirectory) + kMaxFilenameLengthInBytes + 3]);
    
    //copy over the directory - append a '/' if it's missing
	/* ��Log�ļ�·�����Ƶ��½��Ļ�����,��ĩβ����'/',������һ��'/' */
    ::strcpy(theNewNameBuffer, logDirectory);
    if (theNewNameBuffer[::strlen(theNewNameBuffer)-1] != kPathDelimiterChar)
    {
        ::strcat(theNewNameBuffer, kPathDelimiterString);
    }
    
    //copy over the base filename
	/* �ٽ�Log�ļ�����Ҳ���ƽ��� */
    ::strcat(theNewNameBuffer, logBaseName.GetObject());

    //append the date the file was created
	/* ��ȡLog�ļ����ɵ�local time,ת����ָ����ʽ,����buffer�� */
    struct tm  timeResult;
	/* �����ݳ�Աת����local time */
    struct tm* theLocalTime = qtss_localtime(&fLogCreateTime, &timeResult);
    char timeString[10];
	/* ��local timeת����ָ����ʽ(.),����buffer�� */
    qtss_strftime(timeString,  10, ".%y%m%d", theLocalTime);
	/* ��ʱ����Ϣ���ӵ������� */
    ::strcat(theNewNameBuffer, timeString);
    
	/* (����ͳ��)��ȡ�������ַ����ĳ���(BaseNameLength) */
    SInt32 theBaseNameLength = ::strlen(theNewNameBuffer);

    //loop until we find a unique name to rename this file
    //and append the log number and suffix
    SInt32 theErr = 0;
	/* ע��ֻ�������1000��ѭ��,���ҵ�һ��û��������(append the log number and suffix�������)�ļ� */
    for (SInt32 x = 0; (theErr == 0) && (x<=1000); x++)
    {
        if (x  == 1000) //we don't have any digits left, so just reuse the "---" until tomorrow...
        {
            //add a bogus(�ٵ�,α���) log number and exit the loop
			/* ��Base Name��append һ��ʾ��������,���˳� */
            qtss_sprintf(theNewNameBuffer + theBaseNameLength, "---.log");
            break;
        }

        //add the log number & suffix
        qtss_sprintf(theNewNameBuffer + theBaseNameLength, "%03ld.log", x);

        //assume that when ::stat returns an error, it is becase
        //the file doesnt exist. Once that happens, we have a unique name
        // csl - shouldn't you watch for a ENOENT result?
		/* ��ָ�����ļ�·����ȡ�ļ������Ϣ,������error,��˵�������ڸ��ļ�,���ǵ�Ŀ��ﵽ�� */
        struct stat theIdontCare;
        theErr = ::stat(theNewNameBuffer, &theIdontCare);
		/* ������������ʱ����������Ϣ */
        WarnV((theErr == 0 || OSThread::GetErrno() == ENOENT), "unexpected stat error in RenameLogFile");      
    }
    
    //rename the file. Use posix rename function
	/* ���õ����µ��ļ����������inFileName,�ɹ�����0,ʧ�ܷ���-1 */
    int result = ::rename(inFileName, theNewNameBuffer);
    if (result == -1)
        theErr = (SInt32)OSThread::GetErrno();
    else
        theErr = 0;
    
	/* ������������ʱ����������Ϣ */
    WarnV(theErr == 0 , "unexpected rename error in RenameLogFile");

    
    if (theErr != 0)
        return false;
    else
        return true;    
}

/* ��ȡ���ָ��·�����ļ���Ϣ,���ݷ��ؽ���жϸ��ļ��Ƿ���� */
Bool16 QTSSRollingLog::DoesFileExist(const char *inPath)
{
    struct stat theStat;
	/* ��ȡָ��·�����ļ���Ϣ */
    int theErr = ::stat(inPath, &theStat);

    if (theErr != 0)
        return false;
    else
        return true;
}

/* ��������־�ļ���localʱ��׷�ӽ���־�ļ���,�ٴ���־�ļ��ж���������UTCʱ�� */
time_t QTSSRollingLog::WriteLogHeader(FILE* inFile)
{
    OSMutexLocker locker(&fMutex);

    //The point of this header is to record the exact time the log file was created,
    //in a format that is easy to parse through whenever we open the file again.
    //This is necessary to support log rolling based on a time interval, and POSIX doesn't
    //support a create date in files.
	/* ��ȡ��ǰʱ��(��1970-1-1 00:00:00����������) */
    time_t calendarTime = ::time(NULL);
    Assert(-1 != calendarTime);
    if (-1 == calendarTime)
        return -1;

	/* ����ǰʱ��ת����local time��tm�ṹ�� */
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

	/* ����õ�local time��ָ���ĸ�ʽ���뻺�� */
    char tempbuf[1024];
    qtss_strftime(tempbuf, sizeof(tempbuf), "#Log File Created On: %m/%d/%Y %H:%M:%S\n", theLocalTime);
    //qtss_sprintf(tempbuf, "#Log File Created On: %d/%d/%d %d:%d:%d %d:%d:%d GMT\n",
    //          theLocalTime->tm_mon, theLocalTime->tm_mday, theLocalTime->tm_year,
    //          theLocalTime->tm_hour, theLocalTime->tm_min, theLocalTime->tm_sec,
    //          theLocalTime->tm_yday, theLocalTime->tm_wday, theLocalTime->tm_isdst);
	/* ��׷�ӷ�ʽ����־�ļ�,����Ƿ������־? ��tempbuf�е��������ַ�����ʽ׷��д����־�ļ���,�ر���־�ļ� */
    this->WriteToLog(tempbuf, !kAllowLogToRoll);
    
	/* ��Log�ļ���ͷ��ȡtheFileCreateTime����ǡ�������ת���ɴӹ�Ԫ1970��1��1��0ʱ0��0�����������UTCʱ��������������,Ҳ���ظ��� */
    return this->ReadLogHeader(inFile);
}

/* ��Log�ļ���ͷ��ȡtheFileCreateTime����ǡ�������ת���ɴӹ�Ԫ1970��1��1��0ʱ0��0�����������UTCʱ��������������,Ҳ���ظ��� */
time_t QTSSRollingLog::ReadLogHeader(FILE* inFile)
{
    OSMutexLocker locker(&fMutex);

    //This function reads the header in a log file, returning the time stored
    //at the beginning of this file. This value is used to determine when to
    //roll the log.
    //Returns -1 if the header is bogus(�ٵ�). In that case, just ignore time based log rolling

    //first seek to the beginning of the file
	/* ��ȡ��Log�ļ��ĵ�ǰλ��(����ļ���ͷ��ƫ����) */
    SInt32 theCurrentPos = ::ftell(inFile);
    if (theCurrentPos == -1)
        return -1;
	/* �����ļ����Ķ�дλ��Ϊ�ļ���ͷ */
    (void)::rewind(inFile);

	/* �½�ָ�����ȵĻ��� */
    const UInt32 kMaxHeaderLength = 500;
    char theFirstLine[kMaxHeaderLength];
    
	/* �Ӵ��ļ��ڶ���ָ������(500���ַ�)������,��ʧ���˷���-1 */
    if (NULL == ::fgets(theFirstLine, kMaxHeaderLength, inFile))
    {
		/* ����дλ���ƶ����ļ�β */
        ::fseek(inFile, 0, SEEK_END);
        return -1;
    }
    /* ����дλ���ƶ����ļ�β */
    ::fseek(inFile, 0, SEEK_END);
    
    struct tm theFileCreateTime;
    
    // Zero out fields we will not be using
	/* ��tm�ṹ���в����õ���9����Ա�����е�3����ֵ */
    theFileCreateTime.tm_isdst = -1;/* daylight savings time flag */
    theFileCreateTime.tm_wday = 0;/* days since Sunday - [0,6] */
    theFileCreateTime.tm_yday = 0;/* days since January 1 - [0,365] */
    
    //if (EOF == ::sscanf(theFirstLine, "#Log File Created On: %d/%d/%d %d:%d:%d\n",
    //          &theFileCreateTime.tm_mon, &theFileCreateTime.tm_mday, &theFileCreateTime.tm_year,
    //          &theFileCreateTime.tm_hour, &theFileCreateTime.tm_min, &theFileCreateTime.tm_sec))
    //  return -1;
    
    //
    // We always want to roll at hour 0, so ignore the time of creation
    
	/* ��ָ���ַ�����ȡָ����ʽ������(����tm�ṹ����ʣ�µ�6������),ע��::sscanf()����ʮ��ǿ�� */
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
	/* ��������ָ��tm�ṹ����ת���ɴӹ�Ԫ1970��1��1��0ʱ0��0�����������UTCʱ�������������� */
    time_t theTime = ::mktime(&theFileCreateTime);
    return theTime;
}

/* ����ǰʱ����������־�ļ���ʱ����������־�������,д����־�ļ�(����������־�ļ�(������ں�.log),���´���־�ļ�,�ٴ����ø����ݳ�Ա��ֵ),ÿ���ӹ���һ��Log */
SInt64 QTSSRollingLog::Run()
{
    //
    // If we are going away, just return
	//��ȡ�����������event flag bit
    EventFlags events = this->GetEvents();
	//�����Kill event, ��������-1,TaskThread��������������ɾȥ!
    if (events & Task::kKillEvent)
        return -1;
    
    OSMutexLocker locker(&fMutex);
    
	/* ��ȡRoll��ʱ����(s) */
    UInt32 theRollInterval = (this->GetRollIntervalInDays())  * 60 * 60 * 24;
    
	/* ��Log����ʱ���Log�ļ�����ǿ� */
    if((fLogCreateTime != -1) && (fLog != NULL))
    {
		/* ����logRollTimeMidnight */
        time_t logRollTimeMidnight = -1;
		/* ����־����ʱ��fLogCreateTime���ó���ҹʱ��midnight����ת������1970-1-1 00:00:00.���������� */
        this->ResetToMidnight(&fLogCreateTime, &logRollTimeMidnight);
        Assert(logRollTimeMidnight != -1);
        
        if(theRollInterval != 0)
        {
			/* ��ȡ��ǰʱ��(��1970-1-1 00:00:00����������) */
            time_t calendarTime = ::time(NULL);
            Assert(-1 != calendarTime);
			/* ���ص�ǰʱ���logRollTimeMidnight֮��Ĳ�ֵ(s) */
            double theExactInterval = ::difftime(calendarTime, logRollTimeMidnight);/* ��ȷ�ؼ�� */
            if(theExactInterval > 0) {
                /* ��ȡ�����ھ�ȷ�ؼ��theExactInterval������ */
                UInt32 theCurInterval = (UInt32)::floor(theExactInterval);
                if (theCurInterval >= theRollInterval)
					/* ����������־�ļ�(������ں�.log),�ٴ����ø����ݳ�Ա��ֵ */
                    this->RollLog();
            }
        }
    }
    return 60 * 1000;
}

/* ������ʱ�����ó���ҹʱ��midnight����ת������1970-1-1 00:00:00.���������� */
void QTSSRollingLog::ResetToMidnight(time_t* inTimePtr, time_t* outTimePtr) 
{
    if(*inTimePtr == -1)
    {
        *outTimePtr = -1;
        return;
    }
    
	/* ��ȡlocal time */
    struct tm  timeResult;
    struct tm* theLocalTime = qtss_localtime(inTimePtr, &timeResult);
    Assert(theLocalTime != NULL);

	/* reset to midnight */
	/* ����Ϊ��ҹʱ��,ʹʱ���Ӻ��� */
    theLocalTime->tm_hour = 0;
    theLocalTime->tm_min = 0;
    theLocalTime->tm_sec = 0;
    
    // some weird stuff
    //theLocalTime->tm_yday--;
    //theLocalTime->tm_mon--;
    //theLocalTime->tm_year -= 1900;

	/* �����ó�midnight��localʱ���ٴ�ת������1970-1-1 00:00:00.���������� */
    *outTimePtr = ::mktime(theLocalTime);

}
