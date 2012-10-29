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
