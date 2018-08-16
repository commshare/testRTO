
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSModule.cpp
Description: This object represents a single QTSS API compliant module which 
             does the loading and initialization of a module, and stores all 
			 per-module data.A module may either be compiled directly into the 
			 server, or loaded from a code fragment residing on the disk.refer 
			 to QTSS API MOUDLES Documentation.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <errno.h>

#include "QTSSModule.h"
#include "QTSServerInterface.h"
#include "OSArrayObjectDeleter.h"
#include "OSMemory.h"
#include "StringParser.h"
#include "Socket.h"

/* ��̬����������,�μ�QTSSModule::AddRole() */
Bool16  QTSSModule::sHasRTSPRequestModule = false;
Bool16  QTSSModule::sHasOpenFileModule = false;
Bool16  QTSSModule::sHasRTSPAuthenticateModule = false;

/* ������Ϣ����,�μ�QTSSDictionary.cpp��QTSSModule::Initialize() */
/* ����QTSServerInterface::sAttributes[] */
QTSSAttrInfoDict::AttrInfo  QTSSModule::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission, see QTSSAttrInfoDict::AttrInfo in QTSSDictionary.cpp/h */
    /* 0 */ { "qtssModName",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssModDesc",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 2 */ { "qtssModVersion",         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 3 */ { "qtssModRoles",           NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4 */ { "qtssModPrefs",           NULL,                   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe  | qtssAttrModeInstanceAttrAllowed },
    /* 5 */ { "qtssModAttributes",      NULL,                   qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeInstanceAttrAllowed }
};

/* ����QTSSDictionaryMap::SetAttribute()�ģ���ֵ�ModuleDict����������(6��) */
void QTSSModule::Initialize()
{
    //Setup all the dictionary stuff
	/* ��ģ���������(6��)ѭ������QTSS_ModuleObjectAttributes�����ֵ */
    for (UInt32 x = 0; x < qtssModNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                            sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

/* ��������ø����ݳ�Ա��ֵ,�õ�QTSSDictionary::SetVal(),��ο�QTSSDictionaryMap::SetAttribute() */
QTSSModule::QTSSModule(char* inName, char* inPath)
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)),
    fQueueElem(NULL),
    fPath(NULL),
    fFragment(NULL),
    fDispatchFunc(NULL),
    fPrefs(NULL),
    fAttributes(NULL)
{
	/* ���ö���Ԫ�����ڶ�����Ǳ�ģ��ʵ�� */
    fQueueElem.SetEnclosingObject(this);
    this->SetTaskName("QTSSModule");

	/* ����������½���fFragment��fPath */
    if ((inPath != NULL) && (inPath[0] != '\0'))
    {
        // Create a code fragment if this module is being loaded from disk 
        fFragment = NEW OSCodeFragment(inPath);
        fPath = NEW char[::strlen(inPath) + 2];
        ::strcpy(fPath, inPath);
    }
    
	/* ����ģ��������ֵ�fAttributes��Ԥ��ֵfPrefs,д��������ڲ�,����÷�ֵ�òο�!!! */
    fAttributes = NEW QTSSDictionary( NULL, &fAttributesMutex );
    this->SetVal(qtssModPrefs,      &fPrefs,      sizeof(fPrefs));//��ʱ�ǿ�ָ��
    this->SetVal(qtssModAttributes, &fAttributes, sizeof(fAttributes));
    
    // If there is a name, copy it into the module object's internal buffer
    if (inName != NULL)
        this->SetValue(qtssModName, 0, inName, ::strlen(inName), QTSSDictionary::kDontObeyReadOnly);
                
    ::memset(fRoleArray, 0, sizeof(fRoleArray));
    ::memset(&fModuleState, 0, sizeof(fModuleState));

}

/* ע��ú���ʮ����Ҫ!��Ҫ��������fDispatchFunc */
/* ���岢��ʼ��QTSS_PrivateArgs,�ٴӴ��̼���dll/.so�õ��͵�������ں���QTSS_MainEntryPointPtr,�Ӷ��õ��ַ�����ָ�벢����ΪfDispatchFunc,
   ����ָ����ʽ��moduleName�ַ���"Module Loaded...*** static/dynamic"д�������־ */
QTSS_Error  QTSSModule::SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint)
{
    QTSS_Error theErr = QTSS_NoErr;
    
    // Load fragment from disk if necessary
    /* �����Ҫ,�Ӵ��̼���dll/.so,�����ָ�����ŵ�����ں���QTSS_MainEntryPointPtr(�ڶ������)��ָ�� */
    if ((fFragment != NULL) && (inEntrypoint == NULL))
		/* ����OSCodeFragment::GetSymbol()�������ں�����ָ�� */
        theErr = this->LoadFromDisk(&inEntrypoint);
	/* ����Ӵ��̼��ش���,ֱ�ӷ��� */
    if (theErr != QTSS_NoErr)
        return theErr;
        
    // At this point, we must have an entrypoint
	/* �ڴ˴�һ��Ҫȷ����ں���ָ��ǿ�,�����Ǵ�dll/.so���صõ�,�����Ѿ����ڵ� */
    if (inEntrypoint == NULL)
        return QTSS_NotAModule;
        
    // Invoke the private initialization routine
	/* ���岢��ʼ��QTSS_PrivateArgs,��������ǳ���Ҫ,�μ� QTSS_Private.h */
    QTSS_PrivateArgs thePrivateArgs;
    thePrivateArgs.inServerAPIVersion = QTSS_API_VERSION;
    thePrivateArgs.inCallbacks = inCallbacks; //���
    thePrivateArgs.outStubLibraryVersion = 0;//�����汾
    thePrivateArgs.outDispatchFunction = NULL;/* ����Ҫ���õ�,ÿ��Module�ķַ�����,����QTSSFileModuleDispatch() */
    
	/* �������ò�������ε�������ں���QTSS_MainEntryPointPtr */
	/************** NOTE !! *********************/
	/* ����ÿ��Module�ر�������������,��������thePrivateArgs.outDispatchFunction,�μ�QTSS_Private.cpp�е� _stublibrary_main() */
    theErr = (inEntrypoint)(&thePrivateArgs);
	/* ����������������̳���,��������  */
    if (theErr != QTSS_NoErr)
        return theErr;
   
	//��������汾>API�汾,��������
    if (thePrivateArgs.outStubLibraryVersion > thePrivateArgs.inServerAPIVersion)
        return QTSS_WrongVersion;
    
    // Set the dispatch function so we'll be able to invoke this module later on
    /* �þ���Module�ķַ�����(����QTSSFileModuleDispatch())����Moduleʵ���ķַ�����,ע���ⲽ����Ҫ,������Ҫ�õ��Ľ�� */
    fDispatchFunc = thePrivateArgs.outDispatchFunction;
    	
    // Log 
	/* �õ�ָ�����Ⱥ͸�ʽ��moduleName�ַ���"INFO: Module Loaded...QTSSRefMovieModule [dynamic]",��д�������־ */
    char msgStr[2048];
    char* moduleName = NULL;
    (void)this->GetValueAsString (qtssModName, 0, &moduleName);
    qtss_snprintf(msgStr, sizeof(msgStr), "Module Loaded...%s [%s]", moduleName, (fFragment==NULL)?"static":"dynamic");
    delete moduleName;
    QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);
	
     return QTSS_NoErr;
}

/* �Ӵ��̼���dll/.so,�����ָ������(Ҫ���û�ȡ�ļ�·��fPath,�����õ�ģ������)������ں���QTSS_MainEntryPointPtr��ָ�� */
QTSS_Error QTSSModule::LoadFromDisk(QTSS_MainEntryPointPtr* outEntrypoint)
{
	/* ע������ַ������������е�Modules������ */
    static StrPtrLen sMainEntrypointName("_Main");
    
	/* ȷ��ָ��ǿ�,ע������QTSSModule::SetupModule()�в���ָ�� */
    Assert(outEntrypoint != NULL);
    
    // Modules only need to be initialized if they reside on disk. 
	/* ��fFragmentΪ��ʱ,��static modules,ֱ�ӷ��� */
    if (fFragment == NULL)
        return QTSS_NoErr;
    
	/* ���Ҫ���صĴ���Ƭ���Ƿ����? */
    if (!fFragment->IsValid())
        return QTSS_NotAModule;
        
    // fPath is actually a path. Extract the file name.
    
	/**************** ����һ�ײ����ַ����ľ�������� ******************/
    StrPtrLen theFileName(fPath);
    StringParser thePathParser(&theFileName); 
	/* ��fPath�л��·���ַ���,��������theFileName */
    while (thePathParser.GetThru(&theFileName, kPathDelimiterChar))
        ;
    Assert(theFileName.Len > 0);
    Assert(theFileName.Ptr != NULL);

    // At this point, theFileName points to the file name. Make this the module name.
	/* ���ļ���������ģ������ */
    this->SetValue(qtssModName, 0, theFileName.Ptr, theFileName.Len, QTSSDictionary::kDontObeyReadOnly);
    
    // 
    // The main entrypoint symbol name is the file name plus that _Main__ string up there.
    OSCharArrayDeleter theSymbolName(NEW char[theFileName.Len + sMainEntrypointName.Len + 2]);//����"__"�����ַ�
    ::memcpy(theSymbolName, theFileName.Ptr, theFileName.Len);
	/* ����ַ����ᱻ" _Main__"���� */
    theSymbolName[theFileName.Len] = '\0';
	/* �õ�theSymbolName */
    ::strcat(theSymbolName, sMainEntrypointName.Ptr);
	/* �ǳ���Ҫ�ĵط�,��win32����::GetProcAddress()/Linux�µ�dlsys(),�������ں�����ָ�� */
    *outEntrypoint = (QTSS_MainEntryPointPtr)fFragment->GetSymbol(theSymbolName.GetObject());

    return QTSS_NoErr;
}

/* ����ָ����role������role״̬����fRoleArray[]�е���Ӧ����Ϊtrue,���ע�����ֽ�ɫ:QTSS_RTSPRequest_Role,QTSS_OpenFile_Role,QTSS_RTSPAuthenticate_Role */
QTSS_Error  QTSSModule::AddRole(QTSS_Role inRole)
{
	/* ��������roleֻ�ܱ�һ��modulesʹ��,��������������Module��,��������,�������������� */
    // There can only be one QTSS_RTSPRequest processing module
    if ((inRole == QTSS_RTSPRequest_Role) && (sHasRTSPRequestModule))
        return QTSS_RequestFailed;
    if ((inRole == QTSS_OpenFilePreProcess_Role) && (sHasOpenFileModule))
        return QTSS_RequestFailed;
    // There can be only one module registered for QTSS_RTSPAuthenticate_Role 
    if ((inRole == QTSS_RTSPAuthenticate_Role) && (sHasRTSPAuthenticateModule))
        return QTSS_RequestFailed;

	/* ���ÿ��ܵ�fRoleArray[kNumRoles] */
    switch (inRole)
    {
        // Map actual QTSS Role names to our private enum values. Turn on the proper one in the role array 
        case QTSS_Initialize_Role:          fRoleArray[kInitializeRole] = true;         break;
        case QTSS_Shutdown_Role:            fRoleArray[kShutdownRole] = true;           break;
        case QTSS_RTSPFilter_Role:          fRoleArray[kRTSPFilterRole] = true;         break;
        case QTSS_RTSPRoute_Role:           fRoleArray[kRTSPRouteRole] = true;          break;
        case QTSS_RTSPAuthenticate_Role:    fRoleArray[kRTSPAthnRole] = true;           break;
        case QTSS_RTSPAuthorize_Role:       fRoleArray[kRTSPAuthRole] = true;           break;
        case QTSS_RTSPPreProcessor_Role:    fRoleArray[kRTSPPreProcessorRole] = true;   break;
        case QTSS_RTSPRequest_Role:         fRoleArray[kRTSPRequestRole] = true;        break;
        case QTSS_RTSPPostProcessor_Role:   fRoleArray[kRTSPPostProcessorRole] = true;  break;
        case QTSS_RTSPSessionClosing_Role:  fRoleArray[kRTSPSessionClosingRole] = true; break;
        case QTSS_RTPSendPackets_Role:      fRoleArray[kRTPSendPacketsRole] = true;     break;
        case QTSS_ClientSessionClosing_Role:fRoleArray[kClientSessionClosingRole] = true;break;
        case QTSS_RTCPProcess_Role:         fRoleArray[kRTCPProcessRole] = true;        break;
        case QTSS_ErrorLog_Role:            fRoleArray[kErrorLogRole] = true;           break;
        case QTSS_RereadPrefs_Role:         fRoleArray[kRereadPrefsRole] = true;        break;
        case QTSS_OpenFile_Role:            fRoleArray[kOpenFileRole] = true;           break;
        case QTSS_OpenFilePreProcess_Role:  fRoleArray[kOpenFilePreProcessRole] = true; break;
        case QTSS_AdviseFile_Role:          fRoleArray[kAdviseFileRole] = true;         break;
        case QTSS_ReadFile_Role:            fRoleArray[kReadFileRole] = true;           break;
        case QTSS_CloseFile_Role:           fRoleArray[kCloseFileRole] = true;          break;
        case QTSS_RequestEventFile_Role:    fRoleArray[kRequestEventFileRole] = true;   break;
        case QTSS_RTSPIncomingData_Role:    fRoleArray[kRTSPIncomingDataRole] = true;   break;      
        case QTSS_StateChange_Role:         fRoleArray[kStateChangeRole] = true;        break;      
        case QTSS_Interval_Role:            fRoleArray[kTimedIntervalRole] = true;      break;      
        default:
            return QTSS_BadArgument;
    }
    
	/* ���������������� */
    if (inRole == QTSS_RTSPRequest_Role)
        sHasRTSPRequestModule = true;
    if (inRole == QTSS_OpenFile_Role)
        sHasOpenFileModule = true;
    if (inRole == QTSS_RTSPAuthenticate_Role)
        sHasRTSPAuthenticateModule = true;
        
    //
    // Add this role to the array of roles attribute
	/* �����������ӵ�ָ������λ��(�����) */
    QTSS_Error theErr = this->SetValue(qtssModRoles, this->GetNumValues(qtssModRoles), &inRole, sizeof(inRole), QTSSDictionary::kDontObeyReadOnly);
    Assert(theErr == QTSS_NoErr);
    return QTSS_NoErr;
}

/* ����������ʱ,������Update�¼�,�ͽ�ģ��״̬���µ�һ���µ�idle time;���籾ģ��ע����kTimedIntervalRole,�ڵ���һ�κ������ٴε���ͬһ���߳� */
SInt64 QTSSModule::Run()
{
    EventFlags events = this->GetEvents();

 	OSThreadDataSetter theSetter(&fModuleState, NULL);

	/* ������Update�¼�,�ͽ�ģ��״̬���µ�һ���µ�idle time */
    if (events & Task::kUpdateEvent)
    {   // force us to update to a new idle time
        return fModuleState.idleTime;// If the module has requested idle time...
    }
    
	/* ���籾ģ��ע����kTimedIntervalRole,�ڵ���һ�κ������ٴε���ͬһ���߳� */
    if (fRoleArray[kTimedIntervalRole])
    {
        if (events & Task::kIdleEvent || fModuleState.globalLockRequested)
        {
            fModuleState.curModule = this;  // this structure is setup in each thread
            fModuleState.curRole = QTSS_Interval_Role;    // before invoking a module in a role. Sometimes
            fModuleState.eventRequested = false;
            fModuleState.curTask = this;
            if (fModuleState.globalLockRequested )
            {   
				fModuleState.globalLockRequested = false;
                fModuleState.isGlobalLocked = true;
            } 
            
			/* ���÷ַ����� */
			//This gets called whenever the module's interval timer times out calls.
            (void)this->CallDispatch(QTSS_Interval_Role, NULL);
            fModuleState.isGlobalLocked = false;
    
			/* �����ٴε��� */
            if (fModuleState.globalLockRequested) // call this request back locked
                return this->CallLocked();
            
            return fModuleState.idleTime; // If the module has requested idle time...
        }
 	}
	
	return 0;
  }  
