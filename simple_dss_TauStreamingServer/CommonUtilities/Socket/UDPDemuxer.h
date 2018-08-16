
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 UDPDemuxer.h
Description: Provides a "Listener" socket for UDP. Blocks on a local IP & port,
             waiting for data. When it gets data, it passes it off to a UDPDemuxerTask(RTPStream)
             object depending on where it came from.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 


#ifndef __UDPDEMUXER_H__
#define __UDPDEMUXER_H__

#include "OSHashTable.h"
#include "OSMutex.h"
#include "StrPtrLen.h"


class Task;
class UDPDemuxerKey;
class UDPDemuxerTask;
class UDPDemuxer;

//IMPLEMENTATION ONLY:
//HASH TABLE CLASSES USED ONLY IN IMPLEMENTATION

/* ������ṩ�˼����ϣ��ֵ���㷨���� */
class UDPDemuxerUtils
{
    private:
    
		/* ����Hashֵ�ķ���:ȥ��IP Addr��ǰ�����ֽ�,�ٽ���������ֽںͶ˿����һ�����ֽ�UInt32 */
        static UInt32 ComputeHashValue(UInt32 inRemoteAddr, UInt16 inRemotePort)
            { return ((inRemoteAddr << 16) + inRemotePort); }
            
    friend class UDPDemuxerTask;
    friend class UDPDemuxerKey;
};

/* ����䵱��ϣ��ı�Ԫ(����һ������Ԫ) */
class UDPDemuxerTask
{
    public:
    
        UDPDemuxerTask()
            :   fRemoteAddr(0), fRemotePort(0),
                fHashValue(0), fNextHashEntry(NULL) {}
        virtual ~UDPDemuxerTask() {}
        
        UInt32  GetRemoteAddr() { return fRemoteAddr; } //��ȡClient��IP addr
        
    private:

        void Set(UInt32 inRemoteAddr, UInt16 inRemotePort)
            {   
				fRemoteAddr = inRemoteAddr; fRemotePort = inRemotePort;
                fHashValue = UDPDemuxerUtils::ComputeHashValue(fRemoteAddr, fRemotePort); /* ���ù��ߺ�������hashֵ */
            }
        
        //key values (client's ip & port)
        UInt32 fRemoteAddr;
        UInt16 fRemotePort;
        
        //precomputed for performance
        UInt32 fHashValue; //�ù�ϣ��Ԫ�Ĺ�ϣ��ֵ
        
        UDPDemuxerTask  *fNextHashEntry;

        friend class UDPDemuxerKey;
        friend class UDPDemuxer;
        friend class OSHashTable<UDPDemuxerTask,UDPDemuxerKey>;
};


/* ������ϣ��Ĺ�ϣ��ֵ����,�������ֹ��캯�� */
class UDPDemuxerKey
{
    private:

        //CONSTRUCTOR / DESTRUCTOR:
        UDPDemuxerKey(UInt32 inRemoteAddr, UInt16 inRemotePort)
            :   fRemoteAddr(inRemoteAddr), fRemotePort(inRemotePort)
                { fHashValue = UDPDemuxerUtils::ComputeHashValue(inRemoteAddr, inRemotePort);/* ���ù��ߺ�������hashֵ */ }
                
        ~UDPDemuxerKey() {}
        
        
    private:

        //PRIVATE ACCESSORS:    
        UInt32      GetHashKey()        { return fHashValue; }

        //these functions are only used by the hash table itself. This constructor will break the "Set" functions.
		/* ��һ�����캯��,��Hash TableԪ�ĳ�Ա�������ݹ��� */
        UDPDemuxerKey(UDPDemuxerTask *elem) :   fRemoteAddr(elem->fRemoteAddr),
                                                fRemotePort(elem->fRemotePort), 
                                                fHashValue(elem->fHashValue) {}
        
		/* ����Hash keyֵ֮��ıȽ�����,����Hash keyֵ�Ƿ����? */
        friend int operator ==(const UDPDemuxerKey &key1, const UDPDemuxerKey &key2) 
		{
            if ((key1.fRemoteAddr == key2.fRemoteAddr) &&
                (key1.fRemotePort == key2.fRemotePort))
                return true;
            return false;
        }
        
        //data:
        UInt32 fRemoteAddr;
        UInt16 fRemotePort;
        UInt32 fHashValue;

        friend class OSHashTable<UDPDemuxerTask,UDPDemuxerKey>;
        friend class UDPDemuxer;
};

//CLASSES USED ONLY IN IMPLEMENTATION ��ϣ��Ķ���
typedef OSHashTable<UDPDemuxerTask, UDPDemuxerKey> UDPDemuxerHashTable;

/* ע��: �����������Ǿ���ʹ�õ���, ���Թ�ϣ��(UDPDemuxerTask,UDPDemuxerKey)�����ݳ�Ա,��Ҫ���ù�ϣ�������������(����RTPStream)��RegisterTask/UnregisterTask */
class UDPDemuxer
{
    public:

        UDPDemuxer() : fHashTable(kMaxHashTableSize), fMutex() {}
        ~UDPDemuxer() {}

        //These functions grab the mutex and are therefore premptive safe(��ռ���ʰ�ȫ)
        
        // Return values: OS_NoErr, or EPERM if there is already a task registered with this address combination 
		/* ��ָ����address/port��ϴ�����ָ����Hash Table entry,����OS_NoErr;����address/port�����Hash Table���ѱ�ռ��,����EPERM */
        OS_Error RegisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort,UDPDemuxerTask *inTaskP);

        // Return values: OS_NoErr, or EPERM if this task / address combination is not registered
		/* ��ָ����address/port��ϴ�����Hash Table entry,���������inTaskP�Ƚ�,����ͬ,�ʹ�Hash Table��ɾȥ��
		Hash Table entry,����OS_NoErr;���򷵻�EPERM */
        OS_Error UnregisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort,UDPDemuxerTask *inTaskP);
        
        //Assumes that parent has grabbed the mutex!
        UDPDemuxerTask* GetTask(UInt32 inRemoteAddr, UInt16 inRemotePort);

		/* �ж�ָ����address/port combination�Ƿ������Task? */
		/* used in UDPSocketPool::GetUDPSocketPair() */
        Bool16  AddrInMap(UInt32 inRemoteAddr, UInt16 inRemotePort)
                    { return (this->GetTask(inRemoteAddr, inRemotePort) != NULL); }
         
		//accessors
        OSMutex*                GetMutex()      { return &fMutex; }
        UDPDemuxerHashTable*    GetHashTable()  { return &fHashTable; }
        
    private:
    
        enum
        {
            kMaxHashTableSize = 2747 //is this prime? it should be... //UInt32
        };
		
        UDPDemuxerHashTable fHashTable; //�Թ�ϣ�������ݳ�Ա,UDPDemuxerHashTable���������
        OSMutex             fMutex; //this data structure is shared!
};

#endif // __UDPDEMUXER_H__


