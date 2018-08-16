/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 StrPtrLen.cpp
Description: Implemention an class which encapsules the char string class with many untilites.
Comment:     copy from Darwin Streaming Server 5.5.5 
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 



#include <ctype.h>
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "OS.h"
#include "OSMemory.h"



/*just change letters of Upper case into letters of Lower case in ASCII form */
UInt8       StrPtrLen::sCaseInsensitiveMask[] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, //0-9 
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //10-19 
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, //20-29
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, //30-39 
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, //40-49
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59, //50-59
    60, 61, 62, 63, 64, 97, 98, 99, 100, 101, //60-69 //stop on every character except a letter
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, //70-79
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, //80-89
    122, 91, 92, 93, 94, 95, 96, 97, 98, 99, //90-99
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, //100-109
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119, //110-119
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129 //120-129
};

/* point out the non print chars in ASCII alphebet with 1 means no print and 0 print  */
UInt8 StrPtrLen::sNonPrintChars[] =
{
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, //0-9     // stop
    0, 1, 1, 0, 1, 1, 1, 1, 1, 1, //10-19    //'\r' & '\n' are not stop conditions
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //20-29
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, //30-39   
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69  
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, //170-179
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //180-189
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //190-199
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //200-209
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //210-219
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //220-229
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //230-239
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //240-249
    1, 1, 1, 1, 1, 1             //250-255
};

/* Get a C-string with zero-terminated char */
char* StrPtrLen::GetAsCString() const
{
    // convert to a "NEW'd" zero terminated char array
    // caler is reponsible for the newly allocated memory
    char *theString = NEW char[Len+1];
    
    if ( Ptr && Len > 0 )
        ::memcpy( theString, Ptr, Len );
    
	/* the last append char is 0  */
    theString[Len] = 0;
    
    return theString;
}

/*************************************************
COMPARE STRING FUNCTIONS
*************************************************/

/* define the operation Equal between StrPtrLen classes */
Bool16 StrPtrLen::Equal(const StrPtrLen &compare) const
{
    if (NULL == compare.Ptr && NULL == Ptr )
        return true;
        
        if ((NULL == compare.Ptr) || (NULL == Ptr))
        return false;

    if ((compare.Len == Len) && (memcmp(compare.Ptr, Ptr, Len) == 0))
        return true;
    else
        return false;
}

/* define the operation Equal between StrPtrLen class and string */
Bool16 StrPtrLen::Equal(const char* compare) const
{   
    if (NULL == compare && NULL == Ptr )
        return true;
        
    if ((NULL == compare) || (NULL == Ptr))
        return false;
        
    if ((::strlen(compare) == Len) && (memcmp(compare, Ptr, Len) == 0))
        return true;
    else
        return false;
}


/* just return a first boolena value by piecewise comparing */
/* compare under the different length condition */
/* ��ͷ��һ�Ƚ�ָ�����ȵĵ�һ������ַ����͸������,ֻҪ��һ���ַ����Ⱦͷ���false,���򷵻�true.��˵����һ������ַ����Ǹ��������Ӵ�(��ͷ����) */
Bool16 StrPtrLen::NumEqualIgnoreCase(const char* compare, const UInt32 len) const
{
    // compare thru the first "len: bytes
    Assert(compare != NULL);
    
    if (len <= Len)
    {
        for (UInt32 x = 0; x < len; x++)
			/* use fetch-component operation of StrPtrLen */
            if (sCaseInsensitiveMask[Ptr[x]] != sCaseInsensitiveMask[compare[x]])
                return false;
        return true;
    }
    return false;
}

/* compare under the same length condition */
Bool16 StrPtrLen::EqualIgnoreCase(const char* compare, const UInt32 len) const
{
    Assert(compare != NULL);
    if (len == Len)
    {
        for (UInt32 x = 0; x < len; x++)
            if (sCaseInsensitiveMask[Ptr[x]] != sCaseInsensitiveMask[compare[x]])
                return false;
        return true;
    }
    return false;
}

/* THE CORE STRPTRLEN FUNCTION */
/* �Ը������ַ���queryCharStr(��һ����0��β),������ҵ����ַ���resultStr(����ΪStrPtrLen),���ִ�Сд,��󷵻�ֵ�ǲ��ҵ����ַ��� */
char *StrPtrLen::FindStringCase(char *queryCharStr, StrPtrLen *resultStr, Bool16 caseSensitive) const
{
    // Be careful about exiting this method from the middle. This routine deletes allocated memory at the end.
    // 

	/* make sure the initial condition of output StrPtrLen class  */
	/* Ϊ�˴洢����ַ���,��������ղ����� */
    if (resultStr)
        resultStr->Set(NULL,0);

	/* make sure that the query string no null */
    Assert (NULL != queryCharStr);
    if (NULL == queryCharStr) return NULL;

	/* make sure that the two member of output StrPtrLen class is in their initial condition */
	/* NOTE WE CAN ACCESS DIRECTLY THE TWO MEMBERS because they are not capsulted in StrPtrLen class */
    if (NULL == Ptr) return NULL;
    if (0 == Len) return NULL;
    
    /* what is queryStr()? */
	/* ������StrPtrLen�Ķ��󲢳�ʼ��,����ĳ�Ա����������Ϊ�� */
    StrPtrLen queryStr(queryCharStr);

	/* the temporary variables used below */
	/* the sign of whether append 0 terminated */
    char *editSource = NULL;/* ���ܵ��½����ַ�ָ�� */
    char *resultChar = NULL;/* �����ҵĽ���ַ��� */

	/* the last char in the source string with length Len */
    char lastSourceChar = Ptr[Len -1];
    
	/* ��������ַ�������ĩ�ַ�����,���ں��油һ���� */
    if (lastSourceChar != 0) //first need to modify for termination,see GetAsCString(). 
    {   editSource = NEW char[Len + 1]; 
	// Ptr could be a static string so make a copy
        ::memcpy( editSource, Ptr, Len );
		/* the next char of the last char of a string is set to 0 */
        editSource[Len] = 0; // this won't work on static strings so we are modifying a new string here
    }

	/* duplicate query string and source string  */
	/* the temporary variables used below */

	/* the first para is query string */
	/* ȷ������ַ���queryCharStr��0��β,�ٽ��丳��queryString */
    char *queryString = queryCharStr;       
    
	/* the StrPtrLen is source string */
    char *sourceString = Ptr;

	/* temporary copy */
	/* ��ת����Сдʱʹ�� */
	char *dupSourceString = NULL;
    char *dupQueryString = NULL;
    UInt32 foundLen = 0;/* �ҵ�������ַ����ĳ��� */
    
	/* if the source ptr is not 0 terminated, we append 0 at the end of string and length increment by 1 */
	/* ���½����沢����������ַ���queryCharStr��0��β��,���丳��sourceString */
    if (editSource != NULL) // a copy of the source ptr and len 0 terminated
        sourceString = editSource; /* a new string with appending 0 terminated */
    
	/* if ignore lower/upper case of char */
    if (!caseSensitive)
		/* duplicate the normal string because we transform the abnormal string into normal string with 0 terminated */
    {   dupSourceString = ::strdup(sourceString);
        dupQueryString = ::strdup(queryCharStr);
		/* if duplicate successfully */
        if (dupSourceString && dupQueryString) 
        {   /* all change into Upper case  */
			sourceString = StrPtrLen(dupSourceString).ToUpper();
            queryString = StrPtrLen(dupQueryString).ToUpper();
			/* result string  */
			/* ���û�����C�ַ�������,��sourceString�в�ѯqueryString,�������ѯ��� */
            resultChar = ::strstr(sourceString,queryString);
			
			::free(dupSourceString);
			::free(dupQueryString);
        }
    }
    else /* if differentiate lower/upper case of char */
    {   resultChar = ::strstr(sourceString,queryString);        
    }
    
    if (resultChar != NULL) // get the start offset
		/* �ҵ��ַ����ĳ��� */
    {   foundLen = resultChar - sourceString;
        resultChar = Ptr + foundLen;  // return a pointer in the source buffer
        if (resultChar > (Ptr + Len)) // make sure it is in the buffer
            resultChar = NULL;
    }
    
	/* release the buffer we have just created */
    if (editSource != NULL)  
        delete [] editSource;
    /* resultStr�ǳ���,ָ��StrPtrLen */
    if (resultStr != NULL && resultChar != NULL)
        resultStr->Set(resultChar,queryStr.Len);
    
#if STRPTRLENTESTING    
    qtss_printf("StrPtrLen::FindStringCase found string=%s\n",resultChar);
#endif

    return resultChar;
}

/*************************************************
 TRIM STRING FUNCTIONS
*************************************************/

/* return the number of consective chars of StrPtrLen not including ''and '\t' */
UInt32 StrPtrLen::RemoveWhitespace()
{
    if (Ptr == NULL || Len == 0)
        return 0;

	/* the pointer just behind the last char */
    char *EndPtr = Ptr + Len; // one past last char
    char *destPtr = Ptr;
    char *srcPtr = Ptr;
    
    Len = 0;
    while (srcPtr < EndPtr)
    {
        
		/* except the two special characters */
        if (*srcPtr != ' ' && *srcPtr != '\t')
        {    
			/* assure both point to the same location */
            if (srcPtr != destPtr)
               *destPtr = *srcPtr;

             destPtr++;
             Len ++;
        }
        srcPtr ++;
    }

    return Len;
}

/* trim the leading '' and '\t' until encounter the char which is not the two ones */
/* note the Ptr and Len all change probably */
UInt32 StrPtrLen::TrimLeadingWhitespace()
{
    if (Ptr == NULL || Len == 0)
        return 0;

	/* just locate after the end of StrPtrLen */
    char *EndPtr = Ptr + Len; //one past last char

    while (Ptr < EndPtr)
    {
        if (*Ptr != ' ' && *Ptr != '\t')
            break;
        
        Ptr += 1;
        Len -= 1; 
    }

    return Len;
}

/* trim the last '' and '\t' until encounter the char which is not the two ones */
/* note the Ptr and Len all change probably and the idea is on the contrary comparing the above TrimLeadingWhitespace() */
/* ���ȶ�λ���һ���ַ���λ��,��������whitespace���˳�,����ͼ����Ӻ���ǰ�ƶ�,ֱ��������whitespace���˳�.
   �˴�����˼��,��˵,ɾȥ�ַ�����ĩ��whitespace,ֱ��non-whitespaceΪֹ */
UInt32 StrPtrLen::TrimTrailingWhitespace()
{
    if (Ptr == NULL || Len == 0)
        return 0;

	/* locate just the last char  */
    char *theCharPtr = Ptr + (Len - 1); // last char

    while (theCharPtr >= Ptr)
    {
        if (*theCharPtr != ' ' && *theCharPtr != '\t')
            break;
        
        theCharPtr -= 1;
        Len -= 1; 
    }
    
    return Len;
}

/*************************************************
 PRINT STRING FUNCTIONS
*************************************************/

void StrPtrLen::PrintStr()
{
	/* first transform C-String by appending 0-terminated */
	/* note thestr is a pointer to a C-String */
    char *thestr = GetAsCString();
    
    UInt32 i = 0;
	/* ��ͷ����,һ����C-String�еķǴ�ӡ�ַ�,�滻Ϊ0,���жϲ��� */
    for (; i < Len; i ++) 
    { 
	   /* set the non print char to 0, note Ptr[i] represents corresponding ASCII char code between 0-255 */
       if (StrPtrLen::sNonPrintChars[Ptr[i]]) 
       {   thestr[i] = 0;
           break;
       }
       
    } 
       
    if (thestr != NULL)
    {   
		/* print the C-String */
        qtss_printf(thestr);
		/* note release the pointer to StrPtrLen */
        delete thestr;
    }   
}

/* print first StrPtrLen then appendStr  */
void StrPtrLen::PrintStr(char *appendStr)
{
    StrPtrLen::PrintStr();
    if (appendStr != NULL)
        qtss_printf(appendStr);
}

/* used in RTSPRequestStream::ReadRequest()/RTSPResponseStream::WriteV() */
/* ����StrPtrLen����ʵ��,�ڵ�һ���Ǵ�ӡ�ַ����ض�,���洦��ʣ�µ��ַ���,���Ƚ�C-String�е�'%'�滻Ϊ'$',
   �����д�ӡ'\n'��'\r'�ַ��ָ���ַ���,��û��'\n'��'\r'�ַ�,ֱ�Ӵ�ӡStrPtrLen����ʵ��
*/
void StrPtrLen::PrintStrEOL(char* stopStr, char *appendStr)
{
           
    /* first transform C-String by appending 0-terminated */
	/* ���Ƚ�StrPtrLen����ʵ��ת��ΪC-String */
    char *thestr = GetAsCString();
    
	/* ��ͷ����,һ����C-String�еķǴ�ӡ�ַ�,�滻Ϊ0,���жϲ���,ע��õ���iֵ */
    SInt32 i = 0;
    for (; i < (SInt32) Len; i ++) 
    {  /* set the non print char to 0 */
       if (StrPtrLen::sNonPrintChars[Ptr[i]]) //�õ������iֵ����Ҫ!!
       {   thestr[i] = 0;
           break;
       }
       
    } 

	/* change the non-0 char '%' into '$' under the following condition */
	/* ��C-String�е�'%'�滻Ϊ'$' */
    for (i = 0; thestr[i] != 0 ; i ++) 
    { 
       if (thestr[i] == '%' && thestr[i+1] != '%' ) 
       {   thestr[i] = '$';
       }       
    } 

    SInt32 stopLen = 0;
    if (stopStr != NULL)
        stopLen = ::strlen(stopStr);

   /* The strstr() function searches for the first occurrence of a string inside another string.
      This function returns the rest of the string (from the matching point), or FALSE, if the 
	  string to search for is not found.Note: This function is case-sensitive. For a case-insensitive 
	  search, use stristr().This function is binary safe.
  */

    /* ��C-String�в��ҵ�һ�����ָ�����ַ���,ȷ��iΪC-String��ָ���ַ�����ĩβ����γ��� */
    if (stopLen > 0 && stopLen <= i)
    {
		/* locate the stop pointer which points to string thestr */
        char* stopPtr = ::strstr(thestr, stopStr);
        if (stopPtr != NULL)
        {  /* shift the pointer to just after the end of string thestr  */
		   stopPtr +=  stopLen;
		   /* change into C-String with 0-terminated */
           *stopPtr = 0;
		   /* calculate the difference of the two pointer */
           i = stopPtr - thestr;
        }
    }

    char * theStrLine = thestr;
    char * nextLine = NULL;
    char * theChar = NULL;
    static char *cr="\\r";
    static char *lf="\\n\n";
    SInt32 tempLen = i;
	/* ���в���'\n'��'\r',��ӡ'\n'��'\r'�ַ��ָ���ַ��� */
    for (i = 0; i < tempLen; i ++) 
    {   
		/* ��λ��'\r',��ȡ��һ�� */
        if (theStrLine[i] == '\r')
        {   theChar = cr;
            theStrLine[i] = 0;
            nextLine = &theStrLine[i+1];
        }
		/* ��λ��'\n',��ȡ��һ�� */
        else if (theStrLine[i] == '\n')
        {   theChar = lf;
            theStrLine[i] = 0;//�ض��ַ���,�õ�һ��C-String
            nextLine = &theStrLine[i+1];
        }
        
        if (nextLine != NULL)
        { 
            qtss_printf(theStrLine);/* ��ӡ��StrPtrLen����ʵ����C-String */
            qtss_printf(theChar);/* ��ӡ'\n'��'\r'�ַ� */
            
            theStrLine = nextLine;/* ���»�ȡ��һ�� */
            nextLine = NULL;  
            tempLen -= (i+1);/* ����Ϊʣ��ĳ��� */
            i = -1;
        }
    }
	/* ���籾StrPtrLen����ʵ����C-String��û��'\n'��'\r'�ַ�,ֱ�Ӱ�����ӡ���� */
    qtss_printf(theStrLine);
	/* ɾ����C-String */
    delete thestr;

	/* ֻ�ǽ��ڶ������ֱ�Ӵ�ӡ */
    if (appendStr != NULL)
        qtss_printf(appendStr);
   
}




#if STRPTRLENTESTING
Bool16  StrPtrLen::Test()
{
    static char* test1 = "2347.;.][';[;]abcdefghijklmnopqrstuvwxyz#%#$$#";
    static char* test2 = "2347.;.][';[;]ABCDEFGHIJKLMNOPQRSTUVWXYZ#%#$$#";
    static char* test3 = "Content-Type:";
    static char* test4 = "cONTent-TYPe:";
    static char* test5 = "cONTnnt-TYPe:";
    static char* test6 = "cONTent-TY";
    
    static char* test7 = "ontent-Type:";
    static char* test8 = "ONTent-TYPe:";
    static char* test9 = "-TYPe:";
    static char* test10 = ":";
    
    StrPtrLen theVictim1(test1, strlen(test1));
    if (!theVictim1.EqualIgnoreCase(test2, strlen(test2)))
        return false;
        
    if (theVictim1.EqualIgnoreCase(test3, strlen(test3)))
        return false;
    if (!theVictim1.EqualIgnoreCase(test1, strlen(test1)))
        return false;

    StrPtrLen theVictim2(test3, strlen(test3));
    if (!theVictim2.EqualIgnoreCase(test4, strlen(test4)))
        return false;
    if (theVictim2.EqualIgnoreCase(test5, strlen(test5)))
        return false;
    if (theVictim2.EqualIgnoreCase(test6, strlen(test6)))
        return false;
        
    StrPtrLen outResultStr;
    if (!theVictim1.FindStringIgnoreCase(test2, &outResultStr))
        return false;
    if (theVictim1.FindStringIgnoreCase(test3, &outResultStr))
        return false;
    if (!theVictim1.FindStringIgnoreCase(test1, &outResultStr))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test4))
        return false;
    if (theVictim2.FindStringIgnoreCase(test5))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test6))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test7))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test8))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test9))
        return false;
    if (!theVictim2.FindStringIgnoreCase(test10))
        return false;

    if (theVictim1.FindString(test2, &outResultStr))
        return false;
    if (theVictim1.FindString(test3, &outResultStr))
        return false;
    if (!theVictim1.FindString(test1, &outResultStr))
        return false;
    if (theVictim2.FindString(test4))
        return false;
    if (theVictim2.FindString(test5))
        return false;
    if (theVictim2.FindString(test6))
        return false;
    if (!theVictim2.FindString(test7))
        return false;
    if (theVictim2.FindString(test8))
        return false;
    if (theVictim2.FindString(test9))
        return false;
    if (!theVictim2.FindString(test10))
        return false;
    
    StrPtrLen query;
    query.Set(test2);
    if (theVictim1.FindString(query, &outResultStr))
        return false;
    if (outResultStr.Len > 0)
        return false;
    if (outResultStr.Ptr != NULL)
        return false;
        
    query.Set(test3);
    if (theVictim1.FindString(query, &outResultStr))
        return false;
    if (outResultStr.Len > 0)
        return false;
    if (outResultStr.Ptr != NULL)
        return false;
        
    query.Set(test1);
    if (!theVictim1.FindString(query, &outResultStr))
        return false;
    if (!outResultStr.Equal(query))
        return false;
        
    query.Set(test4);
    if (query.Equal(theVictim2.FindString(query)))
        return false;
    
    query.Set(test5);
    if (query.Equal(theVictim2.FindString(query)))
        return false;

    query.Set(test6);
    if (query.Equal(theVictim2.FindString(query)))
        return false;

    query.Set(test7);
    if (!query.Equal(theVictim2.FindString(query)))
        return false;

    query.Set(test8);
    if (query.Equal(theVictim2.FindString(query)))
        return false;

    query.Set(test9);
    if (query.Equal(theVictim2.FindString(query)))
        return false;

    query.Set(test10);
    if (!query.Equal(theVictim2.FindString(query)))
        return false;
    
    query.Set(test10);
    if (!query.Equal(theVictim2.FindString(query)))
        return false;
    
    StrPtrLen partialStaticSource(test1,5);
    query.Set("abcd");
    if (query.Equal(partialStaticSource.FindString(query)))
        return false;
        
    query.Set("47");
    if (query.Equal(partialStaticSource.FindString(query))) // success = !equal because the char str is longer than len
        return false;
    
    if (query.FindString(partialStaticSource.FindString(query))) // success = !found because the 0 term src is not in query
        return false;

    partialStaticSource.FindString(query,&outResultStr);
    if (!outResultStr.Equal(query)) // success =found the result Ptr and Len is the same as the query
        return false;

    return true;
}
#endif
