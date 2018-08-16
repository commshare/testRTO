
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


/* OSMemory���static ��Ա�������� */
#if MEMORY_DEBUGGING

OSQueue OSMemory::sMemoryQueue;
OSQueue OSMemory::sTagQueue;
UInt32  OSMemory::sAllocatedBytes = 0; /* Ĭ�Ϸ����ֽ�Ϊ0 */
OSMutex  OSMemory::sMutex;

#endif

/* �����ڴ����״̬��ʼֵΪ0,����ͨ�� OSMemory::SetMemoryError()��ʱ���ĸ�ֵ */
static SInt32   sMemoryErr = 0;


//
// OPERATORS

#if MEMORY_DEBUGGING
/* ���������汾��DebugNew()������� */
void* operator new(size_t s, char* inFile, int inLine)
{
    return OSMemory::DebugNew(s, inFile, inLine, true);
}

void* operator new[](size_t s, char* inFile, int inLine)
{
    return OSMemory::DebugNew(s, inFile, inLine, false);
}
#endif

/* �����new()/delete()������ͬ������ĸ��д��ͬ������(��OSMemory�еĳ�Ա����) */
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


/******* ����������Ա�����Ķ���ʮ����Ҫ!! *******/
/* ��MEMORY_DEBUGGING���ʱ,�������Ӧ��debug�汾 */

/* ���ñ�ʾ�ڴ��еĴ�����Ϣ�ľ�̬���� */
/* ������������ڴ����ʧ��,Server�˳��󷵻صĴ������ */
void OSMemory::SetMemoryError(SInt32 inErr)
{
    sMemoryErr = inErr;
}

/************��debug���õ�New()/Delete()ʵ�ʾ���malloc()/free()************************/

/* ʵ���õ�malloc(),����ָ����С���ڴ�,�����ڴ���ʼ����ָ�� */
void*   OSMemory::New(size_t inSize)
{
#if MEMORY_DEBUGGING
    return OSMemory::DebugNew(inSize, __FILE__, __LINE__, false);
#else
	/* �����仺�����,��������뷵�ظ�������,��������ֹ�ӽ��� */
    void *m = malloc(inSize);
    if (m == NULL)
        ::exit(sMemoryErr);/* ��Linux C ���� */
    return m;
#endif
}

/* ʵ���õ�free(),ɾȥ�ڴ���ָ������ */
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

/*******************��debug���õ�DebugNew()/DebugDelete()�������������о����ص�********************/

#if MEMORY_DEBUGGING


/* (1)����ʵ�ʵĻ����С:���sָ����С+�ṹ��MemoryDebugging��С+��β�к�,��ʶ������β�к�;
   (2)��ͷ��β��������sTagQueue���е�Ԫ��,�ҵ����ָ���ļ������кŵ��Ǹ�TagElem(����û���ҵ���Ҫ������),�������������Ϣ; 
   (3)����MemoryDebugging�ṹ��,���������Ϣ,����ȫ��sMemoryQueue;
   (4)����ʵ�ʷ��仺�����ʼ��
*/
void* OSMemory::DebugNew(size_t s, char* inFile, int inLine, Bool16 sizeCheck)
{
    //also allocate enough space for a Q elem and a long to store the length of this
    //allocation block
    OSMutexLocker locker(&sMutex);
	/* ��ͷ��β������������е�����Ԫ��,���ÿ������Ԫ�����Ļ��浥Ԫ��βλ���Ƿ���ͬһ����? */
    ValidateMemoryQueue();
	/* ʵ�ʷ���Ļ����С:ʵ�ʴ�С+�ṹ��MemoryDebugging��С+��β�к� */
    UInt32 actualSize = s + sizeof(MemoryDebugging) + (2 * sizeof(inLine));
    char *m = (char *)malloc(actualSize);
    if (m == NULL)
        ::exit(sMemoryErr);

	/* ���ļ�����α����ļ����ַ��� */
    char theFileName[kMaxFileNameSize];
    strncpy(theFileName, inFile, kMaxFileNameSize);
	/* ��סĩβҪ�ض� */
    theFileName[kMaxFileNameSize] = '\0';
    
    //mark the beginning and the end with the line number
	/* ������ʶ���0xfe����ʶ�������ַ� */
    memset(m, 0xfe, actualSize);//mark the block with an easily identifiable pattern
	/* ����β������ͬ���к� */
    memcpy(m, &inLine, sizeof(inLine));
    memcpy((m + actualSize) - sizeof(inLine), &inLine, sizeof(inLine));

	/* ���Ƿ������������Ӧ��TagElemʵ�� */
    TagElem* theElem = NULL;
    
    //also update the tag queue
	/* ��ͷ��β��������sTagQueue���е�Ԫ��,�ҵ����ָ���ļ������кŵ��Ǹ�TagElem,�������������Ϣ */
    for (OSQueueIter iter(&sTagQueue); !iter.IsDone(); iter.Next())
    {
		/* ��ȡ����Ԫ�������� */
        TagElem* elem = (TagElem*)iter.GetCurrent()->GetEnclosingObject();
		/* �ҵ��ļ������к���ͬ���Ǹ�TagElem  */
        if ((::strcmp(elem->fileName, theFileName) == 0) && (elem->line == inLine))
        {
            //verify that the size of this allocation is the same as all others
            //(if requested... some tags are of variable size)
            if (sizeCheck)
				/* ȷ������Ĵ�С�������¼�Ĵ�С��ͬ */
                Assert(s == elem->tagSize);
			/* �ۼ��ܴ�С */
            elem->totMemory += s;
            elem->numObjects++;
            theElem = elem;
        }
    }

	/* �����������sTagQueue����û���ҵ�,��˵���Ǹ��µĶ���Ԫʵ��,����Ҫ������ */
    if (theElem == NULL)
    {
        //if we've gotten here, this tag doesn't exist, so let's add it.
		/* Ϊ�ýṹ�����ָ����С�Ļ��� */
        theElem = (TagElem*)malloc(sizeof(TagElem));
        if (theElem == NULL)
            ::exit(sMemoryErr);
		/* ��ʼ���ýṹ�� */
        memset(theElem, 0, sizeof(TagElem));

		/********** �������øýṹ�����Ա��ֵ **********/

		/* ���ö���Ԫ�������� */
        theElem->elem.SetEnclosingObject(theElem);
		/* ����ε��ļ������к�����������Ӧ�������� */
        ::strcpy(theElem->fileName, theFileName);
        theElem->line = inLine;
		/* ʵ�ʷ���Ļ����С */
        theElem->tagSize = s;
		/* �ܽỺ���С */
        theElem->totMemory = s;
		/* ���������� */
        theElem->numObjects = 1;
		/* ����ö���Ԫ��sTagQueue���� */
        sTagQueue.EnQueue(&theElem->elem);
    }
    
    //put this chunk on the global chunk queue
	/* ����MemoryDebugging�ṹ��,����ȫ��sMemoryQueue */

	/****************** ע��:�ṹ������ǿ��ת��,��ʼ��!! ********************/
	/* �õ�MemoryDebugging*����ʼ�� */
    MemoryDebugging* header = (MemoryDebugging*)(m + sizeof(inLine));
    memset(header, 0, sizeof(MemoryDebugging));
	/****************** ע��:�ṹ������ǿ��ת��,��ʼ��!! ********************/

	/* ���÷���Ļ����С */
    header->size = s;
    header->tagElem = theElem;
	/* ���ù������� */
    header->elem.SetEnclosingObject(header);
	/* ���뻺����� */
    sMemoryQueue.EnQueue(&header->elem);
	/* ͳ�Ʒ�����ֽ����� */
    sAllocatedBytes += s;
    
	/* ����ʵ�ʷ��仺�����ʼ�� */
    return m + sizeof(inLine) + sizeof(MemoryDebugging);
}

/* ������ָ���Ļ���,��ȡ��������ʼָ��,��������sMemoryQueue����,�ҵ�ָ����MemoryDebuggingʵ��,���������Ϣ����ȥ��MemoryDebuggingʵ��;
ͬʱ����memInfo�е�tagElemԪ����Ϣ,��tagElemԪ�еĶ�����ĿΪ0ʱ,��sTagQueue������ɾȥ��;���ɾȥʵ�ʷ���Ļ��沿��,���ͷ�Ϊ MemoryDebuggingʵ������Ļ��� */
void OSMemory::DebugDelete(void *mem)
{
	/* �������/��������� */
    OSMutexLocker locker(&sMutex);
	/* ��ͷ��β�����������sMemoryQueue�е�����Ԫ��,���ÿ������Ԫ�����Ļ��浥Ԫ��βλ���Ƿ���ͬһ����? */
    ValidateMemoryQueue();

	/**������ָ�붼����Ҫ!!***/
	/* ��û�����ʼ�� */
    char* memPtr = (char*)mem;
	/* ��ȡ�� MemoryDebuggingʵ��ָ�� */
    MemoryDebugging* memInfo = (MemoryDebugging*)mem;
	/* ������?? */
    memInfo--;//get a pointer to the MemoryDebugging structure
	/* ȷ�����ڻ�������� */
    Assert(memInfo->elem.IsMemberOfAnyQueue());//must be on the memory Queue


    //double check it's on the memory queue
    Bool16 found  = false;
	/* ��������sMemoryQueue����,�ҵ�ָ����MemoryDebuggingʵ�� */
    for (OSQueueIter iter(&sMemoryQueue); !iter.IsDone(); iter.Next())
    {
        MemoryDebugging* check = (MemoryDebugging*)iter.GetCurrent()->GetEnclosingObject();
        if (check == memInfo)
        {
            found = true;
            break;
        }
    }
	/* ���ȷ���ҵ���ָ����MemoryDebuggingʵ�� */
    Assert(found == true);
	/* ��sMemoryQueue��������ȥ�ö���Ԫ */
    sMemoryQueue.Remove(&memInfo->elem);
	/* ȷ���ö���Ԫ����sMemoryQueue������ */
    Assert(!memInfo->elem.IsMemberOfAnyQueue());
	/* ���ֽ�����Ӧ������Щ */
    sAllocatedBytes -= memInfo->size;
    
    //verify that the tags placed at the very beginning and very end of the
    //block still exist
	/* ��memPtrָ���Ƶ� MemoryDebuggingĩβ,����к���TagElem�е��к���ͬ */
    memPtr += memInfo->size;
    int* linePtr = (int*)memPtr;
    Assert(*linePtr == memInfo->tagElem->line);
	/* ��memPtrָ���Ƶ� MemoryDebugging��ͷ,����к���TagElem�е��к���ͬ */
    memPtr -= sizeof(MemoryDebugging) + sizeof(int) + memInfo->size;
    linePtr = (int*)memPtr;
    Assert(*linePtr == memInfo->tagElem->line);
    
    //also update the tag queue
	/* ͬʱ����memInfo�е�tagElemԪ����Ϣ */
    Assert(memInfo->tagElem->numObjects > 0);
    memInfo->tagElem->numObjects--;
    memInfo->tagElem->totMemory -= memInfo->size;
    
    if (memInfo->tagElem->numObjects == 0)
    {
        // If this tag has no elements, then delete the tag
        Assert(memInfo->tagElem->totMemory == 0);
		/* ��sTagQueue������ɾȥ��tagElem */
        sTagQueue.Remove(&memInfo->tagElem->elem);
		/* �ͷ�ΪtagElemʵ������Ļ���,�μ�OSMemory::DebugNew()����Ӧ���� */
        free(memInfo->tagElem);
    }
    
    // delete our memory block
	/* ��ʶʵ�ʷ���Ļ��沿�� */
    memset(mem, 0xfd,memInfo->size);
	/* �ͷ�Ϊ MemoryDebuggingʵ������Ļ���,�μ�OSMemory::DebugNew()����Ӧ���� */
    free(memPtr);
}

/* ����ǻ����Եĺ���,Ҫ���ȿ� */
/* ��ͷ��β������������е�����Ԫ��,���ÿ������Ԫ�����Ļ��浥Ԫ��βλ�ô�ŵ��к��Ƿ�һ��? */
void OSMemory::ValidateMemoryQueue()
{
	/* ִ������/������ */
    OSMutexLocker locker(&sMutex);
	/* ��ͷ��β����sMemoryQueue��������е�����Ԫ��,ֱ�������յĶ���ԪΪֹ��ͣ�� */
    for(OSQueueIter iter(&sMemoryQueue); !iter.IsDone(); iter.Next())
    {
		/* ��ȡ��ǰҪdebug�Ļ��浥Ԫ,���ǵ�ǰ����Ԫ�Ĺ������� */
        MemoryDebugging* elem = (MemoryDebugging*)iter.GetCurrent()->GetEnclosingObject();
		/* ��ȡ��ǰҪdebug�Ļ��浥Ԫ����ʼλ�� */
        char* rawmem = (char*)elem;
		/* ǰ��4���ֽ� */
        rawmem -= sizeof(int);
		/* ��ȡ��ǰ�к� */
        int* tagPtr = (int*)rawmem;
		/* ȷ�����浥Ԫ��ʼ������ͬһ���� */
        Assert(*tagPtr == elem->tagElem->line);
        rawmem += sizeof(int) + sizeof(MemoryDebugging) + elem->size;
        tagPtr = (int*)rawmem;
		/* ȷ�����浥Ԫ����������ͬһ���� */
        Assert(*tagPtr == elem->tagElem->line);
    }
}

#if 0  /* modified by taoyx 20091030 */
/* ����ĺ����������������ؼ���api */
Bool16 OSMemory::MemoryDebuggingTest()
{
	/* ������3�����ȷֱ�Ϊ19,29,39���ַ��� */
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

