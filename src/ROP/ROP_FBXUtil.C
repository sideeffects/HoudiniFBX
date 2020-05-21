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

#include "ROP_FBXUtil.h"
#include "ROP_FBXCommon.h"

#include <GU/GU_DetailHandle.h>

#include <GEO/GEO_ConvertParms.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <OP/OP_Director.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <PRM/PRM_Parm.h>
#include <CH/CH_Manager.h>

#include <UT/UT_Assert.h>
#include <UT/UT_CrackMatrix.h>
#include <UT/UT_FSATable.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_Thread.h>
#include <UT/UT_XformOrder.h>

#ifdef UT_DEBUG
#include <UT/UT_Debug.h>
#include <time.h>
extern double ROP_FBXdb_maxVertsCountingTime;
extern double ROP_FBXdb_cookingTime;
extern double ROP_FBXdb_convexTime;
extern double ROP_FBXdb_reorderTime;
extern double ROP_FBXdb_convertTime;
extern double ROP_FBXdb_duplicateTime;
#endif

using namespace std;


/********************************************************************************************************/
class ropFBX_AutoCookRender {
public:
    ropFBX_AutoCookRender(OP_Node *sop)
    {
	if ((myObj = sop->getCreator()))
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
ROP_FBXUtil::getGeometryHandle(SOP_Node* sop_node, OP_Context &context, GU_DetailHandle &gdh)
{
    if( sop_node )
    {
#ifdef UT_DEBUG
	double cook_start, cook_end;
	cook_start = clock();
#endif
	ropFBX_AutoCookRender	autopop(sop_node);
	gdh = sop_node->getCookedGeoHandle(context);
#ifdef UT_DEBUG
	cook_end = clock();
	ROP_FBXdb_cookingTime += (cook_end - cook_start);
#endif
	if(gdh.isNull())
	    return false;
	else
	    return true;
    }
    return false;

}
/********************************************************************************************************/
void				
ROP_FBXUtil::getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, fpreal ftime)
{
    PRM_Parm	 *parm;
    strref = "";

    if(!node)
	return;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
	parm->getValue(ftime, strref, 0, /*expand=*/true, SYSgetSTID());
}
/********************************************************************************************************/
int 
ROP_FBXUtil::getIntOPParm(OP_Node *node, const char* parmName, fpreal ftime, int index)
{
    if(!node)
	return 0;

    PRM_Parm	 *parm;
    int res = 0;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
	parm->getValue(ftime, res, index, SYSgetSTID());

    return res;
}
/********************************************************************************************************/
fpreal 
ROP_FBXUtil::getFloatOPParm(OP_Node *node, const char* parmName, fpreal ftime, int index, bool *did_find)
{
    if(did_find)
	*did_find = false;
    PRM_Parm	 *parm;
    fpreal res = 0.0;

    if(!node)
	return 0.0;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
    {
	parm->getValue(ftime, res, index, SYSgetSTID());
	if(did_find)
	    *did_find = true;
    }

    return res;
}
/********************************************************************************************************/
int 
ROP_FBXUtil::getMaxPointsOverAnimation(OP_Node* op_node, fpreal start_time, fpreal end_time, float lod, bool allow_constant_point_detection, 
				       bool convert_surfaces, UT_Interrupt* boss_op, ROP_FBXGDPCache* v_cache_out, bool &is_pure_surfaces)
{
#ifdef UT_DEBUG
    double timer_start = clock();
#endif
    CH_Manager *ch_manager = CHgetManager();
    fpreal start_frame, end_frame;
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);

    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(op_node);
    OBJ_Node* obj_node = dynamic_cast<OBJ_Node*>(op_node);

    is_pure_surfaces = false;

    fpreal hd_time;
    int curr_frame;
    int curr_num_points;

    UT_ASSERT(v_cache_out);

    // Here we unfortunately have to go and find the maximum number of points over all frames.
    GU_DetailHandle gdh;
    const GU_Detail *gdp;
    int max_points = 0;    
    int first_frame_num_points = -1;
    
    int curr_num_unconverted_points;
    bool is_num_verts_constant = true;
    bool is_surfs_only = true;
    bool looked_at_prims = false;

    for(curr_frame = start_frame; curr_frame <= end_frame; curr_frame++)
    {
	hd_time = ch_manager->getTime(curr_frame);
	if(boss_op && curr_frame % 5)
	{
	    if(boss_op->opInterrupt())
	    {
#ifdef UT_DEBUG
		ROP_FBXdb_maxVertsCountingTime += clock() - timer_start;
#endif
		return -1;
	    }
	}

	OP_Context  context(hd_time);

	if(sop_node)
	    ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh);
	else
	    gdh = obj_node->getDisplayGeometryHandle(context);

	if(gdh.isNull() == false)
	{
	    GU_DetailHandleAutoReadLock	 gdl(gdh);
	    gdp = gdl.getGdp();
	    if(!gdp || gdp->getNumPrimitives() <= 0)
		continue;

	    looked_at_prims = true;

	    GU_Detail *conv_gdp;
	    GA_PrimCompat::TypeMask prim_type = ROP_FBXUtil::getGdpPrimId(gdp);

	    GU_Detail temp_detail;
	    // We must save the start frame in any case since it is needed
	    // elsewhere
	    if(v_cache_out->getSaveMemory() && curr_frame != start_frame)
		conv_gdp = &temp_detail;
	    else
		conv_gdp = v_cache_out->addFrame(curr_frame);

	    if(prim_type == GEO_PrimTypeCompat::GEOPRIMPART)
	    {
		convertParticleGDPtoPolyGDP(gdp, *conv_gdp);
		is_num_verts_constant = false;
	    }
	    else
	    {
		GA_PrimCompat::TypeMask prim_type_res;
		prim_type_res = prim_type & (~(GEO_PrimTypeCompat::GEOPRIMNURBSURF | GEO_PrimTypeCompat::GEOPRIMBEZSURF | GEO_PrimTypeCompat::GEOPRIMNURBCURVE | GEO_PrimTypeCompat::GEOPRIMBEZCURVE));
                if (prim_type_res)
                    is_surfs_only = false;

	    	convertGeoGDPtoVertexCacheableGDP(gdp, lod, true, *conv_gdp, curr_num_unconverted_points);
		if(first_frame_num_points < 0)
		    first_frame_num_points = curr_num_unconverted_points;
		else
		{
		    if(first_frame_num_points != curr_num_unconverted_points)
			is_num_verts_constant = false;
		}
	    }

	    curr_num_points = conv_gdp->getNumPoints();
	    if(curr_num_points > max_points)
		max_points = curr_num_points;

	}
    }

    // If we return a value of <0, the code will think we cancelled.
    // However, this may also happen if our target node contains 
    // no primitives over any frames, so we prevent it.
    if(!looked_at_prims && first_frame_num_points < 0)
	first_frame_num_points = 0;

    if(is_num_verts_constant && allow_constant_point_detection)
	v_cache_out->setNumConstantPoints(first_frame_num_points);

    if(is_num_verts_constant && is_surfs_only && !convert_surfaces)
    {
	max_points = first_frame_num_points;
	is_pure_surfaces = true;
    }

#ifdef UT_DEBUG
    ROP_FBXdb_maxVertsCountingTime += clock() - timer_start;
#endif
    return max_points;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::isVertexCacheable(OP_Network *op_net, bool include_deform_nodes, fpreal ftime, bool& found_particles, bool is_sop_export)
{
    OP_Node* dyn_node, *part_node;
    OP_Node* render_node = is_sop_export ? op_net : op_net->getRenderNodePtr();

    const char *const dynamics_node_types[] = { "dopimport", "channel", 0};
    const char *const dynamics_node_types_with_deforms[] = { "dopimport", "channel", "file", "bonedeform", "deform", 0};
    const char *const particle_node_types[] = { "popnet", 0};

    found_particles = false;

    // First, look for particles
    part_node = ROP_FBXUtil::findOpInput(render_node, particle_node_types, true, NULL, NULL);
    if(part_node)
    {
	found_particles = true;
	return true;
    }

    // If include_deform_nodes is true, we want to flag deforms as vc objects. This means that if it is false,
    // we can ignore them.

    // Look for any time-dependent nodes in general.
    const char *const deform_node[] = { "bonedeform", "deform", 0 };
    if(ROP_FBXUtil::findTimeDependentNode(render_node, ROP_FBXallowed_inbetween_node_types, ( include_deform_nodes ? NULL : deform_node ), ftime, true))
	return true;
    
    // Then, if not found, look for other dynamic nodes
    if(include_deform_nodes)
	dyn_node = ROP_FBXUtil::findOpInput(render_node, dynamics_node_types_with_deforms, true, NULL, NULL);
    else
	dyn_node = ROP_FBXUtil::findOpInput(render_node, dynamics_node_types, true, NULL, NULL);

    if(dyn_node)
	return true;
    else
	return false;
}
/********************************************************************************************************/
void 
ROP_FBXUtil::convertParticleGDPtoPolyGDP(const GU_Detail* src_gdp, GU_Detail& out_gdp)
{
    // TODO: We'll need to export attributes, too.
    double sq_size = 0.25;
    UT_Vector4 ut_curr_dir;
    UT_Vector4 tri_points[ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT];
    GA_Offset geo_points[ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT];
    
    GA_Offset srcptoff;
    GA_FOR_ALL_PTOFF(src_gdp, srcptoff)
    {
	UT_Vector4 ut_vec = src_gdp->getPos4(srcptoff);

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
	geo_points[0] = out_gdp.appendPointOffset();
	out_gdp.setPos4(geo_points[0], tri_points[0]);
	geo_points[1] = out_gdp.appendPointOffset();
	out_gdp.setPos4(geo_points[1], tri_points[1]);
	geo_points[2] = out_gdp.appendPointOffset();
	out_gdp.setPos4(geo_points[2], tri_points[2]);
	geo_points[3] = out_gdp.appendPointOffset();
	out_gdp.setPos4(geo_points[3], tri_points[3]);

	// Create a primitive
	GEO_PrimPoly *prim_poly_ptr = (GEO_PrimPoly *)out_gdp.appendPrimitive(GEO_PRIMPOLY);
	prim_poly_ptr->setSize(0);
	prim_poly_ptr->appendVertex(geo_points[0]);
	prim_poly_ptr->appendVertex(geo_points[1]);
	prim_poly_ptr->appendVertex(geo_points[2]);
	prim_poly_ptr->appendVertex(geo_points[3]);
	prim_poly_ptr->close(1,1);
    }
    
}
/********************************************************************************************************/
void 
ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(const GU_Detail* src_gdp, float lod, bool do_triangulate_and_rearrange, GU_Detail& out_gdp, int& num_pre_proc_points)
{
#ifdef UT_DEBUG
    double cook_start, cook_end;
#endif
    GU_Detail conv_gdp;
    GEO_ConvertParms conv_parms;
    conv_parms.setFromType(GEO_PrimTypeCompat::GEOPRIMALL);
    conv_parms.setToType(GEO_PrimTypeCompat::GEOPRIMPOLY);
    conv_parms.method.setULOD(lod);
    conv_parms.method.setVLOD(lod);
    conv_parms.myDestDetail = &conv_gdp;
    conv_parms.mySourceDetail = &conv_gdp;
#ifdef UT_DEBUG
    cook_start = clock();
#endif
    conv_gdp.duplicate(*src_gdp);
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_duplicateTime += (cook_end - cook_start);

    cook_start = clock();
#endif
    num_pre_proc_points = conv_gdp.getNumPoints();
    conv_gdp.convert(conv_parms);
//    num_pre_proc_points = conv_gdp.getNumPoints();

    if(!do_triangulate_and_rearrange)
    {
	out_gdp.duplicate(conv_gdp);
	return;
    }

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
    for (GA_Iterator it(conv_gdp.getPrimitiveRange()); !it.atEnd(); ++it)
    {
        const GA_Primitive *prim = conv_gdp.getPrimitive(*it);
        if (prim->getTypeId() != GA_PRIMPOLY)
            continue;

        const GEO_PrimPoly *poly = UTverify_cast<const GEO_PrimPoly *>(prim);

        UT_ASSERT(poly->getFastVertexCount() == 3);

        GA_Offset startpt = out_gdp.appendPointBlock(3);
        out_gdp.setPos3(startpt+0, poly->getPos3(0));
        out_gdp.setPos3(startpt+1, poly->getPos3(1));
        out_gdp.setPos3(startpt+2, poly->getPos3(2));

        GEO_PrimPoly *prim_poly_ptr = (GEO_PrimPoly *)out_gdp.appendPrimitive(GA_PRIMPOLY);
        prim_poly_ptr->setSize(3);
        prim_poly_ptr->setPointOffset(0, startpt+0);
        prim_poly_ptr->setPointOffset(1, startpt+1);
        prim_poly_ptr->setPointOffset(2, startpt+2);
        prim_poly_ptr->setClosed(true);
    }
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_reorderTime += (cook_end - cook_start);
#endif
}
/********************************************************************************************************/
void 
ROP_FBXUtil::getFinalTransforms(
	OP_Node* hd_node, ROP_FBXBaseNodeVisitInfo *node_info, fpreal bone_length, fpreal time_in,
	const UT_XformOrder& xform_order, UT_Vector3D& t_out, UT_Vector3D& r_out, UT_Vector3D& s_out,
	UT_Vector3D* prev_frame_rotations)
{
    // Get and set transforms
    OP_Context op_context(time_in);
    UT_Matrix4D full_xform(1.0); // identity
    OBJ_Node* obj_node = CAST_OBJNODE(hd_node);
    if(obj_node)
    {
	UT_Matrix4D world_xform;
	obj_node->getWorldTransform(world_xform, op_context);

	OBJ_Node* parent_obj_node = NULL;
	if(node_info && node_info->getParentInfo())
	    parent_obj_node = dynamic_cast<OBJ_Node*>(node_info->getParentInfo()->getHdNode());

	if(parent_obj_node)
	{
	    UT_Matrix4D inverse_parent_world;
	    parent_obj_node->getIWorldTransform(inverse_parent_world, op_context);
	    full_xform = world_xform * inverse_parent_world;
	}
	else
	{
	    full_xform = world_xform;
	}
    }

    // Add a bone length transform if requested
    if(SYSequalZero(bone_length) == false)
	full_xform.translate(0.0, 0.0, -bone_length);

    full_xform.explode(xform_order, r_out,s_out,t_out);
    if(prev_frame_rotations)
    {
	UT_Vector3D prev_rot(*prev_frame_rotations);
	prev_rot.degToRad();
	UTcrackMatrixSmooth(xform_order, r_out.x(), r_out.y(), r_out.z(),
			    prev_rot.x(), prev_rot.y(), prev_rot.z());
    }
    r_out.radToDeg();
}
/********************************************************************************************************/
bool
ROP_FBXUtil::getPostRotateAdjust(const UT_StringRef &node_type, FbxVector4 &post_rotate)
{
    // For lights/cameras, they have a look at axis that is different from Houdini/Maya that use
    // the -Z axis. To compensate, we multiply in a post rotation so that we go from Houdini's -Z
    // to either the +X (for cameras) or -Y (for lights).
    if(ROPfbxIsLightNodeType(node_type))
	post_rotate.Set(-90, 0, 0);
    else if(node_type == "cam")
	post_rotate.Set(0, -90, 0);
    else
	return false;
    return true;
}

void
ROP_FBXUtil::doPostRotateAdjust(FbxVector4 &post_rotate, const FbxVector4 &adjustment)
{
    FbxAMatrix rot_xform;
    rot_xform.SetR(post_rotate);

    FbxAMatrix adjust_xform;
    adjust_xform.SetR(adjustment);

    rot_xform *= adjust_xform;
    post_rotate = rot_xform.GetR();
}
/********************************************************************************************************/
bool
ROP_FBXUtil::findTimeDependentNode(OP_Node *op, const char* const ignored_node_types[], const char * const opt_more_types[], fpreal ftime, bool include_me, UT_Set<OP_Node*> *already_visited)
{
    OP_Context op_context(ftime);

    // NOTE: Traversing a node network (directed acyclic graph) like a tree
    //       can and has produced cases (e.g. Bug 70324) where some nodes
    //       will be visited an exponential number of times, due to multiple
    //       nodes using the same node as input.
    //       DO NOT allow a node to be visited multiple times, even from
    //       different downstream nodes!
    if (!already_visited)
        already_visited = new UT_Set<OP_Node*>();
    else if (already_visited->count(op))
        return false;

    if (op == NULL)
        return false;

    already_visited->insert(op);

    // Check if the node we are checking is already on the stack. If so,
    // we're at risk of recursion, so bail out.
    // Don't care about locks: if (!op->getHardLock())

    if (include_me)
    {
	// See if this op is one of the types we can ignore.
        bool found = false;
	for (int i = 0; !found && ignored_node_types[i]; i++)
	{
	    if (op->getOperator()->getName() == ignored_node_types[i])
		found = true;
	}

	if(opt_more_types)
	{
	    for (int i = 0; !found && opt_more_types[i]; i++)
	    {
		if (op->getOperator()->getName() == opt_more_types[i])
		    found = true;
	    }
	}

	if(!found)
        {
	    // Ensure op is cooked for accurate timedep state
	    op->cook(op_context);
	    return op->isTimeDependent(op_context);
        }
    }

    // If we are a subnet, try to find an appropriate node within the
    // subnet, starting with the subnet's display node.
    if(op->isSubNetwork(false))
    {
        bool is_time_dependent = ROP_FBXUtil::findTimeDependentNode(((OP_Network *)op)->getDisplayNodePtr(), ignored_node_types, opt_more_types, ftime, true, already_visited);
        // Found a time-dependent node. That's all we need.
        if (is_time_dependent)
            return true;
    }

    // if we're a switch SOP, then look up the appropriate input chain
    bool is_aswitch = false;
    if (op->getOperator()->getName() == "switch")
    {
	PRM_Parm *parm = op->getParmList()->getParmPtr( "input" );
	if (parm != NULL)
	{
            int i;
	    parm->getValue(
		ftime,
		i, 0, op_context.getThread());
	    if( op->getInput(i) != NULL )
	    {
		is_aswitch = true;
		bool is_time_dependent = ROP_FBXUtil::findTimeDependentNode(op->getInput(i), ignored_node_types, opt_more_types, ftime, true, already_visited);

		// Found a time-dependent node. That's all we need.
		if (is_time_dependent)
		    return true;
	    }
	}
    }

    for (int i = op->getConnectedInputIndex(-1); !is_aswitch && i >= 0;
	i = op->getConnectedInputIndex(i) )
    {
	// We need to traverse reference inputs as well, in cases,
	// for example, where particles are present.
	if( op->getInput(i) ) //  && !op->isRefInput(i) )
	{
	    bool is_time_dependent = ROP_FBXUtil::findTimeDependentNode(op->getInput(i), ignored_node_types, opt_more_types, ftime, true, already_visited);
	    // Found a time-dependent node. That's all we need.
	    if (is_time_dependent)
		return true;
	}
    }

    return false;
}
/********************************************************************************************************/
OP_Node*
ROP_FBXUtil::findOpInput(OP_Node *op, const char * const find_op_types[], bool include_me, const char* const  allowed_node_types[], bool *did_find_allowed_only, int rec_level, UT_Set<OP_Node*> *already_visited)
{
    bool child_did_find_allowed_types_only;

    if (rec_level == 0 && did_find_allowed_only)
	*did_find_allowed_only = true;

    // NOTE: Traversing a node network (directed acyclic graph) like a tree
    //       can and has produced cases (e.g. Bug 70324) where some nodes
    //       will be visited an exponential number of times, due to multiple
    //       nodes using the same node as input.
    //       DO NOT allow a node to be visited multiple times, even from
    //       different downstream nodes!
    if (!already_visited)
        already_visited = new UT_Set<OP_Node*>();
    else if (already_visited->count(op))
        return NULL;

    if (op == NULL)
        return NULL;

    // Skip pass through nodes, marking as visited along the way
    const fpreal now = CHgetEvalTime();
    while (true)
    {
	already_visited->insert(op);
	OP_Node* pass_through = op->getPassThroughNode(now);
	if (!pass_through)
	{   
	    if (op->getOperator()->getName() != "cache")
		break;
	    pass_through = op->getInput(0);
	}
	op = pass_through;
    }

    // Check if the node we are checking is already on the stack. If so,
    // we're at risk of recursion, so bail out.
    // Don't care about locks: if (!op->getHardLock())

    OP_Node *found = NULL;

    if (include_me)
    {
	// See if this op is one of the types we are looking for.
	for (int i = 0; !found && find_op_types[i]; i++)
	    if (op->getOperator()->getName() == find_op_types[i])
		found = op;

	// See if this op, if not found, is one of the allowed types
	if(allowed_node_types && !found)
	{
	    bool did_is_allowed_only_local = false;

	    for (int i = 0; !did_is_allowed_only_local && allowed_node_types[i]; i++)
	    {
		if( op->getOperator()->getName() == allowed_node_types[i] )
		{
		    did_is_allowed_only_local = true;
		    break;
		}
	    }

	    if(!did_is_allowed_only_local && did_find_allowed_only)
		*did_find_allowed_only = false;

	}
	else if(found && did_find_allowed_only)
	    *did_find_allowed_only = true;

    }

    for (int i = op->getConnectedInputIndex(-1); !found && i >= 0;
	    i = op->getConnectedInputIndex(i))
    {
	// We need to traverse reference inputs as well, in cases,
	// for example, where particles are present.
	if( op->getInput(i)) // && !op->isRefInput(i) )
	{
            child_did_find_allowed_types_only = true;
	    found = ROP_FBXUtil::findOpInput(op->getInput(i), find_op_types, true, allowed_node_types, &child_did_find_allowed_types_only, rec_level+1, already_visited);
	    if(found && !child_did_find_allowed_types_only && did_find_allowed_only)
		*did_find_allowed_only = false;
	}
    }

    return found;
}
/********************************************************************************************************/
EFbxRotationOrder 
ROP_FBXUtil::fbxRotationOrder(UT_XformOrder::xyzOrder rot_order)
{
    switch (rot_order)
    {
	case UT_XformOrder::XYZ:
	    return eEulerXYZ;
	case UT_XformOrder::XZY:
	    return eEulerXZY;
	case UT_XformOrder::YXZ:
	    return eEulerYXZ;
	case UT_XformOrder::YZX:
	    return eEulerYZX;
	case UT_XformOrder::ZXY:
	    return eEulerZXY;
	case UT_XformOrder::ZYX:
	    return eEulerZYX;
    }
    UT_ASSERT(!"Unhandled rotation order");
    return eEulerXYZ;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::mapsToFBXTransform(fpreal t, OBJ_Node* node)
{
    if (!node)
	return false;

    // FBX only supports SRT transform orders, so if it's different, we need to resample
    if (OP_Node::getMainOrder(node->TRS(t)) != UT_XformOrder::SRT)
	return false;

    // Non-zero pivots currently need to resample
    const PRM_Parm &parm(node->getParm("p"));
    if (parm.isTimeDependent())
	return false;
    UT_Vector3R p;
    node->P(p.data(), t);
    if (!SYSequalZero(p(0)) || !SYSequalZero(p(1)) || !SYSequalZero(p(2)))
	return false;

    // Animated post rotations are not currently supported by FBX so we need to resample to SRT animation
    // in that case.
    const PRM_Parm *post_rotate_parm = node->getParmList()->getParmPtr("rpost");
    if (post_rotate_parm && post_rotate_parm->isTimeDependent())
	return false;

    // It's non-trivial to know if a python script operator maps nicely so fail on the conservative side
    const OP_Operator &op = *node->getOperator();
    if (op.getScriptIsPython())
	return false;

    // Explicit black list
    static const UT_FSATable full_obj_types(
				     0, "rivet",
				     1, "bone",
				     2, "blend",
				     3, "pythonscript",
				     4, "muscle",
				     5, "switcher",
				     6, "fetch",
				     7, "extractgeo",
				     8, "sticky",
				     9, "blendsticky",
				    -1, nullptr);
    if (full_obj_types.findSymbol(node->getOperator()->getName()) >= 0)
	return false;

    // Fail if we have a pretransform of some sort
    OP_Context op_context(t);
    UT_Matrix4 pretransform;
    node->getPreLocalTransform(op_context, pretransform);
    if (!pretransform.isIdentity())
	return false;

    // Fail if a constraint is turned on.
    if( node->getEvaluatedConstraints() )
        return false;

    return true;
}
/********************************************************************************************************/
void 
ROP_FBXUtil::setStandardTransforms(OP_Node* hd_node, FbxNode* fbx_node, ROP_FBXBaseNodeVisitInfo *node_info, fpreal bone_length, 
				   fpreal ftime, bool use_world_transform)
{
    UT_Vector3D t,r,s;
    FbxVector4 fbx_vec4;

    // Maintain the same rotation order as obj_node. This logic is relied upon
    // by ROP_FBXAnimVisitor::visit() for cracking rotations!
    UT_XformOrder xform_order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
    OBJ_Node* obj_node = CAST_OBJNODE(hd_node);
    if (obj_node)
	xform_order.rotOrder(OP_Node::getRotOrder(obj_node->XYZ(ftime)));

    if(use_world_transform)
    {
	UT_Matrix4D world_matrix;
	OP_Context op_context(ftime);
	(void) hd_node->getWorldTransform(world_matrix, op_context);

	world_matrix.explode(xform_order, r,s,t);
	r.radToDeg();
    }
    else
    {
	ROP_FBXUtil::getFinalTransforms(hd_node, node_info, bone_length, ftime, xform_order, t,r,s, nullptr);
    }

    fbx_vec4.Set(r[0], r[1], r[2]);
    fbx_node->LclRotation.Set(fbx_vec4);

    fbx_vec4.Set(t[0],t[1],t[2]);
    fbx_node->LclTranslation.Set(fbx_vec4);

    fbx_vec4.Set(s[0],s[1],s[2]);
    fbx_node->LclScaling.Set(fbx_vec4);

    // SetRotationActive(true) must be used for the rotation order (as well as
    // pre/post rotations) to be interpreted by the FBX importer.
    fbx_node->SetRotationActive(true);
    fbx_node->SetRotationOrder(FbxNode::eSourcePivot, fbxRotationOrder(xform_order.rotOrder()));

    // Houdini only has one inherit type
    fbx_node->SetTransformationInheritType(FbxTransform::eInheritRSrs);
}

/********************************************************************************************************/
bool 
ROP_FBXUtil::isJointNullNode(OP_Node* null_node, fpreal ftime)
{
    if(!null_node || !null_node->getOperator())
	return false;

    UT_StringRef node_type = null_node->getOperator()->getName();
    if(node_type != "null")
	return false;

    // Find a bone among its children
    OP_Node* child_bone = NULL;
    for (auto &&output : OP_OutputIterator(*null_node))
    {
	if (!output->getOperator())
	    continue;

	node_type = output->getOperator()->getName();
	if(node_type == "bone")
	{
	    child_bone = output;
	    break;
	}

    }

    // If a Null has a cregion inside of it, export it as a joint 
    OP_NodeList children;
    null_node->getAllChildren(children);
    const int child_count = children.entries();
    for (int i = 0; i < child_count; ++i)
    {
        OP_Node *child_node = children(i);
        UT_StringRef child_node_type = child_node->getOperator()->getName();

        if (child_node_type == "cregion")
        {
            return true;
        }		
    }

    if(!child_bone)	
	return false;

    return isDummyBone(child_bone, ftime);
}

/********************************************************************************************************/
bool 
ROP_FBXUtil::isDummyBone(OP_Node* bone_node, fpreal ftime)
{
    // A bone is a dummy bone iff:
    // 1) It is a child of a null node
    // 2) It has no children
    // 3) It has a look-at node
    // 4) Its look-at node is a null node which is a child of its parent null-node.
    // 5) Its length is an expression.

    if(!bone_node)
	return false;

    UT_StringRef node_type = bone_node->getOperator()->getName();
    if(node_type != "bone")
	return false;

    // Get num children
    if(bone_node->hasAnyOutputNodes())
	return false;

    OP_Node* look_at_node = NULL;
    UT_String look_at_path;
    ROP_FBXUtil::getStringOPParm(bone_node, "lookatpath", look_at_path, ftime);
    if(look_at_path.isstring() == false)
	return false;

    look_at_node = bone_node->findNode(look_at_path);
    if(!look_at_node)
	return false;

    // Check if bone length is an expression
    UT_String bone_length_str;
    ROP_FBXUtil::getStringOPParm(bone_node, "length", bone_length_str, ftime);
    if (!bone_length_str.isFloat())
	return false;

    // Look through all its parents
    int i;
    OP_Node* parent_null_node = NULL;
    for( i = bone_node->getConnectedInputIndex(-1); !parent_null_node && i >= 0;
	i = bone_node->getConnectedInputIndex(i) )
    {
	// Only traverse up real inputs, not reference inputs. But we
	// do want to search up out of subnets, which getInput() does.
	if( bone_node->getInput(i) && !bone_node->isRefInput(i) )
	{
	    node_type = bone_node->getInput(i)->getOperator()->getName();
	    if(node_type == "null")
	    {
		parent_null_node = bone_node->getInput(i);
		break;
	    }
	}
    }
    if(!parent_null_node)
	return false;

    // Look through the parents of our look at node to see if the parent null node is in them.
    bool found_parent = false;
    for( i = look_at_node->getConnectedInputIndex(-1); !found_parent && i >= 0;
	i = look_at_node->getConnectedInputIndex(i) )
    {
	// Only traverse up real inputs, not reference inputs. But we
	// do want to search up out of subnets, which getInput() does.
	if( look_at_node->getInput(i) && !look_at_node->isRefInput(i) )
	{
	    if(look_at_node->getInput(i) == parent_null_node)
	    {
		found_parent = true;
		break;
	    }
	}
    }    

    if(!found_parent)
	return false;

    return true;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::isLODGroupNullNode(OP_Node* null_node)
{
    if (!null_node)
	return false;

    UT_StringRef node_type = null_node->getOperator()->getName();
    if (node_type != "null")
	return false;

    // See if we stored the node type as a spare parameter
    PRM_Parm *parm;
    UT_String strref = "";
    if (null_node->getParameterOrProperty("fbx_node_attribute", 0, null_node, parm, true, NULL))
	parm->getValue(0, strref, 0, false, SYSgetSTID());

    if (strref.length() > 0)
	if (strref.equal("LODGroup", false))
	    return true;

    // Also consider nodes named LODGroup
    if ( null_node->getName().startsWith("LODGroup", false) )
	return true;

    return false;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::outputCustomProperties(OP_Node* node, FbxObject* fbx_node)
{
    if (!node || !fbx_node)
	return false;

    // Custom attributes are stored on import as spare param
    int numparms = node->getParmList()->getEntries();
    for (int n = 0; n < numparms; n++)
    {
	PRM_Parm *parm = node->getParmList()->getParmPtr(n);
	if (!parm)
	    continue;

	// Make sure the parameter is visible
	PRM_Type parm_type = parm->getType();
	if (!parm_type.isVisible())
	    continue;

	// Imported fbx params have their token starting with "fbx_"
	UT_String param_token;
	parm->getToken(param_token);
	if (!param_token.startsWith("fbx_"))
	    continue;

	// Deduce the corresponding FbxType from the param type
	EFbxType fbx_prop_type = eFbxUndefined;
	if (parm_type.isOrdinalType() && (parm_type.getOrdinalType() == PRM_Type::PRM_ORD_TOGGLE))
	    fbx_prop_type = eFbxBool;
	else if (parm_type.isStringType())
	    fbx_prop_type = eFbxString;
	else if (parm_type.isFloatType())
	{
	    if (parm_type.getFloatType() == PRM_Type::PRM_FLOAT_INTEGER)
		fbx_prop_type = eFbxInt;
	    else if (parm_type.getFloatType() == PRM_Type::PRM_FLOAT_RGBA)
		fbx_prop_type = eFbxDouble3;
	    else
	    {
		if (parm->getVectorSize() == 3)
		    fbx_prop_type = eFbxDouble3;
		else if (parm->getVectorSize() == 4)
		    fbx_prop_type = eFbxDouble4;
		else
		    fbx_prop_type = eFbxFloat;
	    }
	}

	if (fbx_prop_type == eFbxBool)
	{
	    // Boolean property
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxBoolDT, parm->getLabel(), parm->getLabel());

	    int32 val;
	    parm->getValue(0.0, val, 0, SYSgetSTID());

	    FbxBool prop_val = val == 1 ? true : false;
	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	else if (fbx_prop_type == eFbxFloat)
	{
	    // Floating point property
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxFloatDT, parm->getLabel(), parm->getLabel());

	    fpreal val;
	    parm->getValue(0, val, 0, SYSgetSTID());
	    FbxFloat prop_val = (float)val;

	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	else if (fbx_prop_type == eFbxDouble4)
	{
	    // 4-vector property.(eFbxDouble4)
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxDouble4DT, parm->getLabel(), parm->getLabel());

	    fpreal val, val1, val2, val3;
	    parm->getValue(0, val, 0, SYSgetSTID());
	    parm->getValue(0, val1, 1, SYSgetSTID());
	    parm->getValue(0, val2, 2, SYSgetSTID());
	    parm->getValue(0, val3, 3, SYSgetSTID());

	    FbxDouble4 prop_val;
	    prop_val[0] = val;
	    prop_val[1] = val1;
	    prop_val[2] = val2;
	    prop_val[3] = val3;

	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	else if (fbx_prop_type == eFbxInt)
	{
	    // Integer property (eFbxInt)
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxIntDT, parm->getLabel(), parm->getLabel());

	    int32 val;
	    parm->getValue(0, val, 0, SYSgetSTID());
	    FbxInt prop_val = (float)val;

	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	else if (fbx_prop_type == eFbxDouble3 )
	{
	    // 3-vector property (fbx_prop_type == eFbxDouble3)
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxDouble3DT, parm->getLabel(), parm->getLabel());

	    fpreal val, val1, val2;
	    parm->getValue(0, val, 0, SYSgetSTID());
	    parm->getValue(0, val1, 1, SYSgetSTID());
	    parm->getValue(0, val2, 2, SYSgetSTID());

	    FbxDouble3 prop_val;
	    prop_val[0] = val;
	    prop_val[1] = val1;
	    prop_val[2] = val2;

	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	else if ( fbx_prop_type == eFbxString )
	{
	    // String property (eFbxString)
	    FbxProperty curr_prop = FbxProperty::Create(fbx_node, FbxStringDT, parm->getLabel(), parm->getLabel());

	    UT_String val;
	    parm->getValue(0, val, 0, /*expand=*/true, SYSgetSTID());

	    FbxString prop_val(val.c_str());
	    curr_prop.Set(prop_val);
	    curr_prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
	}
	/*
	else if ( parm_type == PRM_CHOICELIST_SINGLE )
	{
	    // String list property. Slightly special.
	    TFbxStringVector strings;
	}
	*/
    }

    return true;
}

/********************************************************************************************************/
OP_Node* 
ROP_FBXUtil::findNonInstanceTargetFromInstance(OP_Node* instance_ptr, fpreal ftime)
{
    if(!instance_ptr)
	return NULL;

    OP_Node* curr_node = instance_ptr;
    UT_String node_type, target_obj_path;

    while(curr_node)
    {
	node_type = curr_node->getOperator()->getName();
	if(node_type != "instance")
	    break;

	ROP_FBXUtil::getStringOPParm(curr_node, "instancepath", target_obj_path, ftime);
	curr_node = curr_node->findNode(target_obj_path);
    }

    return curr_node;
}

void
ROP_FBXUtil::getNodeName(OP_Node* node, UT_String& node_name, ROP_FBXNodeManager* node_manager, fpreal ftime)
{
    // First, see if we have a "fbx_node_name" param value on the node
    ROP_FBXUtil::getStringOPParm(node, "fbx_node_name", node_name, ftime);
    if (node_name.length() > 0)
	return;

    // No param, read the node name and make it unique
    node_name = UT_String(UT_String::ALWAYS_DEEP, node->getName());

    // Optional, this will only do anything for "Scene"
    if ( node_manager )
	node_manager->makeNameUnique( node_name );
}

/********************************************************************************************************/
GA_PrimCompat::TypeMask
ROP_FBXUtil::getGdpPrimId(const GU_Detail* gdp)
{
    // Now return all encountered types
    const GEO_Primitive *prim = NULL;
    GA_PrimCompat::TypeMask prim_type(0);
    GA_FOR_ALL_PRIMITIVES(gdp, prim)
    {
	prim_type |= prim->getPrimitiveId();
    }
    return prim_type;
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
    // We need to delete all vertex caches here, since some of them may be shared.
    THDToNodeInfoMap::iterator mi;
    TGDPCacheSet caches_to_delete;
    TGDPCacheSet::iterator si;
    for(mi = myHdToNodeInfoMap.begin(); mi != myHdToNodeInfoMap.end(); mi++)
    {
	if(mi->second->getVertexCache())
	{
	    caches_to_delete.insert(mi->second->getVertexCache());
	    // Prevent its desctructor from deleting the object.
	    mi->second->setVertexCache(NULL);
	}
    }
    
    for(si = caches_to_delete.begin(); si != caches_to_delete.end(); si++)
	delete *si;

    // Deallocate one of the maps. They should be in sync, objects in both point
    // to the same thing.
    for(mi = myHdToNodeInfoMap.begin(); mi != myHdToNodeInfoMap.end(); mi++)
	delete mi->second;
    myHdToNodeInfoMap.clear();
    myFbxToNodeInfoMap.clear();
}
/********************************************************************************************************/
void 
ROP_FBXNodeManager::findNodeInfos(OP_Node* hd_node, TFbxNodeInfoVector &res_infos)
{
    THDToNodeInfoMap::iterator mi, li, ui;
    res_infos.clear();
    li = myHdToNodeInfoMap.lower_bound(hd_node);
    ui = myHdToNodeInfoMap.upper_bound(hd_node);
    for(mi = li; mi != ui; mi++)
    {
	res_infos.push_back(mi->second);
    }
}
/********************************************************************************************************/
ROP_FBXNodeInfo* 
ROP_FBXNodeManager::findNodeInfo(FbxNode* fbx_node)
{
    if(!fbx_node)
	return NULL;

    TFbxToNodeInfoMap::iterator mi = myFbxToNodeInfoMap.find(fbx_node);
    if(mi == myFbxToNodeInfoMap.end())
	return NULL;
    else
	return (mi->second);
}
/********************************************************************************************************/
ROP_FBXNodeInfo& 
ROP_FBXNodeManager::addNodePair(OP_Node* hd_node, FbxNode* fbx_node, ROP_FBXMainNodeVisitInfo& visit_info)
{
    ROP_FBXNodeInfo* new_info = new ROP_FBXNodeInfo();
    new_info->setFbxNode(fbx_node);
    new_info->setHdNode(hd_node);
    new_info->setVisitInfoCopy(visit_info);

    //myHdToNodeInfoMap[hd_node] = new_info;
    myHdToNodeInfoMap.insert(THDToNodeInfoMap::value_type(hd_node, new_info));
    myFbxToNodeInfoMap[fbx_node] = new_info;


    return *new_info;
}
/********************************************************************************************************/
void 
ROP_FBXNodeManager::makeNameUnique(UT_String& strName)
{
    // Special case - never, ever, ever, export a node named "Scene", since this will
    // send older FBX SDKs into an infinite recursion and eventually crash.
    if(strName == "Scene")
	strName.incrementNumberedName();

    /*This section was commented out because the exported sections caused
    unique names for the whole tree, not just within the section. Houdini also
    has a requirement that node names are unique within a section already so
    there is no need for the following logic. Fix for RFE #67311.
    http://internal.sidefx.com/issues/showbrief.php?id=67311 */

    /*TStringSet::iterator si;
    si = myNamesSet.find((const char*)strName);

    // NOTE: This will be slow if we sequentially generate names
    // since it will grow through all previously numbered names first
    // for each new name. It's really meant for a small number of
    // possibly identically named objeccts.
    while(si != myNamesSet.end())
    {
	strName.incrementNumberedName();
	si = myNamesSet.find((const char*)strName);
    }

    myNamesSet.insert((const char*)strName); */
}
/********************************************************************************************************/
void 
ROP_FBXNodeManager::addBundledNode(OP_Node* hd_node)
{
    myNodesInBundles.insert(hd_node);
}
/********************************************************************************************************/
bool 
ROP_FBXNodeManager::isNodeBundled(OP_Node* hd_node)
{
    THDNodeSet::iterator si = myNodesInBundles.find(hd_node);
    return (si != myNodesInBundles.end());
}
/********************************************************************************************************/
// ROP_FBXNodeInfo
/********************************************************************************************************/
ROP_FBXNodeInfo::ROP_FBXNodeInfo() : myVisitInfoCopy(NULL)
{
    myHdNode = NULL;
    myFbxNode = NULL;
    myMaxObjectPoints = 0;
    myVertexCacheMethod = ROP_FBXVertexCacheMethodNone;
    myVertexCache = NULL;
    mySourcePrim = -1;
    myIsSurfacesOnly = false;
    myTravelledIndex = -1;
    
    myVisitResultType = ROP_FBXVisitorResultOk;
}
/********************************************************************************************************/
ROP_FBXNodeInfo::ROP_FBXNodeInfo(FbxNode* main_node) : myVisitInfoCopy(NULL)
{
    myHdNode = NULL;
    myFbxNode = main_node;
    myMaxObjectPoints = 0;
    myVertexCacheMethod = ROP_FBXVertexCacheMethodNone;
    myVertexCache = NULL;
    mySourcePrim = -1;
    myIsSurfacesOnly = false;
    myTravelledIndex = -1;
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
void 
ROP_FBXNodeInfo::setTraveledInputIndex(int idx)
{
    myTravelledIndex = idx;
}
/********************************************************************************************************/
int 
ROP_FBXNodeInfo::getTraveledInputIndex(void)
{
    return myTravelledIndex;
}
/********************************************************************************************************/
OP_Node* 
ROP_FBXNodeInfo::getHdNode(void) const
{
    return myHdNode;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setHdNode(OP_Node* node)
{
    myHdNode = node;
}
/********************************************************************************************************/
FbxNode* ROP_FBXNodeInfo::getFbxNode(void) const
{
    return myFbxNode;
}
/********************************************************************************************************/
void ROP_FBXNodeInfo::setFbxNode(FbxNode* node)
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
ROP_FBXVisitorResultType 
ROP_FBXNodeInfo::getVisitResultType(void)
{
    return myVisitResultType;
}
/********************************************************************************************************/
void  
ROP_FBXNodeInfo::setVisitResultType(ROP_FBXVisitorResultType res_type)
{
    myVisitResultType = res_type;
}
/********************************************************************************************************/
ROP_FBXMainNodeVisitInfo& 
ROP_FBXNodeInfo::getVisitInfo(void)
{
    return myVisitInfoCopy;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setVisitInfoCopy(ROP_FBXMainNodeVisitInfo& info)
{
    // Default == is good enough
    myVisitInfoCopy = info;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setIsSurfacesOnly(bool value)
{
    myIsSurfacesOnly = value;
}
/********************************************************************************************************/
bool 
ROP_FBXNodeInfo::getIsSurfacesOnly(void)
{
    return myIsSurfacesOnly;
}
/********************************************************************************************************/
void 
ROP_FBXNodeInfo::setSourcePrimitive(int prim_cnt)
{
    mySourcePrim = prim_cnt;
}
/********************************************************************************************************/
int
ROP_FBXNodeInfo::getSourcePrimitive(void)
{
    return mySourcePrim;
}
/********************************************************************************************************/
void ROP_FBXNodeInfo::addBlendShapeNode(OP_Node* node)
{
    myBlendShapeNodes.push_back(node);
}
/********************************************************************************************************/
int ROP_FBXNodeInfo::getBlendShapeNodeCount() const
{
    return myBlendShapeNodes.size();
}
/********************************************************************************************************/
OP_Node* ROP_FBXNodeInfo::getBlendShapeNodeAt(const int& index)
{
    if ((index >= 0) && (index < myBlendShapeNodes.size()))
	return myBlendShapeNodes[index];

    return NULL;
}

/********************************************************************************************************/
// ROP_FBXGDPCached
/********************************************************************************************************/
ROP_FBXGDPCache::ROP_FBXGDPCache()
{
    mySaveMemory = false;
    myMinFrame = SYS_FPREAL_MAX;
    myNumConstantPoints = -1;
}
/********************************************************************************************************/
ROP_FBXGDPCache::~ROP_FBXGDPCache()
{
    clearFrames();
}
/********************************************************************************************************/
bool 
ROP_FBXGDPCache::getSaveMemory(void)
{
    return mySaveMemory;
}
/********************************************************************************************************/
void 
ROP_FBXGDPCache::setSaveMemory(bool value)
{
    mySaveMemory = value;
}
/********************************************************************************************************/
void 
ROP_FBXGDPCache::clearFrames(void)
{
    int curr_item, num_items = myFrameItems.size();
    for(curr_item = 0; curr_item < num_items; curr_item++)
	delete myFrameItems[curr_item];
    myFrameItems.clear();

    myMinFrame = SYS_FPREAL_MAX;
}
/********************************************************************************************************/
GU_Detail* 
ROP_FBXGDPCache::addFrame(fpreal frame_num)
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
GU_Detail* 
ROP_FBXGDPCache::getFrameGeometry(fpreal frame_num)
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
fpreal 
ROP_FBXGDPCache::getFirstFrame(void)
{
    return myMinFrame;
}
/********************************************************************************************************/
bool 
ROP_FBXGDPCache::getIsNumPointsConstant(void)
{
    return (myNumConstantPoints > 0);
}
/********************************************************************************************************/
void 
ROP_FBXGDPCache::setNumConstantPoints(int num_points)
{
    myNumConstantPoints = num_points;
}
/********************************************************************************************************/
int 
ROP_FBXGDPCache::getNumConstantPoints(void)
{
    return myNumConstantPoints;
}
/********************************************************************************************************/
