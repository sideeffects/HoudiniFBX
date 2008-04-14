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

#include <UT/UT_Matrix4.h>
#include <OP/OP_Node.h>
#include <OP/OP_Network.h>
#include <OP/OP_Director.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_ConvertParms.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Utils.h>

#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Output.h>

#include "ROP_FBXActionManager.h"

#include "ROP_FBXUtil.h"

#ifdef UT_DEBUG
extern double ROP_FBXdb_maxVertsCountingTime;
#endif

/********************************************************************************************************/
ROP_FBXMainVisitor::ROP_FBXMainVisitor(ROP_FBXExporter* parent_exporter)
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
}
/********************************************************************************************************/
ROP_FBXMainVisitor::~ROP_FBXMainVisitor()
{

}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo* 
ROP_FBXMainVisitor::visitBegin(OP_Node* node)
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

    KFbxNode* res_new_node = NULL;
    KFbxNode* fbx_parent_node = NULL;
    if(node_info_in && node_info_in->getParentInfo() != NULL)
	fbx_parent_node = node_info_in->getParentInfo()->getFbxNode();
    else
	fbx_parent_node = myParentExporter->GetFBXRootNode();

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
    if(is_visible)
    {
	if(node_type == "geo")
	{
	    res_new_node = outputGeoNode(node, node_info, fbx_parent_node, v_cache);

	    // We don't need to dive into the geo node
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
		res_new_node = outputBoneNode(node, node_info, fbx_parent_node, true);
		res_type = ROP_FBXVisitorResultSkipSubnet;

		if(!is_joint_null_node && is_last_joint_node)
		    node_info->setBoneLength(0.0);
	    }
	    else
	    {
		// Regular null node.
		res_new_node = outputNullNode(node, node_info, fbx_parent_node);
		res_type = ROP_FBXVisitorResultSkipSubnet;
	    }
	}
	else if(node_type == "hlight")
	{
	    res_new_node = outputLightNode(node, node_info, fbx_parent_node);
	    res_type = ROP_FBXVisitorResultSkipSubnet;

	    // Light, of course, is special.
	    lookat_parm_name = "l_" + lookat_parm_name;	
	}
	else if(node_type == "cam")
	{
	    res_new_node = outputCameraNode(node, node_info, fbx_parent_node);
	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
	else if(node_type == "bone")
	{
	    // Export bones - only if this isn't a dummy bone for display only.
	    if(ROP_FBXUtil::isDummyBone(node) == false)
	    {
		res_new_node = outputBoneNode(node, node_info, fbx_parent_node, false);
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
	    int is_enabled = ROP_FBXUtil::getIntOPParm(node, "light_enable");
	    if(is_enabled)
	    {
		float amb_intensity;
		float amb_light_col[3];
		amb_intensity = ROP_FBXUtil::getFloatOPParm(node, "light_intensity");
		amb_light_col[0] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 0);
		amb_light_col[1] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 1);
		amb_light_col[2] = ROP_FBXUtil::getFloatOPParm(node, "light_color", 2);

		UT_Color this_amb_color;
		this_amb_color.setRGB(amb_light_col[0] * amb_intensity, amb_light_col[1] * amb_intensity, amb_light_col[2] * amb_intensity);
		myAmbientColor += this_amb_color;
	    }
	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
    }

    if(!res_new_node && !force_ignore_node)
    {
	// Treat everything else as a null node
	res_new_node = outputNullNode(node, node_info, fbx_parent_node);

	// We do this because some invisible nodes may have extremely complex subnets that will
	// take forever to export. Might make this an option later.
	if(!is_visible)
	    res_type = ROP_FBXVisitorResultSkipSubnet;
    }

    if(res_new_node && fbx_parent_node)
    {
	UT_String lookatobjectpath;
	ROP_FBXUtil::getStringOPParm(node, lookat_parm_name.c_str(), lookatobjectpath);

	res_new_node->SetVisibility(node->getDisplay());

	// Export materials
	exportMaterials(node, res_new_node);

	// Set the standard transformations
	float bone_length = 0.0;
	if(node_info_in && node_info_in->getParentInfo())
	    bone_length = dynamic_cast<ROP_FBXMainNodeVisitInfo *>(node_info_in->getParentInfo())->getBoneLength();
	ROP_FBXUtil::setStandardTransforms(node, res_new_node, (lookatobjectpath.length() > 0), bone_length );

	// Add nodes to the map
	ROP_FBXNodeInfo& stored_node_pair = myNodeManager->addNodePair(node, res_new_node);
	stored_node_pair.setVertexCacheMethod(node_info->getVertexCacheMethod());
	stored_node_pair.setMaxObjectPoints(node_info->getMaxObjectPoints());
	stored_node_pair.setVertexCache(v_cache);
	stored_node_pair.setVisitResultType(res_type);

	// If there's a lookat object, queue up the action
	if(lookatobjectpath.length() > 0)
	{
	    // Get the actual node ptr
	    OP_Node *lookat_node = node->findNode((const char*)lookatobjectpath);

	    // Queue up an object to look at. Post-action because it may not have been created yet.
	    if(lookat_node)
		myActionManager->addLookAtAction(res_new_node, lookat_node);
	}

	// Add it to the hierarchy
	fbx_parent_node->AddChild(res_new_node);
	node_info_in->setFbxNode(res_new_node);
	
    }

    return res_type;
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
	UT_String node_name = last_node->getName();
	node_name += "_end_effector";

	KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
	KFbxSkeleton *res_attr = KFbxSkeleton::Create(mySDKManager, (const char*)node_name);
	res_node->SetNodeAttribute(res_attr);
	res_attr->SetSkeletonType(KFbxSkeleton::eLIMB_NODE);

	res_node->SetVisibility(last_node->getDisplay());

	ROP_FBXUtil::setStandardTransforms(NULL, res_node, false, cast_info->getBoneLength() );

	if(last_node_info->getFbxNode())
	    last_node_info->getFbxNode()->AddChild(res_node);
    }
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXMainVisitor::outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, bool is_a_null)
{
    // NOTE: This may get called on a null nodes, as well, if they are being exported as joints.
    UT_String node_name = node->getName();

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
	bone_length = 1.0; // Some dummy value so the next joint knows it's not a root.
    else
	bone_length = ROP_FBXUtil::getFloatOPParm(node, "length");
    node_info->setBoneLength(bone_length);

    return res_node;
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXMainVisitor::outputGeoNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, ROP_FBXGDPCache *&v_cache_out)
{
    KFbxNode* res_node = NULL;
    OP_Network* op_net = dynamic_cast<OP_Network*>(node);
    if(!op_net)
	return res_node;
    OP_Node *rend_node = op_net->getRenderNodePtr();
    if(!rend_node)
	return res_node;
    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(rend_node);
    if(!sop_node)
	return res_node;

    bool temp_bool, found_particles;
    bool is_vertex_cacheable = false;
    OP_Node* skin_deform_node = NULL;

    temp_bool = ROP_FBXUtil::isVertexCacheable(op_net, found_particles);
    if(myParentExporter->getExportingAnimation() || found_particles)
	is_vertex_cacheable = temp_bool;

    int max_vc_verts = 0;
    float start_time = myParentExporter->getStartTime();
    float end_time = myParentExporter->getEndTime();

    // For now, only export skinning if we're not vertex cacheable.
    float geom_export_time = start_time;
    float capture_frame = 1.0;
    if(!is_vertex_cacheable)
    {
	// For now, only export skinning if we're not vertex cacheable.
	bool did_find_allowed_nodes_only = false;
	const char *skin_node_types[] = { "deform", 0};
	const char *allowed_inbetween_node_types[] = {"null", "switch", "subnet", "attribcomposite",
	    "attribcopy", "attribcreate", "attribmirror", "attribpromote", "attribreorient", 
	    "attribpromote", "attribstringedit", "attribute", 0};
	skin_deform_node = ROP_FBXUtil::findOpInput(rend_node, skin_node_types, true, allowed_inbetween_node_types, &did_find_allowed_nodes_only);

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
	    const char *capt_skin_node_types[] = { "capture", 0};
	    OP_Node *capture_node = ROP_FBXUtil::findOpInput(skin_deform_node, capt_skin_node_types, true, NULL, NULL);
	    if(capture_node)
	    {
		if(ROP_FBXUtil::getIntOPParm(capture_node, "usecaptpose") == false)
		{
		    capture_frame = ROP_FBXUtil::getFloatOPParm(capture_node, "captframe");
		}	
	    }

	    geom_export_time = CHgetManager()->getTime(capture_frame);
	}
    }

    // Check that we don't output skinning and vertex caching at the same time
    UT_ASSERT( ! (skin_deform_node && is_vertex_cacheable));

    // We need this here so that the number of points in the static geometry
    // matches the number of points in the vertex cache files. Otherwise Maya
    // crashes and burns.
    if(is_vertex_cacheable)
    {
#ifdef UT_DEBUG
	double max_vert_cnt_start, max_vert_cnt_end;
	max_vert_cnt_start = clock();
#endif
	v_cache_out = new ROP_FBXGDPCache();
	max_vc_verts = ROP_FBXUtil::getMaxPointsOverAnimation(sop_node, geom_export_time, end_time, 
	    myParentExporter->getExportOptions()->getPolyConvertLOD(),
	    myParentExporter->getExportOptions()->getDetectConstantPointCountObjects(), v_cache_out);
#ifdef UT_DEBUG
	max_vert_cnt_end = clock();
	ROP_FBXdb_maxVertsCountingTime += (max_vert_cnt_end - max_vert_cnt_start);
#endif
    }
    else
	v_cache_out = NULL;

    GU_DetailHandle gdh;
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, geom_export_time, gdh))
	return res_node;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail		*gdp = gdl.getGdp();
    if(!gdp)
	return res_node;

    // Try to guess from the first primitive what the SOP represents
    const GEO_Primitive *prim = NULL;
    unsigned prim_type = 0;
    if(gdp->primitives().entries() > 0)
    {
	prim = gdp->primitives()(0);
        prim_type = prim->getPrimitiveId();
    }

    KFbxNodeAttribute *res_attr = NULL;
    UT_String node_name = node->getName();
    if(prim_type == GEOPRIMPOLY && (!is_vertex_cacheable || v_cache_out->getIsNumPointsConstant())) 
    {
	if(is_vertex_cacheable && v_cache_out && v_cache_out->getIsNumPointsConstant())
	{
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometryConstant);
	    res_attr = outputPolygons(gdp, (const char*)node_name, 0, ROP_FBXVertexCacheMethodGeometryConstant);
	}
	else
	    res_attr = outputPolygons(gdp, (const char*)node_name, 0, ROP_FBXVertexCacheMethodNone);
    }
    else if(prim_type == GEOPRIMPART)
    {
	UT_ASSERT(v_cache_out && is_vertex_cacheable);

	// Particles.
	// We cleverly create a square for each particle, then. Use the cache.
	GU_Detail *final_detail;
	//ROP_FBXUtil::convertParticleGDPtoPolyGDP(gdp, max_vc_verts, conv_gdp);
	final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodParticles);
	res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());

	// TODO: Make sure we're not leaking vertices when we're done with this GDP.
    }
    else
    {
	// Convert
	GU_Detail conv_gdp, *final_detail;
	if(is_vertex_cacheable)
	{
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometry);
	    final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	}
	else
	{
	    float lod = myParentExporter->getExportOptions()->getPolyConvertLOD();

	    conv_gdp.duplicate(*gdp);
	    GU_ConvertParms conv_parms;
	    conv_parms.fromType = GEOPRIMALL;
	    conv_parms.toType = GEOPRIMPOLY;
	    conv_parms.method.setULOD(lod);
	    conv_parms.method.setVLOD(lod);
	    conv_gdp.convert(conv_parms);
	    final_detail = &conv_gdp;
	}
	res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());
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

    if(res_attr)
    {	
	res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
	res_node->SetNodeAttribute(res_attr);

	if(skin_deform_node)
	{
	    myActionManager->addSkinningAction(res_node, skin_deform_node, capture_frame);
	}
    }

    return res_node;
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
    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	mesh_attr->BeginPolygon();
	num_verts = prim->getVertexCount();
	for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	    mesh_attr->AddPolygon(prim->getVertex(curr_vert).getBasePt()->getNum());
	mesh_attr->EndPolygon();
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
ROP_FBXMainVisitor::getAndSetFBXLayerElement(KFbxLayer* attr_layer, ROP_FBXAttributeType attr_type, const GU_Detail* gdp, int attr_offset, 
					     KFbxLayerElement::EMappingMode mapping_mode)
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

	KFbxLayerElementNormal* temp_layer = mySDKManager->CreateKFbxLayerElementNormal("");
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
	KFbxLayerElementUV* temp_layer = mySDKManager->CreateKFbxLayerElementUV("");
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
	KFbxLayerElementVertexColor* temp_layer = mySDKManager->CreateKFbxLayerElementVertexColor("");
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
    KFbxLayerElementUserData *layer_elem = mySDKManager->CreateKFbxLayerElementUserData( (const char*)layer_name, layer_idx, custom_types_array, custom_names_array);
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
		res_elem = getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_CONTROL_POINT);
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
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_POLYGON_VERTEX);
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
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eBY_POLYGON);
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
	    getAndSetFBXLayerElement(attr_layer, curr_attr_type, gdp, attr_offset, KFbxLayerElement::eALL_SAME);
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
KFbxNode* 
ROP_FBXMainVisitor::outputNullNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node)
{
    UT_String node_name = node->getName();

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxNull *res_attr = KFbxNull::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    return res_node;
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXMainVisitor::outputLightNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node)
{
    float float_parm[3];
    int int_param;
    UT_String string_param;
    KFbxColor fbx_col;
    UT_String node_name = node->getName();

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxLight *res_attr = KFbxLight::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Set params
    KFbxLight::ELightType light_type;
    ROP_FBXUtil::getStringOPParm(node, "light_type", string_param);
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
    ROP_FBXUtil::getStringOPParm(node, "shadow_type", string_param);
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

    return res_node;
    
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXMainVisitor::outputCameraNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node)
{
    UT_String string_param;
    float float_parm[3];
    double fov_angle, foc_len;
    KFbxVector4 fbx_vec4;
    UT_String node_name = node->getName();

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxCamera *res_attr = KFbxCamera::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    // Projection type
    ROP_FBXUtil::getStringOPParm(node, "projection", string_param);
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
    ROP_FBXUtil::getStringOPParm(node, "focalunits", string_param);
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


    return res_node;
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
    ROP_FBXUtil::getStringOPParm(source_node, GEO_STD_ATTRIB_MATERIAL_PATH, main_mat_path);
    OP_Node* main_mat_node = NULL;
    if(main_mat_path.isstring())
	main_mat_node = source_node->findNode(main_mat_path);

    // See if there are any per-face indices
    float start_time = myParentExporter->getStartTime();
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
    if(per_face_mats)
    {
	// Per-primitive materials
	KFbxLayerElementMaterial* temp_layer_elem = mySDKManager->CreateKFbxLayerElementMaterial("");
	temp_layer_elem->SetMappingMode(KFbxLayerElement::eBY_POLYGON);
	temp_layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);
	mat_layer->SetMaterials(temp_layer_elem);

	for(curr_prim = 0; curr_prim < num_prims; curr_prim++)
	{
	    fbx_material = generateFbxMaterial(per_face_mats[curr_prim], myMaterialsMap);
	    if(!fbx_material)
		fbx_material = getDefaultMaterial(myMaterialsMap);
	    temp_layer_elem->GetDirectArray().Add(fbx_material);

	}
    }
    else
    {
	// One material for the entire object
	KFbxLayerElementMaterial* temp_layer_elem = mySDKManager->CreateKFbxLayerElementMaterial("");
	temp_layer_elem->SetMappingMode(KFbxLayerElement::eALL_SAME);
	temp_layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);
	mat_layer->SetMaterials(temp_layer_elem);

	// Set the value
	fbx_material = generateFbxMaterial(main_mat_node, myMaterialsMap);
	if(!fbx_material)
	    fbx_material = getDefaultMaterial(myMaterialsMap);
	temp_layer_elem->GetDirectArray().Add(fbx_material);
    }

    // Do textures
    KFbxLayer* tex_layer;
    KFbxTexture* fbx_texture, *fbx_default_texture;
    KFbxLayerElementTexture* temp_layer_elem;
    int curr_texture, num_textures;
    int last_layer_idx;
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
	    temp_layer_elem = mySDKManager->CreateKFbxLayerElementTexture("");
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
	// One texture layer for the entire thing. 
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

	    temp_layer_elem = mySDKManager->CreateKFbxLayerElementTexture("");
	    temp_layer_elem->SetMappingMode(KFbxLayerElement::eALL_SAME);
	    temp_layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);
	    temp_layer_elem->SetAlpha(1.0);
	    temp_layer_elem->SetBlendMode(KFbxLayerElementTexture::eTRANSLUCENT);
	    tex_layer->SetDiffuseTextures(temp_layer_elem);

	    // Set the value   
	    temp_layer_elem->GetDirectArray().Add(fbx_texture);
	}
    }

    if(per_face_mats)
	delete[] per_face_mats;
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
	KFbxColor temp_fbx_col;

	temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
	lamb_new_mat->GetDiffuseColor().Set(temp_fbx_col);
	lamb_new_mat->GetDiffuseFactor().Set(1.0);

	temp_col[0] = 0.0; 
	temp_col[1] = 0.0;
	temp_col[2] = 0.0;
	temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
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
KFbxTexture* 
ROP_FBXMainVisitor::generateFbxTexture(OP_Node* mat_node, int texture_idx, THdFbxTextureMap& tex_map)
{
    if(!mat_node)
	return NULL;

    OP_Node* surface_node = getSurfaceNodeFromMaterialNode(mat_node);

    if(!surface_node)
	return NULL;

    const UT_String& mat_name = mat_node->getName();
    UT_String text_parm_name(UT_String::ALWAYS_DEEP);
    UT_String texture_path, texture_name(UT_String::ALWAYS_DEEP);
    text_parm_name.sprintf("ogl_tex%d", texture_idx+1);
    ROP_FBXUtil::getStringOPParm(surface_node, (const char *)text_parm_name, texture_path, true);

    if(texture_path.isstring() == false)
	return NULL;

    // Find the texture if it is already created
    const char* full_name = (const char*)texture_path;
    THdFbxTextureMap::iterator mi = tex_map.find(full_name);
    if(mi != tex_map.end())
	return mi->second;

    // Create the texture and set its properties
    texture_name.sprintf("%s_texture%d", (const char*)mat_name, texture_idx+1);    
    KFbxTexture* new_tex = KFbxTexture::Create(mySDKManager, (const char*)texture_name);
    new_tex->SetFileName((const char*)texture_path);
    new_tex->SetMappingType(KFbxTexture::eUV);
    new_tex->SetMaterialUse(KFbxTexture::eMODEL_MATERIAL);

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
    const UT_String& mat_name = mat_node->getName();
    float temp_col[3];
    bool is_specular = false;
    KFbxColor temp_fbx_col;

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
    temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
    lamb_new_mat->GetDiffuseColor().Set(temp_fbx_col);
    lamb_new_mat->GetDiffuseFactor().Set(1.0);

    // Ambient
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_amb", 2);
    temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
    lamb_new_mat->GetAmbientColor().Set(temp_fbx_col);
    lamb_new_mat->GetAmbientFactor().Set(1.0);

    // Emissive
    temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 0);
    temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 1);
    temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_emit", 2);
    temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
    lamb_new_mat->GetEmissiveColor().Set(temp_fbx_col);
    lamb_new_mat->GetEmissiveFactor().Set(1.0);

    if(new_mat)
    {
	// Specular
	temp_col[0] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 0);
	temp_col[1] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 1);
	temp_col[2] = ROP_FBXUtil::getFloatOPParm(surface_node, "ogl_spec", 2);
	temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
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
    temp_fbx_col.Set(temp_col[0], temp_col[1], temp_col[2]);
    new_mat->GetTransparentColor().Set(temp_fbx_col);

    // Add the new material to our map
    mat_map[mat_node] = new_mat;
    return new_mat;
}
/********************************************************************************************************/
// ROP_FBXMainNodeVisitInfo
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::ROP_FBXMainNodeVisitInfo(OP_Node* hd_node) : ROP_FBXBaseNodeVisitInfo(hd_node)
{
    myBoneLength = 0.0;
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