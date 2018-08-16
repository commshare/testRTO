
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
/* ע��QTSSDictionaryMap����Ҫ��!��Ҫ���QTSSDictionaryMap��OSMutex */
QTSSDictionary::QTSSDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex) 
:   fAttributes(NULL), fInstanceAttrs(NULL), fInstanceArraySize(0),
    fMap(inMap), fInstanceMap(NULL), fMutexP(inMutex), fMyMutex(false), fLocked(false)
{
	/* ���õõ���QTSSDictionaryMap�õ�available attr�ĸ���,�ٴ�����Ӧ���������� */
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
		/* ��ȫ����������ɾ�� */
        this->DeleteAttributeData(fAttributes, fMap->GetNumAttrs());
    if (fAttributes != NULL)
        delete [] fAttributes;
    delete fInstanceMap;
	/* ɾ��ȫ��ʵ���������� */
    this->DeleteAttributeData(fInstanceAttrs, fInstanceArraySize);
    delete [] fInstanceAttrs;
	if (fMyMutex)
		delete fMutexP;
}

// CreateNewDictionary, see QTSSDictionary.h line 196

/* ���ù��캯��QTSSDictionary()�����µ�Dictionary! */
QTSSDictionary* QTSSDictionary::CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex)
{
    return NEW QTSSDictionary(inMap, inMutex);
}

// QTSS API VALUE / ATTRIBUTE CALLBACK ROUTINES
//
//  GetValuePtr
/* ע��ú����Ƿǳ����ĵ�QTSSDictionary��ĺ���,�ǳ���Ҫ! */
/* ����ָ������ID�Ͷ�ֵ����������,ȷ�����Ļ����ַ�ͳ���(��������,�ĸ����),�м�����������Ĵ��� */
QTSS_Error QTSSDictionary::GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                            void** outValueBuffer, UInt32* outValueLen,
                                            Bool16 isInternal)
{
    // Check first to see if this is a static attribute or an instance attribute
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
	/* �ж��Ƿ���ʵ������(���λbit��ֵ��1)?��,�ͽ�ʵ��ӳ���ʵ�����Դ��ݽ��� */
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }

    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* ��ȡ��������MapIndex,ʵ���Ͼ����������λΪ0 */
	/* ���������,Ϊ�������inIndex���ֿ� */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);

	/* ����κ��½���local variable���Ϸ��Լ�� */

    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
	/* ����removed attr(���λΪ1),�ͱ��� */
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
	/* ����������Բ����ڲ���,����ǿ�Ʒ��ʰ�ȫ��,û�м���,�ͱ��� */
    if ((!isInternal) && (!theMap->IsPreemptiveSafe(theMapIndex)) && !this->IsLocked())
        return QTSS_NotPreemptiveSafe;
    // An iterated attribute cannot have a param retrieval function
	/* iterated attribute�ж��ֵ,������param retrieval function */
    if ((inIndex > 0) && (theMap->GetAttrFunction(theMapIndex) != NULL))
        return QTSS_BadIndex;
    // Check to make sure the index parameter is legal
	/* ���ó���available attr���ܸ���! */
    if ((inIndex > 0) && (inIndex >= theAttrs[theMapIndex].fNumAttributes))
        return QTSS_BadIndex;
        
        
    // Retrieve the parameter
	/* ����λ�ȡ��ص���Ϣ */
    char* theBuffer = theAttrs[theMapIndex].fAttributeData.Ptr;
    *outValueLen = theAttrs[theMapIndex].fAttributeData.Len;
    
	/* �����Կ��Է�����? */
	Bool16 cacheable = theMap->IsCacheable(theMapIndex);

	/* ������Ի�����Ժ���ָ��,������������:���ɷ�����û������ֵʱ;�����ɷ���ʱ. ǰ�߿�������GetAttrFunction��ȡָ��������ֵ */
	if ( (theMap->GetAttrFunction(theMapIndex) != NULL) && ((cacheable && (*outValueLen == 0)) || !cacheable) )
    {
        // If function is cacheable: 
		// If the parameter doesn't have a value assigned yet, and there is an attribute
        // retrieval function provided, invoke that function now.
		// If function is *not* cacheable:
		// always call the function
		
		/* ע��theMap->GetAttrFunction(theMapIndex)�ķ���ֵ�Ǹ�QTSS_AttrFunctionPtr,��������ԭ�Ͷ���,��ȡ�ڶ���������Ӧ�ľ���ֵ */
        theBuffer = (char*)theMap->GetAttrFunction(theMapIndex)(this, outValueLen);

        //If the param retrieval function didn't return an explicit value for this attribute,
        //refetch the parameter out of the array, in case the function modified it.
        
		/* ��QTSS_AttrFunctionPtr()����ֵΪ��,������֮����ȡ�������,�Է���������޸���.
		   ���ǸĻ�ԭ����Ԥ��ֵ! */
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
	/* ������ȷȡ������,��������û������ֵ(Ϊ0),���ش��� */
    if (*outValueLen == 0)
        return QTSS_ValueNotFound;
     
	/* ע�����inIndex��iterated attribute�Ķ�ֵ��������,ȷ����������������bufferλ�� */
    theBuffer += theAttrs[theMapIndex].fAttributeData.Len * inIndex;
    *outValueBuffer = theBuffer;
        
    // strings need an extra dereference(������) - moved it up
	/* ������Ϊ������Ե��ַ�������ʱ,���������ڴ��ַ�ͳ���(��Ҫ��ǡ������) */
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
/* ����QTSSDictionary::GetValuePtr()����ȡָ������ID�Ͷ�ֵ���������ԵĻ����ַ�ͳ���,���������ioValueLen��ֵ��ʵ�ʵ����ݳ���.
   ע���Ƿ���ռ��ȫ��,����Ҫ�ӻ����� */
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
/* ����QTSSDictionary::GetValuePtr()����ȡָ������ID�Ͷ�ֵ���������ԵĻ����ַ�ͳ���,��QTSSDataConverter::ValueToString()������ת��.
   ע���Ƿ���ռ��ȫ��,����Ҫ�ӻ����� */
QTSS_Error QTSSDictionary::GetValueAsString(QTSS_AttributeID inAttrID, UInt32 inIndex, char** outString)
{
    void* tempValueBuffer;
    UInt32 tempValueLen = 0;

	/* ȷ����ηǿ�,�Ա������� */
    if (outString == NULL)  
        return QTSS_BadArgument;
        
    OSMutexLocker locker(fMutexP);
    QTSS_Error theErr = this->GetValuePtr(inAttrID, inIndex, &tempValueBuffer,
                                            &tempValueLen, true);
    if (theErr != QTSS_NoErr)
        return theErr;

    /* ��ȡDictionaryMap,�Ա������ȡ���Ե����� */   
    QTSSDictionaryMap* theMap = fMap;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
        theMap = fInstanceMap;

    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* ��ȡ���Ե����� */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    Assert(theMapIndex >= 0);
    
	/* ��ֵ��ָ������ת�� */
    *outString = QTSSDataConverter::ValueToString(tempValueBuffer, tempValueLen, theMap->GetAttrType(theMapIndex));
    return QTSS_NoErr;
}


//CreateObjectValue
/* used in QTSSPrefs::SetObjectValuesFromFile() */
/* ����ָ������ID�Ͷ�ֵ������New Dictionary Object,������fIsDynamicDictionaryΪtrue */
QTSS_Error QTSSDictionary::CreateObjectValue(QTSS_AttributeID inAttrID, UInt32* outIndex,
                                        QTSSDictionary** newObject, QTSSDictionaryMap* inMap, UInt32 inFlags)
{
    // Check first to see if this is a static attribute or an instance attribute
	/* ��ȡDictionaryMap��DictValueElement* */
    QTSSDictionaryMap* theMap = fMap;
    DictValueElement* theAttrs = fAttributes;
    if (QTSSDictionaryMap::IsInstanceAttrID(inAttrID))
    {
        theMap = fInstanceMap;
        theAttrs = fInstanceAttrs;
    }
    
    if (theMap == NULL)
        return QTSS_AttrDoesntExist;
    
	/* ��ȡ���Ե����� */
    SInt32 theMapIndex = theMap->ConvertAttrIDToArrayIndex(inAttrID);
    
    // If there is a mutex, make this action atomic.
    OSMutexLocker locker(fMutexP);
    
    if (theMapIndex < 0)
        return QTSS_AttrDoesntExist;
	/* ��������ֻ�ܶ� */
    if ((!(inFlags & kDontObeyReadOnly)) && (!theMap->IsWriteable(theMapIndex)))
        return QTSS_ReadOnly;
	/* ����������removed attr */
    if (theMap->IsRemoved(theMapIndex))
        return QTSS_AttrDoesntExist;
	/* ������qtssAttrDataTypeQTSS_Object */
    if (theMap->GetAttrType(theMapIndex) != qtssAttrDataTypeQTSS_Object)
        return QTSS_BadArgument;
    
	/* ��ֵ���Ը��� */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

    // if normal QTSSObjects have been added, then we can't add a dynamic one
	/* ���Ƕ�ֵ����,�Ҳ���DynamicDictionary */
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
/* ע��ú����Ƿǳ����ĵ�QTSSDictionary��ĺ���,�ǳ���Ҫ! */
/* ���ҵ��������ֵ�Ļ���λ�ò������㹻�Ļ���ռ�(�п��ܵ������½�,��ԭ�����ַ������͵�����,�ͽ�ԭ�����ַ����洢��Ϊ��ά�ַ�ָ������洢),
   �ٽ�ָ������ID,��ֵ����������ֵ�����inBuffer��ȡ��ָ������inLen,��ӵ�ָ������λ�� */
QTSS_Error QTSSDictionary::SetValue(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                        const void* inBuffer,  UInt32 inLen,
                                        UInt32 inFlags)
{
	/* ע���������·��QTSSDictionary::CreateObjectValue()��ȫ��ͬ */
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
    
	/* ���ָ������ID��Ӧ������Index */
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
    
	/* ��ֵ�������� */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

	/* ���ָ������Index��Ӧ��DataType */
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);

	/* ע�����������Ҫ */
    UInt32 attrLen = inLen;

	/* �������ַ������͵�����,��Ҫ���ر��� */
    if (dataType == qtssAttrDataTypeCharArray)
    {
		/* �����Ƕ�ֵ����,�������������Ҫ */
        if (inIndex > 0)
            attrLen = sizeof(char*);    //4, value just contains a pointer
        
		/* ����ԭ���ǵ�ֵ����,�������ӵڶ�������ֵ(inIndex=1) */
        if ((numValues == 1) && (inIndex == 1))
        {
            // we're adding a second value, so we need to change the storage from directly
            // storing the string to an array of string pointers
          
            // creating new memory here just to create a null terminated string
            // instead of directly using the old storage as the old storage didn't 
            // have its string null terminated

			/* �������ڴ�temp���滻ԭ���ڴ�,����ʹĩβΪ'\0',������ԭ����ͬ,��ɾȥԭ�����ڴ� */

            UInt32 tempStringLen = theAttrs[theMapIndex].fAttributeData.Len;/* 4���ַ��� */
            char* temp = NEW char[tempStringLen + 1];
            ::memcpy(temp, theAttrs[theMapIndex].fAttributeData.Ptr, tempStringLen);/* "****\0" */
            temp[tempStringLen] = '\0';
            delete [] theAttrs[theMapIndex].fAttributeData.Ptr;
                        
            //char* temp = theAttrs[theMapIndex].fAttributeData.Ptr;
            
			/* ע����������˳��,Ҫ����������!��:�ȷ���16���ַ����ڴ�,temp��һ���ַ���"****\0",��ԭ�����ַ���(16*4�ֽڳ�)�����ַ�������������һ��Ԫ�� */
            theAttrs[theMapIndex].fAllocatedLen = 16 * sizeof(char*);/* 64 */
            theAttrs[theMapIndex].fAttributeData.Ptr = NEW char[theAttrs[theMapIndex].fAllocatedLen];
            theAttrs[theMapIndex].fAttributeData.Len = sizeof(char*);//4
            // store off original string as first value in array
			/******************* NOTE IMPORTANT !!*************************************/
			/* ��ԭ���ĵ�һ������ֵ(inIndex=0)�����½��Ļ���(�ַ�ָ������)����һ��Ԫ�� */
            *(char**)theAttrs[theMapIndex].fAttributeData.Ptr = temp;
                        // question: why isn't theAttrs[theMapIndex].fAllocatedInternally set to true?
        }
    }
    else
    {
        // If this attribute is iterated, this new value
        // must be the same size as all the others.
		/* �����Ƕ�ֵ������iterated attr,�������ݳ���Ӧ���������Գ�����ͬ */
        if (((inIndex > 0) || (numValues > 1))
                 &&(theAttrs[theMapIndex].fAttributeData.Len != 0) && (inLen != theAttrs[theMapIndex].fAttributeData.Len))
            return QTSS_BadArgument;
    }
    
    //
    // Can't put empty space into the array of values
	/* check ��ֵ������validity */
    if (inIndex > numValues)
        return QTSS_BadIndex;
     
	/* ����ԭ������Ļ���ռ䲻��,��Ҫ����2����С���»���,����ʱ�������ݳ�Ա��ֵ */
    if ((attrLen * (inIndex + 1)) > theAttrs[theMapIndex].fAllocatedLen)
    {
        // We need to reallocate this buffer.
		/* ��Ҫ���仺��ĳ��� */
        UInt32 theLen;
        
		/* �������inIndexָ������ԭ(�����)����ֵ */
        if (inIndex == 0)
            theLen = attrLen;   // most attributes are single valued, so allocate just enough space
        else
			/* �������������� */
            theLen = 2 * (attrLen * (inIndex + 1));// Allocate twice as much as we need

		/* ���ڷ���ָ�����ȵĻ��� */
        char* theNewBuffer = NEW char[theLen];

		/* ���ж�ֵ����ʱ,��ԭ����buffer���ݸ��ƽ��½���buffer�� */
        if (inIndex > 0)
        {
            // Copy out the old attribute data
            ::memcpy(theNewBuffer, theAttrs[theMapIndex].fAttributeData.Ptr,
                        theAttrs[theMapIndex].fAllocatedLen);
        }
        
        // Now get rid of the old stuff. Delete the buffer
        // if it was already allocated internally
		/* ����ϵͳԭ�����ڲ������,��Ҫɾȥ�� */
        if (theAttrs[theMapIndex].fAllocatedInternally)
            delete [] theAttrs[theMapIndex].fAttributeData.Ptr;
        
        // Finally, update this attribute structure with all the new values.
		/* ��ʱ�������� */
        theAttrs[theMapIndex].fAttributeData.Ptr = theNewBuffer;
        theAttrs[theMapIndex].fAllocatedLen = theLen;
        theAttrs[theMapIndex].fAllocatedInternally = true;
    }
        
    // At this point, we should always have enough space to write what we want
	/* ���Դ�ʱһ�����㹻�ռ�ȥ����µ�����ֵ,��Ϊ�������if loop��֤�� */
    Assert(theAttrs[theMapIndex].fAllocatedLen >= (attrLen * (inIndex + 1)));
    
    // Copy the new data to the right place in our data buffer

    /* ȷ���������ֵ�Ļ����ַ,���÷���������,������ */
    void *attributeBufferPtr;

	/* �������ַ������͵����Ի��߽����޸ĵ�һ������������ֵ */
    if ((dataType != qtssAttrDataTypeCharArray) || ((numValues < 2) && (inIndex == 0)))
    {
		/* ���÷�������ֵ�Ļ����ַָ��ͳ��� */
        attributeBufferPtr = theAttrs[theMapIndex].fAttributeData.Ptr + (inLen * inIndex);
		/* �����ָ���������ݳ��� */
        theAttrs[theMapIndex].fAttributeData.Len = inLen;
    }
    else /* �����ַ��͵ĵڶ������� */
    {
        //attributeBufferPtr = NEW char[inLen];
        // allocating one extra so that we can null terminate the string
		/* ʹtempBuffer��Ϊnull terminated C-String */
        attributeBufferPtr = NEW char[inLen + 1];
        char* tempBuffer = (char*)attributeBufferPtr;
        tempBuffer[inLen] = '\0';
                
        //char** valuePtr = (char**)theAttrs[theMapIndex].fAttributeData.Ptr + (inLen * inIndex);
        // The offset should be (attrLen * inIndex) and not (inLen * inIndex),�μ�������ַ������εĴ��� 
		/* ȷ����������ֵ�����,�����ά���� */
        char** valuePtr = (char**)(theAttrs[theMapIndex].fAttributeData.Ptr + (attrLen * inIndex));
		/* ��Ҫ�滻�Ѵ��ڵ�����ֵ */
        if (inIndex < numValues)    // we're replacing an existing string
            delete *valuePtr;
		/* ���½��������attributeBufferPtr�Ž��ö�ά����Ҫ��������ֵ�����,���½�������ֵ�Ƿ��ڶ�ά�����е� */
        *valuePtr = (char*)attributeBufferPtr;
    }
    
	/******************* NOTE IMPORTANT !!*************************************/
	/* ��ָ�������е�ָ�����ȵ�ֵ���Ƶ�ָ��λ�� */
    ::memcpy(attributeBufferPtr, inBuffer, inLen);
    

    // Set the number of attributes to be proper
	/* ʹ��ֵ������������1 */
    if (inIndex >= theAttrs[theMapIndex].fNumAttributes)
    {
        //
        // We should never have to increment num attributes by more than 1
        Assert(theAttrs[theMapIndex].fNumAttributes == inIndex);
        theAttrs[theMapIndex].fNumAttributes++;
    }

    //
    // Call the completion routine
	/* ע��kDontCallCompletionRoutine��QTSSDictionary���,��kCompleteFunctionsAllowed��QTSSDictionaryMap��� */
    if (((fMap == NULL) || fMap->CompleteFunctionsAllowed()) && !(inFlags & kDontCallCompletionRoutine))
        this->SetValueComplete(theMapIndex, theMap, inIndex, attributeBufferPtr, inLen);
    
    return QTSS_NoErr;
}

//SetValuePtr
/* ��ָ��������ID(���赥����ֵ)�ͻ���ָ��,������ָ�����ȵ��������� */
QTSS_Error QTSSDictionary::SetValuePtr(QTSS_AttributeID inAttrID,
                                        const void* inBuffer,  UInt32 inLen,
                                        UInt32 inFlags)
{
	/* ע���������·��QTSSDictionary::CreateObjectValue()/SetValue()��ȫ��ͬ */
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
    
	/* ��ֵ�������� */
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
/* �ֶ�������,ɾȥָ������ID,��ֵ����������ֵ */
QTSS_Error QTSSDictionary::RemoveValue(QTSS_AttributeID inAttrID, UInt32 inIndex, UInt32 inFlags)
{
	/* ע���������·��QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()��ȫ��ͬ */
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
     
	/* ��ֵ�������� */
    UInt32 numValues = theAttrs[theMapIndex].fNumAttributes;

	/* �������ݳ���,���ڶ�ֵ����,���ǵ��������ݳ�������ͬ�� */
    UInt32 theValueLen = theAttrs[theMapIndex].fAttributeData.Len;

	/* �����������qtssAttrDataTypeQTSS_Object���� */
    if (theAttrs[theMapIndex].fIsDynamicDictionary)
    {
        // this is an internally allocated dictionary, so we need to desctruct it
        Assert(theMap->GetAttrType(theMapIndex) == qtssAttrDataTypeQTSS_Object);
        Assert(theValueLen == sizeof(QTSSDictionary*));
		/* ȷ���ڶ�ά�����е�λ�� */
        QTSSDictionary* dict = *(QTSSDictionary**)(theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex));
        delete dict;
    }
    
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);
	/* ����������Ƕ�ֵ����(�ǵ�һ��)��qtssAttrDataTypeCharArray���� */
    if ((dataType == qtssAttrDataTypeCharArray) && (numValues > 1))
    {
        // we need to delete the string
        char* str = *(char**)(theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex));
        delete str;
    }

    //
    // If there are values after this one in the array, move them.
	/* ��ɾȥ���Ժ��������ǰ��,�����м����¿ո� */
    if (inIndex + 1 < theAttrs[theMapIndex].fNumAttributes) 
    {	
        ::memmove(  theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * inIndex),
                theAttrs[theMapIndex].fAttributeData.Ptr + (theValueLen * (inIndex + 1)),
                theValueLen * ( (theAttrs[theMapIndex].fNumAttributes) - inIndex - 1));
    } // else this is the last in the array so just truncate.
    //
    // Update our number of values
	/* ʹ���Ը�����һ */
    theAttrs[theMapIndex].fNumAttributes--;
    if (theAttrs[theMapIndex].fNumAttributes == 0)
        theAttrs[theMapIndex].fAttributeData.Len = 0;

	/* ����������Ƕ�ֵ����(��һ��)��qtssAttrDataTypeCharArray���� */
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
/* ����QTSSDictionaryMap�еĹ��ߺ���,����ȡָ������ID�����Ը���(��Ȼ�Ƕ�ֵ����) */
UInt32  QTSSDictionary::GetNumValues(QTSS_AttributeID inAttrID)
{
	/* ע���������·��QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()/RemoveValue()��ȫ��ͬ */
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
/* �������,����ָ������ID�����Ը���(��ԭ��С),��internally allocated dictionary object�����ַ���������Ҫѭ���ش����һ����ʼɾȥ���������ֵ */
void    QTSSDictionary::SetNumValues(QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
	/* ע���������·��QTSSDictionary::CreateObjectValue()/SetValue()/SetValuePtr()/RemoveValue()/GetNumValues()��ȫ��ͬ */
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

	/* ��ȡָ������ID�����Ը���(��Ȼ�Ƕ�ֵ����) */
    UInt32 numAttributes = theAttrs[theMapIndex].fNumAttributes;
    // this routine can only be ever used to reduce the number of values
	/* ������ֻ���������Ը�����С����� */
    if (inNumValues >= numAttributes || numAttributes == 0)
        return;

	/* ��ȡָ�����Ե��������� */
    QTSS_AttrDataType dataType = theMap->GetAttrType(theMapIndex);

	/* ����internally allocated dictionary object�����ַ���������ʱ,����ѭ���ش����һ����ʼɾȥ���������ֵ */
    if (theAttrs[theMapIndex].fIsDynamicDictionary || (dataType == qtssAttrDataTypeCharArray))
    {         
        // getting rid of dictionaries or strings is tricky, so it's easier to call remove value
        for (UInt32 removeCount = numAttributes - inNumValues; removeCount > 0; removeCount--)
        {	// the delete index passed to RemoveValue is always the last in the array.
            this->RemoveValue(inAttrID, theAttrs[theMapIndex].fNumAttributes - 1, kDontObeyReadOnly);
        }
    }
    else /* ��������������������,�����ü��� */
    {
        theAttrs[theMapIndex].fNumAttributes = inNumValues;
        if (inNumValues == 0)
            theAttrs[theMapIndex].fAttributeData.Len = 0;
    }
}

//SetVal
/* ����δ��ݽ���,�������ݳ�ԱfAttributes�ڵ�ָ�������Ľṹ��DictValueElement������3����Ա��ֵ,������fNumAttributes & fAllocatedInternally */
void    QTSSDictionary::SetVal( QTSS_AttributeID inAttrID,
                                    void* inValueBuffer,
                                    UInt32 inBufferLen)
{ 
    Assert(inAttrID >= 0);
	/* ȷ��QTSSDictionaryMap���� */
    Assert(fMap);
	/* ȷ��inAttrIDָ����������QTSSDictionaryMap�д��� */
    Assert((UInt32)inAttrID < fMap->GetNumAttrs());

	/* ����ε�ֵ���ݽ��� */
    fAttributes[inAttrID].fAttributeData.Ptr = (char*)inValueBuffer;
    fAttributes[inAttrID].fAttributeData.Len = inBufferLen;
    fAttributes[inAttrID].fAllocatedLen = inBufferLen;
    
    // This function assumes there is only one value and that it isn't allocated internally
    fAttributes[inAttrID].fNumAttributes = 1;
}

//SetEmptyVal
/* ��ָ���Ļ���ָ��ͳ���������ָ������ID����Ӧ�� */
void    QTSSDictionary::SetEmptyVal(QTSS_AttributeID inAttrID, void* inBuf, UInt32 inBufLen)
{
    Assert(inAttrID >= 0);
    Assert(fMap);
	/* ȷ��ָ������ID�ĺϷ��� */
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
/* ����ָ��������ʵ������,����Dictionary Map,����ʵ�����������С���е��� */
QTSS_Error  QTSSDictionary::AddInstanceAttribute(   const char* inAttrName,
                                                    QTSS_AttrFunctionPtr inFuncPtr,
                                                    QTSS_AttrDataType inDataType,
                                                    QTSS_AttrPermission inPermission )
{
	/* ���粻�������ʵ������,���ظô��� */
    if ((fMap != NULL) && !fMap->InstanceAttrsAllowed())
        return QTSS_InstanceAttrsNotAllowed;
        
    OSMutexLocker locker(fMutexP);

    //
    // Check to see if this attribute exists in the static map. If it does,
    // we can't add it as an instance attribute, so return an error
    QTSSAttrInfoDict* throwAway = NULL;
    QTSS_Error theErr;

	/* ��ȡָ���������ľ�̬���� */
    if (fMap != NULL)
    {
        theErr = fMap->GetAttrInfoByName(inAttrName, &throwAway);
        if (theErr == QTSS_NoErr)
            return QTSS_AttrNameExists;
    }
    
	/* ����ʵ��DictionaryMap�ǿյ�,�ʹ����� */
    if (fInstanceMap == NULL)
    {
		/* ����flag */
        UInt32 theFlags = QTSSDictionaryMap::kAllowRemoval | QTSSDictionaryMap::kIsInstanceMap;
        if ((fMap == NULL) || fMap->CompleteFunctionsAllowed())
            theFlags |= QTSSDictionaryMap::kCompleteFunctionsAllowed;
        /* ע���һ��������ʾstatic AttributeΪ0�� */    
        fInstanceMap = new QTSSDictionaryMap( 0, theFlags );
    }
    
    //
    // Add the attribute into the Dictionary Map.
	/* �Ѳ���ȫ���̳й��� */
	/* ���ȼ����θ��������������Ƿ�����е�����������ͬ,��������previously removed attribute,�����ø�����,����������Ѵ���,��������.
	�����������������޷������µ�����,���½�һ���µĴ�СΪ2������������,��ԭ�����������ݸ��ƹ���.������δ���һ���µ����Բ���������ֵ */
    theErr = fInstanceMap->AddAttribute(inAttrName, inFuncPtr, inDataType, inPermission);
    if (theErr != QTSS_NoErr)
        return theErr;
    
    //
    // Check to see if our DictValueElement array needs to be reallocated  
	/* ����ʵ�����������С����ʱ,�½�һ��ԭ��С2����ʵ���������� */
    if (fInstanceMap->GetNumAttrs() >= fInstanceArraySize)
    {
        UInt32 theNewArraySize = fInstanceArraySize * 2;
        if (theNewArraySize == 0)
            theNewArraySize = QTSSDictionaryMap::kMinArraySize;//20
		/* ȷ��ʵ�����������С�㹻�� */
        Assert(theNewArraySize > fInstanceMap->GetNumAttrs());
        
		/* �½�һ��ԭ��С2����ʵ���������� */
        DictValueElement* theNewArray = NEW DictValueElement[theNewArraySize];
		/* ��ԭ������������ת�ƹ���,��ɾ��ԭ������������ */
        if (fInstanceAttrs != NULL)
        {
            ::memcpy(theNewArray, fInstanceAttrs, sizeof(DictValueElement) * fInstanceArraySize);

            //
            // Delete the old instance attr structs, this does not delete the actual attribute memory
            delete [] fInstanceAttrs;
        }

		/* ��ʱ�������ݳ�Ա */
        fInstanceAttrs = theNewArray;
        fInstanceArraySize = theNewArraySize;
    }
    return QTSS_NoErr;
}

/* ɾ��ָ������ID��ʵ������ */
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
/* ���ȼ��㾲̬���Ժ�ʵ�����Ը������ܺ�,���ж���εķ�Χ,�������õ���̬���Ի�ʵ��������Ϣ */
QTSS_Error QTSSDictionary::GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoDict)
{
    if (outAttrInfoDict == NULL)
        return QTSS_BadArgument;
        
    OSMutexLocker locker(fMutexP);

    UInt32 numInstanceValues = 0;
    UInt32 numStaticValues = 0;
    
	/* ��ȡstatic Attributes���� */
    if (fMap != NULL)
        numStaticValues = fMap->GetNumNonRemovedAttrs();
    
	/* ��ȡinstance Attributes���� */
    if (fInstanceMap != NULL)
        numInstanceValues = fInstanceMap->GetNumNonRemovedAttrs();
    
    if (inIndex >= (numStaticValues + numInstanceValues))
        return QTSS_AttrDoesntExist;
    
	/* �������С��numStaticValuesʱ,��ȡ����static Attributes info */
    if ( (numStaticValues > 0)  && (inIndex < numStaticValues) )
        return fMap->GetAttrInfoByIndex(inIndex, outAttrInfoDict);
    else /* �����ȡ����instance Attributes info */
    {
        Assert(fInstanceMap != NULL);
		/* ע���ʱ����Ҫ����Ӧ����  */
        return fInstanceMap->GetAttrInfoByIndex(inIndex - numStaticValues, outAttrInfoDict);
    }
}

//GetAttrInfoByID
/* �����inAttrID,���ж���ʵ�����Ի��Ǿ�̬����,���ֱ��ȡʵ�����Ի�̬������Ϣ */
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
/* ��Ĭ�ϻ�ȡ��̬���Ե���Ϣ,�����ɹ�,�ͻ�ȡʵ�����Ե���Ϣ */
QTSS_Error QTSSDictionary::GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict)
{
	/* ��ʼ������� */
    QTSS_Error theErr = QTSS_AttrDoesntExist;
    if (outAttrInfoDict == NULL)
        return QTSS_BadArgument;
        
    // Retrieve the Dictionary Map for this object type
    if (fMap != NULL)
        theErr = fMap->GetAttrInfoByName(inAttrName, outAttrInfoDict);
    
	/* �����������û�гɹ���� */
    if (theErr == QTSS_AttrDoesntExist)
    {
        OSMutexLocker locker(fMutexP);
        if (fInstanceMap != NULL)
            theErr = fInstanceMap->GetAttrInfoByName(inAttrName, outAttrInfoDict);
    }
    return theErr;
}

//DeleteAttributeData
/* �Ը���������inDictValues,��ɾ��ָ����Ŀ����������(�������ڲ�������ڴ�Ļ�) */
void QTSSDictionary::DeleteAttributeData(DictValueElement* inDictValues, UInt32 inNumValues)
{
    for (UInt32 x = 0; x < inNumValues; x++)
    {
		/* �������ڲ�������ڴ�Ļ� */
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
/* �������ÿ��������constructor QTSSDictionaryMap()����ʼ�����ݳ�ԱsDictionaryMaps[] */
void QTSSDictionaryMap::Initialize()
{
    //
    // Have to do this one first because this dict map is used by all the other
    // dict maps.
	/* ע��,���12������Ҫ���ȴ���,��Ϊ��������Ҫ�õ� */
	/* ע����QTSS.h��,QTSS_AttrInfoObjec��4������ */
    sDictionaryMaps[kAttrInfoDictIndex/*12*/]     = new QTSSDictionaryMap(qtssAttrInfoNumParams/*4*/);

    // Setup the Attr Info attributes before constructing any other dictionaries
	/* ѭ���������� */
    for (UInt32 x = 0; x < qtssAttrInfoNumParams; x++)
        sDictionaryMaps[kAttrInfoDictIndex]->SetAttribute(x, QTSSAttrInfoDict::sAttributes[x].fAttrName,
                                                            QTSSAttrInfoDict::sAttributes[x].fFuncPtr,
                                                            QTSSAttrInfoDict::sAttributes[x].fAttrDataType,
                                                            QTSSAttrInfoDict::sAttributes[x].fAttrPermission);

	/* ���ֵ�ӳ���е�����14��QTSSDictionaryMapҲ��ʼ�� */
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
/* ������δ���ָ����С(>=20)�������鲢��ʼ��Ϊ0,������������. */
QTSSDictionaryMap::QTSSDictionaryMap(UInt32 inNumReservedAttrs, UInt32 inFlags)
:   fNextAvailableID(inNumReservedAttrs), fNumValidAttrs(inNumReservedAttrs),fAttrArraySize(inNumReservedAttrs), fFlags(inFlags)
{
    if (fAttrArraySize < kMinArraySize/* 20 */)
        fAttrArraySize = kMinArraySize;
    fAttrArray = NEW QTSSAttrInfoDict*[fAttrArraySize];
    ::memset(fAttrArray, 0, sizeof(QTSSAttrInfoDict*) * fAttrArraySize);
}

//AddAttribute
/* ע��ú����Ƿǳ����ĵ�QTSSDictionary��ĺ���,�ǳ���Ҫ! */
/* ���ȼ����θ��������������Ƿ�����е�����������ͬ,��������previously removed attribute,�����ø�����,����������Ѵ���,��������.
   �����������������޷������µ�����,���½�һ���µĴ�СΪ2������������,��ԭ�����������ݸ��ƹ���.������δ���һ���µ����Բ���������ֵ */
QTSS_Error QTSSDictionaryMap::AddAttribute( const char* inAttrName,
                                            QTSS_AttrFunctionPtr inFuncPtr,
                                            QTSS_AttrDataType inDataType,
                                            QTSS_AttrPermission inPermission)
{
	/* �ж����������ǺϷ��� */
    if (inAttrName == NULL || ::strlen(inAttrName) > QTSS_MAX_ATTRIBUTE_NAME_SIZE/*64*/)
        return QTSS_BadArgument;

    for (UInt32 count = 0; count < fNextAvailableID; count++)
    {
		/* ������ε�����������ĳ���������Ե�����������ͬ */
        if  (::strcmp(&fAttrArray[count]->fAttrInfo.fAttrName[0], inAttrName) == 0)
        {   // found the name in the dictionary
            if (fAttrArray[count]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved )
            { // it is a previously removed attribute
				/* ������ε��������ͺ�����������Ե�����������ͬ,���������������� */
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

	/* �����������������޷������µ�����,���½�һ���µĴ�СΪ2������������,��ԭ�����������ݸ��ƹ��� */
    if (fAttrArraySize == fNextAvailableID)
    {
        // If there currently isn't an attribute array, or if the current array
        // is full, allocate a new array and copy all the old stuff over to the new array.
        
		/* �����µ���������Ĵ�С */
        UInt32 theNewArraySize = fAttrArraySize * 2;
        if (theNewArraySize == 0)
            theNewArraySize = kMinArraySize;//20
        
		/* ����һ���µ��������鲢��ʼ��Ϊ0 */
        QTSSAttrInfoDict** theNewArray = NEW QTSSAttrInfoDict*[theNewArraySize];
        ::memset(theNewArray, 0, sizeof(QTSSAttrInfoDict*) * theNewArraySize);

		/* ��ԭ�������������ݸ��ƹ�����ɾȥԭ������������ */
        if (fAttrArray != NULL)
        {
            ::memcpy(theNewArray, fAttrArray, sizeof(QTSSAttrInfoDict*) * fAttrArraySize);
            delete [] fAttrArray;
        }

		/* ��ʱ���±�������ݳ�Ա */
        fAttrArray = theNewArray;
        fAttrArraySize = theNewArraySize;
    }
    
	/* ���������������Ե�����ID */
    QTSS_AttributeID theID = fNextAvailableID;

	/* ��ʱ���±�������ݳ�Ա */
    fNextAvailableID++;
    fNumValidAttrs++;

	/* ����kIsInstanceMapʱ,��������ID��ߵ�bitλΪ1,����ʾ��һ��instance attr */
    if (fFlags & kIsInstanceMap)
        theID |= 0x80000000; // Set the high order bit to indicate this is an instance attr

    // Copy the information into the first available element
    // Currently, all attributes added in this fashion are always writeable
	/* ��AttributeID���ArrayIndex,������һ���µ�QTSSAttrInfoDict*���͵��������,��������������ĸ���ֵ */
    this->SetAttribute(theID, inAttrName, inFuncPtr, inDataType, inPermission); 
    return QTSS_NoErr;
}

//SetAttribute
/* used in QTSSModule::Initialize() */
/* ��AttributeID���ArrayIndex,������һ���µ�QTSSAttrInfoDict*���͵��������,��������������ĸ���ֵ */
void QTSSDictionaryMap::SetAttribute(   QTSS_AttributeID inID, 
                                        const char* inAttrName,
                                        QTSS_AttrFunctionPtr inFuncPtr,
                                        QTSS_AttrDataType inDataType,
                                        QTSS_AttrPermission inPermission )
{
	/* ��AttributeID���ArrayIndex */
    UInt32 theIndex = QTSSDictionaryMap::ConvertAttrIDToArrayIndex(inID);
    UInt32 theNameLen = ::strlen(inAttrName);
    Assert(theNameLen < QTSS_MAX_ATTRIBUTE_NAME_SIZE);//64
	/* ȷ����������ĸ÷���ԭ��û��ֵ */
    Assert(fAttrArray[theIndex] == NULL);
    
	/* �½�һ������ķ��� */
    fAttrArray[theIndex] = NEW QTSSAttrInfoDict;
    
    //Copy the information into the first available element
	/* ����λ�ȡ��Ӧ����Ϣ */
    fAttrArray[theIndex]->fID = inID;
        
    ::strcpy(&fAttrArray[theIndex]->fAttrInfo.fAttrName[0], inAttrName);
    fAttrArray[theIndex]->fAttrInfo.fFuncPtr = inFuncPtr;
    fAttrArray[theIndex]->fAttrInfo.fAttrDataType = inDataType; 
    fAttrArray[theIndex]->fAttrInfo.fAttrPermission = inPermission;
    
	/* ������õ���ֵȥ����QTSS_AttrInfoObjectAttributes����Ӧitems��ֵ */
	/* ����������д���ֵ��� */
    fAttrArray[theIndex]->SetVal(qtssAttrName, &fAttrArray[theIndex]->fAttrInfo.fAttrName[0], theNameLen);
    fAttrArray[theIndex]->SetVal(qtssAttrID, &fAttrArray[theIndex]->fID, sizeof(fAttrArray[theIndex]->fID));
    fAttrArray[theIndex]->SetVal(qtssAttrDataType, &fAttrArray[theIndex]->fAttrInfo.fAttrDataType, sizeof(fAttrArray[theIndex]->fAttrInfo.fAttrDataType));
    fAttrArray[theIndex]->SetVal(qtssAttrPermissions, &fAttrArray[theIndex]->fAttrInfo.fAttrPermission, sizeof(fAttrArray[theIndex]->fAttrInfo.fAttrPermission));
}

//CheckRemovePermission
/* ���ָ���������Ƿ���kAllowRemoval?QTSS_AttrPermission�Ƿ�qtssAttrModeDelete? */
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
/* ����ָ�����Ե�Ȩ��,������ǿ���ȥ��(���bitλ1),ʹfNumValidAttrs��һ */
QTSS_Error  QTSSDictionaryMap::RemoveAttribute(QTSS_AttributeID inAttrID)
{
    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inAttrID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
    
	/* ȷ�������Կ�removal */
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
/* ����ȥ�����Իָ�(���bitλ��0),fNumValidAttrs++ */
QTSS_Error  QTSSDictionaryMap::UnRemoveAttribute(QTSS_AttributeID inAttrID)
{
    if (this->ConvertAttrIDToArrayIndex(inAttrID) == -1)
        return QTSS_AttrDoesntExist;
    
    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inAttrID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
     
	/* ��������Ȩ��,ȷ�����λΪ0 */
    fAttrArray[theIndex]->fAttrInfo.fAttrPermission &= ~qtssPrivateAttrModeRemoved;
    
    fNumValidAttrs++;
    return QTSS_NoErr;
}

//GetAttrInfoByName
/* �����е������в��Ҹ���������,�����ҵ���������Ϣ���õڶ�����ε�ֵ,����������˵��������invalid attr(���λΪ1) */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoObject,
                                                    Bool16 returnRemovedAttr)/* �Ƿ񷵻�invalid attr(���λΪ1)? */
{
	/* ����޸���Ϣ�ĵط� */
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;

	/* �����е������в��Ҹ��������� */
    for (UInt32 count = 0; count < fNextAvailableID; count++)
    {
        if (::strcmp(&fAttrArray[count]->fAttrInfo.fAttrName[0], inAttrName) == 0)
        {
			/* ������invalid attr(���λΪ1)���Ҳ�����Removed Attr, �ͼ������� */
            if ((fAttrArray[count]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved) && (!returnRemovedAttr))
                continue;
              
			/* ���ҵ�����Ϣ������ε�ֵ */
            *outAttrInfoObject = fAttrArray[count];
            return QTSS_NoErr;
        }   
    }
    return QTSS_AttrDoesntExist;
}

//GetAttrInfoByID
/* ����ָ������ID��������ϢoutAttrInfoObject,������invalid attr(���λΪ1) */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByID(QTSS_AttributeID inID, QTSSAttrInfoDict** outAttrInfoObject)
{
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;

    SInt32 theIndex = this->ConvertAttrIDToArrayIndex(inID);
    if (theIndex < 0)
        return QTSS_AttrDoesntExist;
    
	/* ������invalid attr(���λΪ1) */
    if (fAttrArray[theIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved)
        return QTSS_AttrDoesntExist;
        
    *outAttrInfoObject = fAttrArray[theIndex];
    return QTSS_NoErr;
}

// GetAttrInfoByIndex
/* ��ָ������������Ӧ��������ϢoutAttrInfoObject,Ҫ����������,�ָ�������removed attr�ͷ�removed attr���� */
QTSS_Error  QTSSDictionaryMap::GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoObject)
{
    if (outAttrInfoObject == NULL)
        return QTSS_BadArgument;
	/* �����������������ݳ�ԱfNumValidAttrs */
    if (inIndex >= this->GetNumNonRemovedAttrs())
        return QTSS_AttrDoesntExist;
        
    UInt32 actualIndex = inIndex;
	/* �õ�fNextAvailableID */
    UInt32 max = this->GetNumAttrs();
	/* ������removed attrʱ,Ҫ������map�в��� */
    if (fFlags & kAllowRemoval)
    {
        // If this dictionary map allows attributes to be removed, then
        // the iteration index and array indexes won't line up exactly, so
        // we have to iterate over the whole map all the time
        actualIndex = 0;
        for (UInt32 x = 0; x < max; x++)
        {   
			/* ������removed attrʱ,������������ */ 
			if (fAttrArray[x] && (fAttrArray[x]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved) )
            {   continue;
            }
             
			/* �ҵ���ζ�Ӧ������,������¼���� */
            if (actualIndex == inIndex)
            {   actualIndex = x;
                break;
            }
            actualIndex++;
        }
    }
    //qtss_printf("QTSSDictionaryMap::GetAttrInfoByIndex arraySize=%lu numNonRemove= %lu fAttrArray[%lu]->fAttrInfo.fAttrName=%s\n",this->GetNumAttrs(), this->GetNumNonRemovedAttrs(), actualIndex,fAttrArray[actualIndex]->fAttrInfo.fAttrName);
    Assert(actualIndex < fNextAvailableID);
	/* ȷ���Ƿ�removed attr */
    Assert(!(fAttrArray[actualIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved));
    *outAttrInfoObject = fAttrArray[actualIndex];
    return QTSS_NoErr;
}

// GetAttrID
/* ���������ƻ�ȡ������ϢtheAttrInfo,����theAttrInfo�������ID */
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
/* ����reserved dictionary,��������ΪUInt32��QTSS_ObjectType���μ�QTSS.h��,ת��Ϊ��Ӧ��DictionaryMap Index������,����һ�ŷ���kIllegalDictionary */
UInt32  QTSSDictionaryMap::GetMapIndex(QTSS_ObjectType inType)
{
	/* ������reserved dictionary,��ֱ�ӷ�����ֵ(UInt32,<15) */
     if (inType < sNextDynamicMap)
        return inType;
     
	 /* ��һ������������,ͨ������ȷ����Ӧ��ϵ */
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
/* ��������̬��Dictionary Map,����ʼ��,���ظ�QTSS_ObjectType.ע��sNextDynamicMap�ǳ���Ҫ. */
QTSS_ObjectType QTSSDictionaryMap::CreateNewMap()
{
	/* ���������ӳ������ͷ��,�Ͳ����� */
    if (sNextDynamicMap == kNumDictionaries + kNumDynamicDictionaryTypes)
        return 0;
        
    sDictionaryMaps[sNextDynamicMap] = new QTSSDictionaryMap(0);
    QTSS_ObjectType result = (QTSS_ObjectType)sNextDynamicMap;
    sNextDynamicMap++;
    
    return result;
}
