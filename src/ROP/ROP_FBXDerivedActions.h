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

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXBaseAction.h"

#include <vector>

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

    ROP_FBXLookAtAction(FbxNode *acted_on_node, OP_Node* look_at_node, ROP_FBXActionManager& parent_manager);
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

    ROP_FBXSkinningAction(FbxNode *acted_on_node, OP_Node* deform_node, fpreal capture_frame, ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXSkinningAction();

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    void createSkinningInfo(FbxNode* fbx_joint_node, FbxNode* fbx_deformed_node, KFbxSkin* fbx_skin, GEO_CaptureData& cap_data, int region_idx, OP_Context& capt_context);
    void addNodeRecursive(FbxArray<FbxNode*>& node_array, FbxNode* curr_node);
    void storeBindPose(FbxNode* fbx_node, fpreal capture_frame);

private:
    OP_Node* myDeformNode;    
    fpreal myCaptureFrame;
};
/********************************************************************************************************/
class ROP_FBXInstanceActionBundle
{
public:
    ROP_FBXInstanceActionBundle() { myHdNode = NULL; myFbxNode = NULL; }
    ROP_FBXInstanceActionBundle(OP_Node* instance_hd_node, FbxNode* instance_fbx_node) { myHdNode = instance_hd_node; myFbxNode = instance_fbx_node; }
    ~ROP_FBXInstanceActionBundle() { }

    OP_Node* myHdNode;
    FbxNode* myFbxNode;
};
typedef std::vector< ROP_FBXInstanceActionBundle > TInstanceBundleVector;

class ROP_FBXCreateInstancesAction : public ROP_FBXBaseAction
{
public:
    ROP_FBXCreateInstancesAction(ROP_FBXActionManager& parent_manager);
    virtual ~ROP_FBXCreateInstancesAction();

    void addInstance(OP_Node* instance_hd_node, FbxNode* instance_fbx_node);

    ROP_FBXActionType getType(void);
    void performAction(void);

private:
    TInstanceBundleVector myItems;
};
/********************************************************************************************************/
#endif // __ROP_FBXDerivedActions_h__
