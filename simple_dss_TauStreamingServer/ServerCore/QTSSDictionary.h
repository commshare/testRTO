
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSDictionary.h
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


#ifndef _QTSSDICTIONARY_H_
#define _QTSSDICTIONARY_H_

#include <stdlib.h>
#include "SafeStdLib.h"
#include "QTSS.h"
#include "OSHeaders.h"
#include "OSMutex.h"
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "QTSSStream.h"  /* ע����base class */

class QTSSDictionary;
class QTSSDictionaryMap;
class QTSSAttrInfoDict;

#define __DICTIONARY_TESTING__ 0

//
// Function prototype for attr functions
/* ����ָ��Ķ���:��ȡ�ڶ���������Ӧ���Եľ���ֵ,�μ�QTSSDictionary::GetValuePtr() */
typedef void* (*QTSS_AttrFunctionPtr)(QTSSDictionary* , UInt32* );

class QTSSDictionary : public QTSSStream
{
    public:
    
        //
        // CONSTRUCTOR / DESTRUCTOR
		/** including GetValue(),SetValue(),CreateValue(),RemoveValue() etc  **/
        
        QTSSDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex = NULL);
        virtual ~QTSSDictionary();
        
        //
        // QTSS API ATTRIBUTE CALLS
        
        // Flags used by internal callers of these routines
		/* �����ڲ����ñ�ʶ */
        enum
        {
            kNoFlags = 0,
            kDontObeyReadOnly = 1,
            kDontCallCompletionRoutine = 2
        };
        
        // This version of GetValue copies the element into a buffer provided by the caller
        // Returns:     QTSS_BadArgument, QTSS_NotPreemptiveSafe (if attribute is not preemptive safe),
        //              QTSS_BadIndex (if inIndex is bad)
        QTSS_Error GetValue(QTSS_AttributeID inAttrID, UInt32 inIndex, void* ioValueBuffer, UInt32* ioValueLen);


        //This version of GetValue returns a pointer to the internal buffer for the attribute.
        //Only usable if the attribute is preemptive safe.
        //
        // Returns:     Same as above, but also QTSS_NotEnoughSpace, if value is too big for buffer.
        QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex, void** outValueBuffer, UInt32* outValueLen)
                        { return GetValuePtr(inAttrID, inIndex, outValueBuffer, outValueLen, false); } /* this version see below */
        
        // This version of GetValue converts the value to a string before returning it. Memory for
        // the string is allocated internally.
        //
        // Returns: QTSS_BadArgument, QTSS_BadIndex, QTSS_ValueNotFound
        QTSS_Error GetValueAsString(QTSS_AttributeID inAttrID, UInt32 inIndex, char** outString);
        
        // Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
        //              QTSS_BadIndex (attempt to set indexed parameter with param retrieval)
        QTSS_Error SetValue(QTSS_AttributeID inAttrID, UInt32 inIndex,
                            const void* inBuffer,  UInt32 inLen, UInt32 inFlags = kNoFlags);
        
        // Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
        QTSS_Error SetValuePtr(QTSS_AttributeID inAttrID,
                            const void* inBuffer,  UInt32 inLen, UInt32 inFlags = kNoFlags);
        
        // Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
        QTSS_Error CreateObjectValue(QTSS_AttributeID inAttrID, UInt32* outIndex,
                                        QTSSDictionary** newObject, QTSSDictionaryMap* inMap = NULL, 
                                        UInt32 inFlags = kNoFlags);
        
        // Returns:     QTSS_BadArgument, QTSS_ReadOnly, QTSS_BadIndex
        QTSS_Error RemoveValue(QTSS_AttributeID inAttrID, UInt32 inIndex, UInt32 inFlags = kNoFlags);
        
        // Utility routine used by the two external flavors of GetValue
        QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, UInt32 inIndex,
                                            void** outValueBuffer, UInt32* outValueLen,
                                            Bool16 isInternal);/* is internal routine? default false,������ */

        //
        // ACCESSORS AND MODIFIERS
        
        QTSSDictionaryMap*  GetDictionaryMap() { return fMap; }
        
        // Returns the Instance dictionary map for this dictionary. This may return NULL
        // if there are no instance attributes in this dictionary
        QTSSDictionaryMap*  GetInstanceDictMap() { return fInstanceMap; }


        
        // Returns the number of values associated with a given attribute
        UInt32              GetNumValues(QTSS_AttributeID inAttrID);
		// Modify the number of values associated with a given attribute with this specific value
        void                SetNumValues(QTSS_AttributeID inAttrID, UInt32 inNumValues);
        
        // Meant only for internal server use. Does no error checking,
        // doesn't invoke the param retrieval function.
		/* ֻ���ڲ�������ʹ��,�������ͬ��������ͬ! */
        StrPtrLen*  GetValue(QTSS_AttributeID inAttrID) 
                    {   return &fAttributes[inAttrID].fAttributeData;   }
                    
        OSMutex*    GetMutex() { return fMutexP; }
		
		/* �����Ƿ����? */
		void		SetLocked(Bool16 inLocked) { fLocked = inLocked; }
		/* ��ѯ��������? */
		Bool16		IsLocked() { return fLocked; }

        //
        // GETTING ATTRIBUTE INFO
        QTSS_Error GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoDict);
        QTSS_Error GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict);
        QTSS_Error GetAttrInfoByID(QTSS_AttributeID inAttrID, QTSSAttrInfoDict** outAttrInfoDict);
        

        //
        // INSTANCE ATTRIBUTES
        
        QTSS_Error  AddInstanceAttribute(   const char* inAttrName,
                                            QTSS_AttrFunctionPtr inFuncPtr,
                                            QTSS_AttrDataType inDataType,
                                            QTSS_AttrPermission inPermission );
                                            
        QTSS_Error  RemoveInstanceAttribute(QTSS_AttributeID inAttr);

        //
        // MODIFIERS

        // These functions are meant to be used by the server when it is setting up the
        // dictionary attributes. They do no error checking.

        // They don't set fNumAttributes & fAllocatedInternally.
		/* ����δ��ݽ���,�������ݳ�ԱfAttributes�ڵ�ָ�������Ľṹ��DictValueElement������3����Ա��ֵ,������fNumAttributes & fAllocatedInternally */
        void    SetVal(QTSS_AttributeID inAttrID, void* inValueBuffer, UInt32 inBufferLen);
        void    SetVal(QTSS_AttributeID inAttrID, StrPtrLen* inNewValue)
                    { this->SetVal(inAttrID, inNewValue->Ptr, inNewValue->Len); }

        // Call this if you want to assign empty storage to an attribute
        void    SetEmptyVal(QTSS_AttributeID inAttrID, void* inBuf, UInt32 inBufLen);
        
#if __DICTIONARY_TESTING__
        static void Test(); // API test for these objects
#endif

    protected:
    
        // Derived classes can provide a completion routine for some dictionary functions
        virtual void    RemoveValueComplete(UInt32 /*inAttrIndex*/, QTSSDictionaryMap* /*inMap*/, UInt32 /*inValueIndex*/) {}
        
        virtual void    SetValueComplete(UInt32 /*inAttrIndex*/, QTSSDictionaryMap* /*inMap*/,
                                    UInt32 /*inValueIndex*/,  void* /*inNewValue*/, UInt32 /*inNewValueLen*/) {}
        virtual void    RemoveInstanceAttrComplete(UInt32 /*inAttrindex*/, QTSSDictionaryMap* /*inMap*/) {}

        
		/* ���ù��캯��QTSSDictionary()�����µ�Dictionary! */
        virtual QTSSDictionary* CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex); //see QTSSDictionary.cpp line 75

    private:
    
        struct DictValueElement
        {
            // This stores all necessary information for each attribute value.
            
            DictValueElement() :    fAllocatedLen(0), fNumAttributes(0),
                                    fAllocatedInternally(false), fIsDynamicDictionary(false) {}
                                    
            // Does not delete! You Must call DeleteAttributeData for that
            ~DictValueElement() {}
            
            StrPtrLen   fAttributeData; // The data
            UInt32      fAllocatedLen;  // How much space do we have allocated?
            UInt32      fNumAttributes; // If this is an iterated attribute, how many? /* ��ֵ���Ը��� */
            Bool16      fAllocatedInternally; //����ϵͳԭ��Ҫ�ڲ�����Ļ���,��ô����Ӧ��ɾȥ����? Should we delete this memory?
            Bool16      fIsDynamicDictionary; //is this a internally allocated dictionary object?����������qtssAttrDataTypeQTSS_Object��
        };
        
		/* static attribute or an instance attribute array? */
        DictValueElement*   fAttributes;
        DictValueElement*   fInstanceAttrs;
		/* ʵ�����������С,��Ȼ��̬���������С��15,�������ñ��� */
        UInt32              fInstanceArraySize;
		/* ����,���ȿ���QTSSDictionaryMap,������������ʹ��QTSSDictionaryMap�Ĺ��ߺ��� */
        QTSSDictionaryMap*  fMap;
        QTSSDictionaryMap*  fInstanceMap;
        OSMutex*            fMutexP;
		Bool16				fMyMutex; /* ��mutex��? */
		Bool16				fLocked; /* ��������? */
        
		/* ����~QTSSDictionary()��ʹ��,�Ը���������inDictValues,��ɾ��ָ����Ŀ����������(�������ڲ�������ڴ�Ļ�) */ 
        void DeleteAttributeData(DictValueElement* inDictValues, UInt32 inNumValues);
};

/* ����ÿ�����Ե���,����Ҫ�ľ���(��ʵ����ɿ���)QTSSAttrInfoDict::sAttributes[] */
class QTSSAttrInfoDict : public QTSSDictionary
{
    public:
    
        struct AttrInfo
        {
            // This is all the relevant information for each dictionary
            // attribute.
            char                    fAttrName[QTSS_MAX_ATTRIBUTE_NAME_SIZE + 1];//64+1
            QTSS_AttrFunctionPtr    fFuncPtr;/* ���Ժ���ָ��,����������ͷ */
            QTSS_AttrDataType       fAttrDataType;
            QTSS_AttrPermission     fAttrPermission;
        };

        QTSSAttrInfoDict();
        virtual ~QTSSAttrInfoDict();
        
    private:
        
		/* ע����QTSS.h��QTSS_AttrInfoObjectAttributes���ݽ��бȽ�!���ｫ QTSS_AttributeID���������. */
        AttrInfo fAttrInfo;
        QTSS_AttributeID fID; //SInt32,��0,1,2,.....��ʼ

        //refer to QTSSDictionary.cpp line 829
		//refer to QTSSModule.h line 159
		//
		/* �����QTSSDictionary.cpp */
        static AttrInfo sAttributes[]; 
        
        friend class QTSSDictionaryMap;

};

class QTSSDictionaryMap
{
    public:
    
        //
        // This must be called before using any QTSSDictionary or QTSSDictionaryMap functionality
        static void Initialize(); // refer to QTSSDictionary.cpp line 855
        
        // Stores all meta-information for attributes
        
        // CONSTRUCTOR FLAGS
		/* �������ݳ�ԱfFlag��ȡֵ,�μ�QTSSDictionaryMap::Initialize() */
        enum
        {
            kNoFlags                  = 0,
            kAllowRemoval             = 1,
            kIsInstanceMap            = 2,
            kInstanceAttrsAllowed     = 4,
            kCompleteFunctionsAllowed = 8
        };
        
        //
        // CONSTRUCTOR / DESTRUCTOR
        
		/* ��һ��������ʾ��QTSS_ObjectType(����Ӧһ�� QTSSDictionaryMapʵ��)�����еĲ�������(�μ�QTSS.h),�ڶ���������ȡֵ������ */
        QTSSDictionaryMap(UInt32 inNumReservedAttrs, UInt32 inFlags = kNoFlags); //refer to QTSSDictionary.cpp line 885
        ~QTSSDictionaryMap(){ delete fAttrArray; }

        //
        // QTSS API ATTRIBUTE CALLS
                
        // All functions either return QTSS_BadArgument or QTSS_NoErr
        QTSS_Error      AddAttribute(   const char* inAttrName,
                                        QTSS_AttrFunctionPtr inFuncPtr,
                                        QTSS_AttrDataType inDataType,
                                        QTSS_AttrPermission inPermission );
                                        
        //
        // Marks this attribute as removed

		/* ����ָ�����Ե�Ȩ��,������ǿ���ȥ��(���bitλ1),ʹfNumValidAttrs��һ */
        QTSS_Error  RemoveAttribute(QTSS_AttributeID inAttrID);
		/* ����ȥ�����Իָ�(���bitλ��0),fNumValidAttrs++ */
        QTSS_Error  UnRemoveAttribute(QTSS_AttributeID inAttrID);

		/* ���ָ���������Ƿ���kAllowRemoval?QTSS_AttrPermission�Ƿ�qtssAttrModeDelete? */
        QTSS_Error  CheckRemovePermission(QTSS_AttributeID inAttrID);

        //
        // Searching / Iteration. These never return removed attributes
        QTSS_Error  GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict, Bool16 returnRemovedAttr = false);
        QTSS_Error  GetAttrInfoByID(QTSS_AttributeID inID, QTSSAttrInfoDict** outAttrInfoDict);
        QTSS_Error  GetAttrInfoByIndex(UInt32 inIndex, QTSSAttrInfoDict** outAttrInfoDict);
        QTSS_Error  GetAttrID(const char* inAttrName, QTSS_AttributeID* outID);
        
        //
        // PRIVATE ATTR PERMISSIONS
        enum
        {
            qtssPrivateAttrModeRemoved = 0x80000000 /* ȥ��˽������ģʽ,��ʾinstance/removed attr */
        };

		/* ����QTSS_AttributeID��ת������ */


        //
        // CONVERTING attribute IDs to array indexes. Returns -1 if inAttrID doesn't exist, 
		// code definition see below outside of class
		/* ȥ�����ܵ�ʵ�����Եı�־λ(���bitλ��0),ʹ���Ϊһ������� */
		/* ע����������ǳ���Ҫ!ʹ�ü���Ƶ��! */
        inline SInt32                   ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID);

		/* ��ʵ�����Ե�����ID��?(�����bitλ�Ƿ���1?)ע���뾲̬��������! */
        static Bool16           IsInstanceAttrID(QTSS_AttributeID inAttrID)
            { return (inAttrID & 0x80000000) != 0; }

        // ACCESSORS
        
        // These functions do no error checking. Be careful.
		/* ע����Щ���Բ����д�����,���Ƕ����Եķ���,Ҫ�õ�QTSSAttrInfoDict�� */
        
        // Includes removed attributes
        UInt32          GetNumAttrs()           { return fNextAvailableID; }
        UInt32          GetNumNonRemovedAttrs() { return fNumValidAttrs; }
        
		/* used in QTSSDictionary::GetValuePtr() */
        Bool16                  IsPreemptiveSafe(UInt32 inIndex) 
            { Assert(inIndex < fNextAvailableID); return (Bool16) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModePreempSafe); }

		/* used in QTSSDictionary::CreateObjectValue() */
        Bool16                  IsWriteable(UInt32 inIndex) 
            { Assert(inIndex < fNextAvailableID); return (Bool16) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeWrite); }
		
		/* used in QTSSDictionary::GetValuePtr() */
		Bool16                  IsCacheable(UInt32 inIndex) 
            { Assert(inIndex < fNextAvailableID); return (Bool16) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeCacheable); }

		/* �����bitλ�Ƿ�Ϊ1?used in QTSSDictionary::GetValuePtr() */
        Bool16                  IsRemoved(UInt32 inIndex) 
            { Assert(inIndex < fNextAvailableID); return (Bool16) (fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved) ; }

		/* used in QTSSDictionary::GetValuePtr() */
        QTSS_AttrFunctionPtr    GetAttrFunction(UInt32 inIndex)
            { Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fFuncPtr; }
            
        char*                   GetAttrName(UInt32 inIndex)
            { Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fAttrName; }
            
        QTSS_AttributeID        GetAttrID(UInt32 inIndex)
            { Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fID; }

		/* used in QTSSDictionary::GetValuePtr()/ValueToString()/CreateObjectValue() */
        QTSS_AttrDataType       GetAttrType(UInt32 inIndex)
            { Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fAttrDataType; }
        
        Bool16                  InstanceAttrsAllowed() { return (Bool16) (fFlags & kInstanceAttrsAllowed); }
        Bool16                  CompleteFunctionsAllowed() { return (Bool16) (fFlags & kCompleteFunctionsAllowed) ; }

        // MODIFIERS
        
        // Sets this attribute ID to have this information
        /* ��ָ������ID��Attributeӵ��ָ������ */

		/* ��AttributeID���ArrayIndex,������һ���µ�QTSSAttrInfoDict*���͵��������,��������������ĸ���ֵ */
        void        SetAttribute(   QTSS_AttributeID inID, 
                                    const char* inAttrName,
                                    QTSS_AttrFunctionPtr inFuncPtr,
                                    QTSS_AttrDataType inDataType,
                                    QTSS_AttrPermission inPermission );

        
        //
        // DICTIONARY MAPS
        
        // All dictionary maps are stored here, and are accessable
        // through these routines
        
        // This enum allows all QTSSDictionaryMaps(refer to object types) to be stored in an array sDictionaryMaps[]. 
		// Note suffix used by "DictIndex"
        enum
        {
			/* reserved attributes,��QTSS.h��QTSS_ObjectType�Ķ���һһ��Ӧ,�μ�QTSSDictionaryMap::GetMapIndex() */
            kServerDictIndex                = 0,/* used in QTSServerInterface::Initialize() */
            kPrefsDictIndex                 = 1,/* used in QTSServerPrefs::Initialize() */
            kTextMessagesDictIndex          = 2,/* used in QTSSMessages::Initialize() */
			kServiceDictIndex               = 3,
            
            kRTPStreamDictIndex             = 4,/* used in RTPStream::Initialize() */
            kClientSessionDictIndex         = 5,/* used in RTPSessionInterface::Initialize() */
            kRTSPSessionDictIndex           = 6,/* used in RTSPSessionInterface::Initialize() */
            kRTSPRequestDictIndex           = 7,/* used in RTSPRequestInterface::Initialize() */
            kRTSPHeaderDictIndex            = 8,/* used in RTSPRequestInterface::Initialize() */
            kFileDictIndex                  = 9,/* used in QTSSFile::Initialize() */
            kModuleDictIndex                = 10,/* used in QTSSModule::Initialize() */
            kModulePrefsDictIndex           = 11,/* used in QTSServer::AddModule() */
            kAttrInfoDictIndex              = 12,/* ���QTSSDictionaryMapҪ�������� */
            kQTSSUserProfileDictIndex       = 13,/* used in QTSSUserProfile::Initialize() */
            kQTSSConnectedUserDictIndex     = 14,/* used in QTSServerInterface::Initialize() */

            kNumDictionaries                = 15,// total Number of all Dictionaries

			/* extend dynamically attributes */
            
            kNumDynamicDictionaryTypes      = 500,
            kIllegalDictionary              = kNumDynamicDictionaryTypes + kNumDictionaries //515
        };
        
        // This function converts a QTSS_ObjectType to an index of sDictionaryMaps
		/* ����reserved dictionary,��������ΪUInt32��QTSS_ObjectType,ת��Ϊ��Ӧ��DictionaryMap Index������,����һ�ŷ���kIllegalDictionary */
        static UInt32                   GetMapIndex(QTSS_ObjectType inType);

        // Using one of the above predefined indexes, this returns the corresponding Dictionary map
        static QTSSDictionaryMap*       GetMap(UInt32 inIndex)
            { Assert(inIndex < kNumDynamicDictionaryTypes + kNumDictionaries); return sDictionaryMaps[inIndex]; }

		/* ��������̬��Dictionary Map,����ʼ��.ע��sNextDynamicMap�ǳ���Ҫ. */
        static QTSS_ObjectType          CreateNewMap();

    private:

        //
        // Repository(��) for dictionary maps, code definition refer to QTSSDictionary.cpp line 851
        
		/* ����DictionaryMap����,�Ѿ����������,�μ� QTSSDictionaryMap::Initialize() */
        static QTSSDictionaryMap*       sDictionaryMaps[kNumDictionaries + kNumDynamicDictionaryTypes];//515 size
        static UInt32                   sNextDynamicMap;//15
            
        enum
        {
            kMinArraySize = 20
        };

        UInt32                          fNextAvailableID;
        UInt32                          fNumValidAttrs;
        UInt32                          fAttrArraySize;
        QTSSAttrInfoDict**              fAttrArray;
        UInt32                          fFlags;
        
        friend class QTSSDictionary;
};

/* used in QTSSPrefs::RereadObjectPreferences() */
/* ������IDת��Ϊ�������������,��Ǹ������ѱ�ʹ��,ע����������ǳ���Ҫ!ʹ�ü���Ƶ��! */
inline SInt32   QTSSDictionaryMap::ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID)
{
	/* ע������㷨:ȷ�����bitλΪ0 */
    SInt32 theIndex = inAttrID & 0x7FFFFFFF;
    if ((theIndex < 0) || (theIndex >= (SInt32)fNextAvailableID))
        return -1;
    else
        return theIndex;
}


#endif
