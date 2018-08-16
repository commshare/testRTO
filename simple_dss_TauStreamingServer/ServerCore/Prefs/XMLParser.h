
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 XMLParser.h
Description: A object that parses the DTD XML file.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-23

****************************************************************************/ 


#ifndef __XMLParser_h__
#define __XMLParser_h__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "StringParser.h"
#include "OSQueue.h"
#include "ResizeableStringFormatter.h"

/* 注意虚基函数在子类中派生 */
class DTDVerifier
{
public:
	virtual ~DTDVerifier(){}
    virtual bool IsValidSubtag(char* tagName, char* subTagName) = 0;
    virtual bool IsValidAttributeName(char* tagName, char* attrName) = 0;
    virtual bool IsValidAttributeValue(char* tagName, char* attrName, char* attrValue) = 0;
    virtual char* GetRequiredAttribute(char* tagName, int index) = 0;
    virtual bool CanHaveValue(char* tagName) = 0;
};

class XMLTag
{
public:
    XMLTag();
    XMLTag(char* tagName);
    ~XMLTag();
    
	/* 解析单Tag或嵌套Tag的合法性 */
    bool ParseTag(StringParser* parser, DTDVerifier* verifier, char* errorBuffer = NULL, int errorBufferSize = 0);
    
    char* GetAttributeValue(const char* attrName);
    char* GetValue() { return fValue; }
    char* GetTagName() { return fTag; }
    
    UInt32 GetNumEmbeddedTags() { return fEmbeddedTags.GetLength(); }
    
    XMLTag* GetEmbeddedTag(const UInt32 index = 0);
    XMLTag* GetEmbeddedTagByName(const char* tagName, const UInt32 index = 0);
    XMLTag* GetEmbeddedTagByAttr(const char* attrName, const char* attrValue, const UInt32 index = 0);
    XMLTag* GetEmbeddedTagByNameAndAttr(const char* tagName, const char* attrName, const char* attrValue, const UInt32 index = 0);
    
    void AddAttribute(char* attrName, char* attrValue);
    void RemoveAttribute(char* attrName);
    void AddEmbeddedTag(XMLTag* tag);
    void RemoveEmbeddedTag(XMLTag* tag);
    
    void SetTagName( char* name);
    void SetValue( char* value);
    
	/* 生成指定格式的空第二个参数的Tag行"\t\t...\t<fTag 属性=\""属性值\" "...属性=\""属性值\" ">fValue<\fTag>\r\n" 或更复杂的嵌套结构,放入第一个入参中 */
    void FormatData(ResizeableStringFormatter* formatter, UInt32 indent);

private:
    void ConsumeIfComment(StringParser* parser);

    char*   fTag;/* Tag名称 */
    char*   fValue;/* Tag值 */
    OSQueue fAttributes;/* 该Tag的属性数组 */
    OSQueue fEmbeddedTags;/* 该Tag的嵌套子Tag数组 */
    
    OSQueueElem fElem;/* 在嵌套Tag队列中作为一个队列元 */

    static UInt8 sNonNameMask[];        // stop when you hit a word
};

class XMLAttribute
{
public:
    XMLAttribute();
    ~XMLAttribute();
    
    char* fAttrName;
    char* fAttrValue;
    
    OSQueueElem fElem;
};

/* 对xml文件进行解析的类 */
class XMLParser
{
public:
    XMLParser( char* inPath, DTDVerifier* verifier = NULL);
    ~XMLParser();
       
	/* 打开xml文件,解析tag合法性,成功返回true,失败返回false */
    Bool16  ParseFile(char* errorBuffer = NULL, int errorBufferSize = 0);
        
    XMLTag* GetRootTag() { return fRootTag; }
    void SetRootTag(XMLTag* tag);
    
	/* 以可变缓存方式存入指定数据,并写入xml文件中 */
    void WriteToFile(char** fileHeader);
    
private:
         
    FILE*           fFile;
    char*           fFilePath;
	UInt64          fFileLen;
	bool            fIsDir;
	XMLTag*         fRootTag;
    DTDVerifier*    fVerifier;
};

#endif
