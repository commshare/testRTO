/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 daemon.h
Description: Provide some utilities to implement the atomic operations.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include "atomic.h"
#include "OSMutex.h"

static OSMutex sAtomicMutex;


unsigned int atomic_add(unsigned int *area, int val)
{
    OSMutexLocker locker(&sAtomicMutex);
    *area += val;
    return *area;
}

unsigned int atomic_sub(unsigned int *area,int val)
{
    return atomic_add(area,-val);
}

unsigned int atomic_or(unsigned int *area, unsigned int val)
{
    unsigned int oldval;

    OSMutexLocker locker(&sAtomicMutex);
    oldval=*area;
    *area = oldval | val;
    return oldval;
}

unsigned int compare_and_store(unsigned int oval, unsigned int nval, unsigned int *area)
{
   int rv;
    OSMutexLocker locker(&sAtomicMutex);
    if( oval == *area )
    {
    rv=1;
    *area = nval;
    }
    else
    rv=0;
    return rv;
}
