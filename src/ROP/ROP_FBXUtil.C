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
#include "ROP_FBXCommon.h"
#include <UT/UT_Interrupt.h>
#include <UT/UT_Thread.h>
#include <UT/UT_CrackMatrix.h>
#include <GU/GU_DetailHandle.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <CH/CH_Manager.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Vertex.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_ConvertParms.h>
#include <GU/GU_PrimPoly.h>
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
ROP_FBXUtil::getStringOPParm(OP_Node *node, const char* parmName, UT_String &strref, bool do_expand, fpreal ftime)
{
    PRM_Parm	 *parm;
    strref = "";

    if(!node)
	return;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
	parm->getValue(ftime, strref, 0, do_expand, UTgetSTID());
}
/********************************************************************************************************/
int 
ROP_FBXUtil::getIntOPParm(OP_Node *node, const char* parmName, int index, fpreal ftime)
{
    if(!node)
	return 0;

    PRM_Parm	 *parm;
    int res = 0;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
	parm->getValue(ftime, res, index, UTgetSTID());

    return res;
}
/********************************************************************************************************/
fpreal 
ROP_FBXUtil::getFloatOPParm(OP_Node *node, const char* parmName, int index, fpreal ftime, bool *did_find)
{
    if(did_find)
	*did_find = false;
    PRM_Parm	 *parm;
    fpreal res = 0.0;

    if(!node)
	return 0.0;

    if (node->getParameterOrProperty(parmName, 0, node, parm, true, NULL))
    {
	parm->getValue(ftime, res, index, UTgetSTID());
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
    int prim_type_res;

    for(curr_frame = start_frame; curr_frame <= end_frame; curr_frame++)
    {
	hd_time = ch_manager->getTime(curr_frame);
	if(boss_op && curr_frame % 5)
	{
	    if(boss_op->opInterrupt())
		return -1;
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
	    if(!gdp || gdp->primitives().entries() <= 0)
		continue;

	    GU_Detail *conv_gdp;
	    unsigned prim_type = ROP_FBXUtil::getGdpPrimId(gdp);

	    GU_Detail temp_detail;
	    // We must save the start frame in any case since it is needed
	    // elsewhere
	    if(v_cache_out->getSaveMemory() && curr_frame != start_frame)
		conv_gdp = &temp_detail;
	    else
		conv_gdp = v_cache_out->addFrame(curr_frame);

	    if(prim_type == GEOPRIMPART)
	    {
		convertParticleGDPtoPolyGDP(gdp, *conv_gdp);
		is_num_verts_constant = false;
	    }
	    else
	    {
		prim_type_res = prim_type & (~(GEOPRIMNURBSURF | GEOPRIMBEZSURF | GEOPRIMNURBCURVE | GEOPRIMBEZCURVE));
		if(prim_type_res != 0)
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

	    curr_num_points = conv_gdp->points().entries();
	    if(curr_num_points > max_points)	    
		max_points = curr_num_points;

	}
    }

    if(is_num_verts_constant && allow_constant_point_detection)
	v_cache_out->setNumConstantPoints(first_frame_num_points);

    if(is_num_verts_constant && is_surfs_only && !convert_surfaces)
    {
	max_points = first_frame_num_points;
	is_pure_surfaces = true;
    }

    return max_points;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::isVertexCacheable(OP_Network *op_net, bool include_deform_nodes, fpreal ftime, bool& found_particles)
{
    OP_Node* dyn_node, *part_node;

    const char *const dynamics_node_types[] = { "dopimport", "channel", 0};
    const char *const dynamics_node_types_with_deforms[] = { "dopimport", "channel", "file", "deform", 0};
    const char *const particle_node_types[] = { "popnet", 0};

    found_particles = false;

    // First, look for particles
    part_node = ROP_FBXUtil::findOpInput(op_net->getRenderNodePtr(), particle_node_types, true, NULL, NULL);
    if(part_node)
    {
	found_particles = true;
	return true;
    }

    // If include_deform_nodes is true, we want to flag deforms as vc objects. This means that if it is false,
    // we can ignore them.

    // Look for any time-dependent nodes in general.
    const char *const deform_node[] = { "deform", 0 };
    if(ROP_FBXUtil::findTimeDependentNode(op_net->getRenderNodePtr(), ROP_FBXallowed_inbetween_node_types, ( include_deform_nodes ? NULL : deform_node ), ftime, true))
	return true;
    
    // Then, if not found, look for other dynamic nodes
    if(include_deform_nodes)
	dyn_node = ROP_FBXUtil::findOpInput(op_net->getRenderNodePtr(), dynamics_node_types_with_deforms, true, NULL, NULL);
    else
	dyn_node = ROP_FBXUtil::findOpInput(op_net->getRenderNodePtr(), dynamics_node_types, true, NULL, NULL);

    if(dyn_node)
	return true;
    else
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
    GU_ConvertParms conv_parms;
    conv_parms.fromType = GEOPRIMALL;
    conv_parms.toType = GEOPRIMPOLY;
    conv_parms.method.setULOD(lod);
    conv_parms.method.setVLOD(lod);
#ifdef UT_DEBUG
    cook_start = clock();
#endif
    conv_gdp.duplicate(*src_gdp);
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_duplicateTime += (cook_end - cook_start);

    cook_start = clock();
#endif
    num_pre_proc_points = conv_gdp.points().entries();
    conv_gdp.convert(conv_parms);
//    num_pre_proc_points = conv_gdp.points().entries();

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
    GU_Detail conv_ordered_gdp;

    int curr_prim_idx, num_prims = conv_gdp.primitives().entries();
    GEO_Point *temp_pts[3];
    GEO_Primitive* prim;
    GEO_PrimPoly *prim_poly_ptr;
    GU_PrimPoly *gu_prim;

    for(curr_prim_idx = 0; curr_prim_idx < num_prims; curr_prim_idx++)
    {
	prim = conv_gdp.primitives()(curr_prim_idx);
	gu_prim = dynamic_cast<GU_PrimPoly *>(prim);

	// In some cases (such as triangulating NURBS), we sometimes 
	// end up with faces that have more than three vertices (and points),
	// where some of the points actually occupy the same position in world space.
	// After removing them, these faces become proper triangles.
	if(gu_prim && prim->getVertexCount() > 3)
	    gu_prim->removeRepeatedPoints(1e-5);

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
	prim_poly_ptr->close(1,1);
    }
#ifdef UT_DEBUG
    cook_end = clock();
    ROP_FBXdb_reorderTime += (cook_end - cook_start);
#endif
}
/********************************************************************************************************/
bool 
ROP_FBXUtil::getFinalTransforms(OP_Node* hd_node, ROP_FBXBaseNodeVisitInfo *node_info, bool has_lookat_node, fpreal bone_length, fpreal time_in, UT_String* override_node_type,
			UT_Vector3& t_out, UT_Vector3& r_out, UT_Vector3& s_out, KFbxVector4* post_rotation, UT_Vector3* prev_frame_rotations)
{
    bool set_post_rotation = false;

    OBJ_Node* obj_node = NULL;
    if(hd_node)
	obj_node = dynamic_cast<OBJ_Node*>(hd_node);

    bool do_special_rotate = false;
    UT_String node_type(UT_String::ALWAYS_DEEP, "");
    if(hd_node)
	node_type = hd_node->getOperator()->getName();

    if(node_type == "hlight" || node_type == "cam")
	do_special_rotate = true;
    if(override_node_type)
    {
	if(*override_node_type == "hlight" || *override_node_type == "cam")
	    do_special_rotate = true;
    }

    // Get and set transforms
    OP_Context op_context(time_in);
    UT_DMatrix4 full_xform;
    if(obj_node)
    {
	UT_DMatrix4		 world_xform, lookat, local_xform, parm_xform;
	obj_node->getPreLocalTransform(op_context, local_xform);
	obj_node->getParmTransform(op_context, parm_xform);
	full_xform = parm_xform * local_xform;

	obj_node->getWorldTransform(world_xform, op_context);

	if(node_type == "blend")
	{
	    OBJ_Node* parent_obj_node = NULL;
	    if(node_info && node_info->getParentInfo())
		parent_obj_node = dynamic_cast<OBJ_Node*>(node_info->getParentInfo()->getHdNode());

	    if(parent_obj_node)
	    {
		UT_DMatrix4 parent_world_xform;
		parent_obj_node->getWorldTransform(parent_world_xform, op_context);
		parent_world_xform.invert();
		full_xform = world_xform * parent_world_xform;
	    }
	    else
		full_xform = world_xform;
	}

	if (obj_node->buildLookAt(op_context, world_xform, lookat))
	{
	    full_xform = lookat * full_xform;
	}
    }
    else
	full_xform.identity();

    if(SYSequalZero(bone_length) == false)
    {
	// Add a bone length transform
	UT_DMatrix4 bone_trans;
	UT_XformOrder xform_default;
	bone_trans.identity();
	bone_trans.xform(xform_default, 0.0,0.0,-bone_length, 
	    0.0,0.0,0.0,
	    1.0,1.0,1.0,
	    0.0,0.0,0.0,
	    0);

	full_xform = full_xform * bone_trans;
    }

    if(do_special_rotate && !has_lookat_node && post_rotation)
    {
	// Set post-transform instead
	if(override_node_type)
	{
	    if(*override_node_type == "hlight")
		post_rotation->Set(-90,0,0);
	    else // if(node_type == "cam")
		post_rotation->Set(0,-90,0);
	}
	else
	{
	    if(node_type == "hlight")
		post_rotation->Set(-90,0,0);
	    else // if(node_type == "cam")
		post_rotation->Set(0,-90,0);
	}
	set_post_rotation = true;
    }

    UT_XformOrder xform_order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
    full_xform.explode(xform_order, r_out,s_out,t_out);
    fpreal rots_out[3];
    if(prev_frame_rotations)
    {
	UT_Vector3 prev_rot(*prev_frame_rotations);
	prev_rot.degToRad();

	// Because UTcrackMatrixSmooth takes a reference to fpreal while
	// our UT_Vector3 is templated and depends on the build, we need
	// this silliness here to let it actually compile.
	rots_out[0] = r_out.x();
	rots_out[1] = r_out.y();
	rots_out[2] = r_out.z();

	UTcrackMatrixSmooth(UT_XformOrder::SRT, rots_out[0], rots_out[1], rots_out[2], prev_rot.x(),
	    prev_rot.y(), prev_rot.z());

	r_out.assign(rots_out[0], rots_out[1], rots_out[2]);
    }
    r_out.radToDeg();

    return set_post_rotation;
}
/********************************************************************************************************/
bool
ROP_FBXUtil::findTimeDependentNode(OP_Node *op, const char* const ignored_node_types[], const char * const opt_more_types[], fpreal ftime, bool include_me)
{
    bool is_time_dependent = false;
    OP_Node *	found = NULL;
    int		i;
    OP_Context op_context(ftime);

    // Check if the node we are checking is already on the stack. If so,
    // we're at risk of recursion, so bail out.
    if( op != NULL && !op->flags().getRecursion()) // Don't care about locks: && !op->getHardLock() )
    {
	op->flags().setRecursion( true );

	if( include_me )
	{
	    // See if this op is one of the types we can ignore.
	    for( i = 0; !found && ignored_node_types[i]; i++ )
	    {
		if( op->getOperator()->getName() == ignored_node_types[i] )
		    found = op;  
	    }

	    if(opt_more_types)
	    {
		for( i = 0; !found && opt_more_types[i]; i++ )
		{
		    if( op->getOperator()->getName() == opt_more_types[i] )
			found = op;  
		}
	    }

	    if(!found)
		is_time_dependent |= op->isTimeDependent(op_context);		
	}

	// Found a time-dependent node. That's all we need.
	if(is_time_dependent)
	{
	    op->flags().setRecursion( false );
	    return true;
	}

	// If we are a subnet, try to find an appropriate node within the
	// subnet, starting with the subnet's display node.
	if(op->isSubNetwork(false) )
	{
	    is_time_dependent |= ROP_FBXUtil::findTimeDependentNode(((OP_Network *)op)->getDisplayNodePtr(), ignored_node_types, opt_more_types, ftime, true);
	}

	// if we're a switch SOP, then look up the appropriate input chain
	bool is_aswitch = false;
	if(op->getOperator()->getName() == "switch" )
	{
	    PRM_Parm *	parm;

	    parm = op->getParmList()->getParmPtr( "input" );
	    if( parm != NULL )
	    {
		parm->getValue(
		    ftime,
		    i, 0, op_context.getThread());
		if( op->getInput(i) != NULL )
		{
		    is_aswitch = true;
		    is_time_dependent |= ROP_FBXUtil::findTimeDependentNode( op->getInput(i), ignored_node_types, opt_more_types, ftime, true);

		    // Found a time-dependent node. That's all we need.
		    if(is_time_dependent)
		    {
			op->flags().setRecursion( false );
			return true;
		    }

		}
	    }
	}

	for( i = op->getConnectedInputIndex(-1); !is_aswitch && !is_time_dependent && i >= 0;
	    i = op->getConnectedInputIndex(i) )
	{
	    // We need to traverse reference inputs as well, in cases,
	    // for example, where particles are present.
	    if( op->getInput(i) ) //  && !op->isRefInput(i) )
	    {
		is_time_dependent |= ROP_FBXUtil::findTimeDependentNode(op->getInput(i), ignored_node_types, opt_more_types, ftime, true);
		// Found a time-dependent node. That's all we need.
		if(is_time_dependent)
		{
		    op->flags().setRecursion( false );
		    return true;
		}
	    }
	}

	op->flags().setRecursion( false );
    }

    return is_time_dependent;
}
/********************************************************************************************************/
OP_Node*
ROP_FBXUtil::findOpInput(OP_Node *op, const char * const find_op_types[], bool include_me, const char* const  allowed_node_types[], bool *did_find_allowed_only, int rec_level)
{
    OP_Node *	found = NULL;
    int		i;
    bool did_is_allowed_only_local;
    bool child_did_find_allowed_types_only;
    int thread = UTgetSTID();

    if(rec_level == 0 && did_find_allowed_only)
	*did_find_allowed_only = true;

    // Check if the node we are checking is already on the stack. If so,
    // we're at risk of recursion, so bail out.
    if( op != NULL && !op->flags().getRecursion()) // Don't care about locks: && !op->getHardLock() )
    {
	op->flags().setRecursion( true );

	if( include_me )
	{
	    // See if this op is one of the types we are looking for.
	    for( i = 0; !found && find_op_types[i]; i++ )
		if( op->getOperator()->getName() == find_op_types[i] )
		    found = op;

	    // See if this op, if not found, is one of the allowed types
	    if(allowed_node_types && !found)
	    {
		did_is_allowed_only_local = false; 

		for( i = 0; !did_is_allowed_only_local && allowed_node_types[i]; i++ )
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

	// If we are a subnet, try to find an appropriate node within the
	// subnet, starting with the subnet's display node.
	if( !found && op->isSubNetwork(false) )
	{
	    child_did_find_allowed_types_only = false;
	    found = ROP_FBXUtil::findOpInput(((OP_Network *)op)->getDisplayNodePtr(),
				   find_op_types, true, allowed_node_types, &child_did_find_allowed_types_only, rec_level+1);
	    if(found && !child_did_find_allowed_types_only && did_find_allowed_only)
		*did_find_allowed_only = false;
	}

	// if we're a switch SOP, then look up the appropriate input chain
	bool is_aswitch = false;
	if( !found && op->getOperator()->getName() == "switch" )
	{
	    PRM_Parm *	parm;
	    
	    parm = op->getParmList()->getParmPtr( "input" );
	    if( parm != NULL )
	    {
		parm->getValue(
			CHgetEvalTime(),
			i, 0, thread);
		if( op->getInput(i) != NULL )
		{
		    is_aswitch = true;
	    	    child_did_find_allowed_types_only = false;
		    found = ROP_FBXUtil::findOpInput( op->getInput(i), find_op_types, true, allowed_node_types, &child_did_find_allowed_types_only, rec_level+1 );
		    if(found && !child_did_find_allowed_types_only && did_find_allowed_only)
			*did_find_allowed_only = false;
		}
	    }
	}

	for( i = op->getConnectedInputIndex(-1); !is_aswitch && !found && i >= 0;
	     i = op->getConnectedInputIndex(i) )
	{
	    // We need to traverse reference inputs as well, in cases,
	    // for example, where particles are present.
	    if( op->getInput(i)) // && !op->isRefInput(i) )
	    {
        	child_did_find_allowed_types_only = false;
		found = ROP_FBXUtil::findOpInput(op->getInput(i), find_op_types, true, allowed_node_types, &child_did_find_allowed_types_only, rec_level+1 );
		if(found && !child_did_find_allowed_types_only && did_find_allowed_only)
		    *did_find_allowed_only = false;
	    }
	}

	op->flags().setRecursion( false );
    }

    return found;
}
/********************************************************************************************************/
void 
ROP_FBXUtil::setStandardTransforms(OP_Node* hd_node, KFbxNode* fbx_node, ROP_FBXBaseNodeVisitInfo *node_info, bool has_lookat_node, fpreal bone_length, 
				   fpreal ftime, UT_String* override_node_type, bool use_world_transform)
{

    UT_Vector3 t,r,s;
    KFbxVector4 post_rotate, fbx_vec4;

    if(use_world_transform)
    {
	UT_DMatrix4 world_matrix;
	OP_Context op_context(ftime);
	(void) hd_node->getWorldTransform(world_matrix, op_context);

	UT_XformOrder xform_order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
	world_matrix.explode(xform_order, r,s,t);
	r.radToDeg();
    }
    else if(ROP_FBXUtil::getFinalTransforms(hd_node, node_info, has_lookat_node, bone_length, ftime, override_node_type, t,r,s, &post_rotate, NULL))
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
bool 
ROP_FBXUtil::isJointNullNode(OP_Node* null_node)
{
    if(!null_node)
	return false;

    UT_String node_type = null_node->getOperator()->getName();
    if(node_type != "null")
	return false;

    // Find a bone among its children
    OP_Node* child_bone = NULL;
    int curr_child, num_children = null_node->nOutputs();
    for(curr_child = 0; curr_child < num_children; curr_child++)
    {
	if(null_node->getOutput(curr_child))
	{
	    node_type = null_node->getOutput(curr_child)->getOperator()->getName();
	    if(node_type == "bone")
	    {
		child_bone = null_node->getOutput(curr_child);
		break;
	    }
	}
    }
    if(!child_bone)	
	return false;

    return isDummyBone(child_bone);
}
/********************************************************************************************************/
bool 
ROP_FBXUtil::isDummyBone(OP_Node* bone_node)
{
    // A bone is a dummy bone iff:
    // 1) It is a child of a null node
    // 2) It has no children
    // 3) It has a look-at node
    // 4) Its look-at node is a null node which is a child of its parent null-node.
    // 5) Its length is an expression.

    if(!bone_node)
	return false;

    UT_String node_type = bone_node->getOperator()->getName();
    if(node_type != "bone")
	return false;

    // Get num children
    if(bone_node->nOutputs() > 0)
	return false;

    OP_Node* look_at_node = NULL;
    UT_String look_at_path;
    ROP_FBXUtil::getStringOPParm(bone_node, "lookatpath", look_at_path, true);
    if(look_at_path.isstring() == false)
	return false;

    look_at_node = bone_node->findNode(look_at_path);
    if(!look_at_node)
	return false;

    // Check if bone length is an expression
    UT_String bone_length_str;
    ROP_FBXUtil::getStringOPParm(bone_node, "length", bone_length_str, true);
    if (bone_length_str.isFloat())
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
OP_Node* 
ROP_FBXUtil::findNonInstanceTargetFromInstance(OP_Node* instance_ptr)
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

	ROP_FBXUtil::getStringOPParm(curr_node, "instancepath", target_obj_path, true);
	curr_node = curr_node->findNode(target_obj_path);
    }

    return curr_node;
}
/********************************************************************************************************/
unsigned 
ROP_FBXUtil::getGdpPrimId(const GU_Detail* gdp)
{
    // Now return all encountered types
    const GEO_Primitive *prim = NULL;
    unsigned prim_type = 0;
    FOR_ALL_PRIMITIVES(gdp, prim)
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
ROP_FBXNodeManager::findNodeInfo(KFbxNode* fbx_node)
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
ROP_FBXNodeManager::addNodePair(OP_Node* hd_node, KFbxNode* fbx_node, ROP_FBXMainNodeVisitInfo& visit_info)
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

    TStringSet::iterator si;
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

    myNamesSet.insert((const char*)strName); 
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
ROP_FBXNodeInfo::ROP_FBXNodeInfo(KFbxNode* main_node) : myVisitInfoCopy(NULL)
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
