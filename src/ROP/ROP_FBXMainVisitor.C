/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Oleg Samus
 *	Side Effects
  *	123 Front Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
 *
 * NAME:	ROP library (C++)
 *
 * COMMENTS:	Class for FBX output.
 *
 */

#include <fbx/fbxsdk.h>
#include "ROP_FBXMainVisitor.h"
#include "ROP_FBXExporter.h"

#include <UT/UT_Interrupt.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_BoundingRect.h>
#include <OP/OP_Node.h>
#include <OP/OP_Network.h>
#include <OP/OP_Director.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_ConvertParms.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimRBezCurve.h>
#include <GU/GU_PrimRBezSurf.h>
#include <GD/GD_TrimRegion.h>
#include <GD/GD_PrimRBezCurve.h>
#include <GD/GD_Detail.h>
#include <GD/GD_Face.h>
#include <GD/GD_PrimPoly.h>
#include <GD/GD_TrimPiece.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Profiles.h>
#include <GEO/GEO_TPSurf.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Utils.h>

#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Output.h>

#include "ROP_FBXActionManager.h"
#include "ROP_FBXDerivedActions.h"

#include "ROP_FBXUtil.h"

#ifdef UT_DEBUG
extern double ROP_FBXdb_maxVertsCountingTime;
#endif

/********************************************************************************************************/
ROP_FBXMainVisitor::ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter) 
: ROP_FBXBaseVisitor(parent_exporter->getExportOptions()->getInvisibleNodeExportMethod())
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

    //KFbxNode* res_new_node = NULL;
    KFbxNode* temp_new_node;
    TFbxNodesVector res_nodes;
    KFbxNode* fbx_parent_node = NULL;
    if(node_info_in && node_info_in->getParentInfo() != NULL)
	fbx_parent_node = node_info_in->getParentInfo()->getFbxNode();
    else
	fbx_parent_node = myParentExporter->GetFBXRootNode(node);

    if(!fbx_parent_node)
	return ROP_FBXVisitorResultSkipSubtreeAndSubnet;

    ROP_FBXMainNodeVisitInfo *node_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info_in);
    ROP_FBXMainNodeVisitInfo *parent_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info_in->getParentInfo());

    // Determine which type of Houdini node this is
    string lookat_parm_name("lookatpath");
    UT_String node_type = node->getOperator()->getName();
    bool is_visible = node->getDisplay();
    ROP_FBXGDPCache *v_cache = NULL;
    bool force_ignore_node = false;
    UT_String override_node_type(UT_String::ALWAYS_DEEP, "");
    if( (is_visible && myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportAsNulls) 
	|| myParentExporter->getExportOptions()->getInvisibleNodeExportMethod() == ROP_FBXInvisibleNodeExportFull
	|| node_type == "null" || node_type == "hlight" || node_type == "cam" || node_type == "bone" || node_type == "ambient")
    {
	if(node_type == "geo")
	{
	    bool did_cancel;
	    outputGeoNode(node, node_info, fbx_parent_node, v_cache, did_cancel, res_nodes);

	    // We don't need to dive into the geo node
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	    if(did_cancel)
	    {
		UT_ASSERT(res_nodes.size() == 0);
		return ROP_FBXVisitorResultAbort;
	    }
	}
	else if(node_type == "instance")
	{
	    // We need to create a node (in case we have children),
	    // *but* we need to attach the instance attribute later, as a post-action,
	    // in case it's not created at this point.
	    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
	    myNodeManager->makeNameUnique(node_name);
	    temp_new_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
	    res_nodes.push_back(temp_new_node);

	    if(!myInstancesActionPtr)	    
		myInstancesActionPtr = myActionManager->addCreateInstancesAction();
	    myInstancesActionPtr->addInstance(node, temp_new_node);

	    // See if we're an instance of a light or a camera, in which case
	    // we need to apply special transforms to this node.
	    OP_Node* inst_target = ROP_FBXUtil::findNonInstanceTargetFromInstance(node);
	    if(inst_target)
	    {
		UT_String inst_target_node_type = inst_target->getOperator()->getName();
		if(inst_target_node_type == "cam" || inst_target_node_type == "hlight")
		    override_node_type = inst_target_node_type;
	    }

	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
	else if(node_type == "null")
	{
	    // Null node
	    bool is_joint_null_node = ROP_FBXUtil::isJointNullNode(node);
	    bool is_last_joint_node = (parent_info && parent_info->getBoneLength() > 0.0);
	    if(is_joint_null_node || is_last_joint_node)
	    {
		// This is really a joint. Export it as such.
		outputBoneNode(node, node_info, fbx_parent_node, true, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;

		if(!is_joint_null_node && is_last_joint_node)
		    node_info->setBoneLength(0.0);
	    }
	    else
	    {
		// Regular null node.
		outputNullNode(node, node_info, fbx_parent_node, res_nodes);
		res_type = ROP_FBXVisitorResultSkipSubnet;
	    }
	}
	else if(node_type == "hlight")
	{
	    outputLightNode(node, node_info, fbx_parent_node, res_nodes);
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	    // Light, of course, is special.
	    lookat_parm_name = "l_" + lookat_parm_name;	
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
		float amb_intensity;
		float amb_light_col[3];
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
    }

    if(res_nodes.size() > 0 && fbx_parent_node)
    {
	UT_ASSERT(node_info);

	int curr_node, num_nodes = res_nodes.size();
	for(curr_node = 0; curr_node < num_nodes; curr_node++)
	{
	    finalizeNewNode(res_nodes[curr_node], node, node_info, fbx_parent_node, override_node_type,
		lookat_parm_name.c_str(), res_type, v_cache);
	}
    }

    return res_type;
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::finalizeNewNode(ROP_FBXConstructionInfo& constr_info, OP_Node* hd_node, ROP_FBXMainNodeVisitInfo *node_info, KFbxNode* fbx_parent_node, 
		UT_String& override_node_type, const char* lookat_parm_name, ROP_FBXVisitorResultType res_type,
		ROP_FBXGDPCache *v_cache)
{
    UT_ASSERT(lookat_parm_name);

    UT_String lookatobjectpath;
    ROP_FBXUtil::getStringOPParm(hd_node, lookat_parm_name, lookatobjectpath, true, myStartTime);

    KFbxNode* new_node = constr_info.getFbxNode();

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
	new_node->SetVisibility(hd_node->getDisplay());

	// Set the standard transformations (unless we're in the instance)
	float bone_length = 0.0;
	if(node_info && node_info->getParentInfo())
	{
	    bone_length = dynamic_cast<ROP_FBXMainNodeVisitInfo *>(node_info->getParentInfo())->getBoneLength();
	    if(dynamic_cast<ROP_FBXMainNodeVisitInfo *>(node_info->getParentInfo())->getIgnoreBoneLengthForTransforms())
		bone_length = 0;
	}
	UT_String* override_type_ptr = NULL;
	if(override_node_type.isstring())
	    override_type_ptr = &override_node_type;
	ROP_FBXUtil::setStandardTransforms(hd_node, new_node, (lookatobjectpath.length() > 0), bone_length, myStartTime, override_type_ptr);

	// If there's a lookat object, queue up the action
	if(lookatobjectpath.length() > 0)
	{
	    // Get the actual node ptr
	    OP_Node *lookat_node = hd_node->findNode((const char*)lookatobjectpath);

	    // Queue up an object to look at. Post-action because it may not have been created yet.
	    if(lookat_node)
		myActionManager->addLookAtAction(new_node, lookat_node);
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

    // Add it to the hierarchy
    if(!node_info->getIsVisitingFromInstance())
    {
	fbx_parent_node->AddChild(new_node);
	node_info->setFbxNode(new_node);
    }	

}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info)
{
    // We have to check if our previous bone length is not zero. If it isn't, that means we
    // have an end effector to create.
    ROP_FBXMainNodeVisitInfo* cast_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(last_node_info);
    if(cast_info && cast_info->getBoneLength() > 0.0 && ROP_FBXUtil::isDummyBone(last_node) == false && ROP_FBXUtil::isJointNullNode(last_node) == false)
    {
	// Create the end effector
	UT_String node_name(UT_String::ALWAYS_DEEP, last_node->getName());
	myNodeManager->makeNameUnique(node_name);
	node_name += "_end_effector";

	KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
	KFbxSkeleton *res_attr = KFbxSkeleton::Create(mySDKManager, (const char*)node_name);
	res_node->SetNodeAttribute(res_attr);
	res_attr->SetSkeletonType(KFbxSkeleton::eLIMB_NODE);

	res_node->SetVisibility(last_node->getDisplay());

	ROP_FBXUtil::setStandardTransforms(NULL, res_node, false, cast_info->getBoneLength(), myStartTime, NULL );

	if(last_node_info->getFbxNode())
	    last_node_info->getFbxNode()->AddChild(res_node);
    }
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, bool is_a_null, TFbxNodesVector& res_nodes)
{
    // NOTE: This may get called on a null nodes, as well, if they are being exported as joints.
    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
    myNodeManager->makeNameUnique(node_name);

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxSkeleton *res_attr = KFbxSkeleton::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    bool is_root = false;
    if(node_info && node_info->getParentInfo())
    {
	if(dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info)->getBoneLength() <= 0.0)
	    is_root = true;
    }

    if(is_root)
	res_attr->SetSkeletonType(KFbxSkeleton::eROOT);
    else
	res_attr->SetSkeletonType(KFbxSkeleton::eLIMB_NODE);
   
    // Get the bone's length
    float bone_length = 0.0;
    if(is_a_null)
    {
	bone_length = 1.0; // Some dummy value so the next joint knows it's not a root.
	node_info->setIgnoreBoneLengthForTransforms(true);
    }
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
ROP_FBXMainVisitor::outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, ROP_FBXGDPCache *&v_cache_out, bool& did_cancel_out, TFbxNodesVector& res_nodes)
{
    did_cancel_out = false;

    KFbxNode* res_node = NULL;
    OP_Network* op_net = dynamic_cast<OP_Network*>(node);
    if(!op_net)
	return false;
    OP_Node *rend_node = op_net->getRenderNodePtr();
    if(!rend_node)
	return false;
    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(rend_node);
    if(!sop_node)
	return false;

    bool temp_bool, found_particles;
    bool is_vertex_cacheable = false;
    OP_Node* skin_deform_node = NULL;

    temp_bool = ROP_FBXUtil::isVertexCacheable(op_net, myParentExporter->getExportOptions()->getExportDeformsAsVC(), myStartTime, found_particles);
    if(myParentExporter->getExportingAnimation() || found_particles)
	is_vertex_cacheable = temp_bool;

    int max_vc_verts = 0;
    float start_time = myParentExporter->getStartTime();
    float end_time = myParentExporter->getEndTime();

    // For now, only export skinning if we're not vertex cacheable.
    float geom_export_time = start_time;
    float capture_frame = CHgetFrameFromTime(start_time);  

    if(myParentExporter->getExportOptions()->getExportDeformsAsVC() == false && !is_vertex_cacheable)
    {
	// For now, only export skinning if we're not vertex cacheable.
	bool did_find_allowed_nodes_only = false;
	const char *const skin_node_types[] = { "deform", 0};
	skin_deform_node = ROP_FBXUtil::findOpInput(rend_node, skin_node_types, true, ROP_FBXallowed_inbetween_node_types, &did_find_allowed_nodes_only);

	if(!did_find_allowed_nodes_only && skin_deform_node)
	{
	    // We've found a deform node, but also other nodes that deform the mesh between this node and the deform node.
	    // Output as a vertex cache instead.
	    is_vertex_cacheable = true;
	    skin_deform_node = NULL;

	    // TODO: Set a flag here telling the vertex cache not to break up the mesh into individual triangles.
	}
	else if(skin_deform_node)
	{
	    // We're skinnable.
	    // Find the capture frame.
	    const char *const capt_skin_node_types[] = { "capture", 0};
	    OP_Node *capture_node = ROP_FBXUtil::findOpInput(skin_deform_node, capt_skin_node_types, true, NULL, NULL);
	    if(capture_node)
	    {
		if(ROP_FBXUtil::getIntOPParm(capture_node, "usecaptpose") == false)
		{
		    capture_frame = ROP_FBXUtil::getFloatOPParm(capture_node, "captframe");
		}	
	    }

	    geom_export_time = CHgetManager()->getTime(capture_frame);
	    is_vertex_cacheable = false;
	}
    }

    // Check that we don't output skinning and vertex caching at the same time
    UT_ASSERT( ! (skin_deform_node && is_vertex_cacheable));

    // We need this here so that the number of points in the static geometry
    // matches the number of points in the vertex cache files. Otherwise Maya
    // crashes and burns.
    bool is_pure_surfaces = false;
    if(is_vertex_cacheable)
    {
#ifdef UT_DEBUG
	double max_vert_cnt_start, max_vert_cnt_end;
	max_vert_cnt_start = clock();
#endif
	v_cache_out = new ROP_FBXGDPCache();

	OP_Node* node_to_use;
	if(node_info->getIsVisitingFromInstance() && node_info->getParentInfo())
	    node_to_use = node_info->getParentInfo()->getHdNode();
	else
	    node_to_use = sop_node;

	max_vc_verts = ROP_FBXUtil::getMaxPointsOverAnimation(node_to_use, geom_export_time, end_time, 
	    myParentExporter->getExportOptions()->getPolyConvertLOD(),
	    myParentExporter->getExportOptions()->getDetectConstantPointCountObjects(), 
	    myParentExporter->getExportOptions()->getConvertSurfaces(),
	    myBoss, v_cache_out, is_pure_surfaces);

	if(max_vc_verts < 0)
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
    }
    else
	v_cache_out = NULL;


    GU_DetailHandle gdh;
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, geom_export_time, gdh))
	return false;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail		*gdp = gdl.getGdp();
    if(!gdp)
	return false;

    // See what types we have in our GDP
    unsigned prim_type = ROP_FBXUtil::getGdpPrimId(gdp);

    KFbxNodeAttribute *res_attr = NULL;
    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
    if(is_vertex_cacheable)
    {
	// We can only cache these:
	// 1) Pure polygons;
	// 2) Pure NURBS;
	// 3) Pure Beziers;
	// 4) Anything else, converted to polygons.
	if(prim_type == GEOPRIMPOLY && v_cache_out->getIsNumPointsConstant())
	{
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometryConstant);
	    res_attr = outputPolygons(gdp, (const char*)node_name, 0, ROP_FBXVertexCacheMethodGeometryConstant);
	    finalizeGeoNode(res_attr, node_name, skin_deform_node, capture_frame, -1, res_nodes);
	}
	else if(prim_type == GEOPRIMPART)
	{
	    UT_ASSERT(v_cache_out && is_vertex_cacheable);

	    // Particles.
	    // We cleverly create a square for each particle, then. Use the cache.
	    GU_Detail *final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodParticles);
	    res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());
	    finalizeGeoNode(res_attr, node_name, skin_deform_node, capture_frame, -1, res_nodes);
	}
	else if(is_pure_surfaces && v_cache_out->getIsNumPointsConstant())
	{
	    // NURBS or Bezier surfaces
	    //GU_Detail *final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometryConstant);
	    node_info->setIsSurfacesOnly(true);

	    // Note: unfortunately, the order of these is important, and matters to the ROP_FBXAnimVisitor::fillVertexArray().
	    int prim_cntr = -1;
	    if(prim_type & GEOPRIMNURBSURF)
		outputNURBSSurfaces(gdp, (const char*)node_name, skin_deform_node, capture_frame, res_nodes, &prim_cntr);
	    if(prim_type & GEOPRIMBEZSURF)
		outputBezierSurfaces(gdp, (const char*)node_name, skin_deform_node, capture_frame, res_nodes, &prim_cntr);
	    if(prim_type & GEOPRIMBEZCURVE)
		outputBezierCurves(gdp, (const char*)node_name, skin_deform_node, capture_frame, res_nodes, &prim_cntr);
	    if(prim_type & GEOPRIMNURBCURVE)
		outputNURBSCurves(gdp, (const char*)node_name, skin_deform_node, capture_frame, res_nodes, &prim_cntr);
	}
	else // Mixed types
	{
	    // Convert
	    GU_Detail *final_detail;
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometry);
	    final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());	    
	    finalizeGeoNode(res_attr, node_name, skin_deform_node, capture_frame, -1, res_nodes);
	}
    }
    else
    {
	// Convert the types we don't natively export.
	unsigned supported_types = GEOPRIMPOLY | GEOPRIMNURBCURVE | GEOPRIMBEZCURVE;

	if(myParentExporter->getExportOptions()->getConvertSurfaces() == false)
	    supported_types |= ( GEOPRIMNURBSURF  | GEOPRIMBEZSURF );

	GU_Detail conv_gdp;
	const GU_Detail* final_detail;

	if( (prim_type & (~supported_types)) != 0)
	{
	    // We have some primitives that are not supported
	    float lod = myParentExporter->getExportOptions()->getPolyConvertLOD();
	    conv_gdp.duplicate(*gdp);
	    GU_ConvertParms conv_parms;
	    conv_parms.fromType = GEOPRIMALL & (~supported_types);
	    conv_parms.toType = GEOPRIMPOLY;
	    conv_parms.method.setULOD(lod);
	    conv_parms.method.setVLOD(lod);
	    conv_gdp.convert(conv_parms);
	    final_detail = &conv_gdp;

	    prim_type = ROP_FBXUtil::getGdpPrimId(final_detail);
	}
	else
	    final_detail = gdp;

	// No vertex caching. Output several separate nodes
	if( prim_type & GEOPRIMPOLY )
	{
	    // There are polygons in this gdp. Output them.
	    res_attr = outputPolygons(final_detail, (const char*)node_name, 0, ROP_FBXVertexCacheMethodNone);
	    finalizeGeoNode(res_attr, node_name, skin_deform_node, capture_frame, -1, res_nodes);

	    // Try output any polylines, if they exist. Unlike Houdini, they're a separate type in FBX.
	    // We ignore them in the about polygon function.
	    outputPolylines(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
	}
	if(prim_type & GEOPRIMNURBSURF)
	    outputNURBSSurfaces(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
	if(prim_type & GEOPRIMNURBCURVE)
	    outputNURBSCurves(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
	if(prim_type & GEOPRIMBEZCURVE)
	    outputBezierCurves(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
	if(prim_type & GEOPRIMBEZSURF)
	    outputBezierSurfaces(final_detail, (const char*)node_name, skin_deform_node, capture_frame, res_nodes);
    }

    if(is_vertex_cacheable)
    {
	// Cache this number so we don't painfully recompute it again when we get to
	// vertex caching.
	if(node_info->getVertexCacheMethod() == ROP_FBXVertexCacheMethodGeometryConstant)
	    node_info->setMaxObjectPoints(v_cache_out->getNumConstantPoints());
	else
	    node_info->setMaxObjectPoints(max_vc_verts);
    }

    return true;
}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::finalizeGeoNode(KFbxNodeAttribute *res_attr, const char* node_name_in, OP_Node* skin_deform_node, 
				    int capture_frame, int opt_prim_cnt, TFbxNodesVector& res_nodes)
{
    if(!res_attr || !node_name_in)
	return;

    UT_String node_name(UT_String::ALWAYS_DEEP, node_name_in);
    myNodeManager->makeNameUnique(node_name);

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
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
    KFbxNurbsSurface *nurbs_surf_attr;

    GEO_Primitive* prim;
    GU_PrimRBezSurf* hd_line;
    GU_PrimNURBSurf* hd_nurb;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    int prim_cnt = -1;
    if(prim_cntr)
	prim_cnt = *prim_cntr;
    int curr_prim, num_prims = copy_gdp.primitives().entries();

    for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
    {
	prim = copy_gdp.primitives()(curr_prim);
	if(prim->getPrimitiveId() != GEOPRIMBEZSURF)
	    continue;

	if(prim_cntr)
	    prim_cnt++;

	hd_line = dynamic_cast<GU_PrimRBezSurf*>(prim);
	if(!hd_line)
	{
	    UT_ASSERT(0);
	    continue;
	}

	hd_nurb = dynamic_cast<GU_PrimNURBSurf*>(hd_line->convertToNURBNew());
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
    KFbxNurbsCurve *nurbs_curve_attr;

    GEO_Primitive* prim;
    GU_PrimRBezCurve* hd_line;
    GU_PrimNURBCurve* hd_nurb;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    int prim_cnt = -1;
    if(prim_cntr)
	prim_cnt = *prim_cntr;
    int curr_prim, num_prims = copy_gdp.primitives().entries();

    for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
    {
	prim = copy_gdp.primitives()(curr_prim);
	if(prim->getPrimitiveId() != GEOPRIMBEZCURVE)
	    continue;

	prim_cnt++;

	hd_line = dynamic_cast<GU_PrimRBezCurve*>(prim);
	if(!hd_line)
	{
	    UT_ASSERT(0);
	    continue;
	}

	hd_nurb = dynamic_cast<GU_PrimNURBCurve*>(hd_line->convertToNURBNew());
	if(!hd_nurb)
	    continue;

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	nurbs_curve_attr = KFbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, curr_name, skin_deform_node, capture_frame, prim_cnt, res_nodes);

    }

    if(prim_cntr)
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
    KFbxNurbsCurve *nurbs_curve_attr;

    const GEO_Primitive* const_prim;
    const GU_PrimPoly* const_hd_line;
    GEO_Primitive* prim;
    GU_PrimPoly* hd_line;
    GU_PrimNURBCurve* hd_nurb;

    bool did_find_open = false;

    FOR_MASK_PRIMITIVES(gdp, const_prim, GEOPRIMPOLY)
    {
	const_hd_line = dynamic_cast<const GU_PrimPoly*>(const_prim);
	if(const_hd_line->isClosed() == false)
	{
	    did_find_open = true;
	    break;
	}
    }

    if(!did_find_open)
	return;

    GU_Detail copy_gdp;
    copy_gdp.duplicate(*gdp);

    int prim_cnt = -1;
    int curr_prim, num_prims = copy_gdp.primitives().entries();
    for(curr_prim = num_prims - 1; curr_prim >= 0; curr_prim--)
    {
	prim_cnt++;
	prim = copy_gdp.primitives()(curr_prim);
	if(prim->getPrimitiveId() != GEOPRIMPOLY)
	    continue;

	hd_line = dynamic_cast<GU_PrimPoly*>(prim);
	if(!hd_line)
	{
	    UT_ASSERT(0);
	    continue;
	}
	if(hd_line->isClosed())
	    continue;

	hd_nurb = dynamic_cast<GU_PrimNURBCurve*>(hd_line->convertToNURBNew(4));
	if(!hd_nurb)
	    continue;

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	nurbs_curve_attr = KFbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, curr_name, skin_deform_node, capture_frame, prim_cnt, res_nodes);
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
    KFbxNurbsCurve *nurbs_curve_attr;
    
    const GEO_Primitive* prim;
    const GU_PrimNURBCurve* hd_nurb;
    int prim_cnt = -1;
    if(prim_cntr)
	prim_cnt = *prim_cntr;
    FOR_MASK_PRIMITIVES(gdp, prim, GEOPRIMNURBCURVE)
    {
	prim_cnt++;
	hd_nurb = dynamic_cast<const GU_PrimNURBCurve*>(prim);
	if(!hd_nurb)
	{
	    UT_ASSERT(0);
	    continue;
	}

	// Generate the name
	curr_name.sprintf("%s%d", (const char*)orig_name, obj_cntr);
	obj_cntr++;

	nurbs_curve_attr = KFbxNurbsCurve::Create(mySDKManager, curr_name);
	setNURBSCurveInfo(nurbs_curve_attr, hd_nurb);
	finalizeGeoNode(nurbs_curve_attr, curr_name, skin_deform_node, capture_frame, prim_cnt, res_nodes);
    }

    if(prim_cntr)
	*prim_cntr = prim_cnt;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::setNURBSCurveInfo(KFbxNurbsCurve* nurbs_curve_attr, const GU_PrimNURBCurve* hd_nurb)
{
    GB_NUBBasis* basis;
    int point_count;
    KFbxNurbsCurve::EType curve_type;
    int curr_knot, num_knots;
    double *knot_vector;
    float* hd_knot_vector;
    UT_Vector4 temp_vec;
    KFbxVector4* fbx_points;
    int curr_point, num_points;

    nurbs_curve_attr->SetDimension(KFbxNurbsCurve::e3D);

    basis = dynamic_cast<GB_NUBBasis*>(hd_nurb->getBasis());
    point_count = hd_nurb->getVertexCount();

    if(hd_nurb->isClosed())
    {
	if(basis->interpolatesEnds())
	    curve_type = KFbxNurbsCurve::eCLOSED;
	else
	    curve_type = KFbxNurbsCurve::ePERIODIC;
    }
    else
	curve_type = KFbxNurbsCurve::eOPEN;

    nurbs_curve_attr->SetOrder(hd_nurb->getOrder());
    nurbs_curve_attr->InitControlPoints(point_count, curve_type);

    // Set the basis
    num_knots = nurbs_curve_attr->GetKnotCount();
    knot_vector = nurbs_curve_attr->GetKnotVector();
    hd_knot_vector = basis->getData();
    for(curr_knot = 0; curr_knot < num_knots; curr_knot++)
	knot_vector[curr_knot] = hd_knot_vector[curr_knot];


    fbx_points = nurbs_curve_attr->GetControlPoints();
    num_points = nurbs_curve_attr->GetControlPointsCount();

    for(curr_point = 0; curr_point < num_points; curr_point++)
    {
	temp_vec = hd_nurb->getVertex(curr_point).getPos();
	fbx_points[curr_point].Set(temp_vec[0],temp_vec[1],temp_vec[2],temp_vec[3]);
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::setTrimRegionInfo(GD_TrimRegion* region, KFbxTrimNurbsSurface *trim_nurbs_surf_attr, 
				      bool& have_fbx_region)
{
    GD_TrimLoop* loop;
    KFbxBoundary* fbx_boundary; 

    UT_BoundingRect brect(0,0,1,1);
    loop = region->getLoop(brect);

    // Add a boundary
    GD_TrimLoop* curr_loop = loop;
    GD_TrimPiece* curr_piece;

    bool is_clockwise;
    KFbxNurbsCurve* fbx_curve;
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

	fbx_boundary = KFbxBoundary::Create(mySDKManager, "");
	curr_piece = curr_loop->getPiece(NULL);
	while(curr_piece)
	{
	    fbx_curve = KFbxNurbsCurve::Create(mySDKManager, "");

	    unsigned face_id = curr_piece->getPrimitiveId();

	    if(face_id == GDPRIMBEZCURVE)
	    {
		GU_Detail temp_gdp;
		GU_PrimNURBSurf *temp_prim = dynamic_cast<GU_PrimNURBSurf *>(temp_gdp.appendPrimitive(GEOPRIMNURBSURF));
		GEO_TPSurf *tp_surf = dynamic_cast<GEO_TPSurf *>(temp_prim);
		GEO_Profiles* profiles = tp_surf->profiles(1);

		GD_PrimRBezCurve* temp_face;
		temp_face = dynamic_cast<GD_PrimRBezCurve*>(curr_piece->createFace(profiles));

		// Make a copy of the face and convert it to the GU_* Curve. We can then dump it to NURBS.
		if(temp_face)
		{
		    GU_PrimRBezCurve* bez_curve = GU_PrimRBezCurve::build(&temp_gdp,  temp_face->getVertexCount(), temp_face->getOrder(), temp_face->isClosed(), true);
		    UT_ASSERT(bez_curve);

		    // Copy the points
		    int curr_pt, num_pts = temp_face->getVertexCount();
		    UT_Vector3 temp_vec;
		    for(curr_pt = 0; curr_pt < num_pts; curr_pt++)
		    {
			temp_vec = temp_face->getVertex(curr_pt).getPos();
			temp_vec.z() = 0;
			bez_curve->getVertex(curr_pt).getPt()->setPos(temp_vec);
		    }

		    // Convert it to NURBS
		    GU_PrimNURBCurve* hd_nurb = dynamic_cast<GU_PrimNURBCurve*>(bez_curve->convertToNURBNew());
		    setNURBSCurveInfo(fbx_curve, hd_nurb);
		}
	    }
	    else if(face_id == GDPRIMPOLY)
	    {
		GU_Detail temp_gdp;
		GU_PrimNURBSurf *temp_prim = dynamic_cast<GU_PrimNURBSurf *>(temp_gdp.appendPrimitive(GEOPRIMNURBSURF));
		GEO_TPSurf *tp_surf = dynamic_cast<GEO_TPSurf *>(temp_prim);
		GEO_Profiles* profiles = tp_surf->profiles(1);

		GD_PrimPoly* temp_face;
		temp_face = dynamic_cast<GD_PrimPoly*>(curr_piece->createFace(profiles));
    
		if(temp_face)
		{
		    GU_PrimPoly* poly = GU_PrimPoly::build(&temp_gdp, temp_face->getVertexCount(), (temp_face->isClosed() ? GU_POLY_CLOSED : GU_POLY_OPEN), true);
		    if(poly)
		    {
			// Copy the points
			int curr_pt, num_pts = temp_face->getVertexCount();
			UT_Vector3 temp_vec;
			for(curr_pt = 0; curr_pt < num_pts; curr_pt++)
			{
			    temp_vec = temp_face->getVertex(curr_pt).getPos();
			    temp_vec.z() = 0;
			    poly->getVertex(curr_pt).getPt()->setPos(temp_vec);
			}
			// Convert it to NURBS
			GU_PrimNURBCurve* hd_nurb = dynamic_cast<GU_PrimNURBCurve*>(poly->convertToNURBNew(4));
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
    const GU_PrimNURBSurf* hd_nurb;

    bool have_profiles = false;
    FOR_MASK_PRIMITIVES(gdp, prim, GEOPRIMNURBSURF)
    {
	hd_nurb = dynamic_cast<const GU_PrimNURBSurf*>(prim);
	if(hd_nurb && hd_nurb->hasProfiles())
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

    FOR_MASK_PRIMITIVES(final_gdp, prim, GEOPRIMNURBSURF)
    {
	if(prim_cntr)
	    prim_cnt++;

	hd_nurb = dynamic_cast<const GU_PrimNURBSurf*>(prim);
	if(!hd_nurb)
	{
	    UT_ASSERT(0);
	    continue;
	}

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
ROP_FBXMainVisitor::outputSingleNURBSSurface(const GU_PrimNURBSurf* hd_nurb, const char* curr_name, OP_Node* skin_deform_node, 
					     int capture_frame, TFbxNodesVector& res_nodes, int prim_cnt)
{
    KFbxTrimNurbsSurface *trim_nurbs_surf_attr;
    KFbxNurbsSurface *nurbs_surf_attr;
    GU_PrimNURBSurf* nc_hd_nurb;

    // Output each NURB   
    if(hd_nurb->hasProfiles())
    {
	string temp_name(curr_name);
	temp_name += "_trim_surf";
	nurbs_surf_attr = KFbxNurbsSurface::Create(mySDKManager, temp_name.c_str());
	trim_nurbs_surf_attr = KFbxTrimNurbsSurface::Create(mySDKManager, curr_name);	
	trim_nurbs_surf_attr->SetNurbsSurface(nurbs_surf_attr);
    }
    else
	nurbs_surf_attr = KFbxNurbsSurface::Create(mySDKManager, curr_name);

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
	finalizeGeoNode(trim_nurbs_surf_attr, curr_name, skin_deform_node, capture_frame, prim_cnt, res_nodes);
    else
	finalizeGeoNode(nurbs_surf_attr, curr_name, skin_deform_node, capture_frame, prim_cnt, res_nodes);

}
/********************************************************************************************************/
void
ROP_FBXMainVisitor::setNURBSSurfaceInfo(KFbxNurbsSurface *nurbs_surf_attr, const GU_PrimNURBSurf* hd_nurb)
{
    GB_NUBBasis* curr_u_basis;
    GB_NUBBasis* curr_v_basis;
    int v_point_count, u_point_count;
    KFbxNurbsSurface::ENurbType u_type, v_type;
    int curr_knot, num_knots;
    double *knot_vector;
    float* hd_knot_vector;
    int i_row, i_col, i_idx;
    UT_Vector4 temp_vec;
    KFbxVector4* fbx_points;

    curr_u_basis = dynamic_cast<GB_NUBBasis*>(hd_nurb->getUBasis());
    curr_v_basis = dynamic_cast<GB_NUBBasis*>(hd_nurb->getVBasis());
    v_point_count = hd_nurb->getNumRows();
    u_point_count = hd_nurb->getNumCols();

    // Determine types
    if(hd_nurb->isWrappedU())
    {
	if(curr_u_basis->interpolatesEnds())
	    u_type = KFbxNurbsSurface::eCLOSED;
	else
	    u_type = KFbxNurbsSurface::ePERIODIC;
    }
    else
	u_type = KFbxNurbsSurface::eOPEN;

    if(hd_nurb->isWrappedV())
    {
	if(curr_v_basis->interpolatesEnds())
	    v_type = KFbxNurbsSurface::eCLOSED;
	else
	    v_type = KFbxNurbsSurface::ePERIODIC;
    }
    else
	v_type = KFbxNurbsSurface::eOPEN;

    nurbs_surf_attr->SetOrder(hd_nurb->getUOrder(), hd_nurb->getVOrder());
    nurbs_surf_attr->SetStep(2, 2);
    nurbs_surf_attr->InitControlPoints(u_point_count, u_type, v_point_count, v_type);

    // Set bases (which are all belong to us...)
    num_knots = nurbs_surf_attr->GetUKnotCount();
    knot_vector = nurbs_surf_attr->GetUKnotVector();
    hd_knot_vector = curr_u_basis->getData();
    for(curr_knot = 0; curr_knot < num_knots; curr_knot++)
	knot_vector[curr_knot] = hd_knot_vector[curr_knot];

    num_knots = nurbs_surf_attr->GetVKnotCount();
    knot_vector = nurbs_surf_attr->GetVKnotVector();
    hd_knot_vector = curr_v_basis->getData();
    for(curr_knot = 0; curr_knot < num_knots; curr_knot++)
	knot_vector[curr_knot] = hd_knot_vector[curr_knot];

    // Set control points
    fbx_points = nurbs_surf_attr->GetControlPoints();
    for(i_row = 0; i_row < u_point_count; i_row++)
    {
	for(i_col = 0; i_col < v_point_count; i_col++)
	{
	    i_idx = i_row*v_point_count + i_col;
	    temp_vec = hd_nurb->getVertex(i_idx).getPos();
	    fbx_points[i_idx].Set(temp_vec[0],temp_vec[1],temp_vec[2],temp_vec[3]);
	}
    }
}
/********************************************************************************************************/
KFbxNodeAttribute* 
ROP_FBXMainVisitor::outputPolygons(const GU_Detail* gdp, const char* node_name, int max_points, ROP_FBXVertexCacheMethodType vc_method)
{
    KFbxMesh* mesh_attr = KFbxMesh::Create(mySDKManager, node_name);

    // Get the number of points
    int curr_point, num_points = gdp->points().entries();
    const GEO_Point *ppt;
    const GEO_Primitive* prim;

    int points_per_poly = 0;
    if(vc_method == ROP_FBXVertexCacheMethodGeometry)
	points_per_poly = 3;
    else if(vc_method == ROP_FBXVertexCacheMethodParticles)
	points_per_poly = ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT;

    if(max_points < num_points)
	max_points = num_points;
    mesh_attr->InitControlPoints(max_points); //  + 1);
    KFbxVector4* fbx_control_points = mesh_attr->GetControlPoints();
    UT_Vector4 pos;

    curr_point = 0;
    if(gdp->points().entries() > 0)
    {
	FOR_ALL_ADDED_POINTS(gdp, gdp->points()(0), ppt)
	{
	    pos = ppt->getPos();
	    fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);
	    curr_point++;
	}
    }

    if(num_points > 2 && max_points > num_points)
    {
	for(;curr_point < max_points; curr_point++)
	{
	    //pos = gdp->points()(( curr_point - num_points) % num_points)->getPos();
	    //fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);
	    fbx_control_points[curr_point].Set(0,0,0);
	}
    }
    // And the last one:
    //fbx_control_points[curr_point].Set(pos[0],pos[1],pos[2],pos[3]);

    // Now set vertices
    int curr_vert, num_verts;
    FOR_MASK_PRIMITIVES(gdp, prim, GEOPRIMPOLY)
    {
	if(dynamic_cast<const GEO_PrimPoly*>(prim)->isClosed())
	{
	    mesh_attr->BeginPolygon();
	    num_verts = prim->getVertexCount();
	    for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
		mesh_attr->AddPolygon(prim->getVertex(curr_vert).getBasePt()->getNum());
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
    UT_String *base_name = curr_attr_name.base();

    // Now compare the base name against known standard names 
    if(*base_name == gdp->getStdAttributeName(GEO_ATTRIBUTE_NORMAL, 1))
	curr_type = ROP_FBXAttributeNormal;
    else if(*base_name == gdp->getStdAttributeName(GEO_ATTRIBUTE_TEXTURE, 1))
	curr_type = ROP_FBXAttributeUV;
    else if(*base_name == gdp->getStdAttributeName(GEO_ATTRIBUTE_DIFFUSE, 1))
	curr_type = ROP_FBXAttributeVertexColor;

    delete base_name;
    return curr_type;
}
/********************************************************************************************************/
// Template support functions
inline void ROP_FBXassignValues(const UT_Vector3& hd_vec3, KFbxVector4& fbx_vec4)
{
    fbx_vec4.Set(hd_vec3[0],hd_vec3[1],hd_vec3[2]);
}
inline void ROP_FBXassignValues(const UT_Vector3& hd_vec3, KFbxVector2& fbx_vec2)
{
    fbx_vec2.Set(hd_vec3[0],hd_vec3[1]);
}
inline void ROP_FBXassignValues(const UT_Vector3& hd_col, KFbxColor& fbx_col)
{
    fbx_col.Set(hd_col[0],hd_col[1],hd_col[2]);
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportPointAttribute(const GU_Detail *gdp, int attr_offset, KFbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Go over all points
    const GEO_Point* ppt;
    const HD_TYPE* hd_type;
    FBX_TYPE fbx_type;

    FOR_ALL_ADDED_POINTS(gdp, gdp->points()(0), ppt)
    {
	hd_type = ppt->template castAttribData<HD_TYPE>(attr_offset);

	if(hd_type)
	{
	    ROP_FBXassignValues(*hd_type, fbx_type);
	    layer_elem->GetDirectArray().Add(fbx_type);
	}
    }
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportVertexAttribute(const GU_Detail *gdp, int attr_offset, KFbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Maya crashes when we export vertex attributes in direct mode. Therefore, export in indirect.

    // Go over all vertices
    const GEO_Point* ppt;
    const HD_TYPE* hd_type;
    FBX_TYPE fbx_type;
    const GEO_Primitive* prim;

    bool is_indexed = (layer_elem->GetReferenceMode() != KFbxLayerElement::eDIRECT);
    
    int curr_arr_cntr = 0;
    int curr_vert, num_verts;
    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	num_verts = prim->getVertexCount();
	for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	{
	    hd_type = prim->getVertex(curr_vert).template castAttribData<HD_TYPE>(attr_offset);
	    if(hd_type)
	    {
		ROP_FBXassignValues(*hd_type, fbx_type);
		layer_elem->GetDirectArray().Add(fbx_type);
		if(is_indexed)
		    layer_elem->GetIndexArray().Add(curr_arr_cntr);
		curr_arr_cntr++;
	    }
	}
    }
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportPrimitiveAttribute(const GU_Detail *gdp, int attr_offset, KFbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Go over all vertices
    const GEO_Point* ppt;
    const HD_TYPE* hd_type;
    FBX_TYPE fbx_type;
    const GEO_Primitive* prim;

    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	hd_type = prim->template castAttribData<HD_TYPE>(attr_offset);
	if(hd_type)
	{
	    ROP_FBXassignValues(*hd_type, fbx_type);
	    layer_elem->GetDirectArray().Add(fbx_type);
	}
    }
}
/********************************************************************************************************/
template <class HD_TYPE, class FBX_TYPE>
void exportDetailAttribute(const GU_Detail *gdp, int attr_offset, KFbxLayerElementTemplate<FBX_TYPE>* layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Go over all vertices
    const GEO_Point* ppt;
    const HD_TYPE* hd_type;
    FBX_TYPE fbx_type;
    const GEO_Primitive* prim;

    hd_type = gdp->attribs().template castAttribData<HD_TYPE>(attr_offset);
    if(hd_type)
    {
	ROP_FBXassignValues(*hd_type, fbx_type);
	layer_elem->GetDirectArray().Add(fbx_type);
    }
}
/********************************************************************************************************/
KFbxLayerElement* 
ROP_FBXMainVisitor::getAndSetFBXLayerElement(KFbxLayer* attr_layer, ROP_FBXAttributeType attr_type, const GU_Detail* gdp, 
					     int attr_offset, KFbxLayerElement::EMappingMode mapping_mode, KFbxLayerContainer* layer_container)
{
    KFbxLayerElement::EReferenceMode ref_mode;

    // These are brutal hacks so that Maya's importer does not crash.
    if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
	ref_mode = KFbxLayerElement::eINDEX_TO_DIRECT;
    else
	ref_mode = KFbxLayerElement::eDIRECT;
    
    KFbxLayerElement* new_elem = NULL;
    if(attr_type == ROP_FBXAttributeNormal)
    {
	// Normals always have to be direct. Also for Maya.
	ref_mode = KFbxLayerElement::eDIRECT;

	KFbxLayerElementNormal* temp_layer = KFbxLayerElementNormal::Create(layer_container, "");
	//KFbxLayerElementNormal* temp_layer = mySDKManager->CreateKFbxLayerElementNormal("");
	temp_layer->SetMappingMode(mapping_mode);
	temp_layer->SetReferenceMode(ref_mode);
	attr_layer->SetNormals(temp_layer);
	new_elem = temp_layer;

	if(mapping_mode == KFbxLayerElement::eBY_CONTROL_POINT)
	    exportPointAttribute<UT_Vector3, KFbxVector4>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
	    exportVertexAttribute<UT_Vector3, KFbxVector4>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON)
	    exportPrimitiveAttribute<UT_Vector3, KFbxVector4>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eALL_SAME)
	    exportDetailAttribute<UT_Vector3, KFbxVector4>(gdp, attr_offset, temp_layer);
    }
    else if(attr_type == ROP_FBXAttributeUV)
    {
	//KFbxLayerElementUV* temp_layer = mySDKManager->CreateKFbxLayerElementUV("");
	KFbxLayerElementUV* temp_layer = KFbxLayerElementUV::Create(layer_container, "");
	temp_layer->SetMappingMode(mapping_mode);
	temp_layer->SetReferenceMode(ref_mode);
	attr_layer->SetUVs(temp_layer);
	new_elem = temp_layer;

	if(mapping_mode == KFbxLayerElement::eBY_CONTROL_POINT)
	    exportPointAttribute<UT_Vector3, KFbxVector2>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
	    exportVertexAttribute<UT_Vector3, KFbxVector2>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON)
	    exportPrimitiveAttribute<UT_Vector3, KFbxVector2>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eALL_SAME)
	    exportDetailAttribute<UT_Vector3, KFbxVector2>(gdp, attr_offset, temp_layer);

    }
    else if(attr_type == ROP_FBXAttributeVertexColor)
    {
	//KFbxLayerElementVertexColor* temp_layer = mySDKManager->CreateKFbxLayerElementVertexColor("");
	KFbxLayerElementVertexColor* temp_layer = KFbxLayerElementVertexColor::Create(layer_container, "");
	temp_layer->SetMappingMode(mapping_mode);
	temp_layer->SetReferenceMode(ref_mode);
	attr_layer->SetVertexColors(temp_layer);
	new_elem = temp_layer;

	if(mapping_mode == KFbxLayerElement::eBY_CONTROL_POINT)
	    exportPointAttribute<UT_Vector3, KFbxColor>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
	    exportVertexAttribute<UT_Vector3, KFbxColor>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eBY_POLYGON)
	    exportPrimitiveAttribute<UT_Vector3, KFbxColor>(gdp, attr_offset, temp_layer);
	else if(mapping_mode == KFbxLayerElement::eALL_SAME)
	    exportDetailAttribute<UT_Vector3, KFbxColor>(gdp, attr_offset, temp_layer);

    }

    return new_elem;
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserPointAttribute(const GU_Detail* gdp, GB_Attribute* attr, int attr_subindex, const char* fbx_prop_name, KFbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem || gdp->points().entries() <= 0 || !fbx_prop_name || strlen(fbx_prop_name) <= 0)
	return;

    // Go over all points
    const GEO_Point* ppt;
    SIMPLE_TYPE const * hd_type;
    int array_pos = 0;
    SIMPLE_TYPE val_copy;

    int attr_offset = gdp->findPointAttrib(attr);
    UT_ASSERT(attr_offset >= 0);
    if(attr_offset < 0)
	return;

    layer_elem->ResizeAllDirectArrays(gdp->points().entries());
    SIMPLE_TYPE *fbx_direct_array =(SIMPLE_TYPE *)(layer_elem->GetDirectArrayVoid(fbx_prop_name))->GetArray();
    FOR_ALL_ADDED_POINTS(gdp, gdp->points()(0), ppt)
    {
	hd_type = ppt->template castAttribData<SIMPLE_TYPE>(attr_offset);

	if(hd_type)
	    fbx_direct_array[array_pos] = hd_type[attr_subindex];
	else
	    fbx_direct_array[array_pos] = 0;
	array_pos++;
    }
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserVertexAttribute(const GU_Detail* gdp, GB_Attribute* attr, int attr_subindex, const char* fbx_prop_name, KFbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem || gdp->points().entries() <= 0 || !fbx_prop_name || strlen(fbx_prop_name) <= 0)
	return;

    // Go over all vertices
    const GEO_Primitive* prim;
    const GEO_Point* ppt;
    SIMPLE_TYPE const * hd_type;
    int array_pos = 0;
    int total_verts = 0;
    int curr_vert, num_verts;

    int attr_offset = gdp->findVertexAttrib(attr);
    UT_ASSERT(attr_offset >= 0);
    if(attr_offset < 0)
	return;

    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	num_verts = prim->getVertexCount();
	total_verts += num_verts;
    }

    layer_elem->ResizeAllDirectArrays(total_verts);
    SIMPLE_TYPE *fbx_direct_array =(SIMPLE_TYPE *)(layer_elem->GetDirectArrayVoid(fbx_prop_name))->GetArray();

    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	num_verts = prim->getVertexCount();
	for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	{
	    hd_type = prim->getVertex(curr_vert).template castAttribData<SIMPLE_TYPE>(attr_offset);
	    if(hd_type)
		fbx_direct_array[array_pos] = hd_type[attr_subindex];
	    else
		fbx_direct_array[array_pos] = 0;
	    array_pos++;
	}
    }
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserPrimitiveAttribute(const GU_Detail* gdp, GB_Attribute* attr, int attr_subindex, const char* fbx_prop_name, KFbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    // Go over all vertices
    const GEO_Primitive* prim;
    const GEO_Point* ppt;
    SIMPLE_TYPE const * hd_type;
    int array_pos = 0;

    int attr_offset = gdp->findPrimAttrib(attr);
    UT_ASSERT(attr_offset >= 0);
    if(attr_offset < 0)
	return;

    layer_elem->ResizeAllDirectArrays(gdp->primitives().entries());
    SIMPLE_TYPE *fbx_direct_array =(SIMPLE_TYPE *)(layer_elem->GetDirectArrayVoid(fbx_prop_name))->GetArray();

    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	hd_type = prim->template castAttribData<SIMPLE_TYPE>(attr_offset);
	if(hd_type)
	    fbx_direct_array[array_pos] = hd_type[attr_subindex];
	else
	    fbx_direct_array[array_pos] = 0;
	array_pos++;
    }
}
/********************************************************************************************************/
template < class SIMPLE_TYPE >
void exportUserDetailAttribute(const GU_Detail* gdp, GB_Attribute* attr, int attr_subindex, const char* fbx_prop_name, KFbxLayerElementUserData *layer_elem)
{
    if(!gdp || !layer_elem)
	return;

    int attr_offset = gdp->findAttrib(attr);
    UT_ASSERT(attr_offset >= 0);
    if(attr_offset < 0)
	return;

    // Go over all vertices
    const GEO_Primitive* prim;
    const GEO_Point* ppt;
    SIMPLE_TYPE const * hd_type;
    int array_pos = 0;

    layer_elem->ResizeAllDirectArrays(1);
    SIMPLE_TYPE *fbx_direct_array =(SIMPLE_TYPE *)(layer_elem->GetDirectArrayVoid(fbx_prop_name))->GetArray();

    hd_type = gdp->attribs().template castAttribData<SIMPLE_TYPE>(attr_offset);
    if(hd_type)
	fbx_direct_array[array_pos] = hd_type[attr_subindex];
    else
	fbx_direct_array[array_pos] = 0;
    array_pos++;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::addUserData(const GU_Detail* gdp, THDAttributeVector& hd_attribs, ROP_FBXAttributeLayerManager& attr_manager,
				KFbxMesh* mesh_attr, KFbxLayerElement::EMappingMode mapping_mode )
{

    if(hd_attribs.size() <= 0)
	return;

    // Arrays for custom attributes
    KArrayTemplate<KFbxDataType> custom_types_array;
    KArrayTemplate<const char*> custom_names_array;
    UT_String full_name(UT_String::ALWAYS_DEEP);
    UT_String suffix(UT_String::ALWAYS_DEEP);
    const char* vec_comps[] = { "x", "y", "z"};
    int curr_pos;
    GB_AttribType attr_type;
    int attr_size;
    const char* orig_name;
    GB_Attribute* attr;
    TStringVector attr_names;
    char* temp_name;

    // Iterate once, adding attribute names and types
    int num_supported_attribs = 0;
    int curr_hd_attr, num_hd_attrs = hd_attribs.size();
    for(curr_hd_attr = 0; curr_hd_attr < num_hd_attrs; curr_hd_attr++)
    {
	attr = hd_attribs[curr_hd_attr];

	// Get the attribute type
	attr_type = attr->getType();
	attr_size = attr->getSize();
	orig_name = attr->getName();

	for(curr_pos = 0; curr_pos < attr_size; curr_pos++)
	{
	    // Convert it to the appropriate FBX type.
	    // Note that apparent FBX doesn't support strings here. Ugh.
	    // We also have to break apart things like vectors.
	    suffix = "";
	    full_name = "";
	    if(attr_type == GB_ATTRIB_INT)
	    {
		custom_types_array.Add(DTInteger);
		suffix.sprintf("_%d", curr_pos);
	    }
	    else if(attr_type == GB_ATTRIB_FLOAT) 
	    {
		custom_types_array.Add(DTFloat);
		suffix.sprintf("_%d", curr_pos);
	    }
	    else if(attr_type == GB_ATTRIB_VECTOR)
	    {
		custom_types_array.Add(DTFloat);
		if(attr_size <= 3)
		    suffix.sprintf("_%s", vec_comps[curr_pos]);
		else
		    suffix.sprintf("_%d", curr_pos);
	    }

	    if(suffix.length() > 0)
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
	    }
	    attr_names.push_back((const char*)full_name);

	}
    } // over all attributes

    if(num_supported_attribs <= 0)
	return;

    // Now create the actual layer
    KFbxLayer* attr_layer;
    int layer_idx;
    UT_String layer_name(UT_String::ALWAYS_DEEP);
    attr_layer = attr_manager.getAttributeLayer(ROP_FBXAttributeUser, &layer_idx);
    layer_name.sprintf("UserDataLayer%d", layer_idx);
    //KFbxLayerElementUserData *layer_elem = mySDKManager->CreateKFbxLayerElementUserData( (const char*)layer_name, layer_idx, custom_types_array, custom_names_array);
    KFbxLayerElementUserData *layer_elem = KFbxLayerElementUserData::Create(mesh_attr, (const char*)layer_name, layer_idx, custom_types_array, custom_names_array);
    layer_elem->SetMappingMode(mapping_mode);
    attr_layer->SetUserData(layer_elem);
    int attr_name_pos = 0;

    // Add data to it
    for(curr_hd_attr = 0; curr_hd_attr < num_hd_attrs; curr_hd_attr++)
    {
	attr = hd_attribs[curr_hd_attr];

	// Get the attribute type
	attr_type = attr->getType();
	attr_size = attr->getSize();
	orig_name = attr->getName();

	for(curr_pos = 0; curr_pos < attr_size; curr_pos++)
	{
	    if(attr_type == GB_ATTRIB_INT)
	    {
		if(mapping_mode == KFbxLayerElement::eBY_CONTROL_POINT)
		    exportUserPointAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
		    exportUserVertexAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eBY_POLYGON)
		    exportUserPrimitiveAttribute<int>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eALL_SAME)
		    exportUserDetailAttribute<int>(gdp, attr,curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);		
	    }
	    else if(attr_type == GB_ATTRIB_FLOAT || attr_type == GB_ATTRIB_VECTOR)  
	    {
		if(mapping_mode == KFbxLayerElement::eBY_CONTROL_POINT)
		    exportUserPointAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eBY_POLYGON_VERTEX)
		    exportUserVertexAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eBY_POLYGON)
		    exportUserPrimitiveAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
		else if(mapping_mode == KFbxLayerElement::eALL_SAME)
		    exportUserDetailAttribute<float>(gdp, attr, curr_pos, attr_names[attr_name_pos].c_str(), layer_elem);
	    }

	    attr_name_pos++;
	}
    }
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::exportAttributes(const GU_Detail* gdp, KFbxMesh* mesh_attr)
{
    ROP_FBXAttributeLayerManager attr_manager(mesh_attr);
    GB_Attribute* attr;
    ROP_FBXAttributeType curr_attr_type;
    KFbxLayer* attr_layer;
    int attr_offset;
    KFbxLayerElement* res_elem;
    THDAttributeVector user_attribs;



    // Go through point attributes first.
    if(gdp->points().entries() > 0)
    {
	attr = gdp->pointAttribs().getHead();
	while(attr)
	{
	    // Determine the proper attribute type
	    curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	    if(curr_attr_type != ROP_FBXAttributeUser)
	    {
		// Get the appropriate layer
		attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
		UT_ASSERT(attr_layer);

		// Create an appropriate layer element and fill it with values
		attr_offset = gdp->findPointAttrib(attr);
		res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_CONTROL_POINT, mesh_attr);
	    }
	    else
		user_attribs.push_back(attr);
	    attr = (GB_Attribute *) attr->next();
	}
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, KFbxLayerElement::eBY_CONTROL_POINT);
    user_attribs.clear();

    // Go through vertex attributes
    attr = gdp->vertexAttribs().getHead();
    while(attr)
    {
	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findVertexAttrib(attr);
	    // Maya crashes when we export vertex attributes in direct mode. Therefore, export in indirect.
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_POLYGON_VERTEX, mesh_attr);
	}
	else
	    user_attribs.push_back(attr);

	attr = (GB_Attribute *) attr->next();
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, KFbxLayerElement::eBY_POLYGON_VERTEX);
    user_attribs.clear();

    // Primitive attributes
    attr = gdp->primitiveAttribs().getHead();
    while(attr)
    {
	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findPrimAttrib(attr);
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_POLYGON, mesh_attr);
	}
	else
	    user_attribs.push_back(attr);

	attr = (GB_Attribute *) attr->next();
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, KFbxLayerElement::eBY_POLYGON);
    user_attribs.clear();

    // Detail attributes
    attr = gdp->attribs().getHead();
    while(attr)
    {
	// Determine the proper attribute type
	curr_attr_type = getAttrTypeByName(gdp, attr->getName());

	if(curr_attr_type != ROP_FBXAttributeUser)
	{
	    // Get the appropriate layer
	    attr_layer = attr_manager.getAttributeLayer(curr_attr_type);
	    UT_ASSERT(attr_layer);

	    // Create an appropriate layer element
	    attr_offset = gdp->findAttrib(attr);
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eALL_SAME, mesh_attr);
	}
	else
	    user_attribs.push_back(attr);

	attr = (GB_Attribute *) attr->next();
    }

    // Process all custom attributes
    addUserData(gdp, user_attribs, attr_manager, mesh_attr, KFbxLayerElement::eALL_SAME);
    user_attribs.clear();
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
    myNodeManager->makeNameUnique(node_name);

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxNull *res_attr = KFbxNull::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    res_nodes.push_back(res_node);
    return true;
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    float float_parm[3];
    int int_param;
    UT_String string_param;
    KFbxColor fbx_col;
    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
    myNodeManager->makeNameUnique(node_name);

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxLight *res_attr = KFbxLight::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Set params
    KFbxLight::ELightType light_type;
    ROP_FBXUtil::getStringOPParm(node, "light_type", string_param, true);
    if(string_param == "point")
    {
	// Point light
	light_type = KFbxLight::ePOINT;
    }
    else if(string_param == "spot")
    {
	light_type = KFbxLight::eSPOT;
    }
    else if(string_param == "distant")
    {
	light_type = KFbxLight::eDIRECTIONAL;
    }
    else
    {	
	light_type = KFbxLight::ePOINT;
	myErrorManager->addError("Unsupported light type. Exporting as point light. Node: ", node_name, NULL);
    }
    res_attr->SetLightType(light_type);

    // Enable/disable flag
    int_param = ROP_FBXUtil::getIntOPParm(node, "light_enable");
    res_attr->SetCastLight((bool)int_param);

    // Cast shadows flag
    ROP_FBXUtil::getStringOPParm(node, "shadow_type", string_param, true);
    if(string_param == "off")
	res_attr->SetCastShadows(false);
    else
	res_attr->SetCastShadows(true);

    // Color
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 0);
    float_parm[1] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 1);
    float_parm[2] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 2);
    fbx_col.Set(float_parm[0], float_parm[1], float_parm[2]);
    res_attr->SetDefaultColor(fbx_col);

    // Intensity
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "light_intensity");
    res_attr->SetDefaultIntensity(float_parm[0]*100.0);

    // Cone angle
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "coneangle");
    res_attr->SetDefaultConeAngle(float_parm[0]);

    // Attenuation
    ROP_FBXUtil::getStringOPParm(node, "atten_type", string_param, true);
    KFbxLight::EDecayType decay_type = KFbxLight::eNONE;
    if(string_param == "quadratic")
	decay_type = KFbxLight::eQUADRATIC;
    else if(string_param == "half")
	decay_type = KFbxLight::eLINEAR;
    else if(string_param != "off")
    {
	// Unsupported attentuation type.
	myErrorManager->addError("Unsupported attenuation type. Node: ", node_name, NULL);
    }
    res_attr->SetDecayType(decay_type);

    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "atten_dist");
    res_attr->SetDecayStart(float_parm[0]*0.5);

    res_nodes.push_back(res_node);
    return true;
    
}
/********************************************************************************************************/
bool
ROP_FBXMainVisitor::outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, TFbxNodesVector& res_nodes)
{
    UT_String string_param;
    float float_parm[3];
    double fov_angle, foc_len;
    KFbxVector4 fbx_vec4;
    UT_String node_name(UT_String::ALWAYS_DEEP, node->getName());
    myNodeManager->makeNameUnique(node_name);

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxCamera *res_attr = KFbxCamera::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Projection type
    ROP_FBXUtil::getStringOPParm(node, "projection", string_param, true);
    KFbxCamera::ECameraProjectionType project_type;
    if(string_param == "ortho")
	project_type = KFbxCamera::eORTHOGONAL;
    else if(string_param == "perspective")
	project_type = KFbxCamera::ePERSPECTIVE;
    else
    {
	myErrorManager->addError("Unsupported camera projection type. Exporting as perspective camera. Node: ", node_name, NULL);
	project_type = KFbxCamera::ePERSPECTIVE;
    }
    res_attr->SetProjectionType(project_type);

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
    foc_len = float_parm[0];
    res_attr->SetApertureMode(KFbxCamera::eFOCAL_LENGTH);
    res_attr->SetFocalLength(float_parm[0]*length_mult);

    // Pixel ratio
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "aspect");
    res_attr->SetPixelRatio(float_parm[0]);

    // Up vector
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "up",0);
    float_parm[1] = ROP_FBXUtil::getFloatOPParm(node, "up",1);
    float_parm[2] = ROP_FBXUtil::getFloatOPParm(node, "up",2);
    fbx_vec4.Set(float_parm[0], float_parm[1], float_parm[2]);
    res_attr->SetUpVector(fbx_vec4);

    // Aperture. Let's hope "horizontal" means we think it means.
    res_attr->SetApertureMode(KFbxCamera::eHORIZONTAL);
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "aperture");
    fov_angle = SYSatan( (double)(float_parm[0])/(2.0*foc_len) ) * 2.0;
    fov_angle = fov_angle/M_PI * 180.0;
    res_attr->SetDefaultFieldOfView(fov_angle);

    // Near and far clip planes
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "near");
    res_attr->SetNearPlane(float_parm[0]);

    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "far");
    res_attr->SetFarPlane(float_parm[0]);

    // Ortho zoom
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "orthowidth");
    res_attr->SetOrthoZoom(float_parm[0]);

    // Focus distance
    float_parm[0] = ROP_FBXUtil::getFloatOPParm(node, "focus");
    res_attr->SetFocusDistanceSource(KFbxCamera::eSPECIFIC_DISTANCE);
    res_attr->SetSpecificDistance(float_parm[0]);

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
ROP_FBXMainVisitor::exportMaterials(OP_Node* source_node, KFbxNode* fbx_node)
{
//    OP_Director* op_director = OPgetDirector();

    UT_String main_mat_path;
    ROP_FBXUtil::getStringOPParm(source_node, GEO_STD_ATTRIB_MATERIAL_PATH, main_mat_path, true);
    OP_Node* main_mat_node = NULL;
    if(main_mat_path.isstring())
	main_mat_node = source_node->findNode(main_mat_path);

    // See if there are any per-face indices
    float start_time = myStartTime;
    int curr_prim, num_prims = 0;
    OP_Node **per_face_mats = NULL;

    const GEO_Primitive *prim;
    OP_Network* src_net = dynamic_cast<OP_Network*>(source_node);
    OP_Node* net_disp_node = NULL;
    if(src_net)
	net_disp_node = src_net->getRenderNodePtr();
    SOP_Node* sop_node = NULL;
    if(net_disp_node)
	sop_node = dynamic_cast<SOP_Node*>(net_disp_node);
    if(sop_node)
    {
	GU_DetailHandle gdh;
	if (ROP_FBXUtil::getGeometryHandle(sop_node, start_time, gdh))
	{
	    GU_DetailHandleAutoReadLock	 gdl(gdh);
	    const GU_Detail		*gdp = gdl.getGdp();
	    if(gdp)
	    {
		// See if we have any per-prim materials
		const GEO_PrimAttribDict &prim_attribs = gdp->primitiveAttribs();
		GB_Attribute *matPathAttr = prim_attribs.find(GEO_STD_ATTRIB_MATERIAL_PATH, GB_ATTRIB_INDEX);
		int attrOffset = -1;
		if (matPathAttr)
		    attrOffset = prim_attribs.getOffset(GEO_STD_ATTRIB_MATERIAL_PATH, GB_ATTRIB_INDEX);
    	    
		const char *loc_mat_path = NULL;
		if(attrOffset >= 0)
		{
		    num_prims = gdp->primitives().entries();
		    per_face_mats = new OP_Node* [num_prims];
		    memset(per_face_mats, 0, sizeof(OP_Node*)*num_prims);

		    int curr_prim_idx = 0;
		    FOR_ALL_PRIMITIVES(gdp, prim)
		    {
			const GB_AttributeData &attr_data = prim->getAttrib();
			int index = *reinterpret_cast<int*>(attr_data[attrOffset]);
			loc_mat_path = matPathAttr->getIndex(index);
			// Find corresponding mat
			if(loc_mat_path)
			{
			    per_face_mats[curr_prim_idx] = source_node->findNode(loc_mat_path);
			    curr_prim_idx++;
			}
		    }
		}
	    }
	}
    }

    // If we have per-face materials, fill in the gaps with our regular material
    if(!per_face_mats && !main_mat_node)
	return;
    
    for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
    {
	if(!per_face_mats[curr_prim])
	    per_face_mats[curr_prim] = main_mat_node;
    }

    // We're guaranteed not have materials on layers yet.
    KFbxLayerContainer* node_attr = dynamic_cast<KFbxLayerContainer*>(fbx_node->GetNodeAttribute());
    if(!node_attr)
    {
	if(per_face_mats)
	    delete[] per_face_mats;
	return;
    }

    UT_ASSERT(node_attr);
    KFbxLayer* mat_layer = node_attr->GetLayer(0);
    if(!mat_layer)
    {
	int new_idx = node_attr->CreateLayer();
	mat_layer = node_attr->GetLayer(new_idx);
    }


    KFbxSurfaceMaterial* fbx_material;
    KFbxLayerElementMaterial* temp_layer_elem;
    if(per_face_mats)
    {
	// Per-primitive materials
	temp_layer_elem = KFbxLayerElementMaterial::Create(node_attr, "");
	temp_layer_elem->SetMappingMode(KFbxLayerElement::eBY_POLYGON);
	temp_layer_elem->SetReferenceMode(KFbxLayerElement::eINDEX_TO_DIRECT);
	mat_layer->SetMaterials(temp_layer_elem);

	THdNodeIntMap mat_idx_map;
	THdNodeIntMap::iterator mi;
	int curr_fbx_mat_idx, curr_added_mats = 0;
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    fbx_material = generateFbxMaterial(per_face_mats[curr_prim], myMaterialsMap);
	    if(!fbx_material)
		fbx_material = getDefaultMaterial(myMaterialsMap);

	    // See if it's in the map
	    mi = mat_idx_map.find(per_face_mats[curr_prim]);
	    if(mi == mat_idx_map.end())
	    {
		// Add it to the map
		mat_idx_map[per_face_mats[curr_prim]] = curr_added_mats;
		fbx_node->AddMaterial(fbx_material);
		curr_fbx_mat_idx = curr_added_mats;
		curr_added_mats++;

		createTexturesForMaterial(per_face_mats[curr_prim], fbx_material, myTexturesMap);
	    }
	    else
	    {
		// Get its count
		curr_fbx_mat_idx = mi->second;
	    }

	    // Set the indirect index
	    temp_layer_elem->GetIndexArray().Add(curr_fbx_mat_idx);
	}
/*
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    fbx_material = generateFbxMaterial(per_face_mats[curr_prim], myMaterialsMap);
	    if(!fbx_material)
		fbx_material = getDefaultMaterial(myMaterialsMap);
	    temp_layer_elem->GetDirectArray().Add(fbx_material);
	}
*/
//	    lNode->AddMaterial(lMaterial);
    }
    else
    {
	// One material for the entire object
	// Set the value
	fbx_material = generateFbxMaterial(main_mat_node, myMaterialsMap);
	if(!fbx_material)
	    fbx_material = getDefaultMaterial(myMaterialsMap);
	
	createTexturesForMaterial(main_mat_node, fbx_material, myTexturesMap);

	temp_layer_elem = KFbxLayerElementMaterial::Create(node_attr, "");
	temp_layer_elem->SetMappingMode(KFbxLayerElement::eALL_SAME);
	temp_layer_elem->SetReferenceMode(KFbxLayerElement::eINDEX_TO_DIRECT);
	temp_layer_elem->GetDirectArray().Add(fbx_material);
	mat_layer->SetMaterials(temp_layer_elem);
	temp_layer_elem->GetIndexArray().Add(0);
    }

    // Do textures
/*
    KFbxLayer* tex_layer;
    KFbxTexture* fbx_texture, *fbx_default_texture;
    int curr_texture, num_textures;
    int last_layer_idx;
    KFbxLayerElementTexture* temp_layer_elem;

    // See if we have any textures to deal with

    OP_Node* surf_node = getSurfaceNodeFromMaterialNode(main_mat_node);
    num_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");

    if(!per_face_mats && fbx_material)
    {
	fbx_texture = generateFbxTexture(main_mat_node, 0, myTexturesMap);
	KFbxProperty diffuse_prop = fbx_material->FindProperty( KFbxSurfaceMaterial::sDiffuse );
	if( diffuse_prop.IsValid() && fbx_texture)
	    diffuse_prop.ConnectSrcObject( fbx_texture );

    }
*/
    /* // To be deleted...
    if(per_face_mats)
    {
	THdNodeSet loc_materials;
	THdNodeSet::iterator si;
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    // See if it's in the map
	    si = loc_materials.find(per_face_mats[curr_prim]);
	    if(si == loc_materials.end())
	    {
		// Add it to the set and export
		loc_materials.insert(per_face_mats[curr_prim]);
		createTexturesForMaterial(per_face_mats[curr_prim], temp_layer_elem->GetDirectArray()[curr_prim], myTexturesMap);
	    }
	}

    }
    else
    {
	createTexturesForMaterial(main_mat_node, fbx_material, myTexturesMap);
    }
    */


/*
    if(per_face_mats)
    {
	int curr_layer_idx;
	TFbxLayerElemsVector fbx_layer_elems;
	int curr_max_layers = 0;

	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    OP_Node* surf_node = getSurfaceNodeFromMaterialNode(per_face_mats[curr_prim]);
	    num_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");
	    if(curr_max_layers < num_textures)
		curr_max_layers = num_textures;
	}

	// Create all the layers that we'll need
	for(curr_layer_idx = 0; curr_layer_idx < curr_max_layers; curr_layer_idx++)
	{
	    tex_layer = node_attr->GetLayer(curr_layer_idx);
	    if(!tex_layer)
	    {
		last_layer_idx = node_attr->CreateLayer();
		tex_layer = node_attr->GetLayer(last_layer_idx);
	    }
	    //temp_layer_elem = mySDKManager->CreateKFbxLayerElementTexture("");
	    temp_layer_elem = KFbxLayerElementTexture::Create(node_attr, "");
	    temp_layer_elem->SetMappingMode(KFbxLayerElement::eBY_POLYGON);
	    temp_layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);
	    temp_layer_elem->SetAlpha(1.0);
	    temp_layer_elem->SetBlendMode(KFbxLayerElementTexture::eTRANSLUCENT);
	    tex_layer->SetDiffuseTextures(temp_layer_elem);

	    fbx_layer_elems.push_back(temp_layer_elem);
	}

	fbx_default_texture = getDefaultTexture(myTexturesMap);
	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    OP_Node* surf_node = getSurfaceNodeFromMaterialNode(per_face_mats[curr_prim]);
	    num_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");
	    for(curr_texture = 0; curr_texture < num_textures; curr_texture++)
	    {
		fbx_texture = generateFbxTexture(per_face_mats[curr_prim], curr_texture, myTexturesMap);
		if(!fbx_texture)
		    fbx_texture = fbx_default_texture;

		for(curr_layer_idx = 0; curr_layer_idx < curr_max_layers; curr_layer_idx++)
		{
		    // Set the value for all layers   
		    if(curr_layer_idx == curr_texture)
			temp_layer_elem->GetDirectArray().Add(fbx_texture);
		    else
			temp_layer_elem->GetDirectArray().Add(fbx_default_texture);
		}
	    }
	}
    }
    else
    {
	OP_Node* surf_node = getSurfaceNodeFromMaterialNode(main_mat_node);
	num_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");
	last_layer_idx = 0;

	for(curr_texture = 0; curr_texture < num_textures; curr_texture++)
	{
	    fbx_texture = generateFbxTexture(main_mat_node, curr_texture, myTexturesMap);
	    if(!fbx_texture)
		continue;

	    // Get the layer
	    tex_layer = node_attr->GetLayer(last_layer_idx);
	    if(!tex_layer)
	    {
		last_layer_idx = node_attr->CreateLayer();
		tex_layer = node_attr->GetLayer(last_layer_idx);
	    }
	    last_layer_idx++;

	    //temp_layer_elem = mySDKManager->CreateKFbxLayerElementTexture("");
	    temp_layer_elem = KFbxLayerElementTexture::Create(node_attr, "");
	    temp_layer_elem->SetMappingMode(KFbxLayerElement::eALL_SAME);
	    temp_layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);
	    temp_layer_elem->SetAlpha(1.0);
	    temp_layer_elem->SetBlendMode(KFbxLayerElementTexture::eTRANSLUCENT);
	    tex_layer->SetDiffuseTextures(temp_layer_elem);

	    // Set the value   
	    temp_layer_elem->GetDirectArray().Add(fbx_texture);
	}
    }
*/

    if(per_face_mats)
	delete[] per_face_mats;
}
/********************************************************************************************************/
int
ROP_FBXMainVisitor::createTexturesForMaterial(OP_Node* mat_node, KFbxSurfaceMaterial* fbx_material, THdFbxTextureMap& tex_map)
{
    if(!fbx_material)
	return 0;

    // See how many layers of textures are there
    OP_Node* surf_node = getSurfaceNodeFromMaterialNode(mat_node);
    int curr_texture, num_spec_textures = ROP_FBXUtil::getIntOPParm(surf_node, "ogl_numtex");
    int num_textures = 0;
    KFbxTexture* fbx_texture, *fbx_default_texture;

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
	KFbxProperty diffuse_prop = fbx_material->FindProperty( KFbxSurfaceMaterial::sDiffuse );
	if( diffuse_prop.IsValid() && fbx_texture)
	    diffuse_prop.ConnectSrcObject( fbx_texture );
    }
    else
    {
	KFbxProperty diffuse_prop = fbx_material->FindProperty( KFbxSurfaceMaterial::sDiffuse );
	if( !diffuse_prop.IsValid() )
	    return 0;

	UT_String texture_name(UT_String::ALWAYS_DEEP);
	texture_name.sprintf("%s_ltexture", fbx_material->GetName());
	KFbxLayeredTexture* layered_texture = KFbxLayeredTexture::Create(mySDKManager, (const char*)texture_name);
	diffuse_prop.ConnectSrcObject( layered_texture );

	// Multi-layer texture
	fbx_default_texture = getDefaultTexture(myTexturesMap);
	for(curr_texture = 0; curr_texture < num_spec_textures; curr_texture++)
	{
	    fbx_texture = generateFbxTexture(mat_node, curr_texture, myTexturesMap);
	    if(!fbx_texture)
		continue;
	    layered_texture->ConnectSrcObject( fbx_texture );
	    layered_texture->SetTextureBlendMode(curr_texture, KFbxLayeredTexture::eTRANSLUCENT);
	}
    }

    return num_textures;
}
/********************************************************************************************************/
KFbxTexture* 
ROP_FBXMainVisitor::getDefaultTexture(THdFbxTextureMap& tex_map)
{
    if(!myDefaultTexture)
    {
	myDefaultTexture = KFbxTexture::Create(mySDKManager, (const char*)"default_texture");
	myDefaultTexture->SetFileName((const char*)"");
	myDefaultTexture->SetMappingType(KFbxTexture::eUV);
	myDefaultTexture->SetMaterialUse(KFbxTexture::eMODEL_MATERIAL);
    }
    return myDefaultTexture;
}
/********************************************************************************************************/
KFbxSurfaceMaterial* 
ROP_FBXMainVisitor::getDefaultMaterial(THdFbxMaterialMap& mat_map)
{
    if(!myDefaultMaterial)
    {
	// Create it

	float temp_col[3] = { 193.0/255.0,193.0/255.0,193.0/255.0};

	KFbxSurfaceLambert* lamb_new_mat = KFbxSurfaceLambert::Create(mySDKManager, (const char*)"default_material");
	//KFbxColor temp_fbx_col;
	fbxDouble3 temp_fbx_col;

	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	//temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
	lamb_new_mat->GetDiffuseColor().Set(temp_fbx_col);
	lamb_new_mat->GetDiffuseFactor().Set(1.0);

	temp_col[0] = 0.0; 
	temp_col[1] = 0.0;
	temp_col[2] = 0.0;
	//temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	lamb_new_mat->GetAmbientColor().Set(temp_fbx_col);
	lamb_new_mat->GetAmbientFactor().Set(1.0);

	lamb_new_mat->GetEmissiveColor().Set(temp_fbx_col);
	lamb_new_mat->GetEmissiveFactor().Set(1.0);

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

    OP_Node* surface_node = NULL;
    SHOP_Node* material_shop_node = dynamic_cast<SHOP_Node*>(material_node);
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

    const UT_String& mat_name = mat_node->getName();
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
KFbxTexture* 
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
    KFbxTexture* new_tex = KFbxTexture::Create(mySDKManager, (const char*)texture_name);
    new_tex->SetFileName((const char*)texture_path);
    new_tex->SetMappingType(KFbxTexture::eUV);
    new_tex->SetMaterialUse(KFbxTexture::eMODEL_MATERIAL);
    new_tex->SetDefaultAlpha(1.0);

    tex_map[full_name] = new_tex;
    return new_tex;
}
/********************************************************************************************************/
KFbxSurfaceMaterial* 
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
    UT_String mat_name(UT_String::ALWAYS_DEEP, mat_node->getName());
    myNodeManager->makeNameUnique(mat_name);
    float temp_col[3];
    bool is_specular = false;
    fbxDouble3 temp_fbx_col;

    ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 0, 0.0, &did_find);
    if(did_find)
	is_specular = true;
    

    // We got the surface SHOP node. Get its OGL properties.
    KFbxSurfacePhong* new_mat = NULL; 
    KFbxSurfaceLambert* lamb_new_mat = NULL;
    if(is_specular)
    {
	new_mat = KFbxSurfacePhong::Create(mySDKManager, (const char*)mat_name);
	lamb_new_mat = new_mat;
    }
    else
	lamb_new_mat = KFbxSurfaceLambert::Create(mySDKManager, (const char*)mat_name);

    // Diffuse
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_diff", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->GetDiffuseColor().Set(temp_fbx_col);
    lamb_new_mat->GetDiffuseFactor().Set(1.0);

    // Ambient
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->GetAmbientColor().Set(temp_fbx_col);
    lamb_new_mat->GetAmbientFactor().Set(1.0);

    // Emissive
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 2);
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    lamb_new_mat->GetEmissiveColor().Set(temp_fbx_col);
    lamb_new_mat->GetEmissiveFactor().Set(1.0);

    if(new_mat)
    {
	// Specular
	temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 0);
	temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 1);
	temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 2);
	temp_fbx_col[0] = temp_col[0];
	temp_fbx_col[1] = temp_col[1];
	temp_fbx_col[2] = temp_col[2];
	new_mat->GetSpecularColor().Set(temp_fbx_col);
	new_mat->GetSpecularFactor().Set(1.0);

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
	new_mat->GetShininess().Set(temp_col[0]*100.0);
    }
    
    // Alpha
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_alpha", 0);
    new_mat->GetTransparencyFactor().Set(1.0 - temp_col[0]);
    temp_col[0] = 1.0;
    temp_col[1] = 1.0;
    temp_col[2] = 1.0;
    temp_fbx_col[0] = temp_col[0];
    temp_fbx_col[1] = temp_col[1];
    temp_fbx_col[2] = temp_col[2];
    new_mat->GetTransparentColor().Set(temp_fbx_col);

    // Add the new material to our map
    mat_map[mat_node] = new_mat;
    return new_mat;
}
/********************************************************************************************************/
ROP_FBXCreateInstancesAction* 
ROP_FBXMainVisitor::getCreateInstancesAction(void)
{
    return myInstancesActionPtr;
}
/********************************************************************************************************/
// ROP_FBXMainNodeVisitInfo
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::ROP_FBXMainNodeVisitInfo(OP_Node* hd_node) : ROP_FBXBaseNodeVisitInfo(hd_node)
{
    myBoneLength = 0.0;
    myIsVisitingFromInstance = false;
    myIgnoreLengthForTransforms = false;
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
ROP_FBXMainNodeVisitInfo::getIgnoreBoneLengthForTransforms(void)
{
    return myIgnoreLengthForTransforms;
}
/********************************************************************************************************/
void 
ROP_FBXMainNodeVisitInfo::setIgnoreBoneLengthForTransforms(bool bValue)
{
    myIgnoreLengthForTransforms = bValue;
}
/********************************************************************************************************/
