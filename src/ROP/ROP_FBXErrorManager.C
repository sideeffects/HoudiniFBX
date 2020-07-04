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
 * NAME:	ROP_FBXErrorManager.C (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */

#include "ROP_FBXErrorManager.h"
#include <UT/UT_Assert.h>

using namespace std;

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
ROP_FBXErrorManager::getNumItems() const
{
    return (int)myErrors.size();
}
/********************************************************************************************************/
ROP_FBXError* 
ROP_FBXErrorManager::getError(int err_index)
{
    return myErrors[err_index];
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::reset()
{
    deleteVectorContents<ROP_FBXError*>(myErrors);
    myErrors.clear();
    myDidReportCricialErrors = false;
}
/********************************************************************************************************/
bool 
ROP_FBXErrorManager::getDidReportCriticalErrors() const
{
    return myDidReportCricialErrors;
}
/********************************************************************************************************/
void 
ROP_FBXErrorManager::appendAllErrors(UT_String& string_out) const
{

    ROP_FBXError* curr_error;
    TROPErrorVector::size_type curr_error_idx, num_errors = myErrors.size();
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
    TROPErrorVector::size_type curr_error_idx, num_errors = myErrors.size();
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
ROP_FBXError::getIsCritical() const
{
    return myIsCritical;
}
/********************************************************************************************************/
const char* 
ROP_FBXError::getMessage() const
{
    return myMessage.c_str();
}
/********************************************************************************************************/
ROP_FBXErrorType 
ROP_FBXError::getType() const
{
    return myType;
}
/********************************************************************************************************/
