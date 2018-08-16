
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMemory.h
Description: Prototypes for overridden new & delete, definition of OSMemory
             class which implements some memory leak debugging features.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef __OS_MEMORY_H__
#define __OS_MEMORY_H__

#include "OSHeaders.h"
#include "OSQueue.h" /* ���������Ķ����� */
#include "OSMutex.h"

class OSMemory
{
    public:
    
#if MEMORY_DEBUGGING
        //If memory debugging is on, clients can get access to data structures that give
        //memory status.
		/* �õ�index���Ķ��� */
        static OSQueue* GetTagQueue() { return &sTagQueue; }
		/* �õ�index�����еĻ����� */
        static OSMutex* GetTagQueueMutex() { return &sMutex;    }
		/* �õ���������ֽ� */
        static UInt32   GetAllocatedMemory() { return sAllocatedBytes; }

		/*************** ����2��������OSMemory���Ա��������ʱ������,ʮ����Ҫ *********************/
		/* new/delete�ĵ��԰汾 */
        static void*    DebugNew(size_t size, char* inFile, int inLine, Bool16 sizeCheck);
        static void     DebugDelete(void *mem);
		/*************** ����2��������OSMemory���Ա��������ʱ������ *********************/

		/* �����ڴ���Եĺ��� */
        static Bool16       MemoryDebuggingTest();
        

		/* ��DebugNew()��Ҫ���� */
		/* �ж��ڴ�����Ƿ�Ϸ�?��ͷ��β����sMemoryQueue��������е�����Ԫ��,���ÿ������Ԫ�����Ļ��浥Ԫ��βλ�ô�ŵ��к��Ƿ�һ��? */
        static void     ValidateMemoryQueue();

        enum
        {
            kMaxFileNameSize = 48
        };
        
		/* ��¼һ���������Ļ��浥Ԫ�������Ϣ */
        struct TagElem
        {
            OSQueueElem elem;/* ��Ϊһ��������еĵ�Ԫ */
            char fileName[kMaxFileNameSize];//������ļ���
            int     line;//�к� ?? used in  OSMemory::ValidateMemoryQueue()
            UInt32 tagSize; //how big are objects of this type?  ʵ�ʷ�����ֽ���,��MemoryDebugging.size��ͬ
            UInt32 totMemory; //how much do they currently occupy �ܹ�ռ�ö����ֽ��ڴ�?
            UInt32 numObjects;//how many are there currently?��ǰ��������
        };
#endif

        // Provides non-debugging behaviour for new and delete
		/* new/delete�ķǵ��԰汾 */
        static void*    New(size_t inSize);
        static void     Delete(void* inMemory);
        
        //When memory allocation fails, the server just exits. This sets the code
        //the server exits with
		/* �������ڴ����ʧ��,Server�˳��󷵻صĴ������ */
        static void SetMemoryError(SInt32 inErr);
        
#if MEMORY_DEBUGGING
    private:
          
		/* �ڴ���Խṹ��,Ҫ���������TagElem�ṹ�� */
        struct MemoryDebugging
        {
            OSQueueElem elem; /* ����Ԫ */
            TagElem* tagElem; /* ָ����һ������sTagQueue��Ԫ��TagElem */
            UInt32 size; /* �ڴ��С */
        };

		/*************** ע����������������!! ****************/

		/* �ڴ���� */
        static OSQueue sMemoryQueue;
		/* ��������TagElem���� */
        static OSQueue sTagQueue;
		/* �ѷ�����ֽ����� */
        static UInt32  sAllocatedBytes;
		/* �ڴ滥���� */
        static OSMutex sMutex;
        
#endif
};


// NEW MACRO
// When memory debugging is on, this macro transparently uses the memory debugging
// overridden version of the new operator. When memory debugging is off, it just compiles
// down to the standard new.

/* �����ǶԺ�NEW�Ķ���:������ʱ����new (__FILE__, __LINE__),�������һ���new */
#if MEMORY_DEBUGGING

#ifdef  NEW
#error Conflicting Macro "NEW"
#endif

#define NEW new (__FILE__, __LINE__) /* ע��new ()�Ķ��������Ķ��� */

#else

#ifdef  NEW
#error Conflicting Macro "NEW"
#endif

#define NEW new  //��Cplusplus�е�new�������ΪNEW

#endif //MEMORY_DEBUGGING


// 
// PLACEMENT NEW OPERATOR
/* ��NEW��ȡ��,����new�����¶���,����c++�е�new */

/* ע�����������inline��,���ر� */
inline void* operator new(size_t, void* ptr) { return ptr;}

#if MEMORY_DEBUGGING

// These versions of the new operator with extra arguments provide memory debugging
// features.

/* ������������ʵ���õ�OSMemory::DebugNew() */
void* operator new(size_t s, char* inFile, int inLine);
void* operator new[](size_t s, char* inFile, int inLine);

#endif //MEMORY_DEBUGGING

// When memory debugging is not on, these are overridden so that if new fails,
// the process will exit.

/* �������������õ�OSMemory::New() */
void* operator new (size_t s);
void* operator new[](size_t s);

/* �������������õ�OSMemory::Delete() */
void operator delete(void* mem);
void operator delete[](void* mem);


#endif //__OS_MEMORY_H__
