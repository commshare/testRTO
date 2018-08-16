/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 StrPtrLen.h
Description: Implemention an class which encapsules the char string class with many untilites.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#ifndef __STRPTRLEN_H__
#define __STRPTRLEN_H__

#include <string.h>
#include <ctype.h> 
#include "OSHeaders.h"
#include "MyAssert.h"
#include "SafeStdLib.h"

#define STRPTRLENTESTING 0

class StrPtrLen
{
    public:

        //CONSTRUCTORS/DESTRUCTOR
        //These are so tiny they can all be inlined
		/* sp-string pointer */
        StrPtrLen() : Ptr(NULL), Len(0) {}
        StrPtrLen(char* sp) : Ptr(sp), Len(sp != NULL ? strlen(sp) : 0) {}
        StrPtrLen(char *sp, UInt32 len) : Ptr(sp), Len(len) {}
        virtual ~StrPtrLen() {}
        
        //OPERATORS:
        Bool16 Equal(const StrPtrLen &compare) const;
        Bool16 EqualIgnoreCase(const char* compare, const UInt32 len) const;
        Bool16 EqualIgnoreCase(const StrPtrLen &compare) const { return EqualIgnoreCase(compare.Ptr, compare.Len); }
        Bool16 Equal(const char* compare) const;
        Bool16 NumEqualIgnoreCase(const char* compare, const UInt32 len) const;
        
		/* delete String and release memory */
        void Delete() { delete [] Ptr; Ptr = NULL; Len = 0; }
		/* transform a string to Upper case */
        char *ToUpper() { for (UInt32 x = 0; x < Len ; x++) Ptr[x] = toupper (Ptr[x]); return Ptr;}
        
		/* find the designated string queryCharStr and output a new string resultStr according to Lower/Upper case */
		/* THE IMPORTANT AND CITED FREQUENTLY FUNCTION, definded in strptrlen.cpp */
        char *FindStringCase(char *queryCharStr, StrPtrLen *resultStr, Bool16 caseSensitive) const;

		/* find the designated string with differentiating Lower/Upper case */
		/* first assert the pointers are not null and the delimiter is 0 */
        char *FindString(StrPtrLen *queryStr, StrPtrLen *outResultStr)              {   Assert(queryStr != NULL);   Assert(queryStr->Ptr != NULL); Assert(0 == queryStr->Ptr[queryStr->Len]);
                                                                                        return FindStringCase(queryStr->Ptr, outResultStr,true);    
                                                                                    }
        
		/* find the designated string with ignoring Lower/Upper case */
		/* used in DoDescribe() in QTSSFileModule.cpp */
        char *FindStringIgnoreCase(StrPtrLen *queryStr, StrPtrLen *outResultStr)    {   Assert(queryStr != NULL);   Assert(queryStr->Ptr != NULL); Assert(0 == queryStr->Ptr[queryStr->Len]); 
                                                                                        return FindStringCase(queryStr->Ptr, outResultStr,false); 
                                                                                    }

		/* locate but not output the designated string with differentiating Lower/Upper case */
		/* used in DoDescribe() in QTSSFileModule.cpp,RTSPRequest::ParseURI() */
        char *FindString(StrPtrLen *queryStr)                                       {   Assert(queryStr != NULL);   Assert(queryStr->Ptr != NULL); Assert(0 == queryStr->Ptr[queryStr->Len]); 
                                                                                        return FindStringCase(queryStr->Ptr, NULL,true);    
                                                                                    }
        
		/* locate but not output the designated string with differentiating Lower/Upper case */
        char *FindStringIgnoreCase(StrPtrLen *queryStr)                             {   Assert(queryStr != NULL);   Assert(queryStr->Ptr != NULL); Assert(0 == queryStr->Ptr[queryStr->Len]); 
                                                                                        return FindStringCase(queryStr->Ptr, NULL,false); 
                                                                                    }
        /* cite the fundamental function FindStringCase() */                                                                          
        char *FindString(char *queryCharStr)                                        { return FindStringCase(queryCharStr, NULL,true);   }
        char *FindStringIgnoreCase(char *queryCharStr)                              { return FindStringCase(queryCharStr, NULL,false);  }
        char *FindString(char *queryCharStr, StrPtrLen *outResultStr)               { return FindStringCase(queryCharStr, outResultStr,true);   }
        char *FindStringIgnoreCase(char *queryCharStr, StrPtrLen *outResultStr)     { return FindStringCase(queryCharStr, outResultStr,false);  }

		/* the input parameters is StrPtrLen type not pointer */
        char *FindString(StrPtrLen &query, StrPtrLen *outResultStr)                 { return FindString( &query, outResultStr);             }
        char *FindStringIgnoreCase(StrPtrLen &query, StrPtrLen *outResultStr)       { return FindStringIgnoreCase( &query, outResultStr);   }
        char *FindString(StrPtrLen &query)                                          { return FindString( &query);           }
        char *FindStringIgnoreCase(StrPtrLen &query)                                { return FindStringIgnoreCase( &query); }
        
		/* define set-value operation = */
        StrPtrLen& operator=(const StrPtrLen& newStr) { Ptr = newStr.Ptr; Len = newStr.Len;
                                                        return *this; }

		/* define acquire-component operation [] */
		/* this no necessary for a string */
         char operator[](int i) { /*Assert(i<Len);i*/ return Ptr[i]; }

	    /* define two set value functions  */
        void Set(char* inPtr, UInt32 inLen) { Ptr = inPtr; Len = inLen; }
        void Set(char* inPtr) { Ptr = inPtr; Len = (inPtr) ?  ::strlen(inPtr) : 0; }

        //This is a non-encapsulating interface. The class allows you to access its
        //data. 
		/* the Class StrPtrLen HAS ONLY TWO MEMBER */
        char*       Ptr;
        UInt32      Len;

        // convert to a "NEW'd" zero terminated char array
        char*   GetAsCString() const;

        void    PrintStr();
        void    PrintStr(char *appendStr);
		/* used in RTSPResponseStream::WriteV() */
        void    PrintStrEOL(char* stopStr = NULL, char *appendStr = NULL);
 
        //Utility function
        UInt32    TrimTrailingWhitespace();
        UInt32    TrimLeadingWhitespace();
   
        UInt32  RemoveWhitespace();
        void  TrimWhitespace() { TrimLeadingWhitespace(); TrimTrailingWhitespace(); }

#if STRPTRLENTESTING
        static Bool16   Test();
#endif

    private:
        /* definition see StrPtrLen.cpp */

		/*just change letters of Upper case into letters of Lower case in ASCII form */
        static UInt8    sCaseInsensitiveMask[];

		/* point out the non print chars in ASCII alphebet with 1 means no print and 0 print  */
        static UInt8    sNonPrintChars[];
};


/* 这个类相较于StrPtrLen,只是增加了析构函数中的Delete() */
class StrPtrLenDel : public StrPtrLen
{
  public:
     StrPtrLenDel() : StrPtrLen() {}
     StrPtrLenDel(char* sp) : StrPtrLen(sp) {}
     StrPtrLenDel(char *sp, UInt32 len) : StrPtrLen(sp,len) {}
     ~StrPtrLenDel() { Delete(); }
};

#endif // __STRPTRLEN_H__
