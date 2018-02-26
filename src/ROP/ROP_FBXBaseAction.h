/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
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
 * NAME:	ROP_FBXBaseAction.h (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */
#ifndef __ROP_FBXBaseAction_h__
#define __ROP_FBXBaseAction_h__

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"

class OP_Node;
class ROP_FBXErrorManager;
class ROP_FBXNodeManager;
class ROP_FBXActionManager;
/********************************************************************************************************/
enum ROP_FBXActionType
{
    ROP_FBXActionSetLookAtTarget = 0,
    ROP_FBXActionApplySkinning,
    ROP_FBXActionCreateInstances
};
/********************************************************************************************************/
class ROP_FBXBaseAction
{
public:
    ROP_FBXBaseAction(ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXBaseAction();

    virtual ROP_FBXActionType getType(void) = 0;
    virtual void performAction(void) = 0;

    void setIsActive(bool value);
    bool getIsActive(void);

    ROP_FBXActionManager& getParentManager(void);

private:
    ROP_FBXActionManager& myParentManager;
    bool myIsActive;
};
/********************************************************************************************************/
class ROP_FBXBaseFbxNodeAction : public ROP_FBXBaseAction
{
public:
    ROP_FBXBaseFbxNodeAction(FbxNode* acted_on_node, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXBaseFbxNodeAction();

    FbxNode* getActedOnNode(void);
    ROP_FBXErrorManager& getErrorManager(void);
    ROP_FBXNodeManager& getNodeManager(void);

private:

    FbxNode* myActedOnNode;
};
/********************************************************************************************************/

#endif // __ROP_FBXBaseAction_h__
