/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSFileSource.h
Description: Provide a simple streaming file abstraction.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "OSFileSource.h"
#include "OSMemory.h"
#include "OSThread.h"
#include "OS.h"
#include "OSQueue.h"
#include "OSHeaders.h"


/* 下面是相关调试的宏开关 */
#define FILE_SOURCE_DEBUG 0
#define FILE_SOURCE_BUFFTEST 0
#define TEST_TIME 0

#if TEST_TIME
static SInt64 startTime = 0;
static SInt64 durationTime = 0;
static SInt32 sReadCount = 0;
static SInt32 sByteCount = 0;
static Bool16 sMovie = false;

#endif


#if READ_LOG
extern UInt32 xTrackID;

/* 设置日志文件路径,打开日志文件,记录相关信息后再关闭该日志文件 */
void OSFileSource::SetLog(const char *inPath)
{
    fFilePath[0] =0;
    ::strcpy(fFilePath,inPath);
    
	/* 当media文件已打开且没有日志文件,设置日志文件的路径并以追加方式打开,在合法状态下记录相应的文件日志 */
    if (fFile != -1 && fFileLog == NULL)
    {
        ::strcat(fFilePath,inPath);/* 注意该路径记录了两次 */
        ::strcat(fFilePath,".readlog");
        fFileLog = ::fopen(fFilePath,"w+");
        if (fFileLog && IsValid())
        {   qtss_fprintf(fFileLog, "%s","QTFILE_READ_LOG\n");
            qtss_fprintf(fFileLog, "size: %qu\n",GetLength());
            qtss_printf("OSFileSource::SetLog=%s\n",fFilePath);
            
        }
        ::fclose(fFileLog);
    }
}
#else
void OSFileSource::SetLog(const char *inPath)
{

	/* 打印简单的信息 */
#if FILE_SOURCE_DEBUG
    qtss_printf("OSFileSource::SetLog=%s\n",inPath);
#endif
    
}
#endif


/* 删除数据缓存 */
FileBlockBuffer::~FileBlockBuffer(void)
{
    if (fDataBuffer != NULL)
    {
		/* 确保文件存放没有越界? */
        Assert (fDataBuffer[fBufferSize] == 0);
        
#if FILE_SOURCE_DEBUG
    ::memset( (char *)fDataBuffer,0, fBufferSize);
    qtss_printf("FileBlockBuffer::~FileBlockBuffer delete %lu this=%lu\n",fDataBuffer, this);
#endif
        delete fDataBuffer;
        fDataBuffer = NULL;
        fArrayIndex = -1;
    }
    else 
        Assert(false);
}

/* used in FileBlockPool::GetBufferElement() */
/* 分配指定大小的buffer,最后一个分量后的一个分量置0 */
void FileBlockBuffer::AllocateBuffer(UInt32 buffSize)
{
    fBufferSize = buffSize;
    fDataBuffer = NEW char[buffSize + 1];
    fDataBuffer[buffSize] = 0;
    
#if FILE_SOURCE_DEBUG
    this->CleanBuffer();
    qtss_printf("FileBlockBuffer::FileBlockBuffer allocate buff ptr =%lu len=%lu this=%lu\n",fDataBuffer,buffSize,this);
#endif

}

/* 检查buffer数据中最后一个分量是否是0 ?以此看文件缓存是否填满? */
void FileBlockBuffer::TestBuffer(void)
{

#if FILE_SOURCE_BUFFTEST    
    if (fDataBuffer != NULL)
        Assert (fDataBuffer[fBufferSize] == 0); 
#endif

}

/* 使即将要读取数据的缓存块位于队列尾部 */ 
void FileBlockPool::MarkUsed(FileBlockBuffer* inBuffPtr)
{
    /* 	若是空缓存块,就无条件返回 */
    if (NULL == inBuffPtr)
        return;
    
	/* 假若队列尾部不是入参指定的buffer块,就把它从队列中移到队列尾部 */
    if (fQueue.GetTail() != inBuffPtr->GetQElem()) // Least Recently Used tail is last accessed
    {
        fQueue.Remove(inBuffPtr->GetQElem());
        fQueue.EnQueue(inBuffPtr->GetQElem()); // put on tail
    }
}  

/* used in FileMap::DeleteOldBuffs() */
/* 当当前buffer block个数没有超过最大buffer block个数时,新建一个FileBlockBuffer,分配入参指定大小的buffer,并放在队列的尾端,并返回该FileBlockBuffer; 
   否则,当已经是最大缓存块时,删除并返回队列头元素,即将队列元素的所在对象返回 */
FileBlockBuffer *FileBlockPool::GetBufferElement(UInt32 bufferSizeBytes)
{
    FileBlockBuffer* theNewBuf = NULL;

	/* 当当前buffer block个数没有超过最大buffer block个数时,新建一个FileBlockBuffer,分配入参指定大小的buffer,并放在队列的尾端,并返回该FileBlockBuffer; */
    if ( fNumCurrentBuffers < fMaxBuffers)
    {
#if FILE_SOURCE_DEBUG
            qtss_printf("FileBlockPool::GetBufferElement NEW element fNumCurrentBuffers=%lu fMaxBuffers=%lu fBufferUnitSizeBytes=%lu bufferSizeBytes=%lu\n",fNumCurrentBuffers,fMaxBuffers,fBufferUnitSizeBytes,bufferSizeBytes);
#endif  /* 生成一个缓存块对象FileBlockBuffer */
        theNewBuf = NEW FileBlockBuffer();
        theNewBuf->AllocateBuffer(bufferSizeBytes);//分配256K=8*32K的缓存
        fNumCurrentBuffers++;
        theNewBuf->fQElem.SetEnclosingObject(theNewBuf);
        fQueue.EnQueue(theNewBuf->GetQElem()); // put on tail
        Assert(theNewBuf != NULL);
        return theNewBuf;//返回新建的缓存块
    }
    
	/* 当已经是最大缓存块时,删除并返回队列头元素 */
    OSQueueElem *theElem = fQueue.DeQueue(); // get head

    Assert(theElem != NULL);
    
    if (theElem == NULL)
        return NULL;
     
	/* 将队列元素的所在对象设为theNewBuf */
    theNewBuf = (FileBlockBuffer*) theElem->GetEnclosingObject();
    Assert(theNewBuf != NULL);
    //qtss_printf("FileBlockPool::GetBufferElement reuse buffer theNewBuf=%lu fDataBuffer=%lu fArrayIndex=%ld\n",theNewBuf,theNewBuf->fDataBuffer,theNewBuf->fArrayIndex);

    return theNewBuf;

}  
 
/* 循环删除队列元素,极其依附的文件缓存块,并作相关参数设置 */
void FileBlockPool::DeleteBlockPool(void) 
{

    FileBlockBuffer *buffer = NULL;
	/* 删除并返回队列头元素 */
    OSQueueElem* theElem = fQueue.DeQueue();
    while (theElem != NULL)
    {   
		/* 删除被删除队列元素的依存内容 */
		buffer = (FileBlockBuffer *) theElem->GetEnclosingObject();
        delete buffer;
		/* 再删除并返回队列头元素 */
        theElem = fQueue.DeQueue(); 
    }
    
	/* 作相关参数设置 */
    fMaxBuffers = 1;
    fNumCurrentBuffers = 0; 
    fBufferUnitSizeBytes = kBufferUnitSize;//32Kbytes
}

FileBlockPool::~FileBlockPool(void) 
{

    this->DeleteBlockPool();
}

/* 非常重要的一个函数,值得深究!used in OSFileSource::AllocateFileCache() */
/* 根据文件长度设置缓存块个数,给媒体文件分配恰当大小的一个缓存块指针的数组结构,并将这些指针清零 */
void FileMap::AllocateBufferMap(UInt32 inUnitSizeInK/*64*/, UInt32 inNumBuffSizeUnits/*1*/, UInt32 inBufferIncCount/*8*/, UInt32 inMaxBitRateBuffSizeInBlocks/*8*/, UInt64 fileLen/* 根据具体文件计算 */, UInt32 inBitRate/*根据具体文件计算比特率*/)
{
    
	/* 假如存在缓存映射,只有一个缓存块,并且最大缓存块是8个,则直接返回 */
    if (fFileMapArray != NULL && fNumBuffSizeUnits == inNumBuffSizeUnits && inBufferIncCount == fBlockPool.GetMaxBuffers())
        return;
        
	/* 文件缓存单元最小是1K字节,一般是32K字节 */
    if( inUnitSizeInK < 1 )
        inUnitSizeInK = 1;
        
	/* 用入参设置缓存单元大小, 默认32Kbytes,此处是64K字节 */
    fBlockPool.SetBufferUnitSize(inUnitSizeInK);
    
	/* 调整媒体文件比特率大小,按最大值计算 */
    if (inBitRate == 0) // just use the maximum possible size
        inBitRate = inMaxBitRateBuffSizeInBlocks * fBlockPool.GetBufferUnitSizeBytes(); /* 最大为8*64K */
    
	/* 手动计算inNumBuffSizeUnits为比特率和缓存块大小的比值,最大值可能为8,最小值为1  */
    if (inNumBuffSizeUnits == 0) // calculate the buffer size ourselves
    {
        inNumBuffSizeUnits = inBitRate / fBlockPool.GetBufferUnitSizeBytes();/* 除以32K */
        
        if( inNumBuffSizeUnits > inMaxBitRateBuffSizeInBlocks) // max is 8 * buffUnit Size (32k) = 256K
        {   inNumBuffSizeUnits = inMaxBitRateBuffSizeInBlocks;
        }
    } //else the inNumBuffSizeUnits is explicitly defined so just use that value
    
    if( inNumBuffSizeUnits < 1 )
        inNumBuffSizeUnits = 1;
     
	/* 删除缓存块指针数组,注意仅是将每个指针分量置空 */
    this->DeleteMap(); 
	/* 循环删除队列元素,极其依附的文件缓存块,并作相关参数设置 */
    fBlockPool.DeleteBlockPool();
    
	/* 注意此处赋值 */
    fNumBuffSizeUnits = inNumBuffSizeUnits;//1
	/* 媒体文件分块大小,该量设置很重要(与比特率相关) */
    fDataBufferSize = fBlockPool.GetBufferUnitSizeBytes() * inNumBuffSizeUnits; //1*32Kbytes
    
	/* 设置最大缓存块个数是8 */
    fBlockPool.SetMaxBuffers(inBufferIncCount);
	/* 设置buffer增量是8 */
    fBlockPool.SetBuffIncValue(inBufferIncCount);

	/* 根据文件长度设置缓存块个数,这里加1就是避免四舍五入的问题 */
    fMapArraySize = (fileLen / fDataBufferSize) + 1;
	/* 给分量为FileBlockBuffer *的数组分配缓存,注意数组分量为指针 */
    fFileMapArray = NEW FileBlockBuffer *[ (SInt32) (fMapArraySize + 1) ];
    
	/* 初始化缓存块指针数组,置0(清0) */
	this->Clean(); // required because fFileMapArray's array is used to store buffer pointers.
#if FILE_SOURCE_DEBUG
    qtss_printf("FileMap::AllocateBufferMap shared buffers fFileMapArray=%lu fDataBufferSize= %lu fMapArraySize=%lu fileLen=%qu \n",fFileMapArray, fDataBufferSize, fMapArraySize,fileLen);   
#endif

}    


/* 当缓存池中当前的buffer block个数大于最大的buffer block个数时,就删去一些旧的buffer block,并及时更新buffer block总个数 */
void FileMap::DeleteOldBuffs()
{
	/* 当当前的buffer block个数大于最大的buffer block个数时,就删去一些旧的buffer block */
    while (fBlockPool.GetNumCurrentBuffers() > fBlockPool.GetMaxBuffers()) // delete any old buffers
    {
		/* 删除队列头元素并返回其所在buffer block对象 */
		FileBlockBuffer *theElem =  fBlockPool.GetBufferElement(fDataBufferSize);
        fFileMapArray[theElem->fArrayIndex] = NULL; /* 将该缓存块指针置空 */
        delete theElem;/* 删除该buffer block */		
        fBlockPool.DecCurBuffers();/* 将当前buffer block总数减一 */
    }
}   


/* used in OSFileSource::ReadFromCache() */
/* 首先通过新建或重用确保取得入参指定的buffer block,将其所含的队列元放在缓存块队列的尾部;再设置该buffer block的当前索引为buffIndex,并返回其内的data.
   注意第二个参数暗示是否需要填充数据?若重用该buffer block,就不需要填充数据,第二个入参为false.默认新建缓存块(true).一定要确保入参buffIndex和FileBlockBuffer->fArrayIndex相等 */
/* 实质上利用fBlockPool.GetBufferElement(fDataBufferSize)来得到新建的缓存 */
char *FileMap::GetBuffer(SInt64 buffIndex, Bool16 *outFillBuff)
{
    Assert(outFillBuff != NULL);
	/* 这个量的值会再分配 */
	/* 暂定第一个入参指定的缓存块要填充数据 */
    *outFillBuff = true; // we are re-using or just created a buff

	/* 当缓存池中当前的buffer block个数大于最大的buffer block个数时,就删去一些旧的buffer block,并及时更新buffer block个数 */
    this->DeleteOldBuffs();
	/* 指定索引必须小于缓存块指针数组的总个数(注意:不是缓存池的缓存块个数) */
    Assert(buffIndex < (SInt32) fMapArraySize);
    
	/* 取得入参指定索引的FileBlockBuffer类指针,一定要判断它是否存在? */
	/* 假如没有分配公有缓存,则fFileMapArray为空 */
    FileBlockBuffer *theElem = fFileMapArray[buffIndex];
	/* 假若指定索引的FileBlockBuffer不存在,就新建一个指定大小(32*8Kbytes)的buffer并放在队尾,或者取出满队列头部的缓存块 */
    if ( NULL == theElem)
    {
        #if FILE_SOURCE_DEBUG
            qtss_printf("FileMap::GetBuffer call fBlockPool.GetBufferElement(); buffIndex=%ld\n",buffIndex);
        #endif
        
		/* 当当前buffer block个数没有超过最大buffer block个数时,新建一个FileBlockBuffer,分配入参指定大小的buffer,并放在队列的尾端,并返回该FileBlockBuffer; 
		否则,当已经是最大缓存块时,删除并返回队列头元素,即将队列元素的所在对象返回 */
         theElem =  fBlockPool.GetBufferElement(fDataBufferSize);
         Assert(theElem);
    }
    
	/* 调整缓存块队列中的缓存块的相对位置,确保上述要读取数据的FileBlockBuffer所含的队列元放在队列的尾部 */
    fBlockPool.MarkUsed(theElem); // must happen here after getting a pre-allocated or used buffer.

	/* 假若命中,直接返回它中的data. */
    if (theElem->fArrayIndex == buffIndex) // found a pre-allocated and filled buffer
    {
        #if FILE_SOURCE_DEBUG
            qtss_printf("FileMap::GetBuffer pre-allocated buff buffIndex=%ld\n",buffIndex);
        #endif
        
        *outFillBuff = false;
        return theElem->fDataBuffer;
    }

	/* 处理重用缓存块情况 */
    if (theElem->fArrayIndex >= 0)
    {
        fFileMapArray[theElem->fArrayIndex] = NULL; // reset the old map location
    }


	/* 确保新取得的buffer block的数组索引就是入参buffIndex */
	/* 根据队列索引(入参buffIndex)重置相应元素theElem的位置 */
    fFileMapArray[buffIndex] = theElem; // a new buffer
    theElem->fArrayIndex = buffIndex; // record the index
    
#if FILE_SOURCE_DEBUG
    theElem->CleanBuffer();
#endif
    
	//注意返回缓存地址时,第二个入参*outFillBuff = true,表示需要填充数据
    return theElem->fDataBuffer;
    
}


/* 初始化缓存块指针数组,置0(清0) */
void    FileMap::Clean(void)
{
    if (fFileMapArray != NULL)
        ::memset( (char *)fFileMapArray,0, (SInt32) (sizeof(FileBlockBuffer *) * fMapArraySize) );
}

/* 删除缓存块指针数组,注意仅是将每个指针分量置空 */
void    FileMap::DeleteMap(void)
{
    if (NULL == fFileMapArray)
        return;
        
#if FILE_SOURCE_DEBUG
    qtss_printf("FileMap::DeleteMap fFileMapArray=%lu fMapArraySize=%ld \n",fFileMapArray, fMapArraySize);   
    this->Clean();
#endif

    delete fFileMapArray;
    fFileMapArray = NULL;

}

 
/* used in QTFile::Open(), 以只读方式打开指定路径的媒体文件,并作出错处理,设置文件相应参数信息 */
void OSFileSource::Set(const char *inPath)
{
    Close();
 
/* 以只读方式打开入参指定路径的文件 */
    fFile = open(inPath, O_RDONLY | O_LARGEFILE);

	/* 假如文件能够成功打开并获取相关统计信息,就设置文件相应参数信息,记录日志信息,否则关闭打开的文件 */
    if (fFile != -1)
    {
        struct stat buf;
		::memset(&buf,sizeof(buf),0);
        if (::fstat(fFile, &buf) >= 0)
        {
            fLength = buf.st_size;
            fModDate = buf.st_mtime;
            if (fModDate < 0)
                fModDate = 0;

            fIsDir = S_ISDIR(buf.st_mode);
            this->SetLog(inPath);
        }
        else
            this->Close();
    }   
}



void OSFileSource::Advise(UInt64 , UInt32 )
{
// does nothing on platforms other than MacOSXServer
}


/* used in OSFileSource::ReadFromCache() */
/* 注意第一个参数没有用到.先定位要查找文件的位置,再使用OSFileSource::ReadFromDisk()从硬盘上指定的文件位置读取数据,存入指定缓存并设置该缓存的填充大小,并检查(该缓存块是否填满)最后一个分量是否是0 ? */
OS_Error    OSFileSource::FillBuffer(char* ioBuffer, char *buffStart, SInt32 buffIndex)
{
	/* 获取每个buffer block大小的最大值 */
    UInt32 buffSize = fFileMap.GetMaxBufSize();
	/* 得到指定buffer的offset,来定位硬盘中要查找文件的位置(注意这里假设每个buffer块是填满的,从而通过计算缓存块的数据总量来间接确定要读取文件的位置!) */
    UInt64 startPos = buffIndex * buffSize;
    UInt32 readLen = 0;
    
	/* 利用入参startPos先定位要查找文件的位置,再使用OSFileSource::ReadFromDisk()从指定的文件处读取数据,存入指定缓存并设置实际读取数据的长度readLen */
    OS_Error theErr = this->ReadFromPos(startPos, buffStart, buffSize, &readLen);   

	/* 为指定buffIndex的buffer设置入参指定的填充大小FillSize */
    fFileMap.SetIndexBuffFillSize(buffIndex, readLen);

	/* 该函数没有定义 */
    /* 在调试时,会检查buffer数据((该缓存块是否填满))中最后一个分量是否是0 ? */
    fFileMap.TestBuffer(buffIndex); 
                
    return theErr;
}

#if FILE_SOURCE_BUFFTEST
static SInt32 sBuffCount = 1;   
#endif

/* needed in QTFile::Read()/QTFile_FileControlBlock::Read()/QTFile_FileControlBlock::ReadInternal() */
/* 除非例外情况使用OSFileSource::ReadFromPos()直接从磁盘读取媒体文件数据,其它情况默认使用OSFileSource::ReadFromCache(),从缓存块读取,来将指定位置inPosition的
媒体数据读入指定长度的指定buffer,并记录实际复制数据的长度 */
OS_Error    OSFileSource::Read(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{ 
    /* 分两种情况讨论: */    
    if  ( ( !fFileMap.Initialized() )/* 假如没有分配私有缓存,使得fFileMapArray=NULL */
            || ( !fCacheEnabled ) /* 假如不能缓存文件 */
            || ( fFileMap.GetBuffIndex(inPosition+inLength) > fFileMap.GetMaxBuffIndex() ) /* 请求媒体数据位置超过最大文件缓冲块的索引 */
        )//直接从磁盘读取媒体文件数据
        return  this->ReadFromPos(inPosition, inBuffer, inLength, outRcvLen);
    
	//否则,默认从公有缓冲块中读取数据
    return  this->ReadFromCache(inPosition, inBuffer, inLength, outRcvLen);
}


/* 极其核心的函数:从inPosition定位开始读取文件数据的buffer block,然后依序读取各buffer block的数据(buffIndex递增),直到读入指定长度inLength的数据,并存入指定缓存inBuffer.
   最后返回从所有buffer block中读取的总字节数outRcvLen */
OS_Error    OSFileSource::ReadFromCache(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{ 
    OSMutexLocker locker(&fMutex);
    
	/* 假如fFileMapArray=NULL,或者不能使用文件缓存 */
    if (!fFileMap.Initialized() || !fCacheEnabled)
    {   Assert(0);
    }
    
	/* 清空接收数据的长度,以便下次存放数据长度 */
    Assert(outRcvLen != NULL);
    *outRcvLen = 0;
   
   /* 假如已经读到文件尾 */
   if (inPosition >= fLength) // eof
        return OS_NoErr;

    SInt64 buffIndex = fFileMap.GetBuffIndex(inPosition);/* 缓存块的起始索引 */   
    SInt64 buffSize = 0;/* 指定buffIndex的buffer block的实际缓存数据大小 */
    SInt64 maxBuffSize = fFileMap.GetMaxBufSize();/* 得到每个缓存块的大小,注意这个量很重要,fFileMap.GetBuffIndex()要用到它 */
    SInt64 endIndex = fFileMap.GetBuffIndex(inPosition+inLength);/* 缓存块的终止索引endIndex */
    SInt64 maxIndex = fFileMap.GetMaxBuffIndex();/* 最大的缓存块索引 */
    SInt64 buffPos =  inPosition - fFileMap.GetBuffOffset(buffIndex);/* 读取数据位置在起始缓存块内的相对偏移 */
    SInt64 buffOffsetLen = 0;

	/* 指定buffer block的起始地址 */
    char *buffStart = NULL;
	/* 指定buffer block中已复制数据的长度 */
    SInt64 buffCopyLen = inLength;
	/* 要复制数据的总长度(逐次递减),并赋给buffCopyLen,见下面 */
    SInt64 bytesToCopy = inLength;
	/* 入参指定的存放数据的缓存 */
    char *buffOut = (char*)inBuffer;

	/* 参见FileMap::GetBuffer(),是否需要填充缓存?默认填充数据 */
    Bool16 fillBuff = true;
    char *buffOffset = NULL;
    
#if FILE_SOURCE_BUFFTEST
    char testBuff[inLength + 1];
    buffOut = (char*)testBuff;
    sBuffCount ++;
    ::memset(inBuffer,0,inLength);  
    ::memset(testBuff,0,inLength);
#endif
    
	/* 判断bufferIndex和endIndex的合法性 */
    if (buffIndex > endIndex || endIndex > maxIndex)
    {
#if FILE_SOURCE_DEBUG
        qtss_printf("OSFileSource::ReadFromCache bad index: buffIndex=%ld endIndex=%ld maxIndex=%ld\n",buffIndex,endIndex,maxIndex);
        qtss_printf("OSFileSource::ReadFromCache inPosition =%qu buffSize = %lu index=%ld\n",inPosition, fFileMap.GetMaxBufSize(),buffIndex);
#endif
        Assert(0);
    }
   
	/* 当当前buffer index合法时,循环复制各buffer block内的数据 */
   while (buffIndex <= endIndex && buffIndex <= maxIndex)
   {    
#if FILE_SOURCE_DEBUG
        qtss_printf("OSFileSource::ReadFromCache inPosition =%qu buffSize = %lu index=%ld\n",inPosition, fFileMap.GetMaxBufSize(),buffIndex);
#endif

		/* 首先通过新建或重用确保取得入参指定的buffer block,将其所含的队列元放在缓存块队列的尾部;再设置该buffer block的当前索引为buffIndex,并返回其内的data.
		注意第二个参数暗示是否需要填充数据?若重用该buffer block,就不需要填充数据,第二个入参为false.默认新建缓存块(true).一定要确保入参buffIndex和FileBlockBuffer->fArrayIndex相等 */
		/* 获得指定缓存块的起始地址,是否需要填充缓存? */
		buffStart = fFileMap.GetBuffer(buffIndex, &fillBuff);
        Assert(buffStart != NULL);
        
		/* 假如要事先填充缓存(默认值),就先定位文件位置,从指定文件处读取数据存入buffStart处,入参buffIndex用来定位文件读取位置 */
        if (fillBuff)
        {   /* 注意第一个参数没有用到 */
            OS_Error theErr = this->FillBuffer( (char *) inBuffer, (char *) buffStart, (SInt32) buffIndex);
            if (theErr != OS_NoErr)
                return theErr;
            
        }
        
        
		/* 得到指定buffIndex的buffer block的实际缓存数据大小 */
        buffSize = fFileMap.GetBuffSize(buffIndex);
		/* 得到要读取的媒体数据在该buffer block中的实际位置(数据偏移量为buffPos) */
        buffOffset = &buffStart[buffPos];
        
		/* 复制到媒体文件的末尾,循环即将退出 */
        if  (   (buffPos == 0) && 
                (bytesToCopy <= maxBuffSize) && /* 由此易见已经移到媒体文件的末尾  */
                (buffSize < bytesToCopy)
            ) // that's all there is in the file
        {         
            #if FILE_SOURCE_DEBUG
                qtss_printf("OSFileSource::ReadFromCache end of file reached buffIndex=%lu buffSize = %ld bytesToCopy=%lu\n",buffIndex, buffSize,bytesToCopy);
            #endif
            Assert(buffSize <= kUInt32_Max);
			/* 将最后一段不足最大缓存数据长度maxBuffSize的数据复制进来 */
            ::memcpy(buffOut,buffOffset,(UInt32) buffSize);
			/* 累计已更新的接收数据的长度 */
            *outRcvLen += (UInt32) buffSize;/* 这里只更新接收数据的长度而没有移动缓存的位置,由此易见已经移到媒体文件的末尾  */
            break; /* 退出循环 */
        }

		/* 计算指定buffIndex的buffer中除去偏移位置后剩下可以复制的缓存数据 */
        buffOffsetLen = buffSize - buffPos;
		/* 还没复制完,将剩下可以复制的缓存数据复制进入参inBuffer提供的缓存buffOut */
        if (buffCopyLen >= buffOffsetLen)
            buffCopyLen = buffOffsetLen;
            
        Assert(buffCopyLen <= buffSize);

        ::memcpy(buffOut,buffOffset, (UInt32) buffCopyLen);

		/* 同时更新存放数据缓存的指针 */
        buffOut += buffCopyLen;
		/* 累计实际接收数据的长度 */
        *outRcvLen += (UInt32) buffCopyLen;
		/* 更新还要复制的数据长度 */
        bytesToCopy -= buffCopyLen;
        Assert(bytesToCopy >= 0);
        
		/* 更新要复制的数据长度,移到下一个buffer,继续复制数据 */
        buffCopyLen = bytesToCopy;
        buffPos = 0;/* 在以后的缓存块中的数据偏移为0 */
		/* 移到下一个缓存块 */
        buffIndex ++;
            
    } 
    
#if FILE_SOURCE_DEBUG
        //qtss_printf("OSFileSource::ReadFromCache inLength= %lu *outRcvLen=%lu\n",inLength, *outRcvLen);
#endif

#if FILE_SOURCE_BUFFTEST    
    {   
		UInt32 outLen = 0;
        OS_Error theErr = this->ReadFromPos(inPosition, inBuffer, inLength, &outLen);       
        
        Assert(*outRcvLen == outLen);
        if (*outRcvLen != outLen)
            qtss_printf("OSFileSource::ReadFromCache *outRcvLen != outLen *outRcvLen=%lu outLen=%lu\n",*outRcvLen,outLen);
            
        for (int i = 0; i < inLength; i++)
        {   if ( ((char*)inBuffer)[i] != testBuff[i])
            {   qtss_printf("OSFileSource::ReadFromCache byte pos %d of %lu failed len=%lu inPosition=%qu sBuffCount=%ld\n",i,inLength,outLen,inPosition,sBuffCount);
                break;
            }
        }
    }
#endif

    return OS_NoErr;
}

/* used in OSFileSource::ReadFromPos() */
/* 利用lseek(),::read()C语言函数从硬盘中指定的文件处读取数据,存入指定缓存并设置实际读取数据的长度 */
OS_Error    OSFileSource::ReadFromDisk(void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{
    #if FILE_SOURCE_BUFFTEST
        qtss_printf("OSFileSource::Read inLength=%lu fFile=%d\n",inLength,fFile);
    #endif

   /* 将文件指针设置为距离文件开头偏移为fPosition处,若出错,就返回错误信息 */
    if (lseek(fFile, fPosition, SEEK_SET) == -1)
		return OSThread::GetErrno();

    /* 从文件开头读取指定长度的文件数据到指定缓存处 */    
    int rcvLen = ::read(fFile, (char*)inBuffer, inLength);
	/* 若出错,就返回错误 */
    if (rcvLen == -1)
        return OSThread::GetErrno();

	/* 设置读取文件数据的长度 */
    if (outRcvLen != NULL)
        *outRcvLen = rcvLen;

	/* 及时更新读取文件数据的位置 */
    fPosition += rcvLen;
    fReadPos = fPosition;
    
    return OS_NoErr;
}

/* 利用入参inPosition先定位要查找文件的位置,再使用OSFileSource::ReadFromDisk()从指定的文件处读取数据,存入指定缓存并设置实际读取数据的长度 */
OS_Error    OSFileSource::ReadFromPos(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{   
#if TEST_TIME
    {   
        startTime = OS::Milliseconds();
        sReadCount++;
        if (outRcvLen)
            *outRcvLen = 0;
        qtss_printf("OSFileSource::Read sReadCount = %ld totalbytes=%ld readsize=%lu\n",sReadCount,sByteCount,inLength);
    }
#endif

	/* 定位查找文件的位置,就是设置fPosition=inPosition */
    this->Seek(inPosition);
	/* 利用::read()C语言函数从指定的文件处读取数据,存入缓存并返回读取数据的长度 */
    OS_Error err =  this->ReadFromDisk(inBuffer,inLength,outRcvLen);
  
/* 及时记录读文件的日志 */
#if READ_LOG
        if (fFileLog)
        {   fFileLog = ::fopen(fFilePath,"a");
            if (fFileLog)
            {   qtss_fprintf(fFileLog, "read: %qu %lu\n",inPosition, *outRcvLen);
                ::fclose(fFileLog);
            }
        }
            
#endif

/* 记录读取文件所耗时间,及读到的字节数 */
#if TEST_TIME
    {
        durationTime += OS::Milliseconds() - startTime;
        sByteCount += *outRcvLen;
    }
#endif

    return err;
}

void OSFileSource::SetTrackID(UInt32 trackID)   
{ 
#if READ_LOG
    fTrackID = trackID;
//  qtss_printf("OSFileSource::SetTrackID = %lu this=%lu\n",fTrackID,(UInt32) this);
#endif
}


/* 关闭文件,设置相关文件参数 */
void    OSFileSource::Close()
{
	/* 假如文件已打开并且要关闭,就关闭文件,并关闭日志记录文件 */
    if ((fFile != -1) && (fShouldClose))
    {   ::close(fFile);
    
        #if READ_LOG
            if ( 0 && fFileLog != NULL )
            {   ::fclose(fFileLog);
                fFileLog = NULL;
                fFilePath[0] =0;
            }
        #endif
    }
    
    fFile = -1;
    fModDate = 0;
    fLength = 0;
    fPosition = 0;
    fReadPos = 0;
    
#if TEST_TIME   
    if (fShouldClose)
    {   sMovie = 0;
//      qtss_printf("OSFileSource::Close sReadCount = %ld totalbytes=%ld\n",sReadCount,sByteCount);
//      qtss_printf("OSFileSource::Close durationTime = %qd\n",durationTime);
    }
#endif
    
}
