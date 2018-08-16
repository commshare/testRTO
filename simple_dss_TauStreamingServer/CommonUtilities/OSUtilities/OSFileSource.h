
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

/* ��¼���ļ�����־�ĺ꿪�� */
#define READ_LOG 0

class FileBlockBuffer 
{

 public:
    FileBlockBuffer(): fArrayIndex(-1),fBufferSize(0),fBufferFillSize(0),fDataBuffer(NULL),fDummy(0){}
    ~FileBlockBuffer(void);


    void AllocateBuffer(UInt32 buffSize);
    void TestBuffer(void);

	/* ������ݻ��� */
    void CleanBuffer() { ::memset(fDataBuffer,0, fBufferSize); }

	/* used in class FileMap */
    void SetFillSize(UInt32 fillSize) {fBufferFillSize = fillSize;}
	/* used in FileMap::GetBufSize() */
    UInt32 GetFillSize(void) { return fBufferFillSize;}

    OSQueueElem *GetQElem() { return &fQElem; }


	/* FileMap.fFileMapArray�����е�ָ�����������(���ݾ����ļ����Ȼ��б仯) */
    SInt64              fArrayIndex;
    UInt32              fBufferSize;
    UInt32              fBufferFillSize;
	/* ���ʵ�����ݵĻ��沿��ָ��,�䳤��ΪfBufferSize */
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

		/* ����buffer��������� */
        void SetMaxBuffers(UInt32 maxBuffers) { if (maxBuffers > 0) fMaxBuffers = maxBuffers; }

		/* ����buffer��ĸ���������ֵ */
        void SetBuffIncValue(UInt32 bufferInc) { if (bufferInc > 0) fBufferInc = bufferInc;}

		/* ����buffer������,�μ������FileMap,OSFileSource�е����� */
		/* ������ֵ���������buffer���� */
        void IncMaxBuffers(void) { fMaxBuffers += fBufferInc; }
        void DecMaxBuffers(void) { if (fMaxBuffers > fBufferInc) fMaxBuffers-= fBufferInc; }
		/* ����ǰbuffer block������һ */
        void DecCurBuffers(void) { if (fNumCurrentBuffers > 0) fNumCurrentBuffers--; }
        
		/* ���û��浥Ԫ��СΪ���*1024 bytes */
        void SetBufferUnitSize  (UInt32 inUnitSizeInK)      { fBufferUnitSizeBytes = inUnitSizeInK * 1024; }

		//accessors

		/* ��ȡ���浥Ԫ��С(��λΪbytes) */
        UInt32 GetBufferUnitSizeBytes()     { return fBufferUnitSizeBytes; }//32K Bytes
		/* �õ�buffer�������� */
        UInt32 GetMaxBuffers(void)  { return fMaxBuffers; }
		/* ��û����С������ */
        UInt32 GetIncBuffers()      { return fBufferInc; }
		/* used in FileMap::DeleteOldBuffs() */
		/* ��ȡ��ǰ����ĸ��� */
        UInt32 GetNumCurrentBuffers(void)   { return fNumCurrentBuffers; }

		//Utilities

		/* ѭ��ɾ������Ԫ��,�����������ļ������,������ز������� */
        void DeleteBlockPool();
        /* ����FileBlockBuffer::AllocateBuffer(),�õ�ָ����С�Ļ���鲢λ�ڶ���β�� */
        FileBlockBuffer* GetBufferElement(UInt32 bufferSizeBytes);
		/* ʹ����Ҫ��ȡ���ݵĻ����λ�ڶ���β�� */
        void MarkUsed(FileBlockBuffer* inBuffPtr);

    private:
        OSQueue fQueue;
        UInt32  fMaxBuffers;/* buffer���������� */
        UInt32  fNumCurrentBuffers; /* ��ǰbuffer�ĸ��� */ 
        UInt32  fBufferInc; /* ����/����buffer��С������ֵ */
        UInt32  fBufferUnitSizeBytes;/* buffer unit��СΪ32K bytes */
        UInt32  fBufferDataSizeBytes;//8*32Kbytes,�����˵,��8��������buffer unit,���ǵ������ܴ�С

};

class FileMap 
{

    public:
        FileMap(void):fFileMapArray(NULL),fDataBufferSize(0),fMapArraySize(0),fNumBuffSizeUnits(0) {}
        ~FileMap(void) {fFileMapArray = NULL;}

		/***************** �����бȽ���Ҫ��2������ ****************/
		/* �μ�OSFileSource::AllocateFileCache(),����Ĭ�ϵ�������� */
		/* �����ļ��������û�������,��ý���ļ�����ǡ����С��һ�������ָ�������ṹ,������Щָ������ */
        void    AllocateBufferMap(UInt32 inUnitSizeInK, UInt32 inNumBuffSizeUnits, UInt32 inBufferIncCount, UInt32 inMaxBitRateBuffSizeInBlocks, UInt64 fileLen, UInt32 inBitRate);
        char*   GetBuffer(SInt64 bufIndex, Bool16 *outIsEmptyBuff);

		/* ʵ��ʹ��FileBlockBuffer::TestBuffer() */
        void    TestBuffer(SInt32 bufIndex) {Assert (bufIndex >= 0); fFileMapArray[bufIndex]->TestBuffer();};

		/* Ϊָ��buffIndex��buffer�������ָ��������СFillSize,used in OSFileSource::FillBuffer() */
        void    SetIndexBuffFillSize(SInt32 bufIndex, UInt32 fillSize) { Assert (bufIndex >= 0); fFileMapArray[bufIndex]->SetFillSize(fillSize);}

		/* �õ�ÿ��������С,ע�����������Ҫ,fFileMap.GetBuffIndex()Ҫ�õ��� */
        UInt32  GetMaxBufSize(void) {return fDataBufferSize;}

		/* �õ�ָ�����������ݻ����ʵ������С,ʵ��ʹ��FileBlockBuffer::GetFillSize() */
        UInt32  GetBuffSize(SInt64 bufIndex)    { Assert (bufIndex >= 0); return fFileMapArray[bufIndex]->GetFillSize(); }

        UInt32  GetIncBuffers(void) { return fBlockPool.GetIncBuffers(); }
        void    IncMaxBuffers()     {fBlockPool.IncMaxBuffers(); }
        void    DecMaxBuffers()     {fBlockPool.DecMaxBuffers(); }

		/* �ж��Ƿ��ʼ��?used in AllocateBufferMap() */
        Bool16  Initialized()       { return fFileMapArray == NULL ? false : true; }
		/* ��ʼ�������ָ������,��0(��0) */
        void    Clean(void);
		/* ɾ�������ָ������,ע����ǽ�ÿ��ָ������ÿ� */
        void    DeleteMap(void);
        void    DeleteOldBuffs(void);

		/* used in OSFileSource::ReadFromCache() */
		/* ��Ӳ����ý���ļ��ĸ���λ�õõ����ƽ��ڴ��е����ڻ��������� */
        SInt64  GetBuffIndex(UInt64 inPosition) {   return inPosition / this->GetMaxBufSize();  }
		/* �õ�File Map array�ķ�������-1 */
		/* �õ�����bufferIndex,�ɾ����ļ����Ⱦ��� */
        SInt64  GetMaxBuffIndex() { Assert(fMapArraySize > 0); return fMapArraySize -1; }
		/* used in OSFileSource::ReadFromCache() */
		/* �õ�ָ�������Ļ���������ƫ�� */
        UInt64  GetBuffOffset(SInt64 bufIndex) { return (UInt64) (bufIndex * this->GetMaxBufSize() ); }

        //data members


		//�õ�ǰ������:FileBlockBuffer,FileBlockPool
        /* �����ṩ�ö����Ǳ��ڴӻ���ؽǶȲ�������� */
        FileBlockPool fBlockPool;

        FileBlockBuffer**   fFileMapArray;
    
    private:

	/* ÿ�����ݻ���(�������)������ֽ���(��������ݻ���Ĵ�С����),������ʵ�ʴ�СΪ��������С,
	����ӳ��ý���ļ��Ļ����ʴ�С(����256K) */
    UInt32              fDataBufferSize;/* ý���ļ��ֿ��С */
	/* ����fFileMapArray�ķ������� */
    SInt64              fMapArraySize;
	/* �����ʺͻ�����С�ı�ֵ,����256K/32K=8,���ֵ����Ϊ8,��СֵΪ1,
	����fBufferUnitSizeBytes��ͬ������fDataBufferSize�Ĵ�С,�μ� FileMap::AllocateBufferMap() */
    UInt32              fNumBuffSizeUnits;/*�ļ���������*/
    
};

/* ע�����ֻʹ��FileMap�� */
class OSFileSource
{
    public:
    
        OSFileSource() :    fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false)/* ��Ŀ¼�ļ���? */, fCacheEnabled(false)/* Ĭ�ϲ������ļ����� */                       
        {
        
        #if READ_LOG 
            fFileLog = NULL;
            fTrackID = 0;
            fFilePath[0]=0;
        #endif
        
        }
                
        OSFileSource(const char *inPath) :  fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false),fCacheEnabled(false)
        {
         Set(inPath); /* ��Windows xp��,�������һ��Ҫ������д */
         
        #if READ_LOG 
            fFileLog = NULL;
            fTrackID = 0;
            fFilePath[0]=0;
        #endif      
         
         }
        
        ~OSFileSource() { Close();  fFileMap.DeleteMap();}
        
        //Sets this object to reference this file
		/* ��ֻ����ʽ��ָ��·����ý���ļ�,�������ļ���Ӧ������Ϣ */
        void            Set(const char *inPath);
        
        // Call this if you don't want Close or the destructor to close the fd
		/* ��Ҫ�ر��ļ�(���ǲ�Ҫ�ر��ļ�������) */
        void            DontCloseFD() { fShouldClose = false; }
        
        //Advise: this advises the OS that we are going to be reading soon from the
        //following position in the file
		/* ����OS���Ͼ�Ҫ���ļ�ָ��λ�ÿ�ʼ�������� */
        void            Advise(UInt64 advisePos, UInt32 adviseAmt);
        
		/******************************** ��������ĵļ�����ȡ�ļ����ݺ��� ************************************************/

        OS_Error    Read(void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL)
                    {   return ReadFromDisk(inBuffer, inLength, outRcvLen);
                    }
                    
        OS_Error    Read(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromDisk(void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromCache(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);
        OS_Error    ReadFromPos(UInt64 inPosition, void* inBuffer, UInt32 inLength, UInt32* outRcvLen = NULL);

		/********************************************************************************************************************/

		/* ͨ����������ļ��Ƿ���Ի���? */
        void        EnableFileCache(Bool16 enabled) {OSMutexLocker locker(&fMutex); fCacheEnabled = enabled; }
		/* ��ѯ�ļ��Ƿ���Ի���? */
        Bool16      GetCacheEnabled() { return fCacheEnabled; }


		/*********************************** ����ע��ú���,ʮ����Ҫ! ************************/
		/* �����ļ��������û�������,��ý���ļ�����ǡ����С��һ�������ָ�������ṹ,������Щָ������ */
        void        AllocateFileCache(UInt32 inUnitSizeInK = 32, UInt32 bufferSizeUnits = 0, UInt32 incBuffers = 1, UInt32 inMaxBitRateBuffSizeInBlocks = 8, UInt32 inBitRate = 32768 /* 2^15 */) 
                    {   
						fFileMap.AllocateBufferMap(inUnitSizeInK, bufferSizeUnits,incBuffers, inMaxBitRateBuffSizeInBlocks, fLength, inBitRate);
                    } 

		/* ��������ļ������ĸ��� */
        void        IncMaxBuffers()     {OSMutexLocker locker(&fMutex); fFileMap.IncMaxBuffers(); }
        void        DecMaxBuffers()     {OSMutexLocker locker(&fMutex); fFileMap.DecMaxBuffers(); }

		/************************************* ����ע��ú���,ʮ����Ҫ! ****************************/
        OS_Error    FillBuffer(char* ioBuffer, char *buffStart, SInt32 bufIndex);
        
		/* �ر��ļ�,��������ļ����� */
        void            Close();

		//accessor

		/* �õ��ļ�����޸�ʱ��, used in QTFile::GetModDate() */
        time_t          GetModDate()                { return fModDate; }
		/* �õ��ļ�����,used in QTFile::ValidTOC() */
        UInt64          GetLength()                 { return fLength; }
        UInt64          GetCurOffset()              { return fPosition; }
		/* ��λ�����ļ���λ��,used in OSFileSource::ReadFromPos() */
        void            Seek(SInt64 newPosition)    { fPosition = newPosition;  }

		/* used in QTFile_FileControlBlock::IsValid(),�ļ���������Ϊ-1����valid */
        Bool16 IsValid()                            { return fFile != -1;       }
		/* ��Ŀ¼�ļ���? */
        Bool16 IsDir()                              { return fIsDir; }
        
        // For async I/O purposes
		/* ����ļ������� */
        int             GetFD()                     { return fFile; }
        void            SetTrackID(UInt32 trackID);
        // So that close won't do anything
		/* ���ļ���������Ϊ�Ƿ�,���ر��ļ� */
        void ResetFD()  { fFile=-1; }
        
		/* ������־�ļ�·��,����־�ļ�,��¼�����Ϣ���ٹرո���־�ļ� */
        void SetLog(const char *inPath);
    
    private:

		/* �ļ������� */
        int     fFile;
		/* �ļ����� */
        UInt64  fLength;
		/* �ļ���ǰƫ��λ��(���ļ���������һ���̶���) */
        UInt64  fPosition;
		/* �ļ���ȡ����λ��,�μ�OSFileSource::ReadFromDisk() */
        UInt64  fReadPos;

		/* Ӧ�ùر��ļ���? */
        Bool16  fShouldClose;
		/* ��Ŀ¼�ļ���? */
        Bool16  fIsDir;

		/* ý���ļ����޸�ʱ��,used in QTFile::GetModDate */
        time_t  fModDate;
        
        
        OSMutex fMutex;

		/* ע��˴��õ������FileMap�� */
        FileMap fFileMap;

		/* �ܷ񻺴��ļ�? */
        Bool16  fCacheEnabled;
#if READ_LOG
        FILE*               fFileLog;/* ��¼���ļ���־���ļ������� */
        char                fFilePath[1024];/* ����־�ļ���·�� */
        UInt32              fTrackID;
#endif

};

#endif //__OSFILE_H_
