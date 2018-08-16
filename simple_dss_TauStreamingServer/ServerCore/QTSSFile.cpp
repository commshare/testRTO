
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSFile.cpp
Description: Class QTSSFile definition .
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "QTSSFile.h"
#include "QTSServerInterface.h"

//sAttributes[] definition, 
//refer to QTSSDictionary.cpp line 829, and QTSSFile.cpp line 78
//参考QTSS_FileObjectAttributes in QTSS.h
QTSSAttrInfoDict::AttrInfo  QTSSFile::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "qtssFlObjStream",                NULL,   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssFlObjFileSysModuleName",     NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },//处理这个文件对象的文件系统模块名称
    /* 2 */ { "qtssFlObjLength",                NULL,   qtssAttrDataTypeUInt64,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 3 */ { "qtssFlObjPosition",              NULL,   qtssAttrDataTypeUInt64,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4 */ { "qtssFlObjModDate",               NULL,   qtssAttrDataTypeUInt64,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite }
};

void    QTSSFile::Initialize()
{
    for (UInt32 x = 0; x < qtssFlObjNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kFileDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

// CONSTRUCTOR
QTSSFile::QTSSFile()
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kFileDictIndex)),
    fModule(NULL),
    fPosition(0),
    fLength(0),
    fModDate(0)
{
    fThisPtr = this;
    //
    // The stream is just a pointer to this thing
    this->SetVal(qtssFlObjStream, &fThisPtr, sizeof(fThisPtr));
    this->SetVal(qtssFlObjLength, &fLength, sizeof(fLength));
    this->SetVal(qtssFlObjPosition, &fPosition, sizeof(fPosition));
    this->SetVal(qtssFlObjModDate, &fModDate, sizeof(fModDate));
}

//
// OPEN and CLOSE
//
/* 以给定标志打开给定路径的媒体文件，并设置执行该打开角色的模块 */
QTSS_Error  QTSSFile::Open(char* inPath, QTSS_OpenFileFlags inFlags)
{
    //
    // Because this is a role being executed from inside a callback, we need to
    // make sure that QTSS_RequestEvent will not work.
    Task* curTask = NULL;
    QTSS_ModuleState* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
    if (OSThread::GetCurrent() != NULL)
        theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();
        
    if (theState != NULL)
        curTask = theState->curTask;
    
    QTSS_RoleParams theParams;
    theParams.openFileParams.inPath = inPath;
    theParams.openFileParams.inFlags = inFlags;
    theParams.openFileParams.inFileObject = this;

    QTSS_Error theErr = QTSS_FileNotFound;
    UInt32 x = 0;
    
    for ( ; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kOpenFilePreProcessRole); x++)
    {
        theErr = QTSServerInterface::GetModule(QTSSModule::kOpenFilePreProcessRole, x)->CallDispatch(QTSS_OpenFilePreProcess_Role, &theParams);
        if (theErr != QTSS_FileNotFound)
        {
            fModule = QTSServerInterface::GetModule(QTSSModule::kOpenFilePreProcessRole, x);
            break;
        }
    }
    
    if (theErr == QTSS_FileNotFound)
    {
        // None of the prepreprocessors claimed this file. Invoke the default file handler
        if (QTSServerInterface::GetNumModulesInRole(QTSSModule::kOpenFileRole) > 0)
        {
            fModule = QTSServerInterface::GetModule(QTSSModule::kOpenFileRole, 0);
            theErr = QTSServerInterface::GetModule(QTSSModule::kOpenFileRole, 0)->CallDispatch(QTSS_OpenFile_Role, &theParams);

        }
    }
    
    //
    // Reset the curTask to what it was before this role started
    if (theState != NULL)
        theState->curTask = curTask;

    return theErr;
}

/* 调用指定模块关闭文件对象 */
void    QTSSFile::Close()
{
    Assert(fModule != NULL);
    
    QTSS_RoleParams theParams;
    theParams.closeFileParams.inFileObject = this;
    (void)fModule->CallDispatch(QTSS_CloseFile_Role, &theParams);
}


// 
// IMPLEMENTATION OF STREAM FUNCTIONS.
/* 调用注册QTSS_ReadFile_Role的模块去读取当前文件对象中的数据到指定缓存,并返回实际读取数据的长度 */
QTSS_Error  QTSSFile::Read(void* ioBuffer, UInt32 inBufLen, UInt32* outLengthRead)
{
    Assert(fModule != NULL);
    UInt32 theLenRead = 0;

    //
    // Invoke the owning QTSS API module. Setup a param block to do so.
    QTSS_RoleParams theParams;
    theParams.readFileParams.inFileObject = this;
    theParams.readFileParams.inFilePosition = fPosition;
    theParams.readFileParams.ioBuffer = ioBuffer;
    theParams.readFileParams.inBufLen = inBufLen;
    theParams.readFileParams.outLenRead = &theLenRead;
    
    QTSS_Error theErr = fModule->CallDispatch(QTSS_ReadFile_Role, &theParams);
    
    fPosition += theLenRead;
    if (outLengthRead != NULL)
        *outLengthRead = theLenRead;
        
    return theErr;
}
 
/* 查询当前文件对象的文件长度,并确保指定的位置没有文件长度,设置入参为fPosition */
QTSS_Error  QTSSFile::Seek(UInt64 inNewPosition)
{
    UInt64* theFileLength = NULL;
    UInt32 theParamLength = 0;
    
	//查询当前文件对象的文件长度
    (void)this->GetValuePtr(qtssFlObjLength, 0, (void**)&theFileLength, &theParamLength);
    
    if (theParamLength != sizeof(UInt64))
        return QTSS_RequestFailed;
    
	//检查入参合法性
    if (inNewPosition > *theFileLength)
        return QTSS_RequestFailed;
    
	//设置文件对象指针的当前位置
    fPosition = inNewPosition;
    return QTSS_NoErr;
}
 
/* 调用注册QTSS_AdviseFile_Role的模块,通知文件系统模块流的指定部分很快将会被读取 */
QTSS_Error  QTSSFile::Advise(UInt64 inPosition, UInt32 inAdviseSize)
{
    Assert(fModule != NULL);

    //
    // Invoke the owning QTSS API module. Setup a param block to do so.
    QTSS_RoleParams theParams;
    theParams.adviseFileParams.inFileObject = this;
    theParams.adviseFileParams.inPosition = inPosition;//即将被读取的部分的起始点(相对于流的起始位置的字节偏移量)
    theParams.adviseFileParams.inSize = inAdviseSize;//即将被读取的部分的字节长度

    return fModule->CallDispatch(QTSS_AdviseFile_Role, &theParams);
}

/* 调用注册QTSS_RequestEventFile_Role的模块,请求当指定事件发生时得到通知 */
QTSS_Error  QTSSFile::RequestEvent(QTSS_EventType inEventMask)
{
    Assert(fModule != NULL);

    //
    // Invoke the owning QTSS API module. Setup a param block to do so.
    QTSS_RoleParams theParams;
    theParams.reqEventFileParams.inFileObject = this;
    theParams.reqEventFileParams.inEventMask = inEventMask;

    return fModule->CallDispatch(QTSS_RequestEventFile_Role, &theParams);
}
