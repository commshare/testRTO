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



/* ��ָ����address/port��ϴ�����ָ����Hash Table entry,����OS_NoErr;����address/port�����Hash Table���ѱ�ռ��,����EPERM */
OS_Error UDPDemuxer::RegisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort, UDPDemuxerTask *inTaskP)
{
	/* ȷ����Hash Table entry���� */
    Assert(NULL != inTaskP);
    OSMutexLocker locker(&fMutex);
	/* ���ɸ�����keyֵȥ��ȡ��Ӧ��Hash Table elementʱ�ɹ���,��ʾ��address/port�����Hash Table���ѱ�ռ�� */
    if (this->GetTask(inRemoteAddr, inRemotePort) != NULL)
        return EPERM;
	/* �������ø�Hash Table entry(ע����ԭ�������ݿ����ѱ��ı�) */
    inTaskP->Set(inRemoteAddr, inRemotePort);
	/* ���½���Hash Table entry����Hash Table�� */
    fHashTable.Add(inTaskP);
    return OS_NoErr;
}

/* ��ָ����address/port��ϴ�����Hash Table entry,���������inTaskP�Ƚ�,����ͬ,�ʹ�Hash Table��ɾȥ��
   Hash Table entry,����OS_NoErr;���򷵻�EPERM */
OS_Error UDPDemuxer::UnregisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort, UDPDemuxerTask *inTaskP)
{
    OSMutexLocker locker(&fMutex);
    //remove by executing a lookup based on key information
	/* ��ȡHash Table�и�ָ����ֵ��Ӧ��Hash Table entry */
    UDPDemuxerTask* theTask = this->GetTask(inRemoteAddr, inRemotePort);

    if ((NULL != theTask) && (theTask == inTaskP))
    {
        fHashTable.Remove(theTask);
        return OS_NoErr;
    }
    else
        return EPERM;
}

/* ע��ú����������RegisterTask/UnregisterTask()��Ҫʹ�� */
/* ��ָ����keyֵ��Hash Table�л�ȡ�����ض�Ӧ��Hash TableԪ */
UDPDemuxerTask* UDPDemuxer::GetTask(UInt32 inRemoteAddr, UInt16 inRemotePort)
{
    UDPDemuxerKey theKey(inRemoteAddr, inRemotePort);
	/* ����map��ȡHash TableԪ */
    return fHashTable.Map(&theKey);
}
