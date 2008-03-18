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
* NAME:	ROP_FBXErrorManager.C (FBX Library, C++)
*
* COMMENTS:	
*
*/

#include "ROP_FBXErrorManager.h"
#include <UT/UT_Assert.h>

template < class T >
void deleteVectorContents( std::vector < T >& vector_in )
{
    typename std::vector<T>::size_type curr_elem, num_elems = vector_in.size();
    for(curr_elem = 0; curr_elem < num_elems; curr_elem++)
	delete vector_in[curr_elem];
    vector_in.clear(); 
}
/********************************************************************************************************/
// ROP_FBXErrorManager
/********************************************************************************************************/
ROP_FBXErrorManager::ROP_FBXErrorManager()
{
    reset();
}
/********************************************************************************************************/
ROP_FBXErrorManager::~ROP_FBXErrorManager()
{
    reset();
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::addError(const char* pcsError, bool bIsCritical, ROP_FBXErrorType eType)
{
    myErrors.push_back(new ROP_FBXError(pcsError, bIsCritical, eType));
    if(bIsCritical)
	myDidReportCricialErrors = true;
}
/********************************************************************************************************/
void ROP_FBXErrorManager::addError(const char* pcsErrorPart1, const char* pcsErrorPart2, const char* pcsErrorPart3, 
				bool bIsCritical, ROP_FBXErrorType eType)
{
    string strCombined;

    strCombined = "";
    if(pcsErrorPart1)
	strCombined += pcsErrorPart1;
    if(pcsErrorPart2)
	strCombined += pcsErrorPart2;
    if(pcsErrorPart3)
	strCombined += pcsErrorPart3;
    this->addError(strCombined.c_str(), bIsCritical, eType);
}
/********************************************************************************************************/
int 
ROP_FBXErrorManager::getNumItems(void) const
{
    return (int)myErrors.size();
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::reset(void)
{
    deleteVectorContents<ROP_FBXError*>(myErrors);
    myErrors.clear();
    myDidReportCricialErrors = false;
}
/********************************************************************************************************/
bool 
ROP_FBXErrorManager::getDidReportCriticalErrors(void) const
{
    return myDidReportCricialErrors;
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::appendAllErrors(UT_String& string_out) const
{

    ROP_FBXError* curr_error;
    TErrorVector::size_type curr_error_idx, num_errors = myErrors.size();
    for(curr_error_idx = 0; curr_error_idx < num_errors; curr_error_idx++)
    {
	curr_error = myErrors[curr_error_idx];
	if(curr_error->getIsCritical())
	{
	    string_out += "Error: ";
	    string_out += curr_error->getMessage();
	    string_out += "\n";
	}
    }
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::appendAllWarnings(UT_String& string_out) const
{
    ROP_FBXError* curr_error;
    TErrorVector::size_type curr_error_idx, num_errors = myErrors.size();
    for(curr_error_idx = 0; curr_error_idx < num_errors; curr_error_idx++)
    {
	curr_error = myErrors[curr_error_idx];
	if(!curr_error->getIsCritical())
	{
	    string_out += "Warning: ";
	    string_out += curr_error->getMessage();
   	    string_out += "\n";
	}
    }
}
/********************************************************************************************************/
// ROP_FBXError
/********************************************************************************************************/
ROP_FBXError::ROP_FBXError(const char* pMessage, bool bIsCritical, ROP_FBXErrorType eType)
{
    UT_ASSERT(pMessage);
    if(pMessage)
	myMessage = pMessage;
    myType = eType;
    myIsCritical = bIsCritical;
}
/********************************************************************************************************/
ROP_FBXError::~ROP_FBXError()
{
    
}
/********************************************************************************************************/
bool 
ROP_FBXError::getIsCritical(void) const
{
    return myIsCritical;
}
/********************************************************************************************************/
const char* 
ROP_FBXError::getMessage(void) const
{
    return myMessage.c_str();
}
/********************************************************************************************************/
ROP_FBXErrorType 
ROP_FBXError::getType(void) const
{
    return myType;
}
/********************************************************************************************************/
