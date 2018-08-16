
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

/* 静态变量的设置,参见QTSSModule::AddRole() */
Bool16  QTSSModule::sHasRTSPRequestModule = false;
Bool16  QTSSModule::sHasOpenFileModule = false;
Bool16  QTSSModule::sHasRTSPAuthenticateModule = false;

/* 属性信息数组,参见QTSSDictionary.cpp和QTSSModule::Initialize() */
/* 类似QTSServerInterface::sAttributes[] */
QTSSAttrInfoDict::AttrInfo  QTSSModule::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission, see QTSSAttrInfoDict::AttrInfo in QTSSDictionary.cpp/h */
    /* 0 */ { "qtssModName",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssModDesc",            NULL,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
    /* 2 */ { "qtssModVersion",         NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
    /* 3 */ { "qtssModRoles",           NULL,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4 */ { "qtssModPrefs",           NULL,                   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe  | qtssAttrModeInstanceAttrAllowed },
    /* 5 */ { "qtssModAttributes",      NULL,                   qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeInstanceAttrAllowed }
};

/* 利用QTSSDictionaryMap::SetAttribute()搭建模块字典ModuleDict的所有属性(6个) */
void QTSSModule::Initialize()
{
    //Setup all the dictionary stuff
	/* 从模块参数个数(6个)循环设置QTSS_ModuleObjectAttributes的相关值 */
    for (UInt32 x = 0; x < qtssModNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                            sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

/* 用入参设置各数据成员的值,用到QTSSDictionary::SetVal(),亦参考QTSSDictionaryMap::SetAttribute() */
QTSSModule::QTSSModule(char* inName, char* inPath)
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)),
    fQueueElem(NULL),
    fPath(NULL),
    fFragment(NULL),
    fDispatchFunc(NULL),
    fPrefs(NULL),
    fAttributes(NULL)
{
	/* 设置队列元的所在对象就是本模块实例 */
    fQueueElem.SetEnclosingObject(this);
    this->SetTaskName("QTSSModule");

	/* 用入参设置新建的fFragment和fPath */
    if ((inPath != NULL) && (inPath[0] != '\0'))
    {
        // Create a code fragment if this module is being loaded from disk 
        fFragment = NEW OSCodeFragment(inPath);
        fPath = NEW char[::strlen(inPath) + 2];
        ::strcpy(fPath, inPath);
    }
    
	/* 设置模块的属性字典fAttributes和预设值fPrefs,写入服务器内部,这个用法值得参考!!! */
    fAttributes = NEW QTSSDictionary( NULL, &fAttributesMutex );
    this->SetVal(qtssModPrefs,      &fPrefs,      sizeof(fPrefs));//此时是空指针
    this->SetVal(qtssModAttributes, &fAttributes, sizeof(fAttributes));
    
    // If there is a name, copy it into the module object's internal buffer
    if (inName != NULL)
        this->SetValue(qtssModName, 0, inName, ::strlen(inName), QTSSDictionary::kDontObeyReadOnly);
                
    ::memset(fRoleArray, 0, sizeof(fRoleArray));
    ::memset(&fModuleState, 0, sizeof(fModuleState));

}

/* 注意该函数十分重要!主要就是设置fDispatchFunc */
/* 定义并初始化QTSS_PrivateArgs,再从磁盘加载dll/.so得到和调用主入口函数QTSS_MainEntryPointPtr,从而得到分发函数指针并设置为fDispatchFunc,
   并将指定格式的moduleName字符串"Module Loaded...*** static/dynamic"写入错误日志 */
QTSS_Error  QTSSModule::SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint)
{
    QTSS_Error theErr = QTSS_NoErr;
    
    // Load fragment from disk if necessary
    /* 假如必要,从磁盘加载dll/.so,并获得指定符号的主入口函数QTSS_MainEntryPointPtr(第二个入参)的指针 */
    if ((fFragment != NULL) && (inEntrypoint == NULL))
		/* 利用OSCodeFragment::GetSymbol()获得主入口函数的指针 */
        theErr = this->LoadFromDisk(&inEntrypoint);
	/* 假如从磁盘加载错误,直接返回 */
    if (theErr != QTSS_NoErr)
        return theErr;
        
    // At this point, we must have an entrypoint
	/* 在此处一定要确保入口函数指针非空,不管是从dll/.so加载得到,还是已经存在的 */
    if (inEntrypoint == NULL)
        return QTSS_NotAModule;
        
    // Invoke the private initialization routine
	/* 定义并初始化QTSS_PrivateArgs,这个参数非常重要,参见 QTSS_Private.h */
    QTSS_PrivateArgs thePrivateArgs;
    thePrivateArgs.inServerAPIVersion = QTSS_API_VERSION;
    thePrivateArgs.inCallbacks = inCallbacks; //入参
    thePrivateArgs.outStubLibraryVersion = 0;//存根库版本
    thePrivateArgs.outDispatchFunction = NULL;/* 这是要设置的,每个Module的分发函数,比如QTSSFileModuleDispatch() */
    
	/* 用上配置参数做入参调用主入口函数QTSS_MainEntryPointPtr */
	/************** NOTE !! *********************/
	/* 调用每个Module必备的主函数例程,它会设置thePrivateArgs.outDispatchFunction,参见QTSS_Private.cpp中的 _stublibrary_main() */
    theErr = (inEntrypoint)(&thePrivateArgs);
	/* 假如调用主函数例程出错,立即返回  */
    if (theErr != QTSS_NoErr)
        return theErr;
   
	//假如存根库版本>API版本,立即返回
    if (thePrivateArgs.outStubLibraryVersion > thePrivateArgs.inServerAPIVersion)
        return QTSS_WrongVersion;
    
    // Set the dispatch function so we'll be able to invoke this module later on
    /* 用具体Module的分发函数(比如QTSSFileModuleDispatch())设置Module实例的分发函数,注意这步很重要,是我们要得到的结果 */
    fDispatchFunc = thePrivateArgs.outDispatchFunction;
    	
    // Log 
	/* 得到指定长度和格式的moduleName字符串"INFO: Module Loaded...QTSSRefMovieModule [dynamic]",并写入错误日志 */
    char msgStr[2048];
    char* moduleName = NULL;
    (void)this->GetValueAsString (qtssModName, 0, &moduleName);
    qtss_snprintf(msgStr, sizeof(msgStr), "Module Loaded...%s [%s]", moduleName, (fFragment==NULL)?"static":"dynamic");
    delete moduleName;
    QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);
	
     return QTSS_NoErr;
}

/* 从磁盘加载dll/.so,并获得指定符号(要利用获取文件路径fPath,解析得到模块名称)的主入口函数QTSS_MainEntryPointPtr的指针 */
QTSS_Error QTSSModule::LoadFromDisk(QTSS_MainEntryPointPtr* outEntrypoint)
{
	/* 注意这个字符串出现在所有的Modules名称中 */
    static StrPtrLen sMainEntrypointName("_Main");
    
	/* 确保指针非空,注意上面QTSSModule::SetupModule()中不是指针 */
    Assert(outEntrypoint != NULL);
    
    // Modules only need to be initialized if they reside on disk. 
	/* 当fFragment为空时,是static modules,直接返回 */
    if (fFragment == NULL)
        return QTSS_NoErr;
    
	/* 检查要加载的代码片段是否存在? */
    if (!fFragment->IsValid())
        return QTSS_NotAModule;
        
    // fPath is actually a path. Extract the file name.
    
	/**************** 这是一套操作字符串的经典的做法 ******************/
    StrPtrLen theFileName(fPath);
    StringParser thePathParser(&theFileName); 
	/* 从fPath中获得路径字符串,把它赋给theFileName */
    while (thePathParser.GetThru(&theFileName, kPathDelimiterChar))
        ;
    Assert(theFileName.Len > 0);
    Assert(theFileName.Ptr != NULL);

    // At this point, theFileName points to the file name. Make this the module name.
	/* 由文件名称设置模块名称 */
    this->SetValue(qtssModName, 0, theFileName.Ptr, theFileName.Len, QTSSDictionary::kDontObeyReadOnly);
    
    // 
    // The main entrypoint symbol name is the file name plus that _Main__ string up there.
    OSCharArrayDeleter theSymbolName(NEW char[theFileName.Len + sMainEntrypointName.Len + 2]);//加上"__"两个字符
    ::memcpy(theSymbolName, theFileName.Ptr, theFileName.Len);
	/* 这个字符随后会被" _Main__"覆盖 */
    theSymbolName[theFileName.Len] = '\0';
	/* 得到theSymbolName */
    ::strcat(theSymbolName, sMainEntrypointName.Ptr);
	/* 非常重要的地方,在win32下由::GetProcAddress()/Linux下的dlsys(),获得主入口函数的指针 */
    *outEntrypoint = (QTSS_MainEntryPointPtr)fFragment->GetSymbol(theSymbolName.GetObject());

    return QTSS_NoErr;
}

/* 根据指定的role来设置role状态数组fRoleArray[]中的相应分量为true,务必注意三种角色:QTSS_RTSPRequest_Role,QTSS_OpenFile_Role,QTSS_RTSPAuthenticate_Role */
QTSS_Error  QTSSModule::AddRole(QTSS_Role inRole)
{
	/* 下面三个role只能被一个modules使用,若现在已有其他Module用,不能再用,函数会立即返回 */
    // There can only be one QTSS_RTSPRequest processing module
    if ((inRole == QTSS_RTSPRequest_Role) && (sHasRTSPRequestModule))
        return QTSS_RequestFailed;
    if ((inRole == QTSS_OpenFilePreProcess_Role) && (sHasOpenFileModule))
        return QTSS_RequestFailed;
    // There can be only one module registered for QTSS_RTSPAuthenticate_Role 
    if ((inRole == QTSS_RTSPAuthenticate_Role) && (sHasRTSPAuthenticateModule))
        return QTSS_RequestFailed;

	/* 配置可能的fRoleArray[kNumRoles] */
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
    
	/* 重新设置这三个量 */
    if (inRole == QTSS_RTSPRequest_Role)
        sHasRTSPRequestModule = true;
    if (inRole == QTSS_OpenFile_Role)
        sHasOpenFileModule = true;
    if (inRole == QTSS_RTSPAuthenticate_Role)
        sHasRTSPAuthenticateModule = true;
        
    //
    // Add this role to the array of roles attribute
	/* 将给定入参添加到指定索引位置(最后面) */
    QTSS_Error theErr = this->SetValue(qtssModRoles, this->GetNumValues(qtssModRoles), &inRole, sizeof(inRole), QTSSDictionary::kDontObeyReadOnly);
    Assert(theErr == QTSS_NoErr);
    return QTSS_NoErr;
}

/* 该任务运行时,假如是Update事件,就将模块状态更新到一个新的idle time;假如本模块注册了kTimedIntervalRole,在调用一次后请求再次调用同一个线程 */
SInt64 QTSSModule::Run()
{
    EventFlags events = this->GetEvents();

 	OSThreadDataSetter theSetter(&fModuleState, NULL);

	/* 假如是Update事件,就将模块状态更新到一个新的idle time */
    if (events & Task::kUpdateEvent)
    {   // force us to update to a new idle time
        return fModuleState.idleTime;// If the module has requested idle time...
    }
    
	/* 假如本模块注册了kTimedIntervalRole,在调用一次后请求再次调用同一个线程 */
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
            
			/* 调用分发函数 */
			//This gets called whenever the module's interval timer times out calls.
            (void)this->CallDispatch(QTSS_Interval_Role, NULL);
            fModuleState.isGlobalLocked = false;
    
			/* 请求再次调用 */
            if (fModuleState.globalLockRequested) // call this request back locked
                return this->CallLocked();
            
            return fModuleState.idleTime; // If the module has requested idle time...
        }
 	}
	
	return 0;
  }  
