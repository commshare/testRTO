/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSPosixFileSysModule.cpp
Description: Provide the async file I/O mechanism.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-23

****************************************************************************/ 


#include "QTSSPosixFileSysModule.h"
#include "QTSSModuleUtils.h"
#include "OSMemory.h"
#include "OSFileSource.h"
#include "Socket.h"


// ATTRIBUTES
static QTSS_AttributeID         sOSFileSourceAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sEventContextAttr = qtssIllegalAttrID;

// FUNCTION PROTOTYPES

static QTSS_Error   QTSSPosixFileSysModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error   Register(QTSS_Register_Params* inParams);
static QTSS_Error   Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error   OpenFile(QTSS_OpenFile_Params* inParams);
static QTSS_Error   AdviseFile(QTSS_AdviseFile_Params* inParams);
static QTSS_Error   ReadFile(QTSS_ReadFile_Params* inParams);
static QTSS_Error   CloseFile(QTSS_CloseFile_Params* inParams);
static QTSS_Error   RequestEventFile(QTSS_RequestEventFile_Params* inParams);

// FUNCTION IMPLEMENTATIONS

QTSS_Error QTSSPosixFileSysModule_Main(void* inPrivateArgs)
{
    return _stublibrary_main(inPrivateArgs, QTSSPosixFileSysModuleDispatch);
}


QTSS_Error  QTSSPosixFileSysModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
    switch (inRole)
    {
        case QTSS_Register_Role:
            return Register(&inParams->regParams);
        case QTSS_Initialize_Role:
            return Initialize(&inParams->initParams);
        case QTSS_OpenFile_Role:
            return OpenFile(&inParams->openFileParams);
        case QTSS_AdviseFile_Role:
            return AdviseFile(&inParams->adviseFileParams);
        case QTSS_ReadFile_Role:
            return ReadFile(&inParams->readFileParams);
        case QTSS_CloseFile_Role:
            return CloseFile(&inParams->closeFileParams);
        case QTSS_RequestEventFile_Role:
            return RequestEventFile(&inParams->reqEventFileParams);
    }
    return QTSS_NoErr;
}


QTSS_Error Register(QTSS_Register_Params* inParams)
{
    // Do role & attribute setup
    (void)QTSS_AddRole(QTSS_Initialize_Role);
    
    // Only open needs to be registered for, the rest happen naturally
    (void)QTSS_AddRole(QTSS_OpenFile_Role);

    // Add an attribute to the file object type to store a pointer to our OSFileSource  
    static char*        sOSFileSourceName   = "QTSSPosixFileSysModuleOSFileSource";
    (void)QTSS_AddStaticAttribute(qtssFileObjectType, sOSFileSourceName, NULL, qtssAttrDataTypeVoidPointer);
    (void)QTSS_IDForAttr(qtssFileObjectType, sOSFileSourceName, &sOSFileSourceAttr);

    static char*        sEventContextName   = "QTSSPosixFileSysModuleEventContext";
    (void)QTSS_AddStaticAttribute(qtssFileObjectType, sEventContextName, NULL, qtssAttrDataTypeVoidPointer);
    (void)QTSS_IDForAttr(qtssFileObjectType, sEventContextName, &sEventContextAttr);
    
    // Tell the server our name!
    static char* sModuleName = "QTSSPosixFileSysModule";
    ::strcpy(inParams->outModuleName, sModuleName);

    return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
    // Setup module utils
    QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
    return QTSS_NoErr;
}

QTSS_Error  OpenFile(QTSS_OpenFile_Params* inParams)
{
    OSFileSource* theFileSource = NEW OSFileSource(inParams->inPath);

    UInt64 theLength = theFileSource->GetLength();
    
    //
    // OSFileSource returns mod date as a time_t.
    // This is the same as a QTSS_TimeVal, except the latter is in msec
    QTSS_TimeVal theModDate = (QTSS_TimeVal)theFileSource->GetModDate();
    theModDate *= 1000;
    
    //
    // Check to see if the file actually exists
    if (theLength == 0)
    {
        delete theFileSource;
        return QTSS_FileNotFound;
    }
    
    //
    // Add this new file source object to the file object
    QTSS_Error theErr = QTSS_SetValue(inParams->inFileObject, sOSFileSourceAttr, 0, &theFileSource, sizeof(theFileSource));
    if (theErr != QTSS_NoErr)
    {
        delete theFileSource;
        return QTSS_RequestFailed;
    }

    //
    // If caller wants async I/O, at this point we should set up the EventContext
    if (inParams->inFlags & qtssOpenFileAsync)
    {
        EventContext* theEventContext = NEW EventContext(EventContext::kInvalidFileDesc, Socket::GetEventThread());
        theEventContext->InitNonBlocking(theFileSource->GetFD());
        
        theErr = QTSS_SetValue(inParams->inFileObject, sEventContextAttr, 0, &theEventContext, sizeof(theEventContext));
        if (theErr != QTSS_NoErr)
        {
            delete theFileSource;
            delete theEventContext;
            return QTSS_RequestFailed;
        }
    }
    
    //
    // Set up the other attribute values in the file object
    (void)QTSS_SetValue(inParams->inFileObject, qtssFlObjLength, 0, &theLength, sizeof(theLength));
    (void)QTSS_SetValue(inParams->inFileObject, qtssFlObjModDate, 0, &theModDate, sizeof(theModDate));

    return QTSS_NoErr;
}

QTSS_Error  AdviseFile(QTSS_AdviseFile_Params* inParams)
{
    OSFileSource** theFile = NULL;
    UInt32 theLen = 0;
    
    (void)QTSS_GetValuePtr(inParams->inFileObject, sOSFileSourceAttr, 0, (void**)&theFile, &theLen);
    Assert(theLen == sizeof(OSFileSource*));
    (*theFile)->Advise(inParams->inPosition, inParams->inSize);
        
    return QTSS_NoErr;
}

QTSS_Error  ReadFile(QTSS_ReadFile_Params* inParams)
{
    OSFileSource** theFile = NULL;
    UInt32 theLen = 0;
    
    (void)QTSS_GetValuePtr(inParams->inFileObject, sOSFileSourceAttr, 0, (void**)&theFile, &theLen);
    Assert(theLen == sizeof(OSFileSource*));
    OS_Error osErr = (*theFile)->Read(inParams->inFilePosition, inParams->ioBuffer, inParams->inBufLen, inParams->outLenRead);

    if (osErr == EAGAIN)
        return QTSS_WouldBlock;
    else if (osErr != OS_NoErr)
        return QTSS_RequestFailed;
    else
        return QTSS_NoErr;
}

QTSS_Error  CloseFile(QTSS_CloseFile_Params* inParams)
{
    OSFileSource** theFile = NULL;
    EventContext** theContext = NULL;
    UInt32 theLen = 0;
    
    QTSS_Error theErr = QTSS_GetValuePtr(inParams->inFileObject, sOSFileSourceAttr, 0, (void**)&theFile, &theLen);
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_GetValuePtr(inParams->inFileObject, sEventContextAttr, 0, (void**)&theContext, &theLen);
    
    if (theErr == QTSS_NoErr)
    {
        //
        // If this file was opened up in async mode, there is an EventContext associated with it.
        // We should first make sure that the OSFileSource destructor doesn't close the FD
        // (the EventContext will do it), then close the EventContext
        (*theFile)->ResetFD();
        delete *theContext;
    }
    delete *theFile;
    return QTSS_NoErr;
}

QTSS_Error  RequestEventFile(QTSS_RequestEventFile_Params* inParams)
{
    QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
    if (OSThread::GetCurrent() != NULL)
        theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

    Assert(theState->curTask != NULL);
    
    EventContext** theContext = NULL;
    UInt32 theLen = 0;
    
    QTSS_Error theErr = QTSS_GetValuePtr(inParams->inFileObject, sEventContextAttr, 0, (void**)&theContext, &theLen);
    if (theErr == QTSS_NoErr)
    {
        //
        // This call only works if the file is async!
        (*theContext)->SetTask(theState->curTask);
        (*theContext)->RequestEvent();
        
        return QTSS_NoErr;
    }
    return QTSS_RequestFailed;
}
