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

    float bone_length = 0.0;
    KFbxNode* res_new_node = NULL;
    KFbxNode* fbx_parent_node = NULL;
    if(node_info_in && node_info_in->getParentInfo() != NULL)
	fbx_parent_node = node_info_in->getParentInfo()->getFbxNode();
    else
	fbx_parent_node = myScene->GetRootNode();
    if(!fbx_parent_node)
	return ROP_FBXVisitorResultSkipSubtreeAndSubnet;

    ROP_FBXMainNodeVisitInfo *node_info = dynamic_cast<ROP_FBXMainNodeVisitInfo*>(node_info_in);

    // Determine which type of Houdini node this is
    string lookat_parm_name("lookatpath");
    UT_String node_type = node->getOperator()->getName();
    bool is_visible = node->getDisplay();
    ROP_FBXGDPCache *v_cache = NULL;
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
	    res_new_node = outputNullNode(node, node_info, fbx_parent_node);
	    res_type = ROP_FBXVisitorResultSkipSubnet;
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
	    // Export bones
	    res_new_node = outputBoneNode(node, node_info, fbx_parent_node, bone_length);
	    res_type = ROP_FBXVisitorResultSkipSubnet;
	}
    }

    if(!res_new_node)
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

	// Set the standard transformations
	setStandardTransforms(node, res_new_node, (lookatobjectpath.length() > 0), bone_length );

	// Add nodes to the map
	ROP_FBXNodeInfo& stored_node_pair = myNodeManager->addNodePair(node, res_new_node);
	stored_node_pair.setVertexCacheMethod(node_info->getVertexCacheMethod());
	stored_node_pair.setMaxObjectPoints(node_info->getMaxObjectPoints());
	stored_node_pair.setVertexCache(v_cache);

	// If there's a lookat object, queue up the action
	if(lookatobjectpath.length() > 0)
	{
	    // Get the actual node ptr
	    OP_Node *lookat_node = OPgetDirector()->findNode((const char*)lookatobjectpath);

	    // Queue up an object to look at. Post-action because it may not have been created yet.
	    if(lookat_node)
		myActionManager->addLookAtAction(res_new_node, lookat_node);
	}

	// Add it to the hierarchy
	if(node_info->GetSkeletonRootNode())
	    node_info->GetSkeletonRootNode()->AddChild(res_new_node);
	else
	    fbx_parent_node->AddChild(res_new_node);
	node_info_in->setFbxNode(res_new_node);
	
    }

    return res_type;
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXMainVisitor::outputBoneNode(OP_Node* node, ROP_FBXMainNodeVisitInfo* node_info, KFbxNode* parent_node, float& bone_length_out)
{
    UT_String node_name = node->getName();

    KFbxNode* res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
    KFbxSkeleton *res_attr = KFbxSkeleton::Create(mySDKManager, (const char*)node_name);
    res_node->SetNodeAttribute(res_attr);

    bool is_root = false;
    if(!parent_node || !parent_node->GetNodeAttribute())
	is_root = true;
    else
    {
        if(parent_node->GetNodeAttribute()->GetAttributeType() != KFbxNodeAttribute::eSKELETON)
	    is_root = true;
    }
    
    if(is_root)
    {
	// NOTE: We need to create an extra node here that will serve as a root.
	// In the future, we could possibly check for an existing null node root in the hierarchy.
	UT_String root_node_name(UT_String::ALWAYS_DEEP, node_name);
	root_node_name += "_root";

	KFbxNode* root_res_node = KFbxNode::Create(mySDKManager, (const char*)root_node_name);
	KFbxSkeleton *root_res_attr = KFbxSkeleton::Create(mySDKManager, (const char*)root_node_name);
	root_res_node->SetNodeAttribute(root_res_attr);

	root_res_attr->SetSkeletonType(KFbxSkeleton::eROOT);

	parent_node->AddChild(root_res_node);
	node_info->SetSkeletonRootNode(root_res_node);
    }

    res_attr->SetSkeletonType(KFbxSkeleton::eLIMB_NODE);

    // Get the bone's length
    bone_length_out = ROP_FBXUtil::getFloatOPParm(node, "length");

    // The transform on the node. We need to take the pre-transform transform (at the moment of bone creation)
    // multiply it by the actual transform, and then set it. We need to pass this bone length as the translation
    // in negative z to the code that will be setting the transform.


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

    temp_bool = ROP_FBXUtil::isVertexCacheable(op_net, found_particles);
    if(myParentExporter->getExportingAnimation() || found_particles)
	is_vertex_cacheable = temp_bool;

    int max_vc_verts = 0;
    float start_time = myParentExporter->getStartTime();
    float end_time = myParentExporter->getEndTime();

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
	max_vc_verts = ROP_FBXUtil::getMaxPointsOverAnimation(sop_node, start_time, end_time, v_cache_out);
#ifdef UT_DEBUG
	max_vert_cnt_end = clock();
	ROP_FBXdb_maxVertsCountingTime += (max_vert_cnt_end - max_vert_cnt_start);
#endif
    }
    else
	v_cache_out = NULL;

    GU_DetailHandle gdh;
    if (!ROP_FBXUtil::getGeometryHandle(sop_node, start_time, gdh))
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
    if(prim_type == GEOPRIMPOLY && !is_vertex_cacheable)
    {
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
	    //ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(gdp, conv_gdp);
	    final_detail = v_cache_out->getFrameGeometry(v_cache_out->getFirstFrame());
	    node_info->setVertexCacheMethod(ROP_FBXVertexCacheMethodGeometry);
	}
	else
	{
	    conv_gdp.duplicate(*gdp);
	    GU_ConvertParms conv_parms;
	    conv_parms.fromType = GEOPRIMALL;
	    conv_parms.toType = GEOPRIMPOLY;
	    conv_parms.method.setULOD(1.0);
	    conv_parms.method.setVLOD(1.0);
	    conv_gdp.convert(conv_parms);
	    final_detail = &conv_gdp;
	}
	res_attr = outputPolygons(final_detail, (const char*)node_name, max_vc_verts, node_info->getVertexCacheMethod());
    }

    if(is_vertex_cacheable)
    {
	// Cache this number so we don't painfully recompute it again when we get to
	// vertex caching.
	node_info->setMaxObjectPoints(max_vc_verts);
    }

    if(res_attr)
    {	
	res_node = KFbxNode::Create(mySDKManager, (const char*)node_name);
	res_node->SetNodeAttribute(res_attr);
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

    // For FBX, output one more point, otherwise Maya's importer won't import any polygons.
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
/*
	UT_Vector4 pts[3];
	pts[0] = gdp->points()(0)->getPos();
	pts[1] = gdp->points()(1)->getPos();
	pts[2] = gdp->points()(2)->getPos();
	for(;curr_point < max_points; curr_point++)
	{
	    fbx_control_points[curr_point].Set(pts[ (curr_point-num_points)%3 ][0],pts[(curr_point-num_points)%3][1],
		pts[(curr_point-num_points)%3][2],pts[(curr_point-num_points)%3][3]);
	}
	*/
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
    int curr_extra_vert;
    for(curr_extra_vert = num_points; curr_extra_vert < max_points; curr_extra_vert+=points_per_poly)
    {
	mesh_attr->BeginPolygon();
	num_verts = points_per_poly;
	for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	    mesh_attr->AddPolygon(curr_extra_vert + curr_vert);	    
	mesh_attr->EndPolygon();
    }

    // Now do attributes, or at least some of them

    int attr_offset;
    attr_offset = gdp->findPointAttrib(gdp->getStdAttributeName(GEO_ATTRIBUTE_NORMAL), sizeof(UT_Vector3), GB_ATTRIB_VECTOR);
    if(attr_offset >= 0)
    {
	// We have per-point normals
	exportPointNormals(mesh_attr, gdp, attr_offset);
    }
    else
    {
	// NOTE: We currently reverse polygon directions, which means vertex attributes will not map properly.
	attr_offset = gdp->findVertexAttrib(gdp->getStdAttributeName(GEO_ATTRIBUTE_NORMAL), sizeof(UT_Vector3), GB_ATTRIB_VECTOR);
	if(attr_offset >= 0)
	{
	    // We have per-vertex normals
	    exportVertexNormals(mesh_attr, gdp, attr_offset);
	}
    }


    return mesh_attr;
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::exportPointNormals(KFbxMesh* mesh_attr, const GU_Detail *gdp, int attr_offset)
{
    KFbxLayer* attr_layer = mesh_attr->GetLayer(0);
    if (attr_layer == NULL)
    {
	mesh_attr->CreateLayer();
	attr_layer = mesh_attr->GetLayer(0);
    }
    KFbxLayerElementNormal* layer_elem = mySDKManager->CreateKFbxLayerElementNormal("");
    layer_elem->SetMappingMode(KFbxLayerElement::eBY_CONTROL_POINT);
    layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);

    // Go over all points
    const GEO_Point* ppt;
    const UT_Vector3* vec3;
    KFbxVector4 fbx_vec4;
    FOR_ALL_ADDED_POINTS(gdp, gdp->points()(0), ppt)
    {
	vec3 = ppt->castAttribData<UT_Vector3>(attr_offset);
	fbx_vec4.Set(vec3->x(),vec3->y(),vec3->z());
	layer_elem->GetDirectArray().Add(fbx_vec4);
    }
    
    attr_layer->SetNormals(layer_elem);
}
/********************************************************************************************************/
void 
ROP_FBXMainVisitor::exportVertexNormals(KFbxMesh* mesh_attr, const GU_Detail *gdp, int attr_offset)
{
    KFbxLayer* attr_layer = mesh_attr->GetLayer(0);
    if (attr_layer == NULL)
    {
	mesh_attr->CreateLayer();
	attr_layer = mesh_attr->GetLayer(0);
    }
    KFbxLayerElementNormal* layer_elem = mySDKManager->CreateKFbxLayerElementNormal("");
    layer_elem->SetMappingMode(KFbxLayerElement::eBY_POLYGON_VERTEX);
    layer_elem->SetReferenceMode(KFbxLayerElement::eDIRECT);

    // Go over all vertices
    const GEO_Point* ppt;
    const UT_Vector3* vec3;
    KFbxVector4 fbx_vec4;
    const GEO_Primitive* prim;

    int curr_vert, num_verts;
    FOR_ALL_PRIMITIVES(gdp, prim)
    {
	num_verts = prim->getVertexCount();
	for(curr_vert = num_verts - 1; curr_vert >= 0 ; curr_vert--)
	{
	    vec3 = prim->getVertex(curr_vert).castAttribData<UT_Vector3>(attr_offset);
	    fbx_vec4.Set(vec3->x(),vec3->y(),vec3->z());
	    layer_elem->GetDirectArray().Add(fbx_vec4);
	}
    }

    attr_layer->SetNormals(layer_elem);
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
void 
ROP_FBXMainVisitor::setStandardTransforms(OP_Node* hd_node, KFbxNode* fbx_node, bool has_lookat_node, float bone_length)
{

    UT_Vector3 t,r,s;
    KFbxVector4 post_rotate, fbx_vec4;
    if(ROP_FBXUtil::getFinalTransforms(hd_node, has_lookat_node, bone_length, 0.0, t,r,s, &post_rotate))
    {
	fbx_node->SetPostRotation(KFbxNode::eSOURCE_SET,post_rotate);
	fbx_node->SetRotationActive(true);
    }

    fbx_vec4.Set(r[0], r[1], r[2]);
    fbx_node->SetDefaultR(fbx_vec4);
    
    fbx_vec4.Set(t[0],t[1],t[2]);
    fbx_node->SetDefaultT(fbx_vec4);

    fbx_vec4.Set(s[0],s[1],s[2]);
    fbx_node->SetDefaultS(fbx_vec4);

    fbx_node->SetRotationOrder(KFbxNode::eDESTINATION_SET, eEULER_XYZ);
}
/********************************************************************************************************/
// ROP_FBXMainNodeVisitInfo
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::ROP_FBXMainNodeVisitInfo(OP_Node* hd_node) : ROP_FBXBaseNodeVisitInfo(hd_node)
{
    mySkeletonRootNode = NULL;
}
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo::~ROP_FBXMainNodeVisitInfo()
{

}
/********************************************************************************************************/
KFbxNode* ROP_FBXMainNodeVisitInfo::GetSkeletonRootNode(void)
{
    return mySkeletonRootNode;
}
/********************************************************************************************************/
void ROP_FBXMainNodeVisitInfo::SetSkeletonRootNode(KFbxNode* node)
{
    mySkeletonRootNode = node;
}
/********************************************************************************************************/