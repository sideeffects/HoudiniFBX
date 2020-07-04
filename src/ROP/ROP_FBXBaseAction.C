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
 * NAME:	ROP_FBXBaseAction.C (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */

#include "ROP_FBXBaseAction.h"
#include "ROP_FBXActionManager.h"
#include <UT/UT_Assert.h>

using namespace std;

/********************************************************************************************************/
// ROP_FBXBaseAction
/********************************************************************************************************/
ROP_FBXBaseAction::ROP_FBXBaseAction(ROP_FBXActionManager& parent_manager) : myParentManager(parent_manager)
{
    myIsActive = true;
}
/********************************************************************************************************/
ROP_FBXBaseAction::~ROP_FBXBaseAction()
{

}
/********************************************************************************************************/
ROP_FBXActionManager& 
ROP_FBXBaseAction::getParentManager()
{
    return myParentManager;
}
/********************************************************************************************************/
void 
ROP_FBXBaseAction::setIsActive(bool value)
{
    myIsActive = value;
}
/********************************************************************************************************/
bool 
ROP_FBXBaseAction::getIsActive()
{
    return myIsActive;
}
/********************************************************************************************************/
// ROP_FBXBaseHoudiniNodeAction
/********************************************************************************************************/
ROP_FBXBaseFbxNodeAction::ROP_FBXBaseFbxNodeAction(FbxNode* acted_on_node, ROP_FBXActionManager& parent_manager) 
    : ROP_FBXBaseAction(parent_manager)
{
    UT_ASSERT(acted_on_node);
    myActedOnNode = acted_on_node;
}
/********************************************************************************************************/
ROP_FBXBaseFbxNodeAction::~ROP_FBXBaseFbxNodeAction()
{

}
/********************************************************************************************************/
FbxNode* 
ROP_FBXBaseFbxNodeAction::getActedOnNode()
{
    return myActedOnNode;
}
/********************************************************************************************************/
ROP_FBXErrorManager& 
ROP_FBXBaseFbxNodeAction::getErrorManager()
{
    return getParentManager().getErrorManager();
}
/********************************************************************************************************/
ROP_FBXNodeManager& 
ROP_FBXBaseFbxNodeAction::getNodeManager()
{
    return getParentManager().getNodeManager();
}
/********************************************************************************************************/
