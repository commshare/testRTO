/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	  SourceInfo.cpp
Description: define an interface to acquire the "interesting" information regarding a content source.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#include "SourceInfo.h"
#include "SDPSourceInfo.h"
#include "SocketUtils.h"
#include "StringParser.h"
#include "OSMemory.h"

SourceInfo::SourceInfo(const SourceInfo& copy) //复制构造函数并用参数列表初始化
:   fStreamArray(NULL), fNumStreams(copy.fNumStreams), 
    fOutputArray(NULL), fNumOutputs(copy.fNumOutputs),
    fTimeSet(copy.fTimeSet),fStartTimeUnixSecs(copy.fStartTimeUnixSecs),
    fEndTimeUnixSecs(copy.fEndTimeUnixSecs), fSessionControlType(copy.fSessionControlType),
    fHasValidTime(false) //相对于原copy重新定义的函数
{   
    
    if(copy.fStreamArray != NULL && fNumStreams != 0)
    {
        fStreamArray = NEW StreamInfo[fNumStreams]; //创建fStreamArray
        for (UInt32 index=0; index < fNumStreams; index++)
            fStreamArray[index].Copy(copy.fStreamArray[index]); //Copy是复制函数,向新创建的数组传入值,定义见SourceInfo.h
    }
    
    if(copy.fOutputArray != NULL && fNumOutputs != 0)
    {
        fOutputArray = NEW OutputInfo[fNumOutputs]; //创建fOutputArray
        for (UInt32 index2=0; index2 < fNumOutputs; index2++)
            fOutputArray[index2].Copy(copy.fOutputArray[index2]);
    }
    
}

SourceInfo::~SourceInfo() //析构函数定义,在SourceInfo类外定义
{
    if(fStreamArray != NULL)
        delete [] fStreamArray; //加[]表示对数组操作,删除创建的数组,释放内存

    if(fOutputArray != NULL)
        delete [] fOutputArray;
        
}

Bool16  SourceInfo::IsReflectable()//IsReflectable函数在SourceInfo类外定义
{
    if (fStreamArray == NULL)
        return false;
    if (fNumStreams == 0)
        return false;
        
    //each stream's info must meet certain criteria
    for (UInt32 x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fIsTCP)
            continue;
                    //定义见下面
        if ((!this->IsReflectableIPAddr(fStreamArray[x].fDestIPAddr)) ||
            (fStreamArray[x].fTimeToLive == 0))
            return false;
    }
    return true;
}

Bool16  SourceInfo::IsReflectableIPAddr(UInt32 inIPAddr)//接应上面的函数声明
{
    if (SocketUtils::IsMulticastIPAddr(inIPAddr) || SocketUtils::IsLocalIPAddr(inIPAddr))
        return true;
    return false;
}

Bool16  SourceInfo::HasTCPStreams()
{   
    //each stream's info must meet certain criteria
    for (UInt32 x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fIsTCP)
            return true;
    }
    return false;
}

Bool16  SourceInfo::HasIncomingBroacast()
{   
    //each stream's info must meet certain criteria
    for (UInt32 x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fSetupToReceive)
            return true;
    }
    return false;
}
SourceInfo::StreamInfo* SourceInfo::GetStreamInfo(UInt32 inIndex) //GetStreamInfo在SourceInfo类外定义
{
    Assert(inIndex < fNumStreams);//判断条件是否成立,真时不执行任何操作,假时给出错误信息并返回空指针,经常用到
    if (fStreamArray == NULL)
        return NULL;
    if (inIndex < fNumStreams)
		/* 用SDPSoureceInfo.cpp::parse()得到 */
        return &fStreamArray[inIndex]; //返回fStreamArray数组中第inIndex-1个分量的值,指针类型
    else
        return NULL;
}

SourceInfo::StreamInfo* SourceInfo::GetStreamInfoByTrackID(UInt32 inTrackID)//GetStreamInfoByTrackID在SourceInfo类外定义
{
    if (fStreamArray == NULL)
        return NULL;
    for (UInt32 x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fTrackID == inTrackID)
            return &fStreamArray[x]; //返回fStreamArray数组中第x-1个分量的值
    }
    return NULL;
}

SourceInfo::OutputInfo* SourceInfo::GetOutputInfo(UInt32 inIndex)//GetOutputInfo在SourceInfo类外定义
{
    Assert(inIndex < fNumOutputs);
    if (fOutputArray == NULL)
        return NULL;
    if (inIndex < fNumOutputs)
        return &fOutputArray[inIndex];//返回fOutputArray数组第inIndex-1个值,指针类型
    else
        return NULL;
}

UInt32 SourceInfo::GetNumNewOutputs() //得到第几个流开始输出
{
    UInt32 theNumNewOutputs = 0; //定义并初始化变量
    for (UInt32 x = 0; x < fNumOutputs; x++)
    {
        if (!fOutputArray[x].fAlreadySetup)
            theNumNewOutputs++;
    }
    return theNumNewOutputs;
}

Bool16  SourceInfo::SetActiveNTPTimes(UInt32 startTimeNTP,UInt32 endTimeNTP)
{   // right now only handles earliest start and latest end time.

    //qtss_printf("SourceInfo::SetActiveNTPTimes start=%lu end=%lu\n",startTimeNTP,endTimeNTP); %lu表示无符号长整形数输出
    Bool16 accepted = false;
    do //do-while直接执行循环体再判断条件,此处只执行一次循环体
    {
        if ((startTimeNTP > 0) && (endTimeNTP > 0) && (endTimeNTP < startTimeNTP)) break; // not valid NTP time
        
        UInt32 startTimeUnixSecs = 0; //对形参初始化
        UInt32 endTimeUnixSecs  = 0; 
        
        if (startTimeNTP != 0 && IsValidNTPSecs(startTimeNTP)) // allow anything less than 1970 
            startTimeUnixSecs = NTPSecs_to_UnixSecs(startTimeNTP);// convert to 1970 time
        
        if (endTimeNTP != 0 && !IsValidNTPSecs(endTimeNTP)) // don't allow anything less than 1970
            break;
            
        if (endTimeNTP != 0) // convert to 1970 time
            endTimeUnixSecs = NTPSecs_to_UnixSecs(endTimeNTP);

        fStartTimeUnixSecs = startTimeUnixSecs;
        fEndTimeUnixSecs = endTimeUnixSecs; 
        accepted = true;
        
    }  while(0);
    
    //char buffer[kTimeStrSize];
    //qtss_printf("SourceInfo::SetActiveNTPTimes fStartTimeUnixSecs=%lu fEndTimeUnixSecs=%lu\n",fStartTimeUnixSecs,fEndTimeUnixSecs);
    //qtss_printf("SourceInfo::SetActiveNTPTimes start time = %s",qtss_ctime(&fStartTimeUnixSecs, buffer, sizeof(buffer)) );
    //qtss_printf("SourceInfo::SetActiveNTPTimes end time = %s",qtss_ctime(&fEndTimeUnixSecs, buffer, sizeof(buffer)) );
    fHasValidTime = accepted;
    return accepted;
}

Bool16  SourceInfo::IsActiveTime(time_t unixTimeSecs)
{ 
    // order of tests are important here
    // we do it this way because of the special case time value of 0 for end time
    // start - 0 = unbounded 
    // 0 - 0 = permanent
    if (false == fHasValidTime) //fHasValidTime见上个函数定义,值真
        return false;
        
    if (unixTimeSecs < 0) //check valid value unixTimeSecs是形参
        return false;
        
    if (IsPermanentSource()) //check for 0 0
        return true;
    
    if (unixTimeSecs < fStartTimeUnixSecs) //fStartTimeUnixSecs见上个函数定义,unixTimeSecs是形参
        return false; //too early

    if (fEndTimeUnixSecs == 0)  //fEndTimeUnixSecs见上个函数定义
        return true;// accept any time after start

    if (unixTimeSecs > fEndTimeUnixSecs)
        return false; // too late

    return true; // ok start <= time <= end

}


UInt32 SourceInfo::GetDurationSecs() 
{    
    
    if (fEndTimeUnixSecs == 0) // unbounded time
        return (UInt32) ~0; // max time
    
    time_t timeNow = OS::UnixTime_Secs(); //timeNow定义
    if (fEndTimeUnixSecs <= timeNow) // the active time has past or duration is 0 so return the minimum duration
        return (UInt32) 0; 
            
    if (fStartTimeUnixSecs == 0) // relative duration = from "now" to end time
        return fEndTimeUnixSecs - timeNow;
    
    return fEndTimeUnixSecs - fStartTimeUnixSecs; // this must be a duration because of test for endtime above

}

Bool16 SourceInfo::Equal(SourceInfo* inInfo)
{
    // Check to make sure the # of streams matches up
    if (this->GetNumStreams() != inInfo->GetNumStreams()) //inInfo是形参
        return false;
    
    // Check the src & dest addr, and port of each stream. 
    for (UInt32 x = 0; x < this->GetNumStreams(); x++)
    {
        if (GetStreamInfo(x)->fDestIPAddr != inInfo->GetStreamInfo(x)->fDestIPAddr)
            return false;
        if (GetStreamInfo(x)->fSrcIPAddr != inInfo->GetStreamInfo(x)->fSrcIPAddr)
            return false;
        
        // If either one of the comparators is 0 (the "wildcard" port), then we know at this point
        // they are equivalent
        if ((GetStreamInfo(x)->fPort == 0) || (inInfo->GetStreamInfo(x)->fPort == 0))
            return true;
            
        // Neither one is the wildcard port, so they must be the same
        if (GetStreamInfo(x)->fPort != inInfo->GetStreamInfo(x)->fPort)
            return false;
    }
    return true;
}

void SourceInfo::StreamInfo::Copy(const StreamInfo& copy) //定义Copy是SourceInfo类中子类StreamInfo复制构造函数并初始化，copy是形参
{
    fSrcIPAddr = copy.fSrcIPAddr;
    fDestIPAddr = copy.fDestIPAddr;
    fPort = copy.fPort;
    fTimeToLive = copy.fTimeToLive;
    fPayloadType = copy.fPayloadType;
    if ((copy.fPayloadName).Ptr != NULL)
        fPayloadName.Set((copy.fPayloadName).GetAsCString(), (copy.fPayloadName).Len);
    fTrackID = copy.fTrackID;
    fBufferDelay = copy.fBufferDelay;
    fIsTCP = copy.fIsTCP;
    fSetupToReceive = copy.fSetupToReceive;
    fTimeScale = copy.fTimeScale;    
}

SourceInfo::StreamInfo::~StreamInfo()//SourceInfo类中子类StreamInfo的析构函数
{
    if (fPayloadName.Ptr != NULL)
        delete fPayloadName.Ptr;
    fPayloadName.Len = 0;
}

void SourceInfo::OutputInfo::Copy(const OutputInfo& copy)//SourceInfo类中子类OutputInfo的复制构造函数并初始化，copy是形参
{
    fDestAddr = copy.fDestAddr;
    fLocalAddr = copy.fLocalAddr;
    fTimeToLive = copy.fTimeToLive;
    fNumPorts = copy.fNumPorts;
    if(fNumPorts != 0)
    {
        fPortArray = NEW UInt16[fNumPorts]; //创建fPortArray数组
        ::memcpy(fPortArray, copy.fPortArray, fNumPorts * sizeof(UInt16));
    }
    fBasePort = copy.fBasePort;
    fAlreadySetup = copy.fAlreadySetup;
}

SourceInfo::OutputInfo::~OutputInfo()//子类OutputInfo的析构函数
{
    if (fPortArray != NULL)
        delete [] fPortArray;
}

Bool16 SourceInfo::OutputInfo::Equal(const OutputInfo& info)
{
    if ((fDestAddr == info.fDestAddr) && (fLocalAddr == info.fLocalAddr) && (fTimeToLive == info.fTimeToLive))
    {
        if ((fBasePort != 0) && (fBasePort == info.fBasePort))
            return true;
        else if ((fNumPorts == 0) || ((fNumPorts == info.fNumPorts) && (fPortArray[0] == info.fPortArray[0])))
            return true;
    }
    return false;
}


