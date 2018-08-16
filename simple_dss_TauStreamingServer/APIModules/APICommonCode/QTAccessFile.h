
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 QTAccessFile.h
Description: This object contains an interface for finding and parsing qtaccess files.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _QT_ACCESS_FILE_H_
#define _QT_ACCESS_FILE_H_

#include <stdlib.h>
#include "SafeStdLib.h"
#include "StrPtrLen.h"
#include "OSHeaders.h"
#include "OSMutex.h"
#include "QTSS.h"

class OSMutex;

class QTAccessFile
{
    public:

		/* mask of whitespace (include \t,\n,\r,' ') and '>' */
        static UInt8 sWhitespaceAndGreaterThanMask[];
        static void Initialize();
        
        static char * GetUserNameCopy(QTSS_UserProfileObject inUserProfile);

        //GetGroupsArrayCopy 
        //
        // GetGroupsArrayCopy allocates outGroupCharPtrArray. Caller must "delete [] outGroupCharPtrArray" when done.
        static char*  GetAccessFile_Copy( const char* movieRootDir, const char* dirPath);

        //AccessAllowed
        //
        // This routine is used to get the Realm to send back to a user and to check if a user has access
        // userName: may be null.
        // accessFileBufPtr:If accessFileBufPtr is NULL or contains a NULL PTR or 0 LEN then false is returned
        // ioRealmNameStr:  ioRealmNameStr and ioRealmNameStr->Ptr may be null. 
        //                  To get a returned ioRealmNameStr value the ioRealmNameStr and ioRealmNameStr->Ptr must be non-NULL
        //                  valid pointers. The ioRealmNameStr.Len should be set to the ioRealmNameStr->Ptr's allocated len.
        // numGroups:       The number of groups in the groupArray. Use GetGroupsArrayCopy to create the groupArray.
        static Bool16 AccessAllowed (   char *userName, char**groupArray, UInt32 numGroups, 
                                        StrPtrLen *accessFileBufPtr,QTSS_ActionFlags inFlags,StrPtrLen* ioRealmNameStr
                                    );

        static void SetAccessFileName(const char *inQTAccessFileName); //makes a copy and stores it
        static char* GetAccessFileName() { return sQTAccessFileName; }; // a reference. Don't delete!
        
        // allocates memory for outUsersFilePath and outGroupsFilePath - remember to delete
        // returns the auth scheme
        static QTSS_AuthScheme FindUsersAndGroupsFilesAndAuthScheme(char* inAccessFilePath, QTSS_ActionFlags inAction, char** outUsersFilePath, char** outGroupsFilePath);
                
        static QTSS_Error AuthorizeRequest(QTSS_StandardRTSP_Params* inParams, Bool16 allowNoAccessFiles, QTSS_ActionFlags noAction, QTSS_ActionFlags authorizeAction);

    private:

		/* file name to access */
        static char* sQTAccessFileName; // managed by the QTAccess module
        static Bool16 sAllocatedName;
        static OSMutex* sAccessFileMutex;
};

#endif //_QT_ACCESS_FILE_H_

