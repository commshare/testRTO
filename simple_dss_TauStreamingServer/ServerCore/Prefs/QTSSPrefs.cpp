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



#include "QTSSPrefs.h"
#include "QTSSDataConverter.h"
#include "MyAssert.h"
#include "OSMemory.h"
#include "OSArrayObjectDeleter.h"



 
// used in QTSServer::AddModule()
QTSSPrefs::QTSSPrefs(XMLPrefsParser* inPrefsSource, StrPtrLen* inModuleName, QTSSDictionaryMap* inMap,
                     Bool16 areInstanceAttrsAllowed, QTSSPrefs* parentDictionary )
:   QTSSDictionary(inMap, &fPrefsMutex),
    fPrefsSource(inPrefsSource),
    fPrefName(NULL),
    fParentDictionary(parentDictionary)
{   //用模块名设置数据成员fPrefName
    if (inModuleName != NULL)
        fPrefName = inModuleName->GetAsCString();
}


/* 生成预设值对象,并实例化,再返回 */
QTSSDictionary* QTSSPrefs::CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* /* inMutex */)
{
    return NEW QTSSPrefs(fPrefsSource, NULL, inMap, true, this );
}

/* 这是本类最重要的一个函数,综合性极强!读取每个Module的预设值 */
void QTSSPrefs::RereadPreferences()
{
    RereadObjectPreferences(GetContainerRef());
}
 
/* 注意理解清楚InstanceDictMap ? used in QTSServer::RereadPrefsService() */
void QTSSPrefs::RereadObjectPreferences(ContainerRef container)
{
    QTSS_Error theErr = QTSS_NoErr;
    
    // Keep track of which pref attributes should remain. All others will be removed. 
    // This routine uses names because it adds and deletes attributes. This means attribute indexes,positions and counts are constantly changing.
    UInt32 initialNumAttrs = 0;
	/* 先获得实例属性Dict映射,再得到初始属性的个数 */
    if (this->GetInstanceDictMap() != NULL)
    {   
    	initialNumAttrs = this->GetInstanceDictMap()->GetNumAttrs();
    }

	/********************* 注意该数组非常重要!! ************************/
	/* 创建Server中module pref的字符数组,其大小就是上面的初始属性的个数 */
	char** modulePrefInServer;
	if (initialNumAttrs > 0)
	{
		modulePrefInServer = NEW char*[initialNumAttrs];
		::memset(modulePrefInServer, 0, sizeof(char*) * initialNumAttrs);
	}
	else
	{
		modulePrefInServer = NULL;
	}
	/********************* 注意该数组非常重要!! ************************/
  	
    OSMutexLocker locker(&fPrefsMutex);
	/* 返回指定Tag下的子Tag个数 */
    UInt32 theNumPrefs = fPrefsSource->GetNumPrefsByContainer(container);

	/* 遍历初始化属性个数,得到指定index的QTSSAttrInfoDict指针,得到Server中module pref的字符串,来设置上面的字符数组 */
    for (UInt32 i = 0; i < initialNumAttrs;i++) // pull out all the names in the server 
    {   
        QTSSAttrInfoDict* theAttrInfoPtr = NULL;
        theErr = this->GetInstanceDictMap()->GetAttrInfoByIndex(i, &theAttrInfoPtr);
        if (theErr != QTSS_NoErr)
            continue; //假如出错,就跳过
        
        UInt32 nameLen = 0;
        theErr = theAttrInfoPtr->GetValuePtr(qtssAttrName,0, (void **) &modulePrefInServer[i], &nameLen);
        Assert(theErr == QTSS_NoErr);
        qtss_printf("QTSSPrefs::RereadPreferences modulePrefInServer in server=%s\n",modulePrefInServer[i]);
    }
    
    // Use the names of the attributes in the attribute map as the key values for
    // finding preferences in the config file.
    //遍历所有的预设值属性,依据xml文件,检查其合法性,删去不合法的,换上新的属性,来维护modulePrefInServer数组
    for (UInt32 x = 0; x < theNumPrefs; x++)
    {
        char* thePrefTypeStr = NULL;
        char* thePrefName = NULL;
		/* 获取container下第x个子Tag的第0个子Tag的值和数据类型字符串*/
        (void)fPrefsSource->GetPrefValueByIndex(container, x, 0, &thePrefName, &thePrefTypeStr);

        // What type is this data type?
		/* 将类型字符串转换成类型 */
        QTSS_AttrDataType thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);

        // 检查实例映射中是否存在指定名称的属性,若存在,无须添加该属性
        // Check to see if there is an attribute with this name already in the
        // instance map. If one matches, then we don't need to add this attribute.
        QTSSAttrInfoDict* theAttrInfo = NULL;
        if (this->GetInstanceDictMap() != NULL)
			/* 获取 QTSSAttrInfoDict指针 */
            (void)this->GetInstanceDictMap()->GetAttrInfoByName(thePrefName, &theAttrInfo, false ); // false=don't return info on deleted attributes
                                                                                                                        
        UInt32 theLen = sizeof(QTSS_AttrDataType);
        QTSS_AttributeID theAttrID = qtssIllegalAttrID;
        // 遍历初始化属性,see if this name is in the server,假如存在,无须添加
        for (UInt32 i = 0; i < initialNumAttrs;i++) 
        {   
			if (modulePrefInServer[i] != NULL && thePrefName != NULL && 0 == ::strcmp(modulePrefInServer[i],thePrefName))
            {  
				modulePrefInServer[i] = NULL; // in the server so don't delete later
                qtss_printf("QTSSPrefs::RereadPreferences modulePrefInServer in file and in server=%s\n",thePrefName);
            }
        }

		//假如实例映射中不存在指定名称的属性,就添加进该实例属性,用返回的属性ID从xml文件中获取预设值来设置该属性值
        if ( theAttrInfo == NULL )
        {
            theAttrID = this->AddPrefAttribute(thePrefName, thePrefType); // not present or deleted
            this->SetPrefValuesFromFile(container, x, theAttrID, 0); // will add another or replace a deleted attribute
        }
        //假如实例映射中存在指定名称的属性,就获取该属性的数据类型和ID,判断与预设值类型是否相同?不同就用新的属性代替,并设置属性值
        else
        {
            QTSS_AttrDataType theAttrType = qtssAttrDataTypeUnknown;
            theErr = theAttrInfo->GetValue(qtssAttrDataType, 0, &theAttrType, &theLen);
            Assert(theErr == QTSS_NoErr);
            
            theLen = sizeof(theAttrID);
            theErr = theAttrInfo->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
            Assert(theErr == QTSS_NoErr);

			//假如数据类型和预设值类型不同, 就移去该实例属性,再添加进新的实例属性
            if (theAttrType != thePrefType)
            {
                // This is not the same pref as before, because the data types
                // are different. Remove the old one from the map, add the new one.
                (void)this->RemoveInstanceAttribute(theAttrID);
                theAttrID = this->AddPrefAttribute(thePrefName, thePrefType);
            }
            else
            {
                // This pref already exists
            }
            
            // Set the values,用指定属性ID从xml文件中获取预设值来设置该属性值
            this->SetPrefValuesFromFile(container, x, theAttrID, 0);

            // Mark this pref as found.让最高比特为0,标记该Pref属性值已经找到
            SInt32 theIndex = this->GetInstanceDictMap()->ConvertAttrIDToArrayIndex(theAttrID);
            Assert(theIndex >= 0);
        }
    }
    
    // Remove all attributes that no longer apply
	// 从实例映射中,删去modulePrefInServer数组中分量非空的项,将该分量置空
    if (this->GetInstanceDictMap() != NULL && initialNumAttrs > 0)
    {   
        for (UInt32 a = 0; a < initialNumAttrs; a++) 
        {
            if (NULL != modulePrefInServer[a]) // found a pref in the server that wasn't in the file
            {   
				//得到QTSSAttrInfoDict指针
                QTSSAttrInfoDict* theAttrInfoPtr = NULL;
                theErr = this->GetInstanceDictMap()->GetAttrInfoByName(modulePrefInServer[a], &theAttrInfoPtr);
                Assert(theErr == QTSS_NoErr);
                if (theErr != QTSS_NoErr) continue; //假如出错,跳到下一个
        
				//得到属性ID
                QTSS_AttributeID theAttrID = qtssIllegalAttrID; 
                UInt32 theLen = sizeof(theAttrID);
                theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
                Assert(theErr == QTSS_NoErr);
                if (theErr != QTSS_NoErr) continue; //假如出错,跳到下一个
                            
                if (0)
                {   char* theName = NULL;
                    UInt32 nameLen = 0;
                    theAttrInfoPtr->GetValuePtr(qtssAttrName,0, (void **) &theName, &nameLen);
                    qtss_printf("QTSSPrefs::RereadPreferences about to delete modulePrefInServer=%s attr=%s id=%lu\n",modulePrefInServer[a], theName,theAttrID);
                }
            
                //删除该实例属性
                this->GetInstanceDictMap()->RemoveAttribute(theAttrID);
                modulePrefInServer[a] = NULL;
            }
        }
    }
    
    delete modulePrefInServer; //删去动态创建的该字符数组
}
        
void QTSSPrefs::SetPrefValuesFromFile(ContainerRef container, UInt32 inPrefIndex, QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
	/* 从xml配置值文件中,获取指定Tag的指定子(XML)Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByIndex(container, inPrefIndex);
    SetPrefValuesFromFileWithRef(pref, inAttrID, inNumValues);
}
 
/* 得到指定Tag的指定index的子Tag的NAME和TYPE属性值,返回该Tag的值;对字典中的属性ID,我们分两次将XMLTag中的子Tag的
相关值加入DictionaryMap中(第一次确定属性值最大长度,第二次逐个复制),最后设置属性个数 */
void QTSSPrefs::SetPrefValuesFromFileWithRef(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
    // We have an attribute ID for this pref, it is in the map and everything.
    // Now, let's add all the values that are in the pref file.
	/* 假如没有指定Tag，返回 */
    if (pref == 0)
        return;
    
	/* 得到指定Tag值的个数 */
    UInt32 numPrefValues = inNumValues;
    if (inNumValues == 0)
        numPrefValues = fPrefsSource->GetNumPrefValues(pref);
        
    char* thePrefName = NULL;
    char* thePrefValue = NULL;
    char* thePrefTypeStr = NULL;
    QTSS_AttrDataType thePrefType = qtssAttrDataTypeUnknown;
    
    // find the type.  If this is a QTSSObject, then we need to call a different routine
	/* 得到指定Tag的指定index(此处为0)的子Tag的NAME和TYPE属性值,返回该Tag的值 */
    thePrefValue = fPrefsSource->GetPrefValueByRef( pref, 0, &thePrefName, &thePrefTypeStr);
    thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);//将类型字符串转换为类型
	/* 假如类型是QTSS_Object,用特定方法转换,并返回 */
    if (thePrefType == qtssAttrDataTypeQTSS_Object)
    {
        SetObjectValuesFromFile(pref, inAttrID, numPrefValues, thePrefName);
        return;
    }

    UInt32 maxPrefValueSize = 0;
    QTSS_Error theErr = QTSS_NoErr;
    
    // We have to loop through all the values associated with this pref twice:
    // first, to figure out the length (in bytes) of the longest value, secondly
    // to actually copy these values into the dictionary.
	/* 做两次与该Tag相关的循环,第一次得到属性值的最大长度 */
    for (UInt32 y = 0; y < numPrefValues; y++)
    {
        UInt32 tempMaxPrefValueSize = 0;
        /* 得到指定Tag的指定index的子Tag的NAME和TYPE属性值,返回该Tag的值 */
        thePrefValue = fPrefsSource->GetPrefValueByRef( pref, y, &thePrefName, &thePrefTypeStr);

        theErr = QTSSDataConverter::StringToValue(  thePrefValue, thePrefType,NULL, &tempMaxPrefValueSize );                                             
        Assert(theErr == QTSS_NotEnoughSpace);
        
        if (tempMaxPrefValueSize > maxPrefValueSize)
            maxPrefValueSize = tempMaxPrefValueSize;
    }

    /* 第二次循环,从xml配置文件中获取配置值,并设置为预设值 */
    for (UInt32 z = 0; z < numPrefValues; z++)
    {
        thePrefValue = fPrefsSource->GetPrefValueByRef( pref, z, &thePrefName, &thePrefTypeStr);
        this->SetPrefValue(inAttrID, z, thePrefValue, thePrefType, maxPrefValueSize);
    }
    
    // Make sure the dictionary knows exactly how many values are associated with this pref
    // 设置该指定属性的预设值个数,让Pref字典知道
    this->SetNumValues(inAttrID, numPrefValues);
}
 
/* 设置指定属性ID的Object的属性,先读取指定index的所有子Tag值,再依次加入QTSSDictionary中,设置指定属性ID的属性个数 */
void QTSSPrefs::SetObjectValuesFromFile(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues, char* prefName)
{
	//对该指定属性ID的Object的属性,用配置文件逐个设置它
    for (UInt32 z = 0; z < inNumValues; z++)
    {
		/* 若为OBJECT,返回该Tag的值;若为OBJECT-LIST,返回指定index的子Tag值 */
        ContainerRef object = fPrefsSource->GetObjectValue( pref, z );
        QTSSPrefs* prefObject;
        UInt32 len = sizeof(QTSSPrefs*);
		/* 得到指定属性ID和索引的属性值,假如失败就创建一个新的ObjectValue */
        QTSS_Error err = this->GetValue(inAttrID, z, &prefObject, &len);
        if (err != QTSS_NoErr)
        {
            UInt32 tempIndex;
            err = CreateObjectValue(inAttrID, &tempIndex, (QTSSDictionary**)&prefObject, NULL, QTSSDictionary::kDontObeyReadOnly | QTSSDictionary::kDontCallCompletionRoutine);
            Assert(err == QTSS_NoErr);
            Assert(tempIndex == z);
            if (err != QTSS_NoErr)  // this shouldn't happen
                return;
            StrPtrLen temp(prefName);
            prefObject->fPrefName = temp.GetAsCString(); //用最后一个入参设置预设值对象的名称
        }
		//设置指定对象的预设值
        prefObject->RereadObjectPreferences(object);
    }
    
    // Make sure the dictionary knows exactly how many values are associated with this pref
    // 在相应字典中设置该指定属性ID的属性的总个数
    this->SetNumValues(inAttrID, inNumValues);
}

/* 在QTSSDictionary中设置指定属性ID和属性index的属性值(先进行类型转换) */
void    QTSSPrefs::SetPrefValue(QTSS_AttributeID inAttrID, UInt32 inAttrIndex,
                                char* inPrefValue, QTSS_AttrDataType inPrefType, UInt32 inValueSize)
                        
{
    static const UInt32 kMaxPrefValueSize = 1024;
    char convertedPrefValue[kMaxPrefValueSize];
    ::memset(convertedPrefValue, 0, kMaxPrefValueSize);
	/* 指定属性值必须小于属性长度最大值 */
    Assert(inValueSize < kMaxPrefValueSize);
    
	/* 将字符串转换为数值形式存储 */
    UInt32 convertedBufSize = kMaxPrefValueSize;
    QTSS_Error theErr = QTSSDataConverter::StringToValue(inPrefValue, inPrefType, convertedPrefValue, &convertedBufSize );     
    Assert(theErr == QTSS_NoErr);
    
    if (inValueSize == 0)
        inValueSize = convertedBufSize;
     
	//在相应字典中设置指定属性ID和索引的属性的值
    this->SetValue(inAttrID, inAttrIndex, convertedPrefValue, inValueSize, QTSSDictionary::kDontObeyReadOnly | QTSSDictionary::kDontCallCompletionRoutine);                         

}

/* 在QTSSDictionary添加指定属性名和数据类型的实例属性,获取并返回该属性名的属性ID */
QTSS_AttributeID QTSSPrefs::AddPrefAttribute(const char* inAttrName, QTSS_AttrDataType inDataType)
{
    QTSS_Error theErr = this->AddInstanceAttribute( inAttrName, NULL, inDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModeDelete);
    Assert(theErr == QTSS_NoErr);
    
    QTSS_AttributeID theID = qtssIllegalAttrID;
    theErr = this->GetInstanceDictMap()->GetAttrID( inAttrName, &theID);
    Assert(theErr == QTSS_NoErr);
    
    return theID;
}

/* 返回指定属性NAME值为inPrefName和index为0的子Tag, 对多值Tag,删除指定index的Tag值  */
void    QTSSPrefs::RemoveValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
                                        UInt32 inValueIndex)
{
	/* 得到Tag */
    ContainerRef objectRef = GetContainerRef();
	/* 返回第一个入参下,指定属性NAME值为inPrefName和index为0的子Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName( objectRef, inMap->GetAttrName(inAttrIndex));
    Assert(pref != NULL);
    if (pref != NULL)
		/* 对多值Tag,删除指定index的Tag值 */
        fPrefsSource->RemovePrefValue( pref, inValueIndex);
    
	/* 将指定内容写入xml文件,假如出错,记录错误日志 */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* 先找到指定属性NAME值为inPrefName和index为0的子Tag,删除它 */
void    QTSSPrefs::RemoveInstanceAttrComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap)
{
    ContainerRef objectRef = GetContainerRef();
	/* 返回第一个入参下,指定属性NAME值为inPrefName和index为0的子Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName( objectRef, inMap->GetAttrName(inAttrIndex));
    Assert(pref != NULL);
    if (pref != NULL)
    {
		/* 删除指定的XMLTag */
        fPrefsSource->RemovePref(pref);
    }
    
	/* 将指定内容写入xml文件,假如出错,记录错误日志 */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* 先生成指定属性值和数据类型的Tag,加入Tag内嵌队列中,再设置其Tag值  */
void QTSSPrefs::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
                                    UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
    ContainerRef objectRef = GetContainerRef();
	/* 生成指定属性值和数据类型的Tag,加入Tag内嵌队列中,并返回该Tag */
    ContainerRef pref = fPrefsSource->AddPref(objectRef, inMap->GetAttrName(inAttrIndex), QTSSDataConverter::TypeToTypeString(inMap->GetAttrType(inAttrIndex)));
    
	//假如该属性类型是QTSS_Object
	if (inMap->GetAttrType(inAttrIndex) == qtssAttrDataTypeQTSS_Object)
    {
        QTSSPrefs* object = *(QTSSPrefs**)inNewValue;   // value is a pointer to a QTSSPrefs object
        StrPtrLen temp(inMap->GetAttrName(inAttrIndex));
        object->fPrefName = temp.GetAsCString();//用该属性名来设置Tag名称
		//假如指定属性值Index是预设值个数,将该Tag作为Object加到最后
        if (inValueIndex == fPrefsSource->GetNumPrefValues(pref))
			/* 将指定Tag加为OBJECT Tag */
            fPrefsSource->AddNewObject(pref);
    }
    else
    {
        OSCharArrayDeleter theValueAsString(QTSSDataConverter::ValueToString(inNewValue, inNewValueLen, inMap->GetAttrType(inAttrIndex)));
        /* 设置指定index的Tag的Tag值(多值的Tag) */
		fPrefsSource->SetPrefValue(pref, inValueIndex, theValueAsString.GetObject());
    }

	/* 将指定内容写入xml文件,假如出错,记录错误日志 */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* 对入参指定的Tag,返回属性字典中单值/指定index的多值OBJECT的Tag值,否则返回空 */
ContainerRef QTSSPrefs::GetContainerRefForObject(QTSSPrefs* object)
{
    ContainerRef thisContainer = GetContainerRef();
	/* 返回第一个入参下,指定属性NAME值为inPrefName和index为0的子Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName(thisContainer, object->fPrefName);
	/* 假如tag为空,直接返回 */
    if (pref == NULL)
        return NULL;
    
	/* 对单值OBJECT,返回指定OBJECT的值 */
    if (fPrefsSource->GetNumPrefValues(pref) <= 1)
        return fPrefsSource->GetObjectValue(pref, 0);
    
    QTSSAttrInfoDict* theAttrInfoPtr = NULL;
	/* 得到该OBJECT的值的属性theAttrInfoPtr */
    QTSS_Error theErr = this->GetInstanceDictMap()->GetAttrInfoByName(object->fPrefName, &theAttrInfoPtr);
    Assert(theErr == QTSS_NoErr);
    if (theErr != QTSS_NoErr) 
		return NULL;

    QTSS_AttributeID theAttrID = qtssIllegalAttrID;
    UInt32 len = sizeof(theAttrID);
	/* 获取该OBJECT的值的属性ID */
    theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &len);
    Assert(theErr == QTSS_NoErr);
    if (theErr != QTSS_NoErr) return NULL;

    UInt32 index = 0;
    QTSSPrefs* prefObject;
    len = sizeof(prefObject);
	//获取指定属性ID和索引的属性的缓存地址和长度(最后会调整到合适长度)
    while (this->GetValue(theAttrID, index, &prefObject, &len) == QTSS_NoErr)
    {
		//假如该Tag就是入参指定的Tag,返回该tag的值
        if (prefObject == object)
        {
            /* 对多值OBJECT,返回指定index的OBJECT的值 */
            return fPrefsSource->GetObjectValue(pref, index);
        }
    }
    
    return NULL;
}

//若父目录不存在存在,返回模块名指定的Tag或Tag SERVER;若父目录存在,返回其指定的多值OBJECT的Tag值
ContainerRef QTSSPrefs::GetContainerRef()
{
	/* 在fRootTag下获取子Tag MODULE,若第一个入参为空,就生成子Tag SERVER并返回 */
    if (fParentDictionary == NULL)  // this is a top level Pref, so it must be a module
        return fPrefsSource->GetRefForModule(fPrefName);
    else    
        return fParentDictionary->GetContainerRefForObject(this);
}
