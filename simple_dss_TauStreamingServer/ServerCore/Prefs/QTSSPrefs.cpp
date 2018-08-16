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
{   //��ģ�����������ݳ�ԱfPrefName
    if (inModuleName != NULL)
        fPrefName = inModuleName->GetAsCString();
}


/* ����Ԥ��ֵ����,��ʵ����,�ٷ��� */
QTSSDictionary* QTSSPrefs::CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* /* inMutex */)
{
    return NEW QTSSPrefs(fPrefsSource, NULL, inMap, true, this );
}

/* ���Ǳ�������Ҫ��һ������,�ۺ��Լ�ǿ!��ȡÿ��Module��Ԥ��ֵ */
void QTSSPrefs::RereadPreferences()
{
    RereadObjectPreferences(GetContainerRef());
}
 
/* ע��������InstanceDictMap ? used in QTSServer::RereadPrefsService() */
void QTSSPrefs::RereadObjectPreferences(ContainerRef container)
{
    QTSS_Error theErr = QTSS_NoErr;
    
    // Keep track of which pref attributes should remain. All others will be removed. 
    // This routine uses names because it adds and deletes attributes. This means attribute indexes,positions and counts are constantly changing.
    UInt32 initialNumAttrs = 0;
	/* �Ȼ��ʵ������Dictӳ��,�ٵõ���ʼ���Եĸ��� */
    if (this->GetInstanceDictMap() != NULL)
    {   
    	initialNumAttrs = this->GetInstanceDictMap()->GetNumAttrs();
    }

	/********************* ע�������ǳ���Ҫ!! ************************/
	/* ����Server��module pref���ַ�����,���С��������ĳ�ʼ���Եĸ��� */
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
	/********************* ע�������ǳ���Ҫ!! ************************/
  	
    OSMutexLocker locker(&fPrefsMutex);
	/* ����ָ��Tag�µ���Tag���� */
    UInt32 theNumPrefs = fPrefsSource->GetNumPrefsByContainer(container);

	/* ������ʼ�����Ը���,�õ�ָ��index��QTSSAttrInfoDictָ��,�õ�Server��module pref���ַ���,������������ַ����� */
    for (UInt32 i = 0; i < initialNumAttrs;i++) // pull out all the names in the server 
    {   
        QTSSAttrInfoDict* theAttrInfoPtr = NULL;
        theErr = this->GetInstanceDictMap()->GetAttrInfoByIndex(i, &theAttrInfoPtr);
        if (theErr != QTSS_NoErr)
            continue; //�������,������
        
        UInt32 nameLen = 0;
        theErr = theAttrInfoPtr->GetValuePtr(qtssAttrName,0, (void **) &modulePrefInServer[i], &nameLen);
        Assert(theErr == QTSS_NoErr);
        qtss_printf("QTSSPrefs::RereadPreferences modulePrefInServer in server=%s\n",modulePrefInServer[i]);
    }
    
    // Use the names of the attributes in the attribute map as the key values for
    // finding preferences in the config file.
    //�������е�Ԥ��ֵ����,����xml�ļ�,�����Ϸ���,ɾȥ���Ϸ���,�����µ�����,��ά��modulePrefInServer����
    for (UInt32 x = 0; x < theNumPrefs; x++)
    {
        char* thePrefTypeStr = NULL;
        char* thePrefName = NULL;
		/* ��ȡcontainer�µ�x����Tag�ĵ�0����Tag��ֵ�����������ַ���*/
        (void)fPrefsSource->GetPrefValueByIndex(container, x, 0, &thePrefName, &thePrefTypeStr);

        // What type is this data type?
		/* �������ַ���ת�������� */
        QTSS_AttrDataType thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);

        // ���ʵ��ӳ�����Ƿ����ָ�����Ƶ�����,������,������Ӹ�����
        // Check to see if there is an attribute with this name already in the
        // instance map. If one matches, then we don't need to add this attribute.
        QTSSAttrInfoDict* theAttrInfo = NULL;
        if (this->GetInstanceDictMap() != NULL)
			/* ��ȡ QTSSAttrInfoDictָ�� */
            (void)this->GetInstanceDictMap()->GetAttrInfoByName(thePrefName, &theAttrInfo, false ); // false=don't return info on deleted attributes
                                                                                                                        
        UInt32 theLen = sizeof(QTSS_AttrDataType);
        QTSS_AttributeID theAttrID = qtssIllegalAttrID;
        // ������ʼ������,see if this name is in the server,�������,�������
        for (UInt32 i = 0; i < initialNumAttrs;i++) 
        {   
			if (modulePrefInServer[i] != NULL && thePrefName != NULL && 0 == ::strcmp(modulePrefInServer[i],thePrefName))
            {  
				modulePrefInServer[i] = NULL; // in the server so don't delete later
                qtss_printf("QTSSPrefs::RereadPreferences modulePrefInServer in file and in server=%s\n",thePrefName);
            }
        }

		//����ʵ��ӳ���в�����ָ�����Ƶ�����,����ӽ���ʵ������,�÷��ص�����ID��xml�ļ��л�ȡԤ��ֵ�����ø�����ֵ
        if ( theAttrInfo == NULL )
        {
            theAttrID = this->AddPrefAttribute(thePrefName, thePrefType); // not present or deleted
            this->SetPrefValuesFromFile(container, x, theAttrID, 0); // will add another or replace a deleted attribute
        }
        //����ʵ��ӳ���д���ָ�����Ƶ�����,�ͻ�ȡ�����Ե��������ͺ�ID,�ж���Ԥ��ֵ�����Ƿ���ͬ?��ͬ�����µ����Դ���,����������ֵ
        else
        {
            QTSS_AttrDataType theAttrType = qtssAttrDataTypeUnknown;
            theErr = theAttrInfo->GetValue(qtssAttrDataType, 0, &theAttrType, &theLen);
            Assert(theErr == QTSS_NoErr);
            
            theLen = sizeof(theAttrID);
            theErr = theAttrInfo->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
            Assert(theErr == QTSS_NoErr);

			//�����������ͺ�Ԥ��ֵ���Ͳ�ͬ, ����ȥ��ʵ������,����ӽ��µ�ʵ������
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
            
            // Set the values,��ָ������ID��xml�ļ��л�ȡԤ��ֵ�����ø�����ֵ
            this->SetPrefValuesFromFile(container, x, theAttrID, 0);

            // Mark this pref as found.����߱���Ϊ0,��Ǹ�Pref����ֵ�Ѿ��ҵ�
            SInt32 theIndex = this->GetInstanceDictMap()->ConvertAttrIDToArrayIndex(theAttrID);
            Assert(theIndex >= 0);
        }
    }
    
    // Remove all attributes that no longer apply
	// ��ʵ��ӳ����,ɾȥmodulePrefInServer�����з����ǿյ���,���÷����ÿ�
    if (this->GetInstanceDictMap() != NULL && initialNumAttrs > 0)
    {   
        for (UInt32 a = 0; a < initialNumAttrs; a++) 
        {
            if (NULL != modulePrefInServer[a]) // found a pref in the server that wasn't in the file
            {   
				//�õ�QTSSAttrInfoDictָ��
                QTSSAttrInfoDict* theAttrInfoPtr = NULL;
                theErr = this->GetInstanceDictMap()->GetAttrInfoByName(modulePrefInServer[a], &theAttrInfoPtr);
                Assert(theErr == QTSS_NoErr);
                if (theErr != QTSS_NoErr) continue; //�������,������һ��
        
				//�õ�����ID
                QTSS_AttributeID theAttrID = qtssIllegalAttrID; 
                UInt32 theLen = sizeof(theAttrID);
                theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
                Assert(theErr == QTSS_NoErr);
                if (theErr != QTSS_NoErr) continue; //�������,������һ��
                            
                if (0)
                {   char* theName = NULL;
                    UInt32 nameLen = 0;
                    theAttrInfoPtr->GetValuePtr(qtssAttrName,0, (void **) &theName, &nameLen);
                    qtss_printf("QTSSPrefs::RereadPreferences about to delete modulePrefInServer=%s attr=%s id=%lu\n",modulePrefInServer[a], theName,theAttrID);
                }
            
                //ɾ����ʵ������
                this->GetInstanceDictMap()->RemoveAttribute(theAttrID);
                modulePrefInServer[a] = NULL;
            }
        }
    }
    
    delete modulePrefInServer; //ɾȥ��̬�����ĸ��ַ�����
}
        
void QTSSPrefs::SetPrefValuesFromFile(ContainerRef container, UInt32 inPrefIndex, QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
	/* ��xml����ֵ�ļ���,��ȡָ��Tag��ָ����(XML)Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByIndex(container, inPrefIndex);
    SetPrefValuesFromFileWithRef(pref, inAttrID, inNumValues);
}
 
/* �õ�ָ��Tag��ָ��index����Tag��NAME��TYPE����ֵ,���ظ�Tag��ֵ;���ֵ��е�����ID,���Ƿ����ν�XMLTag�е���Tag��
���ֵ����DictionaryMap��(��һ��ȷ������ֵ��󳤶�,�ڶ����������),����������Ը��� */
void QTSSPrefs::SetPrefValuesFromFileWithRef(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues)
{
    // We have an attribute ID for this pref, it is in the map and everything.
    // Now, let's add all the values that are in the pref file.
	/* ����û��ָ��Tag������ */
    if (pref == 0)
        return;
    
	/* �õ�ָ��Tagֵ�ĸ��� */
    UInt32 numPrefValues = inNumValues;
    if (inNumValues == 0)
        numPrefValues = fPrefsSource->GetNumPrefValues(pref);
        
    char* thePrefName = NULL;
    char* thePrefValue = NULL;
    char* thePrefTypeStr = NULL;
    QTSS_AttrDataType thePrefType = qtssAttrDataTypeUnknown;
    
    // find the type.  If this is a QTSSObject, then we need to call a different routine
	/* �õ�ָ��Tag��ָ��index(�˴�Ϊ0)����Tag��NAME��TYPE����ֵ,���ظ�Tag��ֵ */
    thePrefValue = fPrefsSource->GetPrefValueByRef( pref, 0, &thePrefName, &thePrefTypeStr);
    thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);//�������ַ���ת��Ϊ����
	/* ����������QTSS_Object,���ض�����ת��,������ */
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
	/* ���������Tag��ص�ѭ��,��һ�εõ�����ֵ����󳤶� */
    for (UInt32 y = 0; y < numPrefValues; y++)
    {
        UInt32 tempMaxPrefValueSize = 0;
        /* �õ�ָ��Tag��ָ��index����Tag��NAME��TYPE����ֵ,���ظ�Tag��ֵ */
        thePrefValue = fPrefsSource->GetPrefValueByRef( pref, y, &thePrefName, &thePrefTypeStr);

        theErr = QTSSDataConverter::StringToValue(  thePrefValue, thePrefType,NULL, &tempMaxPrefValueSize );                                             
        Assert(theErr == QTSS_NotEnoughSpace);
        
        if (tempMaxPrefValueSize > maxPrefValueSize)
            maxPrefValueSize = tempMaxPrefValueSize;
    }

    /* �ڶ���ѭ��,��xml�����ļ��л�ȡ����ֵ,������ΪԤ��ֵ */
    for (UInt32 z = 0; z < numPrefValues; z++)
    {
        thePrefValue = fPrefsSource->GetPrefValueByRef( pref, z, &thePrefName, &thePrefTypeStr);
        this->SetPrefValue(inAttrID, z, thePrefValue, thePrefType, maxPrefValueSize);
    }
    
    // Make sure the dictionary knows exactly how many values are associated with this pref
    // ���ø�ָ�����Ե�Ԥ��ֵ����,��Pref�ֵ�֪��
    this->SetNumValues(inAttrID, numPrefValues);
}
 
/* ����ָ������ID��Object������,�ȶ�ȡָ��index��������Tagֵ,�����μ���QTSSDictionary��,����ָ������ID�����Ը��� */
void QTSSPrefs::SetObjectValuesFromFile(ContainerRef pref, QTSS_AttributeID inAttrID, UInt32 inNumValues, char* prefName)
{
	//�Ը�ָ������ID��Object������,�������ļ����������
    for (UInt32 z = 0; z < inNumValues; z++)
    {
		/* ��ΪOBJECT,���ظ�Tag��ֵ;��ΪOBJECT-LIST,����ָ��index����Tagֵ */
        ContainerRef object = fPrefsSource->GetObjectValue( pref, z );
        QTSSPrefs* prefObject;
        UInt32 len = sizeof(QTSSPrefs*);
		/* �õ�ָ������ID������������ֵ,����ʧ�ܾʹ���һ���µ�ObjectValue */
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
            prefObject->fPrefName = temp.GetAsCString(); //�����һ���������Ԥ��ֵ���������
        }
		//����ָ�������Ԥ��ֵ
        prefObject->RereadObjectPreferences(object);
    }
    
    // Make sure the dictionary knows exactly how many values are associated with this pref
    // ����Ӧ�ֵ������ø�ָ������ID�����Ե��ܸ���
    this->SetNumValues(inAttrID, inNumValues);
}

/* ��QTSSDictionary������ָ������ID������index������ֵ(�Ƚ�������ת��) */
void    QTSSPrefs::SetPrefValue(QTSS_AttributeID inAttrID, UInt32 inAttrIndex,
                                char* inPrefValue, QTSS_AttrDataType inPrefType, UInt32 inValueSize)
                        
{
    static const UInt32 kMaxPrefValueSize = 1024;
    char convertedPrefValue[kMaxPrefValueSize];
    ::memset(convertedPrefValue, 0, kMaxPrefValueSize);
	/* ָ������ֵ����С�����Գ������ֵ */
    Assert(inValueSize < kMaxPrefValueSize);
    
	/* ���ַ���ת��Ϊ��ֵ��ʽ�洢 */
    UInt32 convertedBufSize = kMaxPrefValueSize;
    QTSS_Error theErr = QTSSDataConverter::StringToValue(inPrefValue, inPrefType, convertedPrefValue, &convertedBufSize );     
    Assert(theErr == QTSS_NoErr);
    
    if (inValueSize == 0)
        inValueSize = convertedBufSize;
     
	//����Ӧ�ֵ�������ָ������ID�����������Ե�ֵ
    this->SetValue(inAttrID, inAttrIndex, convertedPrefValue, inValueSize, QTSSDictionary::kDontObeyReadOnly | QTSSDictionary::kDontCallCompletionRoutine);                         

}

/* ��QTSSDictionary���ָ�����������������͵�ʵ������,��ȡ�����ظ�������������ID */
QTSS_AttributeID QTSSPrefs::AddPrefAttribute(const char* inAttrName, QTSS_AttrDataType inDataType)
{
    QTSS_Error theErr = this->AddInstanceAttribute( inAttrName, NULL, inDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModeDelete);
    Assert(theErr == QTSS_NoErr);
    
    QTSS_AttributeID theID = qtssIllegalAttrID;
    theErr = this->GetInstanceDictMap()->GetAttrID( inAttrName, &theID);
    Assert(theErr == QTSS_NoErr);
    
    return theID;
}

/* ����ָ������NAMEֵΪinPrefName��indexΪ0����Tag, �Զ�ֵTag,ɾ��ָ��index��Tagֵ  */
void    QTSSPrefs::RemoveValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
                                        UInt32 inValueIndex)
{
	/* �õ�Tag */
    ContainerRef objectRef = GetContainerRef();
	/* ���ص�һ�������,ָ������NAMEֵΪinPrefName��indexΪ0����Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName( objectRef, inMap->GetAttrName(inAttrIndex));
    Assert(pref != NULL);
    if (pref != NULL)
		/* �Զ�ֵTag,ɾ��ָ��index��Tagֵ */
        fPrefsSource->RemovePrefValue( pref, inValueIndex);
    
	/* ��ָ������д��xml�ļ�,�������,��¼������־ */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* ���ҵ�ָ������NAMEֵΪinPrefName��indexΪ0����Tag,ɾ���� */
void    QTSSPrefs::RemoveInstanceAttrComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap)
{
    ContainerRef objectRef = GetContainerRef();
	/* ���ص�һ�������,ָ������NAMEֵΪinPrefName��indexΪ0����Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName( objectRef, inMap->GetAttrName(inAttrIndex));
    Assert(pref != NULL);
    if (pref != NULL)
    {
		/* ɾ��ָ����XMLTag */
        fPrefsSource->RemovePref(pref);
    }
    
	/* ��ָ������д��xml�ļ�,�������,��¼������־ */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* ������ָ������ֵ���������͵�Tag,����Tag��Ƕ������,��������Tagֵ  */
void QTSSPrefs::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
                                    UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
    ContainerRef objectRef = GetContainerRef();
	/* ����ָ������ֵ���������͵�Tag,����Tag��Ƕ������,�����ظ�Tag */
    ContainerRef pref = fPrefsSource->AddPref(objectRef, inMap->GetAttrName(inAttrIndex), QTSSDataConverter::TypeToTypeString(inMap->GetAttrType(inAttrIndex)));
    
	//���������������QTSS_Object
	if (inMap->GetAttrType(inAttrIndex) == qtssAttrDataTypeQTSS_Object)
    {
        QTSSPrefs* object = *(QTSSPrefs**)inNewValue;   // value is a pointer to a QTSSPrefs object
        StrPtrLen temp(inMap->GetAttrName(inAttrIndex));
        object->fPrefName = temp.GetAsCString();//�ø�������������Tag����
		//����ָ������ֵIndex��Ԥ��ֵ����,����Tag��ΪObject�ӵ����
        if (inValueIndex == fPrefsSource->GetNumPrefValues(pref))
			/* ��ָ��Tag��ΪOBJECT Tag */
            fPrefsSource->AddNewObject(pref);
    }
    else
    {
        OSCharArrayDeleter theValueAsString(QTSSDataConverter::ValueToString(inNewValue, inNewValueLen, inMap->GetAttrType(inAttrIndex)));
        /* ����ָ��index��Tag��Tagֵ(��ֵ��Tag) */
		fPrefsSource->SetPrefValue(pref, inValueIndex, theValueAsString.GetObject());
    }

	/* ��ָ������д��xml�ļ�,�������,��¼������־ */
    if (fPrefsSource->WritePrefsFile())
        QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

/* �����ָ����Tag,���������ֵ��е�ֵ/ָ��index�Ķ�ֵOBJECT��Tagֵ,���򷵻ؿ� */
ContainerRef QTSSPrefs::GetContainerRefForObject(QTSSPrefs* object)
{
    ContainerRef thisContainer = GetContainerRef();
	/* ���ص�һ�������,ָ������NAMEֵΪinPrefName��indexΪ0����Tag */
    ContainerRef pref = fPrefsSource->GetPrefRefByName(thisContainer, object->fPrefName);
	/* ����tagΪ��,ֱ�ӷ��� */
    if (pref == NULL)
        return NULL;
    
	/* �Ե�ֵOBJECT,����ָ��OBJECT��ֵ */
    if (fPrefsSource->GetNumPrefValues(pref) <= 1)
        return fPrefsSource->GetObjectValue(pref, 0);
    
    QTSSAttrInfoDict* theAttrInfoPtr = NULL;
	/* �õ���OBJECT��ֵ������theAttrInfoPtr */
    QTSS_Error theErr = this->GetInstanceDictMap()->GetAttrInfoByName(object->fPrefName, &theAttrInfoPtr);
    Assert(theErr == QTSS_NoErr);
    if (theErr != QTSS_NoErr) 
		return NULL;

    QTSS_AttributeID theAttrID = qtssIllegalAttrID;
    UInt32 len = sizeof(theAttrID);
	/* ��ȡ��OBJECT��ֵ������ID */
    theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &len);
    Assert(theErr == QTSS_NoErr);
    if (theErr != QTSS_NoErr) return NULL;

    UInt32 index = 0;
    QTSSPrefs* prefObject;
    len = sizeof(prefObject);
	//��ȡָ������ID�����������ԵĻ����ַ�ͳ���(������������ʳ���)
    while (this->GetValue(theAttrID, index, &prefObject, &len) == QTSS_NoErr)
    {
		//�����Tag�������ָ����Tag,���ظ�tag��ֵ
        if (prefObject == object)
        {
            /* �Զ�ֵOBJECT,����ָ��index��OBJECT��ֵ */
            return fPrefsSource->GetObjectValue(pref, index);
        }
    }
    
    return NULL;
}

//����Ŀ¼�����ڴ���,����ģ����ָ����Tag��Tag SERVER;����Ŀ¼����,������ָ���Ķ�ֵOBJECT��Tagֵ
ContainerRef QTSSPrefs::GetContainerRef()
{
	/* ��fRootTag�»�ȡ��Tag MODULE,����һ�����Ϊ��,��������Tag SERVER������ */
    if (fParentDictionary == NULL)  // this is a top level Pref, so it must be a module
        return fPrefsSource->GetRefForModule(fPrefName);
    else    
        return fParentDictionary->GetContainerRefForObject(this);
}
