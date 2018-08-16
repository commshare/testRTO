
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 HTTPRequest.h
Description: A object to receive, parse and respond to client using HTTP protocol.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

#include "HTTPProtocol.h"
#include "StrPtrLen.h"
#include "StringParser.h"
#include "ResizeableStringFormatter.h"
#include "OSHeaders.h"
#include "QTSS.h"

class HTTPRequest
{
public:
    // Constructor
    HTTPRequest(StrPtrLen* serverHeader, StrPtrLen* requestPtr);
    
    // This cosntructor is used when the request has been parsed and thrown away
    // and the response has to be created
    HTTPRequest(StrPtrLen* serverHeader); 
    
    // Destructor
    virtual ~HTTPRequest();
  
    // Should be called before accessing anything in the request header
    // Calls ParseRequestLine and ParseHeaders
    QTSS_Error              Parse();

    // Basic access methods for the HTTP method, the absolute request URI,
    // the host name from URI, the relative request URI, the request file path,
    // the HTTP version, the Status code, the keep-alive tag.
    HTTPMethod              GetMethod(){ return fMethod; }
    StrPtrLen*              GetRequestLine(){ return &fRequestLine; }
    StrPtrLen*              GetRequestAbsoluteURI(){ return &fAbsoluteURI; }
    StrPtrLen*              GetSchemefromAbsoluteURI(){ return &fAbsoluteURIScheme; }
    StrPtrLen*              GetHostfromAbsoluteURI(){ return &fHostHeader; }
    StrPtrLen*              GetRequestRelativeURI(){ return &fRelativeURI; }
    char*                   GetRequestPath(){ return fRequestPath; }
    HTTPVersion             GetVersion(){ return fVersion; }
    HTTPStatusCode          GetStatusCode(){ return fStatusCode; }
    Bool16                  IsRequestKeepAlive(){ return fRequestKeepAlive; }
  
    // If header field exists in the request, it will be found in the dictionary
    // and the value returned. Otherwise, NULL is returned.
    StrPtrLen*              GetHeaderValue(HTTPHeader inHeader);
  
    // Creates a header with the corresponding version and status code
    void                    CreateResponseHeader(HTTPVersion version, HTTPStatusCode statusCode);
  
    // To append response header fields as appropriate
    void                    AppendResponseHeader(HTTPHeader inHeader, StrPtrLen* inValue);
    void                    AppendDateAndExpiresFields();
    void                    AppendDateField();
    void                    AppendConnectionCloseHeader();
    void                    AppendConnectionKeepAliveHeader();
    void                    AppendContentLengthHeader(UInt64 length_64bit);
    void                    AppendContentLengthHeader(UInt32 length_32bit);

    // Returns the completed response header by appending CRLF to the end of the header fields buffer
    StrPtrLen*              GetCompleteResponseHeader();
    
    // Parse if-modified-since header
    time_t                  ParseIfModSinceHeader();
  
private:
    enum { kMinHeaderSizeInBytes = 512 };
  
    // Gets the method, version and calls ParseURI
    QTSS_Error              ParseRequestLine(StringParser* parser);
    // Parses the URI to get absolute and relative URIs, the host name and the file path
    QTSS_Error              ParseURI(StringParser* parser);
    // Parses the headers and adds them into a dictionary
    // Also calls SetKeepAlive with the Connection header field's value if it exists
    QTSS_Error              ParseHeaders(StringParser* parser);
  
    // Sets fRequestKeepAlive
    void                    SetKeepAlive(StrPtrLen* keepAliveValue);
    // Used in initialize and CreateResponseHeader
    void                    PutStatusLine(StringFormatter* putStream, HTTPStatusCode status, HTTPVersion version);
    // For writing into the premade headers
    StrPtrLen*              GetServerHeader(){ return &fSvrHeader; }
  
    // Complete request and response headers
    StrPtrLen                       fRequestHeader;
    ResizeableStringFormatter*      fResponseFormatter;
    StrPtrLen*                      fResponseHeader;
  
    // Private members
    HTTPMethod          fMethod;
    HTTPVersion         fVersion;
	HTTPStatusCode      fStatusCode;
    
    StrPtrLen           fRequestLine;
  
    // For the URI (fAbsoluteURI and fRelativeURI are the same if the URI is of the form "/path")
	// If it is an absolute URI, these fields will be filled in "http://foo.bar.com/path"
	//  => fAbsoluteURIScheme = "http", fHostHeader = "foo.bar.com", fRequestPath = "path"
    StrPtrLen           fAbsoluteURI;       // If it is of the form "http://foo.bar.com/path"
    StrPtrLen           fRelativeURI;       // If it is of the form "/path"                                         
    StrPtrLen           fAbsoluteURIScheme;
    StrPtrLen           fHostHeader;        // If the full url is given in the request line
    char*               fRequestPath;       // Also contains the query string
         
    Bool16              fRequestKeepAlive;              // Keep-alive information in the client request
    StrPtrLen           fFieldValues[httpNumHeaders];   // Array of header field values parsed from the request
    StrPtrLen           fSvrHeader;                     // Server header set up at initialization
    static StrPtrLen    sColonSpace;
    static UInt8        sURLStopConditions[]; 
};

#endif // __HTTPREQUEST_H__
