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

#include "ROP_FBXUtil.h"
#include <GU/GU_DetailHandle.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <CH/CH_Manager.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_ConvertParms.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>
#include <PRM/PRM_Parm.h>

#ifdef UT_DEBUG
#include <UT/UT_Debug.h>
#include <time.h>
extern double ROP_FBXdb_cookingTime;

extern double ROP_FBXdb_convexTime;
extern double ROP_FBXdb_reorderTime;
extern double ROP_FBXdb_convertTime;
extern double ROP_FBXdb_duplicateTime;
#endif
/********************************************************************************************************/
class ropFBX_AutoCookRender {
public:
    ropFBX_AutoCookRender(OP_Node *sop)
    {
	if (myObj = sop->getParent())
	{
	    myPrev = myObj->isCookingRender();
	    myObj->setCookingRender(1);
	}
    }
    ~ropFBX_AutoCookRender()
    {
	if (myObj)
	    myObj->setCookingRender(myPrev);
    }
private:
    OP_Node	*myObj;
    int		 myPrev;
};


bool 
ROP_FBXUtil::getGeometryHandle(SOP_Node* sop_node, float time, GU_DetailHandle &gdh)
{
    if( sop_node )
    {
#ifdef UT_DEBUG
	double cook_start, cook_end;
	cook_start = clock();
#endif
	ropFBX_AutoCookRender	autopop(sop_node);
	OP_Context	 context(time);
	gdh = sop_node->getCookedGeoHandle(context);
#ifdef UT_DEBUG
	cook_end = clock();
	ROP_FBXdb_cookingTime += (cook_end - cook_start);
#endif
	return(true);
    }
    return false;

}
/********************************************************************************************************/
void				
ROP_FBXUtil::getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, bool do_expand)
{
    PRM_Parm	 *parm;
    strref = "";

    if (node->getParameterOrProperty(parmName, 0, node, parm))
	parm->getValue(0, strref, 0, do_expand);
}
/********************************************************************************************************/
int 
ROP_FBXUtil::getIntOPParm(OP_Node *node, const char* parmName, int index)
{
    PRM_Parm	 *parm;
    int res = 0;

    if (node->getParameterOrProperty(parmName, 0, node, parm))
	parm->getValue(0, res, index);

    return res;
}
/********************************************************************************************************/
float 
ROP_FBXUtil::getFloatOPParm(OP_Node *node, const char* parmName, int index, float ftime)
{
    PRM_Parm	 *parm;
    float res = 0.0;

    if (node->getParameterOrProperty(parmName, 0, node, parm))
	parm->getValue(ftime, res, index);

    return res;
}
/********************************************************************************************************/
int 
ROP_FBXUtil::getMaxPointsOverAnimation(SOP_Node* sop_node, float start_time, float end_time, ROP_FBXGDPCache* v_cache_out)
{
    CH_Manager *ch_manager = CHgetManager();
//    float start_frame = ch_manager->getGlobalStartFrame();
//    float end_frame = ch_manager->getGlobalEndFrame();
    float start_frame, end_frame;
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);

    float hd_time;
    int curr_frame;

    UT_ASSERT(v_cache_out);

    // Here we unfortunately have to go and find the maximum number of points over all frames.
    GU_DetailHandle gdh;
    const GU_Detail *gdp;
    int max_points = 0;    
    const GU_Detail *final_gdp;
    const GEO_Primitive *prim;

    for(curr_frame = start_frame; curr_frame <= end_frame; curr_frame++)
    {
	hd_time = ch_manager->getTime(curr_frame);


	if(ROP_FBXUtil::getGeometryHandle(sop_node, hd_time, gdh))
	{
	    GU_DetailHandleAutoReadLock	 gdl(gdh);
	    gdp = gdl.getGdp();
	    if(!gdp)
		continue;

	    //GU_Detail conv_gdp;
	    GU_Detail *conv_gdp;
	    prim = gdp->primitives()(0);
	    unsigned prim_type = prim->getPrimitiveId();

	    conv_gdp = v_cache_out->addFrame(curr_frame);

	    if(prim_type == GEOPRIMPART)
		convertParticleGDPtoPolyGDP(gdp, *conv_gdp);
	    else
	    	convertGeoGDPtoVertexCacheableGDP(gdp, *conv_gdp);

	    if(conv_gdp->points().entries() > max_points)	    
		max_points = conv_gdp->points().entries();
/*
	    // Since this function assumes that the object is to be a part of the vertex cache,
	    // and (in Houdini) the number of vertices can change, and we need to pre-create all of them
	    // and assign them to valid polygons, we always need to triangulate these.

	    // It's ok not to have them in proper order (see convertParticleGDPtoPolyGDP()) since we're just
	    // counting them.
	    conv_gdp.duplicate(*gdp);
	    GU_ConvertParms conv_parms;
	    conv_parms.fromType = GEOPRIMALL;
	    conv_parms.toType = GEOPRIMPOLY;
	    conv_parms.method.setULOD(1.0);
	    conv_parms.method.setVLOD(1.0);
	    conv_gdp.convert(conv_parms);
	    conv_gdp.convex(3);
	    conv_gdp.uniquePoints();

	    final_gdp = &conv_gdp;

	    if(final_gdp->points().entries() > max_points)	    
		max_points = final_gdp->points().entries();
*/
	}
    }

    return max_points;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::isVertexCacheable(OP_Network *op_net, bool& found_particles)
{
    found_particles = false;

    if(!op_net)
	return false;
    int curr_node_idx, num_nodes = op_net->getNchildren();
    OP_Node* curr_node;
    UT_String node_type;
    OP_Network *curr_net;
    for(curr_node_idx = 0; curr_node_idx < num_nodes; curr_node_idx++)
    {
	curr_node = op_net->getChild(curr_node_idx);
	if(curr_node)
	{
	    node_type = curr_node->getOperator()->getName();
	    if(node_type == "dopimport")
		return true;
	    else if(node_type == "popnet")
	    {
		found_particles = true;
		return true;
	    }

	    if(curr_node->isNetwork())
	    {
		curr_net = dynamic_cast<OP_Network*>(curr_node);
		if(isVertexCacheable(curr_net, found_particles))
		    return true;
	    }
	}

    }

    return false;
}
/********************************************************************************************************/
void 
ROP_FBXUtil::convertParticleGDPtoPolyGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp)
{
    GEO_PrimPoly* prim_poly_ptr;

    // TODO: We'll need to export attributes, too.
    int curr_particle, num_parts = src_gdp->points().entries();
    double sq_size = 0.25;
    UT_Vector4 ut_vec;
    UT_Vector4 ut_curr_dir;
    UT_Vector4 tri_points[ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT];
    GEO_Point* geo_points[ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT];
    
    for(curr_particle = 0; curr_particle < num_parts; curr_particle++)
    {
	if(curr_particle < num_parts)
	    ut_vec = src_gdp->points()(curr_particle)->getPos();

	// Generate a triangle
	ut_curr_dir.assign(0.5,0,0.5);
	tri_points[0] = ut_vec + ut_curr_dir*sq_size;
	ut_curr_dir.assign(0.5,0,-0.5);
	tri_points[1] = ut_vec + ut_curr_dir*sq_size;
	ut_curr_dir.assign(-0.5,0,-0.5);
	tri_points[2] = ut_vec + ut_curr_dir*sq_size;
	ut_curr_dir.assign(-0.5,0,0.5);
	tri_points[3] = ut_vec + ut_curr_dir*sq_size;

	// Append points
	geo_points[0] = out_gdp.appendPoint();
	geo_points[0]->setPos(tri_points[0]);
	geo_points[1] = out_gdp.appendPoint();
	geo_points[1]->setPos(tri_points[1]);
	geo_points[2] = out_gdp.appendPoint();
	geo_points[2]->setPos(tri_points[2]);
	geo_points[3] = out_gdp.appendPoint();
	geo_points[3]->setPos(tri_points[3]);

	// Create a primitive
	prim_poly_ptr = (GEO_PrimPoly *)out_gdp.appendPrimitive(GEOPRIMPOLY);
	prim_poly_ptr->setSize(0);
	prim_poly_ptr->appendVertex(geo_points[0]);
	prim_poly_ptr->appendVertex(geo_points[1]);
	prim_poly_ptr->appendVertex(geo_points[2]);
	prim_poly_ptr->appendVertex(geo_points[3]);
    }
    
}
/********************************************************************************************************/
void 
ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp)
{
#ifdef UT_DEBUG
    double cook_start, cook_end;
#endif
    GU_Detail conv_gdp;
    GU_ConvertParms conv_parms;
    conv_parms.fromType = GEOPRIMALL;
    conv_parms.toType = GEOPRIMPOLY;
    conv_parms.method.setULOD(1.0);
    conv_parms.method.setVLOD(1.0);
#ifdef UT_DEBUG
    cook_start = clock();
#endif
    conv_gdp.duplicate(*src_gdp);
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_duplicateTime += (cook_end - cook_start);

    cook_start = clock();
#endif
    conv_gdp.convert(conv_parms);
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_convertTime += (cook_end - cook_start);

    cook_start = clock();
#endif
    conv_gdp.convex(3);
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_convexTime += (cook_end - cook_start);

    cook_start = clock();
#endif
    // We need to have all triangles not only have unique vertices, but also have 
    // each triangle consist of three consequent vertices.
    GU_Detail conv_ordered_gdp;

    int curr_prim_idx, num_prims = conv_gdp.primitives().entries();
    GEO_Point *temp_pts[3];
    const GEO_Primitive* prim;
    GEO_PrimPoly *prim_poly_ptr;
    for(curr_prim_idx = 0; curr_prim_idx < num_prims; curr_prim_idx++)
    {
	prim = conv_gdp.primitives()(curr_prim_idx);
	UT_ASSERT(prim->getVertexCount() == 3);

	temp_pts[0] = out_gdp.appendPoint();
	temp_pts[0]->getPos() = prim->getVertex(0).getPos();
	temp_pts[1] = out_gdp.appendPoint();
	temp_pts[1]->getPos() = prim->getVertex(1).getPos();
	temp_pts[2] = out_gdp.appendPoint();
	temp_pts[2]->getPos() = prim->getVertex(2).getPos();
	prim_poly_ptr = (GEO_PrimPoly *)out_gdp.appendPrimitive(GEOPRIMPOLY);
	prim_poly_ptr->setSize(0);
	prim_poly_ptr->appendVertex(temp_pts[0]);
	prim_poly_ptr->appendVertex(temp_pts[1]);
	prim_poly_ptr->appendVertex(temp_pts[2]);
    }
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_reorderTime += (cook_end - cook_start);
#endif
}
/********************************************************************************************************/
bool 
ROP_FBXUtil::getFinalTransforms(OP_Node* hd_node, bool has_lookat_node, float bone_length, float time_in,
			UT_Vector3& t_out, UT_Vector3& r_out, UT_Vector3& s_out, KFbxVector4* post_rotation)
{
    bool set_post_rotation = false;
    OBJ_Node* obj_node = dynamic_cast<OBJ_Node*>(hd_node);
    if(!obj_node)
	return set_post_rotation;

    bool do_special_rotate = false;
    UT_String node_type = hd_node->getOperator()->getName();

    if(node_type == "hlight" || node_type == "cam")
	do_special_rotate = true;

    // TODO: optionally export pivot.
    // Get and set transforms
    UT_Matrix4 full_xform, pre_xform, xform;
    OP_Context op_context(time_in);
    obj_node->getPreLocalTransform(op_context, pre_xform);
    obj_node->getParmTransform(op_context, xform);

    full_xform = xform * pre_xform;

    if(SYSequalZero(bone_length) == false)
    {
	// Add a bone length transform
	UT_Matrix4 bone_trans;
	UT_XformOrder xform_default;
	bone_trans.identity();
	bone_trans.xform(xform_default, 0.0,0.0,-bone_length, 
	    0.0,0.0,0.0,
	    1.0,1.0,1.0,
	    0.0,0.0,0.0,
	    0);
	full_xform = bone_trans * full_xform;
    }

    if(do_special_rotate && !has_lookat_node && post_rotation)
    {
	// Set post-transform instead
	//KFbxVector4 fbx_rot_vec;
	if(node_type == "hlight")
	    post_rotation->Set(-90,0,0);
	else // if(node_type == "cam")
	    post_rotation->Set(0,-90,0);

	set_post_rotation = true;
    }

    UT_XformOrder xform_order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
    full_xform.explode(xform_order, r_out,s_out,t_out);
    r_out.radToDeg();

    return set_post_rotation;
}
/********************************************************************************************************/
// ROP_FBXNodeManager
/********************************************************************************************************/
ROP_FBXNodeManager::ROP_FBXNodeManager()
{

}
/********************************************************************************************************/
ROP_FBXNodeManager::~ROP_FBXNodeManager()
{

}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXNodeManager::findFbxNode(OP_Node* hd_node)
{
    if(!hd_node)
	return NULL;

    THDToFbxNodeMap::iterator mi = myHdToFbxNodeMap.find(hd_node);
    if(mi == myHdToFbxNodeMap.end())
	return NULL;
    else
	return mi->second.getFbxNode();
}
/********************************************************************************************************/
ROP_FBXNodeInfo*
ROP_FBXNodeManager::findNodeInfo(OP_Node* hd_node)
{
    if(!hd_node)
	return NULL;

    THDToFbxNodeMap::iterator mi = myHdToFbxNodeMap.find(hd_node);
    if(mi == myHdToFbxNodeMap.end())
	return NULL;
    else
	return &(mi->second);
}
/********************************************************************************************************/
ROP_FBXNodeInfo& 
ROP_FBXNodeManager::addNodePair(OP_Node* hd_node, KFbxNode* fbx_node)
{
    myHdToFbxNodeMap[hd_node] = fbx_node;
    return myHdToFbxNodeMap[hd_node];
}
/********************************************************************************************************/
// ROP_FBXNodeInfo
/********************************************************************************************************/
ROP_FBXNodeInfo::ROP_FBXNodeInfo()
{
    myFbxNode = NULL;
    myMaxObjectPoints = 0;
    myVertexCacheMethod = ROP_FBXVertexCacheMethodNone;
    myVertexCache = NULL;
}
/********************************************************************************************************/
ROP_FBXNodeInfo::ROP_FBXNodeInfo(KFbxNode* main_node)
{
    myFbxNode = main_node;
    myMaxObjectPoints = 0;
    myVertexCacheMethod = ROP_FBXVertexCacheMethodNone;
    myVertexCache = NULL;
}
/********************************************************************************************************/
ROP_FBXNodeInfo::~ROP_FBXNodeInfo()
{
    myFbxNode = NULL;

    if(myVertexCache)
	delete myVertexCache;
    myVertexCache = NULL;
}
/********************************************************************************************************/
KFbxNode* ROP_FBXNodeInfo::getFbxNode(void) const
{
    return myFbxNode;
}
/********************************************************************************************************/
void ROP_FBXNodeInfo::setFbxNode(KFbxNode* node)
{
    myFbxNode = node;
}
/********************************************************************************************************/
int 
ROP_FBXNodeInfo::getMaxObjectPoints(void)
{
    return myMaxObjectPoints;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setMaxObjectPoints(int num_points)
{
    myMaxObjectPoints = num_points;
}
/********************************************************************************************************/
ROP_FBXVertexCacheMethodType 
ROP_FBXNodeInfo::getVertexCacheMethod(void)
{
    return myVertexCacheMethod;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setVertexCacheMethod(ROP_FBXVertexCacheMethodType vc_method)
{
    myVertexCacheMethod = vc_method;
}
/********************************************************************************************************/
ROP_FBXGDPCache* 
ROP_FBXNodeInfo::getVertexCache(void)
{
    return myVertexCache;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setVertexCache(ROP_FBXGDPCache* v_cache)
{
    myVertexCache = v_cache;
}
/********************************************************************************************************/
// ROP_FBXGDPCached
/********************************************************************************************************/
ROP_FBXGDPCache::ROP_FBXGDPCache()
{
    myMinFrame = FLT_MAX;
}
/********************************************************************************************************/
ROP_FBXGDPCache::~ROP_FBXGDPCache()
{
    int curr_item, num_items = myFrameItems.size();
    for(curr_item = 0; curr_item < num_items; curr_item++)
	delete myFrameItems[curr_item];
    myFrameItems.clear();
}
/********************************************************************************************************/
GU_Detail* ROP_FBXGDPCache::addFrame(float frame_num)
{
    ROP_FBXGDPCacheItem* new_item = new ROP_FBXGDPCacheItem(frame_num);

    if(myFrameItems.size() > 0)
    {
	UT_ASSERT(myFrameItems[myFrameItems.size()-1]->getFrame() < frame_num);
    }
    myFrameItems.push_back(new_item);

    if(frame_num < myMinFrame)
	myMinFrame = frame_num;

    return new_item->getDetail();
}
/********************************************************************************************************/
GU_Detail* ROP_FBXGDPCache::getFrameGeometry(float frame_num)
{
    int vec_pos = (int)(frame_num - myMinFrame);
    if(vec_pos < 0 || vec_pos >= myFrameItems.size())
    {
	UT_ASSERT(0);
	return NULL;
    }
    return myFrameItems[vec_pos]->getDetail();
}
/********************************************************************************************************/
float 
ROP_FBXGDPCache::getFirstFrame(void)
{
    return myMinFrame;
}
/********************************************************************************************************/