
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
//�ο�QTSS_FileObjectAttributes in QTSS.h
QTSSAttrInfoDict::AttrInfo  QTSSFile::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0 */ { "qtssFlObjStream",                NULL,   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssFlObjFileSysModuleName",     NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },//��������ļ�������ļ�ϵͳģ������
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
/* �Ը�����־�򿪸���·����ý���ļ���������ִ�иô򿪽�ɫ��ģ�� */
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

/* ����ָ��ģ��ر��ļ����� */
void    QTSSFile::Close()
{
    Assert(fModule != NULL);
    
    QTSS_RoleParams theParams;
    theParams.closeFileParams.inFileObject = this;
    (void)fModule->CallDispatch(QTSS_CloseFile_Role, &theParams);
}


// 
// IMPLEMENTATION OF STREAM FUNCTIONS.
/* ����ע��QTSS_ReadFile_Role��ģ��ȥ��ȡ��ǰ�ļ������е����ݵ�ָ������,������ʵ�ʶ�ȡ���ݵĳ��� */
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
 
/* ��ѯ��ǰ�ļ�������ļ�����,��ȷ��ָ����λ��û���ļ�����,�������ΪfPosition */
QTSS_Error  QTSSFile::Seek(UInt64 inNewPosition)
{
    UInt64* theFileLength = NULL;
    UInt32 theParamLength = 0;
    
	//��ѯ��ǰ�ļ�������ļ�����
    (void)this->GetValuePtr(qtssFlObjLength, 0, (void**)&theFileLength, &theParamLength);
    
    if (theParamLength != sizeof(UInt64))
        return QTSS_RequestFailed;
    
	//�����κϷ���
    if (inNewPosition > *theFileLength)
        return QTSS_RequestFailed;
    
	//�����ļ�����ָ��ĵ�ǰλ��
    fPosition = inNewPosition;
    return QTSS_NoErr;
}
 
/* ����ע��QTSS_AdviseFile_Role��ģ��,֪ͨ�ļ�ϵͳģ������ָ�����ֺܿ콫�ᱻ��ȡ */
QTSS_Error  QTSSFile::Advise(UInt64 inPosition, UInt32 inAdviseSize)
{
    Assert(fModule != NULL);

    //
    // Invoke the owning QTSS API module. Setup a param block to do so.
    QTSS_RoleParams theParams;
    theParams.adviseFileParams.inFileObject = this;
    theParams.adviseFileParams.inPosition = inPosition;//��������ȡ�Ĳ��ֵ���ʼ��(�����������ʼλ�õ��ֽ�ƫ����)
    theParams.adviseFileParams.inSize = inAdviseSize;//��������ȡ�Ĳ��ֵ��ֽڳ���

    return fModule->CallDispatch(QTSS_AdviseFile_Role, &theParams);
}

/* ����ע��QTSS_RequestEventFile_Role��ģ��,����ָ���¼�����ʱ�õ�֪ͨ */
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
