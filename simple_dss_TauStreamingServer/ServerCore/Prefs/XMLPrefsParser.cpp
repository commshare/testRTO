/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 XMLPrefsParser.cpp
Description: A object that modify items and re-create the DTD XML config file.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "XMLPrefsParser.h"
#include "OSMemory.h"
#include "OSHeaders.h"


static const UInt32 kPrefArrayMinSize = 20;

/* Tag���� */
static char* kMainTag = "CONFIGURATION";
static char* kServer = "SERVER";
static char* kModule = "MODULE";
static char* kPref = "PREF";
static char* kListPref = "LIST-PREF";
static char* kEmptyObject = "EMPTY-OBJECT";
static char* kObject = "OBJECT";
static char* kObjectList = "LIST-OBJECT";
static char* kValue = "VALUE";
static char* kNameAttr = "NAME";
static char* kTypeAttr = "TYPE";

static char* kFileHeader[] = 
{
    "<?xml version =\"1.0\"?>",
    "<!-- The Document Type Definition (DTD) for the file -->",
    "<!DOCTYPE CONFIGURATION [",
    "<!ELEMENT CONFIGURATION (SERVER, MODULE*)>",
    "<!ELEMENT SERVER (PREF|LIST-PREF|OBJECT|LIST-OBJECT)*>",
    "<!ELEMENT MODULE (PREF|LIST-PREF|OBJECT|LIST-OBJECT)*>",
    "<!ATTLIST MODULE",
    "\tNAME CDATA #REQUIRED>",
    "<!ELEMENT PREF (#PCDATA)>",
    "<!ATTLIST PREF",
    "\tNAME CDATA #REQUIRED",
    "\tTYPE (UInt8|SInt8|UInt16|SInt16|UInt32|SInt32|UInt64|SInt64|Float32|Float64|Bool16|Bool8|char) \"char\">",
    "<!ELEMENT LIST-PREF (VALUE*)>",
    "<!ELEMENT VALUE (#PCDATA)>",
    "<!ATTLIST LIST-PREF",
    "\tNAME CDATA #REQUIRED",
    "\tTYPE  (UInt8|SInt8|UInt16|SInt16|UInt32|SInt32|UInt64|SInt64|Float32|Float64|Bool16|Bool8|char) \"char\">",
    "<!ELEMENT OBJECT (PREF|LIST-PREF|OBJECT|LIST-OBJECT)*>",
    "<!ATTLIST OBJECT",
    "\tNAME CDATA #REQUIRED>",
    "<!ELEMENT LIST-OBJECT (OBJECT-VALUE*)>",
    "<!ELEMENT OBJECT-VALUE (PREF|LIST-PREF|OBJECT|LIST-OBJECT)*>",
    "<!ATTLIST LIST-OBJECT",
    "\tNAME CDATA #REQUIRED>",
    "]>",
    NULL
};

XMLPrefsParser::XMLPrefsParser(char* inPath)
:   XMLParser(inPath)
{}

XMLPrefsParser::~XMLPrefsParser()
{}

/* �õ�fRootTag,û�о�����һ��XMLTag,����ΪfRootTag,����fRootTag */
ContainerRef XMLPrefsParser::GetConfigurationTag()
{
    ContainerRef result = GetRootTag();
    if (result == NULL)
    {
        result = new XMLTag(kMainTag);
        SetRootTag(result);
    }
    
    return result;
}

/* ��fRootTag�»�ȡ��Tag MODULE,����һ�����Ϊ��,��������Tag SERVER������,����,����Module��ʱ,û����Tag MODULE�������������� */
ContainerRef XMLPrefsParser::GetRefForModule(char* inModuleName, Bool16 create)
{
    if (inModuleName == NULL)
        return GetRefForServer();
    
    ContainerRef result = GetConfigurationTag()->GetEmbeddedTagByNameAndAttr(kModule, kNameAttr, inModuleName);
    if (result == NULL)
    {
        result = new XMLTag(kModule);
        result->AddAttribute( kNameAttr, (char*)inModuleName);
        GetRootTag()->AddEmbeddedTag(result);
    }
    
    return result;
}

/* ��fRootTag�»�ȡ��Tag SERVER������,û�о������� */
ContainerRef XMLPrefsParser::GetRefForServer()
{
    ContainerRef result = GetConfigurationTag()->GetEmbeddedTagByName(kServer);
    if (result == NULL)
    {
        result = new XMLTag(kServer);
        GetRootTag()->AddEmbeddedTag(result);
    }
    
    return result;
}

/* ��ȡ��Tag PREF,������Tagֵ����;���򷵻�Tag OBJECT��EMPTY-OBJECTֵ����;
����û�и�Tag,�ͷ���Ƕ��Tag���ܸ��� */
UInt32 XMLPrefsParser::GetNumPrefValues(ContainerRef pref)
{
    if (!strcmp(pref->GetTagName(), kPref))
    {
        if (pref->GetValue() == NULL)
            return 0;
        else
            return 1;
    }
    else if (!strcmp(pref->GetTagName(), kObject))
        return 1;
    else if (!strcmp(pref->GetTagName(), kEmptyObject))
        return 0;

    return pref->GetNumEmbeddedTags();  // it must be a list
}

/* ����ָ��Tag�µ���Tag���� */
UInt32 XMLPrefsParser::GetNumPrefsByContainer(ContainerRef container)
{
    return container->GetNumEmbeddedTags();
}
/* �õ�ָ��Tag��ָ��index��Ƕ����Tag��value */
char* XMLPrefsParser::GetPrefValueByIndex(ContainerRef container, const UInt32 inPrefsIndex, const UInt32 inValueIndex,
                                            char** outPrefName, char** outDataType)
{
	/* ������������ */
    if (outPrefName != NULL)
        *outPrefName = NULL;
    if (outPrefName != NULL)
        *outDataType = NULL;

	/* �õ�ָ��������Ƕ��ʽ��Tag */
    XMLTag* pref = container->GetEmbeddedTag(inPrefsIndex);
    if (pref == NULL)
        return NULL;
        
    return GetPrefValueByRef(pref, inValueIndex, outPrefName, outDataType);
}

/* ����ָ��Tag��ָ��index����Tag��value,�õ�ָ��Tag��NAME��TYPE����ֵ,��Tag��������˷��� */
char* XMLPrefsParser::GetPrefValueByRef(ContainerRef pref, const UInt32 inValueIndex,
                                            char** outPrefName, char** outDataType)
{
	/*����ηǿ�, �õ�ָ��Tag������NAME������ֵ */
    if (outPrefName != NULL)
        *outPrefName = pref->GetAttributeValue(kNameAttr);
    /*����ηǿ�, �õ�ָ��Tag������TYPE������ֵ,������ֵΪ��,����"CharArray"���� */
    if (outDataType != NULL)
    {
        *outDataType = pref->GetAttributeValue(kTypeAttr);
        if (*outDataType == NULL)
            *outDataType = "CharArray";
    }
    
	/* ������Tag��PREF,��Ϊ��ֵ,������fTagValue */
    if (!strcmp(pref->GetTagName(), kPref))
    {
        if (inValueIndex > 0)
            return NULL;
        else
            return pref->GetValue();
    }
    
	/* ������Tag��LIST-PREF,������ָ��������Ƕ����Tag��fTagValue */
    if (!strcmp(pref->GetTagName(), kListPref))
    {
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex);
        if (value != NULL)
            return value->GetValue();
    }
    
	/* ����Tag��ΪOBJECT��LIST-OBJECT,����������Ϊ"QTSS_Object" */
    if (!strcmp(pref->GetTagName(), kObject) || !strcmp(pref->GetTagName(), kObjectList))
        *outDataType = "QTSS_Object";
        
    return NULL;
}

/* ��ΪOBJECT,���ظ�Tag��ֵ;��ΪOBJECT-LIST,����ָ��index��Tagֵ */
ContainerRef XMLPrefsParser::GetObjectValue(ContainerRef pref, const UInt32 inValueIndex)
{
    if (!strcmp(pref->GetTagName(), kObject) && (inValueIndex == 0))
        return pref;
    if (!strcmp(pref->GetTagName(), kObjectList))
        return pref->GetEmbeddedTag(inValueIndex);
        
    return NULL;
}

/* ���ص�һ�������,ָ������NAMEֵΪinPrefName��indexΪ0����Tag */
ContainerRef XMLPrefsParser::GetPrefRefByName( ContainerRef container,
                                                    const char* inPrefName)
{
    return container->GetEmbeddedTagByAttr(kNameAttr, inPrefName);
}

/* �õ�ָ��index��tag */
ContainerRef XMLPrefsParser::GetPrefRefByIndex( ContainerRef container,
                                                    const UInt32 inPrefsIndex)
{
    return container->GetEmbeddedTag(inPrefsIndex);
}

/* ����ָ������ֵ���������͵�Tag,����Tag��Ƕ������,�����ظ�Tag */
ContainerRef XMLPrefsParser::AddPref( ContainerRef container, char* inPrefName,
                                      char* inPrefDataType )
{
	/* ����ָ������NAMEֵΪinPrefName��indexΪ0��Tag */
    XMLTag* pref = container->GetEmbeddedTagByAttr(kNameAttr, inPrefName);
    if (pref != NULL)
        return pref;    // it already exists
    
	/* ����һ��PREF Tag */
    pref = NEW XMLTag(kPref);   // start it out as a pref
	/* ���ָ����NAME���Ե�ָ������ֵinPrefName�����Զ����� */
    pref->AddAttribute(kNameAttr, inPrefName);

	/* ������������Ϊ"QTSS_Object",����Tag��Ϊ"EMPTY-OBJECT" */
    if (!strcmp(inPrefDataType, "QTSS_Object"))
        pref->SetTagName(kEmptyObject);

	/* ����������Ϊ"CharArray"ʱ,����TYPEֵΪָ����inPrefDataType,����ӵ����Զ����� */
    else if (strcmp(inPrefDataType, "CharArray"))
        pref->AddAttribute(kTypeAttr, (char*)inPrefDataType);
    
	/* ����Ƕ��ʽTag������ */
    container->AddEmbeddedTag(pref);
    
    return pref;
}

/* ��PREF Tag�м���һ���µ�Tagֵ,����������: */
void XMLPrefsParser::AddPrefValue( ContainerRef pref, char* inNewValue)
{
	/* ������PREF Tag:(1)��Ϊ��Tagֵʱ,ֱ�Ӽ���ָ����Tagֵ;(2)������һ��Tag valueʱ,��PREF Tag��ΪPREF-LIST Tag,
	����һ��VALUE ��Tag,��ֵ��Ϊԭ����PREF Tag value,����Ƕ��Tag����,���½�һ��VALUE Tag,�������ΪTagֵ,�Լ���Ƕ��Tag����*/
    if (!strcmp(pref->GetTagName(), kPref))     // is this a PREF tag
    {
        if (pref->GetValue() == NULL)
        {
            // easy case, no existing value, so just add a vlue
            pref->SetValue(inNewValue);
            return;
        }
        else
        {
            // it already has a value, so change the pref to be a list pref and go to code below
            char* firstValue = pref->GetValue();
            XMLTag* value = NEW XMLTag(kValue);
            value->SetValue(firstValue);

			/* ��PREF Tag��ΪPREF-LIST Tag */
            pref->SetTagName(kListPref);
            pref->SetValue(NULL);
			/* ��VALUE Tag����Ƕ��Tag���� */
            pref->AddEmbeddedTag(value);
        }
    }
    
    // we want to fall through from second case above, so this isn't an else
    if (!strcmp(pref->GetTagName(), kListPref))
    {
        XMLTag* value = NEW XMLTag(kValue);
        value->SetValue(inNewValue);
        pref->AddEmbeddedTag(value);
    }
}

/* ��ָ��Tag��ΪOBJECT Tag */
void XMLPrefsParser::AddNewObject( ContainerRef pref )
{
	/* ����Ϊ"EMPTY-OBJECT",��ΪOBJECT */
    if (!strcmp(pref->GetTagName(), kEmptyObject))
    {
        // just flag that this is now a real object instead of a placeholder
        pref->SetTagName(kObject);
        return;
    }
    
	/* ����ΪOBJECT,��Ϊ"LIST-OBJECT",�ټ���һ��OBJECTǶ�� */
    if (!strcmp(pref->GetTagName(), kObject))
    {
        // change the object to be an object list and go to code below
        XMLTag* subObject = NEW XMLTag(kObject);
        XMLTag* objectPref;
        // copy all this objects tags into the new listed object
        while((objectPref = pref->GetEmbeddedTag()) != NULL)
        {
            pref->RemoveEmbeddedTag(objectPref);
            subObject->AddEmbeddedTag(objectPref);
        }

        pref->SetTagName(kObjectList);
        pref->AddEmbeddedTag(subObject);
    }
    
    // we want to fall through from second case above, so this isn't an else
    if (!strcmp(pref->GetTagName(), kObjectList))
    {
        XMLTag* subObject = NEW XMLTag(kObject);
        pref->AddEmbeddedTag(subObject);
    }
}

/* ����Tag�������������Ե�����ֵ */
void XMLPrefsParser::ChangePrefType( ContainerRef pref, char* inNewPrefDataType)
{
	/* �ȴ����Զ�������ȥTYPE����,Ȼ���ټ���ָ������ */
    pref->RemoveAttribute(kTypeAttr);   // remove it if it exists
    if (strcmp(inNewPrefDataType, "CharArray"))
        pref->AddAttribute(kTypeAttr, inNewPrefDataType);
}
 
/* ����ָ��index��Tag��Tagֵ(��ֵ��Tag) */
void XMLPrefsParser::SetPrefValue( ContainerRef pref, const UInt32 inValueIndex,
                                        char* inNewValue)
{
	/* �õ�Tagֵ���� */
    UInt32 numValues = GetNumPrefValues(pref);
    
    if (((numValues == 0) || (numValues == 1)) && (inValueIndex == 0))
    {
        pref->SetValue(inNewValue);
    }
    else if (inValueIndex == numValues) // this is an additional value
        AddPrefValue(pref, inNewValue);
    else
    {
		/* ����ָ��index��Tag��ֵ,���Ǹ���ֵ��Tag */
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex);
        if (value != NULL)
            value->SetValue(inNewValue);
    }
}
 
/* �Զ�ֵTag,ɾ��ָ��index��Tagֵ */
void XMLPrefsParser::RemovePrefValue( ContainerRef pref, const UInt32 inValueIndex)
{
    UInt32 numValues = GetNumPrefValues(pref);
    if (inValueIndex >= numValues)
        return;
        
    if (numValues == 1)
    {
        delete pref;    // just remove the whole pref
    }
    else if (numValues == 2)
    {
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex); // get the one we're removing
        delete value;                                       // delete it
        value = pref->GetEmbeddedTag(0);         			// get the remaining tag index always 0 for 2 vals
        pref->RemoveEmbeddedTag(value);                     // pull it out of the parent
        if (!strcmp(pref->GetTagName(), kObjectList))
        {
            pref->SetTagName(kObject);  // set it back to a simple pref
            // move all this objects tags into the parent
            XMLTag* objectPref;
            while((objectPref = value->GetEmbeddedTag()) != NULL)
            {
                value->RemoveEmbeddedTag(objectPref);
                pref->AddEmbeddedTag(objectPref);
            }
        }
        else
        {
            char* temp = value->GetValue();
            pref->SetTagName(kPref);    // set it back to a simple pref
            pref->SetValue(temp);
        }
        
        delete value;   // get rid of the other one
    }
    else
    {
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex);
        if (value)
            delete value;
    }
}

/* ɾ��ָ����XMLTag */
void XMLPrefsParser::RemovePref( ContainerRef pref )
{
    delete pref;
}

/* ����xml�ļ��и�Tag�ĺϷ��� */
int XMLPrefsParser::Parse()
{
    char error[500];
	::memset(error,0,sizeof(error));
    
    if (!ParseFile(error, sizeof(error)))
    {
        qtss_printf("%s\n", error);
        return -1;
    }
    
    
    
    // the above routine checks that it's a valid XML file, we should check that
    // all the tags conform to our prefs format
    
    return 0;
}

/* ��ָ������д��xml�ļ� */
int XMLPrefsParser::WritePrefsFile()
{
	/* �õ�fRootTag */
    GetConfigurationTag();  // force it to be created if it doesn't exist
	/* ��ָ������д��xml�ļ� */
    WriteToFile(kFileHeader);
    return 0;
}
