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
ROP_FBXBaseAction::getParentManager(void)
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
ROP_FBXBaseAction::getIsActive(void)
{
    return myIsActive;
}
/********************************************************************************************************/
// ROP_FBXBaseHoudiniNodeAction
/********************************************************************************************************/
ROP_FBXBaseFbxNodeAction::ROP_FBXBaseFbxNodeAction(KFbxNode* acted_on_node, ROP_FBXActionManager& parent_manager) 
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
KFbxNode* 
ROP_FBXBaseFbxNodeAction::getActedOnNode(void)
{
    return myActedOnNode;
}
/********************************************************************************************************/
ROP_FBXErrorManager& 
ROP_FBXBaseFbxNodeAction::getErrorManager(void)
{
    return getParentManager().getErrorManager();
}
/********************************************************************************************************/
ROP_FBXNodeManager& 
ROP_FBXBaseFbxNodeAction::getNodeManager(void)
{
    return getParentManager().getNodeManager();
}
/********************************************************************************************************/
