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
* NAME:	ROP_FBXActionManager.h (FBX Library, C++)
*
* COMMENTS:	
*
*/
#ifndef __ROP_FBXActionManager_h__
#define __ROP_FBXActionManager_h__

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"

#include <vector>

class ROP_FBXBaseAction;
class ROP_FBXErrorManager;
class ROP_FBXNodeManager;

class ROP_FBXLookAtAction;
class ROP_FBXSkinningAction;
class ROP_FBXApplyVertexCacheAction;
class ROP_FBXApplySkinningAction;
class ROP_FBXApplyBlendAction;
class ROP_FBXCreateInstancesAction;
class ROP_FBXExporter;

class OP_Node;
class ROP_FBXIntTranslator;
/********************************************************************************************************/
typedef std::vector <ROP_FBXBaseAction *> TActionsVector;
/********************************************************************************************************/
class ROP_FBXActionManager
{
public:
    ROP_FBXActionManager(ROP_FBXNodeManager& node_manager, ROP_FBXErrorManager& error_manager, ROP_FBXExporter& parent_exporter);
    virtual ~ROP_FBXActionManager();

    ROP_FBXLookAtAction* addLookAtAction(FbxNode* acted_on_node, OP_Node* look_at_node);
    ROP_FBXSkinningAction* addSkinningAction(FbxNode* acted_on_node, OP_Node* deform_node, fpreal capture_frame);
    ROP_FBXCreateInstancesAction* addCreateInstancesAction(void);

    void performPostActions(void);

    /// Clears actions, deleting their objects.
    void clear(void);

    ROP_FBXErrorManager& getErrorManager(void);
    ROP_FBXNodeManager& getNodeManager(void);
    ROP_FBXBaseAction* getCurrentAction(void);
    ROP_FBXExporter& getExporter(void);

private:
    TActionsVector myPostActions;
    ROP_FBXNodeManager& myNodeManager;
    ROP_FBXErrorManager& myErrorManager;
    ROP_FBXExporter& myExporter;

    ROP_FBXBaseAction* myCurrentAction;

};

/********************************************************************************************************/

#endif // __ROP_FBXActionManager_h__
