
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

/* ����ģ��: ��T��ʾ��ϣ��Ԫ����������,��K��ʾ��ϣ��Ԫ�м�ֵkey���������� */
template<class T, class K>
/* ��ϣ���� */
class OSHashTable {
public:

	//
	//constructor/destructor

	/******************* NOTE!! ******************************/

	/* used in OSRefTable:OSRefTable(),default size 1193 */
	/* ����ָ����С��Hash table */
    OSHashTable( UInt32 size )
    {
		/* �½�ָ����С�Ĺ�ϣ��(ʵ��������������) */
        fHashTable = new ( T*[size] );
        Assert( fHashTable );
		/* �ó�ֵΪ0 */
        memset( fHashTable, 0, sizeof(T*) * size );
		/* ��ϣ�����ڴ��еĴ�С */
        fSize = size;
        // Determine whether the hash size is a power of 2
        // if not set the mask to zero, otherwise we can
        // use the mask which is faster for ComputeIndex
		/* ��fSize��2����ʱ,��������fMaskΪfSize - 1,������������fMaskΪ0 */
		/* ע����������������� */
        fMask = fSize - 1;
        if ((fMask & fSize) != 0)
            fMask = 0;
		/* ���ù�ϣ��Ԫ����Ϊ0 */
        fNumEntries = 0;
    }
    ~OSHashTable()
    {
        delete [] fHashTable;
    }

	//
	//modifiers

	/* ��ָ�����������ϣ������,ȷ���������Ϊ�������Ĺ�ϣ��Ԫ,ͬ������������ϣ��Ԫ�Ƶ���κ���,��ϣ��Ԫ����1 */
    void Add( T* entry ) {
        Assert( entry->fNextHashEntry == NULL );
        K key( entry );
        UInt32 theIndex = ComputeIndex( key.GetHashKey() );
		/* �������������ͬ����һ����ϣ��Ԫ��Ϊ��ε���һ��Ԫ�� */
        entry->fNextHashEntry = fHashTable[ theIndex ];
		/* �������Ϊ�������Ĺ�ϣ��Ԫ */
        fHashTable[ theIndex ] = entry;
        fNumEntries++;
    }

	/*����ε���һ����ϣ��Ԫǰ��,ɾȥ���,��ԪԪ�ظ�����һ */
    void Remove( T* entry )
    {
        K key( entry );
        UInt32 theIndex = ComputeIndex( key.GetHashKey() );
		/* �ҵ������������ͬ����һ����ϣ��Ԫ */
        T* elem = fHashTable[ theIndex ];
        T* last = NULL;
		/* ��ͬ�����������ϣ��Ԫ����β���ͬʱ,�������ϣԪ����һ��Ԫ�Ƶ�ǰ���� */
        while (elem && elem != entry) {
            last = elem;
            elem = elem->fNextHashEntry;
        }
    
		/* �������ͬ�����������ϣ��Ԫ������һ����ϣ��Ԫ����ʱ */
        if ( elem ) // sometimes remove is called 2x ( swap, then un register )
        {
            Assert(elem);
			/* ��ͬ�����������ϣ��Ԫ����β���ͬʱ */
            if (last)
				/* ����εĺ�һ���ĺ�һ����ϣ��Ԫ�Ƶ���κ�һ����λ��,ʹ����Ĺ�ϣ��Ԫ�ν������� */
                last->fNextHashEntry = elem->fNextHashEntry;
            else
				/* ��ͬ�����������ϣ��Ԫ�������ͬʱ,����ε���һ����ϣ��Ԫǰ��һ�� */
                fHashTable[ theIndex ] = elem->fNextHashEntry;
			/*����ε���һ����ϣ��Ԫɾȥ,��ԪԪ�ظ�����һ */
            elem->fNextHashEntry = NULL;
            fNumEntries--;
        }
    }

	//
	//hash function

	/* ���Ҳ������������ͬ��ֵ�Ĺ�ϣ��Ԫ */
    T* Map( K* key )
    {
		/* �����ϣ��Ԫ���� */
        UInt32 theIndex = ComputeIndex( key->GetHashKey() );
		/* �õ�ָ������������ΪT�Ĺ�ϣ��Ԫ */
        T* elem = fHashTable[ theIndex ];
		/* �ҵ��������ͬ��ֵ�Ĺ�ϣ��Ԫ */
        while (elem) {
            K elemKey( elem );
			/* �����ֵ������ͬ,�ж�whileѭ��  */
            if (elemKey == *key)
                break;
			/* ����,��������һ����ϣ��Ԫ */
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
	/* ��ϣ����ռ�ڴ��С,Ĭ�ϴ�С��1193,�μ�OSRef.h */
    UInt32 fSize;
	/* ��ϣ����Ԫ������,Ϊ0��ʾfSize��2����,�����ʾλ���ѱ�ռ�� */
	/* ��fSize��2����ʱ,��������Ϊ1,������������ΪfSize - 1 */
    UInt32 fMask; 
	/* ��ϣ��Ԫ���� */
    UInt64 fNumEntries;
    

	/******************* NOTE!! ******************************/
	/* �����ϣ��Ԫ����,������û��ռλ�� */
    UInt32 ComputeIndex( UInt32 hashKey )
    {
        if (fMask)
			/* ע���ʱfMask��fSize-1,Hash table��СfSize��2����.��󷵻�0.
			NOTE:��ʱ���������ComputeIndex() */
            return( hashKey & fMask );
        else
			/* ע���ʱHash table��СfSize����2����,��ʹ��������� */
            return( hashKey % fSize );
    }
};

template<class T, class K>
/* ��ϣ�����������,�Թ�ϣ�����ݽ��в��� */
class OSHashTableIter {
public:

	/* ���캯��,��ʼ�����ݳ�Ա */
    OSHashTableIter( OSHashTable<T,K>* table )
    {
        fHashTable = table;
		/* ʹfCurrentָ���һ���ǿյĹ�ϣ��Ԫ */
        First();
    }

	/* Ѱ���׸��ǿյĹ�ϣ��Ԫ,�������ݳ�ԱfCurrent,���������� */
    void First()
    {
        for (fIndex = 0; fIndex < fHashTable->GetTableSize(); fIndex++) {
            fCurrent = fHashTable->GetTableEntry( fIndex );
			/* �ҵ��������˳�ѭ�� */
            if (fCurrent)
                break;
        }
    }

	/* Ѱ�����ݳ�ԱfCurrent����һ����ϣ��Ԫ */
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

	/* ��������?��ǰ��ϣ��Ԫ�ǿյ���?�ǿյľ�Ϊtrue */
    Bool16 IsDone()
    {
        return( fCurrent == NULL );
    }

	/* �õ���ǰ��ϣ��Ԫ */
    T* GetCurrent() { return fCurrent; }
    
private:
	/* ��ϣ�����ָ�� */
    OSHashTable<T,K>* fHashTable;
	/* ��ǰ��ϣ��Ԫָ�� */
    T* fCurrent;
	/* ��ϣ���е����� */
    UInt32 fIndex;
};
#endif //_OSHASHTABLE_H_
