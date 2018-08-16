
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSession.h
Description: Provides a class to manipulate transmission of the media data, 
             also receive and respond to client's feedback.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _RTPSESSION_H_
#define _RTPSESSION_H_

#include "RTPSessionInterface.h"
#include "RTSPRequestInterface.h"
#include "RTPStream.h"
#include "QTSSModule.h"


class RTPSession : public RTPSessionInterface
{
    public:
    
        RTPSession();
        virtual ~RTPSession();
        
        //ACCESS FUNCTIONS
  
		/* check module which sends packets,here is QTSSFileModule */
        QTSSModule* GetPacketSendingModule()                        { return fModule; }
		/* is there rtp stream now ? */
        Bool16      HasAnRTPStream()                                { return fHasAnRTPStream; }
    
		/* find rtp stream with the designated channel number */
		/* note a RTP session may have many(default 20) RTP stream including audio, video etc */
        RTPStream*  FindRTPStreamForChannelNum(UInt8 inChannelNum);
        
        // MODIFIERS
        
        //This puts this session into the session map (called by the server! not the module!)
        //If this function fails (it can return QTSS_DupName), it means that there is already
        //a session with this session ID in the map.
		/* ��Session Id����Session  */
        QTSS_Error  Activate(const StrPtrLen& inSessionID);
                
        // The way this object is implemented currently, only one module can send the packets for a session
		/* ������������þ�������ֻ��һ��ģ�����ǰ�Ự����,���ĵ�����RTSPSession::Run()�е�kPreprocessingRequest��kProcessingRequest�� */
        void        SetPacketSendingModule(QTSSModule* inModule)    { fModule = inModule; }

        //Once the session is bound, a module can add streams to it.It must pass in a trackID that uniquely identifies this stream.
		/* ���stream������track IDΨһȷ��,�μ�QTSS API doc-QTSS Objects */
        //This call can only be made during an RTSP Setup request, and the RTSPRequestInterface must be provided.
		/* add stream ֻ����RTSP Request�ڼ���� */
        //You may also opt to attach a codec name and type to this stream.
        QTSS_Error  AddStream(  RTSPRequestInterface* request, RTPStream** outStream,
                                    QTSS_AddStreamFlags inFlags);
        
		//Reset the thinning params(�򱡲���) for all streams using the late tolerance value
		void SetStreamThinningParams(Float32 inLateTolerance);
		
        //Begins playing all streams. Currently must be associated with an RTSP Play
        //request, and the request interface must be provided.
		/* QTSS_PlayFlags see QTSS.h and QTSS API Doc, �μ�Darwinʱ��ͼ */
        QTSS_Error      Play(RTSPRequestInterface* request, QTSS_PlayFlags inFlags);
        
        //Pauses all streams.
        void            Pause() { fState = qtssPausedState; }
        
        // Tears down the session. This will cause QTSS_SessionClosing_Role to run
		/* see handling of source codes for this case in RTPSession::run() */
        void            Teardown();

        //Utility functions.

		//Modules aren't required to use these, but can be useful
		//ע���SETUP��Ӧ����������:RTPStream::SendSetupResponse()/RTPSessionInterface::DoSessionSetupResponse()
        void            SendDescribeResponse(RTSPRequestInterface* request);
        void            SendAnnounceResponse(RTSPRequestInterface* request);
        void            SendPlayResponse(RTSPRequestInterface* request, UInt32 inFlags);
        void            SendPauseResponse(RTSPRequestInterface* request){ request->SendHeader(); }
        void            SendTeardownResponse(RTSPRequestInterface* request){ request->SetKeepAlive(false); request->SendHeader(); }
        // Quality Level                            
        SInt32          GetQualityLevel();
        void            SetQualityLevel(SInt32 level);

    private:
    
        //where timeouts, deletion conditions get processed
		/* inherited from Task class */
        virtual SInt64  Run();
        
        // Utility function used by Play
        UInt32 PowerOf2Floor(UInt32 inNumToFloor);
    
		//overbuffer logging function
		void   LogOverbufferStats();
	
		/* definition see other places */
		enum
		{
            kRTPStreamArraySize     = 20,/* һ��RTPSession����ӵ�е�RTPStream������20������ */
            kCantGetMutexIdleTime   = 10
        };

		// Module invocation (����) and module state.This info keeps track of(׷��) our current state so that the state machine (״̬��) works properly.
		/* current state of RTP session */
		enum
		{
			kStart                  = 0,
			kSendingPackets         = 1
		};
	
		char                fRTPStreamArray[kRTPStreamArraySize]; //default 20 in array size
		Bool16              fHasAnRTPStream; /* have a rtp stream ? */
		SInt32              fSessionQualityLevel;/* details including I,B,P frames */

		/* Module����,ע�ⷢ��RTP Packet���ⲿģ�����,��������������Ҫ!! */
		/* ������SetPacketSendingModule(),�õ���GetPacketSendingModule() */
        QTSSModule*         fModule; /* needed by RTPSession::run(),see QTSSModule.h  */
		/* mark the current active module in the array of modules in a common role, same as fCurrentModuleIndex above */
		UInt32              fCurrentModule;
		/* ��ǰModule���Index */
		UInt32              fCurrentModuleIndex;
		/* ����Module�ĵ�ǰ״̬ */
		UInt32              fCurrentState; /* see above enum,just select one of two */ 
		/* class QTSS_ModuleState def see QTSS_Private.h */
		/* record the related info of this QTSS module, used for OSThreadDataSetter in RTPSession::Run()  */
        QTSS_ModuleState    fModuleState;
		/* Client Session(RTP Session) �ر�ԭ���struct */
        QTSS_CliSesClosingReason fClosingReason;/* used in RTPSession::Run() */  
        // This is here to give the ability to the ClientSessionClosing role to do asynchronous I/O
		/* here may use RequestEvent */
        Bool16              fModuleDoingAsyncStuff;
        
#if DEBUG
        Bool16              fActivateCalled;
#endif
		/* needed by RTPSession::run() */
		/* record the last bandwidth */
        SInt64              fLastBandwidthTrackerStatsUpdate;
};

#endif //_RTPSESSION_H_
