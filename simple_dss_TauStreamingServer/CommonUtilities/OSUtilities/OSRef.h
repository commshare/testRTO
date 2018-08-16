
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
#include "OSHashTable.h"  /* ��ϣ�����Ū���,����Ӧ�������OSRefTable */
#include "OSCond.h" /* ��Ҫ���ٽ��� */

class OSRef;
class OSRefKey;
class OSRefTable;

class OSRefTableUtils
{
    private:

		/* ��һ��ͨ�����ַ������Hash�ַ������㷨 */
        static UInt32   HashString(StrPtrLen* inString);    

		/* ������������Ҫ����������� */
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
				/* ��ȡ��һ��εĹ�ϣֵ */
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
		/* ������OSRef�����������ݳ�Ա��������ָ��,�μ�EventContext::SnarfEventContext() */
        void*   fObjectP;
        //key string
		/* ��ֵ��ID�ַ��� */
        StrPtrLen   fString;
        
        //refcounting
        UInt32  fRefCount;
#if DEBUG
        Bool16  fInATable;
        Bool16  fSwapCalled;
#endif
		/* �ٽ������� */
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
	/* ��ȡ�ַ�������Ӧ��Hash�ַ��� */
    OSRefKey(StrPtrLen* inStringP)
        :   fStringP(inStringP)
         { fHashValue = OSRefTableUtils::HashString(inStringP);/* ��ȡHash�ַ��� */ }
            
    ~OSRefKey() {}
    
    
    //ACCESSORS:
    StrPtrLen*  GetString()         { return fStringP; }
    
    
private:

    //PRIVATE ACCESSORS:

	/* ��ȡhash��ֵ(Hash�ַ���) */
    SInt32      GetHashKey()        { return fHashValue; }

    //these functions are only used by the hash table itself. This constructor
    //will break the "Set" functions.
	/* ��������������ֻ��Hash Table�� */
    OSRefKey(OSRef *elem) : fStringP(&elem->fString),
                            fHashValue(elem->fHashValue) {}
    
	/* ʹ�ó����ö���������������"==": �Ƚ�������ֵ�Ƿ����? */
    friend int operator ==(const OSRefKey &key1, const OSRefKey &key2)
    {
		/* �Ƚ������ַ����Ƿ���� */
        if (key1.fStringP->Equal(*key2.fStringP))
            return true;
        return false;
    }
    
    //data members:
	/*  ��ͨ�ַ��� */
    StrPtrLen *fStringP;

	/* Hash�ַ��� */
    UInt32  fHashValue;

    friend class OSHashTable<OSRef, OSRefKey>;
};

/* �����Ͷ���,��������Ķ���μ�OSHashTable.h */
typedef OSHashTable<OSRef, OSRefKey> OSRefHashTable;
typedef OSHashTableIter<OSRef, OSRefKey> OSRefHashTableIter;

class OSRefTable
{
    public:
    
        enum
        {
			/* Ĭ�ϵ�Hash Table��С */
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
        
        //Registers a Ref in the table. Once the Ref is in, clients may resolve(����)
        //the ref by using its string ID. You must setup the Ref before passing it
        //in here, ie., setup the string and object pointers(�ȹ����ַ����������ָ��,��OSRef)
        //This function will succeed unless the string identifier is not unique,
        //in which case it will return QTSS_DupName
        //This function is atomic wrt this ref table.
		/* ��Hash����ע��һ������,���ַ�����ʶΨһ,���ܳɹ�����OS_NoErr */
        OS_Error        Register(OSRef* ref);
        
        // RegisterOrResolve
        // If the ID of the input ref is unique, this function is equivalent to
        // Register, and returns NULL.
        // If there is a duplicate ID already in the map, this funcion
        // leave it, resolves it, and returns it.���뿪��,������,��������
        OSRef*              RegisterOrResolve(OSRef* inRef);
        
        //This function may block(��������). You can only remove a Ref from the table
        //when the refCount drops to the level specified(refCountָ��). If several threads have
        //the ref currently, the calling thread will wait until the other threads
        //stop using the ref (by calling Release, below)
        //This function is atomic wrt this ref table.
        void        UnRegister(OSRef* ref, UInt32 refCount = 0);
        
        // Same as UnRegister, but guarenteed not to block. Will return
        // true if ref was sucessfully unregistered, false otherwise
		/* �������汾��UnRegister() */
        Bool16      TryUnRegister(OSRef* ref, UInt32 refCount = 0);
        
        //Resolve. This function uses the provided key string to identify and grab(ʶ���ȡ��)
        //the Ref keyed by that string. Once the Ref is resolved, it is safe to use
        //(it cannot be removed from the Ref table) until you call Release. Because
        //of that, you MUST call release in a timely manner, and be aware of potential
        //deadlocks(���ܵ�����) because you now own a resource being contended over.
        //This function is atomic wrt this ref table.
		/* ͨ��ָ���ļ�ֵ�ַ���ȥʶ���ȡ�ù�ϣ���е�(��ϣ��Ԫ)���� */
		/* ע��:��������Ǳ���OSRefTable������Ҫ�ĺ���! */
        OSRef*      Resolve(StrPtrLen*  inString);
        
        //Release. Release a Ref, and drops its refCount. After calling this, the
        //Ref is no longer safe to use, as it may be removed from the ref table.
		/* �������ü���,֪ͨ�����ȴ����߳� */
        void        Release(OSRef*  inRef);
        
        // Swap. This atomically removes any existing Ref in the table with the new
        // ref's ID, and replaces it with this new Ref. If there is no matching Ref
        // already in the table, this function does nothing.
        //
        // Be aware that this creates a situation where clients may have a Ref resolved
        // that is no longer in the table. The old Ref must STILL be UnRegistered normally.
        // Once Swap completes sucessfully, clients that call resolve on the ID will get
        // the new OSRef object.
		/* �Ը����ļ�ֵ,��ȥHash����ԭ�е�ͬKeyֵ��Ref,�滻���µ�Ref */
        void        Swap(OSRef* newRef);
        
		/* �õ�Hash����Ԫ�ظ��� */
        UInt32      GetNumRefsInTable() { UInt64 result =  fTable.GetNumEntries(); Assert(result < kUInt32_Max); return (UInt32) result; }
        
    private:
    
        
        //all this object needs to do its job is an atomic hashtable
		/* ����OSHashTable.h�ж����Hash Table */
        OSRefHashTable  fTable;
        OSMutex         fMutex;
};


class OSRefReleaser
{
    public:

        OSRefReleaser(OSRefTable* inTable, OSRef* inRef) : fOSRefTable(inTable), fOSRef(inRef) {}
		/* �ͷŸ�fOSRef���ݳ�Ա */
        ~OSRefReleaser() { fOSRefTable->Release(fOSRef); }
        
		/* ���fOSRef���ݳ�Ա */
        OSRef*          GetRef() { return fOSRef; }
        
    private:

        OSRefTable*     fOSRefTable;
        OSRef*          fOSRef;
};



#endif //_OSREF_H_
