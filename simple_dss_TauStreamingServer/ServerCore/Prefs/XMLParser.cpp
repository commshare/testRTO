
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
    fFilePath = thePath.GetAsCString();/* ȷ����c-string */
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

/* ��xml�ļ�,����tag�Ϸ���,�ɹ�����true,ʧ�ܷ���false */
Bool16 XMLParser::ParseFile(char* errorBuffer, int errorBufferSize)
{
    if (fRootTag != NULL)
    {
        delete fRootTag;    // flush old data
        fRootTag = NULL;
    }

    int fd = -1;
	fd = ::open(fFilePath, O_RDONLY | O_LARGEFILE);

	/* �ļ���С���� */
    if (errorBufferSize < 500) 
		errorBuffer = NULL;  // Just a hack to avoid checking everywhere

	/* ������ */
    if ((fFileLen == 0) || fIsDir)
    {
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "Couldn't read xml file");
        return false;   // we don't have a valid file;
    }
    
	/* ���������ļ��Ļ��� */
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
	/* ���ļ���ͷ���������ļ���ָ���Ļ�����,theLengthRead���Ѷ����ļ����ݳ���,����Ӧ�þ����ļ�����! */ 
	//printf("file descriptor %d, read data %s \n",fd,fileData);

	/* ���濪ʼ�����Ѷ��뻺�������xml�ļ� */
    StrPtrLen theDataPtr(fileData, theLengthRead);
    StringParser theParser(&theDataPtr);
    
	/* �½�root tagʵ�� */
    fRootTag = new XMLTag();
    Bool16 result = fRootTag->ParseTag(&theParser, fVerifier, errorBuffer, errorBufferSize);
    if (!result)
    {
        // got error parsing file
        delete fRootTag;
        fRootTag = NULL;
    }
    
	//�ͷ��ڴ�
    delete fileData;
    
	//�ر�xml�ļ�,���ò���
	::close(fd);

    return result;
}

/* ����root tag */
void XMLParser::SetRootTag(XMLTag* tag)
{
    if (fRootTag != NULL)
        delete fRootTag;
    fRootTag = tag;
}

/* �Կɱ仺�淽ʽ����ָ������,��д��xml�ļ��� */
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
	/* ��ֻд��ʽ��xml�ļ�,��ǰ������ȫ����� */
    fFile = ::fopen(fFilePath, "w");
    if (fFile == NULL)
        return;
     
	/* д��xml�ļ�,���ر��� */
	::fwrite(formatter.GetBufPtr(),formatter.GetBytesWritten(),1,fFile);

    ::fclose(fFile);
	fFile=NULL;
    
	/* �ı��ļ����� */
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
    fTag = temp.GetAsCString();//ȷ��tag��C-string
}

XMLTag::~XMLTag()
{
    if (fTag)
        delete fTag;
    if (fValue)
        delete fValue;      
        
    OSQueueElem* elem;
	//��ɾ��Ƕ�����Զ����е�ÿ������Ԫ
    while ((elem = fAttributes.DeQueue()) != NULL)
    {
        XMLAttribute* attr = (XMLAttribute*)elem->GetEnclosingObject();
        delete attr;
    }       

	//��ɾ��Ƕ��Tag�����е�ÿ������Ԫ
    while ((elem = fEmbeddedTags.DeQueue()) != NULL)
    {
        XMLTag* tag = (XMLTag*)elem->GetEnclosingObject();
        delete tag;
    }
    
    if (fElem.IsMemberOfAnyQueue())
		//��ȡ��ǰ����,�ӵ�ǰ�������Ƴ��ö���Ԫ
        fElem.InQueue()->Remove(&fElem);    // remove from parent tag
}

/* ����xml�ļ��е�ע���� */
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

/* ������Tag��Ƕ��Tag�ĺϷ��� */
bool XMLTag::ParseTag(StringParser* parser, DTDVerifier* verifier, char* errorBuffer, int errorBufferSize)
{
	/* ѭ������,ֱ���ҵ�һ��Tag��,���俪ͷֹͣ */
    while (true)
    {
		/* ��������ö����,�Ҳ���һ��tag��ͷ,����false */
        if (!parser->GetThru(NULL, '<'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Couldn't find a valid tag");
            return false;   // couldn't find beginning of tag
        }
             
		/*��ȡ��ǰ�ַ�*/
		char c = parser->PeekFast();
		/* �������ȿ���������ǩ</>,���ش��� */
        if (c == '/')
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "End tag with no begin tag on line %d", parser->GetCurrentLineNumber());
            return false;   // we shouldn't be seeing a close tag here
        }
         
		/* �ҵ�������ǩ�Ŀ�ͷ */
        if ((c != '!') && (c != '?'))
            break;  // this should be the beginning of a regular tag
        
		/* ���û���ҵ�Tag��,��������ע���� */
        ConsumeIfComment(parser);
        // otherwise this is a processing instruction or a c-data, so look for the next tag
    }
    
	/* ��¼��Tag���к� */
    int tagStartLine = parser->GetCurrentLineNumber();
    
    StrPtrLen temp;
	/* ����ÿ��MaskΪ0���ַ�,��󽫾����Ĳ��ָ���temp */
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
     
	/* ��ȡ��Tag���ַ��� */
    fTag = temp.GetAsCString();
    
	//�����ո�
    parser->ConsumeWhitespace();

    while ((parser->PeekFast() != '>') && (parser->PeekFast() != '/'))
    {
        // we must have an attribute value for this tag
		/* �½�һ��attributeʵ�� */
        XMLAttribute* attr = new XMLAttribute;
		/* ����Ƕ�׵����Զ��� */
        fAttributes.EnQueue(&attr->fElem);

		/* ����ÿ��MaskΪ0���ַ�,��󽫾����Ĳ��ָ���temp */
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

		/* �õ������� */
        attr->fAttrName = temp.GetAsCString();

		//�жϵ�ǰ�ַ�����'=',������,����true;����,����false
        if (!parser->Expect('='))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Missing '=' after attribute %s on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		//�жϵ�ǰ�ַ�����'" ',������,����true;����,����false
        if (!parser->Expect('"'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s value not in quotes on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }
       
		/* ����������ǰ,���������ַ�����temp  */
        parser->ConsumeUntil(&temp, '"');

		/* ��������ֵ */
        attr->fAttrValue = temp.GetAsCString();

		/* ���Ǻ�����,������ */
        if (!parser->Expect('"'))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s value not in quotes on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }
        
		/* �ж��������ĺϷ��� */
        if (verifier && !verifier->IsValidAttributeName(fTag, attr->fAttrName))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Attribute %s not allowed in tag %s on line %d", attr->fAttrName, fTag, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		/* �ж�����ֵ�ĺϷ��� */
        if (verifier && !verifier->IsValidAttributeValue(fTag, attr->fAttrName, attr->fAttrValue))
        {
            if (errorBuffer != NULL)
                qtss_sprintf(errorBuffer, "Bad value for attribute %s on line %d", attr->fAttrName, parser->GetCurrentLineNumber());
            return false;   // bad attribute specification
        }

		//�����ո�
        parser->ConsumeWhitespace();
    }
    
	/* �鿴��ǰ�ַ� '/',�����ƶ� */
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
    
	/* �жϱ�ǩ���Ƿ���'>'?����ֱ���ƶ�,�����¼ */
    if (!parser->Expect('>'))
    {
        if (errorBuffer != NULL)
            qtss_sprintf(errorBuffer, "Bad format for tag <%s> on line %d", fTag, parser->GetCurrentLineNumber());
        return false;   // bad attribute specification
    }
    
    while(true)
    {
		/* ������һ�� '<' */
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
            else/* ��Ƕ��Tag�Ӷ��� */
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
			/* �½�һ��Ƕ��Tag */
            XMLTag* tag = NEW XMLTag();
			/* ����Ƕ��Tag���� */
            fEmbeddedTags.EnQueue(&tag->fElem);
			/* ������Ƕ��Tag */
            if (!tag->ParseTag(parser, verifier, errorBuffer, errorBufferSize))
                return false;
             
			/* ��֤subTag�Ϸ��� */
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
			/* ����ע���� */
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

/* ������������,����ָ��������������ֵ */
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

/*  �ҵ�ָ��index��Tag */
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

/* ����Ƕ��Tag����,����ָ��index��Tag */
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

/* ����Ƕ��Tag����,����ָ��index������ֵ��Tag */
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

/* ����Ƕ��Tag����,����ָ��index,Tag���ƺ�����ֵ��Tag */
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

/* ���ָ��������������ֵ�����Ե����Զ����� */
void XMLTag::AddAttribute( char* attrName, char* attrValue)
{
    XMLAttribute* attr = NEW XMLAttribute;
    StrPtrLen temp(attrName);
    attr->fAttrName = temp.GetAsCString();
    temp.Set(attrValue);
    attr->fAttrValue = temp.GetAsCString();
    
    fAttributes.EnQueue(&attr->fElem);
}

/* ɾ��ָ�������������� */
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

/* ���ָ��Tag��Ƕ��Tag���� */
void XMLTag::AddEmbeddedTag(XMLTag* tag)
{
    fEmbeddedTags.EnQueue(&tag->fElem);
}

/* �Ƴ�ָ��Tag��Ƕ��Tag���� */
void XMLTag::RemoveEmbeddedTag(XMLTag* tag)
{
    fEmbeddedTags.Remove(&tag->fElem);
}

/* ����ǰTag����ָ�������� */
void XMLTag::SetTagName( char* name)
{
    Assert (name != NULL);  // can't have a tag without a name!
    
    if (fTag != NULL)
        delete fTag;
        
    StrPtrLen temp(name);
    fTag = temp.GetAsCString();
}

/* ����Tag����ֵ,��Ƕ��Tag��Ч,ֱ�ӷ��� */
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
  
/* ����ָ����ʽ�Ŀյڶ���������Tag��"\t\t...\t<fTag ����=\""����ֵ\" "...����=\""����ֵ\" ">fValue<\fTag>\r\n" ������ӵ�Ƕ�׽ṹ,�����һ������� */
void XMLTag::FormatData(ResizeableStringFormatter* formatter, UInt32 indent)
{
	/* ���ָ��������'\t' */
    for (UInt32 i=0; i<indent; i++) 
		formatter->PutChar('\t');

    formatter->PutChar('<');
    formatter->Put(fTag);
    if (fAttributes.GetLength() > 0)
    {
        formatter->PutChar(' ');
		/* ѭ�������fTag���������� */
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
    
	/* ����Ϊ��Tag */
    if (fEmbeddedTags.GetLength() == 0)
    {
        if (fValue > 0)
            formatter->Put(fValue);
    }
    else/* ��ΪǶ��Tag */
    {
        formatter->Put(kEOLString);
		/* ����Ƕ��Tag���� */
        for (OSQueueIter iter(&fEmbeddedTags); !iter.IsDone(); iter.Next())
        {
			/* ��ȡ��ǰTag */
            XMLTag* current = (XMLTag*)iter.GetCurrent()->GetEnclosingObject();
			/* ����һ��,�������� */
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
