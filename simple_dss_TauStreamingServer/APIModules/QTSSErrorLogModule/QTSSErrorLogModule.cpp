
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSErrorLogModule.cpp
Description: A module that uses QTSSRollingLog to write error messages to a file.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/


#include <string.h>
#include "QTSSErrorLogModule.h"
#include "QTSSMessages.h"
#include "QTSSRollingLog.h"
#include "QTSServerInterface.h"
#include "QTSSExpirationDate.h"
#include "OSMemory.h"
#include "OS.h"
#include "Task.h"

// STATIC FUNCTIONS

// The dispatch function for this module
static QTSS_Error   QTSSErrorLogModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock);

// A service routine allowing other modules to roll the log
static QTSS_Error   RollErrorLog(QTSS_ServiceFunctionArgsPtr inArgs);

static QTSS_Error   Register(QTSS_Register_Params* inParams);
static QTSS_Error   Shutdown();
        
static QTSS_Error   LogError(QTSS_RoleParamPtr inParamBlock);
static void         CheckErrorLogState();

static QTSS_Error   StateChange(QTSS_StateChange_Params* stateChangeParams);
static void         WriteStartupMessage();
static void         WriteShutdownMessage();

typedef char* LevelMsg;

static LevelMsg sErrorLevel[] = 
{
	"FATAL:",
	"WARNING:",
	"INFO:",
	"ASSERT:",
	"DEBUG:"
};

// QTSSERRORLOG CLASS DEFINITION

class QTSSErrorLog : public QTSSRollingLog
{
    public:
    
        QTSSErrorLog() : QTSSRollingLog() {this->SetTaskName("QTSSErrorLog");}
        virtual ~QTSSErrorLog() {}
    
        virtual char*  GetLogName()  { return QTSServerInterface::GetServer()->GetPrefs()->GetErrorLogName();}
        
        virtual char*  GetLogDir()   { return QTSServerInterface::GetServer()->GetPrefs()->GetErrorLogDir();}
        
        virtual UInt32 GetRollIntervalInDays()  { return QTSServerInterface::GetServer()->GetPrefs()->GetErrorRollIntervalInDays();}
        
        virtual UInt32 GetMaxLogBytes() { return QTSServerInterface::GetServer()->GetPrefs()->GetMaxErrorLogBytes();}
    
};

//ERRORLOGCHECKTASK CLASS DEFINITION

class ErrorLogCheckTask : public Task
{
    public:
        ErrorLogCheckTask() : Task() {this->SetTaskName("ErrorLogCheckTask"); this->Signal(Task::kStartEvent); }
        virtual ~ErrorLogCheckTask() {}
        
    private:
        virtual SInt64 Run();
};

// STATIC DATA

static OSMutex*           sLogMutex = NULL;//Log module isn't reentrant
static QTSSErrorLog*      sErrorLog = NULL;
static char               sLastErrorString[1024] = "";
static int                sDupErrorStringCount = 0;
static Bool16             sStartedUp = false;
static ErrorLogCheckTask* sErrorLogCheckTask = NULL;



// FUNCTION IMPLEMENTATIONS

QTSS_Error QTSSErrorLogModule_Main(void* inPrivateArgs)
{
    return _stublibrary_main(inPrivateArgs, QTSSErrorLogModuleDispatch);
}


QTSS_Error QTSSErrorLogModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock)
{
    switch (inRole)
    {
        case QTSS_Register_Role:
            return Register(&inParamBlock->regParams);
        case QTSS_StateChange_Role:
            return StateChange(&inParamBlock->stateChangeParams);
        case QTSS_ErrorLog_Role:
            return LogError(inParamBlock);
        case QTSS_Shutdown_Role:
            return Shutdown();
    }
    return QTSS_NoErr;
}


// ROLE METHODS

QTSS_Error Register(QTSS_Register_Params* inParams)
{
    sLogMutex = NEW OSMutex();
    
    // Do role & service setup
    
    (void)QTSS_AddRole(QTSS_ErrorLog_Role);
    (void)QTSS_AddRole(QTSS_Shutdown_Role);
    (void)QTSS_AddRole(QTSS_StateChange_Role);
    
    (void)QTSS_AddService("RollErrorLog", &RollErrorLog);
    
    // Unlike most modules, all initialization for this module happens in
    // the register role. This is so that this error log can begin logging
    // errors ASAP(as soon as possible).
    
    CheckErrorLogState();
    WriteStartupMessage();
    
    // Tell the server our name!
    static char* sModuleName = "QTSSErrorLogModule";
    ::strcpy(inParams->outModuleName, sModuleName);
    
	//�½�������־�������
    sErrorLogCheckTask = NEW ErrorLogCheckTask();

    return QTSS_NoErr;
}

QTSS_Error Shutdown()
{
    WriteShutdownMessage();
    if (sErrorLogCheckTask != NULL)
    {
        // sErrorLogCheckTask is a task object, so don't delete it directly
        // instead we signal it to kill itself.
        sErrorLogCheckTask->Signal(Task::kKillEvent); 
        sErrorLogCheckTask = NULL;
    }
    return QTSS_NoErr;
}

QTSS_Error StateChange(QTSS_StateChange_Params* stateChangeParams)
{
    if (stateChangeParams->inNewState == qtssIdleState)
    {
        WriteShutdownMessage();
    }
    else if (stateChangeParams->inNewState == qtssRunningState)
    {
        // Always force our preferences to be reread when we change
        // the server's state back to the start -- [sfu]    
        QTSS_ServiceID id;
        (void) QTSS_IDForService(QTSS_REREAD_PREFS_SERVICE, &id);           
        (void) QTSS_DoService(id, NULL);
        WriteStartupMessage();
    }
    
    return QTSS_NoErr;
}


QTSS_Error LogError(QTSS_RoleParamPtr inParamBlock)
{
    Assert(NULL != inParamBlock->errorParams.inBuffer);
    if (inParamBlock->errorParams.inBuffer == NULL)
        return QTSS_NoErr;
        
    UInt16 verbLvl = (UInt16) inParamBlock->errorParams.inVerbosity;
    if (verbLvl >= qtssIllegalVerbosity)
    	verbLvl = qtssFatalVerbosity;
        
    QTSServerPrefs* thePrefs = QTSServerInterface::GetServer()->GetPrefs();
        
    OSMutexLocker locker(sLogMutex);
    if (thePrefs->GetErrorLogVerbosity() >= inParamBlock->errorParams.inVerbosity)
    {
        
        //is this error message the same as the last one we received?
        if ( ::strcmp(inParamBlock->errorParams.inBuffer, sLastErrorString) == 0 )
        {   //yes?  increment count and bail if it's not the first time we've seen this message (otherwise fall thourhg and write it to the log)
            sDupErrorStringCount++;
            return QTSS_NoErr;
        }
        else 
        {
            //we have a new error message, write a "previous line" message before writing the new log entry
            if ( sDupErrorStringCount > 1 )
            {
            /***  clean this up - lots of duplicate code ***/
            
                //The error logger is the bottleneck for any and all messages printed by the server.
                //For debugging purposes, these messages can be printed to stdout as well.
                if (thePrefs->IsScreenLoggingEnabled())
                    qtss_printf("--last message repeated %d times\n", sDupErrorStringCount);
            
                CheckErrorLogState();
                
                if (sErrorLog == NULL)
                    return QTSS_NoErr;
                    
                //timestamp the error
                char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
                Bool16 result = QTSSRollingLog::FormatDate(theDateBuffer, false);
                //for now, just ignore the error.
                if (!result)
                    theDateBuffer[0] = '\0';
        
                Assert(strlen(theDateBuffer) + ::strlen(inParamBlock->errorParams.inBuffer) < 1024);
                char tempBuffer[1024];
                qtss_sprintf(tempBuffer, "%s: --last message repeated %d times\n", theDateBuffer, sDupErrorStringCount);
                
                sErrorLog->WriteToLog(tempBuffer, kAllowLogToRoll);
    
                sDupErrorStringCount = 0;
            }
            
            ::strcpy(sLastErrorString, inParamBlock->errorParams.inBuffer);
        
        }

        //The error logger is the bottleneck for any and all messages printed by the server.
        //For debugging purposes, these messages can be printed to stdout as well.
        if (thePrefs->IsScreenLoggingEnabled())
            qtss_printf("%s %s\n", sErrorLevel[verbLvl], inParamBlock->errorParams.inBuffer);
        
        CheckErrorLogState();
        
        if (sErrorLog == NULL)
            return QTSS_NoErr;
            
        //timestamp the error
        char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
        Bool16 result = QTSSRollingLog::FormatDate(theDateBuffer, false);
        //for now, just ignore the error.
        if (!result)
            theDateBuffer[0] = '\0';

        Assert(strlen(theDateBuffer) + ::strlen(inParamBlock->errorParams.inBuffer) < 1024);
        char tempBuffer[1024];
        qtss_sprintf(tempBuffer, "%s: %s %s\n", theDateBuffer, sErrorLevel[verbLvl], inParamBlock->errorParams.inBuffer);
        
        sErrorLog->WriteToLog(tempBuffer, kAllowLogToRoll);
    }
    return QTSS_NoErr;
}


void CheckErrorLogState()
{
    //this function makes sure the logging state is in synch with the preferences.
    //extern variable declared in QTSSPreferences.h

    QTSServerPrefs* thePrefs = QTSServerInterface::GetServer()->GetPrefs();

    //check error log.
    if ((NULL == sErrorLog) && (thePrefs->IsErrorLogEnabled()))
    {
        sErrorLog = NEW QTSSErrorLog();
        sErrorLog->EnableLog();
    }

    if ((NULL != sErrorLog) && (!thePrefs->IsErrorLogEnabled()))
    {
        sErrorLog->Delete(); //sErrorLog is a task object, so don't delete it directly
        sErrorLog = NULL;
    }
}

// SERVICE ROUTINES

QTSS_Error RollErrorLog(QTSS_ServiceFunctionArgsPtr /*inArgs*/)
{
    OSMutexLocker locker(sLogMutex);
    if (sErrorLog != NULL)
        sErrorLog->RollLog();
    return QTSS_NoErr;
}

void    WriteStartupMessage()
{
    if (sStartedUp)
        return;
        
    sStartedUp = true;
    
    //format a date for the startup time
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
    Bool16 result = QTSSRollingLog::FormatDate(theDateBuffer, false);
    
    char tempBuffer[1024];
    if (result)
        qtss_sprintf(tempBuffer, "# Streaming STARTUP %s\n", theDateBuffer);
        
    // log startup message to error log as well.
    if ((result) && (sErrorLog != NULL))
        sErrorLog->WriteToLog(tempBuffer, kAllowLogToRoll);
    
    //write the expire date to the log
    if ( QTSSExpirationDate::WillSoftwareExpire() && sErrorLog != NULL )
    {
        QTSSExpirationDate::sPrintExpirationDate(tempBuffer);
        sErrorLog->WriteToLog(tempBuffer, kAllowLogToRoll);
    }
}

void    WriteShutdownMessage()
{
    if (!sStartedUp)
        return;
        
    sStartedUp = false;
    
    //log shutdown message
    //format a date for the shutdown time
    char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
    Bool16 result = QTSSRollingLog::FormatDate(theDateBuffer, false);
    
    char tempBuffer[1024];
    if (result)
        qtss_sprintf(tempBuffer, "# Streaming SHUTDOWN %s\n", theDateBuffer);

    if ( result && sErrorLog != NULL )
        sErrorLog->WriteToLog(tempBuffer, kAllowLogToRoll);
}

// This task runs once an hour to check and see if the log needs to roll.
SInt64 ErrorLogCheckTask::Run()
{
    static Bool16 firstTime = true;
    
    // don't check the log for rolling the first time we run.
    if (firstTime)
    {
        firstTime = false;
    }
    else
    {
        Bool16 success = false;

        if (sErrorLog != NULL && sErrorLog->IsLogEnabled())
            success = sErrorLog->CheckRollLog();
        Assert(success);
    }
    // execute this task again in one hour.
    return (60*60*1000);
}


