/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSRef.h
Description: Class supports creating unique string IDs to object pointers. A grouping
of an object and its string ID may be stored in an OSRefTable, and the
associated object pointer can be looked up by string ID.
Refs can only be removed from the table when no one is using the ref,
therefore allowing clients to arbitrate access to objects in a preemptive,
multithreaded environment.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include "OSRef.h"
#include <errno.h>


/* ��һ��ͨ�����ַ������Hash�ַ������㷨 */
UInt32  OSRefTableUtils::HashString(StrPtrLen* inString)
{
	/* ȷ����ηǿ� */
    Assert(inString != NULL);
    Assert(inString->Ptr != NULL);
    Assert(inString->Len > 0);
    
    //make sure to convert to unsigned here, as there may be binary
    //data in this string
	/* ȷ��ת�����ַ���UInt8������ */
    UInt8* theData = (UInt8*)inString->Ptr;
    
    //divide by 4 and take the characters at quarter points in the string,
    //use those as the basis for the hash value
	/* ʹ���ȱ�Ϊԭ����1/4 */
    UInt32 quarterLen = inString->Len >> 2;
	/* ����*��������ַ�ֵ(�ĵȷֺ�������)�ĺ� */
    return (inString->Len * (theData[0] + theData[quarterLen] +
            theData[quarterLen * 2] + theData[quarterLen * 3] +
            theData[inString->Len - 1]));
}

/* ��Hash����ע�Ტ����һ������OSRef,���ַ�����ʶΨһ,���ܳɹ�����OS_NoErr;����һ����ͬkeyֵ��Ԫ��,�ͷ��ش���EPERM  */
OS_Error OSRefTable::Register(OSRef* inRef)
{
    Assert(inRef != NULL);
#if DEBUG
    Assert(!inRef->fInATable);
#endif
	/* ȷ��û�б������������� */
    Assert(inRef->fRefCount == 0);
    
    OSMutexLocker locker(&fMutex);

    // Check for a duplicate. In this function, if there is a duplicate,
    // return an error, don't resolve the duplicate
	/* ��һ�㹹�캯����ʼ������,�õ������ַ�����Hash�ַ��� */
    OSRefKey key(&inRef->fString);
	/* �õ�ָ����ֵ�Ĺ�ϣ��Ԫ */
    OSRef* duplicateRef = fTable.Map(&key);
	/* �������һ����ͬkeyֵ��Ԫ��,�ͷ��ش���EPERM */
    if (duplicateRef != NULL)
        return EPERM;
        
    // There is no duplicate, so add this ref into the table
#if DEBUG
    inRef->fInATable = true;
#endif
	/* ��û��duplicate,�����üӵ���ǰ��Hash���� */
    fTable.Add(inRef);
    return OS_NoErr;
}


/* ��Ref ID���ַ���Ψһ,���ͬ��Register(),�������Hash����,����NULL;����,�ͽ���resolve��,����������ָ�� */
OSRef* OSRefTable::RegisterOrResolve(OSRef* inRef)
{
    Assert(inRef != NULL);
#if DEBUG
    Assert(!inRef->fInATable);
#endif
    Assert(inRef->fRefCount == 0);
    
    OSMutexLocker locker(&fMutex);

    // Check for a duplicate. If there is one, resolve it and return it to the caller
    OSRef* duplicateRef = this->Resolve(&inRef->fString);
    if (duplicateRef != NULL)
        return duplicateRef;

    // There is no duplicate, so add this ref into the table
#if DEBUG
    inRef->fInATable = true;
#endif
    fTable.Add(inRef);
    return NULL;
}

/* ע��ڶ���������Ĭ��ֵ��0,���������߳���ʹ�ø�refʱ,��ֻ�еȴ�;��û�������߳�ʹ�ø�Hash��Ԫʱ��Hash����ɾȥ�� */
void OSRefTable::UnRegister(OSRef* ref, UInt32 refCount)
{
    Assert(ref != NULL);
    OSMutexLocker locker(&fMutex);

    //make sure that no one else is using the object
	/* ���������߳���ʹ�ø�refʱ,��ֻ�еȴ� */
    while (ref->fRefCount > refCount)
        ref->fCond.Wait(&fMutex);
    
#if DEBUG
    OSRefKey key(&ref->fString);
    if (ref->fInATable)
        Assert(fTable.Map(&key) != NULL);
    ref->fInATable = false;
#endif
    
    //ok, we now definitely have no one else using this object, so
    //remove it from the table
	/* ��Hash����ɾȥ��(Hash��Ԫ)ref */
    fTable.Remove(ref);
}

/* �������汾��UnRegister() */
Bool16 OSRefTable::TryUnRegister(OSRef* ref, UInt32 refCount)
{
    OSMutexLocker locker(&fMutex);
    if (ref->fRefCount > refCount)
        return false;
    
    // At this point, this is guarenteed not to block, because
    // we've already checked that the refCount is low.
    this->UnRegister(ref, refCount);
    return true;
}


/* ע��:��������Ǳ���OSRefTable������Ҫ�ĺ���! */
/* ͨ��ָ���ļ�ֵ�ַ���ȥʶ���ȡ�ù�ϣ���е�(��ϣ��Ԫ)���� */
OSRef*  OSRefTable::Resolve(StrPtrLen* inUniqueID)
{
    Assert(inUniqueID != NULL);
	/* ��һ�㹹�캯����ʼ������,�õ�����Ψһ��ֵ�ַ�����Hash�ַ��� */
    OSRefKey key(inUniqueID);

    //this must be done atomically wrt the table
    OSMutexLocker locker(&fMutex);
	/* �õ�ָ����ֵ�Ĺ�ϣ��Ԫ */
    OSRef* ref = fTable.Map(&key);
	/* ��Ӧ�������ü��� */
    if (ref != NULL)
    {
        ref->fRefCount++;
        Assert(ref->fRefCount > 0);
    }
	/* ����Hash��Ԫ */
    return ref;
}

/* �������ü���,֪ͨ�����ȴ����߳� */
void    OSRefTable::Release(OSRef* ref)
{
    Assert(ref != NULL);
    OSMutexLocker locker(&fMutex);
    ref->fRefCount--;
    // fRefCount is an unsigned long and QTSS should never run into
    // a ref greater than 16 * 64K, so this assert just checks to
    // be sure that we have not decremented the ref less than zero.
    Assert( ref->fRefCount < 1048576L );
    //make sure to wakeup anyone who may be waiting for this resource to be released
	/* ֪ͨ�����ȴ����߳� */
    ref->fCond.Signal();
}

/* �Ը����ļ�ֵ,��ȥHash����ԭ�е�ͬKeyֵ��Ref,�滻���µ�Ref */
void    OSRefTable::Swap(OSRef* newRef)
{
    Assert(newRef != NULL);
    OSMutexLocker locker(&fMutex);
    
    OSRefKey key(&newRef->fString);
	/* ��ȡָ����ֵ��Hash���е�ԭref */
    OSRef* oldRef = fTable.Map(&key);
    if (oldRef != NULL)
    {
		/* ��ȥ�ɵ�,�����µ�Ref,���ǵļ�ֵ��ͬ */
        fTable.Remove(oldRef);
        fTable.Add(newRef);
#if DEBUG
        newRef->fInATable = true;
        oldRef->fInATable = false;
        oldRef->fSwapCalled = true;
#endif
    }
    else
        Assert(0);
}

