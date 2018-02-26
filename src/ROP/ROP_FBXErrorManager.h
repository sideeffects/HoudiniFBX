/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NAME:	ROP_FBXErrorManager.h (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */

#ifndef __ROP_FBXErrorManager_h__
#define __ROP_FBXErrorManager_h__

#include "ROP_FBXCommon.h"
#include <UT/UT_String.h>

#include <vector>

/********************************************************************************************************/
enum ROP_FBXErrorType
{
    ROP_FBXErrorGeneric = 0,
    ROP_FBXErrorIncorrectPassword,
    ROP_FBXErrorLights
};
/********************************************************************************************************/
class ROP_FBXError
{
public:
    ROP_FBXError(const char* pMessage, bool bIsCritical, ROP_FBXErrorType eType);
    virtual ~ROP_FBXError();

    bool getIsCritical(void) const;
    const char* getMessage(void) const;
    ROP_FBXErrorType getType(void) const;

private:
    std::string myMessage;
    bool myIsCritical;
    ROP_FBXErrorType myType;
};
typedef std::vector<ROP_FBXError*> TROPErrorVector;
/********************************************************************************************************/
class ROP_FBXErrorManager
{
public:
    ROP_FBXErrorManager();
    virtual ~ROP_FBXErrorManager();

    void addError(const char* pcsError, bool bIsCritical = false, ROP_FBXErrorType eType = ROP_FBXErrorGeneric);
    void addError(const char* pcsErrorPart1, const char* pcsErrorPart2, const char* pcsErrorPart3, bool bIsCritical = false, ROP_FBXErrorType eType = ROP_FBXErrorGeneric);

    ROP_FBXError* getError(int err_index);
    int getNumItems(void) const;

    bool getDidReportCriticalErrors(void) const;

    void reset(void);

    void appendAllErrors(UT_String& string_out) const;
    void appendAllWarnings(UT_String& string_out) const;

private:

    TROPErrorVector myErrors;
    bool myDidReportCricialErrors;
};
/********************************************************************************************************/

#endif // __ROP_FBXErrorManager_h__

