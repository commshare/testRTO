
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

/* 该类仅提供了计算哈希键值的算法函数 */
class UDPDemuxerUtils
{
    private:
    
		/* 计算Hash值的方法:去掉IP Addr的前两个字节,再将其后两个字节和端口组成一个四字节UInt32 */
        static UInt32 ComputeHashValue(UInt32 inRemoteAddr, UInt16 inRemotePort)
            { return ((inRemoteAddr << 16) + inRemotePort); }
            
    friend class UDPDemuxerTask;
    friend class UDPDemuxerKey;
};

/* 该类充当哈希表的表元(它是一个链表元) */
class UDPDemuxerTask
{
    public:
    
        UDPDemuxerTask()
            :   fRemoteAddr(0), fRemotePort(0),
                fHashValue(0), fNextHashEntry(NULL) {}
        virtual ~UDPDemuxerTask() {}
        
        UInt32  GetRemoteAddr() { return fRemoteAddr; } //获取Client端IP addr
        
    private:

        void Set(UInt32 inRemoteAddr, UInt16 inRemotePort)
            {   
				fRemoteAddr = inRemoteAddr; fRemotePort = inRemotePort;
                fHashValue = UDPDemuxerUtils::ComputeHashValue(fRemoteAddr, fRemotePort); /* 运用工具函数计算hash值 */
            }
        
        //key values (client's ip & port)
        UInt32 fRemoteAddr;
        UInt16 fRemotePort;
        
        //precomputed for performance
        UInt32 fHashValue; //该哈希表元的哈希键值
        
        UDPDemuxerTask  *fNextHashEntry;

        friend class UDPDemuxerKey;
        friend class UDPDemuxer;
        friend class OSHashTable<UDPDemuxerTask,UDPDemuxerKey>;
};


/* 操作哈希表的哈希键值的类,包括两种构造函数 */
class UDPDemuxerKey
{
    private:

        //CONSTRUCTOR / DESTRUCTOR:
        UDPDemuxerKey(UInt32 inRemoteAddr, UInt16 inRemotePort)
            :   fRemoteAddr(inRemoteAddr), fRemotePort(inRemotePort)
                { fHashValue = UDPDemuxerUtils::ComputeHashValue(inRemoteAddr, inRemotePort);/* 运用工具函数计算hash值 */ }
                
        ~UDPDemuxerKey() {}
        
        
    private:

        //PRIVATE ACCESSORS:    
        UInt32      GetHashKey()        { return fHashValue; }

        //these functions are only used by the hash table itself. This constructor will break the "Set" functions.
		/* 另一个构造函数,将Hash Table元的成员变量传递过来 */
        UDPDemuxerKey(UDPDemuxerTask *elem) :   fRemoteAddr(elem->fRemoteAddr),
                                                fRemotePort(elem->fRemotePort), 
                                                fHashValue(elem->fHashValue) {}
        
		/* 定义Hash key值之间的比较运算,两个Hash key值是否相等? */
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

//CLASSES USED ONLY IN IMPLEMENTATION 哈希表的定义
typedef OSHashTable<UDPDemuxerTask, UDPDemuxerKey> UDPDemuxerHashTable;

/* 注意: 这个类才是我们经常使用的类, 它以哈希表(UDPDemuxerTask,UDPDemuxerKey)作数据成员,需要引用哈希表操作进行任务(比如RTPStream)的RegisterTask/UnregisterTask */
class UDPDemuxer
{
    public:

        UDPDemuxer() : fHashTable(kMaxHashTableSize), fMutex() {}
        ~UDPDemuxer() {}

        //These functions grab the mutex and are therefore premptive safe(抢占访问安全)
        
        // Return values: OS_NoErr, or EPERM if there is already a task registered with this address combination 
		/* 在指定的address/port组合处加入指定的Hash Table entry,返回OS_NoErr;若该address/port组合在Hash Table中已被占用,返回EPERM */
        OS_Error RegisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort,UDPDemuxerTask *inTaskP);

        // Return values: OS_NoErr, or EPERM if this task / address combination is not registered
		/* 在指定的address/port组合处查找Hash Table entry,将其与入参inTaskP比较,若相同,就从Hash Table中删去该
		Hash Table entry,返回OS_NoErr;否则返回EPERM */
        OS_Error UnregisterTask(UInt32 inRemoteAddr, UInt16 inRemotePort,UDPDemuxerTask *inTaskP);
        
        //Assumes that parent has grabbed the mutex!
        UDPDemuxerTask* GetTask(UInt32 inRemoteAddr, UInt16 inRemotePort);

		/* 判断指定的address/port combination是否分配了Task? */
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
		
        UDPDemuxerHashTable fHashTable; //以哈希表作数据成员,UDPDemuxerHashTable定义见上面
        OSMutex             fMutex; //this data structure is shared!
};

#endif // __UDPDEMUXER_H__


