
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
#include "QTSSStream.h"  /* 注意是base class */

class QTSSDictionary;
class QTSSDictionaryMap;
class QTSSAttrInfoDict;

#define __DICTIONARY_TESTING__ 0

//
// Function prototype for attr functions
/* 函数指针的定义:获取第二个参数对应属性的具体值,参见QTSSDictionary::GetValuePtr() */
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
		/* 例程内部调用标识 */
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
                                            Bool16 isInternal);/* is internal routine? default false,见上面 */

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
		/* 只限内部服务器使用,与上面的同名函数不同! */
        StrPtrLen*  GetValue(QTSS_AttributeID inAttrID) 
                    {   return &fAttributes[inAttrID].fAttributeData;   }
                    
        OSMutex*    GetMutex() { return fMutexP; }
		
		/* 设置是否加锁? */
		void		SetLocked(Bool16 inLocked) { fLocked = inLocked; }
		/* 查询加锁了吗? */
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
		/* 将入参传递进来,设置数据成员fAttributes内的指定分量的结构体DictValueElement内其它3个成员的值,不设置fNumAttributes & fAllocatedInternally */
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

        
		/* 引用构造函数QTSSDictionary()创建新的Dictionary! */
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
            UInt32      fNumAttributes; // If this is an iterated attribute, how many? /* 多值属性个数 */
            Bool16      fAllocatedInternally; //它是系统原来要内部分配的缓存,那么我们应该删去它吗? Should we delete this memory?
            Bool16      fIsDynamicDictionary; //is this a internally allocated dictionary object?数据类型是qtssAttrDataTypeQTSS_Object的
        };
        
		/* static attribute or an instance attribute array? */
        DictValueElement*   fAttributes;
        DictValueElement*   fInstanceAttrs;
		/* 实例属性数组大小,当然静态属性数组大小是15,无须设置变量 */
        UInt32              fInstanceArraySize;
		/* 看来,得先看类QTSSDictionaryMap,这两个量用来使用QTSSDictionaryMap的工具函数 */
        QTSSDictionaryMap*  fMap;
        QTSSDictionaryMap*  fInstanceMap;
        OSMutex*            fMutexP;
		Bool16				fMyMutex; /* 有mutex吗? */
		Bool16				fLocked; /* 上锁了吗? */
        
		/* 它在~QTSSDictionary()中使用,对给定的数组inDictValues,来删除指定数目的属性数据(假如是内部分配的内存的话) */ 
        void DeleteAttributeData(DictValueElement* inDictValues, UInt32 inNumValues);
};

/* 描述每个属性的类,最重要的就是(其实该类可看成)QTSSAttrInfoDict::sAttributes[] */
class QTSSAttrInfoDict : public QTSSDictionary
{
    public:
    
        struct AttrInfo
        {
            // This is all the relevant information for each dictionary
            // attribute.
            char                    fAttrName[QTSS_MAX_ATTRIBUTE_NAME_SIZE + 1];//64+1
            QTSS_AttrFunctionPtr    fFuncPtr;/* 属性函数指针,定义见程序最开头 */
            QTSS_AttrDataType       fAttrDataType;
            QTSS_AttrPermission     fAttrPermission;
        };

        QTSSAttrInfoDict();
        virtual ~QTSSAttrInfoDict();
        
    private:
        
		/* 注意与QTSS.h中QTSS_AttrInfoObjectAttributes内容进行比较!这里将 QTSS_AttributeID分离出来了. */
        AttrInfo fAttrInfo;
        QTSS_AttributeID fID; //SInt32,从0,1,2,.....开始

        //refer to QTSSDictionary.cpp line 829
		//refer to QTSSModule.h line 159
		//
		/* 定义见QTSSDictionary.cpp */
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
		/* 这是数据成员fFlag的取值,参见QTSSDictionaryMap::Initialize() */
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
        
		/* 第一个参数表示该QTSS_ObjectType(它对应一个 QTSSDictionaryMap实例)所具有的参数总数(参见QTSS.h),第二个参数的取值见上面 */
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

		/* 设置指定属性的权限,标记其是可移去的(最高bit位1),使fNumValidAttrs减一 */
        QTSS_Error  RemoveAttribute(QTSS_AttributeID inAttrID);
		/* 将移去的属性恢复(最高bit位置0),fNumValidAttrs++ */
        QTSS_Error  UnRemoveAttribute(QTSS_AttributeID inAttrID);

		/* 检查指定的属性是否是kAllowRemoval?QTSS_AttrPermission是否qtssAttrModeDelete? */
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
            qtssPrivateAttrModeRemoved = 0x80000000 /* 去掉私有属性模式,暗示instance/removed attr */
        };

		/* 两种QTSS_AttributeID的转换方法 */


        //
        // CONVERTING attribute IDs to array indexes. Returns -1 if inAttrID doesn't exist, 
		// code definition see below outside of class
		/* 去掉可能的实例属性的标志位(最高bit位置0),使其成为一般的属性 */
		/* 注意这个函数非常重要!使用极其频繁! */
        inline SInt32                   ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID);

		/* 是实例属性的属性ID吗?(看最高bit位是否是1?)注意与静态属性区别! */
        static Bool16           IsInstanceAttrID(QTSS_AttributeID inAttrID)
            { return (inAttrID & 0x80000000) != 0; }

        // ACCESSORS
        
        // These functions do no error checking. Be careful.
		/* 注意这些属性不进行错误检查,都是对属性的访问,要用到QTSSAttrInfoDict类 */
        
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

		/* 看最高bit位是否为1?used in QTSSDictionary::GetValuePtr() */
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
        /* 让指定属性ID的Attribute拥有指定属性 */

		/* 由AttributeID获得ArrayIndex,并创建一个新的QTSSAttrInfoDict*类型的数组分量,由入参来设置它的各项值 */
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
			/* reserved attributes,与QTSS.h中QTSS_ObjectType的定义一一对应,参见QTSSDictionaryMap::GetMapIndex() */
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
            kAttrInfoDictIndex              = 12,/* 这个QTSSDictionaryMap要最先生成 */
            kQTSSUserProfileDictIndex       = 13,/* used in QTSSUserProfile::Initialize() */
            kQTSSConnectedUserDictIndex     = 14,/* used in QTSServerInterface::Initialize() */

            kNumDictionaries                = 15,// total Number of all Dictionaries

			/* extend dynamically attributes */
            
            kNumDynamicDictionaryTypes      = 500,
            kIllegalDictionary              = kNumDynamicDictionaryTypes + kNumDictionaries //515
        };
        
        // This function converts a QTSS_ObjectType to an index of sDictionaryMaps
		/* 仅对reserved dictionary,将其类型为UInt32的QTSS_ObjectType,转换为相应的DictionaryMap Index并返回,否则一概返回kIllegalDictionary */
        static UInt32                   GetMapIndex(QTSS_ObjectType inType);

        // Using one of the above predefined indexes, this returns the corresponding Dictionary map
        static QTSSDictionaryMap*       GetMap(UInt32 inIndex)
            { Assert(inIndex < kNumDynamicDictionaryTypes + kNumDictionaries); return sDictionaryMaps[inIndex]; }

		/* 仅创建动态的Dictionary Map,并初始化.注意sNextDynamicMap非常重要. */
        static QTSS_ObjectType          CreateNewMap();

    private:

        //
        // Repository(库) for dictionary maps, code definition refer to QTSSDictionary.cpp line 851
        
		/* 创建DictionaryMap数组,已经被定义好了,参见 QTSSDictionaryMap::Initialize() */
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
/* 从属性ID转化为属性数组的索引,标记该属性已被使用,注意这个函数非常重要!使用极其频繁! */
inline SInt32   QTSSDictionaryMap::ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID)
{
	/* 注意这个算法:确保最高bit位为0 */
    SInt32 theIndex = inAttrID & 0x7FFFFFFF;
    if ((theIndex < 0) || (theIndex >= (SInt32)fNextAvailableID))
        return -1;
    else
        return theIndex;
}


#endif
