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


/* 将一个通常的字符串变成Hash字符串的算法 */
UInt32  OSRefTableUtils::HashString(StrPtrLen* inString)
{
	/* 确保入参非空 */
    Assert(inString != NULL);
    Assert(inString->Ptr != NULL);
    Assert(inString->Len > 0);
    
    //make sure to convert to unsigned here, as there may be binary
    //data in this string
	/* 确保转换成字符型UInt8的数据 */
    UInt8* theData = (UInt8*)inString->Ptr;
    
    //divide by 4 and take the characters at quarter points in the string,
    //use those as the basis for the hash value
	/* 使长度变为原来的1/4 */
    UInt32 quarterLen = inString->Len >> 2;
	/* 长度*五个特殊字符值(四等分后成五个点)的和 */
    return (inString->Len * (theData[0] + theData[quarterLen] +
            theData[quarterLen * 2] + theData[quarterLen * 3] +
            theData[inString->Len - 1]));
}

/* 在Hash表中注册并加入一个引用OSRef,若字符串标识唯一,就能成功返回OS_NoErr;若有一个相同key值的元素,就返回错误EPERM  */
OS_Error OSRefTable::Register(OSRef* inRef)
{
    Assert(inRef != NULL);
#if DEBUG
    Assert(!inRef->fInATable);
#endif
	/* 确保没有被其它对象引用 */
    Assert(inRef->fRefCount == 0);
    
    OSMutexLocker locker(&fMutex);

    // Check for a duplicate. In this function, if there is a duplicate,
    // return an error, don't resolve the duplicate
	/* 用一般构造函数初始化对象,得到它的字符串和Hash字符串 */
    OSRefKey key(&inRef->fString);
	/* 得到指定键值的哈希表元 */
    OSRef* duplicateRef = fTable.Map(&key);
	/* 检查若有一个相同key值的元素,就返回错误EPERM */
    if (duplicateRef != NULL)
        return EPERM;
        
    // There is no duplicate, so add this ref into the table
#if DEBUG
    inRef->fInATable = true;
#endif
	/* 若没有duplicate,将引用加到当前的Hash表中 */
    fTable.Add(inRef);
    return OS_NoErr;
}


/* 当Ref ID的字符串唯一,则等同于Register(),将其加入Hash表列,返回NULL;否则,就解析resolve它,并返回它的指针 */
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

/* 注意第二个参数的默认值是0,当有其它线程在使用该ref时,就只有等待;当没有其它线程使用该Hash表元时从Hash表中删去它 */
void OSRefTable::UnRegister(OSRef* ref, UInt32 refCount)
{
    Assert(ref != NULL);
    OSMutexLocker locker(&fMutex);

    //make sure that no one else is using the object
	/* 当有其它线程在使用该ref时,就只有等待 */
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
	/* 从Hash表中删去该(Hash表元)ref */
    fTable.Remove(ref);
}

/* 非阻塞版本的UnRegister() */
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


/* 注意:这个函数是本类OSRefTable中最重要的函数! */
/* 通过指定的键值字符串去识别和取得哈希表中的(哈希表元)引用 */
OSRef*  OSRefTable::Resolve(StrPtrLen* inUniqueID)
{
    Assert(inUniqueID != NULL);
	/* 用一般构造函数初始化对象,得到它的唯一键值字符串和Hash字符串 */
    OSRefKey key(inUniqueID);

    //this must be done atomically wrt the table
    OSMutexLocker locker(&fMutex);
	/* 得到指定键值的哈希表元 */
    OSRef* ref = fTable.Map(&key);
	/* 相应增加引用计数 */
    if (ref != NULL)
    {
        ref->fRefCount++;
        Assert(ref->fRefCount > 0);
    }
	/* 返回Hash表元 */
    return ref;
}

/* 减少引用计数,通知其他等待的线程 */
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
	/* 通知其它等待的线程 */
    ref->fCond.Signal();
}

/* 对给定的键值,移去Hash表中原有的同Key值的Ref,替换上新的Ref */
void    OSRefTable::Swap(OSRef* newRef)
{
    Assert(newRef != NULL);
    OSMutexLocker locker(&fMutex);
    
    OSRefKey key(&newRef->fString);
	/* 获取指定键值的Hash表中的原ref */
    OSRef* oldRef = fTable.Map(&key);
    if (oldRef != NULL)
    {
		/* 移去旧的,加入新的Ref,它们的键值相同 */
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

