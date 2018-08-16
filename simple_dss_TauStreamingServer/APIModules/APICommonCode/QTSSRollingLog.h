
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

const Bool16 kAllowLogToRoll = true;//������־������?

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
		/* ��׷�ӷ�ʽ����־�ļ�,����Ƿ������־? �����ָ���������ַ�����ʽ׷��д����־�ļ���,�ر���־�ļ� */
        void    WriteToLog(char* inLogData, Bool16 allowLogToRoll);
        
        //log rolls automatically based on the configuration criteria,
        //but you may roll the log manually by calling this function.
        //Returns true if no error, false otherwise
		/* ����������־�ļ�(������ں�.log),���´���־�ļ�,�ٴ����ø����ݳ�Ա��ֵ */
        Bool16  RollLog();

        //
        // Call this to open the log file and begin logging 
		/* ���ƹ��캯��,���ø����ݳ�Ա(fLogging����),��׷�ӷ�ʽ��ָ��Ŀ¼��Log�ļ�,����Log�ļ����������� */
        void EnableLog( Bool16 appendDotLog = true);
        
        //
        // Call this to close the log
        // (pass leaveEnabled as true when we are temporarily closing.)
		/* �ر�Log�ļ� */
        void CloseLog( Bool16 leaveEnabled = false);

        //
        //mainly to check and see if errors occurred
		/* ���ڿ��Լ�¼��־��? ����־�ļ��رջ���־�ļ�����ʱ,�Ϳ��Լ�¼��־. */
        Bool16  IsLogEnabled();
        
        //master switch
		/* ��ȡ��������־�ɷ��¼״̬ */
        Bool16  IsLogging() { return fLogging; }
        void    SetLoggingEnabled( Bool16 logState ) { fLogging = logState; }
        
        //General purpose utility function
        //returns false if some error has occurred
		/* ��ȡ��ǰʱ��,����ָ����ʽYYYY-MM-DD HH:MM:SS�����ָ����С�Ļ�����,����true */
        static Bool16   FormatDate(char *ioDateBuffer, Bool16 logTimeInGMT);
        
        // Check the log to see if it needs to roll
        // (rolls the log if necessary)
		/* ����Ƿ�Roll Log?����,����ǰʱ����������־�ļ���ʱ����������־�������,�͹�����־,Ȼ��,����־�ļ���λ��ָ�볬�������־�ֽ���,�͹�����־ */
        Bool16          CheckRollLog();
        
        // Set this to true to get the log to close the file between writes.
		/* ��������־�ļ���д�����ݺ��Ƿ�ر���־�ļ�? */
        static void		SetCloseOnWrite(Bool16 closeOnWrite);

        enum
        {
            kMaxDateBufferSizeInBytes = 30, //UInt32
            kMaxFilenameLengthInBytes = 31  //UInt32
        };
    
    protected:

        //
        // Task object. Do not delete directly
		/* �ر���־,ɾ����־Ŀ¼ */
        virtual ~QTSSRollingLog();

		/* ע�����ִ�����:���������Щ���������嶨��! */
        //Derived class must provide a way to get the log & rolled log name
        virtual char*  GetLogName() = 0;
        virtual char*  GetLogDir() = 0;
        virtual UInt32 GetRollIntervalInDays() = 0;//0 means no interval
        virtual UInt32 GetMaxLogBytes() = 0;//0 means unlimited
                    
        //to record the time the file was created (for time based rolling����ʱ�����־����,���ǻ����ļ���С����־����)
		/* ��������־�ļ���localʱ��׷�ӽ���־�ļ���,�ٴ���־�ļ��ж���������UTCʱ�� */
        virtual time_t  WriteLogHeader(FILE *inFile);
		/* ��Log�ļ���ͷ��ȡtheFileCreateTime����ǡ�������ת���ɴӹ�Ԫ1970��1��1��0ʱ0��0�����������UTCʱ��������������,Ҳ���ظ��� */
        time_t          ReadLogHeader(FILE* inFile);

    private:
    
        //
        // Run function to roll log right at midnight 
		/* ����ǰʱ����������־�ļ���ʱ����������־�������,д����־�ļ�(����������־�ļ�(������ں�.log),���´���־�ļ�,�ٴ����ø����ݳ�Ա��ֵ),ÿ���ӹ���һ��Log */
        virtual SInt64  Run();

		/* ��־�ļ���� */
        FILE*           fLog;
		/* Log�ļ��Ĵ���ʱ�� */
        time_t          fLogCreateTime;
		/* Log�ļ�����ȫ·�� */
        char*           fLogFullPath;

		/* ���ļ�ĩβ���Ӻ�׺.log��? */
        Bool16          fAppendDotLog;
		/* ��¼Log��? */
        Bool16          fLogging;

		/* ȡ�ò�����LogĿ¼,�����ʵ���С�Ļ�����һ��û��������������(�������ں�.log)����ļ���,���������.�ɹ�����true,ʧ�ܷ���false */
        Bool16          RenameLogFile(const char* inFileName);
		/* ��ȡ���ָ��·�����ļ���Ϣ,���ݷ��ؽ���жϸ��ļ��Ƿ���� */
        Bool16          DoesFileExist(const char *inPath);
		/* ������ʱ�����ó�midnight����ת������1970-1-1 00:00:00.���������� */
        static void     ResetToMidnight(time_t* inTimePtr, time_t* outTimePtr);
		/* ��ȡ��־�ļ���·�����ļ���,������ξ����Ƿ񸽼�.log��׺,��󷵻ظ��ַ��� */
        char*           GetLogPath(char *extension);
        
        // To make sure what happens in Run doesn't also happen at the same time in the public functions.
        OSMutex         fMutex;
};

#endif // __QTSS_ROLLINGLOG_H__

