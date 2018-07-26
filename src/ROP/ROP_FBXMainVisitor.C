/*
 * Copyright (c) 2017
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
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
 * NAME:	ROP library (C++)
 *
 * COMMENTS:	Class for FBX output.
 *
 */

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXMainVisitor.h"

#include "ROP_FBXActionManager.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXDerivedActions.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXUtil.h"

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Geometry.h>
#include <SOP/SOP_Node.h>
#include <SOP/SOP_BlendShapes.h>
#include <VOP/VOP_Node.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Output.h>

#include <GU/GU_ConvertParms.h>
#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimRBezCurve.h>
#include <GU/GU_PrimRBezSurf.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GEO/GEO_Profiles.h>
#include <GEO/GEO_TPSurf.h>
#include <GEO/GEO_Vertex.h>
#include <GD/GD_Detail.h>
#include <GD/GD_Face.h>
#include <GD/GD_PrimPoly.h>
#include <GD/GD_PrimRBezCurve.h>
#include <GD/GD_TrimPiece.h>
#include <GD/GD_TrimRegion.h>
#include <GA/GA_ATIGroupBool.h>
#include <GA/GA_AttributeFilter.h>
#include <GA/GA_ElementWrangler.h>
#include <GA/GA_Names.h>

#include <OP/OP_Director.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_Utils.h>

#include <UT/UT_BoundingRect.h>
#include <UT/UT_CrackMatrix.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_StringHolder.h>


#ifdef UT_DEBUG
extern double ROP_FBXdb_maxVertsCountingTime;
#endif

using namespace std;

const char *const theBlendShapeNodeTypes[] =
{
    "blendshapes",
    nullptr
};

#define CAPT_SKIN_NODE_TYPES "bonecapturebiharmonic", "captureproximity", "capture"

// We do allow more nodes before blend shapes than ROP_FBXallowed_inbetween_node_types:
// capture, captureoverride, deform and merge
const char * theAllowedInBetweenNodeTypes[] =
{
    "null", "switch", "subnet", "attribcomposite",
    "attribcopy", "attribcreate", "attribmirror", "attribpromote", "attribreorient",
    "attribpromote", "attribstringedit", "attribute", "cache",
    CAPT_SKIN_NODE_TYPES, "captureoverride", "bonedeform", "deform", "merge",
    "stash",
    nullptr
};


/********************************************************************************************************/
ROP_FBXMainVisitor::ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter) 
: ROP_FBXBaseVisitor(parent_exporter->getExportOptions()->getInvisibleNodeExportMethod(), parent_exporter->getStartTime())
{
    myParentExporter = parent_exporter;

    mySDKManager = myParentExporter->getSDKManager();
    myScene = myParentExporter->getFBXScene();
    myErrorManager = myParentExporter->getErrorManager();
    UT_ASSERT(myErrorManager);

    myNodeManager = myParentExporter->getNodeManager();
    myActionManager = myParentExporter->getActionManager();

    myAmbientColor.setRGB(0,0,0);
    myMaterialsMap.clear();
    myStringMaterialMap.clear();
    myTexturesMap.clear();

    myDefaultMaterial = NULL;
    myDefaultTexture = NULL;
    myInstancesActionPtr = NULL;

    myStartTime = myParentExporter->getStartTime();
    myBoss = myParentExporter->GetBoss();
}
/********************************************************************************************************/
ROP_FBXMainVisitor::~ROP_FBXMainVisitor()
{

}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo* 
ROP_FBXMainVisitor::visitBegin(OP_Node* node, int input_idx_on_this_node)
{
    return new ROP_FBXMainNodeVisitInfo(node);
}
/********************************************************************************************************/
ROP_FBXVisitorResultType 
ROP_FBXMainVisitor::visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info_in)
{
    ROP_FBXVisitorResultType res_type = ROP_FBXVisitorResultOk;
    if(!node)
	return res_type;

    // Check for interrupt
    if(myBoss->opInterrupt())
	return ROP_FBXVisitorResultAbort;

    //FbxNode* res_new_node = NULL;
    FbxNode* temp_new_node;
    TFbxNodesVector res_nodes;
    FbxNode* fbx_parent_node = NULL;
    if(node_info_in && node_info_in->getParentInfo() != NULL)
    {
	fbx_parent_node = node_info_in->getParentInfo()->getFbxNode();
    }
    else
    {
	fbx_parent_node = myParentExporter->getFBXRootNode(node,
		myParentExporter->getExportOptions()->getCreateSubnetRoot());
    }

    if(!fbx_parent_node)
	return ROP_FBXVisitorResultSkipSubtreeAndSubnet;

    ROP_FBXMainNodeVisitInfo *node_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info_in);
    ROP_FBXMainNodeVisitInfo *parent_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info_in->getParentInfo());

    // Determine which type of Houdini node this is
    bool is_sop_export = myParentExporter->getExportOptions()->isSopExport();

    string lookat_parm_name("lookatpath");
    UT_StringRef node_type = node->getOperator()->getName();
    if (is_sop_export)
	node_type = "geo";

    bool is_visible;
    fpreal start_time = myParentExporter->getStartTime();
    OBJ_Node *obj_node = node->castToOBJNode();
    if (obj_node)
	is_visible = obj_node->getObjectDisplay(start_time);
    else 
	is_visible = is_sop_export ? true : node->getVisible();

    // Consider all nodes visible
    if ( myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportAsVisible )
	is_visible = true;

    bool force_exporting_as_null = false;
    if(myParentExporter->getExportOptions()->isExportingBundles() &&
	myParentExporter->getNodeManager()->isNodeBundled(node) == false)
	force_exporting_as_null = true;

    ROP_FBXGDPCache *v_cache = NULL;
    bool force_ignore_node = false;
    UT_StringRef override_node_type;

    // HDA dont have a "geo" node_type, see if the node is a OBJ_Geo instead
    bool consider_as_geo = is_sop_export;
    if (obj_node != nullptr)
    {
	OBJ_Geometry* geo_node = obj_node->castToOBJGeometry();
	if (geo_node && (geo_node->getObjectType() == OBJ_GEOMETRY))
	    consider_as_geo = true;
    }

    if(!force_exporting_as_null)
    if( ( is_visible && myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportAsNulls ) 
	|| myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportFull
	|| myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportAsVisible
	|| node_type == "null" || ROPfbxIsLightNodeType(node_type) || node_type == "cam" || node_type == "bone" || node_type == "ambient" )
    {
	if(node_type == "instance")
	{
	    // We need to create a node (in case we have children),
	    // *but* we need to attach the instance attribute later, as a post-action,
	    // in case it's not created at this point.
	    UT_String node_name;
	    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

	    temp_new_node = FbxNode::Create(mySDKManager, (const char*)node_name);
	    res_nodes.push_back(temp_new_node);

	    if(!myInstancesActionPtr)	    
		myInstancesActionPtr = myActionManager->addCreateInstancesAction();
	    myInstancesActionPtr->addInstance(node, temp_new_node);

	    // See if we're an instance of a light or a camera, in which case
	    // we need to apply special transforms to this node.
	    OP_Node* inst_target = ROP_FBXUtil::findNonInstanceTargetFromInstance(node);
	    if(inst_target)
	    {
		const UT_StringHolder& inst_target_node_type = inst_target->getOperator()->getName();
		if(inst_target_node_type == "cam" || ROPfbxIsLightNodeType(inst_target_node_type))
		    override_node_type = inst_target_node_type;
	    }

	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
	else if(node_type == "null")
	{
	    // Null node
	    bool is_joint_null_node = ROP_FBXUtil::isJointNullNode(node);
	    bool is_last_joint_node = (parent_info && parent_info->getBoneLength() > 0.0);
	    bool is_lod_group_node = ROP_FBXUtil::isLODGroupNullNode(node);
	    if(is_joint_null_node || is_last_joint_node)
	    {
		// This is really a joint. Export it as such.
		outputBoneNode(node, node_info, fbx_parent_node, true, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;

		if(!is_joint_null_node && is_last_joint_node)
		    node_info->setBoneLength(0.0);
	    }
	    else if (is_lod_group_node)
	    {
		// This null is actually an LODGroup. Export it as such
		outputLODGroupNode(node, node_info, fbx_parent_node, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;
	    }
	    else
	    {
		// Regular null node.
		outputNullNode(node, node_info, fbx_parent_node, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;
	    }
	}
	else if(ROPfbxIsLightNodeType(node_type))
	{
	    outputLightNode(node, node_info, fbx_parent_node, res_nodes);
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	    if (node_type == "hlight")
	    {
		// Old hlight is special.
		lookat_parm_name = "l_" + lookat_parm_name;
	    }
	}
	else if(node_type == "cam")
	{
	    outputCameraNode(node, node_info, fbx_parent_node, res_nodes);
	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
	else if(node_type == "bone")
	{
	    // Export bones - only if this isn't a dummy bone for display only.
	    if(ROP_FBXUtil::isDummyBone(node) == false)
	    {
		outputBoneNode(node, node_info, fbx_parent_node, false, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;
	    }
	    else
	    {
		force_ignore_node = true;
		res_type = ROP_FBXVisitorResultSkipSubtreeAndSubnet;
	    }
	}
	else if(node_type == "ambient")
	{
	    int is_enabled = ROP_FBXUtil::getIntOPParm(node, "light_enable",0, myStartTime);
	    if(is_enabled)
	    {
		fpreal amb_intensity;
		fpreal amb_light_col[3];
		amb_intensity = ROP_FBXUtil::getFloatOPParm(node, "light_intensity", 0, myStartTime);
		amb_light_col[0] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 0, myStartTime);
		amb_light_col[1] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 1, myStartTime);
		amb_light_col[2] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 2, myStartTime);

		UT_Color this_amb_color;
		this_amb_color.setRGB(amb_light_col[0] * amb_intensity, amb_light_col[1] * amb_intensity, amb_light_col[2] * amb_intensity);
		myAmbientColor.addRGB(this_amb_color);
	    }
	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
	else if (node_type == "geo" || consider_as_geo)
	{
	    bool did_cancel;
	    outputGeoNode(node, node_info, fbx_parent_node, v_cache, did_cancel, res_nodes);

	    // We don't need to dive into the geo node, and if we're a SOP ROP, we dont need to keep exporting after this node
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	    if (did_cancel)
	    {
		UT_ASSERT(res_nodes.size() == 0);
		return ROP_FBXVisitorResultAbort;
	    }
	}
    }

    //if(!res_new_node && !force_ignore_node)
    if(res_nodes.size() == 0 && !force_ignore_node)
    {
	// Treat everything else as a null node
	outputNullNode(node, node_info, fbx_parent_node, res_nodes);

	// We do this because some invisible nodes may have extremely complex subnets that will
	// take forever to export. Might make this an option later.
	if(!is_visible)
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	// Set the result to skip the subnet for all non standard objects so
	// that we don't try to output them twice.
	if (obj_node && obj_node->getChildTypeID() != OBJ_OPTYPE_ID)
	    res_type = ROP_FBXVisitorResultSkipSubnet;
    }

    if(res_nodes.size() > 0 && fbx_parent_node)
    {
	UT_ASSERT(node_info);

	int curr_node, num_nodes = res_nodes.size();
	for(curr_node = 0; curr_node < num_nodes; curr_node++)
	{
	    finalizeNewNode(res_nodes[curr_node], node, node_info, fbx_parent_node, override_node_type,
		lookat_parm_name.c_str(), res_type, v_cache, is_visible);
	}
    }

    return res_type;
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::setFbxNodeVisibility(
	FbxNode &node, OP_Node *hd_node, bool visible)
{
    // As is often, we have 3 different models that we're trying to match here
    // and none of them overlap completely. We do the best we can here by
    // exporting as conservatively as possible.
    //
    // FBX:
    // - Animatible Visibility property that is 0-1 float values
    // - VisibilityInheritance property that says whether its Visibility flag
    //   is ANDed with its parent node's effective visibility state
    // - Non-animatible Show property that in practice is ANDed with its
    //   effective visibility state after Visibility/VisibilityInheritance is
    //   taken into account. In the documentation, they suggest that the latter
    //   should take precedence if both properties exist. However, that's not
    //   the case for FBX files output from Maya.
    //
    // Houdini:
    // - Display flag on object, ANDed with an optional animatible display parm
    // - Display state is further filtered by visibility masks on its parent
    // - Ideally, we just map the effective visiblity state (possibly animated)
    //   into the FBX Visiblity property and always turn off inheritance.
    //
    // Maya:
    // - Both transform and shape nodes have their own Visibility attribute
    //   and are animatible
    // - Visibility inheritance ALWAYs occurs
    // - Except, the transform nodes are parents of shape nodes as well and
    //   shape nodes have no children.
    // - Transform node Visibility attribute values are mapped onto the
    //   FBX Visibility property with possible animation. Shape node Visibility
    //   attribute is mapped to the FBX Show property because FBX doesn't have
    //   the concept of child shape nodes. However, FBX forbids the Show
    //   property from being animated (attempts to create channels on it will
    //   fail). From testing, Maya is unable to export FBX files with animated
    //   Visibility attributes on shape nodes.

    // Turn off visibility inheritance as Houdini has no concept of this
    node.VisibilityInheritance.Set(false);

    // Map Houdini's effective visibility state to the FBX Show property as
    // this is the only way in Maya to selectively toggle visibility of nodes
    // in the scene graph without affecting child nodes (because Maya shape
    // nodes (typically?) don't have children). This will usually be just the
    // display flag. If the display state is animated, then we will export it
    // into the Visibility property even though we know Maya will ignore it.
    // Houdini will pick it up though. Note that the defaults for all three
    // properties is 1.0/true.
    OBJ_Node *obj_node = CAST_OBJNODE(hd_node);
    if (obj_node && obj_node->isDisplayTimeDependent())
    {
	node.SetVisibility(visible);
	myErrorManager->addError(
	    "Exported time dependent display."
	    " Will be ignored in Maya and Houdini with Create Nulls as Subnets. Node: ",
	    node.GetName(), nullptr);
    }
    else
    {
	node.Show.Set(visible);
    }
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::finalizeNewNode(ROP_FBXConstructionInfo& constr_info, OP_Node* hd_node, ROP_FBXMainNodeVisitInfo *node_info, FbxNode* fbx_parent_node, 
		const UT_StringRef& override_node_type, const char* lookat_parm_name, ROP_FBXVisitorResultType res_type,
		ROP_FBXGDPCache *v_cache, bool is_visible)
{
    UT_ASSERT(lookat_parm_name);

    UT_String lookatobjectpath;
    ROP_FBXUtil::getStringOPParm(hd_node, lookat_parm_name, lookatobjectpath, true, myStartTime);

    FbxNode* new_node = constr_info.getFbxNode();

    ROP_FBXNodeInfo *res_node_pair_info;
    if(node_info->getIsVisitingFromInstance() && node_info->getFbxNode())
    {
	// Special trick to re-parent the attribute.
	node_info->getFbxNode()->SetNodeAttribute(new_node->GetNodeAttribute());
	new_node->SetNodeAttribute(NULL);
	new_node->Destroy();
	new_node = node_info->getFbxNode();

	res_node_pair_info = myNodeManager->findNodeInfo(node_info->getFbxNode());
	UT_ASSERT(res_node_pair_info);
    }
    else
    {
	setFbxNodeVisibility(*new_node, hd_node, is_visible);

	OBJ_Node *obj_node = hd_node->castToOBJNode();
	if (ROP_FBXUtil::mapsToFBXTransform(myStartTime, obj_node))
	{
	    exportFBXTransform(myStartTime, obj_node, new_node);
	}
	else
	{
	    // Set the standard transformations (unless we're in the instance)
	    ROP_FBXUtil::setStandardTransforms(hd_node, new_node, node_info, 0.0, myStartTime);
	}

	// If there's a lookat object, queue up the action
	if(lookatobjectpath.length() > 0)
	{
	    // Get the actual node ptr
	    OP_Node *lookat_node = hd_node->findNode((const char*)lookatobjectpath);

	    // Queue up an object to look at. Post-action because it may not have been created yet.
	    if(lookat_node)
		myActionManager->addLookAtAction(new_node, lookat_node);
	}
	else
	{
	    // For lights/cameras, they have a look at axis that is different from Houdini/Maya that use
	    // the -Z axis. To compensate, we multiply in a post rotation so that we go from Houdini's -Z
	    // to either the +X (for cameras) or -Y (for lights).

	    UT_StringRef node_type = hd_node->getOperator()->getName();
	    if(override_node_type.isstring())
		node_type = override_node_type;

	    FbxVector4 adjustment;
	    if (ROP_FBXUtil::getPostRotateAdjust(node_type, adjustment))
	    {
		// We assume here that either exportFBXTransform() or setStandardTransforms() has already
		// enabled the Rotation Active flag. It needs to be enabled in order for the post rotate
		// to have an effect.
		UT_ASSERT(new_node->GetRotationActive());
		FbxVector4 post_rotate = new_node->GetPostRotation(FbxNode::eSourcePivot);
		ROP_FBXUtil::doPostRotateAdjust(post_rotate, adjustment);
		new_node->SetPostRotation(FbxNode::eSourcePivot, post_rotate);
	    }
	}

	// Add nodes to the map
	res_node_pair_info = &myNodeManager->addNodePair(hd_node, new_node, *node_info);
    }


    // Export materials
    exportMaterials(hd_node, new_node);

    res_node_pair_info->setVertexCacheMethod(node_info->getVertexCacheMethod());
    res_node_pair_info->setMaxObjectPoints(node_info->getMaxObjectPoints());
    res_node_pair_info->setIsSurfacesOnly(node_info->getIsSurfacesOnly());
    res_node_pair_info->setVertexCache(v_cache);
    res_node_pair_info->setVisitResultType(res_type);
    res_node_pair_info->setSourcePrimitive(constr_info.getHdPrimitiveIndex());
    res_node_pair_info->setTraveledInputIndex(node_info->getTraveledInputIndex());
    for (int curr_blend_index = 0; curr_blend_index < node_info->getBlendShapeNodeCount(); curr_blend_index++)
	res_node_pair_info->addBlendShapeNode(node_info->getBlendShapeNodeAt(curr_blend_index));

    // Add it to the hierarchy
    if(!node_info->getIsVisitingFromInstance())
    {
	fbx_parent_node->AddChild(new_node);
	node_info->setFbxNode(new_node);
    }

    // Add custom FBX properties to the node
    ROP_FBXUtil::outputCustomProperties(hd_node, new_node);
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::exportFBXTransform(fpreal t, const OBJ_Node *hd_node, FbxNode *fbx_node)
{
    // NOTE: Notice that shears is missing from these lists because (as of) FBX
    //	     2016.1, they still don't support shears in their local transforms.
    typedef FbxPropertyT<FbxDouble3> FbxDouble3Property;
    FbxDouble3Property* props[] =
    {
	&(fbx_node->LclTranslation),
	&(fbx_node->RotationOffset),
	&(fbx_node->PreRotation),
	&(fbx_node->LclRotation),
	&(fbx_node->PostRotation),
	&(fbx_node->RotationPivot),
	&(fbx_node->ScalingOffset),
	&(fbx_node->LclScaling),
	&(fbx_node->ScalingPivot),
    };
    const char* hd_names[] = 
    {
	"t",
	"roffset",
	"rpre",
	"r",
	"rpost",
	"rpivot",
	"soffset",
	"s",
	"spivot",
    };
    enum
    {
	ROP_FBX_T,
	ROP_FBX_ROFFSET,
	ROP_FBX_RPRE,
	ROP_FBX_R,
	ROP_FBX_RPOST,
	ROP_FBX_RPIVOT,
	ROP_FBX_SOFFSET,
	ROP_FBX_S,
	ROP_FBX_SPIVOT,
	ROP_FBX_N
    };
    SYS_STATIC_ASSERT(SYScountof(hd_names) == ROP_FBX_N);
    SYS_STATIC_ASSERT(SYScountof(hd_names) == SYScountof(props));

    const UT_StringHolder &node_type = hd_node->getOperator()->getName();
    const char* UNIFORM_SCALE = "scale";
    if(node_type == "instance")
    {
	hd_names[ROP_FBX_T] = "i_t";
	hd_names[ROP_FBX_R] = "i_r";
	hd_names[ROP_FBX_S] = "i_s";
	UNIFORM_SCALE = "i_scale";
    }
    else if(node_type == "hlight") // for old hlight 1.0 only
    {
	hd_names[ROP_FBX_T] = "l_t";
	hd_names[ROP_FBX_R] = "l_r";
	hd_names[ROP_FBX_S] = "l_s";
	UNIFORM_SCALE = "l_scale";
    }

    const int thread = SYSgetSTID();
    const PRM_ParmList *plist = hd_node->getParmList();
    for (int i = 0; i < ROP_FBX_N; ++i)
    {
	const char *parm_name = hd_names[i];
	const PRM_Parm *parm = plist->getParmPtr(parm_name);
	if (!parm)
	    continue;
	UT_Vector3R v;
	parm->getValues(t, v.data(), thread);
	if (i == ROP_FBX_S)
	{
	    parm = plist->getParmPtr(UNIFORM_SCALE);
	    if (parm)
	    {
		fpreal s;
		parm->getValue(t, s, 0, thread);
		v *= s;
	    }
	}
	else if (i == ROP_FBX_RPOST)
	{
	    // FBX's Post Rotate is the inverse of Maya's Rotate Axis (and
	    // Houdini's chosen Post Rotate). So we need to invert.
	    const UT_XformOrder order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
	    v.degToRad();
	    UT_Matrix3D mat(1.0);
	    mat.rotate(v, order);
	    mat.transpose();	    // invert rotation matrix
	    mat.crack(v, order);  // smooth wrt (0,0,0) for consistency 
	    UTcrackMatrixSmooth(order, v(0), v(1), v(2), 0, 0, 0);
	    v.radToDeg();
	}
	props[i]->Set(FbxDouble3(v(0), v(1), v(2)));
    }

    // SetRotationActive(true) must be used for the rotation order (as well as
    // pre/post rotations) to be interpreted by the FBX importer.
    fbx_node->SetRotationActive(true);

    // When saving, the pre/post rotations and pivots are only taken from the
    // eSourcePivot set. But we only set the properties above so we need to
    // update the pivots from the properties.
    // NOTE: This is an unofficial API that they say can change with FBX
    //       versions!
    fbx_node->UpdatePivotsAndLimitsFromProperties();

    // Maintain the same rotation order. This logic is relied upon by
    // ROP_FBXAnimVisitor::visit() for cracking rotations!
    EFbxRotationOrder rot_order = ROP_FBXUtil::fbxRotationOrder(OP_Node::getRotOrder(hd_node->XYZ(t)));
    fbx_node->SetRotationOrder(FbxNode::eSourcePivot, rot_order);

    // Houdini only has one inherit type
    fbx_node->SetTransformationInheritType(FbxTransform::eInheritRSrs);
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info)
{
    // We have to check if our previous bone length is not zero. 
    // If it isn't, that means we have an end effector to create.
    ROP_FBXMainNodeVisitInfo* cast_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(last_node_info);
    if (cast_info && cast_info->getBoneLength() > 0.0
	&& ROP_FBXUtil::isDummyBone(last_node) == false
	&& ROP_FBXUtil::isJointNullNode(last_node) == false
	&& myParentExporter->getExportOptions()->getExportBonesEndEffectors())
    {
	// Create the end effector
        UT_String node_name;
        ROP_FBXUtil::getNodeName(last_node, node_name, myNodeManager);
	node_name += "_end_effector";

	FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);
	FbxSkeleton *res_attr = FbxSkeleton::Create(mySDKManager, (const char*)node_name);
	res_node->SetNodeAttribute(res_attr);
	res_attr->SetSkeletonType(FbxSkeleton::eLimbNode);

	setFbxNodeVisibility(*res_node, nullptr, last_node->getVisible());

	// Pass in the bone-length here so that it gets placed at the end of the bone
	ROP_FBXUtil::setStandardTransforms(NULL, res_node, last_node_info, cast_info->getBoneLength(), myStartTime);

	if(last_node_info->getFbxNode())
	    last_node_info->getFbxNode()->AddChild(res_node);
    }
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, bool is_a_null, TFbxNodesVector& res_nodes)
{
    // NOTE: This may get called on a null nodes, as well, if they are being exported as joints.
    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);
    FbxSkeleton *res_attr = FbxSkeleton::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    bool is_root = false;
    if(node_info && node_info->getParentInfo())
    {
	if(UTverify_cast<ROP_FBXMainNodeVisitInfo*>(node_info->getParentInfo())->getBoneLength() <= 0.0)
	    is_root = true;
    }

    if(is_root)
	res_attr->SetSkeletonType(FbxSkeleton::eRoot);
    else
	res_attr->SetSkeletonType(FbxSkeleton::eLimbNode);//eLimb);

    // Get the bone's length
    fpreal bone_length = 0.0;
    if(is_a_null)
	bone_length = 1.0; // Some dummy value so the next joint knows it's not a root.
    else
	bone_length = ROP_FBXUtil::getFloatOPParm(node, "length", 0, myStartTime);
    node_info->setBoneLength(bone_length);

    res_nodes.push_back(res_node);
    return true;
}
/********************************************************************************************************/
/*
Thoughts on multi types:
- Anything that is vertex cacheable shall be of one monolithic type. If it's not, it shall be converted to
    polygons. In practice, we can limit this to: Either it is 1) Pure NURBS or 2) Convert it all to polygons.
- The above means we only worry about multiple types when they're non-vc. However, we still have to take caching 
into account.
- Remember that we can be here in an instance.

- For all other objects, we need to break them apart...?
*/
bool
ROP_FBXMainVisitor::outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, ROP_FBXGDPCache *&v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes)
{
    did_cancel_out = false;

    OP_Network* op_net = dynamic_cast<OP_Network*>(node);
    if(!op_net)
	return false;
    OP_Node *rend_node = myParentExporter->getExportOptions()->isSopExport() ? node : op_net->getRenderNodePtr();
    if(!rend_node)
	return false;
    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(rend_node);
    if(!sop_node)
	return false;

    bool temp_bool, found_particles;
    bool is_vertex_cacheable = false;
    OP_Node* skin_deform_node = NULL;
    OP_Node* blend_shape_node = NULL;

    temp_bool = ROP_FBXUtil::isVertexCacheable(op_net, myParentExporter->getExportOptions()->getExportDeformsAsVC(), myStartTime, found_particles, myParentExporter->getExportOptions()->isSopExport() );
    if(myParentExporter->getExportingAnimation() || found_particles)
	is_vertex_cacheable = temp_bool;

    bool force_skin_deform = myParentExporter->getExportOptions()->getForceSkinDeformExport();
    bool look_for_skin_deform_node = myParentExporter->getExportOptions()->getExportDeformsAsVC() == false && !is_vertex_cacheable;
    if (!look_for_skin_deform_node && force_skin_deform)
	look_for_skin_deform_node = true;

    if(look_for_skin_deform_node)
    {
	// For now, only export skinning if we're not vertex cacheable.
	bool did_find_allowed_nodes_only = false;
	const char *const skin_node_types[] = { "bonedeform", "deform", 0};
	skin_deform_node = ROP_FBXUtil::findOpInput(rend_node, skin_node_types, true, ROP_FBXallowed_inbetween_node_types, &did_find_allowed_nodes_only);

	// Forcing will ignore the other nodes that deforms the mesh
	if (skin_deform_node && force_skin_deform)
	    did_find_allowed_nodes_only = true;

	if(!did_find_allowed_nodes_only && skin_deform_node)
	{
	    // We've found a deform node, but also other nodes that deform the mesh between this node and the deform node.
	    // Output as a vertex cache instead.
	    is_vertex_cacheable = true;
	    skin_deform_node = NULL;

	    // TODO: Set a flag here telling the vertex cache not to break up the mesh into individual triangles.
	}
    }

    bool blend_shapes_out_only = false;
    bool force_blend_shape = myParentExporter->getExportOptions()->getForceBlendShapeExport();
    bool look_for_blend_shape_node = myParentExporter->getExportOptions()->getExportDeformsAsVC() == false;// && !is_vertex_cacheable;
    if (!look_for_blend_shape_node && force_blend_shape)
	look_for_blend_shape_node = true;

    if (look_for_blend_shape_node)
    {
	// For now, only export blend shapes if we're not vertex cacheable.
	bool did_find_allowed_nodes_only = false;

	blend_shape_node = ROP_FBXUtil::findOpInput(rend_node, theBlendShapeNodeTypes, true, theAllowedInBetweenNodeTypes, &did_find_allowed_nodes_only);
	
	// Forcing will ignore the other nodes that modifies the mesh
	if ( blend_shape_node && force_blend_shape )
	    did_find_allowed_nodes_only = true;

	// We've found a blend_shape node, and there is no other nodes that modifies the mesh 
	// between this node and the blend shape node, so exporting the blend shape is sufficient...
	if (blend_shape_node &&  did_find_allowed_nodes_only)
	    blend_shapes_out_only = true;
    }

    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    if (blend_shapes_out_only)
    {
	return outputBlendShapesNodesIn(rend_node, node_name, skin_deform_node, did_cancel_out, res_nodes, NULL, node_info);
    }	

    if (is_vertex_cacheable)
    {
	return outputSOPNodeWithVC(sop_node, node_name, node_info, v_cache_out, did_cancel_out, res_nodes);
    }
    else
    {
	v_cache_out = NULL;
	return outputSOPNodeWithoutVC(sop_node, node_name, skin_deform_node, did_cancel_out, res_nodes);
    }

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputSOPNodeWithVC(SOP_Node* sop_node, const UT_String& node_name, ROP_FBXMainNodeVisitInfo* node_info, ROP_FBXGDPCache *&v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes)
{
    if (!sop_node)
	return false;

    int max_vc_verts = 0;
    fpreal start_time = myParentExporter->getStartTime();
    fpreal end_time = myParentExporter->getEndTime();
    fpreal geom_export_time = start_time;
    fpreal capture_frame = CHgetFrameFromTime(start_time);

    // We need this here so that the number of points in the static geometry
    // matches the number of points in the vertex cache files. Otherwise Maya
    // crashes and burns.
    bool is_pure_surfaces = false;
#ifdef UT_DEBUG
    double max_vert_cnt_start, max_vert_cnt_end;
    max_vert_cnt_start = clock();
#endif
    v_cache_out = new ROP_FBXGDPCache();
    v_cache_out->setSaveMemory(myParentExporter->getExportOptions()->getSaveMemory());

    OP_Node* node_to_use;
    if (node_info->getIsVisitingFromInstance() && node_info->getParentInfo())
	node_to_use = node_info->getParentInfo()->getHdNode();
    else
	node_to_use = sop_node;

    max_vc_verts = ROP_FBXUtil::getMaxPointsOverAnimation(node_to_use, geom_export_time, end_time,
	myParentExporter->getExportOptions()->getPolyConvertLOD(),
	myParentExporter->getExportOptions()->getDetectConstantPointCountObjects(),
	myParentExporter->getExportOptions()->getConvertSurfaces(),
	myBoss, v_cache_out, is_pure_surfaces);

    if (max_vc_verts < 0)
    {
	// The user cancelled.
	delete v_cache_out;
	did_cancel_out = true;
	return false;
    }

#ifdef UT_DEBUG
    max_vert_cnt_end = clock();
    ROP_FBXdb_maxVertsCountingTime += (max_vert_cnt_end - max_vert_cnt_start);
#endif

    GU_DetailHandle gdh;
    OP_Context	    context(geom_export_time);
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh))
	return false;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail		*gdp = gdl.getGdp();
    if (!gdp)
	return false;

    // See what types we have in our GDP
    GA_PrimCompat::TypeMask prim_type = ROP_FBXUtil::getGdpPrimId(gdp);

    FbxNodeAttribute *res_attr = NULL;
    if (v_cache_out && v_cache_out->getNumFrames() > 0)
    {
	// We can only cache these:
	// 1) Pure polygons;
	// 2) Pure NURBS;
	// 3) Pure Beziers;
	// 4) Anything else, converted to polygons.
	if ((prim_type == GEO_PrimTypeCompat::GEOPRIMPOLY || prim_type == GEO_PrimTypeCompat::GEOPRIMMESH || prim_type == (GEO_PrimTypeCompat::GEOPRIMPOLY | GEO_PrimTypeCompat::GEOPRIMMESH)) && v_cache_out->getIsNumPointsConstant())
	{
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometryConstant);
	    res_attr = outputPolygons(gdp, (const char*)node_name, 0, ROP_FBXVertexCacheMethodGeometryConstant);
	    finalizeGeoNode(res_attr, NULL, capture_frame, -1, res_nodes);
	}
	else if (prim_type == GEO_PrimTypeCompat::GEOPRIMPART)
	{
	    // Particles.
	    // We cleverly create a square for each particle, then. Use the cache.
	    GU_Detail *final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodParticles);
	    res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());
	    finalizeGeoNode(res_attr, NULL, capture_frame, -1, res_nodes);
	}
	else if (is_pure_surfaces && v_cache_out->getIsNumPointsConstant())
	{
	    // NURBS or Bezier surfaces
	    //GU_Detail *final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometryConstant);
	    node_info->setIsSurfacesOnly(true);

	    // Note: unfortunately, the order of these is important, and matters to the ROP_FBXAnimVisitor::fillVertexArray().
	    int prim_cntr = -1;
	    if (prim_type & GEO_PrimTypeCompat::GEOPRIMNURBSURF)
		outputNURBSSurfaces(gdp, (const char*)node_name, NULL, capture_frame, res_nodes, &prim_cntr);
	    if (prim_type & GEO_PrimTypeCompat::GEOPRIMBEZSURF)
		outputBezierSurfaces(gdp, (const char*)node_name, NULL, capture_frame, res_nodes, &prim_cntr);
	    if (prim_type & GEO_PrimTypeCompat::GEOPRIMBEZCURVE)
		outputBezierCurves(gdp, (const char*)node_name, NULL, capture_frame, res_nodes, &prim_cntr);
	    if (prim_type & GEO_PrimTypeCompat::GEOPRIMNURBCURVE)
		outputNURBSCurves(gdp, (const char*)node_name, NULL, capture_frame, res_nodes, &prim_cntr);
	}
	else // Mixed types
	{
	    // Convert
	    GU_Detail *final_detail;
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometry);
	    final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());
	    finalizeGeoNode(res_attr, NULL, capture_frame, -1, res_nodes);
	}
    }
    
    if ( v_cache_out && v_cache_out->getNumFrames() > 0 )
    {
	// Cache this number so we don't painfully recompute it again when we get to
	// vertex caching.
	if (node_info->getVertexCacheMethod() == ROP_FBXVertexCacheMethodGeometryConstant)
	    node_info->setMaxObjectPoints(v_cache_out->getNumConstantPoints());
	else
	    node_info->setMaxObjectPoints(max_vc_verts);
    }

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputSOPNodeWithoutVC( SOP_Node* sop_node, const UT_String& node_name, OP_Node* skin_deform_node,
					    bool& did_cancel_out, TFbxNodesVector& res_nodes)
{
    if (!sop_node)
	return false;

    fpreal geom_export_time = myParentExporter->getStartTime(); 
    fpreal capture_frame = CHgetFrameFromTime(geom_export_time);
    if (skin_deform_node)
    {
	// We're skinnable. Find the capture frame.
	const char *const capt_skin_node_types[] = { CAPT_SKIN_NODE_TYPES, nullptr };
	OP_Node *capture_node = ROP_FBXUtil::findOpInput(skin_deform_node, capt_skin_node_types, true, NULL, NULL);
	if (capture_node)
	{
	    if (ROP_FBXUtil::getIntOPParm(capture_node, "usecaptpose") == false)
	    {
		capture_frame = ROP_FBXUtil::getFloatOPParm(capture_node, "captframe");
	    }
	}

	geom_export_time = CHgetManager()->getTime(capture_frame);

	if (skin_deform_node->getInput(0))
	{
	    sop_node = CAST_SOPNODE(skin_deform_node->getInput(0));
	    UT_ASSERT(sop_node);
	}
    }
    
    GU_DetailHandle gdh;
    OP_Context	    context(geom_export_time);
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh))
	return false;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail		*gdp = gdl.getGdp();
    if (!gdp)
	return false;

    // See what types we have in our GDP
    GA_PrimCompat::TypeMask prim_type = ROP_FBXUtil::getGdpPrimId(gdp);

    FbxNodeAttribute *res_attr = NULL;

    GU_Detail conv_gdp;
    const GU_Detail* final_detail = getExportableGeo(gdp, conv_gdp, prim_type);

    // No vertex caching. Output several separate nodes
    if (prim_type & GEO_PrimTypeCompat::GEOPRIMPOLY)
    {
	// There are polygons in this gdp. Output them.
	res_attr = outputPolygons(final_detail, (const char*)node_name, 0, ROP_FBXVertexCacheMethodNone);
	finalizeGeoNode(res_attr, skin_deform_node, capture_frame, -1, res_nodes);

	// Try output any polylines, if they exist. Unlike Houdini, they're a separate type in FBX.
	// We ignore them in the about polygon function.
	outputPolylines(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
    }
    if (prim_type & GEO_PrimTypeCompat::GEOPRIMNURBSURF)
	outputNURBSSurfaces(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
    if (prim_type & GEO_PrimTypeCompat::GEOPRIMNURBCURVE)
	outputNURBSCurves(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
    if (prim_type & GEO_PrimTypeCompat::GEOPRIMBEZCURVE)
	outputBezierCurves(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
    if (prim_type & GEO_PrimTypeCompat::GEOPRIMBEZSURF)
	outputBezierSurfaces(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);

    return true;
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::finalizeGeoNode(FbxNodeAttribute *res_attr, OP_Node* skin_deform_node, 
				    int capture_frame, int opt_prim_cnt, TFbxNodesVector& res_nodes)
{
    if(!res_attr)
	return;

    // The attribute name is already guaranteed to be unique.
    FbxNode* res_node = FbxNode::Create(mySDKManager, res_attr->GetName());
    res_node->SetNodeAttribute(res_attr);

    ROP_FBXConstructionInfo constr_info(res_node);
    constr_info.setHdPrimitiveIndex(opt_prim_cnt);
    res_nodes.push_back(constr_info);

    if(skin_deform_node)
    {
	myActionManager->addSkinningAction(res_node, skin_deform_node, capture_frame);
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::outputBezierSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, 
					 int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr)
{
    UT_String orig_name(node_name, UT_String::ALWAYS_DEEP);
    orig_name += "_bezier_surf";
    UT_String curr_name(UT_String::ALWAYS_DEEP);
    int obj_cntr = 0;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    GA_ElementWranglerCache	 wranglers(copy_gdp,
					   GA_PointWrangler::EXCLUDE_P);

    int prim_cnt = -1;
    if(prim_cntr)
	prim_cnt = *prim_cntr;

    GEO_Primitive* prim;
    GA_FOR_ALL_PRIMITIVES(&copy_gdp, prim)
    {
	if(prim->getTypeId() != GA_PRIMBEZSURF)
	    continue;

	if(prim_cntr)
	    prim_cnt++;

	GU_PrimRBezSurf *hd_line = (GU_PrimRBezSurf*)prim;

	GU_PrimNURBSurf *hd_nurb = static_cast<GU_PrimNURBSurf*>(hd_line->convertToNURBNew(
								    wranglers));
	if(!hd_nurb)
	    continue;

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	outputSingleNURBSSurface(hd_nurb, curr_name, skin_deform_node, capture_frame, res_nodes, prim_cnt);

    }

    if(prim_cntr)
	*prim_cntr = prim_cnt;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::outputBezierCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame,
				       TFbxNodesVector& res_nodes, int* prim_cntr)
{
    UT_String orig_name(node_name, UT_String::ALWAYS_DEEP);
    orig_name += "_bezier_curve";
    UT_String curr_name(UT_String::ALWAYS_DEEP);
    int obj_cntr = 0;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    GA_ElementWranglerCache wranglers(copy_gdp, GA_PointWrangler::EXCLUDE_P);

    int prim_cnt = -1;
    if (prim_cntr)
	prim_cnt = *prim_cntr;

    GEO_Primitive *prim;
    GA_FOR_ALL_PRIMITIVES(&copy_gdp, prim)
    {
	if (prim->getTypeId() != GA_PRIMBEZCURVE)
	    continue;

	prim_cnt++;

	GU_PrimRBezCurve *hd_line = static_cast<GU_PrimRBezCurve*>(prim);

	GU_PrimNURBCurve *hd_nurb = static_cast<GU_PrimNURBCurve*>(hd_line->convertToNURBNew(wranglers));
	if (!hd_nurb)
	    continue;

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	FbxNurbsCurve *nurbs_curve_attr = FbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, skin_deform_node, capture_frame, prim_cnt, res_nodes);

    }

    if (prim_cntr)
	*prim_cntr = prim_cnt;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::outputPolylines(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, int capture_frame, TFbxNodesVector& res_nodes)
{
    UT_String orig_name(node_name, UT_String::ALWAYS_DEEP);
    orig_name += "_polyline";
    UT_String curr_name(UT_String::ALWAYS_DEEP);
    int obj_cntr = 0;

    bool did_find_open = false;

    const GEO_Primitive* const_prim;
    GA_FOR_ALL_PRIMITIVES(gdp, const_prim)
    {
        if (const_prim->getTypeId() != GA_PRIMPOLY)
            continue;
	const GU_PrimPoly *const_hd_line = static_cast<const GU_PrimPoly*>(const_prim);
	if (const_hd_line->isClosed() == false)
	{
	    did_find_open = true;
	    break;
	}
    }

    if(!did_find_open)
	return;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    GA_ElementWranglerCache wranglers(copy_gdp, GA_PointWrangler::EXCLUDE_P);

    int prim_cnt = -1;
    for (GA_Index curr_prim = copy_gdp.getNumPrimitives() - 1; curr_prim >= 0; --curr_prim)
    {
	prim_cnt++;
	GEO_Primitive *prim = copy_gdp.getGEOPrimitive(copy_gdp.primitiveOffset(curr_prim));
	if (prim->getTypeId() != GA_PRIMPOLY)
	    continue;

	GU_PrimPoly *hd_line = static_cast<GU_PrimPoly*>(prim);
	if(hd_line->isClosed())
	    continue;

	GU_PrimNURBCurve *hd_nurb = static_cast<GU_PrimNURBCurve*>(hd_line->convertToNURBNew(
								wranglers, 4));
	if(!hd_nurb)
	    continue;

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	FbxNurbsCurve *nurbs_curve_attr = FbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, skin_deform_node, capture_frame, prim_cnt, res_nodes);
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::outputNURBSCurves(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node, 
				      int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr)
{
    UT_String orig_name(node_name, UT_String::ALWAYS_DEEP);
    orig_name += "_nurbs_curve";
    UT_String curr_name(UT_String::ALWAYS_DEEP);
    int obj_cntr = 0;
    FbxNurbsCurve *nurbs_curve_attr;
    
    int prim_cnt = -1;
    if (prim_cntr)
	prim_cnt = *prim_cntr;

    const GEO_Primitive* prim;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMNURBCURVE)
            continue;
	prim_cnt++;
	const GU_PrimNURBCurve *hd_nurb = static_cast<const GU_PrimNURBCurve*>(prim);

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	nurbs_curve_attr = FbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, skin_deform_node, capture_frame, prim_cnt, res_nodes);
    }

    if (prim_cntr)
	*prim_cntr = prim_cnt;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::setNURBSCurveInfo(FbxNurbsCurve* nurbs_curve_attr, const GU_PrimNURBCurve* hd_nurb)
{
    nurbs_curve_attr->SetDimension(FbxNurbsCurve::e3D);

    const GA_NUBBasis *basis = static_cast<const GA_NUBBasis *>(hd_nurb->getBasis());
    GA_Size point_count = hd_nurb->getFastVertexCount();

    FbxNurbsCurve::EType curve_type;
    if(hd_nurb->isClosed())
    {
	if(basis->getEndInterpolation())
	    curve_type = FbxNurbsCurve::eClosed;
	else
	    curve_type = FbxNurbsCurve::ePeriodic;
    }
    else
	curve_type = FbxNurbsCurve::eOpen;

    nurbs_curve_attr->SetOrder(hd_nurb->getOrder());
    nurbs_curve_attr->InitControlPoints(point_count, curve_type);

    // Set the basis
    int num_knots = nurbs_curve_attr->GetKnotCount();
    double *knot_vector = nurbs_curve_attr->GetKnotVector();
    const GA_KnotVector &hd_knot_vector = basis->getKnotVector();
    for (int curr_knot = 0; curr_knot < num_knots; curr_knot++)
	knot_vector[curr_knot] = hd_knot_vector(curr_knot);


    FbxVector4 *fbx_points = nurbs_curve_attr->GetControlPoints();
    int num_points = nurbs_curve_attr->GetControlPointsCount();

    const GA_Detail &detail = hd_nurb->getDetail();
    for (int curr_point = 0; curr_point < num_points; curr_point++)
    {
	UT_Vector4 temp_vec = detail.getPos4(hd_nurb->getPointOffset(curr_point));
	fbx_points[curr_point].Set(temp_vec[0],temp_vec[1],temp_vec[2],temp_vec[3]);
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::setTrimRegionInfo(GD_TrimRegion* region, FbxTrimNurbsSurface *trim_nurbs_surf_attr,
				      bool& have_fbx_region)
{
    GD_TrimLoop* loop;
    FbxBoundary* fbx_boundary;

    UT_BoundingRect brect(0,0,1,1);
    loop = region->getLoop(brect);

    // Add a boundary
    GD_TrimLoop* curr_loop = loop;
    GD_TrimPiece* curr_piece;

    bool is_clockwise;
    FbxNurbsCurve* fbx_curve;
    while(curr_loop) 
    {
	is_clockwise = curr_loop->isClockwise();
	if(!is_clockwise)
	{
	    if(have_fbx_region)
		trim_nurbs_surf_attr->EndTrimRegion();
	    trim_nurbs_surf_attr->BeginTrimRegion();
	    have_fbx_region = true;
	}
//	if( (is_outer && is_clockwise) || (!is_outer && !is_clockwise))
//	    curr_loop->reverse();

	curr_loop->flatten();

	if(!curr_loop->isClosed())
	    curr_loop->close(1);

	fbx_boundary = FbxBoundary::Create(mySDKManager, "");
	curr_piece = curr_loop->getPiece(NULL);
	while (curr_piece)
	{
	    fbx_curve = FbxNurbsCurve::Create(mySDKManager, "");

	    unsigned face_id = curr_piece->getPrimitiveTypeId();

	    if (face_id == GD_PRIMBEZCURVE)
	    {
		GU_Detail temp_gdp;
		GU_PrimNURBSurf *temp_prim = static_cast<GU_PrimNURBSurf *>(temp_gdp.appendPrimitive(GEO_PRIMNURBSURF));
		GEO_Profiles* profiles = temp_prim->profiles(1);

		GD_PrimRBezCurve* temp_face = dynamic_cast<GD_PrimRBezCurve*>(curr_piece->createFace(profiles));

		// Make a copy of the face and convert it to the GU_* Curve. We can then dump it to NURBS.
		if (temp_face)
		{
		    GU_PrimRBezCurve* bez_curve = GU_PrimRBezCurve::build(&temp_gdp,  temp_face->getVertexCount(), temp_face->getOrder(), temp_face->isClosed(), true);
		    UT_ASSERT(bez_curve);

		    // Copy the points
		    GA_Size nvertices = temp_face->getVertexCount();
		    for (GA_Size vertex = 0; vertex < nvertices; ++vertex)
		    {
			UT_Vector3 temp_vec = profiles->getPos3(temp_face->getPointOffset(vertex));
			temp_vec.z() = 0;
			temp_gdp.setPos3(bez_curve->getPointOffset(vertex), temp_vec);
		    }

		    // Convert it to NURBS
		    GA_ElementWranglerCache wranglers(temp_gdp, GA_PointWrangler::EXCLUDE_P);
		    GU_PrimNURBCurve* hd_nurb = static_cast<GU_PrimNURBCurve*>(bez_curve->convertToNURBNew(wranglers));
		    setNURBSCurveInfo(fbx_curve, hd_nurb);
		}
	    }
	    else if (face_id == GD_PRIMPOLY)
	    {
		GU_Detail temp_gdp;
		GU_PrimNURBSurf *temp_prim = static_cast<GU_PrimNURBSurf *>(temp_gdp.appendPrimitive(GEO_PRIMNURBSURF));
		GEO_Profiles* profiles = temp_prim->profiles(1);

		GD_PrimPoly* temp_face;
		temp_face = dynamic_cast<GD_PrimPoly*>(curr_piece->createFace(profiles));

		if (temp_face)
		{
		    GU_PrimPoly* poly = GU_PrimPoly::build(&temp_gdp, temp_face->getVertexCount(), (temp_face->isClosed() ? GU_POLY_CLOSED : GU_POLY_OPEN), true);
		    if(poly)
		    {
			// Copy the points
			GA_Size nvertices = temp_face->getVertexCount();
			for (GA_Size vertex = 0; vertex < nvertices; ++vertex)
			{
			    UT_Vector3 temp_vec = profiles->getPos3(temp_face->getPointOffset(vertex));
			    temp_vec.z() = 0;
			    temp_gdp.setPos3(poly->getPointOffset(vertex), temp_vec);
			}
			// Convert it to NURBS
			GA_ElementWranglerCache wranglers(temp_gdp, GA_PointWrangler::EXCLUDE_P);
			GU_PrimNURBCurve* hd_nurb = static_cast<GU_PrimNURBCurve*>(poly->convertToNURBNew(wranglers, 4));
			setNURBSCurveInfo(fbx_curve, hd_nurb);
		    }
		}

	    }
	    else
	    {
		// Unknown trim curve type
		UT_ASSERT(0);
	    }

	    fbx_boundary->AddCurve(fbx_curve);
	    curr_piece = curr_loop->getPiece(curr_piece);
	}

	trim_nurbs_surf_attr->AddBoundary(fbx_boundary);
	curr_loop = loop->getNext();
    }         

}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::outputNURBSSurfaces(const GU_Detail* gdp, const char* node_name, OP_Node* skin_deform_node,
					int capture_frame, TFbxNodesVector& res_nodes, int* prim_cntr)
{
    UT_String orig_name(node_name, UT_String::ALWAYS_DEEP);
    orig_name += "_nurbs_surf";
    UT_String curr_name(UT_String::ALWAYS_DEEP);
    int obj_cntr = 0;

    const GEO_Primitive* prim;
    bool have_profiles = false;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMNURBSURF)
            continue;
	const GU_PrimNURBSurf* hd_nurb = static_cast<const GU_PrimNURBSurf*>(prim);
	if (hd_nurb->hasProfiles())
	{
	    have_profiles = true;
	    break;
	}
    }

    const GU_Detail *final_gdp;
    GU_Detail copy_gdp;
    if(have_profiles)
    {
	copy_gdp.duplicate(*gdp);
	final_gdp = &copy_gdp;
    }
    else
	final_gdp = gdp;

    int prim_cnt = -1;
    if(prim_cntr)
	prim_cnt = *prim_cntr;

    GA_FOR_ALL_PRIMITIVES(final_gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMNURBSURF)
            continue;

	if (prim_cntr)
	    prim_cnt++;

	const GU_PrimNURBSurf* hd_nurb = static_cast<const GU_PrimNURBSurf*>(prim);

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	outputSingleNURBSSurface(hd_nurb, curr_name, skin_deform_node, capture_frame, res_nodes, prim_cnt);

    }

    if (prim_cntr)
	*prim_cntr = prim_cnt;
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::outputSingleNURBSSurface(const GU_PrimNURBSurf* hd_nurb, const char* curr_name, OP_Node* skin_deform_node, 
					     int capture_frame, TFbxNodesVector& res_nodes, int prim_cnt)
{
    FbxTrimNurbsSurface *trim_nurbs_surf_attr;
    FbxNurbsSurface *nurbs_surf_attr;
    GU_PrimNURBSurf* nc_hd_nurb;

    // Output each NURB   
    if(hd_nurb->hasProfiles())
    {
	string temp_name(curr_name);
	temp_name += "_trim_surf";
	nurbs_surf_attr = FbxNurbsSurface::Create(mySDKManager, temp_name.c_str());
	trim_nurbs_surf_attr = FbxTrimNurbsSurface::Create(mySDKManager, curr_name);	
	trim_nurbs_surf_attr->SetNurbsSurface(nurbs_surf_attr);
    }
    else
	nurbs_surf_attr = FbxNurbsSurface::Create(mySDKManager, curr_name);

    // Set the main surface
    setNURBSSurfaceInfo(nurbs_surf_attr, hd_nurb);

    // Set the boundaries
    if(hd_nurb->hasProfiles())
    {
	// We're guaranteed we're only here if this is a local copy
	// of the gdp, so we can actually modify it. So this isn't
	// as evil as it appears at first.
	nc_hd_nurb = const_cast<GU_PrimNURBSurf*>(hd_nurb);
	GD_TrimRegion* region;
	GEO_Profiles* profiles  = nc_hd_nurb->profiles();
	bool have_fbx_region = false;

	// Make sure we have an outer loop
	int curr_region, num_regions = profiles->trimRegions().entries();

	// Output in reverse, starting a new FBX region for every counterclockwise 
	// profile loop we find. In FBX, outer loops have to be output first.
	for(curr_region = num_regions - 1; curr_region >= 0; curr_region--)
	{
	    region = profiles->trimRegions()(curr_region);
	    setTrimRegionInfo(region, trim_nurbs_surf_attr, have_fbx_region);
	}

	// End the FBX region
	if(have_fbx_region)
	    trim_nurbs_surf_attr->EndTrimRegion();

    }

    if(hd_nurb->hasProfiles())
	finalizeGeoNode(trim_nurbs_surf_attr, skin_deform_node, capture_frame, prim_cnt, res_nodes);
    else
	finalizeGeoNode(nurbs_surf_attr, skin_deform_node, capture_frame, prim_cnt, res_nodes);

}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::setNURBSSurfaceInfo(FbxNurbsSurface *nurbs_surf_attr, const GU_PrimNURBSurf* hd_nurb)
{
    GA_NUBBasis* curr_u_basis = static_cast<GA_NUBBasis*>(hd_nurb->getUBasis());
    GA_NUBBasis* curr_v_basis = static_cast<GA_NUBBasis*>(hd_nurb->getVBasis());
    int v_point_count = hd_nurb->getNumRows();
    int u_point_count = hd_nurb->getNumCols();

    // Determine types
    FbxNurbsSurface::EType u_type;
    if(hd_nurb->isWrappedU())
    {
	if(curr_u_basis->getEndInterpolation())
	    u_type = FbxNurbsSurface::eClosed;
	else
	    u_type = FbxNurbsSurface::ePeriodic;
    }
    else
	u_type = FbxNurbsSurface::eOpen;

    FbxNurbsSurface::EType v_type;
    if(hd_nurb->isWrappedV())
    {
	if(curr_v_basis->getEndInterpolation())
	    v_type = FbxNurbsSurface::eClosed;
	else
	    v_type = FbxNurbsSurface::ePeriodic;
    }
    else
	v_type = FbxNurbsSurface::eOpen;

    nurbs_surf_attr->SetOrder(hd_nurb->getUOrder(), hd_nurb->getVOrder());
    nurbs_surf_attr->SetStep(2, 2);
    nurbs_surf_attr->InitControlPoints(u_point_count, u_type, v_point_count, v_type);

    // Set bases (which are all belong to us...)
    int num_uknots = nurbs_surf_attr->GetUKnotCount();
    double *uknot_vector = nurbs_surf_attr->GetUKnotVector();
    const GA_KnotVector &hd_uknot_vector = curr_u_basis->getKnotVector();
    for (int curr_uknot = 0; curr_uknot < num_uknots; ++curr_uknot)
	uknot_vector[curr_uknot] = hd_uknot_vector(curr_uknot);

    int num_vknots = nurbs_surf_attr->GetVKnotCount();
    double *vknot_vector = nurbs_surf_attr->GetVKnotVector();
    const GA_KnotVector &hd_vknot_vector = curr_v_basis->getKnotVector();
    for (int curr_vknot = 0; curr_vknot < num_vknots; ++curr_vknot)
	vknot_vector[curr_vknot] = hd_vknot_vector(curr_vknot);

    // Set control points
    FbxVector4* fbx_points = nurbs_surf_attr->GetControlPoints();
    const GA_Detail &detail = hd_nurb->getDetail();
    int i_idx = 0;
    for (int i_row = 0; i_row < v_point_count; ++i_row)
    {
	for (int i_col = 0; i_col < u_point_count; ++i_col)
	{
	    UT_Vector4 temp_vec = detail.getPos4(hd_nurb->getPointOffset(i_row, i_col));
	    fbx_points[i_idx++].Set(temp_vec[0],temp_vec[1],temp_vec[2],temp_vec[3]);
	}
    }
}
/********************************************************************************************************/
FbxNodeAttribute*
ROP_FBXMainVisitor::outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method)
{
    FbxMesh* mesh_attr = FbxMesh::Create(mySDKManager, node_name);

    int points_per_poly = 0;
    if(vc_method == ROP_FBXVertexCacheMethodGeometry)
	points_per_poly = 3;
    else if(vc_method == ROP_FBXVertexCacheMethodParticles)
	points_per_poly = ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT;

    // Get the number of points
    int num_points = gdp->getNumPoints();
    if(max_points < num_points)
	max_points = num_points;
    mesh_attr->InitControlPoints(max_points); //  + 1);
    FbxVector4* fbx_control_points = mesh_attr->GetControlPoints();

    {
        GA_Index curr_point(0);
        GA_Offset ptoff;
        GA_FOR_ALL_PTOFF(gdp, ptoff)
        {
            UT_Vector4 pos = gdp->getPos4(ptoff);
            fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);
            curr_point++;
        }
    }

    if (num_points > 2 && max_points > num_points)
    {
	for (int curr_point = num_points; curr_point < max_points; curr_point++)
	{
	    //UT_Vector4 pos = gdp->getPos4(gdp->pointOffset((curr_point - num_points) % num_points));
	    //fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);
	    fbx_control_points[curr_point].Set(0,0,0);
	}
    }
    // And the last one:
    //fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);

    const GEO_Primitive* prim;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMMESH)
            continue;

	const GEO_Hull *hull = (const GEO_Hull*)prim;

	int rows = hull->getNumRows();
	int cols = hull->getNumCols();
	int wrapr = hull->isWrappedV() ? rows : rows-1;
	int wrapc = hull->isWrappedU() ? cols : cols-1;

	int r,c,c1, r1;
	for (r = 0; r < wrapr; r++)
	{
	    r1 = (r+1) % rows;
	    for (c = 0; c < wrapc; c++)
	    {
		c1 = (c+1) % cols;
		GA_Index p0 = hull->getPointIndex(r,  c );
		GA_Index p1 = hull->getPointIndex(r1, c );
		GA_Index p2 = hull->getPointIndex(r1, c1);
		GA_Index p3 = hull->getPointIndex(r,  c1);

		mesh_attr->BeginPolygon();
		mesh_attr->AddPolygon(p0);
		mesh_attr->AddPolygon(p1);
		mesh_attr->AddPolygon(p2);
		mesh_attr->AddPolygon(p3);
		mesh_attr->EndPolygon();
	    }
	}
    }

    // Now set vertices
    int curr_vert, num_verts;
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getTypeId() != GA_PRIMPOLY)
            continue;

	if (((const GEO_PrimPoly*)prim)->isClosed())
	{
	    mesh_attr->BeginPolygon();
	    num_verts = prim->getVertexCount();
	    for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
		mesh_attr->AddPolygon(prim->getPointIndex(curr_vert));
	    mesh_attr->EndPolygon();
	}
    }

    // Add dummy prims if we have to use the extra vertices available
    if(points_per_poly > 0)
    {
	int curr_extra_vert;
	for(curr_extra_vert = num_points; curr_extra_vert < max_points; curr_extra_vert+=points_per_poly)
	{
	    mesh_attr->BeginPolygon();
	    num_verts = points_per_poly;
	    for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
		mesh_attr->AddPolygon(curr_extra_vert + curr_vert);
	    mesh_attr->EndPolygon();
	}
    }

    // Now do attributes, or at least some of them
    exportAttributes(gdp, mesh_attr);
    return mesh_attr;
}
/********************************************************************************************************/
ROP_FBXAttributeType 
ROP_FBXMainVisitor::getAttrTypeByName(const GU_Detail* gdp, const char* attr_name)
{
    ROP_FBXAttributeType curr_type = ROP_FBXAttributeUser;

    // Get the name without any numerical suffixes
    UT_String curr_attr_name(attr_name);
    UT_String base_name;
    curr_attr_name.base(base_name);

    // Now compare the base name against known standard names 
    if (GA_Names::N == base_name)
	curr_type = ROP_FBXAttributeNormal;
    else if (base_name == "tangentu")
        curr_type = ROP_FBXAttributeTangent;
    else if (base_name == "tangentv")
        curr_type = ROP_FBXAttributeBinormal;
    else if (GA_Names::uv == base_name)
	curr_type = ROP_FBXAttributeUV;
    else if (GA_Names::Cd == base_name)
	curr_type = ROP_FBXAttributeVertexColor;

    return curr_type;
}
/********************************************************************************************************/
// Template support functions
inline void ROP_FBXassignValues(const UT_Vector3& hd_vec3, FbxVector4& fbx_vec4, float* extra_val)
{
    fbx_vec4.Set(hd_vec3[0],hd_vec3[1],hd_vec3[2]);
}
inline void ROP_FBXassignValues(const UT_Vector3& hd_vec3, FbxVector2& fbx_vec2, float* extra_val)
{
    fbx_vec2.Set(hd_vec3[0],hd_vec3[1]);
}
inline void ROP_FBXassignValues(const UT_Vector3& hd_col, FbxColor& fbx_col, float* extra_val)
{
    if(extra_val)
	fbx_col.Set(hd_col[0],hd_col[1],hd_col[2],*extra_val);
    else
	fbx_col.Set(hd_col[0],hd_col[1],hd_col[2]);
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportPointAttribute(const GU_Detail *gdp, const GA_ROHandleT<HD_TYPE> &attrib, const GA_ROHandleF &extra_attrib, FbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if (!gdp || !layer_elem || attrib.isInvalid())
	return;

    // Go over all points
    HD_TYPE hd_type;
    FBX_TYPE fbx_type;

    GA_Offset ptoff;
    GA_FOR_ALL_PTOFF(gdp, ptoff)
    {
	hd_type = attrib.get(ptoff);
	if (extra_attrib.isValid())
	{
	    float extra_attr_type = extra_attrib.get(ptoff);
	    ROP_FBXassignValues(hd_type, fbx_type, &extra_attr_type);
	}
	else
	    ROP_FBXassignValues(hd_type, fbx_type, NULL);
	layer_elem->GetDirectArray().Add(fbx_type);
    }
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportVertexAttribute(const GU_Detail *gdp, const GA_ROHandleT<HD_TYPE> &attrib, const GA_ROHandleF &extra_attrib, FbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if (!gdp || !layer_elem || attrib.isInvalid())
	return;

    // Maya crashes when we export vertex attributes in direct mode. Therefore, export in indirect.

    // Go over all vertices
    HD_TYPE hd_type;
    FBX_TYPE fbx_type;
    const GEO_Primitive* prim;
    float extra_attr_type;

    bool is_indexed = (layer_elem->GetReferenceMode() != FbxLayerElement::eDirect);
    if (is_indexed)
    {
	int curr_arr_cntr = 0;
	UT_Map<HD_TYPE, int> hd_type_map;
	GA_FOR_ALL_PRIMITIVES(gdp, prim)
	{	    
	    GA_Size num_verts = prim->getVertexCount();
	    for (GA_Size curr_vert = num_verts - 1; curr_vert >= 0; curr_vert--)
	    {
		GA_Offset vertexoffset = prim->getVertexOffset(curr_vert);
		hd_type = attrib.get(vertexoffset);

		auto found = hd_type_map.find(hd_type);
		if ( found != hd_type_map.end() )
		{
		    // Add the found IDx
		    layer_elem->GetIndexArray().Add( found->second );
		}
		else
		{
		    // Add a new element to the indexed array
		    // Convert the houdini type to an fbx type
		    if (extra_attrib.isValid())
		    {
			extra_attr_type = extra_attrib.get(vertexoffset);
			ROP_FBXassignValues(hd_type, fbx_type, &extra_attr_type);
		    }
		    else
			ROP_FBXassignValues(hd_type, fbx_type, NULL);

		    // Add it to the direct and index arrays
		    layer_elem->GetDirectArray().Add(fbx_type);
		    layer_elem->GetIndexArray().Add(curr_arr_cntr);

		    // Also add it to the map
		    hd_type_map[hd_type] = curr_arr_cntr;
		    curr_arr_cntr++;
		}
	    }
	}
    }
    else
    {
	// Elements aren't indexed, so we can just simply add them to the direct array
	GA_FOR_ALL_PRIMITIVES(gdp, prim)
	{
	    GA_Size num_verts = prim->getVertexCount();
	    for (GA_Size curr_vert = num_verts - 1; curr_vert >= 0; curr_vert--)
	    {
		GA_Offset vertexoffset = prim->getVertexOffset(curr_vert);
		hd_type = attrib.get(vertexoffset);
		if (extra_attrib.isValid())
		{
		    extra_attr_type = extra_attrib.get(vertexoffset);
		    ROP_FBXassignValues(hd_type, fbx_type, &extra_attr_type);
		}
		else
		    ROP_FBXassignValues(hd_type, fbx_type, NULL);

		layer_elem->GetDirectArray().Add(fbx_type);
	    }
	}
    }


}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportPrimitiveAttribute(const GU_Detail *gdp, const GA_ROHandleT<HD_TYPE> &attrib, const GA_ROHandleF &extra_attrib, FbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if (!gdp || !layer_elem || attrib.isInvalid())
	return;

    // Go over all vertices
    HD_TYPE hd_type;
    FBX_TYPE fbx_type;
    const GEO_Primitive* prim;
    float extra_attr_type;

    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
        GA_Offset primitiveoffset = prim->getMapOffset();
	hd_type = attrib.get(primitiveoffset);
	if (extra_attrib.isValid())
	{
	    extra_attr_type = extra_attrib.get(primitiveoffset);
	    ROP_FBXassignValues(hd_type, fbx_type, &extra_attr_type);
	}
	else
	    ROP_FBXassignValues(hd_type, fbx_type, NULL);
	layer_elem->GetDirectArray().Add(fbx_type);
    }
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportDetailAttribute(const GU_Detail *gdp, const GA_ROHandleT<HD_TYPE> &attrib, const GA_ROHandleF &extra_attrib, FbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if (!gdp || !layer_elem || attrib.isInvalid())
	return;

    // Go over all vertices
    HD_TYPE hd_type;
    FBX_TYPE fbx_type;
    float extra_attr_type;

    hd_type = attrib.get(GA_Offset(0));
    if (extra_attrib.isValid())
    {
	extra_attr_type = extra_attrib.get(GA_Offset(0));
	ROP_FBXassignValues(hd_type, fbx_type, &extra_attr_type);
    }
    else
	ROP_FBXassignValues(hd_type, fbx_type, NULL);
    layer_elem->GetDirectArray().Add(fbx_type);
}
/********************************************************************************************************/
FbxLayerElement* 
ROP_FBXMainVisitor::getAndSetFBXLayerElement(FbxLayer* attr_layer, ROP_FBXAttributeType attr_type, const GU_Detail* gdp,
					     const GA_ROAttributeRef &attr_offset, const GA_ROAttributeRef &extra_attr_offset,
					     FbxLayerElement::EMappingMode mapping_mode, FbxLayerContainer* layer_container)
{
    FbxLayerElement::EReferenceMode ref_mode;

    // These are brutal hacks so that Maya's importer does not crash.
    if(mapping_mode == FbxLayerElement::eByPolygonVertex)
	ref_mode = FbxLayerElement::eIndexToDirect;
    else
	ref_mode = FbxLayerElement::eDirect;
    
    FbxLayerElement* new_elem = NULL;
    if (attr_type == ROP_FBXAttributeNormal ||
        attr_type == ROP_FBXAttributeTangent ||
        attr_type == ROP_FBXAttributeBinormal)
    {
	// Normals always have to be direct. Also for Maya.
	ref_mode = FbxLayerElement::eDirect;

        FbxLayerElementTemplate<FbxVector4> *temp_layer;
        if (attr_type == ROP_FBXAttributeNormal)
        {
            FbxLayerElementNormal* nml_layer = FbxLayerElementNormal::Create(layer_container, "");
            //FbxLayerElementNormal* nml_layer = mySDKManager->CreateFbxLayerElementNormal("");
            nml_layer->SetMappingMode(mapping_mode);
            nml_layer->SetReferenceMode(ref_mode);
            attr_layer->SetNormals(nml_layer);
            temp_layer = nml_layer;
        }
        else if (attr_type == ROP_FBXAttributeTangent)
        {
            FbxLayerElementTangent* tan_layer = FbxLayerElementTangent::Create(layer_container, "");
            //FbxLayerElementTangent* tan_layer = mySDKManager->CreateFbxLayerElementTangent("");
            tan_layer->SetMappingMode(mapping_mode);
            tan_layer->SetReferenceMode(ref_mode);
            attr_layer->SetTangents(tan_layer);
            temp_layer = tan_layer;
        }
        else
        {
            UT_ASSERT(attr_type == ROP_FBXAttributeBinormal);
            FbxLayerElementBinormal* bin_layer = FbxLayerElementBinormal::Create(layer_container, "");
            //FbxLayerElementBinormal* bin_layer = mySDKManager->CreateFbxLayerElementBinormal("");
            bin_layer->SetMappingMode(mapping_mode);
            bin_layer->SetReferenceMode(ref_mode);
            attr_layer->SetBinormals(bin_layer);
            temp_layer = bin_layer;
        }
        new_elem = temp_layer;

        GA_ROHandleV3 attrib(attr_offset);
        GA_ROHandleF extra_attrib(extra_attr_offset);

	if(mapping_mode == FbxLayerElement::eByControlPoint)
	    exportPointAttribute<UT_Vector3, FbxVector4>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygonVertex)
	    exportVertexAttribute<UT_Vector3, FbxVector4>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygon)
	    exportPrimitiveAttribute<UT_Vector3, FbxVector4>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eAllSame)
	    exportDetailAttribute<UT_Vector3, FbxVector4>(gdp, attrib, extra_attrib, temp_layer);
    }
    else if(attr_type == ROP_FBXAttributeUV)
    {
	//FbxLayerElementUV* temp_layer = mySDKManager->CreateFbxLayerElementUV("");
	FbxLayerElementUV* temp_layer = FbxLayerElementUV::Create(layer_container, "");
	temp_layer->SetMappingMode(mapping_mode);
	temp_layer->SetReferenceMode(ref_mode);
	attr_layer->SetUVs(temp_layer);
	new_elem = temp_layer;

        GA_ROHandleV3 attrib(attr_offset);
        GA_ROHandleF extra_attrib(extra_attr_offset);

	if(mapping_mode == FbxLayerElement::eByControlPoint)
	    exportPointAttribute<UT_Vector3, FbxVector2>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygonVertex)
	    exportVertexAttribute<UT_Vector3, FbxVector2>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygon)
	    exportPrimitiveAttribute<UT_Vector3, FbxVector2>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eAllSame)
	    exportDetailAttribute<UT_Vector3, FbxVector2>(gdp, attrib, extra_attrib, temp_layer);

    }
    else if(attr_type == ROP_FBXAttributeVertexColor)
    {
	//FbxLayerElementVertexColor* temp_layer = mySDKManager->CreateFbxLayerElementVertexColor("");
	FbxLayerElementVertexColor* temp_layer = FbxLayerElementVertexColor::Create(layer_container, "");
	temp_layer->SetMappingMode(mapping_mode);
	temp_layer->SetReferenceMode(ref_mode);
	attr_layer->SetVertexColors(temp_layer);
	new_elem = temp_layer;

        GA_ROHandleV3 attrib(attr_offset);
        GA_ROHandleF extra_attrib(extra_attr_offset);

	if(mapping_mode == FbxLayerElement::eByControlPoint)
	    exportPointAttribute<UT_Vector3, FbxColor>(gdp, attrib, extra_attrib,  temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygonVertex)
	    exportVertexAttribute<UT_Vector3, FbxColor>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eByPolygon)
	    exportPrimitiveAttribute<UT_Vector3, FbxColor>(gdp, attrib, extra_attrib, temp_layer);
	else if(mapping_mode == FbxLayerElement::eAllSame)
	    exportDetailAttribute<UT_Vector3, FbxColor>(gdp, attrib, extra_attrib, temp_layer);

    }

    return new_elem;
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserPointAttribute(const GU_Detail* gdp, const GA_Attribute* attr, int attr_subindex, const char* fbx_prop_name, FbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem || gdp->getNumPoints() == 0 || !fbx_prop_name || strlen(fbx_prop_name) <= 0)
	return;

    SIMPLE_TYPE hd_type;
    int array_pos = 0;

    const GA_Attribute *attrib = gdp->findPointAttrib(*attr);
    UT_ASSERT(attrib);
    if (!attrib)
	return;

    layer_elem->ResizeAllDirectArrays(gdp->getNumPoints());
    FbxLayerElementArrayTemplate<void*> * fbx_direct_array_ptr = layer_elem->GetDirectArrayVoid(fbx_prop_name);
    if(!fbx_direct_array_ptr)
	return;
    SIMPLE_TYPE* fbx_direct_array = NULL;
    fbx_direct_array = fbx_direct_array_ptr->GetLocked(fbx_direct_array);
    const GA_ATIGroupBool *group = dynamic_cast<const GA_ATIGroupBool *>(attrib);
    if (group)
    {
        UT_ASSERT(attr_subindex == 0);
        // Go over all points
        GA_Offset ptoff;
        GA_FOR_ALL_PTOFF(gdp, ptoff)
        {
            hd_type = group->contains(ptoff);
            fbx_direct_array[array_pos] = hd_type;
            array_pos++;
        }
    }
    else
    {
        GA_ROHandleT<SIMPLE_TYPE> attribhandle(attrib);
	if (attribhandle.isValid())
	{
	    GA_Offset ptoff;
	    GA_FOR_ALL_PTOFF(gdp, ptoff)
	    {
		hd_type = attribhandle.get(ptoff, attr_subindex);
		fbx_direct_array[array_pos] = hd_type;
		array_pos++;
	    }
	}
    }
    fbx_direct_array_ptr->Release((void**)&fbx_direct_array);
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserVertexAttribute(const GU_Detail* gdp, const GA_Attribute* attr, int attr_subindex, const char* fbx_prop_name, FbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem || gdp->getNumPoints() <= 0 || !fbx_prop_name || strlen(fbx_prop_name) <= 0)
	return;

    // Go over all vertices
    const GEO_Primitive* prim;
    SIMPLE_TYPE hd_type;

    const GA_Attribute *attrib = gdp->findVertexAttrib(*attr);
    UT_ASSERT(attrib);
    if (!attrib)
	return;

    layer_elem->ResizeAllDirectArrays(gdp->getNumVertices());
    FbxLayerElementArrayTemplate <void*>* fbx_direct_array_ptr = layer_elem->GetDirectArrayVoid(fbx_prop_name);
    if(!fbx_direct_array_ptr)
	return;
    SIMPLE_TYPE* fbx_direct_array = NULL;
    fbx_direct_array = fbx_direct_array_ptr->GetLocked(fbx_direct_array);
    int array_pos = 0;
    const GA_ATIGroupBool *group = dynamic_cast<const GA_ATIGroupBool *>(attrib);
    UT_ASSERT(group == NULL || attr_subindex == 0);
    GA_ROHandleT<SIMPLE_TYPE> attribhandle(attrib);
    if (attribhandle.isValid())
    {
	GA_FOR_ALL_PRIMITIVES(gdp, prim)
	{
	    GA_Size num_verts = prim->getVertexCount();
	    for (GA_Size curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	    {
		GA_Offset vtxoff = prim->getVertexOffset(curr_vert);
		if (group)
		    hd_type = group->contains(vtxoff);
		else
		    hd_type = attribhandle.get(vtxoff, attr_subindex);
		fbx_direct_array[array_pos] = hd_type;
		array_pos++;
	    }
	}
    }
    fbx_direct_array_ptr->Release((void**)&fbx_direct_array);
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserPrimitiveAttribute(const GU_Detail* gdp, const GA_Attribute* attr, int attr_subindex, const char* fbx_prop_name, FbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Go over all vertices
    const GEO_Primitive* prim;
    SIMPLE_TYPE hd_type;
    int array_pos = 0;

    const GA_Attribute *attrib = gdp->findPrimAttrib(*attr);
    UT_ASSERT(attrib);
    if (!attrib)
	return;

    layer_elem->ResizeAllDirectArrays(gdp->getNumPrimitives());
    FbxLayerElementArrayTemplate<void*> *fbx_direct_array_ptr = layer_elem->GetDirectArrayVoid(fbx_prop_name);
    if(!fbx_direct_array_ptr)
	return;
    SIMPLE_TYPE* fbx_direct_array = NULL;
    fbx_direct_array = fbx_direct_array_ptr->GetLocked(fbx_direct_array);

    const GA_ATIGroupBool *group = dynamic_cast<const GA_ATIGroupBool *>(attrib);
    if (group)
    {
        UT_ASSERT(attr_subindex == 0);
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            hd_type = group->contains(prim->getMapOffset());
            fbx_direct_array[array_pos] = hd_type;
            array_pos++;
        }
    }
    else
    {
        GA_ROHandleT<SIMPLE_TYPE> attribhandle(attrib);
	if (attribhandle.isValid())
	{
	    GA_FOR_ALL_PRIMITIVES(gdp, prim)
	    {
		hd_type = attribhandle.get(prim->getMapOffset(), attr_subindex);
		fbx_direct_array[array_pos] = hd_type;
		array_pos++;
	    }
	}
    }

    fbx_direct_array_ptr->Release((void**)&fbx_direct_array);
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserDetailAttribute(const GU_Detail* gdp, const GA_Attribute* attr, int attr_subindex, const char* fbx_prop_name, FbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    const GA_Attribute *attrib = gdp->findGlobalAttrib(*attr);
    UT_ASSERT(attrib);
    if (!attrib)
	return;
    GA_ROHandleT<SIMPLE_TYPE> attribhandle(attrib);
    if (attribhandle.isInvalid())
	return;

    // Go over all vertices
    SIMPLE_TYPE hd_type;
    int array_pos = 0;

    layer_elem->ResizeAllDirectArrays(1);
    //SIMPLE_TYPE *fbx_direct_array =(SIMPLE_TYPE *)(layer_elem->GetDirectArrayVoid(fbx_prop_name))->GetArray();
    FbxLayerElementArrayTemplate<void*>* fbx_direct_array_ptr = layer_elem->GetDirectArrayVoid(fbx_prop_name);
    if(!fbx_direct_array_ptr)
	return;
    SIMPLE_TYPE* fbx_direct_array = NULL;
    fbx_direct_array = fbx_direct_array_ptr->GetLocked(fbx_direct_array);

    hd_type = attribhandle.get(GA_Offset(0), attr_subindex);
    fbx_direct_array[array_pos] = hd_type;
    array_pos++;

    fbx_direct_array_ptr->Release((void**)&fbx_direct_array);
}
/********************************************************************************************************/
static int getNumAttrElems(const GA_Attribute* attr)
{
    GA_StorageClass storage = attr->getStorageClass();
    if(storage == GA_STORECLASS_REAL || storage == GA_STORECLASS_INT)
	return attr->getTupleSize();
    return 0;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager,
				FbxMesh* mesh_attr, FbxLayerElement::EMappingMode mapping_mode )
{

    if(hd_attribs.size() <= 0)
	return;

    // Arrays for custom attributes
    FbxArray<FbxDataType> custom_types_array;
    FbxArray<const char*> custom_names_array;
    UT_String full_name(UT_String::ALWAYS_DEEP);
    UT_String suffix(UT_String::ALWAYS_DEEP);
    const char* vec_comps[] = { "x", "y", "z"};
    int curr_pos;
    GA_TypeInfo attr_type;
    GA_StorageClass attr_store;
    int attr_size;
    const char* orig_name;
    TStringVector attr_names;
    char* temp_name;
    bool is_supported;

    // Iterate once, adding attribute names and types
    int num_supported_attribs = 0;
    int curr_hd_attr, num_hd_attrs = hd_attribs.size();
    for(curr_hd_attr = 0; curr_hd_attr < num_hd_attrs; curr_hd_attr++)
    {
	const GA_Attribute *attr = hd_attribs[curr_hd_attr];

        // P is already stored in other ways
        UT_ASSERT(attr != attr->getDetail().getP());
        if (attr == attr->getDetail().getP())
            continue;

        // Don't store private attributes, including internal groups
        UT_ASSERT(attr->getScope() != GA_SCOPE_PRIVATE);
        if (attr->getScope() == GA_SCOPE_PRIVATE)
            continue;
        UT_ASSERT(attr->getScope() != GA_SCOPE_GROUP || !GA_ATIGroupBool::cast(attr)->getGroup()->getInternal());
        if (attr->getScope() == GA_SCOPE_GROUP && GA_ATIGroupBool::cast(attr)->getGroup()->getInternal())
            continue;

	// Get the attribute type
	attr_type = attr->getTypeInfo();
	attr_store = attr->getStorageClass();
	attr_size = getNumAttrElems(attr);
	if(attr_size <= 0)
	    continue;
	orig_name = attr->getName();

	is_supported = false;

	for(curr_pos = 0; curr_pos < attr_size; curr_pos++)
	{
	    // Convert it to the appropriate FBX type.
	    // Note that apparent FBX doesn't support strings here. Ugh.
	    // We also have to break apart things like vectors.
	    suffix = "";
	    full_name = "";
	    if(attr_type == GA_TYPE_VECTOR)
	    {
		custom_types_array.Add(FbxFloatDT);
		if(attr_size <= 3)
		{
		    if(attr_size > 1)
			suffix.sprintf("_%s", vec_comps[curr_pos]);
		}
		else
		    suffix.sprintf("_%d", curr_pos);
		is_supported = true;
	    }
	    else if(attr_store == GA_STORECLASS_INT)
	    {
		custom_types_array.Add(FbxIntDT);
		if(attr_size > 1)
		    suffix.sprintf("_%d", curr_pos);
		is_supported = true;
	    }
	    else if(attr_store == GA_STORECLASS_REAL) 
	    {
		custom_types_array.Add(FbxFloatDT);
		if(attr_size > 1)
		    suffix.sprintf("_%d", curr_pos);
		is_supported = true;
	    }

	    if(is_supported)
	    {
		// Construct a full name and add it
		full_name = orig_name + suffix;

		// NOTE: This is highly incovenient. The FBX array only stores a pointer
		// to a string; however, we need this pointer up until we export to the actual
		// file on disk, which happens in a separate function call. Thus, we allocate it here,
		// queue it up to be deallocated later, and do the actual deallocation in 
		// ROP_FBXExporter::finishExport().
		temp_name =  new char[full_name.length()+1];
		strcpy(temp_name, (const char*)full_name);
		myParentExporter->queueStringToDeallocate(temp_name);
		custom_names_array.Add( temp_name);

		num_supported_attribs++;
		attr_names.push_back((const char*)full_name);
	    }	    

	} // over attr size

    } // over all attributes

    if(num_supported_attribs <= 0)
	return;

    // Now create the actual layer
    FbxLayer* attr_layer;
    int layer_idx;
    UT_String layer_name(UT_String::ALWAYS_DEEP);
    attr_layer = attr_manager.getAttributeLayer(ROP_FBXAttributeUser, &layer_idx);
    layer_name.sprintf("UserDataLayer%d", layer_idx);
    //FbxLayerElementUserData *layer_elem = mySDKManager->CreateFbxLayerElementUserData( (const char*)layer_name, layer_idx, custom_types_array, custom_names_array);
    FbxLayerElementUserData *layer_elem = FbxLayerElementUserData::Create(mesh_attr, (const char*)layer_name, layer_idx, custom_types_array, custom_names_array);

    layer_elem->SetMappingMode(mapping_mode);
    attr_layer->SetUserData(layer_elem);
    int attr_name_pos = 0;

    // Add data to it
    for(curr_hd_attr = 0; curr_hd_attr < num_hd_attrs; curr_hd_attr++)
    {
	const GA_Attribute *attr = hd_attribs[curr_hd_attr];

	// Get the attribute type
	attr_store = attr->getStorageClass();
	attr_size = getNumAttrElems(attr);
	if(attr_size <= 0)
	    continue;
	orig_name = attr->getName();

	for(curr_pos = 0; curr_pos < attr_size; curr_pos++)
	{
	    if(attr_store == GA_STORECLASS_INT)
	    {
		if(mapping_mode == FbxLayerElement::eByControlPoint)
		    exportUserPointAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eByPolygonVertex)
		    exportUserVertexAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eByPolygon)
		    exportUserPrimitiveAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eAllSame)
		    exportUserDetailAttribute<int>(gdp, attr,curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);		
	    }
	    else if(attr_store == GA_STORECLASS_REAL)
	    {
		if(mapping_mode == FbxLayerElement::eByControlPoint)
		    exportUserPointAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eByPolygonVertex)
		    exportUserVertexAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eByPolygon)
		    exportUserPrimitiveAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == FbxLayerElement::eAllSame)
		    exportUserDetailAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
	    }

	    attr_name_pos++;
	}
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::exportAttributes(const GU_Detail* gdp, FbxMesh* mesh_attr)
{
    ROP_FBXAttributeLayerManager attr_manager(mesh_attr);
    ROP_FBXAttributeType curr_attr_type;
    FbxLayer* attr_layer;
    GA_ROAttributeRef attr_offset, extra_attr_offset;
    FbxLayerElement* res_elem;
    THDAttributeVector user_attribs;



    // Go through point attributes first.
    if(gdp->getNumPoints() > 0)
    {
	GA_AttributeFilter filter_no_P = GA_AttributeFilter::selectOr(GA_AttributeFilter::selectStandard(gdp->getP()),GA_AttributeFilter::selectGroup());
	for (GA_AttributeDict::ordered_iterator itor = gdp->pointAttribs().obegin();
	     itor != gdp->pointAttribs().oend(); ++itor)
	{
	    const GA_Attribute *attr = itor.item();
	    if (!filter_no_P.match(attr))
		continue;
            if (attr->getScope() == GA_SCOPE_PRIVATE)
                continue;
            if (attr->getScope() == GA_SCOPE_GROUP && GA_ATIGroupBool::cast(attr)->getGroup()->getInternal())
                continue;

	    // Determine the proper attribute type
	    curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	    if(curr_attr_type != ROP_FBXAttributeUser)
	    {
		// Get the appropriate layer
		attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
		UT_ASSERT(attr_layer);

		// Create an appropriate layer element and fill it with values
		attr_offset = gdp->findPointAttrib(*attr);
		if(curr_attr_type == ROP_FBXAttributeVertexColor)
		    extra_attr_offset = gdp->findFloatTuple(GA_ATTRIB_POINT, gdp->getStdAttributeName(GEO_ATTRIBUTE_ALPHA, gdp->getAttributeLayer(attr->getName())));
		else
		    extra_attr_offset.clear();

		res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, extra_attr_offset, FbxLayerElement::eByControlPoint, mesh_attr);
		setProperName(res_elem, gdp, attr);
	    }
	    else
		user_attribs.push_back(attr);
	}
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, FbxLayerElement::eByControlPoint);
    user_attribs.clear();

    // Go through vertex attributes
    GA_AttributeFilter filter = GA_AttributeFilter::selectOr(GA_AttributeFilter::selectStandard(),GA_AttributeFilter::selectGroup());
    for (GA_AttributeDict::ordered_iterator itor = gdp->vertexAttribs().obegin();
	 itor != gdp->vertexAttribs().oend(); ++itor)
    {
	const GA_Attribute *attr = itor.item();
	if (!filter.match(attr))
	    continue;
        if (attr->getScope() == GA_SCOPE_PRIVATE)
            continue;
        if (attr->getScope() == GA_SCOPE_GROUP && GA_ATIGroupBool::cast(attr)->getGroup()->getInternal())
            continue;

	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findVertexAttrib(*attr);
	    if(curr_attr_type == ROP_FBXAttributeVertexColor)
		extra_attr_offset = gdp->findFloatTuple(GA_ATTRIB_VERTEX, gdp->getStdAttributeName(GEO_ATTRIBUTE_ALPHA, gdp->getAttributeLayer(attr->getName())));
	    else
		extra_attr_offset.clear();
	    // Maya crashes when we export vertex attributes in direct mode. Therefore, export in indirect.
	    res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset,extra_attr_offset, FbxLayerElement::eByPolygonVertex, mesh_attr);
	    setProperName(res_elem, gdp, attr);
	}
	else
	    user_attribs.push_back(attr);
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, FbxLayerElement::eByPolygonVertex);
    user_attribs.clear();

    // Primitive attributes
    for (GA_AttributeDict::ordered_iterator itor = gdp->primitiveAttribs().obegin();
	 itor != gdp->primitiveAttribs().oend(); ++itor)
    {
	const GA_Attribute *attr = itor.item();
	if (!filter.match(attr))
	    continue;
        if (attr->getScope() == GA_SCOPE_PRIVATE)
            continue;
        if (attr->getScope() == GA_SCOPE_GROUP && GA_ATIGroupBool::cast(attr)->getGroup()->getInternal())
            continue;

	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findPrimAttrib(*attr);
	    if(curr_attr_type == ROP_FBXAttributeVertexColor)
		extra_attr_offset = gdp->findFloatTuple(GA_ATTRIB_PRIMITIVE, gdp->getStdAttributeName(GEO_ATTRIBUTE_ALPHA, gdp->getAttributeLayer(attr->getName())));
	    else
		extra_attr_offset.clear();
	    res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, extra_attr_offset, FbxLayerElement::eByPolygon, mesh_attr);
	    setProperName(res_elem, gdp, attr);
	}
	else
	    user_attribs.push_back(attr);
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, FbxLayerElement::eByPolygon);
    user_attribs.clear();

    // Detail attributes
    for (GA_AttributeDict::ordered_iterator itor = gdp->attribs().obegin();
	 itor != gdp->attribs().oend(); ++itor)
    {
	const GA_Attribute *attr = itor.item();
	if (!filter.match(attr))
	    continue;
        if (attr->getScope() == GA_SCOPE_PRIVATE)
            continue;
        if (attr->getScope() == GA_SCOPE_GROUP && GA_ATIGroupBool::cast(attr)->getGroup()->getInternal())
            continue;

	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findGlobalAttrib(*attr);
	    if(curr_attr_type == ROP_FBXAttributeVertexColor)
		extra_attr_offset = gdp->findFloatTuple(GA_ATTRIB_GLOBAL, gdp->getStdAttributeName(GEO_ATTRIBUTE_ALPHA, gdp->getAttributeLayer(attr->getName())));
	    else
		extra_attr_offset.clear();
	    res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, extra_attr_offset, FbxLayerElement::eAllSame, mesh_attr);
	    setProperName(res_elem, gdp, attr);
	}
	else
	    user_attribs.push_back(attr);
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, FbxLayerElement::eAllSame);
    user_attribs.clear();
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    // Determine what null look we should output. Only output as a cross if we have geometry.
    // This means that subnets get output as fbx nulls with no look, matching
    // what Maya imports/exports for group nodes.
    bool as_cross = false;
    OBJ_Node *obj = node->castToOBJNode();
    if (obj && obj->getChildTypeID() == SOP_OPTYPE_ID)
    {
	OP_Context context(myParentExporter->getStartTime());
	const GU_Detail* gdp = obj->getDisplayGeometry(context, /*check_enable*/false);
	as_cross = !gdp || (gdp->getNumPoints() > 0);
    }

    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);

    FbxNull *res_attr = FbxNull::Create(mySDKManager, (const char*)node_name);
    res_attr->Look.Set(as_cross ? FbxNull::eCross : FbxNull::eNone);
    res_node->SetNodeAttribute(res_attr);

    res_nodes.push_back(res_node);
    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputLODGroupNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    // Create a new node and add an LOD Group attribute to it
    FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);
    FbxLODGroup *res_attr = FbxLODGroup::Create(mySDKManager, (const char*)node_name);

    // All the LODGroup's info should have been stored as spare parameters by the importer
    PRM_Parm *parm;
    if (node->getParameterOrProperty("fbx_lod_min_max", 0, node, parm, true, NULL))
    {
	int32 value = -1;
	parm->getValue(0, value, 0, SYSgetSTID());
	res_attr->MinMaxDistance.Set(value ? true : false);
    }

    if (node->getParameterOrProperty("fbx_lod_min", 0, node, parm, true, NULL))
    {
	fpreal value;
	parm->getValue(0, value, 0, SYSgetSTID());
	res_attr->MinDistance.Set(value);
    }

    if (node->getParameterOrProperty("fbx_lod_max", 0, node, parm, true, NULL))
    {
	fpreal value;
	parm->getValue(0, value, 0, SYSgetSTID());
	res_attr->MaxDistance.Set(value);
    }

    if (node->getParameterOrProperty("fbx_lod_world_space", 0, node, parm, true, NULL))
    {
	int32 value = -1;
	parm->getValue(0, value, 0, SYSgetSTID());
	res_attr->WorldSpace.Set(value ? true : false);
    }

    int32 num_diplay_levels = -1;
    if (node->getParameterOrProperty("fbx_lod_num_display_levels", 0, node, parm, true, NULL))
	parm->getValue(0, num_diplay_levels, 0, SYSgetSTID());

    if (node->getParameterOrProperty("fbx_lod_display_levels", 0, node, parm, true, NULL))
    {
	for (int n = 0; n < num_diplay_levels; n++)
	{
	    int value;
	    parm->getValue(0, value, n, SYSgetSTID());

	    if (value == 1)
		res_attr->AddDisplayLevel(FbxLODGroup::EDisplayLevel::eShow);
	    else if (value == 2)
		res_attr->AddDisplayLevel(FbxLODGroup::EDisplayLevel::eHide);
	    else
		res_attr->AddDisplayLevel(FbxLODGroup::EDisplayLevel::eUseLOD);
	}
    }

    int32 num_thresholds = -1;
    if (node->getParameterOrProperty("fbx_lod_num_thresholds", 0, node, parm, true, NULL))
	parm->getValue(0, num_thresholds, 0, SYSgetSTID());

    if (node->getParameterOrProperty("fbx_lod_thresholds", 0, node, parm, true, NULL))
    {
	for (int n = 0; n < num_thresholds; n++)
	{
	    fpreal value;
	    parm->getValue(0, value, n, SYSgetSTID());

	    FbxDistance value_as_distance((float)value, "");
	    res_attr->AddThreshold(value_as_distance);
	}
    }

    /* 
    // deactivated temporarily as the linux fbxsdk as issue with this...
    if (node->getParameterOrProperty("fbx_threshold_used_as_percentage", 0, node, parm, true, NULL))
    {
	int32 value = -1;
	parm->getValue(0, value, 0, SYSgetSTID());
	res_attr->ThresholdsUsedAsPercentage.Set(value ? true : false);
    }
    */
    
    res_node->SetNodeAttribute(res_attr);
    res_nodes.push_back(res_node);

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    fpreal float_parm[3];
    int int_param;
    UT_String string_param;
    FbxDouble3 fbx_col;
    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);
    FbxLight *res_attr = FbxLight::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Set params
    FbxLight::EType light_type;
    ROP_FBXUtil::getStringOPParm(node, "light_type", string_param, true);
    if(string_param == "point")
    {
	// Point light
	light_type = FbxLight::ePoint;
    }
    else if(string_param == "spot")
    {
	light_type = FbxLight::eSpot;
    }
    else if(string_param == "distant")
    {
	light_type = FbxLight::eDirectional;
    }
    else
    {	
	light_type = FbxLight::ePoint;
	myErrorManager->addError("Unsupported light type. Exporting as point light. Node: ", node_name, NULL);
    }
    res_attr->LightType.Set(light_type);

    // Enable/disable flag
    int_param = ROP_FBXUtil::getIntOPParm(node, "light_enable");
    res_attr->CastLight.Set((bool)int_param);

    // Cast shadows flag
    ROP_FBXUtil::getStringOPParm(node, "shadow_type", string_param, true);
    res_attr->CastShadows.Set(string_param != "off");

    // Color
    fbx_col[0] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 0);
    fbx_col[1] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 1);
    fbx_col[2] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 2);
//    fbx_col.Set(float_parm[0], float_parm[1], float_parm[2]);
//    res_attr->SetDefaultColor(fbx_col);
    res_attr->Color.Set(fbx_col);

    // Intensity
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "light_intensity");
    //res_attr->SetDefaultIntensity(float_parm[0]*100.0);
    res_attr->Intensity.Set(float_parm[0]*100.0);

    // Cone angle
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "coneangle");
    //res_attr->SetDefaultConeAngle(float_parm[0]);
    res_attr->OuterAngle.Set(float_parm[0]);

    // Attenuation
    ROP_FBXUtil::getStringOPParm(node, "atten_type", string_param, true);
    FbxLight::EDecayType decay_type = FbxLight::eNone;
    if(string_param == "quadratic")
	decay_type = FbxLight::eQuadratic;
    else if(string_param == "half")
	decay_type = FbxLight::eLinear;
    else if(string_param != "off")
    {
	// Unsupported attentuation type.
	myErrorManager->addError("Unsupported attenuation type. Node: ", node_name, NULL);
    }
    res_attr->DecayType.Set(decay_type);

    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "atten_dist");
    res_attr->DecayStart.Set(float_parm[0]*0.5);

    res_nodes.push_back(res_node);
    return true;
    
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, FbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    UT_String string_param;
    fpreal float_parm[3];
    FbxVector4 fbx_vec4;

    UT_String node_name;
    ROP_FBXUtil::getNodeName(node, node_name, myNodeManager);

    FbxNode* res_node = FbxNode::Create(mySDKManager, (const char*)node_name);
    FbxCamera *res_attr = FbxCamera::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Projection type
    ROP_FBXUtil::getStringOPParm(node, "projection", string_param, true);
    FbxCamera::EProjectionType project_type;
    if(string_param == "ortho")
	project_type = FbxCamera::eOrthogonal;
    else if(string_param == "perspective")
	project_type = FbxCamera::ePerspective;
    else
    {
	myErrorManager->addError("Unsupported camera projection type. Exporting as perspective camera. Node: ", node_name, NULL);
	project_type = FbxCamera::ePerspective;
    }
    res_attr->ProjectionType.Set(project_type);

    // Focal length
    // Get the units, as well
    double length_mult = 1.0;
    ROP_FBXUtil::getStringOPParm(node, "focalunits", string_param, true);
    if(string_param == "m")
	length_mult = 1000.0;
    else if(string_param == "nm")
	length_mult = 1000000.0;
    else if(string_param == "in")
	length_mult = 25.4;
    else if(string_param == "ft")
	length_mult = 304.8;

    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "focal");
    //double foc_len = float_parm[0];
    res_attr->SetApertureMode(FbxCamera::eFocalLength);
    res_attr->FocalLength.Set(float_parm[0]*length_mult);

    // Pixel ratio
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "aspect");
    res_attr->SetPixelRatio(float_parm[0]);

    // Up vector
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "up",0);
    float_parm[1] = ROP_FBXUtil::getFloatOPParm(node, "up",1);
    float_parm[2] = ROP_FBXUtil::getFloatOPParm(node, "up",2);
    fbx_vec4.Set(float_parm[0], float_parm[1], float_parm[2]);
    res_attr->UpVector.Set(fbx_vec4);

    // Convert aperture. Because FBX SDK only recognizes animation if the 
    // aperture mode is set to FbxCamera::eFOCAL_LENGTH, we have to
    // convert our aperture into aperture width and height, in inches,
    // that the FBX SDK expects.
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "aperture");
    //double fov_angle = SYSatan( (double)(float_parm[0])/(2.0*foc_len*length_mult) ) * 2.0;

    // Get the x/y resolution of the camera to get aperture ratios.
    fpreal xres = ROP_FBXUtil::getFloatOPParm(node, "res",0);
    fpreal yres = ROP_FBXUtil::getFloatOPParm(node, "res",1);

    // Record the camera resolution
    res_attr->SetAspect(FbxCamera::eFixedResolution, xres, yres);

    fpreal ap_height = float_parm[0];
    if(SYSequalZero(xres) == false)
	ap_height = float_parm[0]*yres/xres;

    // Set the custom aperture format and convert our measurments 
    // to inches.
    res_attr->SetApertureFormat(FbxCamera::eCustomAperture);
    res_attr->SetApertureHeight( (ap_height / 10.0) / 2.54);
    res_attr->SetApertureWidth((float_parm[0] / 10.0) / 2.54);

    // Near and far clip planes
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "near");
    res_attr->SetNearPlane(float_parm[0]);

    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "far");
    res_attr->SetFarPlane(float_parm[0]);

    // Ortho zoom
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "orthowidth");
    res_attr->OrthoZoom.Set(float_parm[0]);

    // Focus distance
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "focus");
    res_attr->FocusSource.Set(FbxCamera::eFocusSpecificDistance);
    res_attr->FocusDistance.Set(float_parm[0]);

    res_nodes.push_back(res_node);
    return true;
}
/********************************************************************************************************/
UT_Color 
ROP_FBXMainVisitor::getAccumAmbientColor(void)
{
    return myAmbientColor;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::exportMaterials(OP_Node* source_node, FbxNode* fbx_node)
{
//    OP_Director* op_director = OPgetDirector();

    UT_String main_mat_path;
    ROP_FBXUtil::getStringOPParm(source_node, GEO_STD_ATTRIB_MATERIAL, main_mat_path, true);
    OP_Node* main_mat_node = NULL;
    if(main_mat_path.isstring())
	main_mat_node = source_node->findNode(main_mat_path);

    // See if there are any per-face indices
    fpreal start_time = myStartTime;
    int curr_prim, num_prims = 0;
    OP_Node **per_face_mats = NULL;
    UT_StringArray per_face_mats_paths;

    const GEO_Primitive *prim;
    OP_Network* src_net = dynamic_cast<OP_Network*>(source_node);
    OP_Node* net_disp_node = NULL;
    if(src_net)
	net_disp_node = src_net->getRenderNodePtr();
    SOP_Node* sop_node = NULL;
    if(net_disp_node)
	sop_node = dynamic_cast<SOP_Node*>(net_disp_node);

    if ( myParentExporter->getExportOptions()->isSopExport() )
	sop_node = dynamic_cast<SOP_Node*>(source_node);

    if(sop_node)
    {
	GU_DetailHandle gdh;
	OP_Context	context(start_time);
	if (ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh))
	{
	    GU_DetailHandleAutoReadLock	 gdl(gdh);
	    const GU_Detail		*gdp = gdl.getGdp();
	    GU_Detail conv_gdp;
	    GA_PrimCompat::TypeMask prim_types = ROP_FBXUtil::getGdpPrimId(gdp);
	    const GU_Detail* final_detail = getExportableGeo(gdp, conv_gdp, prim_types);

	    if(final_detail)
	    {
		// See if we have any per-prim materials
		GA_ROAttributeRef attrOffset = final_detail->findStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL);
		const GA_Attribute *matPathAttr = attrOffset.getAttribute();
    	    
		const char *loc_mat_path = NULL;
		if(attrOffset.isValid())
		{
		    const GA_AIFStringTuple *stuple = attrOffset.getAIFStringTuple();
		    num_prims = final_detail->getNumPrimitives();
		    per_face_mats = new OP_Node* [num_prims];
		    memset(per_face_mats, 0, sizeof(OP_Node*)*num_prims);
		    per_face_mats_paths.setSize(num_prims);

		    int curr_prim_idx = 0;
		    GA_FOR_ALL_PRIMITIVES(final_detail, prim)
		    {
			loc_mat_path = stuple->getString(matPathAttr, prim->getMapOffset());
			// Find corresponding mat
			if(loc_mat_path)
			{
			    per_face_mats[curr_prim_idx] = source_node->findNode(loc_mat_path);
			}
			per_face_mats_paths[curr_prim_idx] = loc_mat_path;
			curr_prim_idx++;
		    }
		}
	    }
	}
    }
    
    // No materials found
    if(!per_face_mats && ( per_face_mats_paths.entries() < 0 ) && !main_mat_node)
	return;
    
    // If we have per-face materials, fill in the gaps with our regular material
    if ( main_mat_node )
    {
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    if(!per_face_mats[curr_prim])
		per_face_mats[curr_prim] = main_mat_node;
	}
    }

    // We're guaranteed not have materials on layers yet.
    FbxLayerContainer* node_attr = FbxCast<FbxLayerContainer>(fbx_node->GetNodeAttribute());
    if(!node_attr)
    {
	if(per_face_mats)
	    delete[] per_face_mats;
	return;
    }

    UT_ASSERT(node_attr);
    FbxLayer* mat_layer = node_attr->GetLayer(0);
    if(!mat_layer)
    {
	int new_idx = node_attr->CreateLayer();
	mat_layer = node_attr->GetLayer(new_idx);
    }


    FbxSurfaceMaterial* fbx_material;
    FbxLayerElementMaterial* temp_layer_elem;
    if(per_face_mats)
    {
	// Per-primitive materials
	temp_layer_elem = FbxLayerElementMaterial::Create(node_attr, "");
	temp_layer_elem->SetMappingMode(FbxLayerElement::eByPolygon);
	temp_layer_elem->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	mat_layer->SetMaterials(temp_layer_elem);

	// We need two material maps
	// One for existing material nodes...
	THdNodeIntMap mat_idx_map;
	THdNodeIntMap::iterator mi;

	// ... and one for material paths/names
	THdFbxStringIntMap mat_path_idx_map;
	THdFbxStringIntMap::iterator mpathi;

	int curr_fbx_mat_idx, curr_added_mats = 0;
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    bool material_found_in_path_map = false;
	    fbx_material = generateFbxMaterial(per_face_mats[curr_prim], myMaterialsMap);
	    if (!fbx_material)
	    {
		// couldnt find in the main map, look for the material in the backup map (named material)
		fbx_material = generateFbxMaterial(per_face_mats_paths[curr_prim], myStringMaterialMap);

		if (fbx_material)
		{
		    // The material was found in the path/name map
		    material_found_in_path_map = true;
		}
		else
		{
		    // If not found, use the default material
		    fbx_material = getDefaultMaterial(myMaterialsMap);
		}
	    }

	    if (!material_found_in_path_map)
	    {
		// See if the material is already in the main map (existing material nodes)
		mi = mat_idx_map.find(per_face_mats[curr_prim]);
		if (mi == mat_idx_map.end())
		{
		    // Add it to the map
		    mat_idx_map[per_face_mats[curr_prim]] = curr_added_mats;
		    fbx_node->AddMaterial(fbx_material);
		    curr_fbx_mat_idx = curr_added_mats;
		    curr_added_mats++;

		    createTexturesForMaterial(per_face_mats[curr_prim],
					      fbx_material, myTexturesMap);
		}
		else
		{
		    // Get its index
		    curr_fbx_mat_idx = mi->second;
		}
	    }
	    else
	    {
		// See if the material is already in the secondary map (path/names)
		mpathi = mat_path_idx_map.find(per_face_mats_paths[curr_prim].buffer());
		if (mpathi == mat_path_idx_map.end())
		{
		    // Add it to the map
		    mat_path_idx_map[per_face_mats_paths[curr_prim].buffer()] = curr_added_mats;
		    fbx_node->AddMaterial(fbx_material);
		    curr_fbx_mat_idx = curr_added_mats;
		    curr_added_mats++;

		    /*createTexturesForMaterial(per_face_mats[curr_prim],
					      fbx_material, myTexturesMap);*/
		}
		else
		{
		    // Get its index
		    curr_fbx_mat_idx = mpathi->second;
		}
	    }

	    // Set the indirect index
	    temp_layer_elem->GetIndexArray().Add(curr_fbx_mat_idx);
	}
    }
    else
    {
	// One material for the entire object
	// Set the value
	fbx_material = generateFbxMaterial(main_mat_node, myMaterialsMap);
	if(!fbx_material)
	    fbx_material = generateFbxMaterial(main_mat_path.buffer(), myStringMaterialMap);
	if(!fbx_material)
	    fbx_material = getDefaultMaterial(myMaterialsMap);
	
	createTexturesForMaterial(main_mat_node, fbx_material, myTexturesMap);

	temp_layer_elem = FbxLayerElementMaterial::Create(node_attr, "");
	temp_layer_elem->SetMappingMode(FbxLayerElement::eAllSame);
	temp_layer_elem->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	int mat_index = fbx_node->AddMaterial(fbx_material);
	mat_layer->SetMaterials(temp_layer_elem);
	temp_layer_elem->GetIndexArray().Add(mat_index);
    }

    if(per_face_mats)
	delete[] per_face_mats;
}
/********************************************************************************************************/
int
ROP_FBXMainVisitor::createTexturesForMaterial(OP_Node* mat_node, FbxSurfaceMaterial* fbx_material, THdFbxTextureMap& tex_map)
{
    if(!fbx_material)
	return 0;

    // See how many layers of textures are there
    OP_Node* surf_node = getSurfaceNodeFromMaterialNode(mat_node);
    int curr_texture, num_spec_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");
    int num_textures = 0;
    FbxTexture* fbx_texture;

    for(curr_texture = 0; curr_texture < num_spec_textures; curr_texture++)
    {
	if(isTexturePresent(mat_node, curr_texture, NULL))
	    num_textures++;
    }
    
    if(num_textures == 0)
	return 0;
    else if(num_textures == 1)
    {
	// Single-layer texture
	fbx_texture = generateFbxTexture(mat_node, 0, myTexturesMap);
	FbxProperty diffuse_prop = fbx_material->FindProperty( FbxSurfaceMaterial::sDiffuse );
	if( diffuse_prop.IsValid() && fbx_texture)
	    diffuse_prop.ConnectSrcObject( fbx_texture );
    }
    else
    {
	FbxProperty diffuse_prop = fbx_material->FindProperty( FbxSurfaceMaterial::sDiffuse );
	if( !diffuse_prop.IsValid() )
	    return 0;

	UT_String texture_name(UT_String::ALWAYS_DEEP);
	texture_name.sprintf("%s_ltexture", fbx_material->GetName());
	FbxLayeredTexture* layered_texture = FbxLayeredTexture::Create(mySDKManager, (const char*)texture_name);
	diffuse_prop.ConnectSrcObject( layered_texture );

	// Multi-layer texture
	/*fbx_default_texture =*/ getDefaultTexture(myTexturesMap);
	for(curr_texture = 0; curr_texture < num_spec_textures; curr_texture++)
	{
	    fbx_texture = generateFbxTexture(mat_node, curr_texture, myTexturesMap);
	    if(!fbx_texture)
		continue;
	    layered_texture->ConnectSrcObject( fbx_texture );
	    layered_texture->SetTextureBlendMode(curr_texture, FbxLayeredTexture::eTranslucent);
	}
    }

    return num_textures;
}
/********************************************************************************************************/
FbxTexture* 
ROP_FBXMainVisitor::getDefaultTexture(THdFbxTextureMap& tex_map)
{
    if(!myDefaultTexture)
    {
	myDefaultTexture = FbxTexture::Create(mySDKManager, (const char*)"default_texture");
	myDefaultTexture->SetMappingType(FbxTexture::eUV);
    }
    return myDefaultTexture;
}
/********************************************************************************************************/
FbxSurfaceMaterial* 
ROP_FBXMainVisitor::getDefaultMaterial(THdFbxMaterialMap& mat_map)
{
    if(!myDefaultMaterial)
    {
	// Create it

	float temp_col[3] = { 193.0f/255.0f,193.0f/255.0f,193.0f/255.0f};

	FbxSurfaceLambert* lamb_new_mat = FbxSurfaceLambert::Create(mySDKManager, (const char*)"default_material");
	//FbxColor temp_fbx_col;
	FbxDouble3 temp_fbx_col;

	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	//temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
	lamb_new_mat->Diffuse.Set(temp_fbx_col);
	lamb_new_mat->DiffuseFactor.Set(1.0);

	temp_col[0] = 0.0; 
	temp_col[1] = 0.0;
	temp_col[2] = 0.0;
	//temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	lamb_new_mat->Ambient.Set(temp_fbx_col);
	lamb_new_mat->AmbientFactor.Set(1.0);

	lamb_new_mat->Emissive.Set(temp_fbx_col);
	lamb_new_mat->EmissiveFactor.Set(1.0);

	myDefaultMaterial = lamb_new_mat;
    }

    return myDefaultMaterial;
}
/********************************************************************************************************/
OP_Node* 
ROP_FBXMainVisitor::getSurfaceNodeFromMaterialNode(OP_Node* material_node)
{
    if(!material_node)
	return NULL;

    VOP_Node* material_vop_node = CAST_VOPNODE(material_node);
    if( material_vop_node )
	return material_vop_node;

    OP_Node* surface_node = NULL;
    SHOP_Node* material_shop_node = CAST_SHOPNODE(material_node);
    if(!material_shop_node)
	return NULL;
    SHOP_Output* output_node = dynamic_cast<SHOP_Output*>(material_shop_node->getOutputNode());
    if(output_node)
    {
	// Find the surface input.
	int curr_input;
	for( curr_input = output_node->getConnectedInputIndex(-1); curr_input >= 0;
	    curr_input = output_node->getConnectedInputIndex(curr_input) )
	{
	    // Only traverse up real inputs, not reference inputs. But we
	    // do want to search up out of subnets, which getInput() does.
	    if(output_node->getInput(curr_input))
	    {
		if(output_node->getInputType(curr_input) == SHOP_SURFACE)
		{
		    surface_node = output_node->getInput(curr_input);
		    break;
		}
	    }
	}
    }
    else
	surface_node = material_shop_node;

    return surface_node;
}
/********************************************************************************************************/
bool 
ROP_FBXMainVisitor::isTexturePresent(OP_Node* mat_node, int texture_idx, UT_String* texture_path_out)
{
    if(!mat_node)
	return false;

    OP_Node* surface_node = getSurfaceNodeFromMaterialNode(mat_node);

    if(!surface_node)
	return false;

    UT_String text_parm_name(UT_String::ALWAYS_DEEP);
    UT_String texture_path, texture_name(UT_String::ALWAYS_DEEP);
    text_parm_name.sprintf("ogl_tex%d", texture_idx+1);
    ROP_FBXUtil::getStringOPParm(surface_node, (const char *)text_parm_name, texture_path, true);

    if(texture_path.isstring() == false || texture_path.length() == 0)
	return false;

    if(texture_path_out)
	*texture_path_out = texture_path;

    return true;
}
/********************************************************************************************************/
FbxTexture* 
ROP_FBXMainVisitor::generateFbxTexture(OP_Node* mat_node, int texture_idx, THdFbxTextureMap& tex_map)
{
    UT_String texture_path(UT_String::ALWAYS_DEEP);
    if(!isTexturePresent(mat_node, texture_idx, &texture_path))
	return NULL;

    const UT_String& mat_name = mat_node->getName();

    // Find the texture if it is already created
    const char* full_name = (const char*)texture_path;
    THdFbxTextureMap::iterator mi = tex_map.find(full_name);
    if(mi != tex_map.end())
	return mi->second;

    // Create the texture and set its properties
    UT_String texture_name(UT_String::ALWAYS_DEEP);
    texture_name.sprintf("%s_texture%d", (const char*)mat_name, texture_idx+1);    
    FbxFileTexture* new_tex = FbxFileTexture::Create(mySDKManager, (const char*)texture_name);
    new_tex->SetFileName((const char*)texture_path);
    new_tex->SetMappingType(FbxTexture::eUV);
    new_tex->SetMaterialUse(FbxFileTexture::eModelMaterial);
    new_tex->SetDefaultAlpha(1.0);

    tex_map[full_name] = new_tex;
    return new_tex;
}
/********************************************************************************************************/
FbxSurfaceMaterial*
ROP_FBXMainVisitor::generateFbxMaterial(const char * mat_string, THdFbxStringMaterialMap& mat_map)
{
    if (!mat_string)
	return NULL;

    // Find the material if it is already created
    THdFbxStringMaterialMap::iterator mi = mat_map.find(mat_string);
    if (mi != mat_map.end())
	return mi->second;

    // Create a new "default" material with the needed name
    float temp_col[3] = { 193.0f / 255.0f,193.0f / 255.0f,193.0f / 255.0f };

    FbxSurfaceLambert* lamb_new_mat = FbxSurfaceLambert::Create(mySDKManager, mat_string);

    FbxDouble3 temp_fbx_col;
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->Diffuse.Set(temp_fbx_col);
    lamb_new_mat->DiffuseFactor.Set(1.0);

    temp_col[0] = 0.0;
    temp_col[1] = 0.0;
    temp_col[2] = 0.0;

    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];

    lamb_new_mat->Ambient.Set(temp_fbx_col);
    lamb_new_mat->AmbientFactor.Set(1.0);

    lamb_new_mat->Emissive.Set(temp_fbx_col);
    lamb_new_mat->EmissiveFactor.Set(1.0);

    // Add the new material to our map
    mat_map[mat_string] = lamb_new_mat;
    return lamb_new_mat;
}

/********************************************************************************************************/
FbxSurfaceMaterial* 
ROP_FBXMainVisitor::generateFbxMaterial(OP_Node* mat_node, THdFbxMaterialMap& mat_map)
{
    if(!mat_node)
	return NULL;

    // Find the material if it is already created
    THdFbxMaterialMap::iterator mi = mat_map.find(mat_node);
    if(mi != mat_map.end())
	return mi->second;

    OP_Node* surface_node = getSurfaceNodeFromMaterialNode(mat_node);
    
    if(!surface_node)
	return NULL;

    bool did_find;
    fpreal temp_col[3];
    bool is_specular = false;
    FbxDouble3 temp_fbx_col;

    UT_String mat_name;
    ROP_FBXUtil::getNodeName(mat_node, mat_name, myNodeManager);

    ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 0, 0.0, &did_find);
    if(did_find)
	is_specular = true;    

    // We got the surface SHOP node. Get its OGL properties.
    FbxSurfacePhong* new_mat = NULL; 
    FbxSurfaceLambert* lamb_new_mat = NULL;
    if(is_specular)
    {
	new_mat = FbxSurfacePhong::Create(mySDKManager, (const char*)mat_name);
	lamb_new_mat = new_mat;
    }
    else
	lamb_new_mat = FbxSurfaceLambert::Create(mySDKManager, (const char*)mat_name);

    // Diffuse
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->Diffuse.Set(temp_fbx_col);
    lamb_new_mat->DiffuseFactor.Set(1.0);

    // Ambient
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->Ambient.Set(temp_fbx_col);
    lamb_new_mat->AmbientFactor.Set(1.0);

    // Emissive
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->Emissive.Set(temp_fbx_col);
    lamb_new_mat->EmissiveFactor.Set(1.0);

    if(new_mat)
    {
	// Specular
	temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 0);
	temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 1);
	temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 2);
	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	new_mat->Specular.Set(temp_fbx_col);
	new_mat->SpecularFactor.Set(1.0);

	// Shininess
	temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "shininess", 0, 0.0, &did_find);
	if(did_find)
	    temp_col[0] /= 100.0;
	else
	{
	    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_rough", 0, 0.0, &did_find);
	    if(!did_find)
		temp_col[0] = 1.0;
	}
	new_mat->Shininess.Set(temp_col[0]*100.0);
    }

    // Alpha
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_alpha", 0, 0.0, &did_find);
    if(!did_find)
	temp_col[0] = 1.0;
    lamb_new_mat->TransparencyFactor.Set(1.0 - temp_col[0]);
    temp_col[0] = 1.0;
    temp_col[1] = 1.0;
    temp_col[2] = 1.0;
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->TransparentColor.Set(temp_fbx_col);

    // Add the new material to our map
    mat_map[mat_node] = lamb_new_mat;
    return lamb_new_mat;
}
/********************************************************************************************************/
ROP_FBXCreateInstancesAction* 
ROP_FBXMainVisitor::getCreateInstancesAction(void)
{
    return myInstancesActionPtr;
}
/********************************************************************************************************/
static int
ROP_FBXgFindMappedName(const char *attr, const char *varname, void *data)
{
    string* str_orig_name = (string*)data;
    
    if(*str_orig_name == attr)
    {
	*str_orig_name = varname;
	return 0;
    }
    else
	return 1;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::setProperName(FbxLayerElement* fbx_layer_elem, const GU_Detail* gdp, const GA_Attribute* attr)
{
    if(!fbx_layer_elem || !attr || !attr->getName().isstring())
	return;

    // Try to get a mapped name
    string str_mapped_name(attr->getName());
    gdp->traverseVariableNames(ROP_FBXgFindMappedName, &str_mapped_name);

    if(str_mapped_name.length() <= 0)
	str_mapped_name = attr->getName();
    
    // Set it
    if(str_mapped_name.length() > 0)
	fbx_layer_elem->SetName(str_mapped_name.c_str());
}
/********************************************************************************************************/
const GU_Detail* 
ROP_FBXMainVisitor::getExportableGeo(const GU_Detail* gdp_orig, GU_Detail& conversion_spare, GA_PrimCompat::TypeMask &prim_types_in_out)
{
    if(!gdp_orig)
	return NULL;

    const GU_Detail* final_detail = gdp_orig;

    // Convert the types we don't natively export.
    GA_PrimCompat::TypeMask supported_types = GEO_PrimTypeCompat::GEOPRIMPOLY | GEO_PrimTypeCompat::GEOPRIMNURBCURVE | GEO_PrimTypeCompat::GEOPRIMBEZCURVE;

    if(myParentExporter->getExportOptions()->getConvertSurfaces() == false)
	supported_types |= ( GEO_PrimTypeCompat::GEOPRIMNURBSURF  | GEO_PrimTypeCompat::GEOPRIMBEZSURF ); 

    if (prim_types_in_out & (~supported_types))
    {
	// We have some primitives that are not supported
	float lod = myParentExporter->getExportOptions()->getPolyConvertLOD();
	conversion_spare.duplicate(*gdp_orig);
	GU_ConvertParms conv_parms;
	conv_parms.setFromType(GEO_PrimTypeCompat::GEOPRIMALL & (~supported_types));
	conv_parms.setToType(GEO_PrimTypeCompat::GEOPRIMPOLY);
	conv_parms.method.setULOD(lod);
	conv_parms.method.setVLOD(lod);
	conversion_spare.convert(conv_parms);
	final_detail = &conversion_spare;

	prim_types_in_out = ROP_FBXUtil::getGdpPrimId(final_detail);
    }

    // Don't export the boneCapture attribute since we regenerate this on import
    if (final_detail->findPointCaptureAttribute(GEO_Detail::CAPTURE_BONE) != nullptr)
    {
	if (final_detail != &conversion_spare)
	{
	    conversion_spare.duplicate(*gdp_orig);
	    final_detail = &conversion_spare;
	}
	conversion_spare.destroyPointCaptureAttribute(GEO_Detail::CAPTURE_BONE);
    }

    return final_detail;
}
/********************************************************************************************************/
// ROP_FBXMainNodeVisitInfo
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::ROP_FBXMainNodeVisitInfo(OP_Node* hd_node) : ROP_FBXBaseNodeVisitInfo(hd_node)
{
    myBoneLength = 0.0;
    myIsVisitingFromInstance = false;
}
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::~ROP_FBXMainNodeVisitInfo()
{

}
/********************************************************************************************************/
double 
ROP_FBXMainNodeVisitInfo::getBoneLength(void)
{
    return myBoneLength;
}
/********************************************************************************************************/
void 
ROP_FBXMainNodeVisitInfo::setBoneLength(double b_length)
{
    myBoneLength = b_length;
}
/********************************************************************************************************/
bool 
ROP_FBXMainNodeVisitInfo::getIsVisitingFromInstance(void)
{
    return myIsVisitingFromInstance;
}
/********************************************************************************************************/
void 
ROP_FBXMainNodeVisitInfo::setIsVisitingFromInstance(bool value)
{
    myIsVisitingFromInstance = value;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputBlendShapesNodesIn(OP_Node* node, const UT_String& node_name, OP_Node* skin_deform_node, bool& did_cancel_out, TFbxNodesVector& res_nodes, UT_Set<OP_Node*> *already_visited, ROP_FBXMainNodeVisitInfo* node_info)
{
    did_cancel_out = false;

    if (!node)
	return false;

    if(!already_visited)
	already_visited = new UT_Set<OP_Node*>();

    // Skip pass through nodes, marking as visited along the way
    const fpreal now = CHgetEvalTime();
    while (true)
    {
	already_visited->insert(node);
	OP_Node* pass_through = node->getPassThroughNode(now);
	if (!pass_through)
	{
	    if (node->getOperator()->getName() != "cache")
		break;
	    pass_through = node->getInput(0);
	}
	node = pass_through;
    }

    const UT_StringHolder &node_type = node->getOperator()->getName();
    if (node_type == "blendshapes")
	return outputBlendShapeNode(node, node_name, skin_deform_node, did_cancel_out, res_nodes, node_info);

    // We'll be looking for the blend shape node in the current node's input
    TFbxNodesVector current_res_nodes;
    UT_Set<OP_Node*> *local_already_visited = new UT_Set<OP_Node *>(*already_visited);
    for (int i = node->getConnectedInputIndex(-1); i >= 0; i = node->getConnectedInputIndex(i))
    {
	OP_Node* current_input = node->getInput(i);
	if (!current_input)
	    continue;

	UT_String current_input_name;
	ROP_FBXUtil::getNodeName(current_input, current_input_name, myNodeManager);

	bool found_allowed_only = false;
	if (ROP_FBXUtil::findOpInput(current_input, theBlendShapeNodeTypes, true, theAllowedInBetweenNodeTypes, &found_allowed_only, 0, local_already_visited))
	{
	    outputBlendShapesNodesIn(current_input, current_input_name, skin_deform_node, did_cancel_out, current_res_nodes, already_visited, node_info);
	}	    
	else if(node_type == "merge")
	{
	    // Handling merge node
	    SOP_Node* current_input_sop = dynamic_cast<SOP_Node*>(current_input);
	    if (!current_input_sop)
		continue;

	    outputSOPNodeWithoutVC(current_input_sop, current_input_name, skin_deform_node, did_cancel_out, current_res_nodes);
	}	    
    }

    if (node_type == "merge")
    {
	// we need to merge the results from the inputs into a single FBXNode
	FbxNode* merge_node = FbxNode::Create(mySDKManager, node_name);
	for (int curr_node = 0; curr_node < current_res_nodes.size(); curr_node++)
	{
	    FbxNode* currentFbxNode = current_res_nodes[curr_node].getFbxNode();
	    if (!currentFbxNode)
		continue;

	    merge_node->AddChild(currentFbxNode);
	}

	ROP_FBXConstructionInfo constr_info(merge_node);
	res_nodes.push_back(constr_info);
    }
    else
    {
	//
	for (int curr_node = 0; curr_node < current_res_nodes.size(); curr_node++)
	{
	    res_nodes.push_back(current_res_nodes[curr_node]);
	}
    }

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputBlendShapeNode(OP_Node* node, const UT_String& node_name, OP_Node* skin_deform_node, bool& did_cancel_out, TFbxNodesVector& res_nodes, ROP_FBXMainNodeVisitInfo* node_info)
{
    did_cancel_out = false;

    //SOP_Node* blend_shape_node = dynamic_cast<SOP_Node*>(node);
    SOP_BlendShapes* blend_shape_node = dynamic_cast<SOP_BlendShapes*>(node);
    if (!blend_shape_node)
	return false;

    //if (blend_shape_node->getOperator()->getName() != "blendshapes")
    //	return false;

    int num_blend_input = blend_shape_node->nInputs();
    if (num_blend_input < 1)
	return false;
            
    // The first input is the base mesh used for the blend shape
    OP_Node* current_input_node = blend_shape_node->getInput(0);
    SOP_Node* current_SOP_Node = dynamic_cast<SOP_Node*>(current_input_node);
    if (!current_SOP_Node)
	return false;

    FbxBlendShape* fbx_blend_shape = FbxBlendShape::Create(mySDKManager, blend_shape_node->getName());
    if (!fbx_blend_shape)
	return false;

    TFbxNodesVector current_fbx_res;
    if(!outputSOPNodeWithoutVC(current_SOP_Node, node_name, skin_deform_node, did_cancel_out, current_fbx_res))
	return false;

    if (current_fbx_res.size() < 1)
	return false;

    FbxNode* fbx_node = current_fbx_res[0].getFbxNode();
    if (!fbx_node)
	return false;

    FbxGeometry* fbx_geo = fbx_node->GetGeometry();
    if (!fbx_geo)
	return false;

    // Add the blend shape deformer to input 0
    fbx_geo->AddDeformer(fbx_blend_shape);

    for (int n = 0; n < current_fbx_res.size(); n++)
	res_nodes.push_back(current_fbx_res[n]);
    

    for (int current_input = 1; current_input < num_blend_input; current_input++)
    {
	current_input_node = blend_shape_node->getInput(current_input);
	current_SOP_Node = dynamic_cast<SOP_Node*>(current_input_node);
	if (!current_SOP_Node)
	    continue;	

	UT_String node_name;
	ROP_FBXUtil::getNodeName(current_SOP_Node, node_name, myNodeManager);

	// Add a blend shape channel for this input
	UT_String channel_name(node_name, UT_String::ALWAYS_DEEP);
	channel_name += "_channel";
	FbxBlendShapeChannel* blend_shape_channel = FbxBlendShapeChannel::Create(mySDKManager, (const char*)channel_name);
	if (!blend_shape_channel)
	    continue;
	    
	fbx_blend_shape->AddBlendShapeChannel(blend_shape_channel);

	// Get current value
	//float blend_value = 1.0f;// blend_shape_node->BLEND(current_input, 0);
	float blend_value = blend_shape_node->evalFloatInst("blend#", &current_input, 0, 0);

	if (current_SOP_Node->getOperator()->getName() == "sblend")
	{
	    // Each sequence blend input will be exported as an in-between
	    outputSequenceBlendNode(current_SOP_Node, (const char*)node_name, blend_shape_channel, res_nodes);
	}
	else
	{
	    // Output the deformed meshes to shapes
	    outputSOPNodeToShape(current_SOP_Node, (const char*)node_name, blend_shape_channel,100.0f, res_nodes);
	}

	// Setting the current shape value
	FbxDouble blend_value_percent = blend_value * 100.0;
	blend_shape_channel->DeformPercent.Set(blend_value_percent);
    }

    // Adding the blend shape node the construction info
    if(node_info)
	node_info->addBlendShapeNode(blend_shape_node);

    // and adding the pair Houdini/FbxNode to the node manager
    ROP_FBXMainNodeVisitInfo main_node_info(blend_shape_node);
    myNodeManager->addNodePair(blend_shape_node, fbx_node, main_node_info);

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputSOPNodeToShape(SOP_Node* sop_node, const char* node_name, FbxBlendShapeChannel* fbx_blend_shape_channel, const float& blend_percent, TFbxNodesVector& res_nodes)
{
    if (!sop_node || !fbx_blend_shape_channel)
	return false;

    // Exporting the geometry from the current input node
    fpreal geom_export_time = myParentExporter->getStartTime();

    GU_DetailHandle gdh;
    OP_Context	context(geom_export_time);
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh))
	return false;

    GU_DetailHandleAutoReadLock gdl(gdh);
    const GU_Detail* gdp = gdl.getGdp();
    if (!gdp)
	return false;

    // See what types we have in our GDP
    FbxShape* fbx_shape = FbxShape::Create(mySDKManager, node_name);

    if (!fbx_shape)
	return false;

    // Get the number of points
    int num_points = gdp->getNumPoints();
    fbx_shape->InitControlPoints(num_points);
    FbxVector4* fbx_control_points = fbx_shape->GetControlPoints();

    {
	GA_Index curr_point(0);
	GA_Offset ptoff;
	GA_FOR_ALL_PTOFF(gdp, ptoff)
	{
	    UT_Vector4 pos = gdp->getPos4(ptoff);
	    fbx_control_points[curr_point].Set(pos[0], pos[1], pos[2], pos[3]);
	    curr_point++;
	}
    }

    // Add the shape to the blend shape channel
    fbx_blend_shape_channel->AddTargetShape(fbx_shape, blend_percent);

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputSequenceBlendNode(SOP_Node* seq_blend_node, const char* node_name, FbxBlendShapeChannel* fbx_blend_shape_channel, TFbxNodesVector& res_nodes)
{
    if (!seq_blend_node)
	return false;

    if (seq_blend_node->getOperator()->getName() != "sblend")
	return false;

    if (!fbx_blend_shape_channel)
	return false;

    OP_Node* current_input_node = NULL;
    SOP_Node* current_SOP_Node = NULL;
    int num_input = seq_blend_node->nInputs();
    if (num_input < 1)
	return false;

    //UT_String sblend_name(UT_String::ALWAYS_DEEP, seq_blend_node->getName());   
    for (int current_input = 0; current_input < num_input; current_input++)
    {
	current_input_node = seq_blend_node->getInput(current_input);
	current_SOP_Node = dynamic_cast<SOP_Node*>(current_input_node);

	if (!current_SOP_Node)
	    continue;

	// Output the deformed meshes to shapes
	float current_blend_percent = ((float)(current_input + 1)) / (float)num_input * 100.0f;	
	outputSOPNodeToShape(current_SOP_Node, (const char*)node_name, fbx_blend_shape_channel, current_blend_percent, res_nodes);
    }

    return true;
}
/********************************************************************************************************/
