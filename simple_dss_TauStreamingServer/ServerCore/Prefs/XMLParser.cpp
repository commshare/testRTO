
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 XMLParser.cpp
Description: A object that parses the DTD XML file.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-22

****************************************************************************/ 


#include "XMLParser.h"
#include "OSMemory.h"


XMLParser::XMLParser( char* inPath, DTDVerifier* verifier)
: fRootTag(NULL), fFilePath(NULL), fFile(NULL), fFileLen(0),fIsDir(false)
{
	struct stat filestat;
    StrPtrLen thePath(inPath);
    fFilePath = thePath.GetAsCString();/* 确保是c-string */
    fVerifier = verifier;
	::stat(inPath,&filestat);
	fFileLen=filestat.st_size;

    if(S_ISDIR(filestat.st_mode))
		fIsDir=true;
}

XMLParser::~XMLParser()
{
    if (fRootTag)
        delete fRootTag;

	if (fFile != NULL)
	{
		fclose(fFile);
		fFile=NULL;
	}
	
    delete [] fFilePath;
}

/* 打开xml文件,解析tag合法性,成功返回true,失败返回false */
Bool16 XMLParser::ParseFile(char* errorBuffer, int errorBufferSize)
{
    if (fRootTag != NULL)
    {
        delete fRootTag;    // flush old data
        fRootTag = NULL;
    }

    int fd = -1;
	fd = ::open(fFilePath, O_RDONLY | O_LARGEFILE);

	/* 文件大小限制 */
    if (errorBufferSize < 500) 
		errorBuffer = NULL;  // Just a hack to avoid checking everywhere

	/* 错误处理 */
    if ((fFileLen == 0) || fIsDir)
    {
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "Couldn't read xml file");
        return false;   // we don't have a valid file;
    }
    
	/* 创建载入文件的缓存 */
    char* fileData = new char[ (SInt32) (fFileLen + 1)];
    UInt64 theLengthRead = 0;
    ::memset(fileData,0,strlen(fileData));
	
	while (theLengthRead < fFileLen)
	{
		SInt32 ret = ::read(fd, fileData + theLengthRead, fFileLen - theLengthRead);
		if (ret > 0)
		{
			theLengthRead += ret;
			printf("file descriptor %d, real read data length %d \n",fd,ret);
		}
		else
		{
			printf("read data failed, real read data len %lu \n",theLengthRead);
			return false;
		}

	}
	/* 从文件开头读入整个文件到指定的缓存中,theLengthRead是已读入文件数据长度,这里应该就是文件长度! */ 
	//printf("file descriptor %d, read data %s \n",fd,fileData);

	/* 下面开始解析已读入缓存的整个xml文件 */
    StrPtrLen theDataPtr(fileData, theLengthRead);
    StringParser theParser(&theDataPtr);
    
	/* 新建root tag实例 */
    fRootTag = new XMLTag();
    Bool16 result = fRootTag->ParseTag(&theParser, fVerifier, errorBuffer, errorBufferSize);
    if (!result)
    {
        // got error parsing file
        delete fRootTag;
        fRootTag = NULL;
    }
    
	//释放内存
    delete fileData;
    
	//关闭xml文件,重置参数
	::close(fd);

    return result;
}

/* 设置root tag */
void XMLParser::SetRootTag(XMLTag* tag)
{
    if (fRootTag != NULL)
        delete fRootTag;
    fRootTag = tag;
}

/* 以可变缓存方式存入指定数据,并写入xml文件中 */
void XMLParser::WriteToFile(char** fileHeader)
{
    char theBuffer[8192];
    ResizeableStringFormatter formatter(theBuffer, 8192);  
    
    // Write the file header
    for (UInt32 a = 0; fileHeader[a] != NULL; a++)
    {
        formatter.Put(fileHeader[a]);
        formatter.Put(kEOLString);
    }
    
    if (fRootTag)
        fRootTag->FormatData(&formatter, 0);

    // New libC code. This seems to work better on Win32
    formatter.PutTerminator();
	/* 以只写方式打开xml文件,以前的内容全部清空 */
    fFile = ::fopen(fFilePath, "w");
    if (fFile == NULL)
        return;
     
	/* 写入xml文件,并关闭它 */
	::fwrite(formatter.GetBufPtr(),formatter.GetBytesWritten(),1,fFile);

    ::fclose(fFile);
	fFile=NULL;
    
	/* 改变文件属性 */
    ::chmod(fFilePath, S_IRUSR | S_IWUSR | S_IRGRP );
}

UInt8 XMLTag::sNonNameMask[] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //0-9 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //10-19 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //30-39 
    1, 1, 1, 1, 1, 0, 0, 1, 0, 0, //40-49 '.' and '-' are name chars
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //50-59 ':' is a name char
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //60-69 //stop on every character except a letter or number
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 1, 1, 1, 1, 0, 1, 0, 0, 0, //90-99 '_' is a name char
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, //120-129
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //130-139
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //140-149
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //150-159
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //160-169
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //170-179
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
    1, 1, 1, 1, 1, 1             //250-255
};

XMLTag::XMLTag() :
    fTag(NULL),
    fValue(NULL),
    fElem(NULL)
{ fElem = this;
}

XMLTag::XMLTag(char* tagName) :
    fTag(NULL),
    fValue(NULL),
    fElem(NULL)
{   fElem = this;
    StrPtrLen temp(tagName);
    fTag = temp.GetAsCString();//确保tag是C-string
}

XMLTag::~XMLTag()
{
    if (fTag)
        delete fTag;
    if (fValue)
        delete fValue;      
        
    OSQueueElem* elem;
	//逐步删除嵌套属性队列中的每个队列元
    while ((elem = fAttributes.DeQueue()) != NULL)
    {
        XMLAttribute* attr = (XMLAttribute*)elem->GetEnclosingObject();
        delete attr;
    }       

	//逐步删除嵌套Tag队列中的每个队列元
    while ((elem = fEmbeddedTags.DeQueue()) != NULL)
    {
        XMLTag* tag = (XMLTag*)elem->GetEnclosingObject();
        delete tag;
    }
    
    if (fElem.IsMemberOfAnyQueue())
		//获取当前队列,从当前队列中移除该队列元
        fElem.InQueue()->Remove(&fElem);    // remove from parent tag
}

/* 跳过xml文件中的注释行 */
void XMLTag::ConsumeIfComment(StringParser* parser)
{
    if ((parser->GetDataRemaining() > 2) && ((*parser)[1] == '-') && ((*parser)[2] == '-'))
    {
        // this is a comment, so skip to end of comment
        parser->ConsumeLength(NULL, 2); // skip '--'
        
        // look for -->
        while((parser->GetDataRemaining() > 2) && ((parser->PeekFast() != '-') ||
                ((*parser)[1] != '-') || ((*parser)[2] != '>')))
        {
            if (parser->PeekFast() == '-') 
				parser->ConsumeLength(NULL, 1);
            parser->ConsumeUntil(NULL, '-');
        }
        
        if (parser->GetDataRemaining() > 2) 
			parser->ConsumeLength(NULL, 3); // consume -->
    }
}

/* 解析单Tag或嵌套Tag的合法性 */
bool XMLTag::ParseTag(StringParser* parser, DTDVerifier* verifier, char* errorBuffer, int errorBufferSize)
{
	/* 循环查找,直到找到一个Tag行,在其开头停止 */
    while (true)
    {
		/* 假如遍历该段语句,找不到一个tag开头,返回false */
        if (!parser->GetThru(NULL, '<'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Couldn't find a valid tag");
            return false;   // couldn't find beginning of tag
        }
             
		/*获取当前字符*/
		char c = parser->PeekFast();
		/* 假如首先看到结束标签</>,返回错误 */
        if (c == '/')
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "End tag with no begin tag on line %d", parser->GetCurrentLineNumber());
            return false;   // we shouldn't be seeing a close tag here
        }
         
		/* 找到正常标签的开头 */
        if ((c != '!') && (c != '?'))
            break;  // this should be the beginning of a regular tag
        
		/* 如果没有找到Tag行,就跳过该注释行 */
        ConsumeIfComment(parser);
        // otherwise this is a processing instruction or a c-data, so look for the next tag
    }
    
	/* 记录该Tag行行号 */
    int tagStartLine = parser->GetCurrentLineNumber();
    
    StrPtrLen temp;
	/* 跳过每个Mask为0的字符,最后将经过的部分赋给temp */
    parser->ConsumeUntil(&temp, sNonNameMask);
    if (temp.Len == 0)
    {
        if (errorBuffer != NULL)
        {
            if (parser->GetDataRemaining() == 0)
                qtss_sprintf(errorBuffer, "Unexpected end of file on line %d", parser->GetCurrentLineNumber());
            else
                qtss_sprintf(errorBuffer, "Unexpected character (%c) on line %d", parser->PeekFast(), parser->GetCurrentLineNumber());
        }
        return false;   // bad file
    }
     
	/* 获取该Tag的字符串 */
    fTag = temp.GetAsCString();
    
	//跳过空格
    parser->ConsumeWhitespace();

    while ((parser->PeekFast() != '>') && (parser->PeekFast() != '/'))
    {
        // we must have an attribute value for this tag
		/* 新建一个attribute实例 */
        XMLAttribute* attr = new XMLAttribute;
		/* 加入嵌套的属性队列 */
        fAttributes.EnQueue(&attr->fElem);

		/* 跳过每个Mask为0的字符,最后将经过的部分赋给temp */
        parser->ConsumeUntil(&temp, sNonNameMask);
        if (temp.Len == 0)
        {
            if (errorBuffer != NULL)
            {
                if (parser->GetDataRemaining() == 0)
                    qtss_sprintf(errorBuffer, "Unexpected end of file on line %d", parser->GetCurrentLineNumber());
                else
                    qtss_sprintf(errorBuffer, "Unexpected character (%c) on line %d", parser->PeekFast(), parser->GetCurrentLineNumber());
            }
            return false;   // bad file
        }

		/* 得到属性名 */
        attr->fAttrName = temp.GetAsCString();

		//判断当前字符若是'=',就跳过,返回true;否则,返回false
        if (!parser->Expect('='))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Missing '=' after attribute %s on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		//判断当前字符若是'" ',就跳过,返回true;否则,返回false
        if (!parser->Expect('"'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s value not in quotes on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }
       
		/* 跳到后引号前,将经过的字符赋给temp  */
        parser->ConsumeUntil(&temp, '"');

		/* 设置属性值 */
        attr->fAttrValue = temp.GetAsCString();

		/* 若是后引号,跳过它 */
        if (!parser->Expect('"'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s value not in quotes on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }
        
		/* 判断属性名的合法性 */
        if (verifier && !verifier->IsValidAttributeName(fTag, attr->fAttrName))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s not allowed in tag %s on line %d", attr->fAttrName, fTag, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		/* 判断属性值的合法性 */
        if (verifier && !verifier->IsValidAttributeValue(fTag, attr->fAttrName, attr->fAttrValue))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Bad value for attribute %s on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		//跳过空格
        parser->ConsumeWhitespace();
    }
    
	/* 查看当前字符 '/',但不移动 */
    if (parser->PeekFast() == '/')
    {
        // this is an empty element tag, i.e. no contents or end tag (e.g <TAG attr="value" />
        parser->Expect('/');
        if (!parser->Expect('>'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "'>' must follow '/' on line %d", parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }
        
        return true;    // we're done with this tag
    }
    
	/* 判断标签后是否是'>'?若是直接移动,否则记录 */
    if (!parser->Expect('>'))
    {
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "Bad format for tag <%s> on line %d", fTag, parser->GetCurrentLineNumber());
        return false;   // bad attribute specification
    }
    
    while(true)
    {
		/* 跳到下一个 '<' */
        parser->ConsumeUntil(&temp, '<');   // this is either value or whitespace
        if (parser->GetDataRemaining() < 4)
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Reached end of file without end for tag <%s> declared on line %d", fTag, tagStartLine);
            return false;
        }
        if ((*parser)[1] == '/')
        {
            // we'll only assign a value if there were no embedded tags
            if (fEmbeddedTags.GetLength() == 0 && (!verifier || verifier->CanHaveValue(fTag)))
                fValue = temp.GetAsCString();
            else/* 有嵌套Tag子队列 */
            {
                // otherwise this needs to have been just whitespace
                StringParser tempParser(&temp);
                tempParser.ConsumeWhitespace();
                if (tempParser.GetDataRemaining() > 0)
                {
                    if (errorBuffer)
                    {
                        if (fEmbeddedTags.GetLength() > 0)
                            qtss_sprintf(errorBuffer, "Unexpected text outside of tag on line %d", tagStartLine);
                        else
                            qtss_sprintf(errorBuffer, "Tag <%s> on line %d not allowed to have data", fTag, tagStartLine);
                    }
                }
            }
            break;  // we're all done with this tag
        }
        
        if (((*parser)[1] != '!') && ((*parser)[1] != '?'))
        {
            // this must be the beginning of an embedded tag
			/* 新建一个嵌入Tag */
            XMLTag* tag = NEW XMLTag();
			/* 加入嵌入Tag队列 */
            fEmbeddedTags.EnQueue(&tag->fElem);
			/* 解析该嵌入Tag */
            if (!tag->ParseTag(parser, verifier, errorBuffer, errorBufferSize))
                return false;
             
			/* 验证subTag合法性 */
            if (verifier && !verifier->IsValidSubtag(fTag, tag->GetTagName()))
            {
                if (errorBuffer != NULL)
                    qtss_sprintf(errorBuffer, "Tag %s not allowed in tag %s on line %d", tag->GetTagName(), fTag, parser->GetCurrentLineNumber());
                return false;   // bad attribute specification
            }
        }
        else
        {
            parser->ConsumeLength(NULL, 1); // skip '<'
			/* 跳过注释行 */
            ConsumeIfComment(parser);
        }
    }

    parser->ConsumeLength(NULL, 2); // skip '</'
    parser->ConsumeUntil(&temp, sNonNameMask);
    if (!temp.Equal(fTag))
    {
        char* newTag = temp.GetAsCString();
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "End tag </%s> on line %d doesn't match tag <%s> declared on line %d", newTag, parser->GetCurrentLineNumber(),fTag, tagStartLine);
        delete newTag;
        return false;   // bad attribute specification
    }
    
    if (!parser->GetThru(NULL, '>'))
    {
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "Couldn't find end of tag <%s> declared on line %d", fTag, tagStartLine);
        return false;   // bad attribute specification
    }
    
    return true;
}

/* 遍历属性数组,查找指定属性名的属性值 */
char* XMLTag::GetAttributeValue(const char* attrName)
{
    for (OSQueueIter iter(&fAttributes); !iter.IsDone(); iter.Next())
    {
        XMLAttribute* attr = (XMLAttribute*)iter.GetCurrent()->GetEnclosingObject();
        if (!strcmp(attr->fAttrName, attrName))
            return attr->fAttrValue;
    }
    
    return NULL;
}

/*  找到指定index的Tag */
XMLTag* XMLTag::GetEmbeddedTag(const UInt32 index)
{
    if (fEmbeddedTags.GetLength() <= index)
        return NULL;
    
    OSQueueIter iter(&fEmbeddedTags);
    for (UInt32 i = 0; i < index; i++)
    {
        iter.Next();
    }
    OSQueueElem* result = iter.GetCurrent();
    
    return (XMLTag*)result->GetEnclosingObject();
}

/* 遍历嵌套Tag数组,查找指定index的Tag */
XMLTag* XMLTag::GetEmbeddedTagByName(const char* tagName, const UInt32 index)
{
    if (fEmbeddedTags.GetLength() <= index)
        return NULL;
    
    XMLTag* result = NULL;
    UInt32 curIndex = 0;
    for (OSQueueIter iter(&fEmbeddedTags); !iter.IsDone(); iter.Next())
    {
        XMLTag* temp = (XMLTag*)iter.GetCurrent()->GetEnclosingObject();
        if (!strcmp(temp->GetTagName(), tagName))
        {
            if (curIndex == index)
            {
                result = temp;
                break;
            }
                
            curIndex++;
        }
    }
    
    return result;
}

/* 遍历嵌套Tag数组,查找指定index和属性值的Tag */
XMLTag* XMLTag::GetEmbeddedTagByAttr(const char* attrName, const char* attrValue, const UInt32 index)
{
    if (fEmbeddedTags.GetLength() <= index)
        return NULL;
    
    XMLTag* result = NULL;
    UInt32 curIndex = 0;
    for (OSQueueIter iter(&fEmbeddedTags); !iter.IsDone(); iter.Next())
    {
        XMLTag* temp = (XMLTag*)iter.GetCurrent()->GetEnclosingObject();
        if ((temp->GetAttributeValue(attrName) != NULL) && (!strcmp(temp->GetAttributeValue(attrName), attrValue)))
        {
            if (curIndex == index)
            {
                result = temp;
                break;
            }
                
            curIndex++;
        }
    }
    
    return result;
}

/* 遍历嵌套Tag数组,查找指定index,Tag名称和属性值的Tag */
XMLTag* XMLTag::GetEmbeddedTagByNameAndAttr(const char* tagName, const char* attrName, const char* attrValue, const UInt32 index)
{
    if (fEmbeddedTags.GetLength() <= index)
        return NULL;
    
    XMLTag* result = NULL;
    UInt32 curIndex = 0;
    for (OSQueueIter iter(&fEmbeddedTags); !iter.IsDone(); iter.Next())
    {
        XMLTag* temp = (XMLTag*)iter.GetCurrent()->GetEnclosingObject();
        if (!strcmp(temp->GetTagName(), tagName) && (temp->GetAttributeValue(attrName) != NULL) && 
            (!strcmp(temp->GetAttributeValue(attrName), attrValue)))
        {
            if (curIndex == index)
            {
                result = temp;
                break;
            }
                
            curIndex++;
        }
    }
    
    return result;
}

/* 添加指定属性名和属性值的属性到属性队列中 */
void XMLTag::AddAttribute( char* attrName, char* attrValue)
{
    XMLAttribute* attr = NEW XMLAttribute;
    StrPtrLen temp(attrName);
    attr->fAttrName = temp.GetAsCString();
    temp.Set(attrValue);
    attr->fAttrValue = temp.GetAsCString();
    
    fAttributes.EnQueue(&attr->fElem);
}

/* 删除指定属性名的属性 */
void XMLTag::RemoveAttribute(char* attrName)
{
    for (OSQueueIter iter(&fAttributes); !iter.IsDone(); iter.Next())
    {
        XMLAttribute* attr = (XMLAttribute*)iter.GetCurrent()->GetEnclosingObject();
        if (!strcmp(attr->fAttrName, attrName))
        {
            fAttributes.Remove(&attr->fElem);
            delete attr;
            return;
        }
    }
}

/* 添加指定Tag给嵌套Tag队列 */
void XMLTag::AddEmbeddedTag(XMLTag* tag)
{
    fEmbeddedTags.EnQueue(&tag->fElem);
}

/* 移除指定Tag从嵌套Tag队列 */
void XMLTag::RemoveEmbeddedTag(XMLTag* tag)
{
    fEmbeddedTags.Remove(&tag->fElem);
}

/* 给当前Tag设置指定的名称 */
void XMLTag::SetTagName( char* name)
{
    Assert (name != NULL);  // can't have a tag without a name!
    
    if (fTag != NULL)
        delete fTag;
        
    StrPtrLen temp(name);
    fTag = temp.GetAsCString();
}

/* 给单Tag设置值,对嵌套Tag无效,直接返回 */
void XMLTag::SetValue( char* value)
{
    if (fEmbeddedTags.GetLength() > 0)
        return;     // can't have a value with embedded tags
        
    if (fValue != NULL)
        delete fValue;
        
    if (value == NULL)
        fValue = NULL;
    else
    {
        StrPtrLen temp(value);
        fValue = temp.GetAsCString();
    }
}
  
/* 生成指定格式的空第二个参数的Tag行"\t\t...\t<fTag 属性=\""属性值\" "...属性=\""属性值\" ">fValue<\fTag>\r\n" 或更复杂的嵌套结构,放入第一个入参中 */
void XMLTag::FormatData(ResizeableStringFormatter* formatter, UInt32 indent)
{
	/* 添加指定个数的'\t' */
    for (UInt32 i=0; i<indent; i++) 
		formatter->PutChar('\t');

    formatter->PutChar('<');
    formatter->Put(fTag);
    if (fAttributes.GetLength() > 0)
    {
        formatter->PutChar(' ');
		/* 循环放入该fTag的属性数组 */
        for (OSQueueIter iter(&fAttributes); !iter.IsDone(); iter.Next())
        {
            XMLAttribute* attr = (XMLAttribute*)iter.GetCurrent()->GetEnclosingObject();
            formatter->Put(attr->fAttrName);
            formatter->Put("=\"");
            formatter->Put(attr->fAttrValue);
            formatter->Put("\" ");
        }
    }
    formatter->PutChar('>');
    
	/* 假如为单Tag */
    if (fEmbeddedTags.GetLength() == 0)
    {
        if (fValue > 0)
            formatter->Put(fValue);
    }
    else/* 若为嵌套Tag */
    {
        formatter->Put(kEOLString);
		/* 遍历嵌套Tag数组 */
        for (OSQueueIter iter(&fEmbeddedTags); !iter.IsDone(); iter.Next())
        {
			/* 获取当前Tag */
            XMLTag* current = (XMLTag*)iter.GetCurrent()->GetEnclosingObject();
			/* 后退一格,放入属性 */
            current->FormatData(formatter, indent + 1);
        }

        for (UInt32 i=0; i<indent; i++) 
			formatter->PutChar('\t');
    }
    
    formatter->Put("</");
    formatter->Put(fTag);
    formatter->PutChar('>');
    formatter->Put(kEOLString);
}

XMLAttribute::XMLAttribute()
    : fAttrName(NULL),
    fAttrValue(NULL)
{   fElem = this;
}

XMLAttribute::~XMLAttribute()
{
    if (fAttrName)
        delete fAttrName;
    if (fAttrValue)
        delete fAttrValue;      
}
