/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SDPSourceInfo.h
Description: This object parsers input SDP data, and uses it to support the SourceInfo API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "SDPSourceInfo.h"
#include "StringParser.h"
#include "StringFormatter.h"
#include "StrPtrLen.h"
#include "SocketUtils.h"
#include "SDPUtils.h"
#include "OSArrayObjectDeleter.h"
#include "OSMemory.h"


/* make full use of the string PTR container class defined in StrPtrLen.h */
/* def and initialize the following objects of StrPtrLen class */
static StrPtrLen    sCLine("c=IN IP4 0.0.0.0");
static StrPtrLen    sControlLine("a=control:*");// value attribute
static StrPtrLen    sVideoStr("video"); //media
static StrPtrLen    sAudioStr("audio");
static StrPtrLen    sRtpMapStr("rtpmap");
static StrPtrLen    sControlStr("control");
static StrPtrLen    sBufferDelayStr("x-bufferdelay");
static StrPtrLen    sBroadcastControlStr("x-broadcastcontrol");
static StrPtrLen    sAutoDisconnect("RTSP"); /* RTSP Session Control */
static StrPtrLen    sAutoDisconnectTime("TIME"); /* SDP Time Control */

SDPSourceInfo::~SDPSourceInfo()
{
    // Not reqd(required) as the destructor of the 
    // base class will take care of delete the stream array
    // and output array if allocated
    /* 
    if (fStreamArray != NULL)
    {
        char* theCharArray = (char*)fStreamArray;
        delete [] theCharArray;
    }
    */
    
	/* Delete() is member func in StrPtrLen class */
    fSDPData.Delete();
}

/* ���ػ����д�ŵİ�ָ��˳�����е�sdp���ݵ�ָ��,����д��sdp���ݵĳ��ȸ����newSDPLen */
char* SDPSourceInfo::GetLocalSDP(UInt32* newSDPLen)
{
	/* make sure member fSDPData no null */
	/* һ��Ҫȷ�����sdp�������ڴ����Ǵ��ڵ� */
    Assert(fSDPData.Ptr != NULL);

	/* append new connect info in a first 'm' line */
	/* ׼����m��ǰ�����һ���µ�c�� */
    Bool16 appendCLine = true;
    UInt32 trackIndex = 0;
    
	/* allocate buffer for localSDP */
	/* �����⼸�зǳ���Ҫ!! */
	/* �ڻ������½�һ���������,������ԭ����2�� */
    char *localSDP = NEW char[fSDPData.Len * 2];
	/* ͬʱ����ɾ��׼�� */
    OSCharArrayDeleter charArrayPathDeleter(localSDP);
    StringFormatter localSDPFormatter(localSDP, fSDPData.Len * 2);

	/* frequently usage: consider each line of sdp file as a StrPtrLen Container Class */
    StrPtrLen sdpLine;
    StringParser sdpParser(&fSDPData);
    char trackIndexBuffer[50];
    
    // Only generate our own trackIDs if this file doesn't have 'em.
    // Our assumption here is that either the file has them, or it doesn't.
    // A file with some trackIDs, and some not, won't work.
    Bool16 hasControlLine = false;

    while (sdpParser.GetDataRemaining() > 0)
    {
        //stop when we reach an empty line.
		/* ͨ��fStartGet���ƶ���ȡ��sdpLine,��fStartGet�Ƶ�eolʱ */
		/* ����fStartGet�Ѿ�Խ��eol */
        sdpParser.GetThruEOL(&sdpLine);
        if (sdpLine.Len == 0)
            continue;
            
        switch (*sdpLine.Ptr)
        {
            case 'c':
                break;//ignore connection information
            case 'm':
            {
                //append new connection information right before the first 'm'
				
                if (appendCLine)
                {
					/* sCLine("c=IN IP4 0.0.0.0") */
                    localSDPFormatter.Put(sCLine);
                    localSDPFormatter.PutEOL();
                   
                    if (!hasControlLine)
                    { 
						/* sControlLine("a=control:*") */
                      localSDPFormatter.Put(sControlLine);
                      localSDPFormatter.PutEOL();
                    }
                    
					/* ��������״̬,�Ժ�������� */
                    appendCLine = false;
                }
                //the last "a=" for each m should be the control a=
                if ((trackIndex > 0) && (!hasControlLine))
                {
					/* ������"a=control:trackID=4"���� */
                    qtss_sprintf(trackIndexBuffer, "a=control:trackID=%ld\r\n",trackIndex);
                    localSDPFormatter.Put(trackIndexBuffer, ::strlen(trackIndexBuffer));
                }
                //now write the 'm' line, but strip off(����)(delete) the port information
                StringParser mParser(&sdpLine);
                StrPtrLen mPrefix;
				/* stop if meet with digit numbers */
				/* fStartGet��m��ʼ�ƶ�������ǰ,���ƹ����ַ������ָ���mPrefix */
                mParser.ConsumeUntil(&mPrefix, StringParser::sDigitMask);
				/* ����mPrefix��'0' */
                localSDPFormatter.Put(mPrefix);
                localSDPFormatter.Put("0", 1);
				/* fetch all digit number,��Ҫ���� */
				/* ע��ָ���Ѿ�һ������һ���������� */
                (void)mParser.ConsumeInteger(NULL);
				/* ���⴮����(����˿ں�)֮��Ĳ���ȫ�����뻺�� */
                localSDPFormatter.Put(mParser.GetCurrentPosition(), mParser.GetDataRemaining());
                localSDPFormatter.PutEOL();
				/* track index ��һ */
                trackIndex++;
                break;
            }
            case 'a':
            {
                localSDPFormatter.Put(sdpLine);
                localSDPFormatter.PutEOL();

                StringParser aParser(&sdpLine);
				/* skip 2 chars */
                aParser.ConsumeLength(NULL, 2);//go past 'a='
                StrPtrLen aLineType;
				/* ����StringParser::sNonWordMask[]��ConsumeUntil����,��Ӣ����ĸ���˶�,���Ǿ�ͣ�� */
				/* ���������ַ�����aLineType,�ٱȽ����Ƿ���sControlLine("a=control:*"),�Ǿ�֤���п����� */
                aParser.ConsumeWord(&aLineType);
				/* check if has control line */
                if (aLineType.Equal(sControlStr))
                    hasControlLine = true;
                break;
            }
            default:
            {
				/* Ĭ�ϵĲ����ǽ�sdp�����ȡ��,ĩβ�ټ�eol */
                localSDPFormatter.Put(sdpLine);
                localSDPFormatter.PutEOL();
            }
        }
    }
    
	/* ����û�п�����,�ͽ�ָ�����ȵ��ַ�������"a=control:trackID=4\r\n"���뻺�� */
    if ((trackIndex > 0) && (!hasControlLine))
    {
        qtss_sprintf(trackIndexBuffer, "a=control:trackID=%ld\r\n",trackIndex);
        localSDPFormatter.Put(trackIndexBuffer, ::strlen(trackIndexBuffer));
    }
	/* ��ͷ���һ��д���˶��ٸ��ֽڵ����� */
    *newSDPLen = (UInt32)localSDPFormatter.GetCurrentOffset();
    
	/* theSDPStr�����Ǵ��sdp���ݵĶ��� */
    StrPtrLen theSDPStr(localSDP);
    SDPContainer rawSDPContainer; 
	/* ���н���sdp���ݵĺϷ���,�����Ϸ���sdp�з��뵱ǰ�е���һ��,��󷵻��ܵĺϷ��Լ��(��Ȼ���ﲻ��Ҫ,��Void��ʾ) */
    (void) rawSDPContainer.SetSDPBuffer( &theSDPStr ); 
	/*��ָ��˳������sdp��,��ͷ��ĸ�ظ��з���һ��,��Ҫ�γ����󲿷�:fSessionHeaders��fMediaHeaders*/
    SDPLineSorter sortedSDP(&rawSDPContainer);

	/* �õ����кõ�sdp�е�һ��copy */
    return sortedSDP.GetSortedSDPCopy(); // return a new copy of the sorted SDP
}

/***************************ADDED by taoyx***********************************/
// Get the http-linked address of key to encrypt a MP4 file from DRM-Agent
char* SDPSourceInfo::GetKeyAddress(char* sdpData, UInt32 sdpLen)
{
	if (fSDPData.Ptr == NULL)
		return NULL;
	

  	/* allocate buffer to save sdpData copy */
    char *sdpDataCopy = NEW char[sdpLen];
    Assert(sdpDataCopy != NULL);
    
	/* Copy sdpData */
    memcpy(sdpDataCopy,sdpData, sdpLen);
	/* set value of member variable fSDPData and transmit sdpData into member fSDPData for further deal with */
    fSDPData.Set(sdpDataCopy, sdpLen);

	/* frequently usage: define each line of sdp file sdpLine as a object of StrPtrLen Container Class */
	/* sdpLine represents a common line to parse */
    StrPtrLen sdpLine;
	
	/* parse fSDPData, here define a object of StringParser class */
    StringParser sdpParser(&fSDPData);

    //Now actually get all the data on all the streams
	/* judge the info of stream similar parse() function in SDPUtils.cpp */
    while (sdpParser.GetDataRemaining() > 0)
    {
		/* become sdpLine into string before eol */
        sdpParser.GetThruEOL(&sdpLine);
        if (sdpLine.Len == 0)
            continue;//skip over any blank lines

		while (*sdpLine.Ptr == 'k')
		{
		 /* initialize the fStartGet and fEndGet of object mParser using sdpLine */
         StringParser kParser(&sdpLine);

		 /* step forward 6 chars and go past 'k=uri:'*/
         kParser.ConsumeLength(NULL, 6);

		 /* record original fStartGet position for later use */
		 char* KeyAddrBegin=kParser.GetCurrentPosition();
		 /* step forward until eol */
		 kParser.ConsumeEOL(&sdpLine);

		 /*the length of string of key address */
		 UInt32 KeyAddrLen=kParser.GetCurrentPosition()-KeyAddrBegin;

		 /* def and initialize a new object of StrLenPtr class */
		 StrPtrLen tempKeyAddr(KeyAddrBegin,KeyAddrLen);
		 /* type transform from StrLenPtr to char string  */
		 char* KeyAddr=tempKeyAddr.GetAsCString();

		 return KeyAddr;
                
		}

		return NULL;
	}

}
/***************************ADDED by taoyx***********************************/

/* THE IMPORTANT AND FUNDAMENTAL FUNCTION */
/* ��������������Ǵ����е�sdp��Ϣ����ȡ�������ԱfStreamArray�Ľṹ��StreamInfo����Ϣ,������fStreamArray�ĸ�����Ϣ */
/* note: input parameter is string sdpData whose length is sdpLen */
void SDPSourceInfo::Parse(char* sdpData, UInt32 sdpLen)
{
    //
    // There are some situations in which Parse can be called twice.
    // If that happens, just return and don't do anything the second time.
	/* note data member fSDPData's pointer has initial value null at first. If not, 
	means has been called once and no need to call again */
    if (fSDPData.Ptr != NULL)
        return;

    /* where is fStreamArray? see SourceInfo.h,���ǽṹ��StreamInfo������ */
	/* �����е�ÿһ��Ԫ�ض�����һ���� */
	/* def in below and it records the info of each stream in each track  */
	/* see  SourceInfo::SourceInfo() */
    Assert(fStreamArray == NULL);
    
   /***********************************************************************/
   /* transmit in-para  sdpData into member variable fSDPData              */
   /* note here carry on a type transform from string to StrPtrLen        */
   /***********************************************************************/

	/* ����:Ϊ��Ҫ���ƶ���ֱ��ʹ��?����Ϊ��ȷ��Ϊ�����һ�λ���,����SDPUtils.fSDPBuffer */
	/* allocate buffer to save sdpData copy */
    char *sdpDataCopy = NEW char[sdpLen];
    Assert(sdpDataCopy != NULL);
    
	/* Copy sdpData */
    memcpy(sdpDataCopy,sdpData, sdpLen);
	/* set value of member variable fSDPData and transmit sdpData into member fSDPData for further deal with */
    fSDPData.Set(sdpDataCopy, sdpLen);

    // If there is no trackID information in this SDP, we make the track IDs start
    // at 1 -> N
    UInt32 currentTrack = 1;
    
	/* initial value is set false because has not parse sdp data */
	/* flag of global stream info */
    Bool16 hasGlobalStreamInfo = false;
	/* metadata struct of global stream, see SourceInfo.h */
	/* StreamInfo�ṹ��,������һ��һ��������Ϣ */
    StreamInfo theGlobalStreamInfo; //needed if there is one c= header(in session level not in media level) 
                                    //independent of individual streams

   /**********************************************************/
   /* three important data struct:  object                   */
   /**********************************************************/

	/* frequently usage: define each line of sdp file sdpLine as a object of StrPtrLen Container Class */
	/* sdpLine represents a common line to parse */
    StrPtrLen sdpLine;
	/* calculate how many track in sdp file from the input-parameter fSDPData */
	/* def and initialize objects of class  */
    StringParser trackCounter(&fSDPData);
	/* parse fSDPData, here define a object of StringParser class */
    StringParser sdpParser(&fSDPData);

    /* obtain global stream info from fSDPData using trackCounter */

	/* note stream index begin from 0  */
    UInt32 theStreamIndex = 0;

    //walk through the SDP, counting up the number of tracks(����sdp��track��)
    // Repeat until there's no more data in the SDP
    while (trackCounter.GetDataRemaining() > 0)
    {
        //each 'm' line in the SDP file corresponds to another track.
		/* sdpLine represents a common line to parse */
		/* ��sdpLine���ܳ�eol֮ǰ���ַ���,����fStartGet������ƹ�eol��ͣ�� */
        trackCounter.GetThruEOL(&sdpLine);
		/* ������ڵ�sdp���Ƿ���������? */
		/* check the first char of each sdp line with 'm' */
        if ((sdpLine.Len > 0) && (sdpLine.Ptr[0] == 'm'))
			/* �����̳��Ի���SourceInfo�ĳ�Ա */
            fNumStreams++;  
    }

    //We should scale the # of StreamInfos to the # of trax, but we can't because
    //of an annoying compiler bug...
    
	/* allocate buffer to store stream in each track */
	/* ���������Ѿ��ҳ������е�m��(��),�ͽ��������� */
    fStreamArray = NEW StreamInfo[fNumStreams];

    // set the default destination as our default IP address and set the default ttl
	/* ����һ��������Ϣ,�����������Щ��Ϣ,�����SourceInfo.h */
    theGlobalStreamInfo.fDestIPAddr = INADDR_ANY; //IN-internet,����IP��ַ
    theGlobalStreamInfo.fTimeToLive = kDefaultTTL; //15, defined in SDPSourceInfo.h
        
    //Set bufferdelay to default of 3,see SourceInfo.h
    theGlobalStreamInfo.fBufferDelay = (Float32) eDefaultBufferDelay;
    
    /* parse information of each line using StringParser  */

    //Now actually get all the data on all the streams
	/* �������¾���ط���ÿ��SDP��,�õ�������Ҫ��StreamInfo����Ҫ����Ϣ(�����3������) */
	/* ע��Ƚ�SDPUtils::SDPContainer::Parse()��SDPSourceInfo::Parse() */
	/* judge the info of stream similar parse() function in SDPUtils.cpp */
    while (sdpParser.GetDataRemaining() > 0)
    {
		/* become sdpLine into string before eol */
        sdpParser.GetThruEOL(&sdpLine);
        if (sdpLine.Len == 0)
            continue;//skip over any blank lines

		/* ע������sdpLineֵ�Ѹı�,ֻ����һ��sdp(ǰ�������Whitespace) */
		/* judge the first char in different cases including 'tmac' */
        switch (*sdpLine.Ptr)
        {
			/* obtain time info by re-define a StringParser object */
            case 't':
            {
				/* initialize the fStartGet and fEndGet of object mParser using sdpLine */
				/* ʵ����mParserӦ�ø�ΪtParser */
                StringParser mParser(&sdpLine);
                
				/* just move fStartGet before sDigitMask */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask);
				/* the start time of ntp */
				/* fetch all digit number in outString in turn to give a decimal integer */
				/* ע��˴�fStartGetֻ����ȡ��������һ�������ʱ����,�����ַ�ɢʱ��ֻ��һ��һ��ȡ,����Խ�м�Ŀո�� */
				/* ȡ�����е�������Ϊ��ʼʱ��(NTP��ʽ) */
                UInt32 ntpStart = mParser.ConsumeInteger(NULL);
                
				/* fStartGet������Ծ�����ָ�ǰ */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask); 
				/* the end time of ntp */
				/* fetch all digit number in outString in turn to give a decimal integer */
				/* ȡ������һ���һ�������ַ� */
                UInt32 ntpEnd = mParser.ConsumeInteger(NULL);
                
				/* why? see SourceInfo.h */
				/* ����������������ʱ��(������ʱ��ϵͳ�ϵ�һ��ʱ��) */
                SetActiveNTPTimes(ntpStart,ntpEnd);
            }
            break;
            
			/* obtain media info by re-define a StringParser object */
			/* ������һ��m��ʱ˵���Ѿ�����GlobalStreamInfo,������ĸ�ֵ������ */
			/* �����ٻ�ȡһЩ�����Ĺ���StreamInfo�ı�Ҫ��Ϣ */
            case 'm':
            {
				/* check exist gobal stream info,initial val is fause, see above */
                if (hasGlobalStreamInfo)
                {
					/** theStreamIndex is 0 */
                    fStreamArray[theStreamIndex].fDestIPAddr = theGlobalStreamInfo.fDestIPAddr;//INADDR_ANY
                    fStreamArray[theStreamIndex].fTimeToLive = theGlobalStreamInfo.fTimeToLive;//15
                }
				/* default currentTrack is 1, see above */
                fStreamArray[theStreamIndex].fTrackID = currentTrack;//1
                currentTrack++;
                
				/* define new object */
                StringParser mParser(&sdpLine);
                
                //find out what type of track this is
				/* step forward 2 chars */
				/* fStartGet��ǰ�ƶ�2����λ */
                mParser.ConsumeLength(NULL, 2);//go past 'm=' and skip 2 chars
				/* define new object to check stream type */
                StrPtrLen theStreamType;
				/* check stream type */
				/* ������ĸ�ͼ�������������ĸ��ͣ��,��fStartGetɨ�����ַ�������theStreamType */
				/* theStreamType����:audio,video,application,data,control */
                mParser.ConsumeWord(&theStreamType);
				/* find out the video/audio media type.Equal() def see StrPtrLen.cpp */
                if (theStreamType.Equal(sVideoStr))
                    fStreamArray[theStreamIndex].fPayloadType = qtssVideoPayloadType;
                else if (theStreamType.Equal(sAudioStr))
                    fStreamArray[theStreamIndex].fPayloadType = qtssAudioPayloadType;
                    
                //find the port for this stream
				/* fStartGet�������־�ͣ���� */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask);
				/* fetch temp port */
				/* ȡ��������һ��������Ϊ�˿ں� */
                SInt32 tempPort = mParser.ConsumeInteger(NULL);
				/* check the temp port whether between 0 and 65536 */
                if ((tempPort > 0) && (tempPort < 65536))
                    fStreamArray[theStreamIndex].fPort = (UInt16) tempPort;
                    
                // find out whether this is TCP or UDP
				/* stop if meet with ' ' etc */
				/* �����ո�whitespace�ַ���ǰ��ֱ������non-whitespace��ͣ�� */
                mParser.ConsumeWhitespace();
                StrPtrLen transportID;
				/* stop if meet with char  */
				/* ������ĸ�ͼ�������������ĸ��ͣ��,��fStartGetɨ�����ַ�������transportID */
                mParser.ConsumeWord(&transportID);
                
				/* def object is TCP link */
				// set fIsTCP of struct SourceInfo::StreamInfo
                static const StrPtrLen kTCPTransportStr("RTP/AVP/TCP");
                if (transportID.Equal(kTCPTransportStr))
                    fStreamArray[theStreamIndex].fIsTCP = true;
          
				/* each m corresponds to a single stream */
                theStreamIndex++;
            }
            break;

			/* obtain attribute info by re-define a StringParser object */
			/* including broadcast control,rtpmap,bufferdelay,control attributes */
            case 'a':
            {
                StringParser aParser(&sdpLine);

				/* fStartGetǰ��2���ַ� */
                aParser.ConsumeLength(NULL, 2);//go past 'a='

				/* define object of attribute line type */
                StrPtrLen aLineType;

				/* stop if meet with english letter */
				/* ������ĸ�ͼ�������������ĸ��ͣ��,��fStartGetɨ�����ַ�������aLineType */
                aParser.ConsumeWord(&aLineType);

                /****************************************************************************/
				//
				//��������۷����¼�������:
				//a=x-broadcastcontrol: RTSP/MIME
				//a=rtpmap
				//a=control
				//a=x-bufferdelay

                /*Brodcast control attribute has two broadcast method: RTSP and MIME */
				// sBroadcastControlStr("x-broadcastcontrol");
				/* ʵ�ʵ�sdp����Ϣ��: a=x-broadcastcontrol: RTSP/MIME */
                if (aLineType.Equal(sBroadcastControlStr))

                {   // found a control line for the broadcast (delete at time or delete at end of broadcast/server startup) 

					/* ����DOS��������ʾ��Ϣ:"found =x-broadcastcontrol\n"  */
                    // qtss_printf("found =%s\n",sBroadcastControlStr);

					/* stop if non-english letter */
					/* ����StringParser::sWordMask[]��ConsumeUntil����,��Ӣ����ĸ��ͣ��,���Ǿ��˶� */
                    aParser.ConsumeUntil(NULL,StringParser::sWordMask);

					/* session control type: RTSPSessionControl and SDPTimeControl  */
					/* def object of session control type */
                    StrPtrLen sessionControlType;

					/* stop if meet with english letter */
                    aParser.ConsumeWord(&sessionControlType);

					/* check session control types,sAutoDisconnect("RTSP") */
                    if (sessionControlType.Equal(sAutoDisconnect))
                    {
                       fSessionControlType = kRTSPSessionControl; 
                    }       
                    else if (sessionControlType.Equal(sAutoDisconnectTime))
                    {
                       fSessionControlType = kSDPTimeControl; 
                    }       
                    

                }

                //if we haven't even hit an 'm' line yet, just ignore all 'a' lines
				/* ����theStreamIndex�ĳ�ʼ������0,˵��û�м���m��,�ͺ������е�a��(���������),�ж�whileѭ�� */
                if (theStreamIndex == 0)
                    break;
                
				/***ע����������ۼ���theStreamIndex����Ϊ1 **************************************/
				//

				/* set rtp map attribute  */
				/* sRtpMapStr("rtpmap") */				
                if (aLineType.Equal(sRtpMapStr))
                {
                    //mark the codec type if this line has a codec name on it. If we already
                    //have a codec type for this track, just ignore this line
					/* ������һ��StreamInfo��û��fPayloadName��������ֻ�ҵ��ո� */
					/* ע��fStartGet���ƹ�''������true */
                    if ((fStreamArray[theStreamIndex - 1].fPayloadName.Len == 0) &&
                        (aParser.GetThru(NULL, ' ')))
                    {
						/* def payload name to parse */
                        StrPtrLen payloadNameFromParser;
						/* move the end of the line, return value is boolean */
						/* �ҵ�eol�ͷ���true������payloadNameFromParser */
						/* ע��payloadNameFromParser�Ǵ�' '��eolǰ�����,��������fStartGet���ƹ�eol֮�� */
                        (void)aParser.GetThruEOL(&payloadNameFromParser);
                        /* transform into C-String as temporary variable */
						/* �õ�payloadNameFromParser��c-string */
                        char* temp = payloadNameFromParser.GetAsCString();
//                     qtss_printf("payloadNameFromParser (%x) = %s\n", temp, temp);
						/* set payload name of last StreamInfo */
                       (fStreamArray[theStreamIndex - 1].fPayloadName).Set(temp, payloadNameFromParser.Len);
//                     qtss_printf("%s\n", fStreamArray[theStreamIndex - 1].fPayloadName.Ptr);
                    }
                } 
				/* set a ordinary control attribute */
				/* sControlStr("control") */
                else if (aLineType.Equal(sControlStr))
                {           
                    //mark the trackID if that's what this line has
					/* advance until digit */
                    aParser.ConsumeUntil(NULL, StringParser::sDigitMask);
					/* fetch all number and set to track id */
                    fStreamArray[theStreamIndex - 1].fTrackID = aParser.ConsumeInteger(NULL);
                }
				/* set a bufferdelay  attribute */
				/* sBufferDelayStr("x-bufferdelay") */
                else if (aLineType.Equal(sBufferDelayStr))
                {   // if a BufferDelay is found then set all of the streams to the same buffer delay (it's global)
					/* advance until digit number */
                    aParser.ConsumeUntil(NULL, StringParser::sDigitMask);
					/* fetch all float number, reset global buffer delay again */
					/* note the dauflt delay is 3,see SourceInfo.h */
					/* ������������Ƿ��ѱ�����,��������,��Ҫ�����е�������ͬ�ĸ���! */
                    theGlobalStreamInfo.fBufferDelay = aParser.ConsumeFloat();
                }

            }
            break;

			/***************************ADDED by taoyx***********************************/
			case 'k':
				{
					/* initialize the fStartGet and fEndGet of object mParser using sdpLine */
					StringParser kParser(&sdpLine);

					/* step forward 6 chars and go past 'k=uri:'*/
					kParser.ConsumeLength(NULL, 6);

					/* record original fStartGet position for later use */
					char* KeyAddrBegin=kParser.GetCurrentPosition();
					/* step forward until eol */
					/* ע������ط��Ǵ����!��������: */
					//kParser.ConsumeEOL(&sdpLine);
					kParser.ConsumeUntil(&sdpLine,StringParser::sEOLMask);

					/*the length of string of key address */
					/* �����Ƿ��һ? */
					UInt32 KeyAddrLen=kParser.GetCurrentPosition()-KeyAddrBegin;

					/* def and initialize a new object of StrLenPtr class */
					StrPtrLen tempKeyAddr(KeyAddrBegin,KeyAddrLen);
					/* type transform from StrLenPtr to C-string  */
					char* keyAddress=tempKeyAddr.GetAsCString();	

				}
			break;
				/***************************ADDED by taoyx***********************************/

			/* obtain connect info by re-define a StringParser object */
            case 'c':
            {
                //get the Dest IP address off this header
				/* �μ������ȫ������override */

				/* connect sdp line def */
                StringParser cParser(&sdpLine);
				/* skip 9 chars backwards */
                cParser.ConsumeLength(NULL, 9);//strip off "c=in ip4 "

				/* GetIPAddr() def see below */
				/* note the fStartGet has reach before ip string */
				/* ʹ�øú�����,ע��fStartGetͣ����ֹͣ�ַ�'/'ǰ */
                UInt32 tempIPAddr = SDPSourceInfo::GetIPAddr(&cParser, '/');
                                
                //grab the ttl
                SInt32 tempTtl = kDefaultTTL; //15
				/* ע��fStartGet���ƶ���'/'ǰ��,��Խ��'/'������true,���򷵻�false */
				/* if go past '/' then return true */
                if (cParser.GetThru(NULL, '/'))
                {
					/* fetch all continuous digits after '/' */
					/* ע��fStartGet�Ѿ��ƹ� '/',�����ƶ�ȥȡ������������ */
                    tempTtl = cParser.ConsumeInteger(NULL);
					/* make sure tempTtl between 0 and 65536=2^16 */
                    Assert(tempTtl >= 0);
                    Assert(tempTtl < 65536);
                }

                if (theStreamIndex > 0)
                {
                    //if this c= line is part of a stream, it overrides the
                    //global stream information, note stream index begin from 0
					/* override global stream information */
                    fStreamArray[theStreamIndex - 1].fDestIPAddr = tempIPAddr;
                    fStreamArray[theStreamIndex - 1].fTimeToLive = (UInt16) tempTtl; /* ttl occupy 2 bytes */
                } 
				else 
				{
					/* just have one stream not stream array */
					/* ����ǸղŴ����ģ����¸�������ֵ */
                    theGlobalStreamInfo.fDestIPAddr = tempIPAddr;
                    theGlobalStreamInfo.fTimeToLive = (UInt16) tempTtl;
                    hasGlobalStreamInfo = true;
                }
            }
        }
    }       
    
	/* ��������ĳ��"a=x-bufferdelay" ����ĳ������bufferdelay���˸���,��������Ҳ����ͬ���ĸ��� */
    // Add the default buffer delay 3, see SourceInfo.h
    Float32 bufferDelay = (Float32) eDefaultBufferDelay;
	/* check the ttl change or not  */
    if (theGlobalStreamInfo.fBufferDelay != (Float32) eDefaultBufferDelay)
		/* reset if ttl has changed */
        bufferDelay = theGlobalStreamInfo.fBufferDelay;
    
	/* update the buffer delay for each stream in stream array */
    UInt32 count = 0;
    while (count < fNumStreams)
    {   fStreamArray[count].fBufferDelay = bufferDelay;
        count ++;
    }
        
}

/* THE IMPORTANT FUNCTION */
/* used in SDPSourceInfo::Parse() to obtain the Dest IP Address */
/* return numeric IP address in host order by triming from soure string */
/* ע��fStartGetͣ����ֹͣ�ַ�inStopCharǰ */
UInt32 SDPSourceInfo::GetIPAddr(StringParser* inParser, char inStopChar)
{
    StrPtrLen ipAddrStr;

    // Get the IP addr str
	/* note inParser is string parser object,ConsumeUntil() def in StringParse.h/cpp */
	/* ע��fStartGet��һֱ�ƶ���ֹͣ�ַ�inStopCharǰ,�����������ַ�����ipAddrStr */
    inParser->ConsumeUntil(&ipAddrStr, inStopChar);
    
	/* return 0 if null string */
	/* ��˵��inParser��inStopCharǰ�ǿյ�,����˵inStopChar������ǰ����ַ� */
    if (ipAddrStr.Len == 0)
        return 0;
    
    // NULL terminate it
	/* ���⽫���ַ����Ľ�β�ַ�(���������ַ���֮��)����Ϊ'\0'��β */
	/* temporary save the char just after the last char, note it is not included in ipAddrStr */
    char endChar = ipAddrStr.Ptr[ipAddrStr.Len];

	/* transform ipAddrStr into a common C-String with appending 0-terminated */
    ipAddrStr.Ptr[ipAddrStr.Len] = '\0';
    
    //inet_addr returns numeric IP addr in network byte order(�����ֽ���), make
    //sure to convert to host order(�����ֽ���).
	/* �ֽ���ת�� */
    UInt32 ipAddr = SocketUtils::ConvertStringToAddr(ipAddrStr.Ptr);
    
    // Make sure to put the old char back!
	/* �мǽ�ԭ���Ľ�β�ַ��Ż�ȥ,���������������õĲ��� */
    ipAddrStr.Ptr[ipAddrStr.Len] = endChar;

    return ipAddr;
}

