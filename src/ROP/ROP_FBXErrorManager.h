/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*	Oleg Samus
*	Side Effects
*	123 Front St. West, Suite 1401
*	Toronto, Ontario
*	Canada   M5J 2M2
*	416-504-9876
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
class ROP_API ROP_FBXError
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
class ROP_API ROP_FBXErrorManager
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

