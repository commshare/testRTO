
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


#ifndef _OSREF_H_
#define _OSREF_H_


#include "StrPtrLen.h"
#include "OSHashTable.h"  /* 哈希表务必弄清楚,它对应这里的类OSRefTable */
#include "OSCond.h" /* 主要是临界区 */

class OSRef;
class OSRefKey;
class OSRefTable;

class OSRefTableUtils
{
    private:

		/* 将一个通常的字符串变成Hash字符串的算法 */
        static UInt32   HashString(StrPtrLen* inString);    

		/* 下面这两个类要引用这个函数 */
        friend class OSRef;
        friend class OSRefKey;
};

class OSRef
{
    public:

		//
		//constructor/destructor
        OSRef() :   fObjectP(NULL), fRefCount(0), fNextHashEntry(NULL)
            {
#if DEBUG
                fInATable = false;
                fSwapCalled = false;
#endif          
            }
        OSRef(const StrPtrLen &inString, void* inObjectP)
                                : fRefCount(0), fNextHashEntry(NULL)
                                    {   Set(inString, inObjectP); }
        ~OSRef() {}
        
		/* separated from destructor */
        void Set(const StrPtrLen& inString, void* inObjectP)
            { 
#if DEBUG
                fInATable = false;
                fSwapCalled = false;
#endif          
                fString = inString; fObjectP = inObjectP;
				/* 获取第一入参的哈希值 */
                fHashValue = OSRefTableUtils::HashString(&fString);
            }
        
#if DEBUG
        Bool16  IsInTable()     { return fInATable; }
#endif

		//
		//accessor
        void**  GetObjectPtr()  { return &fObjectP; }
        void*   GetObject()     { return fObjectP; }
        UInt32  GetRefCount()   { return fRefCount; }
        StrPtrLen *GetString()  { return &fString; }

    private:
        
        //value
		/* 包含该OSRef对象作用数据成员的外表类的指针,参见EventContext::SnarfEventContext() */
        void*   fObjectP;
        //key string
		/* 键值的ID字符串 */
        StrPtrLen   fString;
        
        //refcounting
        UInt32  fRefCount;
#if DEBUG
        Bool16  fInATable;
        Bool16  fSwapCalled;
#endif
		/* 临界区对象 */
        OSCond  fCond;//to block threads waiting for this ref.
        
        UInt32              fHashValue;
        OSRef*              fNextHashEntry;
        
        friend class OSRefKey;
        friend class OSHashTable<OSRef, OSRefKey>;
        friend class OSHashTableIter<OSRef, OSRefKey>;
        friend class OSRefTable;

};


class OSRefKey
{
public:

    //CONSTRUCTOR / DESTRUCTOR:
	/* 获取字符串和相应的Hash字符串 */
    OSRefKey(StrPtrLen* inStringP)
        :   fStringP(inStringP)
         { fHashValue = OSRefTableUtils::HashString(inStringP);/* 获取Hash字符串 */ }
            
    ~OSRefKey() {}
    
    
    //ACCESSORS:
    StrPtrLen*  GetString()         { return fStringP; }
    
    
private:

    //PRIVATE ACCESSORS:

	/* 获取hash键值(Hash字符串) */
    SInt32      GetHashKey()        { return fHashValue; }

    //these functions are only used by the hash table itself. This constructor
    //will break the "Set" functions.
	/* 下面这两个函数只被Hash Table用 */
    OSRefKey(OSRef *elem) : fStringP(&elem->fString),
                            fHashValue(elem->fHashValue) {}
    
	/* 使用常引用定义友类重置算子"==": 比较两个键值是否相等? */
    friend int operator ==(const OSRefKey &key1, const OSRefKey &key2)
    {
		/* 比较两个字符串是否相等 */
        if (key1.fStringP->Equal(*key2.fStringP))
            return true;
        return false;
    }
    
    //data members:
	/*  普通字符串 */
    StrPtrLen *fStringP;

	/* Hash字符串 */
    UInt32  fHashValue;

    friend class OSHashTable<OSRef, OSRefKey>;
};

/* 简化类型定义,这两个类的定义参见OSHashTable.h */
typedef OSHashTable<OSRef, OSRefKey> OSRefHashTable;
typedef OSHashTableIter<OSRef, OSRefKey> OSRefHashTableIter;

class OSRefTable
{
    public:
    
        enum
        {
			/* 默认的Hash Table大小 */
            kDefaultTableSize = 1193 //UInt32
        };
    
        //tableSize doesn't indicate the max number of Refs that can be added
        //(it's unlimited), but is rather just how big to make the hash table
        OSRefTable(UInt32 tableSize = kDefaultTableSize) : fTable(tableSize), fMutex() {}
        ~OSRefTable() {}

        //
		//accessor

        //Allows access to the mutex in case you need to lock the table down
        //between operations
        OSMutex*    GetMutex()      { return &fMutex; }
        OSRefHashTable* GetHashTable() { return &fTable; }
        
        //Registers a Ref in the table. Once the Ref is in, clients may resolve(解析)
        //the ref by using its string ID. You must setup the Ref before passing it
        //in here, ie., setup the string and object pointers(先构建字符串和类对象指针,见OSRef)
        //This function will succeed unless the string identifier is not unique,
        //in which case it will return QTSS_DupName
        //This function is atomic wrt this ref table.
		/* 在Hash表中注册一个引用,若字符串标识唯一,就能成功返回OS_NoErr */
        OS_Error        Register(OSRef* ref);
        
        // RegisterOrResolve
        // If the ID of the input ref is unique, this function is equivalent to
        // Register, and returns NULL.
        // If there is a duplicate ID already in the map, this funcion
        // leave it, resolves it, and returns it.（离开它,解析它,返回它）
        OSRef*              RegisterOrResolve(OSRef* inRef);
        
        //This function may block(可能阻塞). You can only remove a Ref from the table
        //when the refCount drops to the level specified(refCount指定). If several threads have
        //the ref currently, the calling thread will wait until the other threads
        //stop using the ref (by calling Release, below)
        //This function is atomic wrt this ref table.
        void        UnRegister(OSRef* ref, UInt32 refCount = 0);
        
        // Same as UnRegister, but guarenteed not to block. Will return
        // true if ref was sucessfully unregistered, false otherwise
		/* 非阻塞版本的UnRegister() */
        Bool16      TryUnRegister(OSRef* ref, UInt32 refCount = 0);
        
        //Resolve. This function uses the provided key string to identify and grab(识别和取出)
        //the Ref keyed by that string. Once the Ref is resolved, it is safe to use
        //(it cannot be removed from the Ref table) until you call Release. Because
        //of that, you MUST call release in a timely manner, and be aware of potential
        //deadlocks(可能的死锁) because you now own a resource being contended over.
        //This function is atomic wrt this ref table.
		/* 通过指定的键值字符串去识别和取得哈希表中的(哈希表元)引用 */
		/* 注意:这个函数是本类OSRefTable中最重要的函数! */
        OSRef*      Resolve(StrPtrLen*  inString);
        
        //Release. Release a Ref, and drops its refCount. After calling this, the
        //Ref is no longer safe to use, as it may be removed from the ref table.
		/* 减少引用计数,通知其他等待的线程 */
        void        Release(OSRef*  inRef);
        
        // Swap. This atomically removes any existing Ref in the table with the new
        // ref's ID, and replaces it with this new Ref. If there is no matching Ref
        // already in the table, this function does nothing.
        //
        // Be aware that this creates a situation where clients may have a Ref resolved
        // that is no longer in the table. The old Ref must STILL be UnRegistered normally.
        // Once Swap completes sucessfully, clients that call resolve on the ID will get
        // the new OSRef object.
		/* 对给定的键值,移去Hash表中原有的同Key值的Ref,替换上新的Ref */
        void        Swap(OSRef* newRef);
        
		/* 得到Hash表中元素个数 */
        UInt32      GetNumRefsInTable() { UInt64 result =  fTable.GetNumEntries(); Assert(result < kUInt32_Max); return (UInt32) result; }
        
    private:
    
        
        //all this object needs to do its job is an atomic hashtable
		/* 利用OSHashTable.h中定义的Hash Table */
        OSRefHashTable  fTable;
        OSMutex         fMutex;
};


class OSRefReleaser
{
    public:

        OSRefReleaser(OSRefTable* inTable, OSRef* inRef) : fOSRefTable(inTable), fOSRef(inRef) {}
		/* 释放该fOSRef数据成员 */
        ~OSRefReleaser() { fOSRefTable->Release(fOSRef); }
        
		/* 获得fOSRef数据成员 */
        OSRef*          GetRef() { return fOSRef; }
        
    private:

        OSRefTable*     fOSRefTable;
        OSRef*          fOSRef;
};



#endif //_OSREF_H_
