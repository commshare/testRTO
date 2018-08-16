
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  SourceInfo.h
Description: define an interface to acquire the "interesting" information regarding a content source.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __SOURCE_INFO_H__
#define __SOURCE_INFO_H__

#include "QTSS.h"
#include "StrPtrLen.h"
#include "OSQueue.h"
#include "OS.h"

class SourceInfo
{
    public:
    
        SourceInfo() :  fStreamArray(NULL), fNumStreams(0),
                        fOutputArray(NULL), fNumOutputs(0),
                        fTimeSet(false),fStartTimeUnixSecs(0),fEndTimeUnixSecs(0),
                        fSessionControlType(kRTSPSessionControl)  {}
        SourceInfo(const SourceInfo& copy);// Does copy dynamically allocated data ���ƹ��캯��,������
        virtual ~SourceInfo(); // Deletes the dynamically allocated data
        
        enum
        {
            eDefaultBufferDelay = 3
        };
        
        // Returns whether this source is reflectable(�����).
        Bool16  IsReflectable(); //����� SourceInfo.cpp�ļ�

        // Each source is comprised of a set of streams. Those streams have
        // the following metadata.whose setup can be obtained by SDPSourceInfo::Parse()
        struct StreamInfo
        {
            StreamInfo() : fSrcIPAddr(0), fDestIPAddr(0), fPort(0), fTimeToLive(0), fPayloadType(0), fPayloadName(NULL), fTrackID(0), fBufferDelay((Float32) eDefaultBufferDelay), fIsTCP(false),fSetupToReceive(false), fTimeScale(0){}
            ~StreamInfo(); // Deletes the memory allocated for the fPayloadName string                eDefaultBufferDelayǿ������ת����Ĭ��3         
            
			/* �β�Ϊ������ */
            void Copy(const StreamInfo& copy);// Does copy dynamically allocated data //CopyΪ���ƹ��캯��,SourceInfo.cpp��Ҫ��
            
			/* important member data */

            UInt32 fSrcIPAddr;  // Src IP address of content (this may be 0 if not known for sure)
            UInt32 fDestIPAddr; // Dest IP address of content (destination IP addr for source broadcast!)
            UInt16 fPort;       // Dest (RTP) port of source content
            UInt16 fTimeToLive; // Ttl(��Ч�����) for this stream, 15
            QTSS_RTPPayloadType fPayloadType;   // Payload type of this stream(audio/Video),qtssAudioPayloadType, qtssVideoPayloadType, defined by QTSS.h
            StrPtrLen fPayloadName; // Payload name of this stream, string of StrPtrLen type, def in StrPtrLen.h
            UInt32 fTrackID;    // ID of this stream or track
            Float32 fBufferDelay; // buffer delay (default is 3 seconds)
            Bool16  fIsTCP;     // Is this a TCP broadcast? If this is the case "RTP/AVP/TCP", the port and ttl are not valid
            Bool16  fSetupToReceive;    // If true then a push to the server is setup on this stream.
            UInt32  fTimeScale; //timescale of RTP stream
        };
        
        // Returns the number of StreamInfo objects (number of Streams in this source)
        UInt32      GetNumStreams() { return fNumStreams; }
        StreamInfo* GetStreamInfo(UInt32 inStreamIndex);
        StreamInfo* GetStreamInfoByTrackID(UInt32 inTrackID);

		/*  Not all routers support multicasting. QuickTime clients behind routers that don��t implement multicasting
			can still receive a multicast by requesting the streams from a reflector. A reflector is an RTSP server that joins
			a multicast, then converts the multicast into a series of unicasts, passing the streams to clients who request
			them (see Figure 1-4). The original server may be sending live content, such as a concert or a news broadcast,
			or a previously-recorded movie. The reflector is always sending ��live�� data, passing the streams in real time.
		*/
         
        // If this source is to be Relayed(�м�), it may have "Output" information. This
        // tells the reader where to forward the incoming streams onto. There may be
        // 0 -> N OutputInfo objects in this SourceInfo. Each OutputInfo refers to a
        // single, complete copy of ALL the input streams. The fPortArray field
        // contains one RTP port for each incoming stream.
        struct OutputInfo
        {
            OutputInfo() : fDestAddr(0), fLocalAddr(0), fTimeToLive(0), fPortArray(NULL), fNumPorts(0), fBasePort(0), fAlreadySetup(false) {}
            ~OutputInfo(); // Deletes the memory allocated for fPortArray
            
            // Returns true if the two are equal
            Bool16 Equal(const OutputInfo& info);
            
            void Copy(const OutputInfo& copy);// Does copy dynamically allocated data

			/* important member data */

            UInt32 fDestAddr;       // Destination address to forward the input onto
            UInt32 fLocalAddr;      // Address of local interface to send out on (may be 0)
            UInt16 fTimeToLive;     // Time to live for resulting output (if multicast)
            UInt16* fPortArray;     // 1 destination RTP port for each Stream.
            UInt32 fNumPorts;       // Size of the fPortArray (usually equal to fNumStreams)
            UInt16 fBasePort;       // The base destination RTP port - for i=1 to fNumStreams fPortArray[i] = fPortArray[i-1] + 2
            Bool16  fAlreadySetup;  // A flag used in QTSSReflectorModule.cpp
        };

        // Returns the number of OutputInfo objects.
        UInt32      GetNumOutputs() { return fNumOutputs; }
        UInt32      GetNumNewOutputs(); // Returns # of outputs not already setup

		/* return info of  output stream with given output index */
        OutputInfo* GetOutputInfo(UInt32 inOutputIndex);
        
        // GetLocalSDP. This may or may not be supported by sources. Typically, if
        // the source is reflectable, this must be supported. It returns a newly
        // allocated buffer (that the caller is responsible for) containing an SDP
        // description of the source, stripped of(����) all network info.
		/* see SDPSourceInfo.h/cpp */
        virtual char*   GetLocalSDP(UInt32* /*newSDPLen*/) { return NULL; }
        
        // This is only supported by the RTSPSourceInfo sub class
        virtual Bool16 IsRTSPSourceInfo() { return false; }
        
        // This is only supported by the RCFSourceInfo sub class and its derived classes
        virtual char*   Name()  { return NULL; }
      
		/* equal operation between SourceInfo classes */
        virtual Bool16 Equal(SourceInfo* inInfo);
        
        // SDP scheduled times supports earliest start and latest end -- doesn't handle repeat times or multiple active times.

		/* time-related constant */
        #define kNTP_Offset_From_1970 2208988800LU

		/* serval time handling functions */
        time_t  NTPSecs_to_UnixSecs(time_t time) {return (time_t) (time - (UInt32)kNTP_Offset_From_1970);}
        UInt32  UnixSecs_to_NTPSecs(time_t time) {return (UInt32) (time + (UInt32)kNTP_Offset_From_1970);}
        Bool16  SetActiveNTPTimes(UInt32 startNTPTime,UInt32 endNTPTime);/* see SDPSourceInfo::Parse() */
        Bool16  IsValidNTPSecs(UInt32 time) {return time >= (UInt32) kNTP_Offset_From_1970 ? true : false;}
        Bool16  IsPermanentSource() { return ((fStartTimeUnixSecs == 0) && (fEndTimeUnixSecs == 0)) ? true : false; }
        Bool16  IsActiveTime(time_t unixTimeSecs);
        Bool16  IsActiveNow() { return IsActiveTime(OS::UnixTime_Secs()); }
        Bool16  IsRTSPControlled() {return (fSessionControlType == kRTSPSessionControl) ? true : false; }
        Bool16  HasTCPStreams();
        Bool16  HasIncomingBroacast();
        time_t  GetStartTimeUnixSecs() {return fStartTimeUnixSecs; }
        time_t  GetEndTimeUnixSecs() {return fEndTimeUnixSecs; }
        UInt32  GetDurationSecs();

		/* control type info */
        enum {kSDPTimeControl, kRTSPSessionControl};
    protected:
        
        //utility function used by IsReflectable(������IP��ַ)
        Bool16 IsReflectableIPAddr(UInt32 inIPAddr);

        /******************************* data member  **********************************/

		/* a 'm='line in sdp file corresponds to each stream/track */
		/* ��SDPSoureceInfo.cpp::parse()�õ� */
        StreamInfo* fStreamArray;
        UInt32      fNumStreams;

		/* info of output stream */
        OutputInfo* fOutputArray;
        UInt32      fNumOutputs;
        
		/* time set items */
        Bool16      fTimeSet;
        time_t      fStartTimeUnixSecs;
        time_t      fEndTimeUnixSecs;
    
		/* session control type, see SDPSourceInfo::Parse() in SDPSourceInfo.cpp */
		/* there are two: RTSPSessionControl, SDPTimeControl  */
        UInt32      fSessionControlType; //kRTSPSessionControl or kSDPTimeControl
        Bool16      fHasValidTime;
};



#endif //__SOURCE_INFO_H__
