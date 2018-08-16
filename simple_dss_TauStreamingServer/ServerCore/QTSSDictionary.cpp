
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSDictionary.cpp
Description: Definitions of two classes: QTSSDictionary and QTSSDictionaryMap.
             Collectively, these classes implement the "dictionary" APIs in QTSS
             API. A QTSSDictionary corresponds to a QTSS_Object,a QTSSDictionaryMap 
			 corresponds to a QTSS_ObjectType.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "MyAssert.h"
#include "OSMemory.h"
#include "QTSSDictionary.h"
#include "QTSSDataConverter.h" /* only just used in QTSSDictionary::GetValueAsString() */



/***************** class QTSSDictionary ***************************/

//constructor/destructor
/* 注意QTSSDictionaryMap的重要性!需要入参QTSSDictionaryMap和OSMutex */
QTSSDictionary::QTSSDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex) 
:   fAttributes(NULL), fInstanceAttrs(NULL), fInstanceArraySize(0),
    fMap(inMap), fInstanceMap(NULL), fMutexP(inMutex), fMyMutex(false), fLocked(false)
{
	/* 利用得到的QTSSDictionaryMap得到available attr的个数,再创建相应的属性数组 */
    if (fMap != NULL)
        fAttributes = NEW DictValueElement[inMap->GetNumAttrs()];
	if (fMutexP == NULL)
	{
		fMyMutex = true;
		fMutexP = NEW OSMutex();
	}
}

QTSSDictionary::~QTSSDictionary()
{
    if (fMap != NULL)
		/* 将全部属性数据删除 */
        this->DeleteAttributeData(fAttributes, fMap->GetNumAttrs());
    if (fAttributes != NULL)
        delete [] fAttributes;
    delete fInstanceMap;
	/* 删除全部实例属性数据 */
    this->DeleteAttributeData(fInstanceAttrs, fInstanceArraySize);
    delete [] fInstanceAttrs;
	if (fMyMutex)
		delete fMutexP;
}

// CreateNewDictionary, see QTSSDictionary.h line 196

/* 引用构造函数QTSSDictionary()创建新的Dictionary! */
QTSSDictionary* QTSSDictionary::CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex)
{
    return NEW QTSSDictionary(inMap, inMutex);
}

// QTSS API VALUE / ATTRIBUTE CALLBACK ROUTINES
//
//  GetValuePtr
/* 注意该函数是非常核心的QTSSDictionary类的函数,非常重要! */
/* 给出指定属性ID和多值索引的属性,确定它的缓存地址和长度(赋给第三,四个入参),有几个特殊情况的处理 */
QTSS_Error QTSSDictionary::GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                            void** outValueBuffer, UInt32* outValueLen,
                                            Bool16 isInternal)
{
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
	/* 判断是否是实例属性(最高位bit的值是1)?是,就将实例映射和实例属性传递进来 */
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }

    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* 获取属性索引MapIndex,实际上就是设置最高位为0 */
	/* 起这个名称,为了与入参inIndex区分开 */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);

	/* 对入参和新建的local variable作合法性检查 */

    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
	/* 若是removed attr(最高位为1),就报错 */
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
	/* 假如这个属性不是内部的,不是强制访问安全的,没有加锁,就报错 */
    if ((!isInternal) && (!theMap->IsPreemptiveSafe(theMapIndex)) && !this->IsLocked())
        return QTSS_NotPreemptiveSafe;
    // An iterated attribute cannot have a param retrieval function
	/* iterated attribute有多个值,不能有param retrieval function */
    if ((inIndex > 0) && (theMap->GetAttrFunction(theMapIndex) != NULL))
        return QTSS_BadIndex;
    // Check to make sure the index parameter is legal
	/* 不得超过available attr的总个数! */
    if ((inIndex > 0) && (inIndex >= theAttrs[theMapIndex].fNumAttributes))
        return QTSS_BadIndex;
        
        
    // Retrieve the parameter
	/* 从入参获取相关的信息 */
    char* theBuffer = theAttrs[theMapIndex].fAttributeData.Ptr;
    *outValueLen = theAttrs[theMapIndex].fAttributeData.Len;
    
	/* 该属性可以访问吗? */
	Bool16 cacheable = theMap->IsCacheable(theMapIndex);

	/* 假如可以获得属性函数指针,并分两种情形:当可访问且没有属性值时;当不可访问时. 前者可以利用GetAttrFunction获取指定的属性值 */
	if ( (theMap->GetAttrFunction(theMapIndex) != NULL) && ((cacheable && (*outValueLen == 0)) || !cacheable) )
    {
        // If function is cacheable: 
		// If the parameter doesn't have a value assigned yet, and there is an attribute
        // retrieval function provided, invoke that function now.
		// If function is *not* cacheable:
		// always call the function
		
		/* 注意theMap->GetAttrFunction(theMapIndex)的返回值是个QTSS_AttrFunctionPtr,再由它的原型定义,获取第二个参数对应的具体值 */
        theBuffer = (char*)theMap->GetAttrFunction(theMapIndex)(this, outValueLen);

        //If the param retrieval function didn't return an explicit value for this attribute,
        //refetch the parameter out of the array, in case the function modified it.
        
		/* 若QTSS_AttrFunctionPtr()返回值为空,在数组之外重取这个参数,以防这个函数修改它.
		   就是改回原来的预设值! */
        if (theBuffer == NULL)
        {
            theBuffer = theAttrs[theMapIndex].fAttributeData.Ptr;
            *outValueLen = theAttrs[theMapIndex].fAttributeData.Len;
        }
        
    }
#if DEBUG
    else
        // Make sure we aren't outside the bounds of attribute memory
        Assert(theAttrs[theMapIndex].fAllocatedLen >=
            (theAttrs[theMapIndex].fAttributeData.Len * (theAttrs[theMapIndex].fNumAttributes)));
#endif

    // Return an error if there is no data for this attribute
	/* 假如正确取得属性,但是里面没有属性值(为0),返回错误 */
    if (*outValueLen == 0)
        return QTSS_ValueNotFound;
     
	/* 注意入参inIndex是iterated attribute的多值属性索引,确定给定属性索引的buffer位置 */
    theBuffer += theAttrs[theMapIndex].fAttributeData.Len * inIndex;
    *outValueBuffer = theBuffer;
        
    // strings need an extra dereference(重引用) - moved it up
	/* 当属性为多个属性的字符串类型时,给出它的内存地址和长度(需要作恰当处理) */
    if ((theMap->GetAttrType(theMapIndex) == qtssAttrDataTypeCharArray) && (theAttrs[theMapIndex].fNumAttributes > 1))
        {
            char** string = (char**)theBuffer;
            *outValueBuffer = *string;
            //*outValueLen = strlen(*string) + 1;
            *outValueLen = strlen(*string);
        }
    
    return QTSS_NoErr;
}


//GetValue
/* 利用QTSSDictionary::GetValuePtr()来获取指定属性ID和多值索引的属性的缓存地址和长度,并调整入参ioValueLen的值到实际的数据长度.
   注意是非抢占安全的,所以要加互斥锁 */
QTSS_Error QTSSDictionary::GetValue(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                            void* ioValueBuffer, UInt32* ioValueLen)
{
    // If there is a mutex, lock it and get a pointer to the proper attribute
    OSMutexLocker locker(fMutexP);

    void* tempValueBuffer = NULL;
    UInt32 tempValueLen = 0;
    QTSS_Error theErr = this->GetValuePtr(inAttrID, inIndex, &tempValueBuffer, &tempValueLen, true);
    if (theErr != QTSS_NoErr)
        return theErr;
        
    if (theErr == QTSS_NoErr)
    {
        // If caller provided a buffer that's too small for this attribute, report that error
        if (tempValueLen > *ioValueLen)
            theErr = QTSS_NotEnoughSpace;
            
        // Only copy out the attribute if the buffer is big enough
        if ((ioValueBuffer != NULL) && (theErr == QTSS_NoErr))
            ::memcpy(ioValueBuffer, tempValueBuffer, tempValueLen);
            
        // Always set the ioValueLen to be the actual length of the attribute.
        *ioValueLen = tempValueLen;
    }

    return QTSS_NoErr;
}

//GetValueAsString
/* 利用QTSSDictionary::GetValuePtr()来获取指定属性ID和多值索引的属性的缓存地址和长度,并QTSSDataConverter::ValueToString()做类型转换.
   注意是非抢占安全的,所以要加互斥锁 */
QTSS_Error QTSSDictionary::GetValueAsString(QTSS_AttributeID inAttrID, UInt32 inIndex, char** outString)
{
    void* tempValueBuffer;
    UInt32 tempValueLen = 0;

	/* 确保入参非空,以便存放数据 */
    if (outString == NULL)  
        return QTSS_BadArgument;
        
    OSMutexLocker locker(fMutexP);
    QTSS_Error theErr = this->GetValuePtr(inAttrID, inIndex, &tempValueBuffer,
                                            &tempValueLen, true);
    if (theErr != QTSS_NoErr)
        return theErr;

    /* 获取DictionaryMap,以便下面获取属性的索引 */   
    QTSSDictionaryMap* theMap = fMap;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
        theMap = fInstanceMap;

    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* 获取属性的索引 */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    Assert(theMapIndex >= 0);
    
	/* 将值作指定类型转换 */
    *outString = QTSSDataConverter::ValueToString(tempValueBuffer, tempValueLen, theMap->GetAttrType(theMapIndex));
    return QTSS_NoErr;
}


//CreateObjectValue
/* used in QTSSPrefs::SetObjectValuesFromFile() */
/* 创建指定属性ID和多值索引的New Dictionary Object,并设置fIsDynamicDictionary为true */
QTSS_Error QTSSDictionary::CreateObjectValue(QTSS_AttributeID inAttrID, UInt32* outIndex,
                                        QTSSDictionary** newObject, QTSSDictionaryMap* inMap, UInt32 inFlags)
{
    // Check first to see if this is a static attribute or an instance attribute
	/* 获取DictionaryMap和DictValueElement* */
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }
    
    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* 获取属性的索引 */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    
    // If there is a mutex, make this action atomic.
    OSMutexLocker locker(fMutexP);
    
    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
	/* 若该属性只能读 */
    if ((!(inFlags & kDontObeyReadOnly)) && (!theMap->IsWriteable(theMapIndex)))
        return QTSS_ReadOnly;
	/* 若该属性是removed attr */
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
	/* 若不是qtssAttrDataTypeQTSS_Object */
    if (theMap->GetAttrType(theMapIndex) != qtssAttrDataTypeQTSS_Object)
        return QTSS_BadArgument;
    
	/* 多值属性个数 */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

    // if normal QTSSObjects have been added, then we can't add a dynamic one
	/* 若是多值属性,且不是DynamicDictionary */
    if (!theAttrs[theMapIndex].fIsDynamicDictionary && (numValues > 0))
        return QTSS_ReadOnly;

    QTSSDictionary* oldDict = NULL;
    *outIndex = numValues;  // add the object into the next spot

    UInt32 len = sizeof(QTSSDictionary*);
    QTSSDictionary* dict = CreateNewDictionary(inMap, fMutexP);
    
    // kind of a hack to avoid the check in SetValue
    theAttrs[theMapIndex].fIsDynamicDictionary = false;
    QTSS_Error err = SetValue(inAttrID, *outIndex, &dict, len, inFlags);
    if (err != QTSS_NoErr)
    {
        delete dict;
        return err;
    }
    
    if (oldDict != NULL)
    {
        delete oldDict;
    }
    
    theAttrs[theMapIndex].fIsDynamicDictionary = true;
    *newObject = dict;
    
    return QTSS_NoErr;
}

//SetValue
/* 注意该函数是非常核心的QTSSDictionary类的函数,非常重要! */
/* 先找到添加属性值的缓存位置并分配足够的缓存空间(有可能调整或新建,若原来是字符串类型的属性,就将原来的字符串存储变为二维字符指针数组存储),
   再将指定属性ID,多值索引的属性值从入参inBuffer处取出指定长度inLen,添加到指定缓存位置 */
QTSS_Error QTSSDictionary::SetValue(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                        const void* inBuffer,  UInt32 inLen,
                                        UInt32 inFlags)
{
	/* 注意下面的套路与QTSSDictionary::CreateObjectValue()完全相同 */
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }
    
    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* 获得指定属性ID对应的属性Index */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    
    // If there is a mutex, make this action atomic.
    OSMutexLocker locker(fMutexP);
    
    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
    if ((!(inFlags & kDontObeyReadOnly)) && (!theMap->IsWriteable(theMapIndex)))
        return QTSS_ReadOnly;
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
    if (theAttrs[theMapIndex].fIsDynamicDictionary)
        return QTSS_ReadOnly;
    
	/* 多值索引总数 */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

	/* 获得指定属性Index对应的DataType */
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);

	/* 注意这个量很重要 */
    UInt32 attrLen = inLen;

	/* 假如是字符串类型的属性,就要做特别处理 */
    if (dataType == qtssAttrDataTypeCharArray)
    {
		/* 假如是多值属性,设置这个量很重要 */
        if (inIndex > 0)
            attrLen = sizeof(char*);    //4, value just contains a pointer
        
		/* 假如原来是单值属性,现在增加第二个属性值(inIndex=1) */
        if ((numValues == 1) && (inIndex == 1))
        {
            // we're adding a second value, so we need to change the storage from directly
            // storing the string to an array of string pointers
          
            // creating new memory here just to create a null terminated string
            // instead of directly using the old storage as the old storage didn't 
            // have its string null terminated

			/* 创建新内存temp来替换原来内存,仅是使末尾为'\0',内容与原来相同,并删去原来的内存 */

            UInt32 tempStringLen = theAttrs[theMapIndex].fAttributeData.Len;/* 4个字符长 */
            char* temp = NEW char[tempStringLen + 1];
            ::memcpy(temp, theAttrs[theMapIndex].fAttributeData.Ptr, tempStringLen);/* "****\0" */
            temp[tempStringLen] = '\0';
            delete [] theAttrs[theMapIndex].fAttributeData.Ptr;
                        
            //char* temp = theAttrs[theMapIndex].fAttributeData.Ptr;
            
			/* 注意这里的理解顺序,要结合上面的来!即:先分配16个字符的内存,temp是一个字符串"****\0",将原来的字符串(16*4字节长)放在字符串数组中作第一个元素 */
            theAttrs[theMapIndex].fAllocatedLen = 16 * sizeof(char*);/* 64 */
            theAttrs[theMapIndex].fAttributeData.Ptr = NEW char[theAttrs[theMapIndex].fAllocatedLen];
            theAttrs[theMapIndex].fAttributeData.Len = sizeof(char*);//4
            // store off original string as first value in array
			/******************* NOTE IMPORTANT !!*************************************/
			/* 将原来的第一个属性值(inIndex=0)放入新建的缓存(字符指针数组)作第一个元素 */
            *(char**)theAttrs[theMapIndex].fAttributeData.Ptr = temp;
                        // question: why isn't theAttrs[theMapIndex].fAllocatedInternally set to true?
        }
    }
    else
    {
        // If this attribute is iterated, this new value
        // must be the same size as all the others.
		/* 假如是多值索引的iterated attr,它的数据长度应与其它属性长度相同 */
        if (((inIndex > 0) || (numValues > 1))
                 &&(theAttrs[theMapIndex].fAttributeData.Len != 0) && (inLen != theAttrs[theMapIndex].fAttributeData.Len))
            return QTSS_BadArgument;
    }
    
    //
    // Can't put empty space into the array of values
	/* check 多值索引的validity */
    if (inIndex > numValues)
        return QTSS_BadIndex;
     
	/* 假如原来分配的缓存空间不够,就要分配2倍大小的新缓存,并及时更新数据成员的值 */
    if ((attrLen * (inIndex + 1)) > theAttrs[theMapIndex].fAllocatedLen)
    {
        // We need to reallocate this buffer.
		/* 需要分配缓存的长度 */
        UInt32 theLen;
        
		/* 假如入参inIndex指定的是原(最初的)属性值 */
        if (inIndex == 0)
            theLen = attrLen;   // most attributes are single valued, so allocate just enough space
        else
			/* 若是新增的属性 */
            theLen = 2 * (attrLen * (inIndex + 1));// Allocate twice as much as we need

		/* 现在分配指定长度的缓存 */
        char* theNewBuffer = NEW char[theLen];

		/* 当有多值索引时,将原来的buffer数据复制进新建的buffer中 */
        if (inIndex > 0)
        {
            // Copy out the old attribute data
            ::memcpy(theNewBuffer, theAttrs[theMapIndex].fAttributeData.Ptr,
                        theAttrs[theMapIndex].fAllocatedLen);
        }
        
        // Now get rid of the old stuff. Delete the buffer
        // if it was already allocated internally
		/* 若是系统原来从内部分配的,就要删去它 */
        if (theAttrs[theMapIndex].fAllocatedInternally)
            delete [] theAttrs[theMapIndex].fAttributeData.Ptr;
        
        // Finally, update this attribute structure with all the new values.
		/* 及时更新属性 */
        theAttrs[theMapIndex].fAttributeData.Ptr = theNewBuffer;
        theAttrs[theMapIndex].fAllocatedLen = theLen;
        theAttrs[theMapIndex].fAllocatedInternally = true;
    }
        
    // At this point, we should always have enough space to write what we want
	/* 断言此时一定有足够空间去存放新的属性值,因为由上面的if loop保证了 */
    Assert(theAttrs[theMapIndex].fAllocatedLen >= (attrLen * (inIndex + 1)));
    
    // Copy the new data to the right place in our data buffer

    /* 确定存放属性值的缓存地址,设置分两种情形,见下面 */
    void *attributeBufferPtr;

	/* 若不是字符串类型的属性或者仅是修改第一个索引的属性值 */
    if ((dataType != qtssAttrDataTypeCharArray) || ((numValues < 2) && (inIndex == 0)))
    {
		/* 设置放入属性值的缓存地址指针和长度 */
        attributeBufferPtr = theAttrs[theMapIndex].fAttributeData.Ptr + (inLen * inIndex);
		/* 由入参指定属性数据长度 */
        theAttrs[theMapIndex].fAttributeData.Len = inLen;
    }
    else /* 若是字符型的第二个属性 */
    {
        //attributeBufferPtr = NEW char[inLen];
        // allocating one extra so that we can null terminate the string
		/* 使tempBuffer成为null terminated C-String */
        attributeBufferPtr = NEW char[inLen + 1];
        char* tempBuffer = (char*)attributeBufferPtr;
        tempBuffer[inLen] = '\0';
                
        //char** valuePtr = (char**)theAttrs[theMapIndex].fAttributeData.Ptr + (inLen * inIndex);
        // The offset should be (attrLen * inIndex) and not (inLen * inIndex),参见上面对字符串情形的处理 
		/* 确定设置属性值的起点,放入二维数组 */
        char** valuePtr = (char**)(theAttrs[theMapIndex].fAttributeData.Ptr + (attrLen * inIndex));
		/* 不要替换已存在的属性值 */
        if (inIndex < numValues)    // we're replacing an existing string
            delete *valuePtr;
		/* 将新建缓存起点attributeBufferPtr放进该二维数组要设置属性值的起点,即新建的属性值是放在二维数组中的 */
        *valuePtr = (char*)attributeBufferPtr;
    }
    
	/******************* NOTE IMPORTANT !!*************************************/
	/* 将指定缓存中的指定长度的值复制到指定位置 */
    ::memcpy(attributeBufferPtr, inBuffer, inLen);
    

    // Set the number of attributes to be proper
	/* 使多值索引至多增加1 */
    if (inIndex >= theAttrs[theMapIndex].fNumAttributes)
    {
        //
        // We should never have to increment num attributes by more than 1
        Assert(theAttrs[theMapIndex].fNumAttributes == inIndex);
        theAttrs[theMapIndex].fNumAttributes++;
    }

    //
    // Call the completion routine
	/* 注意kDontCallCompletionRoutine是QTSSDictionary类的,而kCompleteFunctionsAllowed是QTSSDictionaryMap类的 */
    if (((fMap == NULL) || fMap->CompleteFunctionsAllowed()) && !(inFlags & kDontCallCompletionRoutine))
        this->SetValueComplete(theMapIndex, theMap, inIndex, attributeBufferPtr, inLen);
    
    return QTSS_NoErr;
}

//SetValuePtr
/* 用指定的属性ID(假设单属性值)和缓存指针,来设置指定长度的属性数据 */
QTSS_Error QTSSDictionary::SetValuePtr(QTSS_AttributeID inAttrID,
                                        const void* inBuffer,  UInt32 inLen,
                                        UInt32 inFlags)
{
	/* 注意下面的套路与QTSSDictionary::CreateObjectValue()/SetValue()完全相同 */
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }
    
    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    
    // If there is a mutex, make this action atomic.
    OSMutexLocker locker(fMutexP);
    
    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
    if ((!(inFlags & kDontObeyReadOnly)) && (!theMap->IsWriteable(theMapIndex)))
        return QTSS_ReadOnly;
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
    if (theAttrs[theMapIndex].fIsDynamicDictionary)
        return QTSS_ReadOnly;
    
	/* 多值索引总数 */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;
    if ((numValues > 0) || (theAttrs[theMapIndex].fAttributeData.Ptr != NULL))
        return QTSS_BadArgument;    // you can only set the pointer if you haven't done set value

    theAttrs[theMapIndex].fAttributeData.Ptr = (char*) inBuffer;
    theAttrs[theMapIndex].fAttributeData.Len = inLen;
    theAttrs[theMapIndex].fAllocatedLen = inLen;
    
    // This function assumes there is only one value and that it isn't allocated internally
    theAttrs[theMapIndex].fNumAttributes = 1;
        
        return QTSS_NoErr;
}

//RemoveValue
/* 分多种情形,删去指定属性ID,多值索引的属性值 */
QTSS_Error QTSSDictionary::RemoveValue(QTSS_AttributeID inAttrID, UInt32 inIndex, UInt32 inFlags)
{
	/* 注意下面的套路与QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()完全相同 */
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }
    
    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    
    // If there is a mutex, make this action atomic.
    OSMutexLocker locker(fMutexP);
    
    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
    if ((!(inFlags & kDontObeyReadOnly)) && (!theMap->IsWriteable(theMapIndex)))
        return QTSS_ReadOnly;
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
    if ((theMap->GetAttrFunction(theMapIndex) != NULL) && (inIndex > 0))
        return QTSS_BadIndex;
     
	/* 多值索引总数 */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

	/* 属性数据长度,对于多值属性,它们的属性数据长度是相同的 */
    UInt32 theValueLen = theAttrs[theMapIndex].fAttributeData.Len;

	/* 假如该属性是qtssAttrDataTypeQTSS_Object类型 */
    if (theAttrs[theMapIndex].fIsDynamicDictionary)
    {
        // this is an internally allocated dictionary, so we need to desctruct it
        Assert(theMap->GetAttrType(theMapIndex) == qtssAttrDataTypeQTSS_Object);
        Assert(theValueLen == sizeof(QTSSDictionary*));
		/* 确定在二维数组中的位置 */
        QTSSDictionary* dict = *(QTSSDictionary**)(theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex));
        delete dict;
    }
    
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);
	/* 假如该属性是多值索引(非第一个)的qtssAttrDataTypeCharArray类型 */
    if ((dataType == qtssAttrDataTypeCharArray) && (numValues > 1))
    {
        // we need to delete the string
        char* str = *(char**)(theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex));
        delete str;
    }

    //
    // If there are values after this one in the array, move them.
	/* 将删去属性后面的属性前移,以免中间留下空格 */
    if (inIndex + 1 < theAttrs[theMapIndex].fNumAttributes) 
    {	
        ::memmove(  theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex),
                theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * (inIndex + 1)),
                theValueLen * ( (theAttrs[theMapIndex].fNumAttributes) - inIndex - 1));
    } // else this is the last in the array so just truncate.
    //
    // Update our number of values
	/* 使属性个数减一 */
    theAttrs[theMapIndex].fNumAttributes--;
    if (theAttrs[theMapIndex].fNumAttributes == 0)
        theAttrs[theMapIndex].fAttributeData.Len = 0;

	/* 假如该属性是多值索引(第一个)的qtssAttrDataTypeCharArray类型 */
    if ((dataType == qtssAttrDataTypeCharArray) && (theAttrs[theMapIndex].fNumAttributes == 1))
    {
        // we only have one string left, so we don't need the extra pointer
        char* str = *(char**)(theAttrs[theMapIndex].fAttributeData.Ptr);
        delete theAttrs[theMapIndex].fAttributeData.Ptr;
        theAttrs[theMapIndex].fAttributeData.Ptr = str;
        theAttrs[theMapIndex].fAttributeData.Len = strlen(str);
        theAttrs[theMapIndex].fAllocatedLen = strlen(str);
    }

    //
    // Call the completion routine
    if (((fMap == NULL) || fMap->CompleteFunctionsAllowed()) && !(inFlags & kDontCallCompletionRoutine))
        this->RemoveValueComplete(theMapIndex, theMap, inIndex);
        
    return QTSS_NoErr;
}

//GetNumValues
/* 利用QTSSDictionaryMap中的工具函数,来获取指定属性ID的属性个数(显然是多值属性) */
UInt32  QTSSDictionary::GetNumValues(QTSS_AttributeID inAttrID)
{
	/* 注意下面的套路与QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()/RemoveValue()完全相同 */
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }

    if (theMap == NULL)
        return 0;

    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    if (theMapIndex < 0)
        return 0;

    return theAttrs[theMapIndex].fNumAttributes;
}

//SetNumValues
/* 依据入参,设置指定属性ID的属性个数(比原来小),对internally allocated dictionary object或者字符数组属性要循环地从最后一个开始删去多余的属性值 */
void    QTSSDictionary::SetNumValues(QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
	/* 注意下面的套路与QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()/RemoveValue()/GetNumValues()完全相同 */
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }

    if (theMap == NULL)
        return;

    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    if (theMapIndex < 0)
        return;

	/* 获取指定属性ID的属性个数(显然是多值属性) */
    UInt32 numAttributes = theAttrs[theMapIndex].fNumAttributes;
    // this routine can only be ever used to reduce the number of values
	/* 本函数只能设置属性个数变小的情况 */
    if (inNumValues >= numAttributes || numAttributes == 0)
        return;

	/* 获取指定属性的数据类型 */
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);

	/* 当是internally allocated dictionary object或者字符数组属性时,总是循环地从最后一个开始删去多余的属性值 */
    if (theAttrs[theMapIndex].fIsDynamicDictionary || (dataType == qtssAttrDataTypeCharArray))
    {         
        // getting rid of dictionaries or strings is tricky, so it's easier to call remove value
        for (UInt32 removeCount = numAttributes - inNumValues; removeCount > 0; removeCount--)
        {	// the delete index passed to RemoveValue is always the last in the array.
            this->RemoveValue(inAttrID, theAttrs[theMapIndex].fNumAttributes - 1, kDontObeyReadOnly);
        }
    }
    else /* 对其它的属性数据类型,简单设置即可 */
    {
        theAttrs[theMapIndex].fNumAttributes = inNumValues;
        if (inNumValues == 0)
            theAttrs[theMapIndex].fAttributeData.Len = 0;
    }
}

//SetVal
/* 将入参传递进来,设置数据成员fAttributes内的指定分量的结构体DictValueElement内其它3个成员的值,不设置fNumAttributes & fAllocatedInternally */
void    QTSSDictionary::SetVal( QTSS_AttributeID inAttrID,
                                    void* inValueBuffer,
                                    UInt32 inBufferLen)
{ 
    Assert(inAttrID >= 0);
	/* 确保QTSSDictionaryMap存在 */
    Assert(fMap);
	/* 确保inAttrID指定的属性在QTSSDictionaryMap中存在 */
    Assert((UInt32)inAttrID < fMap->GetNumAttrs());

	/* 将入参的值传递进来 */
    fAttributes[inAttrID].fAttributeData.Ptr = (char*)inValueBuffer;
    fAttributes[inAttrID].fAttributeData.Len = inBufferLen;
    fAttributes[inAttrID].fAllocatedLen = inBufferLen;
    
    // This function assumes there is only one value and that it isn't allocated internally
    fAttributes[inAttrID].fNumAttributes = 1;
}

//SetEmptyVal
/* 用指定的缓存指针和长度来设置指定属性ID的相应项 */
void    QTSSDictionary::SetEmptyVal(QTSS_AttributeID inAttrID, void* inBuf, UInt32 inBufLen)
{
    Assert(inAttrID >= 0);
    Assert(fMap);
	/* 确定指定属性ID的合法性 */
    Assert((UInt32)inAttrID < fMap->GetNumAttrs());
    fAttributes[inAttrID].fAttributeData.Ptr = (char*)inBuf;
    fAttributes[inAttrID].fAllocatedLen = inBufLen;

#if !ALLOW_NON_WORD_ALIGN_ACCESS
    //if (((UInt32) inBuf % 4) > 0)
    //  qtss_printf("bad align by %d\n",((UInt32) inBuf % 4) );
    Assert( ((PointerSizedInt) inBuf % 4) == 0 );
#endif
}

//ATTRIBUTE-SPECIFIC CALL ROUTINES
//
//AddInstanceAttribute
/* 创建指定参数的实例属性,加入Dictionary Map,并对实例属性数组大小进行调整 */
QTSS_Error  QTSSDictionary::AddInstanceAttribute(   const char* inAttrName,
                                                    QTSS_AttrFunctionPtr inFuncPtr,
                                                    QTSS_AttrDataType inDataType,
                                                    QTSS_AttrPermission inPermission )
{
	/* 假如不允许添加实例属性,返回该错误 */
    if ((fMap != NULL) && !fMap->InstanceAttrsAllowed())
        return QTSS_InstanceAttrsNotAllowed;
        
    OSMutexLocker locker(fMutexP);

    //
    // Check to see if this attribute exists in the static map. If it does,
    // we can't add it as an instance attribute, so return an error
    QTSSAttrInfoDict* throwAway = NULL;
    QTSS_Error theErr;

	/* 获取指定属性名的静态属性 */
    if (fMap != NULL)
    {
        theErr = fMap->GetAttrInfoByName(inAttrName, &throwAway);
        if (theErr == QTSS_NoErr)
            return QTSS_AttrNameExists;
    }
    
	/* 假如实例DictionaryMap是空的,就创建它 */
    if (fInstanceMap == NULL)
    {
		/* 设置flag */
        UInt32 theFlags = QTSSDictionaryMap::kAllowRemoval | QTSSDictionaryMap::kIsInstanceMap;
        if ((fMap == NULL) || fMap->CompleteFunctionsAllowed())
            theFlags |= QTSSDictionaryMap::kCompleteFunctionsAllowed;
        /* 注意第一个参数表示static Attribute为0个 */    
        fInstanceMap = new QTSSDictionaryMap( 0, theFlags );
    }
    
    //
    // Add the attribute into the Dictionary Map.
	/* 把参数全部继承过来 */
	/* 首先检查入参给定的属性名称是否和已有的属性名称相同,若是且是previously removed attribute,就重置该属性,否则该属性已存在,无须重置.
	假如属性数组已满无法加入新的属性,就新建一个新的大小为2倍的属性数组,将原属性数组数据复制过来.再由入参创建一个新的属性并重设它的值 */
    theErr = fInstanceMap->AddAttribute(inAttrName, inFuncPtr, inDataType, inPermission);
    if (theErr != QTSS_NoErr)
        return theErr;
    
    //
    // Check to see if our DictValueElement array needs to be reallocated  
	/* 假如实例属性数组大小不够时,新建一个原大小2倍的实例属性数组 */
    if (fInstanceMap->GetNumAttrs() >= fInstanceArraySize)
    {
        UInt32 theNewArraySize = fInstanceArraySize * 2;
        if (theNewArraySize == 0)
            theNewArraySize = QTSSDictionaryMap::kMinArraySize;//20
		/* 确保实例属性数组大小足够大 */
        Assert(theNewArraySize > fInstanceMap->GetNumAttrs());
        
		/* 新建一个原大小2倍的实例属性数组 */
        DictValueElement* theNewArray = NEW DictValueElement[theNewArraySize];
		/* 将原来的属性数据转移过来,并删除原来的属性数组 */
        if (fInstanceAttrs != NULL)
        {
            ::memcpy(theNewArray, fInstanceAttrs, sizeof(DictValueElement) * fInstanceArraySize);

            //
            // Delete the old instance attr structs, this does not delete the actual attribute memory
            delete [] fInstanceAttrs;
        }

		/* 及时更新数据成员 */
        fInstanceAttrs = theNewArray;
        fInstanceArraySize = theNewArraySize;
    }
    return QTSS_NoErr;
}

/* 删除指定属性ID的实例属性 */
QTSS_Error  QTSSDictionary::RemoveInstanceAttribute(QTSS_AttributeID inAttr)
{
    OSMutexLocker locker(fMutexP);

    if (fInstanceMap != NULL)
    {   
        QTSS_Error theErr = fInstanceMap->CheckRemovePermission(inAttr);
        if (theErr != QTSS_NoErr)
            return theErr;

        this->SetNumValues(inAttr,(UInt32) 0); // make sure to set num values to 0 since it is a deleted attribute
        fInstanceMap->RemoveAttribute(inAttr);
    }
    else
        return QTSS_BadArgument;
    
    //
    // Call the completion routine
    SInt32 theMapIndex = fInstanceMap->ConvertAttrIDToArrayIndex(inAttr);
    this->RemoveInstanceAttrComplete(theMapIndex, fInstanceMap);
    
    return QTSS_NoErr;
}

// GetAttrInfoByIndex
/* 首先计算静态属性和实例属性个数的总和,来判断入参的范围,进而来得到静态属性或实例属性信息 */
QTSS_Error QTSSDictionary::GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoDict)
{
    if (outAttrInfoDict == NULL)
        return QTSS_BadArgument;
        
    OSMutexLocker locker(fMutexP);

    UInt32 numInstanceValues = 0;
    UInt32 numStaticValues = 0;
    
	/* 获取static Attributes个数 */
    if (fMap != NULL)
        numStaticValues = fMap->GetNumNonRemovedAttrs();
    
	/* 获取instance Attributes个数 */
    if (fInstanceMap != NULL)
        numInstanceValues = fInstanceMap->GetNumNonRemovedAttrs();
    
    if (inIndex >= (numStaticValues + numInstanceValues))
        return QTSS_AttrDoesntExist;
    
	/* 当入参是小于numStaticValues时,获取的是static Attributes info */
    if ( (numStaticValues > 0)  && (inIndex < numStaticValues) )
        return fMap->GetAttrInfoByIndex(inIndex, outAttrInfoDict);
    else /* 否则获取的是instance Attributes info */
    {
        Assert(fInstanceMap != NULL);
		/* 注意此时索引要作相应调整  */
        return fInstanceMap->GetAttrInfoByIndex(inIndex - numStaticValues, outAttrInfoDict);
    }
}

//GetAttrInfoByID
/* 对入参inAttrID,先判断是实例属性还是静态属性,来分别获取实例属性或静态属性信息 */
QTSS_Error QTSSDictionary::GetAttrInfoByID(QTSS_AttributeID inAttrID, QTSSAttrInfoDict** outAttrInfoDict)
{
    if (outAttrInfoDict == NULL)
        return QTSS_BadArgument;
        
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        OSMutexLocker locker(fMutexP);

        if (fInstanceMap != NULL)
            return fInstanceMap->GetAttrInfoByID(inAttrID, outAttrInfoDict);
    }
    else
        if (fMap != NULL) return fMap->GetAttrInfoByID(inAttrID, outAttrInfoDict);
            
    return QTSS_AttrDoesntExist;
}

//GetAttrInfoByName
/* 先默认获取静态属性的信息,若不成功,就获取实例属性的信息 */
QTSS_Error QTSSDictionary::GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict)
{
	/* 初始化这个量 */
    QTSS_Error theErr = QTSS_AttrDoesntExist;
    if (outAttrInfoDict == NULL)
        return QTSS_BadArgument;
        
    // Retrieve the Dictionary Map for this object type
    if (fMap != NULL)
        theErr = fMap->GetAttrInfoByName(inAttrName, outAttrInfoDict);
    
	/* 假如上面的量没有成功获得 */
    if (theErr == QTSS_AttrDoesntExist)
    {
        OSMutexLocker locker(fMutexP);
        if (fInstanceMap != NULL)
            theErr = fInstanceMap->GetAttrInfoByName(inAttrName, outAttrInfoDict);
    }
    return theErr;
}

//DeleteAttributeData
/* 对给定的数组inDictValues,来删除指定数目的属性数据(假如是内部分配的内存的话) */
void QTSSDictionary::DeleteAttributeData(DictValueElement* inDictValues, UInt32 inNumValues)
{
    for (UInt32 x = 0; x < inNumValues; x++)
    {
		/* 假如是内部分配的内存的话 */
        if (inDictValues[x].fAllocatedInternally)
            delete [] inDictValues[x].fAttributeData.Ptr;
    }
}

/***************** class QTSSAttrInfoDict ***************************/
//
//QTSSAttrInfoDict-specific 
//stuct array sAttributes[], 
//refer to QTSSDictionary.h line 252 and refer to QTSSModule.h line 159
QTSSAttrInfoDict::AttrInfo  QTSSAttrInfoDict::sAttributes[] =
{
    /* 0 */ { "qtssAttrName",       NULL,       qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1 */ { "qtssAttrID",         NULL,       qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 2 */ { "qtssAttrDataType",   NULL,       qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 3 */ { "qtssAttrPermissions",NULL,       qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe }
};

// constructor/destructor
QTSSAttrInfoDict::QTSSAttrInfoDict()
: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kAttrInfoDictIndex)), fID(qtssIllegalAttrID)
{}

QTSSAttrInfoDict::~QTSSAttrInfoDict() {}


/***************** class QTSSDictionaryMap ***************************/

//QTSSDictionaryMap-specific
//
//refer to QTSSDictionary.h line 412
QTSSDictionaryMap*      QTSSDictionaryMap::sDictionaryMaps[kNumDictionaries + kNumDynamicDictionaryTypes];//515 size
UInt32                  QTSSDictionaryMap::sNextDynamicMap = kNumDictionaries;//15

//Initialize, refer to QTSSDictionary.h line 264
/* 逐个创建每个分量的constructor QTSSDictionaryMap()来初始化数据成员sDictionaryMaps[] */
void QTSSDictionaryMap::Initialize()
{
    //
    // Have to do this one first because this dict map is used by all the other
    // dict maps.
	/* 注意,这第12个分量要首先创建,因为其它分量要用到 */
	/* 注意在QTSS.h中,QTSS_AttrInfoObjec有4个参数 */
    sDictionaryMaps[kAttrInfoDictIndex/*12*/]     = new QTSSDictionaryMap(qtssAttrInfoNumParams/*4*/);

    // Setup the Attr Info attributes before constructing any other dictionaries
	/* 循环设置属性 */
    for (UInt32 x = 0; x < qtssAttrInfoNumParams; x++)
        sDictionaryMaps[kAttrInfoDictIndex]->SetAttribute(x, QTSSAttrInfoDict::sAttributes[x].fAttrName,
                                                            QTSSAttrInfoDict::sAttributes[x].fFuncPtr,
                                                            QTSSAttrInfoDict::sAttributes[x].fAttrDataType,
                                                            QTSSAttrInfoDict::sAttributes[x].fAttrPermission);

	/* 对字典映射中的其他14个QTSSDictionaryMap也初始化 */
    sDictionaryMaps[kServerDictIndex]       = new QTSSDictionaryMap(qtssSvrNumParams, QTSSDictionaryMap::kCompleteFunctionsAllowed);
    sDictionaryMaps[kPrefsDictIndex]        = new QTSSDictionaryMap(qtssPrefsNumParams, QTSSDictionaryMap::kInstanceAttrsAllowed | QTSSDictionaryMap::kCompleteFunctionsAllowed);
    sDictionaryMaps[kTextMessagesDictIndex] = new QTSSDictionaryMap(qtssMsgNumParams);
    sDictionaryMaps[kServiceDictIndex]      = new QTSSDictionaryMap(0);
    sDictionaryMaps[kRTPStreamDictIndex]    = new QTSSDictionaryMap(qtssRTPStrNumParams);
	sDictionaryMaps[kClientSessionDictIndex]= new QTSSDictionaryMap(qtssCliSesNumParams, QTSSDictionaryMap::kCompleteFunctionsAllowed);
    sDictionaryMaps[kRTSPSessionDictIndex]  = new QTSSDictionaryMap(qtssRTSPSesNumParams);
    sDictionaryMaps[kRTSPRequestDictIndex]  = new QTSSDictionaryMap(qtssRTSPReqNumParams);
    sDictionaryMaps[kRTSPHeaderDictIndex]   = new QTSSDictionaryMap(qtssNumHeaders);
    sDictionaryMaps[kFileDictIndex]         = new QTSSDictionaryMap(qtssFlObjNumParams);
    sDictionaryMaps[kModuleDictIndex]       = new QTSSDictionaryMap(qtssModNumParams);
    sDictionaryMaps[kModulePrefsDictIndex]  = new QTSSDictionaryMap(0, QTSSDictionaryMap::kInstanceAttrsAllowed | QTSSDictionaryMap::kCompleteFunctionsAllowed);
    sDictionaryMaps[kQTSSUserProfileDictIndex] = new QTSSDictionaryMap(qtssUserNumParams);
    sDictionaryMaps[kQTSSConnectedUserDictIndex] = new QTSSDictionaryMap(qtssConnectionNumParams);
}

// constructor, see QTSSDictionary.h line 281 
/* 依据入参创建指定大小(>=20)属性数组并初始化为0,配置其它参数. */
QTSSDictionaryMap::QTSSDictionaryMap(UInt32 inNumReservedAttrs, UInt32 inFlags)
:   fNextAvailableID(inNumReservedAttrs), fNumValidAttrs(inNumReservedAttrs),fAttrArraySize(inNumReservedAttrs), fFlags(inFlags)
{
    if (fAttrArraySize < kMinArraySize/* 20 */)
        fAttrArraySize = kMinArraySize;
    fAttrArray = NEW QTSSAttrInfoDict*[fAttrArraySize];
    ::memset(fAttrArray, 0, sizeof(QTSSAttrInfoDict*) * fAttrArraySize);
}

//AddAttribute
/* 注意该函数是非常核心的QTSSDictionary类的函数,非常重要! */
/* 首先检查入参给定的属性名称是否和已有的属性名称相同,若是且是previously removed attribute,就重置该属性,否则该属性已存在,无须重置.
   假如属性数组已满无法加入新的属性,就新建一个新的大小为2倍的属性数组,将原属性数组数据复制过来.再由入参创建一个新的属性并重设它的值 */
QTSS_Error QTSSDictionaryMap::AddAttribute( const char* inAttrName,
                                            QTSS_AttrFunctionPtr inFuncPtr,
                                            QTSS_AttrDataType inDataType,
                                            QTSS_AttrPermission inPermission)
{
	/* 判断属性名称是合法的 */
    if (inAttrName == NULL || ::strlen(inAttrName) > QTSS_MAX_ATTRIBUTE_NAME_SIZE/*64*/)
        return QTSS_BadArgument;

    for (UInt32 count = 0; count < fNextAvailableID; count++)
    {
		/* 假如入参的属性名称与某个已有属性的属性名称相同 */
        if  (::strcmp(&fAttrArray[count]->fAttrInfo.fAttrName[0], inAttrName) == 0)
        {   // found the name in the dictionary
            if (fAttrArray[count]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved )
            { // it is a previously removed attribute
				/* 假如入参的数据类型和这个已有属性的数据类型相同,用入参重设这个属性 */
                if (fAttrArray[count]->fAttrInfo.fAttrDataType == inDataType)
                { //same type so reuse the attribute
                    QTSS_AttributeID attrID = fAttrArray[count]->fID; 
                    this->UnRemoveAttribute(attrID); 
                    fAttrArray[count]->fAttrInfo.fFuncPtr = inFuncPtr; // reset
                    fAttrArray[count]->fAttrInfo.fAttrPermission = inPermission;// reset
                    return QTSS_NoErr; // nothing left to do. It is re-added.
                }
                
                // a removed attribute with the same name but different type--so keep checking 
                continue;
            }
            // an error, an active attribute with this name exists
            return QTSS_AttrNameExists;
        }
    }

	/* 假如属性数组已满无法加入新的属性,就新建一个新的大小为2倍的属性数组,将原属性数组数据复制过来 */
    if (fAttrArraySize == fNextAvailableID)
    {
        // If there currently isn't an attribute array, or if the current array
        // is full, allocate a new array and copy all the old stuff over to the new array.
        
		/* 设置新的属性数组的大小 */
        UInt32 theNewArraySize = fAttrArraySize * 2;
        if (theNewArraySize == 0)
            theNewArraySize = kMinArraySize;//20
        
		/* 创建一个新的属性数组并初始化为0 */
        QTSSAttrInfoDict** theNewArray = NEW QTSSAttrInfoDict*[theNewArraySize];
        ::memset(theNewArray, 0, sizeof(QTSSAttrInfoDict*) * theNewArraySize);

		/* 将原有属性数组数据复制过来并删去原来的属性数组 */
        if (fAttrArray != NULL)
        {
            ::memcpy(theNewArray, fAttrArray, sizeof(QTSSAttrInfoDict*) * fAttrArraySize);
            delete [] fAttrArray;
        }

		/* 及时更新本类的数据成员 */
        fAttrArray = theNewArray;
        fAttrArraySize = theNewArraySize;
    }
    
	/* 这是现在新增属性的属性ID */
    QTSS_AttributeID theID = fNextAvailableID;

	/* 及时更新本类的数据成员 */
    fNextAvailableID++;
    fNumValidAttrs++;

	/* 当是kIsInstanceMap时,调整属性ID最高的bit位为1,来暗示是一个instance attr */
    if (fFlags & kIsInstanceMap)
        theID |= 0x80000000; // Set the high order bit to indicate this is an instance attr

    // Copy the information into the first available element
    // Currently, all attributes added in this fashion are always writeable
	/* 由AttributeID获得ArrayIndex,并创建一个新的QTSSAttrInfoDict*类型的数组分量,由入参来设置它的各项值 */
    this->SetAttribute(theID, inAttrName, inFuncPtr, inDataType, inPermission); 
    return QTSS_NoErr;
}

//SetAttribute
/* used in QTSSModule::Initialize() */
/* 由AttributeID获得ArrayIndex,并创建一个新的QTSSAttrInfoDict*类型的数组分量,由入参来设置它的各项值 */
void QTSSDictionaryMap::SetAttribute(   QTSS_AttributeID inID, 
                                        const char* inAttrName,
                                        QTSS_AttrFunctionPtr inFuncPtr,
                                        QTSS_AttrDataType inDataType,
                                        QTSS_AttrPermission inPermission )
{
	/* 由AttributeID获得ArrayIndex */
    UInt32 theIndex = QTSSDictionaryMap::ConvertAttrIDToArrayIndex(inID);
    UInt32 theNameLen = ::strlen(inAttrName);
    Assert(theNameLen < QTSS_MAX_ATTRIBUTE_NAME_SIZE);//64
	/* 确保属性数组的该分量原来没有值 */
    Assert(fAttrArray[theIndex] == NULL);
    
	/* 新建一个数组的分量 */
    fAttrArray[theIndex] = NEW QTSSAttrInfoDict;
    
    //Copy the information into the first available element
	/* 从入参获取相应的信息 */
    fAttrArray[theIndex]->fID = inID;
        
    ::strcpy(&fAttrArray[theIndex]->fAttrInfo.fAttrName[0], inAttrName);
    fAttrArray[theIndex]->fAttrInfo.fFuncPtr = inFuncPtr;
    fAttrArray[theIndex]->fAttrInfo.fAttrDataType = inDataType; 
    fAttrArray[theIndex]->fAttrInfo.fAttrPermission = inPermission;
    
	/* 将上面得到的值去设置QTSS_AttrInfoObjectAttributes里相应items的值 */
	/* 将上述设置写进字典中 */
    fAttrArray[theIndex]->SetVal(qtssAttrName, &fAttrArray[theIndex]->fAttrInfo.fAttrName[0], theNameLen);
    fAttrArray[theIndex]->SetVal(qtssAttrID, &fAttrArray[theIndex]->fID, sizeof(fAttrArray[theIndex]->fID));
    fAttrArray[theIndex]->SetVal(qtssAttrDataType, &fAttrArray[theIndex]->fAttrInfo.fAttrDataType, sizeof(fAttrArray[theIndex]->fAttrInfo.fAttrDataType));
    fAttrArray[theIndex]->SetVal(qtssAttrPermissions, &fAttrArray[theIndex]->fAttrInfo.fAttrPermission, sizeof(fAttrArray[theIndex]->fAttrInfo.fAttrPermission));
}

//CheckRemovePermission
/* 检查指定的属性是否是kAllowRemoval?QTSS_AttrPermission是否qtssAttrModeDelete? */
QTSS_Error  QTSSDictionaryMap::CheckRemovePermission(QTSS_AttributeID inAttrID)
{
    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inAttrID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
    
    if (0 == (fAttrArray[theIndex]->fAttrInfo.fAttrPermission & qtssAttrModeDelete))
         return QTSS_BadArgument;

    if (!(fFlags & kAllowRemoval))
        return QTSS_BadArgument;
    
    return QTSS_NoErr;
}

//RemoveAttribute
/* 设置指定属性的权限,标记其是可移去的(最高bit位1),使fNumValidAttrs减一 */
QTSS_Error  QTSSDictionaryMap::RemoveAttribute(QTSS_AttributeID inAttrID)
{
    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inAttrID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
    
	/* 确保该属性可removal */
    Assert(fFlags & kAllowRemoval);
    if (!(fFlags & kAllowRemoval))
        return QTSS_BadArgument;
    
    //qtss_printf("QTSSDictionaryMap::RemoveAttribute arraySize=%lu numNonRemove= %lu fAttrArray[%lu]->fAttrInfo.fAttrName=%s\n",this->GetNumAttrs(), this->GetNumNonRemovedAttrs(), theIndex,fAttrArray[theIndex]->fAttrInfo.fAttrName);
    //
    // Don't actually touch the attribute or anything. Just flag the
    // it as removed.
    fAttrArray[theIndex]->fAttrInfo.fAttrPermission |= qtssPrivateAttrModeRemoved;
    fNumValidAttrs--;
    Assert(fNumValidAttrs < 1000000);
    return QTSS_NoErr;
}

//UnRemoveAttribute
/* 将移去的属性恢复(最高bit位置0),fNumValidAttrs++ */
QTSS_Error  QTSSDictionaryMap::UnRemoveAttribute(QTSS_AttributeID inAttrID)
{
    if (this->ConvertAttrIDToArrayIndex(inAttrID) == -1)
        return QTSS_AttrDoesntExist;
    
    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inAttrID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
     
	/* 更改属性权限,确保最高位为0 */
    fAttrArray[theIndex]->fAttrInfo.fAttrPermission &= ~qtssPrivateAttrModeRemoved;
    
    fNumValidAttrs++;
    return QTSS_NoErr;
}

//GetAttrInfoByName
/* 在已有的属性中查找给定的属性,并用找到的属性信息设置第二个入参的值,第三个参数说明不返回invalid attr(最高位为1) */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoObject,
                                                    Bool16 returnRemovedAttr)/* 是否返回invalid attr(最高位为1)? */
{
	/* 存放修改信息的地方 */
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;

	/* 在已有的属性中查找给定的属性 */
    for (UInt32 count = 0; count < fNextAvailableID; count++)
    {
        if (::strcmp(&fAttrArray[count]->fAttrInfo.fAttrName[0], inAttrName) == 0)
        {
			/* 假如是invalid attr(最高位为1)并且不返回Removed Attr, 就继续查找 */
            if ((fAttrArray[count]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved) && (!returnRemovedAttr))
                continue;
              
			/* 用找到的信息设置入参的值 */
            *outAttrInfoObject = fAttrArray[count];
            return QTSS_NoErr;
        }   
    }
    return QTSS_AttrDoesntExist;
}

//GetAttrInfoByID
/* 查找指定属性ID的属性信息outAttrInfoObject,不返回invalid attr(最高位为1) */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByID(QTSS_AttributeID inID, QTSSAttrInfoDict** outAttrInfoObject)
{
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;

    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
    
	/* 不返回invalid attr(最高位为1) */
    if (fAttrArray[theIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved)
        return QTSS_AttrDoesntExist;
        
    *outAttrInfoObject = fAttrArray[theIndex];
    return QTSS_NoErr;
}

// GetAttrInfoByIndex
/* 从指定索引查找相应的属性信息outAttrInfoObject,要分两种情形,分该属性是removed attr和非removed attr处理 */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoObject)
{
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;
	/* 属性索引不超过数据成员fNumValidAttrs */
    if (inIndex >= this->GetNumNonRemovedAttrs())
        return QTSS_AttrDoesntExist;
        
    UInt32 actualIndex = inIndex;
	/* 得到fNextAvailableID */
    UInt32 max = this->GetNumAttrs();
	/* 假如是removed attr时,要从整个map中查找 */
    if (fFlags & kAllowRemoval)
    {
        // If this dictionary map allows attributes to be removed, then
        // the iteration index and array indexes won't line up exactly, so
        // we have to iterate over the whole map all the time
        actualIndex = 0;
        for (UInt32 x = 0; x < max; x++)
        {   
			/* 假如是removed attr时,跳过继续查找 */ 
			if (fAttrArray[x] && (fAttrArray[x]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved) )
            {   continue;
            }
             
			/* 找到入参对应的索引,把它记录下来 */
            if (actualIndex == inIndex)
            {   actualIndex = x;
                break;
            }
            actualIndex++;
        }
    }
    //qtss_printf("QTSSDictionaryMap::GetAttrInfoByIndex arraySize=%lu numNonRemove= %lu fAttrArray[%lu]->fAttrInfo.fAttrName=%s\n",this->GetNumAttrs(), this->GetNumNonRemovedAttrs(), actualIndex,fAttrArray[actualIndex]->fAttrInfo.fAttrName);
    Assert(actualIndex < fNextAvailableID);
	/* 确保是非removed attr */
    Assert(!(fAttrArray[actualIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved));
    *outAttrInfoObject = fAttrArray[actualIndex];
    return QTSS_NoErr;
}

// GetAttrID
/* 由属性名称获取属性信息theAttrInfo,再由theAttrInfo获得属性ID */
QTSS_Error  QTSSDictionaryMap::GetAttrID(const char* inAttrName, QTSS_AttributeID* outID)
{
    if (outID == NULL)
        return QTSS_BadArgument;

    QTSSAttrInfoDict* theAttrInfo = NULL;
    QTSS_Error theErr = this->GetAttrInfoByName(inAttrName, &theAttrInfo);
    if (theErr == QTSS_NoErr)
        *outID = theAttrInfo->fID;

    return theErr;
}

// GetMapIndex
/* 仅对reserved dictionary,将其类型为UInt32的QTSS_ObjectType（参见QTSS.h）,转换为相应的DictionaryMap Index并返回,否则一概返回kIllegalDictionary */
UInt32  QTSSDictionaryMap::GetMapIndex(QTSS_ObjectType inType)
{
	/* 假如是reserved dictionary,就直接返回其值(UInt32,<15) */
     if (inType < sNextDynamicMap)
        return inType;
     
	 /* 进一步分情形讨论,通过名称确定对应关系 */
     switch (inType)
     {
        case qtssRTPStreamObjectType:       return kRTPStreamDictIndex;
        case qtssClientSessionObjectType:   return kClientSessionDictIndex;
        case qtssRTSPSessionObjectType:     return kRTSPSessionDictIndex;
        case qtssRTSPRequestObjectType:     return kRTSPRequestDictIndex;
        case qtssRTSPHeaderObjectType:      return kRTSPHeaderDictIndex;
        case qtssServerObjectType:          return kServerDictIndex;
        case qtssPrefsObjectType:           return kPrefsDictIndex;
        case qtssTextMessagesObjectType:    return kTextMessagesDictIndex;
        case qtssFileObjectType:            return kFileDictIndex;
        case qtssModuleObjectType:          return kModuleDictIndex;
        case qtssModulePrefsObjectType:     return kModulePrefsDictIndex;
        case qtssAttrInfoObjectType:        return kAttrInfoDictIndex;
        case qtssUserProfileObjectType:     return kQTSSUserProfileDictIndex;
        case qtssConnectedUserObjectType:   return kQTSSConnectedUserDictIndex;
        default:                            return kIllegalDictionary;
     }
     return kIllegalDictionary;
}

// CreateNewMap
/* 仅创建动态的Dictionary Map,并初始化,返回该QTSS_ObjectType.注意sNextDynamicMap非常重要. */
QTSS_ObjectType QTSSDictionaryMap::CreateNewMap()
{
	/* 假如服务器映射数到头了,就不创建 */
    if (sNextDynamicMap == kNumDictionaries + kNumDynamicDictionaryTypes)
        return 0;
        
    sDictionaryMaps[sNextDynamicMap] = new QTSSDictionaryMap(0);
    QTSS_ObjectType result = (QTSS_ObjectType)sNextDynamicMap;
    sNextDynamicMap++;
    
    return result;
}
