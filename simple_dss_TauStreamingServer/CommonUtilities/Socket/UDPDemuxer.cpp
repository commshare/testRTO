/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPDemuxer.h
Description: Provides a "Listener" socket for UDP. Blocks on a local IP & port,
waiting for data. When it gets data, it passes it off to a UDPDemuxerTask(RTPStream)
object depending on where it came from.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#include "UDPDemuxer.h"
#include <errno.h>



/* 在指定的address/port组合处加入指定的Hash Table entry,返回OS_NoErr;若该address/port组合在Hash Table中已被占用,返回EPERM */
OS_Error UDPDemuxer::RegisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort, UDPDemuxerTask *inTaskP)
{
	/* 确保该Hash Table entry存在 */
    Assert(NULL != inTaskP);
    OSMutexLocker locker(&fMutex);
	/* 若由给定的key值去获取相应的Hash Table element时成功了,表示该address/port组合在Hash Table中已被占用 */
    if (this->GetTask(inRemoteAddr, inRemotePort) != NULL)
        return EPERM;
	/* 否则设置该Hash Table entry(注意它原来的数据可能已被改变) */
    inTaskP->Set(inRemoteAddr, inRemotePort);
	/* 将新建的Hash Table entry加入Hash Table中 */
    fHashTable.Add(inTaskP);
    return OS_NoErr;
}

/* 在指定的address/port组合处查找Hash Table entry,将其与入参inTaskP比较,若相同,就从Hash Table中删去该
   Hash Table entry,返回OS_NoErr;否则返回EPERM */
OS_Error UDPDemuxer::UnregisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort, UDPDemuxerTask *inTaskP)
{
    OSMutexLocker locker(&fMutex);
    //remove by executing a lookup based on key information
	/* 获取Hash Table中该指定键值对应的Hash Table entry */
    UDPDemuxerTask* theTask = this->GetTask(inRemoteAddr, inRemotePort);

    if ((NULL != theTask) && (theTask == inTaskP))
    {
        fHashTable.Remove(theTask);
        return OS_NoErr;
    }
    else
        return EPERM;
}

/* 注意该函数在上面的RegisterTask/UnregisterTask()都要使用 */
/* 由指定的key值在Hash Table中获取并返回对应的Hash Table元 */
UDPDemuxerTask* UDPDemuxer::GetTask(UInt32 inRemoteAddr, UInt16 inRemotePort)
{
    UDPDemuxerKey theKey(inRemoteAddr, inRemotePort);
	/* 利用map获取Hash Table元 */
    return fHashTable.Map(&theKey);
}
