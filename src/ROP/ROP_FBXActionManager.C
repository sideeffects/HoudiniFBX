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
* NAME:	ROP_FBXActionManager.C (FBX Library, C++)
*
* COMMENTS:	
*
*/

#include "ROP_FBXActionManager.h"
#include "ROP_FBXBaseAction.h"

#include "ROP_FBXDerivedActions.h"
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
ROP_FBXActionManager::addLookAtAction(KFbxNode* acted_on_node, OP_Node* look_at_node)
{
    ROP_FBXLookAtAction* new_action = new ROP_FBXLookAtAction(acted_on_node, look_at_node, *this);
    myPostActions.push_back(new_action);
    return new_action;
}
/********************************************************************************************************/
ROP_FBXSkinningAction* 
ROP_FBXActionManager::addSkinningAction(KFbxNode* acted_on_node, OP_Node* deform_node, float capture_frame)
{
    ROP_FBXSkinningAction* new_action = new ROP_FBXSkinningAction(acted_on_node, deform_node, capture_frame, *this);
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