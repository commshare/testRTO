
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 OSHashTable.h
Description: Defines a template class for hash tables.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-19

****************************************************************************/ 



#ifndef _OSHASHTABLE_H_
#define _OSHASHTABLE_H_

#include "MyAssert.h"
#include "OSHeaders.h"

/* 函数模板: 类T表示哈希表元的数据类型,类K表示哈希表元中键值key的数据类型 */
template<class T, class K>
/* 哈希表类 */
class OSHashTable {
public:

	//
	//constructor/destructor

	/******************* NOTE!! ******************************/

	/* used in OSRefTable:OSRefTable(),default size 1193 */
	/* 创建指定大小的Hash table */
    OSHashTable( UInt32 size )
    {
		/* 新建指定大小的哈希表(实质是类对象的数组) */
        fHashTable = new ( T*[size] );
        Assert( fHashTable );
		/* 置初值为0 */
        memset( fHashTable, 0, sizeof(T*) * size );
		/* 哈希表在内存中的大小 */
        fSize = size;
        // Determine whether the hash size is a power of 2
        // if not set the mask to zero, otherwise we can
        // use the mask which is faster for ComputeIndex
		/* 当fSize是2的幂时,设置掩码fMask为fSize - 1,否则设置掩码fMask为0 */
		/* 注意用掩码比用索引快 */
        fMask = fSize - 1;
        if ((fMask & fSize) != 0)
            fMask = 0;
		/* 设置哈希表元个数为0 */
        fNumEntries = 0;
    }
    ~OSHashTable()
    {
        delete [] fHashTable;
    }

	//
	//modifiers

	/* 将指定类对象加入哈希表列中,确保将入参作为该索引的哈希表元,同索引的其他哈希表元移到入参后面,哈希表元数加1 */
    void Add( T* entry ) {
        Assert( entry->fNextHashEntry == NULL );
        K key( entry );
        UInt32 theIndex = ComputeIndex( key.GetHashKey() );
		/* 将与入参索引相同的那一个哈希表元作为入参的下一个元素 */
        entry->fNextHashEntry = fHashTable[ theIndex ];
		/* 将入参作为该索引的哈希表元 */
        fHashTable[ theIndex ] = entry;
        fNumEntries++;
    }

	/*将入参的下一个哈希表元前移,删去入参,表元元素个数减一 */
    void Remove( T* entry )
    {
        K key( entry );
        UInt32 theIndex = ComputeIndex( key.GetHashKey() );
		/* 找到与入参索引相同的那一个哈希表元 */
        T* elem = fHashTable[ theIndex ];
        T* last = NULL;
		/* 当同索引的这个哈希表元和入参不相同时,将这个哈希元的下一个元移到前面来 */
        while (elem && elem != entry) {
            last = elem;
            elem = elem->fNextHashEntry;
        }
    
		/* 当与入参同索引的这个哈希表元或其下一个哈希表元存在时 */
        if ( elem ) // sometimes remove is called 2x ( swap, then un register )
        {
            Assert(elem);
			/* 当同索引的这个哈希表元和入参不相同时 */
            if (last)
				/* 将入参的后一个的后一个哈希表元移到入参后一个的位置,使后面的哈希表元衔接连贯上 */
                last->fNextHashEntry = elem->fNextHashEntry;
            else
				/* 当同索引的这个哈希表元和入参相同时,将入参的下一个哈希表元前移一个 */
                fHashTable[ theIndex ] = elem->fNextHashEntry;
			/*将入参的下一个哈希表元删去,表元元素个数减一 */
            elem->fNextHashEntry = NULL;
            fNumEntries--;
        }
    }

	//
	//hash function

	/* 查找并返回与入参相同键值的哈希表元 */
    T* Map( K* key )
    {
		/* 计算哈希表元索引 */
        UInt32 theIndex = ComputeIndex( key->GetHashKey() );
		/* 得到指定索引的类型为T的哈希表元 */
        T* elem = fHashTable[ theIndex ];
		/* 找到与入参相同键值的哈希表元 */
        while (elem) {
            K elemKey( elem );
			/* 假如键值类型相同,中断while循环  */
            if (elemKey == *key)
                break;
			/* 否则,继续找下一个哈希表元 */
            elem = elem->fNextHashEntry;
        }
        return elem;
    }

    //
	//accessors

    UInt64 GetNumEntries() { return fNumEntries; }
    
    UInt32 GetTableSize() { return fSize; }
    T* GetTableEntry( int i ) { return fHashTable[i]; }

private:
    T** fHashTable;
	/* 哈希表所占内存大小,默认大小是1193,参见OSRef.h */
    UInt32 fSize;
	/* 哈希表中元的掩码,为0表示fSize是2的幂,非零表示位置已被占有 */
	/* 当fSize是2的幂时,设置掩码为1,否则设置掩码为fSize - 1 */
    UInt32 fMask; 
	/* 哈希表元个数 */
    UInt64 fNumEntries;
    

	/******************* NOTE!! ******************************/
	/* 计算哈希表元索引,区分有没有占位置 */
    UInt32 ComputeIndex( UInt32 hashKey )
    {
        if (fMask)
			/* 注意此时fMask是fSize-1,Hash table大小fSize是2的幂.最后返回0.
			NOTE:此时的掩码快于ComputeIndex() */
            return( hashKey & fMask );
        else
			/* 注意此时Hash table大小fSize不是2的幂,就使用求和运算 */
            return( hashKey % fSize );
    }
};

template<class T, class K>
/* 哈希表迭代算子类,对哈希表数据进行操作 */
class OSHashTableIter {
public:

	/* 构造函数,初始化数据成员 */
    OSHashTableIter( OSHashTable<T,K>* table )
    {
        fHashTable = table;
		/* 使fCurrent指向第一个非空的哈希表元 */
        First();
    }

	/* 寻找首个非空的哈希表元,赋给数据成员fCurrent,并立即返回 */
    void First()
    {
        for (fIndex = 0; fIndex < fHashTable->GetTableSize(); fIndex++) {
            fCurrent = fHashTable->GetTableEntry( fIndex );
			/* 找到后立即退出循环 */
            if (fCurrent)
                break;
        }
    }

	/* 寻找数据成员fCurrent的下一个哈希表元 */
    void Next()
    {
        fCurrent = fCurrent->fNextHashEntry;
        if (!fCurrent) {
            for (fIndex = fIndex + 1; fIndex < fHashTable->GetTableSize(); fIndex++) {
                fCurrent = fHashTable->GetTableEntry( fIndex );
                if (fCurrent)
                    break;
            }
        }
    }

	/* 击中了吗?当前哈希表元是空的吗?是空的就为true */
    Bool16 IsDone()
    {
        return( fCurrent == NULL );
    }

	/* 得到当前哈希表元 */
    T* GetCurrent() { return fCurrent; }
    
private:
	/* 哈希表类的指针 */
    OSHashTable<T,K>* fHashTable;
	/* 当前哈希表元指针 */
    T* fCurrent;
	/* 哈希表中的索引 */
    UInt32 fIndex;
};
#endif //_OSHASHTABLE_H_
