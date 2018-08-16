
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
#include "OSQueue.h" /* 有序管理缓存的队列类 */
#include "OSMutex.h"

class OSMemory
{
    public:
    
#if MEMORY_DEBUGGING
        //If memory debugging is on, clients can get access to data structures that give
        //memory status.
		/* 得到index过的队列 */
        static OSQueue* GetTagQueue() { return &sTagQueue; }
		/* 得到index过队列的互斥锁 */
        static OSMutex* GetTagQueueMutex() { return &sMutex;    }
		/* 得到分配过的字节 */
        static UInt32   GetAllocatedMemory() { return sAllocatedBytes; }

		/*************** 以下2个函数在OSMemory类成员函数调用时会引用,十分重要 *********************/
		/* new/delete的调试版本 */
        static void*    DebugNew(size_t size, char* inFile, int inLine, Bool16 sizeCheck);
        static void     DebugDelete(void *mem);
		/*************** 以上2个函数在OSMemory类成员函数调用时会引用 *********************/

		/* 用于内存调试的函数 */
        static Bool16       MemoryDebuggingTest();
        

		/* 在DebugNew()中要引用 */
		/* 判断内存队列是否合法?从头至尾遍历sMemoryQueue缓存队列中的所有元素,检查每个队列元关联的缓存单元首尾位置存放的行号是否一致? */
        static void     ValidateMemoryQueue();

        enum
        {
            kMaxFileNameSize = 48
        };
        
		/* 记录一个索引过的缓存单元的相关信息 */
        struct TagElem
        {
            OSQueueElem elem;/* 作为一个缓存队列的单元 */
            char fileName[kMaxFileNameSize];//缓存的文件名
            int     line;//行号 ?? used in  OSMemory::ValidateMemoryQueue()
            UInt32 tagSize; //how big are objects of this type?  实际分配的字节数,与MemoryDebugging.size相同
            UInt32 totMemory; //how much do they currently occupy 总共占用多少字节内存?
            UInt32 numObjects;//how many are there currently?当前对象总数
        };
#endif

        // Provides non-debugging behaviour for new and delete
		/* new/delete的非调试版本 */
        static void*    New(size_t inSize);
        static void     Delete(void* inMemory);
        
        //When memory allocation fails, the server just exits. This sets the code
        //the server exits with
		/* 设置因内存分配失败,Server退出后返回的错误代码 */
        static void SetMemoryError(SInt32 inErr);
        
#if MEMORY_DEBUGGING
    private:
          
		/* 内存调试结构体,要引用上面的TagElem结构体 */
        struct MemoryDebugging
        {
            OSQueueElem elem; /* 队列元 */
            TagElem* tagElem; /* 指向另一个队列sTagQueue的元素TagElem */
            UInt32 size; /* 内存大小 */
        };

		/*************** 注意这里有两个队列!! ****************/

		/* 内存队列 */
        static OSQueue sMemoryQueue;
		/* 索引过的TagElem队列 */
        static OSQueue sTagQueue;
		/* 已分配的字节总数 */
        static UInt32  sAllocatedBytes;
		/* 内存互斥锁 */
        static OSMutex sMutex;
        
#endif
};


// NEW MACRO
// When memory debugging is on, this macro transparently uses the memory debugging
// overridden version of the new operator. When memory debugging is off, it just compiles
// down to the standard new.

/* 下面是对宏NEW的定义:当调试时就是new (__FILE__, __LINE__),否则就是一般的new */
#if MEMORY_DEBUGGING

#ifdef  NEW
#error Conflicting Macro "NEW"
#endif

#define NEW new (__FILE__, __LINE__) /* 注意new ()的定义见下面的定义 */

#else

#ifdef  NEW
#error Conflicting Macro "NEW"
#endif

#define NEW new  //将Cplusplus中的new命令更改为NEW

#endif //MEMORY_DEBUGGING


// 
// PLACEMENT NEW OPERATOR
/* 对NEW的取代,它是new的重新定义,不是c++中的new */

/* 注意这个函数是inline的,很特别 */
inline void* operator new(size_t, void* ptr) { return ptr;}

#if MEMORY_DEBUGGING

// These versions of the new operator with extra arguments provide memory debugging
// features.

/* 以下两个函数实际用到OSMemory::DebugNew() */
void* operator new(size_t s, char* inFile, int inLine);
void* operator new[](size_t s, char* inFile, int inLine);

#endif //MEMORY_DEBUGGING

// When memory debugging is not on, these are overridden so that if new fails,
// the process will exit.

/* 以下两个函数用到OSMemory::New() */
void* operator new (size_t s);
void* operator new[](size_t s);

/* 以下两个函数用到OSMemory::Delete() */
void operator delete(void* mem);
void operator delete[](void* mem);


#endif //__OS_MEMORY_H__
