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

/* 返回缓存中存放的按指定顺序排列的sdp数据的指针,并将写入sdp数据的长度给入参newSDPLen */
char* SDPSourceInfo::GetLocalSDP(UInt32* newSDPLen)
{
	/* make sure member fSDPData no null */
	/* 一定要确保存放sdp数据在内存中是存在的 */
    Assert(fSDPData.Ptr != NULL);

	/* append new connect info in a first 'm' line */
	/* 准备在m行前面添加一个新的c行 */
    Bool16 appendCLine = true;
    UInt32 trackIndex = 0;
    
	/* allocate buffer for localSDP */
	/* 下面这几行非常重要!! */
	/* 在缓存中新建一个存放区域,长度是原来的2倍 */
    char *localSDP = NEW char[fSDPData.Len * 2];
	/* 同时做好删除准备 */
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
		/* 通过fStartGet的移动来取得sdpLine,当fStartGet移到eol时 */
		/* 现在fStartGet已经越过eol */
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
                    
					/* 立即更改状态,以后不让再添加 */
                    appendCLine = false;
                }
                //the last "a=" for each m should be the control a=
                if ((trackIndex > 0) && (!hasControlLine))
                {
					/* 将形如"a=control:trackID=4"放入 */
                    qtss_sprintf(trackIndexBuffer, "a=control:trackID=%ld\r\n",trackIndex);
                    localSDPFormatter.Put(trackIndexBuffer, ::strlen(trackIndexBuffer));
                }
                //now write the 'm' line, but strip off(剥离)(delete) the port information
                StringParser mParser(&sdpLine);
                StrPtrLen mPrefix;
				/* stop if meet with digit numbers */
				/* fStartGet从m开始移动到数字前,将移过的字符串部分赋给mPrefix */
                mParser.ConsumeUntil(&mPrefix, StringParser::sDigitMask);
				/* 放入mPrefix和'0' */
                localSDPFormatter.Put(mPrefix);
                localSDPFormatter.Put("0", 1);
				/* fetch all digit number,不要管它 */
				/* 注意指针已经一个了这一连串的数字 */
                (void)mParser.ConsumeInteger(NULL);
				/* 将这串数字(代表端口号)之后的部分全都放入缓存 */
                localSDPFormatter.Put(mParser.GetCurrentPosition(), mParser.GetDataRemaining());
                localSDPFormatter.PutEOL();
				/* track index 加一 */
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
				/* 按照StringParser::sNonWordMask[]和ConsumeUntil定义,是英文字母就运动,不是就停下 */
				/* 将经过的字符串给aLineType,再比较它是否是sControlLine("a=control:*"),是就证明有控制行 */
                aParser.ConsumeWord(&aLineType);
				/* check if has control line */
                if (aLineType.Equal(sControlStr))
                    hasControlLine = true;
                break;
            }
            default:
            {
				/* 默认的操作是将sdp行逐个取出,末尾再加eol */
                localSDPFormatter.Put(sdpLine);
                localSDPFormatter.PutEOL();
            }
        }
    }
    
	/* 假如没有控制行,就将指定长度的字符串形如"a=control:trackID=4\r\n"放入缓存 */
    if ((trackIndex > 0) && (!hasControlLine))
    {
        qtss_sprintf(trackIndexBuffer, "a=control:trackID=%ld\r\n",trackIndex);
        localSDPFormatter.Put(trackIndexBuffer, ::strlen(trackIndexBuffer));
    }
	/* 回头检查一共写入了多少个字节的数据 */
    *newSDPLen = (UInt32)localSDPFormatter.GetCurrentOffset();
    
	/* theSDPStr现在是存放sdp数据的对象 */
    StrPtrLen theSDPStr(localSDP);
    SDPContainer rawSDPContainer; 
	/* 逐行解析sdp数据的合法性,并将合法的sdp行放入当前行的下一行,最后返回总的合法性检查(当然这里不需要,用Void暗示) */
    (void) rawSDPContainer.SetSDPBuffer( &theSDPStr ); 
	/*按指定顺序重排sdp行,开头字母重复行放在一起,主要形成两大部分:fSessionHeaders和fMediaHeaders*/
    SDPLineSorter sortedSDP(&rawSDPContainer);

	/* 得到排列好的sdp行的一个copy */
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
/* 这个函数的作用是从已有的sdp信息中提取出基类成员fStreamArray的结构体StreamInfo的信息,并配置fStreamArray的各项信息 */
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

    /* where is fStreamArray? see SourceInfo.h,它是结构体StreamInfo的数组 */
	/* 数组中的每一个元素都代表一个流 */
	/* def in below and it records the info of each stream in each track  */
	/* see  SourceInfo::SourceInfo() */
    Assert(fStreamArray == NULL);
    
   /***********************************************************************/
   /* transmit in-para  sdpData into member variable fSDPData              */
   /* note here carry on a type transform from string to StrPtrLen        */
   /***********************************************************************/

	/* 试问:为何要复制而不直接使用?这是为了确保为其分配一段缓存,类似SDPUtils.fSDPBuffer */
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
	/* StreamInfo结构体,给出了一个一般流的信息 */
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

    //walk through the SDP, counting up the number of tracks(计算sdp中track数)
    // Repeat until there's no more data in the SDP
    while (trackCounter.GetDataRemaining() > 0)
    {
        //each 'm' line in the SDP file corresponds to another track.
		/* sdpLine represents a common line to parse */
		/* 将sdpLine重塑成eol之前的字符串,但是fStartGet会继续移过eol再停下 */
        trackCounter.GetThruEOL(&sdpLine);
		/* 检查现在的sdp行是否满足条件? */
		/* check the first char of each sdp line with 'm' */
        if ((sdpLine.Len > 0) && (sdpLine.Ptr[0] == 'm'))
			/* 该量继承自基类SourceInfo的成员 */
            fNumStreams++;  
    }

    //We should scale the # of StreamInfos to the # of trax, but we can't because
    //of an annoying compiler bug...
    
	/* allocate buffer to store stream in each track */
	/* 我们现在已经找出了所有的m行(流),就建立流数组 */
    fStreamArray = NEW StreamInfo[fNumStreams];

    // set the default destination as our default IP address and set the default ttl
	/* 设置一般流的信息,下面会引用这些信息,定义见SourceInfo.h */
    theGlobalStreamInfo.fDestIPAddr = INADDR_ANY; //IN-internet,任意IP地址
    theGlobalStreamInfo.fTimeToLive = kDefaultTTL; //15, defined in SDPSourceInfo.h
        
    //Set bufferdelay to default of 3,see SourceInfo.h
    theGlobalStreamInfo.fBufferDelay = (Float32) eDefaultBufferDelay;
    
    /* parse information of each line using StringParser  */

    //Now actually get all the data on all the streams
	/* 现在重新具体地分析每个SDP行,得到我们想要的StreamInfo中需要的信息(上面的3条除外) */
	/* 注意比较SDPUtils::SDPContainer::Parse()和SDPSourceInfo::Parse() */
	/* judge the info of stream similar parse() function in SDPUtils.cpp */
    while (sdpParser.GetDataRemaining() > 0)
    {
		/* become sdpLine into string before eol */
        sdpParser.GetThruEOL(&sdpLine);
        if (sdpLine.Len == 0)
            continue;//skip over any blank lines

		/* 注意现在sdpLine值已改变,只代表一行sdp(前面可能有Whitespace) */
		/* judge the first char in different cases including 'tmac' */
        switch (*sdpLine.Ptr)
        {
			/* obtain time info by re-define a StringParser object */
            case 't':
            {
				/* initialize the fStartGet and fEndGet of object mParser using sdpLine */
				/* 实际上mParser应该改为tParser */
                StringParser mParser(&sdpLine);
                
				/* just move fStartGet before sDigitMask */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask);
				/* the start time of ntp */
				/* fetch all digit number in outString in turn to give a decimal integer */
				/* 注意此处fStartGet只是在取出集中在一起的数字时管用,当数字分散时它只能一个一个取,在逾越中间的空格后 */
				/* 取出所有的数字作为开始时间(NTP格式) */
                UInt32 ntpStart = mParser.ConsumeInteger(NULL);
                
				/* fStartGet继续跳跃到数字跟前 */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask); 
				/* the end time of ntp */
				/* fetch all digit number in outString in turn to give a decimal integer */
				/* 取出连在一起的一串数字字符 */
                UInt32 ntpEnd = mParser.ConsumeInteger(NULL);
                
				/* why? see SourceInfo.h */
				/* 设置所在流的生存时间(服务器时间系统上的一段时间) */
                SetActiveNTPTimes(ntpStart,ntpEnd);
            }
            break;
            
			/* obtain media info by re-define a StringParser object */
			/* 当出现一个m行时说明已经有了GlobalStreamInfo,将上面的赋值抄过来 */
			/* 下面再获取一些其它的关于StreamInfo的必要信息 */
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
				/* fStartGet向前移动2个单位 */
                mParser.ConsumeLength(NULL, 2);//go past 'm=' and skip 2 chars
				/* define new object to check stream type */
                StrPtrLen theStreamType;
				/* check stream type */
				/* 碰到字母就继续，碰到非字母就停下,将fStartGet扫过的字符串赋给theStreamType */
				/* theStreamType包括:audio,video,application,data,control */
                mParser.ConsumeWord(&theStreamType);
				/* find out the video/audio media type.Equal() def see StrPtrLen.cpp */
                if (theStreamType.Equal(sVideoStr))
                    fStreamArray[theStreamIndex].fPayloadType = qtssVideoPayloadType;
                else if (theStreamType.Equal(sAudioStr))
                    fStreamArray[theStreamIndex].fPayloadType = qtssAudioPayloadType;
                    
                //find the port for this stream
				/* fStartGet遇到数字就停下来 */
                mParser.ConsumeUntil(NULL, StringParser::sDigitMask);
				/* fetch temp port */
				/* 取出连续的一串数字作为端口号 */
                SInt32 tempPort = mParser.ConsumeInteger(NULL);
				/* check the temp port whether between 0 and 65536 */
                if ((tempPort > 0) && (tempPort < 65536))
                    fStreamArray[theStreamIndex].fPort = (UInt16) tempPort;
                    
                // find out whether this is TCP or UDP
				/* stop if meet with ' ' etc */
				/* 遇到空格whitespace字符就前进直到遇到non-whitespace就停下 */
                mParser.ConsumeWhitespace();
                StrPtrLen transportID;
				/* stop if meet with char  */
				/* 碰到字母就继续，碰到非字母就停下,将fStartGet扫过的字符串赋给transportID */
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

				/* fStartGet前进2个字符 */
                aParser.ConsumeLength(NULL, 2);//go past 'a='

				/* define object of attribute line type */
                StrPtrLen aLineType;

				/* stop if meet with english letter */
				/* 碰到字母就继续，碰到非字母就停下,将fStartGet扫过的字符串赋给aLineType */
                aParser.ConsumeWord(&aLineType);

                /****************************************************************************/
				//
				//下面的讨论分如下几种情形:
				//a=x-broadcastcontrol: RTSP/MIME
				//a=rtpmap
				//a=control
				//a=x-bufferdelay

                /*Brodcast control attribute has two broadcast method: RTSP and MIME */
				// sBroadcastControlStr("x-broadcastcontrol");
				/* 实际的sdp行信息是: a=x-broadcastcontrol: RTSP/MIME */
                if (aLineType.Equal(sBroadcastControlStr))

                {   // found a control line for the broadcast (delete at time or delete at end of broadcast/server startup) 

					/* 将在DOS窗口中显示信息:"found =x-broadcastcontrol\n"  */
                    // qtss_printf("found =%s\n",sBroadcastControlStr);

					/* stop if non-english letter */
					/* 按照StringParser::sWordMask[]和ConsumeUntil定义,是英文字母就停下,不是就运动 */
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
				/* 假如theStreamIndex的初始化还是0,说明没有见到m行,就忽略所有的a行(包括下面的),中断while循环 */
                if (theStreamIndex == 0)
                    break;
                
				/***注意下面的讨论假设theStreamIndex至少为1 **************************************/
				//

				/* set rtp map attribute  */
				/* sRtpMapStr("rtpmap") */				
                if (aLineType.Equal(sRtpMapStr))
                {
                    //mark the codec type if this line has a codec name on it. If we already
                    //have a codec type for this track, just ignore this line
					/* 假如上一个StreamInfo中没有fPayloadName并且这里只找到空格 */
					/* 注意fStartGet会移过''并返回true */
                    if ((fStreamArray[theStreamIndex - 1].fPayloadName.Len == 0) &&
                        (aParser.GetThru(NULL, ' ')))
                    {
						/* def payload name to parse */
                        StrPtrLen payloadNameFromParser;
						/* move the end of the line, return value is boolean */
						/* 找到eol就返回true，重塑payloadNameFromParser */
						/* 注意payloadNameFromParser是从' '后到eol前的这段,并且现在fStartGet已移过eol之后 */
                        (void)aParser.GetThruEOL(&payloadNameFromParser);
                        /* transform into C-String as temporary variable */
						/* 得到payloadNameFromParser的c-string */
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
					/* 下面会检查这项是否已被更改,若更改了,就要对所有的流对相同的更改! */
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
					/* 注意这个地方是错误的!更正如下: */
					//kParser.ConsumeEOL(&sdpLine);
					kParser.ConsumeUntil(&sdpLine,StringParser::sEOLMask);

					/*the length of string of key address */
					/* 这里是否加一? */
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
				/* 参见下面对全局流的override */

				/* connect sdp line def */
                StringParser cParser(&sdpLine);
				/* skip 9 chars backwards */
                cParser.ConsumeLength(NULL, 9);//strip off "c=in ip4 "

				/* GetIPAddr() def see below */
				/* note the fStartGet has reach before ip string */
				/* 使用该函数后,注意fStartGet停在了停止字符'/'前 */
                UInt32 tempIPAddr = SDPSourceInfo::GetIPAddr(&cParser, '/');
                                
                //grab the ttl
                SInt32 tempTtl = kDefaultTTL; //15
				/* 注意fStartGet先移动到'/'前面,再越过'/'并返回true,否则返回false */
				/* if go past '/' then return true */
                if (cParser.GetThru(NULL, '/'))
                {
					/* fetch all continuous digits after '/' */
					/* 注意fStartGet已经移过 '/',现在移动去取出连续的数字 */
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
					/* 这就是刚才创建的，重新覆盖它的值 */
                    theGlobalStreamInfo.fDestIPAddr = tempIPAddr;
                    theGlobalStreamInfo.fTimeToLive = (UInt16) tempTtl;
                    hasGlobalStreamInfo = true;
                }
            }
        }
    }       
    
	/* 上面若有某个"a=x-bufferdelay" 语句对某个流的bufferdelay作了更改,其它的流也得作同样的更改 */
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
/* 注意fStartGet停在了停止字符inStopChar前 */
UInt32 SDPSourceInfo::GetIPAddr(StringParser* inParser, char inStopChar)
{
    StrPtrLen ipAddrStr;

    // Get the IP addr str
	/* note inParser is string parser object,ConsumeUntil() def in StringParse.h/cpp */
	/* 注意fStartGet会一直移动到停止字符inStopChar前,并将经过的字符串给ipAddrStr */
    inParser->ConsumeUntil(&ipAddrStr, inStopChar);
    
	/* return 0 if null string */
	/* 这说明inParser在inStopChar前是空的,或者说inStopChar就是最前面的字符 */
    if (ipAddrStr.Len == 0)
        return 0;
    
    // NULL terminate it
	/* 特意将该字符串的结尾字符(不包括在字符串之内)更改为'\0'结尾 */
	/* temporary save the char just after the last char, note it is not included in ipAddrStr */
    char endChar = ipAddrStr.Ptr[ipAddrStr.Len];

	/* transform ipAddrStr into a common C-String with appending 0-terminated */
    ipAddrStr.Ptr[ipAddrStr.Len] = '\0';
    
    //inet_addr returns numeric IP addr in network byte order(网络字节序), make
    //sure to convert to host order(主机字节序).
	/* 字节序转换 */
    UInt32 ipAddr = SocketUtils::ConvertStringToAddr(ipAddrStr.Ptr);
    
    // Make sure to put the old char back!
	/* 切记将原来的结尾字符放回去,它可能是其它有用的部分 */
    ipAddrStr.Ptr[ipAddrStr.Len] = endChar;

    return ipAddr;
}

