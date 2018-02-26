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
 * NAME:	ROP_FBXActionManager.C (FBX Library, C++)
 *
 * COMMENTS:	
 *
 */

#include "ROP_FBXActionManager.h"
#include "ROP_FBXBaseAction.h"

#include "ROP_FBXDerivedActions.h"

using namespace std;

/********************************************************************************************************/
ROP_FBXActionManager::ROP_FBXActionManager(ROP_FBXNodeManager& node_manager, ROP_FBXErrorManager& error_manager, ROP_FBXExporter& parent_exporter) 
    : myNodeManager(node_manager), myErrorManager(error_manager), myExporter(parent_exporter)
{
    myCurrentAction = NULL;
}
/********************************************************************************************************/
ROP_FBXActionManager::~ROP_FBXActionManager()
{
    clear();
}
/********************************************************************************************************/
ROP_FBXLookAtAction* 
ROP_FBXActionManager::addLookAtAction(FbxNode* acted_on_node, OP_Node* look_at_node)
{
    ROP_FBXLookAtAction* new_action = new ROP_FBXLookAtAction(acted_on_node, look_at_node, *this);
    myPostActions.push_back(new_action);
    return new_action;
}
/********************************************************************************************************/
ROP_FBXSkinningAction* 
ROP_FBXActionManager::addSkinningAction(FbxNode* acted_on_node, OP_Node* deform_node, fpreal capture_frame)
{
    ROP_FBXSkinningAction* new_action = new ROP_FBXSkinningAction(acted_on_node, deform_node, capture_frame, *this);
    myPostActions.push_back(new_action);
    return new_action;
}
/********************************************************************************************************/
ROP_FBXCreateInstancesAction* 
ROP_FBXActionManager::addCreateInstancesAction(void)
{
    ROP_FBXCreateInstancesAction* new_action = new ROP_FBXCreateInstancesAction(*this);
    myPostActions.push_back(new_action);
    return new_action;
}
/********************************************************************************************************/
void 
ROP_FBXActionManager::performPostActions(void)
{
    TActionsVector::size_type curr_action, num_actions = myPostActions.size();
    for(curr_action = 0; curr_action < num_actions; curr_action++)
    {
	myCurrentAction = myPostActions[curr_action];
	if(myCurrentAction->getIsActive())
	    myPostActions[curr_action]->performAction();
    }
    myCurrentAction = NULL;

}
/********************************************************************************************************/
void 
ROP_FBXActionManager::clear(void)
{
    TActionsVector::size_type curr_action, num_actions = myPostActions.size();
    for(curr_action = 0; curr_action < num_actions; curr_action++)
    {
	delete myPostActions[curr_action];
    }    
    myPostActions.clear();
}
/********************************************************************************************************/
ROP_FBXErrorManager& 
ROP_FBXActionManager::getErrorManager(void)
{
    return myErrorManager;
}
/********************************************************************************************************/
ROP_FBXNodeManager& 
ROP_FBXActionManager::getNodeManager(void)
{
    return myNodeManager;
}
/********************************************************************************************************/
ROP_FBXBaseAction* 
ROP_FBXActionManager::getCurrentAction(void)
{
    return myCurrentAction;
}
/********************************************************************************************************/
ROP_FBXExporter& 
ROP_FBXActionManager::getExporter(void)
{
    return myExporter;
}
/********************************************************************************************************/
