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


/* ��������ص��Եĺ꿪�� */
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

/* ������־�ļ�·��,����־�ļ�,��¼�����Ϣ���ٹرո���־�ļ� */
void OSFileSource::SetLog(const char *inPath)
{
    fFilePath[0] =0;
    ::strcpy(fFilePath,inPath);
    
	/* ��media�ļ��Ѵ���û����־�ļ�,������־�ļ���·������׷�ӷ�ʽ��,�ںϷ�״̬�¼�¼��Ӧ���ļ���־ */
    if (fFile != -1 && fFileLog == NULL)
    {
        ::strcat(fFilePath,inPath);/* ע���·����¼������ */
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

	/* ��ӡ�򵥵���Ϣ */
#if FILE_SOURCE_DEBUG
    qtss_printf("OSFileSource::SetLog=%s\n",inPath);
#endif
    
}
#endif


/* ɾ�����ݻ��� */
FileBlockBuffer::~FileBlockBuffer(void)
{
    if (fDataBuffer != NULL)
    {
		/* ȷ���ļ����û��Խ��? */
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
/* ����ָ����С��buffer,���һ���������һ��������0 */
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

/* ���buffer���������һ�������Ƿ���0 ?�Դ˿��ļ������Ƿ�����? */
void FileBlockBuffer::TestBuffer(void)
{

#if FILE_SOURCE_BUFFTEST    
    if (fDataBuffer != NULL)
        Assert (fDataBuffer[fBufferSize] == 0); 
#endif

}

/* ʹ����Ҫ��ȡ���ݵĻ����λ�ڶ���β�� */ 
void FileBlockPool::MarkUsed(FileBlockBuffer* inBuffPtr)
{
    /* 	���ǿջ����,������������ */
    if (NULL == inBuffPtr)
        return;
    
	/* ��������β���������ָ����buffer��,�Ͱ����Ӷ������Ƶ�����β�� */
    if (fQueue.GetTail() != inBuffPtr->GetQElem()) // Least Recently Used tail is last accessed
    {
        fQueue.Remove(inBuffPtr->GetQElem());
        fQueue.EnQueue(inBuffPtr->GetQElem()); // put on tail
    }
}  

/* used in FileMap::DeleteOldBuffs() */
/* ����ǰbuffer block����û�г������buffer block����ʱ,�½�һ��FileBlockBuffer,�������ָ����С��buffer,�����ڶ��е�β��,�����ظ�FileBlockBuffer; 
   ����,���Ѿ�����󻺴��ʱ,ɾ�������ض���ͷԪ��,��������Ԫ�ص����ڶ��󷵻� */
FileBlockBuffer *FileBlockPool::GetBufferElement(UInt32 bufferSizeBytes)
{
    FileBlockBuffer* theNewBuf = NULL;

	/* ����ǰbuffer block����û�г������buffer block����ʱ,�½�һ��FileBlockBuffer,�������ָ����С��buffer,�����ڶ��е�β��,�����ظ�FileBlockBuffer; */
    if ( fNumCurrentBuffers < fMaxBuffers)
    {
#if FILE_SOURCE_DEBUG
            qtss_printf("FileBlockPool::GetBufferElement NEW element fNumCurrentBuffers=%lu fMaxBuffers=%lu fBufferUnitSizeBytes=%lu bufferSizeBytes=%lu\n",fNumCurrentBuffers,fMaxBuffers,fBufferUnitSizeBytes,bufferSizeBytes);
#endif  /* ����һ����������FileBlockBuffer */
        theNewBuf = NEW FileBlockBuffer();
        theNewBuf->AllocateBuffer(bufferSizeBytes);//����256K=8*32K�Ļ���
        fNumCurrentBuffers++;
        theNewBuf->fQElem.SetEnclosingObject(theNewBuf);
        fQueue.EnQueue(theNewBuf->GetQElem()); // put on tail
        Assert(theNewBuf != NULL);
        return theNewBuf;//�����½��Ļ����
    }
    
	/* ���Ѿ�����󻺴��ʱ,ɾ�������ض���ͷԪ�� */
    OSQueueElem *theElem = fQueue.DeQueue(); // get head

    Assert(theElem != NULL);
    
    if (theElem == NULL)
        return NULL;
     
	/* ������Ԫ�ص����ڶ�����ΪtheNewBuf */
    theNewBuf = (FileBlockBuffer*) theElem->GetEnclosingObject();
    Assert(theNewBuf != NULL);
    //qtss_printf("FileBlockPool::GetBufferElement reuse buffer theNewBuf=%lu fDataBuffer=%lu fArrayIndex=%ld\n",theNewBuf,theNewBuf->fDataBuffer,theNewBuf->fArrayIndex);

    return theNewBuf;

}  
 
/* ѭ��ɾ������Ԫ��,�����������ļ������,������ز������� */
void FileBlockPool::DeleteBlockPool(void) 
{

    FileBlockBuffer *buffer = NULL;
	/* ɾ�������ض���ͷԪ�� */
    OSQueueElem* theElem = fQueue.DeQueue();
    while (theElem != NULL)
    {   
		/* ɾ����ɾ������Ԫ�ص��������� */
		buffer = (FileBlockBuffer *) theElem->GetEnclosingObject();
        delete buffer;
		/* ��ɾ�������ض���ͷԪ�� */
        theElem = fQueue.DeQueue(); 
    }
    
	/* ����ز������� */
    fMaxBuffers = 1;
    fNumCurrentBuffers = 0; 
    fBufferUnitSizeBytes = kBufferUnitSize;//32Kbytes
}

FileBlockPool::~FileBlockPool(void) 
{

    this->DeleteBlockPool();
}

/* �ǳ���Ҫ��һ������,ֵ���!used in OSFileSource::AllocateFileCache() */
/* �����ļ��������û�������,��ý���ļ�����ǡ����С��һ�������ָ�������ṹ,������Щָ������ */
void FileMap::AllocateBufferMap(UInt32 inUnitSizeInK/*64*/, UInt32 inNumBuffSizeUnits/*1*/, UInt32 inBufferIncCount/*8*/, UInt32 inMaxBitRateBuffSizeInBlocks/*8*/, UInt64 fileLen/* ���ݾ����ļ����� */, UInt32 inBitRate/*���ݾ����ļ����������*/)
{
    
	/* ������ڻ���ӳ��,ֻ��һ�������,������󻺴����8��,��ֱ�ӷ��� */
    if (fFileMapArray != NULL && fNumBuffSizeUnits == inNumBuffSizeUnits && inBufferIncCount == fBlockPool.GetMaxBuffers())
        return;
        
	/* �ļ����浥Ԫ��С��1K�ֽ�,һ����32K�ֽ� */
    if( inUnitSizeInK < 1 )
        inUnitSizeInK = 1;
        
	/* ��������û��浥Ԫ��С, Ĭ��32Kbytes,�˴���64K�ֽ� */
    fBlockPool.SetBufferUnitSize(inUnitSizeInK);
    
	/* ����ý���ļ������ʴ�С,�����ֵ���� */
    if (inBitRate == 0) // just use the maximum possible size
        inBitRate = inMaxBitRateBuffSizeInBlocks * fBlockPool.GetBufferUnitSizeBytes(); /* ���Ϊ8*64K */
    
	/* �ֶ�����inNumBuffSizeUnitsΪ�����ʺͻ�����С�ı�ֵ,���ֵ����Ϊ8,��СֵΪ1  */
    if (inNumBuffSizeUnits == 0) // calculate the buffer size ourselves
    {
        inNumBuffSizeUnits = inBitRate / fBlockPool.GetBufferUnitSizeBytes();/* ����32K */
        
        if( inNumBuffSizeUnits > inMaxBitRateBuffSizeInBlocks) // max is 8 * buffUnit Size (32k) = 256K
        {   inNumBuffSizeUnits = inMaxBitRateBuffSizeInBlocks;
        }
    } //else the inNumBuffSizeUnits is explicitly defined so just use that value
    
    if( inNumBuffSizeUnits < 1 )
        inNumBuffSizeUnits = 1;
     
	/* ɾ�������ָ������,ע����ǽ�ÿ��ָ������ÿ� */
    this->DeleteMap(); 
	/* ѭ��ɾ������Ԫ��,�����������ļ������,������ز������� */
    fBlockPool.DeleteBlockPool();
    
	/* ע��˴���ֵ */
    fNumBuffSizeUnits = inNumBuffSizeUnits;//1
	/* ý���ļ��ֿ��С,�������ú���Ҫ(����������) */
    fDataBufferSize = fBlockPool.GetBufferUnitSizeBytes() * inNumBuffSizeUnits; //1*32Kbytes
    
	/* ������󻺴�������8 */
    fBlockPool.SetMaxBuffers(inBufferIncCount);
	/* ����buffer������8 */
    fBlockPool.SetBuffIncValue(inBufferIncCount);

	/* �����ļ��������û�������,�����1���Ǳ���������������� */
    fMapArraySize = (fileLen / fDataBufferSize) + 1;
	/* ������ΪFileBlockBuffer *��������仺��,ע���������Ϊָ�� */
    fFileMapArray = NEW FileBlockBuffer *[ (SInt32) (fMapArraySize + 1) ];
    
	/* ��ʼ�������ָ������,��0(��0) */
	this->Clean(); // required because fFileMapArray's array is used to store buffer pointers.
#if FILE_SOURCE_DEBUG
    qtss_printf("FileMap::AllocateBufferMap shared buffers fFileMapArray=%lu fDataBufferSize= %lu fMapArraySize=%lu fileLen=%qu \n",fFileMapArray, fDataBufferSize, fMapArraySize,fileLen);   
#endif

}    


/* ��������е�ǰ��buffer block������������buffer block����ʱ,��ɾȥһЩ�ɵ�buffer block,����ʱ����buffer block�ܸ��� */
void FileMap::DeleteOldBuffs()
{
	/* ����ǰ��buffer block������������buffer block����ʱ,��ɾȥһЩ�ɵ�buffer block */
    while (fBlockPool.GetNumCurrentBuffers() > fBlockPool.GetMaxBuffers()) // delete any old buffers
    {
		/* ɾ������ͷԪ�ز�����������buffer block���� */
		FileBlockBuffer *theElem =  fBlockPool.GetBufferElement(fDataBufferSize);
        fFileMapArray[theElem->fArrayIndex] = NULL; /* ���û����ָ���ÿ� */
        delete theElem;/* ɾ����buffer block */		
        fBlockPool.DecCurBuffers();/* ����ǰbuffer block������һ */
    }
}   


/* used in OSFileSource::ReadFromCache() */
/* ����ͨ���½�������ȷ��ȡ�����ָ����buffer block,���������Ķ���Ԫ���ڻ������е�β��;�����ø�buffer block�ĵ�ǰ����ΪbuffIndex,���������ڵ�data.
   ע��ڶ���������ʾ�Ƿ���Ҫ�������?�����ø�buffer block,�Ͳ���Ҫ�������,�ڶ������Ϊfalse.Ĭ���½������(true).һ��Ҫȷ�����buffIndex��FileBlockBuffer->fArrayIndex��� */
/* ʵ��������fBlockPool.GetBufferElement(fDataBufferSize)���õ��½��Ļ��� */
char *FileMap::GetBuffer(SInt64 buffIndex, Bool16 *outFillBuff)
{
    Assert(outFillBuff != NULL);
	/* �������ֵ���ٷ��� */
	/* �ݶ���һ�����ָ���Ļ����Ҫ������� */
    *outFillBuff = true; // we are re-using or just created a buff

	/* ��������е�ǰ��buffer block������������buffer block����ʱ,��ɾȥһЩ�ɵ�buffer block,����ʱ����buffer block���� */
    this->DeleteOldBuffs();
	/* ָ����������С�ڻ����ָ��������ܸ���(ע��:���ǻ���صĻ�������) */
    Assert(buffIndex < (SInt32) fMapArraySize);
    
	/* ȡ�����ָ��������FileBlockBuffer��ָ��,һ��Ҫ�ж����Ƿ����? */
	/* ����û�з��乫�л���,��fFileMapArrayΪ�� */
    FileBlockBuffer *theElem = fFileMapArray[buffIndex];
	/* ����ָ��������FileBlockBuffer������,���½�һ��ָ����С(32*8Kbytes)��buffer�����ڶ�β,����ȡ��������ͷ���Ļ���� */
    if ( NULL == theElem)
    {
        #if FILE_SOURCE_DEBUG
            qtss_printf("FileMap::GetBuffer call fBlockPool.GetBufferElement(); buffIndex=%ld\n",buffIndex);
        #endif
        
		/* ����ǰbuffer block����û�г������buffer block����ʱ,�½�һ��FileBlockBuffer,�������ָ����С��buffer,�����ڶ��е�β��,�����ظ�FileBlockBuffer; 
		����,���Ѿ�����󻺴��ʱ,ɾ�������ض���ͷԪ��,��������Ԫ�ص����ڶ��󷵻� */
         theElem =  fBlockPool.GetBufferElement(fDataBufferSize);
         Assert(theElem);
    }
    
	/* �������������еĻ��������λ��,ȷ������Ҫ��ȡ���ݵ�FileBlockBuffer�����Ķ���Ԫ���ڶ��е�β�� */
    fBlockPool.MarkUsed(theElem); // must happen here after getting a pre-allocated or used buffer.

	/* ��������,ֱ�ӷ������е�data. */
    if (theElem->fArrayIndex == buffIndex) // found a pre-allocated and filled buffer
    {
        #if FILE_SOURCE_DEBUG
            qtss_printf("FileMap::GetBuffer pre-allocated buff buffIndex=%ld\n",buffIndex);
        #endif
        
        *outFillBuff = false;
        return theElem->fDataBuffer;
    }

	/* �������û������� */
    if (theElem->fArrayIndex >= 0)
    {
        fFileMapArray[theElem->fArrayIndex] = NULL; // reset the old map location
    }


	/* ȷ����ȡ�õ�buffer block�����������������buffIndex */
	/* ���ݶ�������(���buffIndex)������ӦԪ��theElem��λ�� */
    fFileMapArray[buffIndex] = theElem; // a new buffer
    theElem->fArrayIndex = buffIndex; // record the index
    
#if FILE_SOURCE_DEBUG
    theElem->CleanBuffer();
#endif
    
	//ע�ⷵ�ػ����ַʱ,�ڶ������*outFillBuff = true,��ʾ��Ҫ�������
    return theElem->fDataBuffer;
    
}


/* ��ʼ�������ָ������,��0(��0) */
void    FileMap::Clean(void)
{
    if (fFileMapArray != NULL)
        ::memset( (char *)fFileMapArray,0, (SInt32) (sizeof(FileBlockBuffer *) * fMapArraySize) );
}

/* ɾ�������ָ������,ע����ǽ�ÿ��ָ������ÿ� */
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

 
/* used in QTFile::Open(), ��ֻ����ʽ��ָ��·����ý���ļ�,����������,�����ļ���Ӧ������Ϣ */
void OSFileSource::Set(const char *inPath)
{
    Close();
 
/* ��ֻ����ʽ�����ָ��·�����ļ� */
    fFile = open(inPath, O_RDONLY | O_LARGEFILE);

	/* �����ļ��ܹ��ɹ��򿪲���ȡ���ͳ����Ϣ,�������ļ���Ӧ������Ϣ,��¼��־��Ϣ,����رմ򿪵��ļ� */
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
/* ע���һ������û���õ�.�ȶ�λҪ�����ļ���λ��,��ʹ��OSFileSource::ReadFromDisk()��Ӳ����ָ�����ļ�λ�ö�ȡ����,����ָ�����沢���øû��������С,�����(�û�����Ƿ�����)���һ�������Ƿ���0 ? */
OS_Error    OSFileSource::FillBuffer(char* ioBuffer, char *buffStart, SInt32 buffIndex)
{
	/* ��ȡÿ��buffer block��С�����ֵ */
    UInt32 buffSize = fFileMap.GetMaxBufSize();
	/* �õ�ָ��buffer��offset,����λӲ����Ҫ�����ļ���λ��(ע���������ÿ��buffer����������,�Ӷ�ͨ�����㻺�����������������ȷ��Ҫ��ȡ�ļ���λ��!) */
    UInt64 startPos = buffIndex * buffSize;
    UInt32 readLen = 0;
    
	/* �������startPos�ȶ�λҪ�����ļ���λ��,��ʹ��OSFileSource::ReadFromDisk()��ָ�����ļ�����ȡ����,����ָ�����沢����ʵ�ʶ�ȡ���ݵĳ���readLen */
    OS_Error theErr = this->ReadFromPos(startPos, buffStart, buffSize, &readLen);   

	/* Ϊָ��buffIndex��buffer�������ָ��������СFillSize */
    fFileMap.SetIndexBuffFillSize(buffIndex, readLen);

	/* �ú���û�ж��� */
    /* �ڵ���ʱ,����buffer����((�û�����Ƿ�����))�����һ�������Ƿ���0 ? */
    fFileMap.TestBuffer(buffIndex); 
                
    return theErr;
}

#if FILE_SOURCE_BUFFTEST
static SInt32 sBuffCount = 1;   
#endif

/* needed in QTFile::Read()/QTFile_FileControlBlock::Read()/QTFile_FileControlBlock::ReadInternal() */
/* �����������ʹ��OSFileSource::ReadFromPos()ֱ�ӴӴ��̶�ȡý���ļ�����,�������Ĭ��ʹ��OSFileSource::ReadFromCache(),�ӻ�����ȡ,����ָ��λ��inPosition��
ý�����ݶ���ָ�����ȵ�ָ��buffer,����¼ʵ�ʸ������ݵĳ��� */
OS_Error    OSFileSource::Read(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{ 
    /* �������������: */    
    if  ( ( !fFileMap.Initialized() )/* ����û�з���˽�л���,ʹ��fFileMapArray=NULL */
            || ( !fCacheEnabled ) /* ���粻�ܻ����ļ� */
            || ( fFileMap.GetBuffIndex(inPosition+inLength) > fFileMap.GetMaxBuffIndex() ) /* ����ý������λ�ó�������ļ����������� */
        )//ֱ�ӴӴ��̶�ȡý���ļ�����
        return  this->ReadFromPos(inPosition, inBuffer, inLength, outRcvLen);
    
	//����,Ĭ�ϴӹ��л�����ж�ȡ����
    return  this->ReadFromCache(inPosition, inBuffer, inLength, outRcvLen);
}


/* ������ĵĺ���:��inPosition��λ��ʼ��ȡ�ļ����ݵ�buffer block,Ȼ�������ȡ��buffer block������(buffIndex����),ֱ������ָ������inLength������,������ָ������inBuffer.
   ��󷵻ش�����buffer block�ж�ȡ�����ֽ���outRcvLen */
OS_Error    OSFileSource::ReadFromCache(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{ 
    OSMutexLocker locker(&fMutex);
    
	/* ����fFileMapArray=NULL,���߲���ʹ���ļ����� */
    if (!fFileMap.Initialized() || !fCacheEnabled)
    {   Assert(0);
    }
    
	/* ��ս������ݵĳ���,�Ա��´δ�����ݳ��� */
    Assert(outRcvLen != NULL);
    *outRcvLen = 0;
   
   /* �����Ѿ������ļ�β */
   if (inPosition >= fLength) // eof
        return OS_NoErr;

    SInt64 buffIndex = fFileMap.GetBuffIndex(inPosition);/* ��������ʼ���� */   
    SInt64 buffSize = 0;/* ָ��buffIndex��buffer block��ʵ�ʻ������ݴ�С */
    SInt64 maxBuffSize = fFileMap.GetMaxBufSize();/* �õ�ÿ�������Ĵ�С,ע�����������Ҫ,fFileMap.GetBuffIndex()Ҫ�õ��� */
    SInt64 endIndex = fFileMap.GetBuffIndex(inPosition+inLength);/* ��������ֹ����endIndex */
    SInt64 maxIndex = fFileMap.GetMaxBuffIndex();/* ���Ļ�������� */
    SInt64 buffPos =  inPosition - fFileMap.GetBuffOffset(buffIndex);/* ��ȡ����λ������ʼ������ڵ����ƫ�� */
    SInt64 buffOffsetLen = 0;

	/* ָ��buffer block����ʼ��ַ */
    char *buffStart = NULL;
	/* ָ��buffer block���Ѹ������ݵĳ��� */
    SInt64 buffCopyLen = inLength;
	/* Ҫ�������ݵ��ܳ���(��εݼ�),������buffCopyLen,������ */
    SInt64 bytesToCopy = inLength;
	/* ���ָ���Ĵ�����ݵĻ��� */
    char *buffOut = (char*)inBuffer;

	/* �μ�FileMap::GetBuffer(),�Ƿ���Ҫ��仺��?Ĭ��������� */
    Bool16 fillBuff = true;
    char *buffOffset = NULL;
    
#if FILE_SOURCE_BUFFTEST
    char testBuff[inLength + 1];
    buffOut = (char*)testBuff;
    sBuffCount ++;
    ::memset(inBuffer,0,inLength);  
    ::memset(testBuff,0,inLength);
#endif
    
	/* �ж�bufferIndex��endIndex�ĺϷ��� */
    if (buffIndex > endIndex || endIndex > maxIndex)
    {
#if FILE_SOURCE_DEBUG
        qtss_printf("OSFileSource::ReadFromCache bad index: buffIndex=%ld endIndex=%ld maxIndex=%ld\n",buffIndex,endIndex,maxIndex);
        qtss_printf("OSFileSource::ReadFromCache inPosition =%qu buffSize = %lu index=%ld\n",inPosition, fFileMap.GetMaxBufSize(),buffIndex);
#endif
        Assert(0);
    }
   
	/* ����ǰbuffer index�Ϸ�ʱ,ѭ�����Ƹ�buffer block�ڵ����� */
   while (buffIndex <= endIndex && buffIndex <= maxIndex)
   {    
#if FILE_SOURCE_DEBUG
        qtss_printf("OSFileSource::ReadFromCache inPosition =%qu buffSize = %lu index=%ld\n",inPosition, fFileMap.GetMaxBufSize(),buffIndex);
#endif

		/* ����ͨ���½�������ȷ��ȡ�����ָ����buffer block,���������Ķ���Ԫ���ڻ������е�β��;�����ø�buffer block�ĵ�ǰ����ΪbuffIndex,���������ڵ�data.
		ע��ڶ���������ʾ�Ƿ���Ҫ�������?�����ø�buffer block,�Ͳ���Ҫ�������,�ڶ������Ϊfalse.Ĭ���½������(true).һ��Ҫȷ�����buffIndex��FileBlockBuffer->fArrayIndex��� */
		/* ���ָ����������ʼ��ַ,�Ƿ���Ҫ��仺��? */
		buffStart = fFileMap.GetBuffer(buffIndex, &fillBuff);
        Assert(buffStart != NULL);
        
		/* ����Ҫ������仺��(Ĭ��ֵ),���ȶ�λ�ļ�λ��,��ָ���ļ�����ȡ���ݴ���buffStart��,���buffIndex������λ�ļ���ȡλ�� */
        if (fillBuff)
        {   /* ע���һ������û���õ� */
            OS_Error theErr = this->FillBuffer( (char *) inBuffer, (char *) buffStart, (SInt32) buffIndex);
            if (theErr != OS_NoErr)
                return theErr;
            
        }
        
        
		/* �õ�ָ��buffIndex��buffer block��ʵ�ʻ������ݴ�С */
        buffSize = fFileMap.GetBuffSize(buffIndex);
		/* �õ�Ҫ��ȡ��ý�������ڸ�buffer block�е�ʵ��λ��(����ƫ����ΪbuffPos) */
        buffOffset = &buffStart[buffPos];
        
		/* ���Ƶ�ý���ļ���ĩβ,ѭ�������˳� */
        if  (   (buffPos == 0) && 
                (bytesToCopy <= maxBuffSize) && /* �ɴ��׼��Ѿ��Ƶ�ý���ļ���ĩβ  */
                (buffSize < bytesToCopy)
            ) // that's all there is in the file
        {         
            #if FILE_SOURCE_DEBUG
                qtss_printf("OSFileSource::ReadFromCache end of file reached buffIndex=%lu buffSize = %ld bytesToCopy=%lu\n",buffIndex, buffSize,bytesToCopy);
            #endif
            Assert(buffSize <= kUInt32_Max);
			/* �����һ�β�����󻺴����ݳ���maxBuffSize�����ݸ��ƽ��� */
            ::memcpy(buffOut,buffOffset,(UInt32) buffSize);
			/* �ۼ��Ѹ��µĽ������ݵĳ��� */
            *outRcvLen += (UInt32) buffSize;/* ����ֻ���½������ݵĳ��ȶ�û���ƶ������λ��,�ɴ��׼��Ѿ��Ƶ�ý���ļ���ĩβ  */
            break; /* �˳�ѭ�� */
        }

		/* ����ָ��buffIndex��buffer�г�ȥƫ��λ�ú�ʣ�¿��Ը��ƵĻ������� */
        buffOffsetLen = buffSize - buffPos;
		/* ��û������,��ʣ�¿��Ը��ƵĻ������ݸ��ƽ����inBuffer�ṩ�Ļ���buffOut */
        if (buffCopyLen >= buffOffsetLen)
            buffCopyLen = buffOffsetLen;
            
        Assert(buffCopyLen <= buffSize);

        ::memcpy(buffOut,buffOffset, (UInt32) buffCopyLen);

		/* ͬʱ���´�����ݻ����ָ�� */
        buffOut += buffCopyLen;
		/* �ۼ�ʵ�ʽ������ݵĳ��� */
        *outRcvLen += (UInt32) buffCopyLen;
		/* ���»�Ҫ���Ƶ����ݳ��� */
        bytesToCopy -= buffCopyLen;
        Assert(bytesToCopy >= 0);
        
		/* ����Ҫ���Ƶ����ݳ���,�Ƶ���һ��buffer,������������ */
        buffCopyLen = bytesToCopy;
        buffPos = 0;/* ���Ժ�Ļ�����е�����ƫ��Ϊ0 */
		/* �Ƶ���һ������� */
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
/* ����lseek(),::read()C���Ժ�����Ӳ����ָ�����ļ�����ȡ����,����ָ�����沢����ʵ�ʶ�ȡ���ݵĳ��� */
OS_Error    OSFileSource::ReadFromDisk(void* inBuffer, UInt32 inLength, UInt32* outRcvLen)
{
    #if FILE_SOURCE_BUFFTEST
        qtss_printf("OSFileSource::Read inLength=%lu fFile=%d\n",inLength,fFile);
    #endif

   /* ���ļ�ָ������Ϊ�����ļ���ͷƫ��ΪfPosition��,������,�ͷ��ش�����Ϣ */
    if (lseek(fFile, fPosition, SEEK_SET) == -1)
		return OSThread::GetErrno();

    /* ���ļ���ͷ��ȡָ�����ȵ��ļ����ݵ�ָ�����洦 */    
    int rcvLen = ::read(fFile, (char*)inBuffer, inLength);
	/* ������,�ͷ��ش��� */
    if (rcvLen == -1)
        return OSThread::GetErrno();

	/* ���ö�ȡ�ļ����ݵĳ��� */
    if (outRcvLen != NULL)
        *outRcvLen = rcvLen;

	/* ��ʱ���¶�ȡ�ļ����ݵ�λ�� */
    fPosition += rcvLen;
    fReadPos = fPosition;
    
    return OS_NoErr;
}

/* �������inPosition�ȶ�λҪ�����ļ���λ��,��ʹ��OSFileSource::ReadFromDisk()��ָ�����ļ�����ȡ����,����ָ�����沢����ʵ�ʶ�ȡ���ݵĳ��� */
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

	/* ��λ�����ļ���λ��,��������fPosition=inPosition */
    this->Seek(inPosition);
	/* ����::read()C���Ժ�����ָ�����ļ�����ȡ����,���뻺�沢���ض�ȡ���ݵĳ��� */
    OS_Error err =  this->ReadFromDisk(inBuffer,inLength,outRcvLen);
  
/* ��ʱ��¼���ļ�����־ */
#if READ_LOG
        if (fFileLog)
        {   fFileLog = ::fopen(fFilePath,"a");
            if (fFileLog)
            {   qtss_fprintf(fFileLog, "read: %qu %lu\n",inPosition, *outRcvLen);
                ::fclose(fFileLog);
            }
        }
            
#endif

/* ��¼��ȡ�ļ�����ʱ��,���������ֽ��� */
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


/* �ر��ļ�,��������ļ����� */
void    OSFileSource::Close()
{
	/* �����ļ��Ѵ򿪲���Ҫ�ر�,�͹ر��ļ�,���ر���־��¼�ļ� */
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
