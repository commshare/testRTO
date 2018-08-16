
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


#ifndef __OSFILE_H_
#define __OSFILE_H_

#include <stdio.h>
#include <time.h>

#include "OSHeaders.h"
#include "StrPtrLen.h"
#include "OSQueue.h"

/* 记录读文件的日志的宏开关 */
#define READ_LOG 0

class FileBlockBuffer 
{

 public:
    FileBlockBuffer(): fArrayIndex(-1),fBufferSize(0),fBufferFillSize(0),fDataBuffer(NULL),fDummy(0){}
    ~FileBlockBuffer(void);


    void AllocateBuffer(UInt32 buffSize);
    void TestBuffer(void);

	/* 清空数据缓存 */
    void CleanBuffer() { ::memset(fDataBuffer,0, fBufferSize); }

	/* used in class FileMap */
    void SetFillSize(UInt32 fillSize) {fBufferFillSize = fillSize;}
	/* used in FileMap::GetBufSize() */
    UInt32 GetFillSize(void) { return fBufferFillSize;}

    OSQueueElem *GetQElem() { return &fQElem; }


	/* FileMap.fFileMapArray数组中的指针分量的索引(根据具体文件长度会有变化) */
    SInt64              fArrayIndex;
    UInt32              fBufferSize;
    UInt32              fBufferFillSize;
	/* 存放实际数据的缓存部分指针,其长度为fBufferSize */
    char                *fDataBuffer;
    OSQueueElem         fQElem;
    UInt32              fDummy;
};



class FileBlockPool
{
    enum 
	{
            kDataBufferUnitSizeExp      = 15,// base 2 exponent
            kBufferUnitSize             = (1 << kDataBufferUnitSizeExp ) /* 15*2=30 */    // 32Kbytes /* 2^15=(2^5)*2^10 */
    };

    public:
        FileBlockPool(void) :  fMaxBuffers(1),  fNumCurrentBuffers(0), fBufferUnitSizeBytes(kBufferUnitSize){}
        ~FileBlockPool(void);
        
		//modifiers

		/* 设置buffer块的最大个数 */
        void SetMaxBuffers(UInt32 maxBuffers) { if (maxBuffers > 0) fMaxBuffers = maxBuffers; }

		/* 设置buffer块的个数的增量值 */
        void SetBuffIncValue(UInt32 bufferInc) { if (bufferInc > 0) fBufferInc = bufferInc;}

		/* 增减buffer最大个数,参见下面的FileMap,OSFileSource中的引用 */
		/* 用增量值来调整最大buffer个数 */
        void IncMaxBuffers(void) { fMaxBuffers += fBufferInc; }
        void DecMaxBuffers(void) { if (fMaxBuffers > fBufferInc) fMaxBuffers-= fBufferInc; }
		/* 将当前buffer block个数减一 */
        void DecCurBuffers(void) { if (fNumCurrentBuffers > 0) fNumCurrentBuffers--; }
        
		/* 设置缓存单元大小为入参*1024 bytes */
        void SetBufferUnitSize  (UInt32 inUnitSizeInK)      { fBufferUnitSizeBytes = inUnitSizeInK * 1024; }

		//accessors

		/* 获取缓存单元大小(单位为bytes) */
        UInt32 GetBufferUnitSizeBytes()     { return fBufferUnitSizeBytes; }//32K Bytes
		/* 得到buffer的最大个数 */
        UInt32 GetMaxBuffers(void)  { return fMaxBuffers; }
		/* 获得缓存大小的增量 */
        UInt32 GetIncBuffers()      { return fBufferInc; }
		/* used in FileMap::DeleteOldBuffs() */
		/* 获取当前缓存的个数 */
        UInt32 GetNumCurrentBuffers(void)   { return fNumCurrentBuffers; }

		//Utilities

		/* 循环删除队列元素,极其依附的文件缓存块,并作相关参数设置 */
        void DeleteBlockPool();
        /* 调用FileBlockBuffer::AllocateBuffer(),得到指定大小的缓存块并位于队列尾部 */
        FileBlockBuffer* GetBufferElement(UInt32 bufferSizeBytes);
		/* 使即将要读取数据的缓存块位于队列尾部 */
        void MarkUsed(FileBlockBuffer* inBuffPtr);

    private:
        OSQueue fQueue;
        UInt32  fMaxBuffers;/* buffer最大个数限制 */
        UInt32  fNumCurrentBuffers; /* 当前buffer的个数 */ 
        UInt32  fBufferInc; /* 增加/减少buffer大小的增量值 */
        UInt32  fBufferUnitSizeBytes;/* buffer unit大小为32K bytes */
        UInt32  fBufferDataSizeBytes;//8*32Kbytes,这就是说,有8个这样的buffer unit,它们的数据总大小

};

class FileMap 
{

    public:
        FileMap(void):fFileMapArray(NULL),fDataBufferSize(0),fMapArraySize(0),fNumBuffSizeUnits(0) {}
        ~FileMap(void) {fFileMapArray = NULL;}

		/***************** 本类中比较重要的2个函数 ****************/
		/* 参见OSFileSource::AllocateFileCache(),其有默认的入参设置 */
		/* 根据文件长度设置缓存块个数,给媒体文件分配恰当大小的一个缓存块指针的数组结构,并将这些指针清零 */
        void    AllocateBufferMap(UInt32 inUnitSizeInK, UInt32 inNumBuffSizeUnits, UInt32 inBufferIncCount, UInt32 inMaxBitRateBuffSizeInBlocks, UInt64 fileLen, UInt32 inBitRate);
        char*   GetBuffer(SInt64 bufIndex, Bool16 *outIsEmptyBuff);

		/* 实际使用FileBlockBuffer::TestBuffer() */
        void    TestBuffer(SInt32 bufIndex) {Assert (bufIndex >= 0); fFileMapArray[bufIndex]->TestBuffer();};

		/* 为指定buffIndex的buffer设置入参指定的填充大小FillSize,used in OSFileSource::FillBuffer() */
        void    SetIndexBuffFillSize(SInt32 bufIndex, UInt32 fillSize) { Assert (bufIndex >= 0); fFileMapArray[bufIndex]->SetFillSize(fillSize);}

		/* 得到每个缓存块大小,注意这个量很重要,fFileMap.GetBuffIndex()要用到它 */
        UInt32  GetMaxBufSize(void) {return fDataBufferSize;}

		/* 得到指定索引的数据缓存的实际填充大小,实际使用FileBlockBuffer::GetFillSize() */
        UInt32  GetBuffSize(SInt64 bufIndex)    { Assert (bufIndex >= 0); return fFileMapArray[bufIndex]->GetFillSize(); }

        UInt32  GetIncBuffers(void) { return fBlockPool.GetIncBuffers(); }
        void    IncMaxBuffers()     {fBlockPool.IncMaxBuffers(); }
        void    DecMaxBuffers()     {fBlockPool.DecMaxBuffers(); }

		/* 判断是否初始化?used in AllocateBufferMap() */
        Bool16  Initialized()       { return fFileMapArray == NULL ? false : true; }
		/* 初始化缓存块指针数组,置0(清0) */
        void    Clean(void);
		/* 删除缓存块指针数组,注意仅是将每个指针分量置空 */
        void    DeleteMap(void);
        void    DeleteOldBuffs(void);

		/* used in OSFileSource::ReadFromCache() */
		/* 由硬盘上媒体文件的给定位置得到复制进内存中的所在缓存块的索引 */
        SInt64  GetBuffIndex(UInt64 inPosition) {   return inPosition / this->GetMaxBufSize();  }
		/* 得到File Map array的分量个数-1 */
		/* 得到最大的bufferIndex,由具体文件长度决定 */
        SInt64  GetMaxBuffIndex() { Assert(fMapArraySize > 0); return fMapArraySize -1; }
		/* used in OSFileSource::ReadFromCache() */
		/* 得到指定索引的缓存块的数据偏移 */
        UInt64  GetBuffOffset(SInt64 bufIndex) { return (UInt64) (bufIndex * this->GetMaxBufSize() ); }

        //data members


		//用到前两个类:FileBlockBuffer,FileBlockPool
        /* 这里提供该对象是便于从缓存池角度操作缓存块 */
        FileBlockPool fBlockPool;

        FileBlockBuffer**   fFileMapArray;
    
    private:

	/* 每个数据缓冲(数组分量)的最大字节数(以最大数据缓存的大小计算),而它的实际大小为数据填充大小,
	它反映了媒体文件的缓冲率大小(比如256K) */
    UInt32              fDataBufferSize;/* 媒体文件分块大小 */
	/* 数组fFileMapArray的分量个数 */
    SInt64              fMapArraySize;
	/* 比特率和缓存块大小的比值,比如256K/32K=8,最大值可能为8,最小值为1,
	它和fBufferUnitSizeBytes共同决定了fDataBufferSize的大小,参见 FileMap::AllocateBufferMap() */
    UInt32              fNumBuffSizeUnits;/*文件缓存块个数*/
    
};

/* 注意该类只使用FileMap类 */
class OSFileSource
{
    public:
    
        OSFileSource() :    fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false)/* 是目录文件吗? */, fCacheEnabled(false)/* 默认不设置文件缓存 */                       
        {
        
        #if READ_LOG 
            fFileLog = NULL;
            fTrackID = 0;
            fFilePath[0]=0;
        #endif
        
        }
                
        OSFileSource(const char *inPath) :  fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false),fCacheEnabled(false)
        {
         Set(inPath); /* 在Windows xp下,这个函数一定要经过改写 */
         
        #if READ_LOG 
            fFileLog = NULL;
            fTrackID = 0;
            fFilePath[0]=0;
        #endif      
         
         }
        
        ~OSFileSource() { Close();  fFileMap.DeleteMap();}
        
        //Sets this object to reference this file
		/* 以只读方式打开指定路径的媒体文件,并设置文件相应参数信息 */
        void            Set(const char *inPath);
        
        // Call this if you don't want Close or the destructor to close the fd
		/* 不要关闭文件(就是不要关闭文件描述符) */
        void            DontCloseFD() { fShouldClose = false; }
        
        //Advise: this advises the OS that we are going to be reading soon from the
        //following position in the file
		/* 告诉OS马上就要在文件指定位置开始读入数据 */
        void            Advise(UInt64 advisePos, UInt32 adviseAmt);
        
		/******************************** 本类最核心的几个读取文件数据函数 ************************************************/

        OS_Error    Read(void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL)
                    {   return ReadFromDisk(inBuffer, inLength, outRcvLen);
                    }
                    
        OS_Error    Read(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromDisk(void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromCache(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromPos(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);

		/********************************************************************************************************************/

		/* 通过入参设置文件是否可以缓存? */
        void        EnableFileCache(Bool16 enabled) {OSMutexLocker locker(&fMutex); fCacheEnabled = enabled; }
		/* 查询文件是否可以缓存? */
        Bool16      GetCacheEnabled() { return fCacheEnabled; }


		/*********************************** 密切注意该函数,十分重要! ************************/
		/* 根据文件长度设置缓存块个数,给媒体文件分配恰当大小的一个缓存块指针的数组结构,并将这些指针清零 */
        void        AllocateFileCache(UInt32 inUnitSizeInK = 32, UInt32 bufferSizeUnits = 0, UInt32 incBuffers = 1, UInt32 inMaxBitRateBuffSizeInBlocks = 8, UInt32 inBitRate = 32768 /* 2^15 */) 
                    {   
						fFileMap.AllocateBufferMap(inUnitSizeInK, bufferSizeUnits,incBuffers, inMaxBitRateBuffSizeInBlocks, fLength, inBitRate);
                    } 

		/* 增减最大文件缓存块的个数 */
        void        IncMaxBuffers()     {OSMutexLocker locker(&fMutex); fFileMap.IncMaxBuffers(); }
        void        DecMaxBuffers()     {OSMutexLocker locker(&fMutex); fFileMap.DecMaxBuffers(); }

		/************************************* 密切注意该函数,十分重要! ****************************/
        OS_Error    FillBuffer(char* ioBuffer, char *buffStart, SInt32 bufIndex);
        
		/* 关闭文件,设置相关文件参数 */
        void            Close();

		//accessor

		/* 得到文件最后修改时间, used in QTFile::GetModDate() */
        time_t          GetModDate()                { return fModDate; }
		/* 得到文件长度,used in QTFile::ValidTOC() */
        UInt64          GetLength()                 { return fLength; }
        UInt64          GetCurOffset()              { return fPosition; }
		/* 定位查找文件的位置,used in OSFileSource::ReadFromPos() */
        void            Seek(SInt64 newPosition)    { fPosition = newPosition;  }

		/* used in QTFile_FileControlBlock::IsValid(),文件描述符不为-1才是valid */
        Bool16 IsValid()                            { return fFile != -1;       }
		/* 是目录文件吗? */
        Bool16 IsDir()                              { return fIsDir; }
        
        // For async I/O purposes
		/* 获得文件描述符 */
        int             GetFD()                     { return fFile; }
        void            SetTrackID(UInt32 trackID);
        // So that close won't do anything
		/* 将文件描述符置为非法,即关闭文件 */
        void ResetFD()  { fFile=-1; }
        
		/* 设置日志文件路径,打开日志文件,记录相关信息后再关闭该日志文件 */
        void SetLog(const char *inPath);
    
    private:

		/* 文件描述符 */
        int     fFile;
		/* 文件长度 */
        UInt64  fLength;
		/* 文件当前偏移位置(当文件读入后就是一个固定量) */
        UInt64  fPosition;
		/* 文件读取处的位置,参见OSFileSource::ReadFromDisk() */
        UInt64  fReadPos;

		/* 应该关闭文件吗? */
        Bool16  fShouldClose;
		/* 是目录文件吗? */
        Bool16  fIsDir;

		/* 媒体文件的修改时间,used in QTFile::GetModDate */
        time_t  fModDate;
        
        
        OSMutex fMutex;

		/* 注意此处用到上面的FileMap类 */
        FileMap fFileMap;

		/* 能否缓存文件? */
        Bool16  fCacheEnabled;
#if READ_LOG
        FILE*               fFileLog;/* 记录读文件日志的文件描述符 */
        char                fFilePath[1024];/* 该日志文件的路径 */
        UInt32              fTrackID;
#endif

};

#endif //__OSFILE_H_
