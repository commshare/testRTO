
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  QTSServer.h
Description: define an object for bringing up & shutting down the RTSP serve, and also loads & initializes all modules.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSSERVER_H__
#define __QTSSERVER_H__

#include "QTSServerInterface.h"
#include "Task.h"


class RTCPTask;
class RTSPListenerSocket; //class definition refer to QTSServer.cpp
class RTPSocketPool; //as friend class,class definition refer to QTSServer.cpp

class QTSServer : public QTSServerInterface
{
    public:

        QTSServer() {} //ע�⹹�캯�������ñ�Initialize()ȡ��
        virtual ~QTSServer();

        //
        // Initialize
        //
        // This function starts the server. If it returns true, the server has
        // started up sucessfully. If it returns false, a fatal error occurred
        // while attempting to start the server.
        //
        // This function *must* be called before the server creates any threads,
        // because one of its actions is to change the server to the right UID / GID. UID-User ID/GID-Group ID.
        // Threads will only inherit these if they are created afterwards.
		/* �����������ĺ���!! */
        Bool16 Initialize(XMLPrefsParser* inPrefsSource,/* PrefsSource* inMessagesSource,*/
                            UInt16 inPortOverride, Bool16 createListeners); //note the difference:createListeners and CreateListeners (Line 99)
        
        //
        // InitModules
		// LOAD AND INITIALIZE ALL MODULES
        //
        // Initialize *does not* do much of the module initialization tasks. This
        // function may be called after the server has created threads, but the
        // server must not be in a state where it can do real work. In other words,
        // call this function *right after* calling Initialize.                   
        void InitModules(QTSS_ServerState inEndState);
        
        //
        // StartTasks
        //
        // The server has certain global tasks that it runs for things like stats
        // updating and RTCP processing. This function must be called to start those
        // going, and it must be called after Initialize 
		/* ����ĳЩglobal����(����״̬����,RTCP�����)�ĺ��� */
        void StartTasks();


        //
        // RereadPrefsService
        //
        // This service is registered by the server (calling "RereadPreferences").
        // It rereads the preferences. Anyone can call this to reread the preferences,
        // and it may be called safely at any time, though it will fail with a
        // QTSS_OutOfState if the server isn't in the qtssRunningState.
        /* Ԥ����������ģ���Ԥ��ֵ�ľ�̬����,�κ�ʱ�򶼿����� */
        static QTSS_Error RereadPrefsService(QTSS_ServiceFunctionArgsPtr inArgs);//����RereadPrefsTask��Ķ���ʱҪ����

        //
        // CreateListeners
        //
        // This function may be called multiple times & at any time.
        // It updates the server's listeners to reflect what the preferences say.
        // Returns false if server couldn't listen on one or more of the ports, true otherwise
		/* ������������ */
        Bool16                  CreateListeners(Bool16 startListeningNow, QTSServerPrefs* inPrefs, UInt16 inPortOverride);

        //
        // SetDefaultIPAddr
        //
        // Sets the IP address related attributes of the server.
        Bool16                  SetDefaultIPAddr();
        
        Bool16                  SetupUDPSockets();
                
        Bool16                  SwitchPersonality();

     private:
    
        // the following is data structures and variables to be used
        // GLOBAL TASKS
        RTCPTask*               fRTCPTask;
        RTPStatsUpdaterTask*    fStatsTask;
        //TimeoutTask*     fSessionTimeoutTask;

		/* static pref and callback routines */
        static char*            sPortPrefString; // "rtsp_port"
        static XMLPrefsParser*  sPrefsSource;/* ��������������QTSServer::Initialize() */
        //static PrefsSource*     sMessagesSource;
        static QTSS_Callbacks   sCallbacks; //Module loading & unloading routines
        
        // Sets up QTSS API callback routines   //callback routines �ص�����
        void                    InitCallbacks();
        
        // Loads compiled-in modules
        void                    LoadCompiledInModules();

        // Loads modules from disk
        void                    LoadModules(QTSServerPrefs* inPrefs);
        void                    CreateModule(char* inModuleFolderPath, char* inModuleName);
        
        // Adds a module to the module array(Queue)
        Bool16                  AddModule(QTSSModule* inModule);
        
        // Call module init roles
        void                    DoInitRole();
        void                    SetupPublicHeader();
        
        // Build & destroy the optimized role / module arrays for invoking modules
		/* ����OSQueue���й��ߴ������н�ɫ��module�����ͳ�Ƹý�ɫ�µ�module����  */
        void                    BuildModuleRoleArrays();
        void                    DestroyModuleRoleArrays();

		// �õ��󶨵�IP����,���һ��������Ĭ�ϵİ󶨵㲥ip
		UInt32*                 GetRTSPIPAddrs(QTSServerPrefs* inPrefs, UInt32* outNumAddrsPtr);
		// �õ�Ԥ��ĵ㲥�Ķ˿�����
		UInt16*                 GetRTSPPorts(QTSServerPrefs* inPrefs, UInt32* outNumPortsPtr);

        static pid_t            sMainPid;
         
		/* used in RTPSocketPool::ConstructUDPSocketPair() */
        friend class RTPSocketPool;
};

class RereadPrefsTask : public Task
{
public:
    virtual SInt64 Run()
    {
        QTSServer::RereadPrefsService(NULL);
        return -1;
    }
};


#endif // __QTSSERVER_H__


