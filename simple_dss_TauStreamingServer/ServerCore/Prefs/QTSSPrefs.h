
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTSSPrefs.h
Description: A object that stores for module preferences.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTSSMODULEPREFS_H__
#define __QTSSMODULEPREFS_H__

#include "QTSS.h"
#include "QTSSDictionary.h"
#include "QTSSModuleUtils.h"
#include "StrPtrLen.h"
#include "XMLPrefsParser.h"  //used frequently here!



class QTSSPrefs : public QTSSDictionary
{
    public:

        QTSSPrefs(  XMLPrefsParser* inPrefsSource,
                    StrPtrLen* inModuleName,
                    QTSSDictionaryMap* inMap,
                    Bool16 areInstanceAttrsAllowed,       //允许实例属性吗?
                    QTSSPrefs* parentDictionary = NULL );
        virtual ~QTSSPrefs() { if (fPrefName != NULL) delete [] fPrefName; }
        
        //This is callable at any time, and is thread safe wrt to the accessors
        void        RereadPreferences();
        void        RereadObjectPreferences(ContainerRef container);
        
        
        // ACCESSORS
        OSMutex*       GetMutex() { return &fPrefsMutex; }  
        ContainerRef   GetContainerRefForObject(QTSSPrefs* object);
        ContainerRef   GetContainerRef();
        
    protected:

        XMLPrefsParser* fPrefsSource;      //预设值文件(xml文件)及其解析类
        OSMutex         fPrefsMutex;       //预设值文件互斥锁
        char*           fPrefName;         //模块预设值文件名
        QTSSPrefs*      fParentDictionary; //预设值链表(指向上一级预设值目录)
    
        // SET PREF VALUES FROM FILE
        //
        // Places all the values at inPrefIndex of the prefs file into the attribute
        // with the specified ID. This attribute must already exist.
        // Specify inNumValues if you wish to restrict the number of values retrieved
        // from the text file to a certain number, otherwise specify 0.
        void SetPrefValuesFromFile(ContainerRef container, UInt32 inPrefIndex, QTSS_AttributeID inAttrID, UInt32 inNumValues = 0);
        void SetPrefValuesFromFileWithRef(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues = 0);
        void SetObjectValuesFromFile(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues, char* prefName);

        
        // SET PREF VALUE
        //
        // Places the specified value into the attribute with inAttrID, at inAttrIndex
        // index. This function does the conversion, and uses the converted size of the
        // value when setting the value. If you wish to override this size, specify inValueSize,
        // otherwise it can be 0.
        void SetPrefValue(QTSS_AttributeID inAttrID, UInt32 inAttrIndex,
                         char* inPrefValue, QTSS_AttrDataType inPrefType, UInt32 inValueSize = 0);

        
    protected:
    
        // Completion routines for SetValue and RemoveValue write back to the config source
        virtual void    RemoveValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,UInt32 inValueIndex);                                                                    
        virtual void    SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen);                        
        virtual void    RemoveInstanceAttrComplete(UInt32 inAttrindex, QTSSDictionaryMap* inMap);
        
        virtual QTSSDictionary* CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex);

    private:
    
        QTSS_AttributeID AddPrefAttribute(const char* inAttrName, QTSS_AttrDataType inDataType);
                        
};
#endif //__QTSSMODULEPREFS_H__
