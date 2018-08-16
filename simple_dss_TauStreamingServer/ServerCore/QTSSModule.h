
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSModule.h
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


#ifndef __QTSSMODULE_H__
#define __QTSSMODULE_H__

#include "QTSS.h"
#include "QTSS_Private.h"
#include "QTSSDictionary.h"
#include "QTSSPrefs.h"
#include "Task.h"
#include "OSCodeFragment.h"
#include "OSQueue.h"
#include "StrPtrLen.h"

class QTSSModule : public QTSSDictionary, public Task
{
    public:
    
        // INITIALIZE
		/* 利用QTSSDictionaryMap::SetAttribute()循环设置6个模块参数的属性信息QTSSAttrInfoDict::AttrInfo(见上) */
        static void     Initialize();
    
        // CONSTRUCTOR / SETUP / DESTRUCTOR
        
        // Specify the path to the code fragment if this module
        // is to be loaded from disk. If it is loaded from disk, the
        // name of the module will be its file name. Otherwise, the
        // inName parameter will set it.
		/* 用入参设置各数据成员的值,用到QTSSDictionary::SetVal(),亦参考QTSSDictionaryMap::SetAttribute() */
        QTSSModule(char* inName, char* inPath = NULL);

        // This function does all the module setup. If the module is being
        // loaded from disk, you need not pass in(传递) a main entrypoint (as
        // it will be grabbed from the fragment). Otherwise, you must pass
        // in a main entrypoint(主入口点).
        // Note that this function does not invoke any public module roles.这个函数不会引用任何公共的Role
		/* 搭载静态或动态库,得到分发函数指针fDispatchFunc,并将指定格式的字符串"INFO: Module Loaded...QTSSRefMovieModule [dynamic]"写入错误日志 */
        QTSS_Error  SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint = NULL);

        // Doesn't free up internally allocated stuff,不要从内部释放分配的内存
        virtual ~QTSSModule(){}
        
        // MODIFIERS
        void            SetPrefsDict(QTSSPrefs* inPrefs) { fPrefs = inPrefs; }
        void            SetAttributesDict(QTSSDictionary* inAttributes) { fAttributes = inAttributes; }
        
        // ACCESSORS
        
		Bool16          IsInitialized() { return fDispatchFunc != NULL; } /* used in QTSServer::AddModule() */
        OSQueueElem*    GetQueueElem()  { return &fQueueElem; }
        QTSSPrefs*      GetPrefsDict()  { return fPrefs; }
        QTSSDictionary* GetAttributesDict() { return fAttributes; }
        OSMutex*        GetAttributesMutex() { return &fAttributesMutex; }
        
        // This calls into the module.
		/* 对指定的Role,设置相应参数后,调用模块的分发函数(仅是对分发函数的包装) */
        QTSS_Error  CallDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
            {  return (fDispatchFunc)(inRole, inParams);    }
        

        // These enums allow roles to be stored in a more optimized way
		// Each role represents a unique situation in which a module may be
		// invoked. Modules must specify which roles they want to be invoked for. 
        /* 模块可能有的所有role列表索引,每个Role代表模块引用的唯一场景,总角色数是kNumRoles */        
        enum
        {
            kInitializeRole =           0,
            kShutdownRole =             1,
            kRTSPFilterRole =           2,
            kRTSPRouteRole =            3,
            kRTSPAthnRole =             4,/* Authenticate,只能被一个Module使用 */      
            kRTSPAuthRole =             5,//Authorize
            kRTSPPreProcessorRole =     6,
            kRTSPRequestRole =          7,/* 只能被一个Module使用 */
            kRTSPPostProcessorRole =    8,
            kRTSPSessionClosingRole =   9,
            kRTPSendPacketsRole =       10,/* RTPSession::run() needed */
            kClientSessionClosingRole = 11,/* RTPSession::run() needed */
            kRTCPProcessRole =          12,
            kErrorLogRole =             13,
            kRereadPrefsRole =          14,
            kOpenFileRole =             15,
            kOpenFilePreProcessRole =   16,/* 只能被一个Module使用 */
            kAdviseFileRole =           17,
            kReadFileRole =             18,
            kCloseFileRole =            19,
            kRequestEventFileRole =     20,
            kRTSPIncomingDataRole =     21,
            kStateChangeRole =          22,
            kTimedIntervalRole =        23,/* QTSSModule::Run() needed */ //This gets called whenever the module's interval timer times out calls.
            
			/* 角色总数,这个参数非常重要,used in QTSServer::BuildModuleRoleArrays() */
            kNumRoles =                 24 //act as counting role
        };
        typedef UInt32 RoleIndex;
        
        // Call this to activate this module in the specified role.根据指定的role来设置role状态数组fRoleArray[]中的相应分量为true
        QTSS_Error  AddRole(QTSS_Role inRole);
        
		/* used in QTSServer::BuildModuleRoleArrays() */
        // This returns true if this module is supposed to run in the specified role.指定Role是否被Module引用?
        Bool16  RunsInRole(RoleIndex inIndex) { Assert(inIndex < kNumRoles); return fRoleArray[inIndex]; }
        
		/********** 非常重要的一个函数 ***********/
        SInt64 Run();
        /********** 非常重要的一个函数 ***********/

        QTSS_ModuleState* GetModuleState() { return &fModuleState;}
        
    private:
    
		/* 从磁盘加载dll/.so,并获得指定符号(要利用获取文件路径fPath,解析得到模块名称)的主入口函数QTSS_MainEntryPointPtr的指针 */
        QTSS_Error LoadFromDisk(QTSS_MainEntryPointPtr* outEntrypoint);
  
        char*                       fPath;/* 模块文件路径 */
        Bool16                      fRoleArray[kNumRoles];/* 该模块引用了哪些角色的flag数组,注意十分重要,参见QTSSModule::AddRole() */
		QTSS_ModuleState            fModuleState;/* 定义参见QTSSPrivate.h */
        QTSS_DispatchFuncPtr        fDispatchFunc;/* very important! 模块的分发函数指针,设置在QTSSModule::SetupModule() */  
        QTSSPrefs*                  fPrefs;/* Module的预设值 */
        QTSSDictionary*             fAttributes; /* 模块的属性字典 */
		OSCodeFragment*             fFragment;/* 回调函数代码片段,其指针若为空,即为static module,否则是dynamic module */
		OSQueueElem                 fQueueElem; /* 该模块代表的队列元,每个Module作为一个队列元素放进Module队列 */
        OSMutex                     fAttributesMutex;   

		/* 下面这三个量表示这三个Role(QTSS_RTSPRequest_Role,QTSS_OpenFile_Role,QTSS_RTSPAuthenticate_Role)只能被一个modules使用,若现在已有其他Module用,不能再用,参见QTSSModule::AddRole() */
        static Bool16               sHasRTSPRequestModule;
        static Bool16               sHasOpenFileModule;
        static Bool16               sHasRTSPAuthenticateModule;
     
		/* 相关属性信息的数组(6个分量),定义参见参见QTSSModule.cpp */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[]; 
         
};



#endif //__QTSSMODULE_H__
