
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
		/* ����QTSSDictionaryMap::SetAttribute()ѭ������6��ģ�������������ϢQTSSAttrInfoDict::AttrInfo(����) */
        static void     Initialize();
    
        // CONSTRUCTOR / SETUP / DESTRUCTOR
        
        // Specify the path to the code fragment if this module
        // is to be loaded from disk. If it is loaded from disk, the
        // name of the module will be its file name. Otherwise, the
        // inName parameter will set it.
		/* ��������ø����ݳ�Ա��ֵ,�õ�QTSSDictionary::SetVal(),��ο�QTSSDictionaryMap::SetAttribute() */
        QTSSModule(char* inName, char* inPath = NULL);

        // This function does all the module setup. If the module is being
        // loaded from disk, you need not pass in(����) a main entrypoint (as
        // it will be grabbed from the fragment). Otherwise, you must pass
        // in a main entrypoint(����ڵ�).
        // Note that this function does not invoke any public module roles.����������������κι�����Role
		/* ���ؾ�̬��̬��,�õ��ַ�����ָ��fDispatchFunc,����ָ����ʽ���ַ���"INFO: Module Loaded...QTSSRefMovieModule [dynamic]"д�������־ */
        QTSS_Error  SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint = NULL);

        // Doesn't free up internally allocated stuff,��Ҫ���ڲ��ͷŷ�����ڴ�
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
		/* ��ָ����Role,������Ӧ������,����ģ��ķַ�����(���ǶԷַ������İ�װ) */
        QTSS_Error  CallDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
            {  return (fDispatchFunc)(inRole, inParams);    }
        

        // These enums allow roles to be stored in a more optimized way
		// Each role represents a unique situation in which a module may be
		// invoked. Modules must specify which roles they want to be invoked for. 
        /* ģ������е�����role�б�����,ÿ��Role����ģ�����õ�Ψһ����,�ܽ�ɫ����kNumRoles */        
        enum
        {
            kInitializeRole =           0,
            kShutdownRole =             1,
            kRTSPFilterRole =           2,
            kRTSPRouteRole =            3,
            kRTSPAthnRole =             4,/* Authenticate,ֻ�ܱ�һ��Moduleʹ�� */      
            kRTSPAuthRole =             5,//Authorize
            kRTSPPreProcessorRole =     6,
            kRTSPRequestRole =          7,/* ֻ�ܱ�һ��Moduleʹ�� */
            kRTSPPostProcessorRole =    8,
            kRTSPSessionClosingRole =   9,
            kRTPSendPacketsRole =       10,/* RTPSession::run() needed */
            kClientSessionClosingRole = 11,/* RTPSession::run() needed */
            kRTCPProcessRole =          12,
            kErrorLogRole =             13,
            kRereadPrefsRole =          14,
            kOpenFileRole =             15,
            kOpenFilePreProcessRole =   16,/* ֻ�ܱ�һ��Moduleʹ�� */
            kAdviseFileRole =           17,
            kReadFileRole =             18,
            kCloseFileRole =            19,
            kRequestEventFileRole =     20,
            kRTSPIncomingDataRole =     21,
            kStateChangeRole =          22,
            kTimedIntervalRole =        23,/* QTSSModule::Run() needed */ //This gets called whenever the module's interval timer times out calls.
            
			/* ��ɫ����,��������ǳ���Ҫ,used in QTSServer::BuildModuleRoleArrays() */
            kNumRoles =                 24 //act as counting role
        };
        typedef UInt32 RoleIndex;
        
        // Call this to activate this module in the specified role.����ָ����role������role״̬����fRoleArray[]�е���Ӧ����Ϊtrue
        QTSS_Error  AddRole(QTSS_Role inRole);
        
		/* used in QTSServer::BuildModuleRoleArrays() */
        // This returns true if this module is supposed to run in the specified role.ָ��Role�Ƿ�Module����?
        Bool16  RunsInRole(RoleIndex inIndex) { Assert(inIndex < kNumRoles); return fRoleArray[inIndex]; }
        
		/********** �ǳ���Ҫ��һ������ ***********/
        SInt64 Run();
        /********** �ǳ���Ҫ��һ������ ***********/

        QTSS_ModuleState* GetModuleState() { return &fModuleState;}
        
    private:
    
		/* �Ӵ��̼���dll/.so,�����ָ������(Ҫ���û�ȡ�ļ�·��fPath,�����õ�ģ������)������ں���QTSS_MainEntryPointPtr��ָ�� */
        QTSS_Error LoadFromDisk(QTSS_MainEntryPointPtr* outEntrypoint);
  
        char*                       fPath;/* ģ���ļ�·�� */
        Bool16                      fRoleArray[kNumRoles];/* ��ģ����������Щ��ɫ��flag����,ע��ʮ����Ҫ,�μ�QTSSModule::AddRole() */
		QTSS_ModuleState            fModuleState;/* ����μ�QTSSPrivate.h */
        QTSS_DispatchFuncPtr        fDispatchFunc;/* very important! ģ��ķַ�����ָ��,������QTSSModule::SetupModule() */  
        QTSSPrefs*                  fPrefs;/* Module��Ԥ��ֵ */
        QTSSDictionary*             fAttributes; /* ģ��������ֵ� */
		OSCodeFragment*             fFragment;/* �ص���������Ƭ��,��ָ����Ϊ��,��Ϊstatic module,������dynamic module */
		OSQueueElem                 fQueueElem; /* ��ģ�����Ķ���Ԫ,ÿ��Module��Ϊһ������Ԫ�طŽ�Module���� */
        OSMutex                     fAttributesMutex;   

		/* ��������������ʾ������Role(QTSS_RTSPRequest_Role,QTSS_OpenFile_Role,QTSS_RTSPAuthenticate_Role)ֻ�ܱ�һ��modulesʹ��,��������������Module��,��������,�μ�QTSSModule::AddRole() */
        static Bool16               sHasRTSPRequestModule;
        static Bool16               sHasOpenFileModule;
        static Bool16               sHasRTSPAuthenticateModule;
     
		/* ���������Ϣ������(6������),����μ��μ�QTSSModule.cpp */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[]; 
         
};



#endif //__QTSSMODULE_H__
