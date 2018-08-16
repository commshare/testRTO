
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 SDPUtils.cpp
Description: define some static routines for dealing with SDPs.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include "OS.h"
#include "OSMemory.h"
#include "SDPUtils.h"
#include "StrPtrLen.h"
#include "StringParser.h"
#include "ResizeableStringFormatter.h"



/* add the given line to the next line of the current one and return this line number */
SInt32 SDPContainer::AddHeaderLine (StrPtrLen *theLinePtr)
{   
	/* assure in-para no null */
    Assert(theLinePtr);
	/* record number of actual, used lines which is the location of the newly-added line */
	/* �趨��ǰ���к�# */
    UInt32 thisLine = fNumUsedLines;
	/* assure the number of used line no bigger than total line numbers */
    Assert(fNumUsedLines < fNumSDPLines);
	/* set paras of the (thisLine+1)-th line */
	/* ȡfSDPLineArray�е�ָ���ַ�,�ü̳���StrPtrLen��Set()���趨��� */
	/* ע����п�ͷ����һ����־λ(SDPLine�ĳ�Ա),�������ü����� */
    fSDPLineArray[thisLine].Set(theLinePtr->Ptr, theLinePtr->Len);
	/* ʵ���õ���������1 */
    fNumUsedLines++;

	/* if increased line reach the border of array, then enlarge the size of original array to double  */
	/* �˴����⴦��߽���� */
    if (fNumUsedLines == fNumSDPLines)
    {
		/* �ӵ�һ�к���ĩ�п��ƶϳ�fNumUsedLines����ʵ��ʹ�õ�sdp���� */
        SDPLine   *tempSDPLineArray = NEW SDPLine[fNumSDPLines * 2];
		/* copy original fSDPLineArray[fNumSDPLines] to newly build tempSDPLineArray then delete the original one  */
        for (int i = 0; i < fNumSDPLines; i++)
        {
            tempSDPLineArray[i].Set(fSDPLineArray[i].Ptr,fSDPLineArray[i].Len);
            tempSDPLineArray[i].fHeaderType = fSDPLineArray[i].fHeaderType;
        }
        delete [] fSDPLineArray;
		/* revise back the original array and name */
        fSDPLineArray = tempSDPLineArray;
        fNumSDPLines = (fNumUsedLines * 2);
    }
    
	/* set header type of thisLine line */
	/* ����εڵ�һ���ַ�������е�������ĳ�Ա����fHeaderType(���ĳ�ʼֵ��'\0') */
	/* ����ͳһ��ε�һ���ַ��͸���������ĳ�Ա����fHeaderType */
    if (theLinePtr->Ptr)
        fSDPLineArray[thisLine].fHeaderType = theLinePtr->Ptr[0];
    
	/* return line number */
	/* ���ص�ǰ���к� */
    return thisLine;
}

/* find out which line  the sdp line with the given id when fHeaderType locates and return the line index */
/* �ӱ��Ϊstart���п�ʼѰ��ָ��id����,�ҵ��󷵻ظ��е��к� */
SInt32 SDPContainer::FindHeaderLineType(char id, SInt32 start)
{   
    SInt32 theIndex = -1;
    
	/* assure the start has a rational range  */
    if (start >= fNumUsedLines || start < 0)
        return -1;
        
    for (int i = start; i < fNumUsedLines; i++)
		/* ���ҵ�ָ���е�ͷ���fHeaderType,�ٺ����id�Ƚ� */
		/* compare given id and fHeaderType */
    {   if (fSDPLineArray[i].fHeaderType == id)
        {   theIndex = i;
	        /* current line has the given id */
            fCurrentLine = theIndex;
            break;
        }
    }
    
    return theIndex;
}

/* return pointer to fHeaderType of the next line of current sdp line */
SDPLine* SDPContainer::GetNextLine()
{
    if (fCurrentLine < fNumUsedLines)
    {   fCurrentLine ++;
        return &fSDPLineArray[fCurrentLine];
    }
    
    return NULL;

}

/* return pointer to fHeaderType of the sdp line with the given line index */
SDPLine* SDPContainer::GetLine(SInt32 lineIndex)
{
    /* the lineIndex has range from 0 to fNumUsedLines-1 */
    if (lineIndex > -1 && lineIndex < fNumUsedLines)
    {   return &fSDPLineArray[lineIndex];
    }

    return NULL;
}

/* set the current line to the line with given index */
/* �趨��ǰ�е��к� */
void SDPContainer::SetLine(SInt32 index)
{
	/* the lineIndex has range from 0 to fNumUsedLines-1 */
    if (index > -1 && index < fNumUsedLines)
    {   fCurrentLine = index;
    }
    else
        Assert(0);
        
}

/* FUNDAMENTAL FUNCTION and always invoke the functions in StringParser.h  */
/* parse sdp line in buffer and add it to the next line of the current one */
/* and mark the valid status of this line */
void SDPContainer::Parse()
{
	/* string array consist of the line type char of each line */
	char*	    validChars = "vosiuepcbtrzkam";
	/* separater between name and concrete value */
	char        nameValueSeparator = '=';
	
	/* all kinds of  validity check */
	// check line if begin with one of the valid characters followed by an "="
	// check if line has whitespace after the "="
	// check if fNumUsedLines==0?...
	Bool16      valid = true;

	/* initialize and def object of StringParser */
	/* IMPORTANT CLASS TO USE LATER */

	/* sdpParser��������Ǹ��ǳ��ص��Ĵʣ�����һ���õ���StrPtrLen��StringParser�������ײ���� */
	/* �����½�һ��StringParser����������fSDPBuffer�е����� */
	StringParser	sdpParser(&fSDPBuffer);//see StringParser.h,here fSDPBuffer is StrPtrLen��ŵ���SDP����

	/* use to describe a sdp line to parse */
	/* ��������������ڽ���һ����ͨ��sdp��ʱҪ�õ� */
	/* ����sdp�е�һ����ͨ�� */
	StrPtrLen		line; /* ������ÿ�е�һ���ַ�,����Ƿ���к�fReqLines,�Ϸ��Լ���������sdp�е���ǰ�е���һ�� */
	StrPtrLen 		fieldName;/* ר�Ŵ�������,�����Ƿ��ԺϷ��ַ�("vosiuepcbtrzkam"֮һ)���'='��ͷ */
	StrPtrLen		space;/* ר�Ŵ���ո�,����'='�Ժ��sdp��,�Ƿ�'='���пո� */
	
	/* parse the whole line,fieldName and whitespace of sdp line */
	/* �õ�stream��fSDPbuffer�е���βָ�������� */
	/* ���ѭ�����н���sdp���ݵĺϷ���,���Ϸ������ݼӵ���ǰ�е���һ��,�������Ϸ������ݾ��ж�ѭ�� */
	while ( sdpParser.GetDataRemaining() != 0 )// see StringParser.h
	{
		/* fStartGet continue to move until eol, and this part string as a substitute of line */
		/* then fStartGet continue to walk over eol */
		/* ��һ�е�������ʹfSDPBuffer�е�sdp���ݷָ��һ��һ��,����line����sdp���ݵ�һ��,ĩβû��eol  */
		sdpParser.GetThruEOL(&line);  // Read each line  

        /* initialize and def  another object of StringParser */
		/* sdp�����е���ͨһ��,�������ط����� */
        StringParser lineParser(&line);//�˴��صؽ�����һ�У�ע����һ�ж�������ֵ�ѱ仯,ĩβû��eol

		/* why? analysis the new sdp line and apply the fStartGet and fEndGet pointer of this new line not former pointers */
		/* ���յ��ո�,ע��ȽϿո��'\0'������,ǰ�߷�Χ����,��StringParser.cpp */
		/* �����ո�whitespace�ַ�fStartGet��ǰ��ֱ������non-whitespace��ͣ�� */
		/* ����line��ͷû���˿ո� */
        lineParser.ConsumeWhitespace();//skip over leading whitespace

		/***********************************************************/
		//���������GetThruEOL��ConsumeWhitespaceʵ���˶�һ��SDP����ͷȥβ������

		/* judge the valid length */
		/* ������յ��ո���ǿ���,������,��whileѭ�������¿�ʼ */
        if (lineParser.GetDataRemaining() == 0) // must be an empty line
            continue;

		/* return the current char pointed to by the fStartGet */
		/* ȡ��line�еĵ�һ���ַ�(��ʶλ,fStartGet��ǰָ���) */
		/* �����������fStartGet��ǰָ����ַ�����'\0',ע��fStartGetλ�ò��� */
        char firstChar = lineParser.PeekFast();
		/* means a blank line */
		/* ��û�б�SDPContainer::AddHeaderLine()���� */
        if (firstChar == '\0')
            continue; //skip over blank lines
     
		/* give the first char in this line to firstChar-th component in fieldname array whose size is 256  */
		/* store the firstChar into the corresponding position in ASCII form */
		/* ��line�ı�ʶ��firstChar������ASCII���е��Ⱥ�λ�ðڷ����ַ�����fFieldStr��,��������С��256 */
		/* ��Ȼ����ַ��������кܶ�ո��϶��,��Ϊ����256��λ�ò��������� */
		/* ��Ȼ��ʱfirstChar����'\0' */
        fFieldStr[firstChar] = firstChar;
		/* ����������,�����������ĸ���ʶ��ʱ�����Ա����fReqLines��ֵ */
        switch (firstChar)
        {
            case 'v': fReqLines |= kV;
            break;
    
            case 's': fReqLines |= kS ;
            break;
    
            case 't': fReqLines |= kT ;
            break;
    
            case 'o': fReqLines |= kO ;
            break;
        
        }

		/* fStartGet go pass the first char and come to the second char '=' */
		/* ���ڿ�ʼ��������,fieldName�Ѿ����sdp��line�еĵ�һ���ַ�(����) */
		lineParser.ConsumeUntil(&fieldName, nameValueSeparator);

     /* The strchr() function searches for the first occurrence of a string inside another string.
		This function returns the rest of the string (from the matching point), or FALSE, if the 
		string to search for is not found.This function is an alias of the strstr() function.
        This function is case-sensitive. For a case-insensitive search, use stristr().
     */
        /* field name char must in valid char and its length is 1 */
		/* validChars = "vosiuepcbtrzkam"  */
		if ((fieldName.Len != 1) || (::strchr(validChars, fieldName.Ptr[0]) == NULL))
		{
			valid = false; // line doesn't begin with one of the valid characters followed by an "="
			break;
		}
		
		/* the '=' not exist  */
		/* note that the fStartGet has come to the current location nameValueSeparator */
		/* ע��fStartGet��ָ��"=",�����ƹ�"="������true,����������ǰû��ָ��"="�ͷ���false */
		if (!lineParser.Expect(nameValueSeparator))
		{
			valid = false; // line doesn't have the "=" after the first char
			break;
		}
		
		/* ʱ��ע��fStartGet��λ��,�����������Ҫ,��ϵ���������� */
		/* ע��ָ��fStartGet�����Ѿ��ƹ���'='�����������ƶ� */
		/* �������ַ�����Ϊָ����ֹ��ǰ���ַ��������������κ�ֵ */
        /* ���ڽ������涨��ĵ��������� */
		/* judge whether there exist whitespace in this sdp line */
		/* spaceӦ���ǿյ�,��ΪsWhitespaceMask�Ѿ���������  */
		/* ע��sWhitespaceMask��ConsumeUntil�Ķ���,fStartGet�ᾭ��sWhitespaceMask,ֱ��������sWhitespaceMask,��ͣ��  */
		/* �Ƚ������lineParser.ConsumeWhitespace() */
		lineParser.ConsumeUntil(&space, StringParser::sWhitespaceMask);
		
		/* note fStartGet always move forward and no step backward */
		/* �϶�"="���пո�,���ж� */
		if (space.Len != 0)
		{
			valid = false; // line has whitespace after the "=" 
			break;
		}
		/* add the parsed line to the next line of the current one and return this next line number# */
		AddHeaderLine(&line);
	}
	
	if (fNumUsedLines == 0) // didn't add any lines
	{   valid = false;
	}
	/* update valid member variable */
	fValid = valid;
	
}

/* initialize member function */
void SDPContainer::Initialize()
{
    fCurrentLine = 0;
    fNumUsedLines = 0;
	/* ɾȥ������,�Ա��ؽ��µ����� */
    delete [] fSDPLineArray;
	/* each sdp line is C-char with a header type '\0' */
	/* ÿ��sdpLine��'\0',��������������"\0\0\0...\0" */
    fSDPLineArray = NEW SDPLine[fNumSDPLines]; 
    fValid = false;
    fReqLines = 0;
	/* string array consists of each line field name */
	/* ��ʼ��ÿ��sdp��Ϊ256��0 */
    ::memset(fFieldStr, sizeof(fFieldStr), 0);
}

/* IMPORTANT FUNCTION */
/* input the given string, store in buffer, then parse its validity and add it to the next sdp line to current line */
Bool16 SDPContainer::SetSDPBuffer(char *sdpBuffer) 
{ 
    /* first initialize and clear buffer */
    Initialize();
	/* set member fSDPBuffer value for non-null input-parameter */
    if (sdpBuffer != NULL)
		/* use StrPtrLen.Set() */
    {   fSDPBuffer.Set(sdpBuffer); 
       /* parse the validity of the above fSDPBuffer, then  add it to the next line to the current one  */
	   /* ����ѭ�����н���sdp���ݵĺϷ���,���Ϸ������ݼӵ���ǰ�е���һ��,�������Ϸ������ݾ��ж�ѭ�� */
	   /* ���Ҫ�趨fValid */
        Parse(); 
    }
    /*check validity of returned fValid value, def in SDPUtils.h */
    return IsSDPBufferValid();
}

Bool16 SDPContainer::SetSDPBuffer(StrPtrLen *sdpBufferPtr)
{ 
    Initialize();
    if (sdpBufferPtr != NULL)
    {   fSDPBuffer.Set(sdpBufferPtr->Ptr, sdpBufferPtr->Len); 
        Parse(); 
    }
    
    return IsSDPBufferValid();
}


void  SDPContainer::PrintLine(SInt32 lineIndex)
{
    StrPtrLen *printLinePtr = GetLine(lineIndex);
    if (printLinePtr)
    {   printLinePtr->PrintStr();
        qtss_printf("\n");
    }

}

void  SDPContainer::PrintAllLines()
{
    if (fNumUsedLines > 0)
    {   for (int i = 0; i < fNumUsedLines; i++)
            PrintLine(i);
    }
    else
        qtss_printf("SDPContainer::PrintAllLines no lines\n"); 
}


/* def and initialize objects */
char SDPLineSorter::sSessionOrderedLines[] = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
char SDPLineSorter::sessionSingleLines[]  = "vosiuepcbzk";    // return only 1 of each of these session field types
StrPtrLen  SDPLineSorter::sEOL("\r\n");
StrPtrLen  SDPLineSorter::sMaxBandwidthTag("b=AS:"); //AS=Application Specific Maximum

/* the non-default constructor function */
/* ��ʼ������������Ա,����SDP���ݽ������� */
SDPLineSorter::SDPLineSorter(SDPContainer *rawSDPContainerPtr, Float32 adjustMediaBandwidthPercent) : fSessionLineCount(0),fSDPSessionHeaders(NULL,0), fSDPMediaHeaders(NULL,0)
/* �����ļ�����Ա��ʼ������:fMediaHeaders, fSessionSDPContainer,fSDPSessionHeaders,fSessionHeaders*/
{

	/* make sure the pointer no null */
	Assert(rawSDPContainerPtr != NULL);
	if (NULL == rawSDPContainerPtr) 
		return;
	
	/* define two objects of class StrPtrLen  */
	/* ��buffer�е�sdp���ݳ�ʼ�� */
	StrPtrLen theSDPData(rawSDPContainerPtr->fSDPBuffer.Ptr,rawSDPContainerPtr->fSDPBuffer.Len);
	
	/* �ӵ�0�п�ʼ,�ҵ���m��ͷ����һ��,���ظ����к�,��ȡ��m��ͷ����һ�� */
	StrPtrLen *theMediaStart = rawSDPContainerPtr->GetLine(rawSDPContainerPtr->FindHeaderLineType('m',0));
	/* ��m��ͷ�����Ժ��ҵ����ܵ�b��ͷ����,������MediaBandwidth,����ʼ����Ա����fMediaHeaders */
 	if (theMediaStart && theMediaStart->Ptr && theSDPData.Ptr)
	{
		/* ��m��ͷ��һ��(����)��ʼ��, ����SDP���ݵ����һ(��)�� */
		UInt32  mediaLen = theSDPData.Len - (UInt32) (theMediaStart->Ptr - theSDPData.Ptr);
		char *mediaStartPtr= theMediaStart->Ptr;
		/********************** ��ʼ����Ա����fMediaHeaders****************/
		fMediaHeaders.Set(mediaStartPtr,mediaLen);
		/* ���岢��ʼ������sdpParser */
        StringParser sdpParser(&fMediaHeaders);
		/* ������ͨ��һ�� */
        StrPtrLen sdpLine;
        while (sdpParser.GetDataRemaining() > 0)
        {
            sdpParser.GetThruEOL(&sdpLine);
            
            if ( ( 'b' == sdpLine.Ptr[0]) && (1.0 != adjustMediaBandwidthPercent) )
            {   
				/* ���½�һ������ */
                StringParser bLineParser(&sdpLine);
				/* �н���������ǰ */
                bLineParser.ConsumeUntilDigit();
				/* ȡ�����е����ֳ����adjustMediaBandwidthPercent(50%) */
                UInt32 bandwidth = (UInt32) (.5 + (adjustMediaBandwidthPercent * (Float32) bLineParser.ConsumeInteger() ) );
                if (bandwidth < 1) 
                    bandwidth = 1;
                
				/* �õ�������ʽ���ַ���bandwidthStr,�����һ���ַ�Ϊ0 */
                char bandwidthStr[10];
                qtss_snprintf(bandwidthStr,sizeof(bandwidthStr) -1, "%lu", bandwidth);
                bandwidthStr[sizeof(bandwidthStr) -1] = 0;
                
				/* �����Ϊ"b=AS:xxxxxx0"���ַ��� */
                fSDPMediaHeaders.Put(sMaxBandwidthTag);
                fSDPMediaHeaders.Put(bandwidthStr);
            }
            else
				/* �������������һ��,��\r\n��β */
                fSDPMediaHeaders.Put(sdpLine);

            fSDPMediaHeaders.Put(SDPLineSorter::sEOL);
        } 
		/************************** ��fSDPMediaHeaders��һ�г�ʼ����һ��Ա����fMediaHeaders ***********/
		/*****�ڶ��γ�ʼ����Ա����fMediaHeaders,�ҳ���m��ͷ����,�Ժ���п��ܰ���b��ͷ����*****/
        fMediaHeaders.Set(fSDPMediaHeaders.GetBufPtr(),fSDPMediaHeaders.GetBytesWritten());
    }

	/* �ӱ��Ϊ0���п�ʼѰ��m��ͷ����,�ҵ��󷵻ظ��е��к� */
	fSessionLineCount = rawSDPContainerPtr->FindHeaderLineType('m',0);
	/* �����ҵ����еı��Ϊ��ֵ,�͸���Ϊʵ���õ������� */
	if (fSessionLineCount < 0) // didn't find it use the whole buffer
	{   fSessionLineCount = rawSDPContainerPtr->GetNumLines();
	}

	/* �õ�ָ�������ŵ��мӵ���ǰ�е���һ��,ֱ���ҵ��кŵ�����Ϊֹ */
	/* ע�����ҵ��кŵ�m�������������,������������ǵ�0,1,2,..�е� */
	for (SInt16 sessionLineIndex = 0; sessionLineIndex < fSessionLineCount; sessionLineIndex++)
		fSessionSDPContainer.AddHeaderLine( (StrPtrLen *) rawSDPContainerPtr->GetLine(sessionLineIndex));

	//qtss_printf("\nSession raw Lines:\n"); fSessionSDPContainer.PrintAllLines();

	/* sSessionOrderedLines[]= "vosiuepcbtrzka";  */
	SInt16 numHeaderTypes = sizeof(SDPLineSorter::sSessionOrderedLines) -1;//13

	/* �����ѭ����sSessionOrderedLines[]= "vosiuepcbtrzka"Ϊ�̶�˳��,������ͬ����ĸ��ͷ���з���һ��,�γ�fSDPSessionHeaders */
	for (SInt16 fieldTypeIndex = 0; fieldTypeIndex < numHeaderTypes; fieldTypeIndex ++)
	{
		/* �ӵ�0�п�ʼ����ָ���ַ�˳����е��к� */
		SInt32 lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], 0);
		/* ȡ������ */
		StrPtrLen *theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);
		/* �����ҳ���ָ����ĸ��ͷ���в�����fSDPSessionHeaders�в���'\r\n'��β,ֱ���Ҿ��������� */
		while (theHeaderLinePtr != NULL)
		{
			fSDPSessionHeaders.Put(*theHeaderLinePtr);
			fSDPSessionHeaders.Put(SDPLineSorter::sEOL);

			/* ����ָ����ĸ��ͷ����һ�г�����ֻ�ܳ���һ�ε���sessionSingleLines[]= "vosiuepcbzk"��,�������˳� */
			if (NULL != ::strchr(sessionSingleLines, theHeaderLinePtr->Ptr[0] ) ) // allow 1 of this type: use first found
				break; // move on to next line type

			/* ���ҵ����кź��ٴ�Ѱ���Ƿ�Ҫ��ָ����ĸ��ͷ����,�ҵ�����������к�(���ϴε��кŲ�ͬ) */
			lineIndex = fSessionSDPContainer.FindHeaderLineType(SDPLineSorter::sSessionOrderedLines[fieldTypeIndex], lineIndex + 1);
			/* ����ȡ������,���������ѭ�� */
			theHeaderLinePtr = fSessionSDPContainer.GetLine(lineIndex);
		}
	}
	/* ��fSDPSessionHeaders���Ľ������ΪfSessionHeaders */
	fSessionHeaders.Set(fSDPSessionHeaders.GetBufPtr(),fSDPSessionHeaders.GetBytesWritten());

}

/* �õ�SortedSDP��һ��copy,��fSessionHeaders��fMediaHeaders�������θ��Ƶõ�,�����ڴ�ָ�� */
char* SDPLineSorter::GetSortedSDPCopy()
{
	char* fullbuffCopy = NEW char[fSessionHeaders.Len + fMediaHeaders.Len + 2];
	SInt32 buffPos = 0;
	memcpy(&fullbuffCopy[buffPos], fSessionHeaders.Ptr,fSessionHeaders.Len);
	buffPos += fSessionHeaders.Len;
	memcpy(&fullbuffCopy[buffPos], fMediaHeaders.Ptr,fMediaHeaders.Len);
	buffPos += fMediaHeaders.Len;
	fullbuffCopy[buffPos] = 0;	
	
	return fullbuffCopy;
}


