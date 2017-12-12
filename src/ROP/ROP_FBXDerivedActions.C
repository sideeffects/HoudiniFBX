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

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXDerivedActions.h"
#include "ROP_FBXActionManager.h"
#include "ROP_FBXUtil.h"
#include "ROP_FBXMainVisitor.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXErrorManager.h"
#include <UT/UT_Matrix4.h>
#include <OBJ/OBJ_Node.h>
#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <SOP/SOP_Node.h>
#include <SOP/SOP_Capture.h>
#include <SOP/SOP_CaptureRegion.h>
#include <GEO/GEO_CaptureData.h>

using namespace std;

/********************************************************************************************************/
// ROP_FBXLookAtAction
/********************************************************************************************************/
ROP_FBXLookAtAction::ROP_FBXLookAtAction(FbxNode *acted_on_node, OP_Node* look_at_node, ROP_FBXActionManager& parent_manager) 
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
    FbxNode *acted_on_node = this->getActedOnNode();

    if(!acted_on_node || !myLookAtHdNode)
	return;

    // Find the FBX node corresponding to the look at HD node
    TFbxNodeInfoVector target_nodes;

    getNodeManager().findNodeInfos(myLookAtHdNode, target_nodes);
    if(target_nodes.size() == 0 || !target_nodes[0]->getFbxNode())
	return;
    
    acted_on_node->SetTarget(target_nodes[0]->getFbxNode());
}
/********************************************************************************************************/
// ROP_FBXSkinningAction
/********************************************************************************************************/
ROP_FBXSkinningAction::ROP_FBXSkinningAction(FbxNode *acted_on_node, OP_Node* deform_node, fpreal capture_frame, ROP_FBXActionManager& parent_manager)
    : ROP_FBXBaseFbxNodeAction(acted_on_node, parent_manager)
{
    UT_ASSERT(deform_node);
    myDeformNode = deform_node;
    myCaptureFrame = capture_frame;
}
/********************************************************************************************************/
ROP_FBXSkinningAction::~ROP_FBXSkinningAction()
{

}
/********************************************************************************************************/
ROP_FBXActionType 
ROP_FBXSkinningAction::getType(void)
{
    return ROP_FBXActionApplySkinning;
}
/********************************************************************************************************/
void 
ROP_FBXSkinningAction::performAction(void)
{
    if(!myDeformNode)
	return;

    FbxManager *sdk_manager = getParentManager().getExporter().getSDKManager();
    FbxNode* fbx_deformed_node = NULL; 

    TFbxNodeInfoVector res_nodes;
    getNodeManager().findNodeInfos(myDeformNode->getParentNetwork(), res_nodes);
    if(res_nodes.size() == 0)
	return;

    fbx_deformed_node = res_nodes[0]->getFbxNode();
    if(!fbx_deformed_node)
	return;

    // Get the immediate parent of the deform node
    OP_Node* node_above_deform = myDeformNode->getInput(myDeformNode->getConnectedInputIndex(-1));
    if(!node_above_deform)
	return;

    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(node_above_deform);
    if(!sop_node)
	return;

    fpreal start_time = getParentManager().getExporter().getStartTime();

    // Read weights and capture regions from the GDP
    GU_DetailHandle gdh;
    const GU_Detail *gdp;
    OP_Context	    context(start_time);

    if(!ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh))
	return;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    gdp = gdl.getGdp();
    if(!gdp)
	return;

    UT_String path;
    GEO_CaptureData cap_data;
    sop_node->getFullPath(path);

    cap_data.initialize(path, 0.0f);
    if(!cap_data.transferFromGdp(gdp, NULL))
	return;

    FbxSkin* fbx_skin = NULL; 
    OP_Node* cregion_node, *cregion_parent;
    //FbxNode* fbx_cregion_parent;

    fpreal capture_time = CHgetManager()->getTime(myCaptureFrame);
    OP_Context capt_context(capture_time);

    // Go through every capture region, find a corresponding FBX node.
    // If none exists, keep finding the parent object until its FBX node is found, then create
    // a new FBX node under that, and use it as a skinning node.
    int curr_region, num_regions = cap_data.getNumRegions();
    for(curr_region = 0; curr_region < num_regions; curr_region++)
    {
	path = cap_data.regionPath(curr_region);
	cregion_node = OPgetDirector()->findNode(path);
	if(!cregion_node)
	    continue;

	// Get the parent 
	cregion_parent = cregion_node->getParentNetwork();

	if(cregion_parent == (OP_Node*)(myDeformNode->getParentNetwork()))
	{
	    // Special case - the cregion is in the same network as us.
	    UT_ASSERT(0);
	}
	else
	{
	    // Try find the FBX node corresponding to the parent
	    TFbxNodeInfoVector cregion_parent_infos;
	    getNodeManager().findNodeInfos(cregion_parent, cregion_parent_infos);
	    //fbx_cregion_parent = getNodeManager().findFbxNode(cregion_parent);
	    if(cregion_parent_infos.size() > 0 && cregion_parent_infos[0]->getFbxNode())
	    {
		// TODO: we need to create, parent, and set the transform of a fake
		// null node that will symbolize the center of the capture region in question.
		// Even when the region has an  identity transform, its center is at the center of 
		// the bone. Or do we?

		if(!fbx_skin)
		{
		    fbx_skin = FbxSkin::Create(sdk_manager, "");
		    if (ROP_FBXUtil::getIntOPParm(myDeformNode, "usedqskin") == 0)
		    {
			fbx_skin->SetSkinningType(FbxSkin::eLinear);
		    }
		    else
		    {
			// TODO: Add support for FbxSkin::eBlend which is blend
			//       of linear and dual quaternion
			fbx_skin->SetSkinningType(FbxSkin::eDualQuaternion);
		    }
		}
		createSkinningInfo(
			cregion_parent_infos[0]->getFbxNode(),
			fbx_deformed_node, fbx_skin,
			cap_data, curr_region,
			dynamic_cast<SOP_CaptureRegion*>(cregion_node),
			capt_context);
	    }
	    else
	    {
		// Cregion's parent wasn't exported to FBX. TODO - created it,
		// parent to world, set transform as its world transform multiplied
		// by inverse of the uppermost exported node's transform.
		UT_ASSERT(0);
		getErrorManager().addError("Cregion's container node was not exported to file.", NULL, NULL, false);
	    }
	}

    } // end for over cregions

    if(fbx_skin)
    {
	FbxGeometry* node_attr = FbxCast<FbxGeometry>(fbx_deformed_node->GetNodeAttribute());
	if(node_attr)
	    node_attr->AddDeformer(fbx_skin);

	// Store the bind pose
	storeBindPose(fbx_deformed_node, myCaptureFrame);
    }
}
/********************************************************************************************************/
void 
ROP_FBXSkinningAction::createSkinningInfo(
	FbxNode* fbx_joint_node, FbxNode* fbx_deformed_node,  FbxSkin* fbx_skin,
	GEO_CaptureData& cap_data, int region_idx, SOP_CaptureRegion *cregion,
	OP_Context& capt_context)
{
    FbxManager *sdk_manager = getParentManager().getExporter().getSDKManager();
    FbxCluster *main_cluster = FbxCluster::Create(sdk_manager,"");

    FbxAMatrix xform_matrix;

    main_cluster->SetLink(fbx_joint_node);
    main_cluster->SetLinkMode(FbxCluster::eNormalize);

    // Set the skin deformer params
    int curr_point, num_points = cap_data.getNumStoredPts();
    double pt_weight = -1;
    int opt_get_weight_idx = 0;

    for(curr_point = 0; curr_point < num_points; curr_point++)
    {
	pt_weight = cap_data.getPointWeight(curr_point, region_idx, &opt_get_weight_idx);
	if(pt_weight < 0.0)
	    pt_weight = 0.0;
        if( pt_weight > 0.0 )
        {            
	    main_cluster->AddControlPointIndex(curr_point, pt_weight);
        }
    }

    ROP_FBXNodeInfo* node_info;
    OP_Node* hd_node;
    ROP_FBXNodeManager& node_manager = getParentManager().getNodeManager();
    UT_Matrix4D world_matrix;

    // Set the world transform of the skin object at capture time
    node_info = node_manager.findNodeInfo(fbx_deformed_node);
    if(node_info)
    {
	hd_node = node_info->getHdNode();
	(void) hd_node->getWorldTransform(world_matrix, capt_context);
	ROP_FBXUtil::convertHdMatrixToFbxMatrix<FbxAMatrix>(world_matrix, xform_matrix);
    }
    else
	xform_matrix = fbx_deformed_node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE, FbxNode::eSourcePivot);
    main_cluster->SetTransformMatrix(xform_matrix);

    // Get the world transform of the link object at capture time
    //
    // In Houdini, skinning is done not just using the bone's world transform,
    // but also using its cregion primitive transform as well, which is 
    // different (in general) at capture time vs deform time.
    // FBX on the other hand, only does skinning using joint transforms. When
    // we export, we keep the joint transforms the same as the bone transforms
    // for animation. So what we do here is fake the link's capture transform
    // to absorb the deform cregion's transform, assuming that its not
    // animated. If it's animated, then the export won't match exactly but
    // we're willing to give that up because the deform cregion is rarely
    // animated.
    //
    // In Houdini, we compute at Capture time:
    //	    CRegionWorld = CRegionXform * CBoneWorld
    // But then store its inverse within cap_data, ie:
    //      cap_data.regionXform = CRegionWorld.Inverse
    //                           = CBoneWorld.Inverse * CRegionXform.Inverse
    // At Deform time, this is used to determine how much the bone's deform
    // cregion moved relative to the bone's capture cregion:
    //      DeltaXform = CRegionWorld.Inverse * DRegionXform * DBoneWorld
    // On export we, set JointWorld = DBoneWorld, so:
    //      DeltaXform = CRegionWorld.Inverse * DRegionXform * JointWorld
    // Thus, we want,
    //      JointCaptureWorld.Inverse = CRegionWorld.Inverse * DRegionXform
    //      JointCaptureWorld = (CRegionWorld.Inverse * DRegionXform).Inverse
    //
    UT_Matrix4 dregion_xform(1.0); // identity
    if (cregion)
    {
	float bcap, tcap;
	float taperx, taperz;
	float iweight, oweight;
	(void) cregion->getCaptureData(
		    capt_context.getTime(), dregion_xform,
		    bcap, tcap, taperx, taperz, iweight, oweight,
		    /*capture*/false, /*depnode*/nullptr);
    }
    UT_Matrix4D joint_xform(cap_data.regionXform(region_idx));
    joint_xform *= dregion_xform;
    joint_xform.invert();
    joint_xform *= world_matrix;
    ROP_FBXUtil::convertHdMatrixToFbxMatrix(joint_xform, xform_matrix);
    main_cluster->SetTransformLinkMatrix(xform_matrix);

    // Add it to the cluster
    fbx_skin->AddCluster(main_cluster);
}
/********************************************************************************************************/
void 
ROP_FBXSkinningAction::storeBindPose(FbxNode* fbx_node, fpreal capture_frame)
{  
    FbxScene *fbx_scene = getParentManager().getExporter().getFBXScene();
    FbxManager *fbx_sdk_manager = getParentManager().getExporter().getSDKManager();

    fpreal capture_time = CHgetManager()->getTime(capture_frame);
    OP_Context capt_context(capture_time);
    
    // Now list the all the link involve in the patch deformation	
    FbxArray<FbxNode*> pose_fbx_nodes;
    int                       i, j;

    if (fbx_node && fbx_node->GetNodeAttribute())
    {
	int num_skins = 0;
	int num_clusters = 0;
	switch (fbx_node->GetNodeAttribute()->GetAttributeType())
	{
	case FbxNodeAttribute::eMesh:
	case FbxNodeAttribute::eNurbs:
	case FbxNodeAttribute::ePatch:

	    num_skins = ((FbxGeometry*)fbx_node->GetNodeAttribute())->GetDeformerCount(FbxDeformer::eSkin);
	    //Go through all the skins and count them
	    //then go through each skin and get their cluster count
	    for(i=0; i<num_skins; ++i)
	    {
		FbxSkin *lSkin=(FbxSkin*)((FbxGeometry*)fbx_node->GetNodeAttribute())->GetDeformer(i, FbxDeformer::eSkin);
		num_clusters += lSkin->GetClusterCount();	
	    }
	    break;
	default:
	    // ignore all others
	    break;
	}
	//if we found some clusters we must add the node
	if (num_clusters > 0)
	{
	    FbxNode* cluster_node;
	    FbxSkin *curr_skin;

	    //Again, go through all the skins get each cluster link and add them
	    for (i=0; i<num_skins; ++i)
	    {
		curr_skin = (FbxSkin*)((FbxGeometry*)fbx_node->GetNodeAttribute())->GetDeformer(i, FbxDeformer::eSkin);
		num_clusters = curr_skin->GetClusterCount();
		for (j=0; j<num_clusters; ++j)
		{
		    cluster_node = curr_skin->GetCluster(j)->GetLink();
		    addNodeRecursive(pose_fbx_nodes, cluster_node);
		}

	    }

	    // Add the patch to the pose
	    addNodeRecursive(pose_fbx_nodes, fbx_node);
	}
    }

    // Now create a bind pose with the link list
    if (pose_fbx_nodes.GetCount())
    {
	// A pose must be named. Arbitrarily use the name of the patch node.
	FbxPose* bind_pose = FbxPose::Create(fbx_sdk_manager,fbx_node->GetName());
	FbxNode*  curr_pose_node;
	FbxMatrix bind_matrix;
	bind_pose->SetIsBindPose(true);
	ROP_FBXNodeInfo* node_info;
	OP_Node* hd_node;
	ROP_FBXNodeManager& node_manager = getParentManager().getNodeManager();
	UT_Matrix4D world_matrix;
    
	for (i=0; i<pose_fbx_nodes.GetCount(); i++)
	{
	    curr_pose_node = pose_fbx_nodes.GetAt(i);
	    node_info = node_manager.findNodeInfo(curr_pose_node);
	    if(node_info)
	    {
		hd_node = node_info->getHdNode();
		(void) hd_node->getWorldTransform(world_matrix, capt_context);
		ROP_FBXUtil::convertHdMatrixToFbxMatrix<FbxMatrix>(world_matrix, bind_matrix);
	    }
	    else
		bind_matrix = curr_pose_node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE, FbxNode::eSourcePivot);

	    bind_pose->Add(curr_pose_node , bind_matrix);
	}

	// Add the pose to the scene
	if(!fbx_scene->AddPose(bind_pose))
	{
	    UT_ASSERT(0);
	    getParentManager().getErrorManager().addError("Could not add the bind pose: ", fbx_node->GetName(),NULL, false);
	}
    }
}
/********************************************************************************************************/
void 
ROP_FBXSkinningAction::addNodeRecursive(FbxArray<FbxNode*>& node_array, FbxNode* curr_node)
{
    // Add the specified node to the node array. Also, add recursively
    // all the parent node of the specified node to the array.
    if (curr_node)
    {
	addNodeRecursive(node_array, curr_node->GetParent());

	if (node_array.Find(curr_node) == -1)
	{
	    // Node not in the list, add it
	    node_array.Add(curr_node);
	}
    }
}
/********************************************************************************************************/
// ROP_FBXCreateInstancesAction
/********************************************************************************************************/
ROP_FBXCreateInstancesAction::ROP_FBXCreateInstancesAction(ROP_FBXActionManager& parent_manager)
    : ROP_FBXBaseAction(parent_manager)
{

}
/********************************************************************************************************/
ROP_FBXCreateInstancesAction::~ROP_FBXCreateInstancesAction()
{

}
/********************************************************************************************************/
void 
ROP_FBXCreateInstancesAction::addInstance(OP_Node* instance_hd_node, FbxNode* instance_fbx_node)
{
    ROP_FBXInstanceActionBundle temp_bundle(instance_hd_node, instance_fbx_node);
    myItems.push_back(temp_bundle);
}
/********************************************************************************************************/
ROP_FBXActionType 
ROP_FBXCreateInstancesAction::getType(void)
{
    return ROP_FBXActionCreateInstances;
}
/********************************************************************************************************/
void 
ROP_FBXCreateInstancesAction::performAction(void)
{
    // Go over all hd instance nodes. For each try to find its target hd node, then
    // a corresponding FBX node with an attribute set.
    // If found, copy the attribute and set it as this node. If not, continue.
    // Add a check to prevent infinite loops - if didn't find anything in one iteration,
    // quite with an error.

    bool did_find_any_targets = true;
    bool are_all_instances_set = false;

    ROP_FBXNodeManager& node_manager = getParentManager().getNodeManager();    

    OP_Node *hd_inst, *hd_inst_target;
    UT_String target_obj_path;
    int curr_inst_idx, num_inst = myItems.size();

    ROP_FBXNodeInfo *this_node_info;

    ROP_FBXMainVisitor geom_visitor(&getParentManager().getExporter());
    ROP_FBXMainNodeVisitInfo visit_info(NULL);
    ROP_FBXMainNodeVisitInfo *target_node_info;
    geom_visitor.addNonVisitableNetworkTypes(ROP_FBXnetworkTypesToIgnore);

    TFbxNodeInfoVector inst_nodes;
    int curr_inst_node, num_inst_nodes;
    while(!are_all_instances_set && did_find_any_targets)
    {
	did_find_any_targets = false;
	are_all_instances_set = true;

	for(curr_inst_idx = 0; curr_inst_idx < num_inst; curr_inst_idx++)
	{

	    if(myItems[curr_inst_idx].myFbxNode->GetNodeAttribute())
		continue;

	    are_all_instances_set = false;

	    // Get the pointed-to HD node
	    hd_inst = myItems[curr_inst_idx].myHdNode;
	    hd_inst_target = ROP_FBXUtil::findNonInstanceTargetFromInstance(hd_inst);
	    if(!hd_inst_target)
		continue;

	    // Find the corresponding FBX node
	    //node_manager.findNodeInfos(hd_inst_target, inst_nodes);
//		target_fbx = inst_nodes[curr_inst_node]->getFbxNode();
//		if(!target_fbx || !target_fbx->GetNodeAttribute())
//		    continue;
	    inst_nodes.clear();
	    node_manager.findNodeInfos(hd_inst, inst_nodes);
	    num_inst_nodes = inst_nodes.size();
	    for(curr_inst_node = 0; curr_inst_node < num_inst_nodes; curr_inst_node++)
	    {
		this_node_info = inst_nodes[curr_inst_node];
//		this_node_info = node_manager.findNodeInfo(hd_inst);
		if(!this_node_info)
		    continue;

		// Unbelievably, there is no way to make a copy of a node's attribute
		// in FBX, and they can't be instanced. We're forced to re-create
		// the node from scratch. Arghhh.
    	    
		//fbx_target_attr = target_fbx->GetNodeAttribute();
		//myItems[curr_inst_idx].myFbxNode->AddNodeAttribute(fbx_target_attr);
		visit_info = this_node_info->getVisitInfo();
		visit_info.setFbxNode(myItems[curr_inst_idx].myFbxNode->GetParent());
		visit_info.setHdNode(hd_inst);

		target_node_info = dynamic_cast<ROP_FBXMainNodeVisitInfo *>(geom_visitor.visitBegin(hd_inst_target, -1));
		target_node_info->setParentInfo(&visit_info);
		target_node_info->setFbxNode(myItems[curr_inst_idx].myFbxNode);
		target_node_info->setIsVisitingFromInstance(true);

		geom_visitor.visit(hd_inst_target, target_node_info);
		did_find_any_targets = true;

		delete target_node_info;
	    }
	}
    }

    if(!are_all_instances_set)
    {
	getParentManager().getErrorManager().addError("Some instances could not be connected properly.",NULL,NULL, false);	
    }

    setIsActive(false);
}
/********************************************************************************************************/
