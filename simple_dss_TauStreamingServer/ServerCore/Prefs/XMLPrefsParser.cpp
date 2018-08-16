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

/* Tag名称 */
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

/* 得到fRootTag,没有就生成一个XMLTag,再设为fRootTag,返回fRootTag */
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

/* 在fRootTag下获取子Tag MODULE,若第一个入参为空,就生成子Tag SERVER并返回,否则,在有Module名时,没有子Tag MODULE就生成它并返回 */
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

/* 在fRootTag下获取子Tag SERVER并返回,没有就生成它 */
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

/* 获取子Tag PREF,返回其Tag值个数;否则返回Tag OBJECT或EMPTY-OBJECT值个数;
若都没有该Tag,就返回嵌入Tag的总个数 */
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

/* 返回指定Tag下的子Tag个数 */
UInt32 XMLPrefsParser::GetNumPrefsByContainer(ContainerRef container)
{
    return container->GetNumEmbeddedTags();
}
/* 得到指定Tag的指定index的嵌套子Tag的value */
char* XMLPrefsParser::GetPrefValueByIndex(ContainerRef container, const UInt32 inPrefsIndex, const UInt32 inValueIndex,
                                            char** outPrefName, char** outDataType)
{
	/* 先清空输出参数 */
    if (outPrefName != NULL)
        *outPrefName = NULL;
    if (outPrefName != NULL)
        *outDataType = NULL;

	/* 得到指定索引的嵌入式子Tag */
    XMLTag* pref = container->GetEmbeddedTag(inPrefsIndex);
    if (pref == NULL)
        return NULL;
        
    return GetPrefValueByRef(pref, inValueIndex, outPrefName, outDataType);
}

/* 返回指定Tag的指定index的子Tag的value,得到指定Tag的NAME和TYPE属性值,对Tag的情况作了分析 */
char* XMLPrefsParser::GetPrefValueByRef(ContainerRef pref, const UInt32 inValueIndex,
                                            char** outPrefName, char** outDataType)
{
	/*若入参非空, 得到指定Tag的属性NAME的属性值 */
    if (outPrefName != NULL)
        *outPrefName = pref->GetAttributeValue(kNameAttr);
    /*若入参非空, 得到指定Tag的属性TYPE的属性值,若属性值为空,就用"CharArray"代替 */
    if (outDataType != NULL)
    {
        *outDataType = pref->GetAttributeValue(kTypeAttr);
        if (*outDataType == NULL)
            *outDataType = "CharArray";
    }
    
	/* 假如子Tag是PREF,且为单值,返回其fTagValue */
    if (!strcmp(pref->GetTagName(), kPref))
    {
        if (inValueIndex > 0)
            return NULL;
        else
            return pref->GetValue();
    }
    
	/* 假如子Tag是LIST-PREF,返回其指定索引的嵌套子Tag的fTagValue */
    if (!strcmp(pref->GetTagName(), kListPref))
    {
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex);
        if (value != NULL)
            return value->GetValue();
    }
    
	/* 假如Tag名为OBJECT或LIST-OBJECT,数据类型设为"QTSS_Object" */
    if (!strcmp(pref->GetTagName(), kObject) || !strcmp(pref->GetTagName(), kObjectList))
        *outDataType = "QTSS_Object";
        
    return NULL;
}

/* 若为OBJECT,返回该Tag的值;若为OBJECT-LIST,返回指定index的Tag值 */
ContainerRef XMLPrefsParser::GetObjectValue(ContainerRef pref, const UInt32 inValueIndex)
{
    if (!strcmp(pref->GetTagName(), kObject) && (inValueIndex == 0))
        return pref;
    if (!strcmp(pref->GetTagName(), kObjectList))
        return pref->GetEmbeddedTag(inValueIndex);
        
    return NULL;
}

/* 返回第一个入参下,指定属性NAME值为inPrefName和index为0的子Tag */
ContainerRef XMLPrefsParser::GetPrefRefByName( ContainerRef container,
                                                    const char* inPrefName)
{
    return container->GetEmbeddedTagByAttr(kNameAttr, inPrefName);
}

/* 得到指定index的tag */
ContainerRef XMLPrefsParser::GetPrefRefByIndex( ContainerRef container,
                                                    const UInt32 inPrefsIndex)
{
    return container->GetEmbeddedTag(inPrefsIndex);
}

/* 生成指定属性值和数据类型的Tag,加入Tag内嵌队列中,并返回该Tag */
ContainerRef XMLPrefsParser::AddPref( ContainerRef container, char* inPrefName,
                                      char* inPrefDataType )
{
	/* 返回指定属性NAME值为inPrefName和index为0的Tag */
    XMLTag* pref = container->GetEmbeddedTagByAttr(kNameAttr, inPrefName);
    if (pref != NULL)
        return pref;    // it already exists
    
	/* 创建一个PREF Tag */
    pref = NEW XMLTag(kPref);   // start it out as a pref
	/* 添加指定的NAME属性的指定属性值inPrefName到属性队列中 */
    pref->AddAttribute(kNameAttr, inPrefName);

	/* 假如数据类型为"QTSS_Object",设置Tag名为"EMPTY-OBJECT" */
    if (!strcmp(inPrefDataType, "QTSS_Object"))
        pref->SetTagName(kEmptyObject);

	/* 当数据类型为"CharArray"时,设置TYPE值为指定的inPrefDataType,并添加到属性队列中 */
    else if (strcmp(inPrefDataType, "CharArray"))
        pref->AddAttribute(kTypeAttr, (char*)inPrefDataType);
    
	/* 加入嵌入式Tag队列中 */
    container->AddEmbeddedTag(pref);
    
    return pref;
}

/* 向PREF Tag中加入一个新的Tag值,分情形讨论: */
void XMLPrefsParser::AddPrefValue( ContainerRef pref, char* inNewValue)
{
	/* 假如是PREF Tag:(1)当为空Tag值时,直接加入指定的Tag值;(2)当已有一个Tag value时,将PREF Tag变为PREF-LIST Tag,
	生成一个VALUE 子Tag,其值设为原来的PREF Tag value,加入嵌套Tag队列,再新建一个VALUE Tag,将入参设为Tag值,仍加入嵌套Tag队列*/
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

			/* 将PREF Tag变为PREF-LIST Tag */
            pref->SetTagName(kListPref);
            pref->SetValue(NULL);
			/* 将VALUE Tag加入嵌套Tag队列 */
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

/* 将指定Tag加为OBJECT Tag */
void XMLPrefsParser::AddNewObject( ContainerRef pref )
{
	/* 假如为"EMPTY-OBJECT",设为OBJECT */
    if (!strcmp(pref->GetTagName(), kEmptyObject))
    {
        // just flag that this is now a real object instead of a placeholder
        pref->SetTagName(kObject);
        return;
    }
    
	/* 假如为OBJECT,变为"LIST-OBJECT",再加入一级OBJECT嵌套 */
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

/* 更改Tag的数据类型属性的属性值 */
void XMLPrefsParser::ChangePrefType( ContainerRef pref, char* inNewPrefDataType)
{
	/* 先从属性队列中移去TYPE属性,然后再加入指定属性 */
    pref->RemoveAttribute(kTypeAttr);   // remove it if it exists
    if (strcmp(inNewPrefDataType, "CharArray"))
        pref->AddAttribute(kTypeAttr, inNewPrefDataType);
}
 
/* 设置指定index的Tag的Tag值(多值的Tag) */
void XMLPrefsParser::SetPrefValue( ContainerRef pref, const UInt32 inValueIndex,
                                        char* inNewValue)
{
	/* 得到Tag值个数 */
    UInt32 numValues = GetNumPrefValues(pref);
    
    if (((numValues == 0) || (numValues == 1)) && (inValueIndex == 0))
    {
        pref->SetValue(inNewValue);
    }
    else if (inValueIndex == numValues) // this is an additional value
        AddPrefValue(pref, inNewValue);
    else
    {
		/* 设置指定index的Tag的值,这是个多值的Tag */
        XMLTag* value = pref->GetEmbeddedTag(inValueIndex);
        if (value != NULL)
            value->SetValue(inNewValue);
    }
}
 
/* 对多值Tag,删除指定index的Tag值 */
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

/* 删除指定的XMLTag */
void XMLPrefsParser::RemovePref( ContainerRef pref )
{
    delete pref;
}

/* 解析xml文件中各Tag的合法性 */
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

/* 将指定内容写入xml文件 */
int XMLPrefsParser::WritePrefsFile()
{
	/* 得到fRootTag */
    GetConfigurationTag();  // force it to be created if it doesn't exist
	/* 将指定内容写入xml文件 */
    WriteToFile(kFileHeader);
    return 0;
}
