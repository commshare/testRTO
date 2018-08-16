
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSMemory_Server.cpp
Description: Implementation of OSMemory stuff, including all new & delete operators.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <string.h>
#include "OSMemory.h" 


/* OSMemory类的static 成员事先声明 */
#if MEMORY_DEBUGGING

OSQueue OSMemory::sMemoryQueue;
OSQueue OSMemory::sTagQueue;
UInt32  OSMemory::sAllocatedBytes = 0; /* 默认分配字节为0 */
OSMutex  OSMemory::sMutex;

#endif

/* 设置内存错误状态初始值为0,下面通过 OSMemory::SetMemoryError()随时更改该值 */
static SInt32   sMemoryErr = 0;


//
// OPERATORS

#if MEMORY_DEBUGGING
/* 以下两个版本是DebugNew()引申而来 */
void* operator new(size_t s, char* inFile, int inLine)
{
    return OSMemory::DebugNew(s, inFile, inLine, true);
}

void* operator new[](size_t s, char* inFile, int inLine)
{
    return OSMemory::DebugNew(s, inFile, inLine, false);
}
#endif

/* 下面的new()/delete()函数等同于首字母大写的同名函数(类OSMemory中的成员函数) */
void* operator new (size_t s)
{
    return OSMemory::New(s);
}

void* operator new[](size_t s)
{
    return OSMemory::New(s);
}

void operator delete(void* mem)
{
    OSMemory::Delete(mem);
}

void operator delete[](void* mem)
{
    OSMemory::Delete(mem);
}


/******* 下面三个成员函数的定义十分重要!! *******/
/* 当MEMORY_DEBUGGING宏打开时,会调用相应的debug版本 */

/* 设置表示内存中的错误信息的静态变量 */
/* 用入参设置因内存分配失败,Server退出后返回的错误代码 */
void OSMemory::SetMemoryError(SInt32 inErr)
{
    sMemoryErr = inErr;
}

/************无debug作用的New()/Delete()实质就是malloc()/free()************************/

/* 实质用到malloc(),分配指定大小的内存,返回内存起始处的指针 */
void*   OSMemory::New(size_t inSize)
{
#if MEMORY_DEBUGGING
    return OSMemory::DebugNew(inSize, __FILE__, __LINE__, false);
#else
	/* 若分配缓存出错,将错误代码返回给父进程,并正常终止子进程 */
    void *m = malloc(inSize);
    if (m == NULL)
        ::exit(sMemoryErr);/* 见Linux C 函数 */
    return m;
#endif
}

/* 实质用到free(),删去内存中指定内容 */
void    OSMemory::Delete(void* inMemory)
{
    if (inMemory == NULL)
        return;
#if MEMORY_DEBUGGING
    OSMemory::DebugDelete(inMemory);
#else
    free(inMemory);
#endif
}

/*******************有debug作用的DebugNew()/DebugDelete()就是下面我们研究的重点********************/

#if MEMORY_DEBUGGING


/* (1)分配实际的缓存大小:入参s指定大小+结构体MemoryDebugging大小+首尾行号,标识缓存首尾行号;
   (2)从头至尾遍历整个sTagQueue队列的元素,找到入参指定文件名和行号的那个TagElem(假如没有找到就要创建它),更新它的相关信息; 
   (3)更新MemoryDebugging结构体,设置相关信息,加入全局sMemoryQueue;
   (4)返回实际分配缓存的起始处
*/
void* OSMemory::DebugNew(size_t s, char* inFile, int inLine, Bool16 sizeCheck)
{
    //also allocate enough space for a Q elem and a long to store the length of this
    //allocation block
    OSMutexLocker locker(&sMutex);
	/* 从头至尾遍历缓存队列中的所有元素,检查每个队列元关联的缓存单元首尾位置是否在同一行上? */
    ValidateMemoryQueue();
	/* 实际分配的缓存大小:实际大小+结构体MemoryDebugging大小+首尾行号 */
    UInt32 actualSize = s + sizeof(MemoryDebugging) + (2 * sizeof(inLine));
    char *m = (char *)malloc(actualSize);
    if (m == NULL)
        ::exit(sMemoryErr);

	/* 由文件名入参保存文件名字符串 */
    char theFileName[kMaxFileNameSize];
    strncpy(theFileName, inFile, kMaxFileNameSize);
	/* 记住末尾要截断 */
    theFileName[kMaxFileNameSize] = '\0';
    
    //mark the beginning and the end with the line number
	/* 用易于识别的0xfe来标识缓存首字符 */
    memset(m, 0xfe, actualSize);//mark the block with an easily identifiable pattern
	/* 将首尾设置相同的行号 */
    memcpy(m, &inLine, sizeof(inLine));
    memcpy((m + actualSize) - sizeof(inLine), &inLine, sizeof(inLine));

	/* 我们分配的这个缓存对应的TagElem实例 */
    TagElem* theElem = NULL;
    
    //also update the tag queue
	/* 从头至尾遍历整个sTagQueue队列的元素,找到入参指定文件名和行号的那个TagElem,更新它的相关信息 */
    for (OSQueueIter iter(&sTagQueue); !iter.IsDone(); iter.Next())
    {
		/* 获取队列元关联对象 */
        TagElem* elem = (TagElem*)iter.GetCurrent()->GetEnclosingObject();
		/* 找到文件名和行号相同的那个TagElem  */
        if ((::strcmp(elem->fileName, theFileName) == 0) && (elem->line == inLine))
        {
            //verify that the size of this allocation is the same as all others
            //(if requested... some tags are of variable size)
            if (sizeCheck)
				/* 确保分配的大小和这里记录的大小相同 */
                Assert(s == elem->tagSize);
			/* 累计总大小 */
            elem->totMemory += s;
            elem->numObjects++;
            theElem = elem;
        }
    }

	/* 假如遍历整个sTagQueue队列没有找到,就说明是个新的队列元实例,这里要创建它 */
    if (theElem == NULL)
    {
        //if we've gotten here, this tag doesn't exist, so let's add it.
		/* 为该结构体分配指定大小的缓存 */
        theElem = (TagElem*)malloc(sizeof(TagElem));
        if (theElem == NULL)
            ::exit(sMemoryErr);
		/* 初始化该结构体 */
        memset(theElem, 0, sizeof(TagElem));

		/********** 以下设置该结构体各成员的值 **********/

		/* 设置队列元关联对象 */
        theElem->elem.SetEnclosingObject(theElem);
		/* 用入参的文件名和行号来更新这相应的两个域 */
        ::strcpy(theElem->fileName, theFileName);
        theElem->line = inLine;
		/* 实际分配的缓存大小 */
        theElem->tagSize = s;
		/* 总结缓存大小 */
        theElem->totMemory = s;
		/* 对象数计数 */
        theElem->numObjects = 1;
		/* 加入该队列元进sTagQueue队列 */
        sTagQueue.EnQueue(&theElem->elem);
    }
    
    //put this chunk on the global chunk queue
	/* 更新MemoryDebugging结构体,加入全局sMemoryQueue */

	/****************** 注意:结构体类型强制转换,初始化!! ********************/
	/* 得到MemoryDebugging*的起始处 */
    MemoryDebugging* header = (MemoryDebugging*)(m + sizeof(inLine));
    memset(header, 0, sizeof(MemoryDebugging));
	/****************** 注意:结构体类型强制转换,初始化!! ********************/

	/* 设置分配的缓存大小 */
    header->size = s;
    header->tagElem = theElem;
	/* 设置关联对象 */
    header->elem.SetEnclosingObject(header);
	/* 加入缓存队列 */
    sMemoryQueue.EnQueue(&header->elem);
	/* 统计分配的字节总数 */
    sAllocatedBytes += s;
    
	/* 返回实际分配缓存的起始处 */
    return m + sizeof(inLine) + sizeof(MemoryDebugging);
}

/* 针对入参指定的缓存,获取其对象和起始指针,遍历整个sMemoryQueue队列,找到指定的MemoryDebugging实例,更新相关信息后移去该MemoryDebugging实例;
同时更新memInfo中的tagElem元的信息,当tagElem元中的对象数目为0时,从sTagQueue队列里删去它;最后删去实际分配的缓存部分,并释放为 MemoryDebugging实例分配的缓存 */
void OSMemory::DebugDelete(void *mem)
{
	/* 载入加锁/解锁类对象 */
    OSMutexLocker locker(&sMutex);
	/* 从头至尾遍历缓存队列sMemoryQueue中的所有元素,检查每个队列元关联的缓存单元首尾位置是否在同一行上? */
    ValidateMemoryQueue();

	/**这两个指针都很重要!!***/
	/* 获得缓存起始处 */
    char* memPtr = (char*)mem;
	/* 获取该 MemoryDebugging实例指针 */
    MemoryDebugging* memInfo = (MemoryDebugging*)mem;
	/* 如何理解?? */
    memInfo--;//get a pointer to the MemoryDebugging structure
	/* 确保是在缓存队列里 */
    Assert(memInfo->elem.IsMemberOfAnyQueue());//must be on the memory Queue


    //double check it's on the memory queue
    Bool16 found  = false;
	/* 遍历整个sMemoryQueue队列,找到指定的MemoryDebugging实例 */
    for (OSQueueIter iter(&sMemoryQueue); !iter.IsDone(); iter.Next())
    {
        MemoryDebugging* check = (MemoryDebugging*)iter.GetCurrent()->GetEnclosingObject();
        if (check == memInfo)
        {
            found = true;
            break;
        }
    }
	/* 务必确保找到该指定的MemoryDebugging实例 */
    Assert(found == true);
	/* 从sMemoryQueue队列中移去该队列元 */
    sMemoryQueue.Remove(&memInfo->elem);
	/* 确保该队列元不在sMemoryQueue队列中 */
    Assert(!memInfo->elem.IsMemberOfAnyQueue());
	/* 总字节数相应减少这些 */
    sAllocatedBytes -= memInfo->size;
    
    //verify that the tags placed at the very beginning and very end of the
    //block still exist
	/* 将memPtr指针移到 MemoryDebugging末尾,检查行号与TagElem中的行号相同 */
    memPtr += memInfo->size;
    int* linePtr = (int*)memPtr;
    Assert(*linePtr == memInfo->tagElem->line);
	/* 将memPtr指针移到 MemoryDebugging开头,检查行号与TagElem中的行号相同 */
    memPtr -= sizeof(MemoryDebugging) + sizeof(int) + memInfo->size;
    linePtr = (int*)memPtr;
    Assert(*linePtr == memInfo->tagElem->line);
    
    //also update the tag queue
	/* 同时更新memInfo中的tagElem元的信息 */
    Assert(memInfo->tagElem->numObjects > 0);
    memInfo->tagElem->numObjects--;
    memInfo->tagElem->totMemory -= memInfo->size;
    
    if (memInfo->tagElem->numObjects == 0)
    {
        // If this tag has no elements, then delete the tag
        Assert(memInfo->tagElem->totMemory == 0);
		/* 从sTagQueue队列中删去该tagElem */
        sTagQueue.Remove(&memInfo->tagElem->elem);
		/* 释放为tagElem实例分配的缓存,参见OSMemory::DebugNew()中相应部分 */
        free(memInfo->tagElem);
    }
    
    // delete our memory block
	/* 标识实际分配的缓存部分 */
    memset(mem, 0xfd,memInfo->size);
	/* 释放为 MemoryDebugging实例分配的缓存,参见OSMemory::DebugNew()中相应部分 */
    free(memPtr);
}

/* 这个是基础性的函数,要首先看 */
/* 从头至尾遍历缓存队列中的所有元素,检查每个队列元关联的缓存单元首尾位置存放的行号是否一致? */
void OSMemory::ValidateMemoryQueue()
{
	/* 执行上锁/解锁类 */
    OSMutexLocker locker(&sMutex);
	/* 从头至尾遍历sMemoryQueue缓存队列中的所有元素,直至遇到空的队列元为止才停下 */
    for(OSQueueIter iter(&sMemoryQueue); !iter.IsDone(); iter.Next())
    {
		/* 获取当前要debug的缓存单元,它是当前队列元的关联对象 */
        MemoryDebugging* elem = (MemoryDebugging*)iter.GetCurrent()->GetEnclosingObject();
		/* 获取当前要debug的缓存单元的起始位置 */
        char* rawmem = (char*)elem;
		/* 前移4个字节 */
        rawmem -= sizeof(int);
		/* 获取当前行号 */
        int* tagPtr = (int*)rawmem;
		/* 确保缓存单元起始处是在同一行上 */
        Assert(*tagPtr == elem->tagElem->line);
        rawmem += sizeof(int) + sizeof(MemoryDebugging) + elem->size;
        tagPtr = (int*)rawmem;
		/* 确保缓存单元结束处是在同一行上 */
        Assert(*tagPtr == elem->tagElem->line);
    }
}

#if 0  /* modified by taoyx 20091030 */
/* 上面的函数测试上述几个关键的api */
Bool16 OSMemory::MemoryDebuggingTest()
{
	/* 下面是3个长度分别为19,29,39的字符串 */
    static char* s20 = "this is 20 characte";
    static char* s30 = "this is 30 characters long, o";
    static char* s40 = "this is 40 characters long, okey dokeys";
    
    void* victim = DebugNew(20, 'tsta', true);
    strcpy((char*)victim, s20);
    MemoryDebugging* victimInfo = (MemoryDebugging*)victim;
    ValidateMemoryQueue();
    victimInfo--;
    if (victimInfo->tag != 'tsta')
        return false;
    if (victimInfo->size != 20)
        return false;
        
    void* victim2 = DebugNew(30, 'tstb', true);
    strcpy((char*)victim2, s30);
    ValidateMemoryQueue();
    void* victim3 = DebugNew(20, 'tsta', true);
    strcpy((char*)victim3, s20);
    ValidateMemoryQueue();
    void* victim4 = DebugNew(40, 'tstc', true);
    strcpy((char*)victim4, s40);
    ValidateMemoryQueue();
    void* victim5 = DebugNew(30, 'tstb', true);
    strcpy((char*)victim5, s30);
    ValidateMemoryQueue();
    
    if (sTagQueue.GetLength() != 3)
        return false;
    for (OSQueueIter iter(&sTagQueue); !iter.IsDone(); iter.Next())
    {
        TagElem* elem = (TagElem*)iter.GetCurrent()->GetEnclosingObject();
        if (*elem->tagPtr == 'tstb')
        {
            if (elem->tagSize != 30)
                return false;
            if (elem->numObjects != 2)
                return false;
        }
        else if (*elem->tagPtr == 'tsta')
        {
            if (elem->tagSize != 20)
                return false;
            if (elem->numObjects != 2)
                return false;
        }
        else if (*elem->tagPtr == 'tstc')
        {
            if (elem->tagSize != 40)
                return false;
            if (elem->numObjects != 1)
                return false;
        }
        else
            return false;
    }
    
    DebugDelete(victim3);
    ValidateMemoryQueue();
    DebugDelete(victim4);
    ValidateMemoryQueue();

    if (sTagQueue.GetLength() != 3)
        return false;
    for (OSQueueIter iter2(&sTagQueue); !iter2.IsDone(); iter2.Next())
    {
        TagElem* elem = (TagElem*)iter2.GetCurrent()->GetEnclosingObject();
        if (*elem->tagPtr == 'tstb')
        {
            if (elem->tagSize != 30)
                return false;
            if (elem->numObjects != 2)
                return false;
        }
        else if (*elem->tagPtr == 'tsta')
        {
            if (elem->tagSize != 20)
                return false;
            if (elem->numObjects != 1)
                return false;
        }
        else if (*elem->tagPtr == 'tstc')
        {
            if (elem->tagSize != 40)
                return false;
            if (elem->numObjects != 0)
                return false;
        }
        else
            return false;
    }
    
    if (sMemoryQueue.GetLength() != 3)
        return false;
    DebugDelete(victim);
    ValidateMemoryQueue();
    if (sMemoryQueue.GetLength() != 2)
        return false;
    DebugDelete(victim5);
    ValidateMemoryQueue();
    if (sMemoryQueue.GetLength() != 1)
        return false;
    DebugDelete(victim2);
    ValidateMemoryQueue();
    if (sMemoryQueue.GetLength() != 0)
        return false;
    DebugDelete(victim4);
    return true;
}
#endif //0

#endif // MEMORY_DEBUGGING

