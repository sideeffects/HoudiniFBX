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
* NAME:	ROP_FBXDerivedActions.C (FBX Library, C++)
*
* COMMENTS:	
*
*/

#include "ROP_FBXDerivedActions.h"
#include "ROP_FBXUtil.h"
#include "ROP_FBXMainVisitor.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXErrorManager.h"
#include "UT/UT_DMatrix4.h"
#include "OBJ/OBJ_Node.h"
#include "OP/OP_Node.h"
#include "OP/OP_Director.h"
#include "GEO/GEO_Point.h"
#include "SOP/SOP_Node.h"
#include "SOP/SOP_Capture.h"
/********************************************************************************************************/
// ROP_FBXLookAtAction
/********************************************************************************************************/
ROP_FBXLookAtAction::ROP_FBXLookAtAction(KFbxNode *acted_on_node, OP_Node* look_at_node, ROP_FBXActionManager& parent_manager) 
    : ROP_FBXBaseFbxNodeAction(acted_on_node, parent_manager)
{
    UT_ASSERT(look_at_node);
    myLookAtHdNode = look_at_node;
    
}
/********************************************************************************************************/
ROP_FBXLookAtAction::~ROP_FBXLookAtAction()
{
    
}
/********************************************************************************************************/
ROP_FBXActionType 
ROP_FBXLookAtAction::getType(void)
{
    return ROP_FBXActionSetLookAtTarget;
}
/********************************************************************************************************/
void 
ROP_FBXLookAtAction::performAction(void)
{
    KFbxNode *acted_on_node = this->getActedOnNode();

    if(!acted_on_node || !myLookAtHdNode)
	return;

    // Find the FBX node corresponding to the look at HD node
    KFbxNode *fbx_lookat_node = getNodeManager().findFbxNode(myLookAtHdNode);
    if(!fbx_lookat_node)
	return;

    acted_on_node->SetTarget(fbx_lookat_node);
}
/********************************************************************************************************/