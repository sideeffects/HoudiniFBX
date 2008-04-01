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
* NAME:	ROP_FBXDerivedActions.h (FBX Library, C++)
*
* COMMENTS:	
*
*/
#ifndef __ROP_FBXDerivedActions_h__
#define __ROP_FBXDerivedActions_h__

#include <fbx/fbxsdk.h>
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseAction.h"

class FBX_FILMBOX_NAMESPACE::KFbxNode;
class FBX_FILMBOX_NAMESPACE::KFbxCluster;
class FBX_FILMBOX_NAMESPACE::KFbxSkin;
class ROP_FBXMainVisitor;
class OP_Network;
class GU_Detail;
class GEO_CaptureData;
class ROP_FBXNodeManager;
class ROP_FBXIntTranslator;
class OP_Context;
/********************************************************************************************************/
class ROP_FBXLookAtAction : public ROP_FBXBaseFbxNodeAction
{
public:

    ROP_FBXLookAtAction(KFbxNode *acted_on_node, OP_Node* look_at_node, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXLookAtAction();

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    OP_Node* myLookAtHdNode;    
};
/********************************************************************************************************/
class ROP_FBXSkinningAction : public ROP_FBXBaseFbxNodeAction
{
public:

    ROP_FBXSkinningAction(KFbxNode *acted_on_node, OP_Node* deform_node, float capture_frame, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXSkinningAction();

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    void createSkinningInfo(KFbxNode* fbx_joint_node, KFbxNode* fbx_deformed_node, KFbxSkin* fbx_skin, GEO_CaptureData& cap_data, int region_idx, OP_Context& capt_context);
    void addNodeRecursive(KArrayTemplate<KFbxNode*>& node_array, KFbxNode* curr_node);
    void storeBindPose(KFbxNode* fbx_node, float capture_frame);

private:
    OP_Node* myDeformNode;    
    float myCaptureFrame;
};
/********************************************************************************************************/
#endif // __ROP_FBXDerivedActions_h__
