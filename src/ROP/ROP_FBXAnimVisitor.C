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

#include "ROP_FBXHeaderWrapper.h"
#include "ROP_FBXAnimVisitor.h"
#include "ROP_FBXExporter.h"

#include <UT/UT_Interrupt.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_FloatArray.h>
#include <UT/UT_Thread.h>
#include <OP/OP_Node.h>
#include <OP/OP_Network.h>
#include <OP/OP_Director.h>
#include <OP/OP_Take.h>
#include <TAKE/TAKE_Take.h>
#include <PRM/PRM_Parm.h>
#include <CH/CH_Channel.h>
#include <CH/CH_Segment.h>
#include <SOP/SOP_Node.h>
#include <CH/CH_Expression.h>
#include <GA/GA_ElementWrangler.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_ConvertParms.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Vertex.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimRBezCurve.h>
#include <GU/GU_PrimRBezSurf.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include "ROP_FBXUtil.h"
#include "ROP_FBXActionManager.h"

#include "ROP_FBXUtil.h"
#ifdef UT_DEBUG
extern double ROP_FBXdb_vcacheExportTime;
#endif

using namespace std;

/********************************************************************************************************/
ROP_FBXAnimVisitor::ROP_FBXAnimVisitor(ROP_FBXExporter* parent_exporter) 
: ROP_FBXBaseVisitor(parent_exporter->getExportOptions()->getInvisibleNodeExportMethod(), parent_exporter->getStartTime())
{
    myAnimLayer = NULL;
    myParentExporter = parent_exporter;

    mySDKManager = myParentExporter->getSDKManager();
    myScene = myParentExporter->getFBXScene();
    myErrorManager = myParentExporter->getErrorManager();
    UT_ASSERT(myErrorManager);

    myNodeManager = myParentExporter->getNodeManager();
    myActionManager = myParentExporter->getActionManager();
    myExportOptions = myParentExporter->getExportOptions();
    myOutputFileName = myParentExporter->getOutputFileName();

    UT_String full_name(UT_String::ALWAYS_DEEP, myOutputFileName.c_str()), file_path(UT_String::ALWAYS_DEEP), file_name(UT_String::ALWAYS_DEEP);
    full_name.splitPath(file_path, file_name);	
    if(file_path.isstring())
	myFBXFileSourceFolder = file_path;
    else
	myFBXFileSourceFolder = "";

    if(file_name.isstring())
	myFBXShortFileName = file_name.pathUpToExtension();
    else
	myFBXShortFileName = full_name.pathUpToExtension();

    myBoss = myParentExporter->GetBoss();
}
/********************************************************************************************************/
ROP_FBXAnimVisitor::~ROP_FBXAnimVisitor()
{

}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::reset(FbxAnimLayer* curr_layer)
{
    myAnimLayer = curr_layer;
}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo* 
ROP_FBXAnimVisitor::visitBegin(OP_Node* node, int input_idx_on_this_node)
{
    return new ROP_FBXBaseNodeVisitInfo(node);
}
/********************************************************************************************************/
ROP_FBXVisitorResultType 
ROP_FBXAnimVisitor::visit(OP_Node* node, ROP_FBXBaseNodeVisitInfo* node_info_in)
{
    ROP_FBXVisitorResultType res_type = ROP_FBXVisitorResultOk;
    if(!node)
	return res_type;

    if(myBoss->opInterrupt())
	return ROP_FBXVisitorResultAbort;

    res_type = ROP_FBXVisitorResultSkipSubnet;

    // Find the related FBX node
    ROP_FBXNodeInfo* stored_node_info_ptr;
    TFbxNodeInfoVector fbx_nodes;

    myNodeManager->findNodeInfos(node, fbx_nodes);
    int curr_fbx_node, num_fbx_nodes = fbx_nodes.size();
    for(curr_fbx_node = 0; curr_fbx_node < num_fbx_nodes; curr_fbx_node++)
    {
	stored_node_info_ptr = fbx_nodes[curr_fbx_node];
	if(!stored_node_info_ptr || !stored_node_info_ptr->getFbxNode())
	    continue;
    /*
	{
	    // An object may validly be not exported (meaning this will be NULL).
	    return res_type;
	}
	*/

	if(stored_node_info_ptr)
	    res_type = stored_node_info_ptr->getVisitResultType();

	FbxNode *fbx_node = stored_node_info_ptr->getFbxNode();
	node_info_in->setMaxObjectPoints(stored_node_info_ptr->getMaxObjectPoints());
	node_info_in->setVertexCacheMethod(stored_node_info_ptr->getVertexCacheMethod());
	node_info_in->setIsSurfacesOnly(stored_node_info_ptr->getIsSurfacesOnly());
	node_info_in->setSourcePrimitive(stored_node_info_ptr->getSourcePrimitive());

/*
	// Don't need this anymore.
	// Create take nodes
	FbxTakeNode* curr_fbx_take;
	FbxTakeNode* fbx_attr_take_node = NULL;
	KFCurve* curr_fbx_curve;
	curr_fbx_take = addFBXTakeNode(fbx_node);

	if(!curr_fbx_take)
	    return res_type;
*/
	FbxAnimCurve* curr_anim_curve;
	UT_String node_type = node->getOperator()->getName();

	bool force_obj_transfrom_from_world = false;
	bool force_resampled_anim = false;
	bool account_for_pivot = hasPivotInfo(node);
	bool has_pretransform = false;
	OBJ_Node* obj_node = dynamic_cast<OBJ_Node*>(node);
	if(obj_node)
	{
	    // Check for pre-transform
	    UT_Matrix4 pretransform;
	    OP_Context op_context(myParentExporter->getStartTime());
	    obj_node->getPreLocalTransform(op_context, pretransform);
	    if(pretransform.isIdentity() == false)
		has_pretransform = true;

	    // In the case of a rivet, we have no parameter transforms
	    // that are animated, but instead we have an internal matrix
	    // that stores them. In this case, we force the retrieval of
	    // transformation channels by computing the local matrix 
	    // from the node's world matrix and the parent's inverse world
	    // matrix.
	    UT_String node_type(UT_String::ALWAYS_DEEP, "");
	    node_type = obj_node->getOperator()->getName();
	    if(node_type == "rivet")
	    {
		force_resampled_anim = true;
		force_obj_transfrom_from_world = true;
	    }
	}

	if(node_type == "bone" || account_for_pivot || has_pretransform || force_resampled_anim)
	{
	    // Bones are special, since we have to force-resample them and
	    // output all channels at the same time.

	    // Also, we have to put the bone animation onto the parent of the fbx node,
	    // since this FBX node corresponds to the end tip of the bone.
	    //FbxTakeNode* curr_fbx_bone_take = fbx_node->GetParent()->GetCurrentTakeNode();    
	    //exportResampledAnimation(curr_fbx_bone_take, node);
	    exportResampledAnimation(myAnimLayer, node, fbx_node, node_info_in, force_obj_transfrom_from_world);
	}
	else
	{

	    exportTRSAnimation(node, myAnimLayer, fbx_node);
	}

	if(node_type == "geo" || node_type == "instance")
	{
	    // For geometry, check if we have a dopimport SOP in the chain.
	    if(node_info_in->getMaxObjectPoints() > 0)
	    {
#ifdef UT_DEBUG
		double vc_start_time, vc_end_time;
		vc_start_time = clock();
#endif
		OP_Network* geo_net = dynamic_cast<OP_Network*>(node);
		OP_Node* vc_node;
		if(node_type == "instance")
		    vc_node = node;
		else
		    vc_node = geo_net->getRenderNodePtr();
		outputVertexCache(fbx_node, vc_node, myOutputFileName.c_str(), node_info_in, stored_node_info_ptr);
#ifdef UT_DEBUG
		vc_end_time = clock();
		ROP_FBXdb_vcacheExportTime += (vc_end_time - vc_start_time);
#endif
	    }
	}
	else if(node_type == "hlight")
	{
	    FbxLight *light_attrib = FbxCast<FbxLight>(fbx_node->GetNodeAttribute());

	    // Create curve nodes
	    if(light_attrib)
	    {
		// Output its colour, intensity, and cone angle channels
		curr_anim_curve = light_attrib->Intensity.GetCurve(myAnimLayer, NULL, true);
		exportChannel(curr_anim_curve, node, "light_intensity", 0, 100.0);

		curr_anim_curve = light_attrib->OuterAngle.GetCurve(myAnimLayer, NULL, true);
		exportChannel(curr_anim_curve, node, "coneangle", 0);

		curr_anim_curve = light_attrib->Color.GetCurve(myAnimLayer, FBXSDK_CURVENODE_COLOR_RED, true);
		exportChannel(curr_anim_curve, node, "light_color", 0);

		curr_anim_curve = light_attrib->Color.GetCurve(myAnimLayer, FBXSDK_CURVENODE_COLOR_GREEN, true);
		exportChannel(curr_anim_curve, node, "light_color", 1);

		curr_anim_curve = light_attrib->Color.GetCurve(myAnimLayer, FBXSDK_CURVENODE_COLOR_BLUE, true);
		exportChannel(curr_anim_curve, node, "light_color", 2);
	    }
	}
	else if(node_type == "cam")
	{
	    FbxCamera *cam_attrib = FbxCast<FbxCamera>(fbx_node->GetNodeAttribute());
///	    fbx_attr_take_node = addFBXTakeNode(cam_attrib);
//	    cam_attrib->FocalLength.GetKFCurveNode(true, fbx_attr_take_node->GetName());

	    curr_anim_curve = cam_attrib->FocalLength.GetCurve(myAnimLayer, NULL, true);
	    exportChannel(curr_anim_curve, node, "focal", 0);
	}


	// Export visibility channel
	curr_anim_curve = fbx_node->Visibility.GetCurve(myAnimLayer, NULL, true);
	exportChannel(curr_anim_curve, node, "vm_renderable", 0);

    } // end for over all fbx nodes

    return res_type;
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportTRSAnimation(OP_Node* node, FbxAnimLayer* curr_fbx_anim_layer, FbxNode* fbx_node)
{
    FbxAnimCurve* curr_anim_curve;

    if(!node || !curr_fbx_anim_layer)
	return;

    UT_String node_type = node->getOperator()->getName();
    string channel_prefix;
    if(node_type == "instance")
	channel_prefix = "i_";
    else if(node_type == "hlight")
	channel_prefix = "l_";

    // Create the curves
//     fbx_node->LclRotation.GetKFCurveNode(true, curr_fbx_take->GetName());
//     fbx_node->LclTranslation.GetKFCurveNode(true, curr_fbx_take->GetName());
//     fbx_node->LclScaling.GetKFCurveNode(true, curr_fbx_take->GetName());

    string channel_name;

    // Translations
    channel_name = channel_prefix + "t";
    curr_anim_curve = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 0);

    curr_anim_curve = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 1);

    curr_anim_curve = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 2);

    // Rotations
    channel_name = channel_prefix + "r";
    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 0);

    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 1);

    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 2);

    // Scaling
    channel_name = channel_prefix + "s";
    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 0);

    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 1);

    curr_anim_curve = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    exportChannel(curr_anim_curve, node, channel_name.c_str(), 2); 

}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info)
{
    // Nothing to do for now.
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportChannel(FbxAnimCurve* fbx_anim_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor)
{
    if(!fbx_anim_curve || !source_node || !parm_name)
	return;

    CH_Channel  *ch;
    PRM_Parm    *parm;

    // Get parameter.
    parm = &source_node->getParm(parm_name);
    if (!parm)
	return;

    bool use_override = false;
    bool force_resample = false;

    // See if we have any overrides
    if(parm->getIsOverrideActive(parm_idx))
    {
	force_resample = true;
	use_override = true;
    }

    ch = parm->getChannel(parm_idx);
    if(!force_resample && (!ch || ch->getLastSegment() == NULL))
	return;

    if(!force_resample && ch && ch->getLastSegment()->getLength() <= 0.0)
    {
	// It might be an expression. In this case, we force resampling.
	force_resample = ch->isTimeDependent();
	if(!force_resample)
	    return;
    }

    fpreal temp_float;
    fpreal start_frame;
    fpreal end_frame;
    UT_SuperIntervalR range;
    UT_FprealArray tmp_array;
    CH_Manager *ch_manager = CHgetManager();

    fpreal start_time = myParentExporter->getStartTime();
    fpreal end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);

    if(myExportOptions->getResampleAllAnimation() || force_resample)
    {
	// Do the entire time range
	tmp_array.entries(2);
	tmp_array[0] = ch_manager->getTime(start_frame);
	tmp_array[1] = ch_manager->getTime(end_frame);
    }
    else
    {
	CHbuildRange( start_frame, end_frame, range );
	ch->getKeyTimes(range, tmp_array, false);

	if(!ch->isAtHardKeyframe(start_frame))
	{
	    // We're not starting at a key frame. Add start time to the array.
	    tmp_array.insert(ch_manager->getTime(start_frame), 0);
	}
	if(!ch->isAtHardKeyframe(end_frame))
	{
	    // We're not ending at a key frame. Add end time to the array.
	    tmp_array.append(ch_manager->getTime(end_frame));
	}
    }

    fbx_anim_curve->KeyModifyBegin();

    fpreal secs_per_sample = 1.0/ch_manager->getSamplesPerSec();
    if(myExportOptions->getResampleAllAnimation() || force_resample)
    {
	PRM_Parm* temp_parm_ptr = NULL;
	if(use_override)
	    temp_parm_ptr = parm;
	outputResampled(fbx_anim_curve, ch, 0, tmp_array.entries()-1, tmp_array, false, temp_parm_ptr, parm_idx);
    }
    else
    {
	bool found_untied_keys = false;

	int fbx_key_idx;
	fpreal key_time;
	FbxTime fbx_time;
	CH_Segment* next_seg;
	UT_String str_expression(UT_String::ALWAYS_DEEP);
	int curr_frame, num_frames = tmp_array.entries();
	double key_val, db_val;
	int thread = SYSgetSTID();

	for(curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    key_time = tmp_array[curr_frame];
	    fbx_time.SetSecondDouble(key_time+secs_per_sample);
	    fbx_key_idx = fbx_anim_curve->KeyAdd(fbx_time);
	}

	int c_index = 0;
	for(curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    // Convert frame to time
	    CH_FullKey full_key;
	    key_time = tmp_array[curr_frame];
	    ch->getFullKey(key_time, full_key);

	    fbx_time.SetSecondDouble(key_time+secs_per_sample);
	    fbx_key_idx = fbx_anim_curve->KeyFind(fbx_time, &c_index);

	    if( (full_key.k[0].myVValid[CH_VALUE] && full_key.k[1].myVValid[CH_VALUE]) )
	    {
		if( SYSisEqual( full_key.k[0].myV[CH_VALUE], full_key.k[1].myV[CH_VALUE]) )
		    key_val = full_key.k[0].myV[CH_VALUE];
		else
		{
		    found_untied_keys = true;
		    key_val = (full_key.k[0].myV[CH_VALUE] + full_key.k[1].myV[CH_VALUE])*0.5;
		}
	    }
	    else if(full_key.k[0].myVValid[CH_VALUE])
		key_val = full_key.k[0].myV[CH_VALUE];
	    else if(full_key.k[1].myVValid[CH_VALUE])
		key_val = full_key.k[1].myV[CH_VALUE];
	    else
	    {
		parm->getValue(key_time, temp_float, parm_idx, thread);
		key_val = temp_float;
	    }

	    key_val *= scale_factor;

	    // Doh! Segments can be expressions, as well.
	    // We'll support some basic types, and then resample the rest.
	    fbx_anim_curve->KeySetValue(fbx_key_idx, key_val);

	    // Look at the next segment type
	    next_seg = ch->getSegmentAfterKey(key_time);
	    if(next_seg)
	    { 
		str_expression = next_seg->getCHExpr()->getExpression();
		if(str_expression == "bezier()" || str_expression == "cubic()")
		{
		    fbx_anim_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationCubic);
		    if(full_key.k[0].myVTied[CH_SLOPE] || full_key.k[1].myVTied[CH_SLOPE])
			fbx_anim_curve->KeySetTangentMode(fbx_key_idx, FbxAnimCurveDef::eTangentUser);
		    else
			fbx_anim_curve->KeySetTangentMode(fbx_key_idx, FbxAnimCurveDef::eTangentGenericBreak);

		    FbxAnimCurveTangentInfo r_info, l_info;

		    // Set the slopes
		    if(full_key.k[0].myVValid[CH_SLOPE])
		    {
			db_val = full_key.k[0].myV[CH_SLOPE]*scale_factor;
    			l_info.mDerivative = db_val;
		    }
		    if(full_key.k[1].myVValid[CH_SLOPE])
		    {
			db_val = full_key.k[1].myV[CH_SLOPE]*scale_factor;
			r_info.mDerivative = db_val;
		    }

		    if(full_key.k[0].myVValid[CH_ACCEL])
		    {
			l_info.mWeighted = true;
			l_info.mWeight = full_key.k[0].myV[CH_ACCEL];
		    }
		    else
			l_info.mWeighted = false;

		    if(full_key.k[1].myVValid[CH_ACCEL])
		    {
			r_info.mWeighted = true;
			r_info.mWeight = full_key.k[1].myV[CH_ACCEL];
		    }
		    else
			r_info.mWeighted = false;

		    fbx_anim_curve->KeySetLeftDerivativeInfo(fbx_key_idx, l_info);
		    fbx_anim_curve->KeySetRightDerivativeInfo(fbx_key_idx, r_info);
		}
		else
		{
		    // Try to look for the word linear in it
		    if(str_expression == "linear()" || str_expression == "qlinear()")
		    {
			// Linear segment
			fbx_anim_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
		    }
		    else if(str_expression == "constant()")
		    {
			// Constant segment
			fbx_anim_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationConstant);
			fbx_anim_curve->KeySetConstantMode(fbx_key_idx, FbxAnimCurveDef::eConstantStandard);
		    }
		    else if(str_expression == "vmatchout()" || str_expression == "matchout()")
		    {
			// Constant segment
			fbx_anim_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationConstant);
			fbx_anim_curve->KeySetConstantMode(fbx_key_idx, FbxAnimCurveDef::eConstantNext);
		    }
		    else
		    {
			// Unsupported segment. Treat as linear and resample. 
			fbx_anim_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);

			int next_frame_idx = curr_frame + 1;
			if(next_frame_idx >= num_frames)
			    next_frame_idx = curr_frame;
			outputResampled(fbx_anim_curve, ch, curr_frame, next_frame_idx, tmp_array, true, parm, parm_idx);
		    }
		}
    	
	    }

	}

	if(found_untied_keys)
	    myErrorManager->addError("Untied key values encountered. This is not supported. Node: ", source_node->getName(), NULL, false );
    }
    fbx_anim_curve->KeyModifyEnd();
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::outputResampled(FbxAnimCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_FprealArray& time_array, bool do_insert, PRM_Parm* direct_eval_parm, int parm_idx)
{
    UT_ASSERT(start_array_idx <= end_array_idx);
    if(end_array_idx < start_array_idx)
	return;

    CH_Manager *ch_manager = CHgetManager();
    int thread = SYSgetSTID();
    int curr_idx;
    fpreal key_time;

    fpreal secs_per_sample = 1.0/ch_manager->getSamplesPerSec();    
    fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
    fpreal curr_time, end_time = 0.0;
    int end_idx;
    bool is_last;
    int fbx_key_idx;
    FbxTime fbx_time;
    CH_Segment *next_seg;
    int opt_idx;
    fpreal key_val = 0;
    fpreal s;
    curr_time = 0;
    for(curr_idx = start_array_idx; curr_idx < end_array_idx; curr_idx++)
    {
	// Insert keyframes, evaluate the values at the channel
	key_time = time_array[curr_idx];
	end_idx = curr_idx + 1;
	if(end_idx > end_array_idx)
	    end_idx = end_array_idx;
	end_time = time_array[end_idx];

	if(ch)
	    next_seg = ch->getSegmentAfterKey(key_time);
	else
	    next_seg = NULL;

	if((ch && next_seg) || (direct_eval_parm && parm_idx >= 0))
	{
	    opt_idx = 0;
	    is_last = false;
	    for(curr_time = key_time; curr_time < end_time; curr_time += time_step)
	    {
		if(direct_eval_parm && parm_idx >= 0)
		    direct_eval_parm->getValue(curr_time, key_val, parm_idx,
					       thread);
		else if(ch && next_seg)
		    ch->sampleValueSlope(next_seg, curr_time, thread, key_val, s);		    

		fbx_time.SetSecondDouble(curr_time+secs_per_sample);
		if(do_insert)
		    fbx_key_idx = fbx_curve->KeyInsert(fbx_time, &opt_idx);
		else
		    fbx_key_idx = fbx_curve->KeyAdd(fbx_time, &opt_idx);
		fbx_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
		fbx_curve->KeySetValue(fbx_key_idx, key_val);
	    }
	}
    }

    if(start_array_idx < end_array_idx && curr_time >= end_time)
    {
	CH_FullKey full_key;

	// We skipped the last frame. Add it now.
	end_time = time_array[end_array_idx];
	if(direct_eval_parm && parm_idx >= 0)
	{
	    direct_eval_parm->getValue(end_time, key_val, parm_idx, thread);
	    full_key.k[0].myVValid[CH_VALUE] = true;
	    full_key.k[1].myVValid[CH_VALUE] = false;
	    full_key.k[0].myV[CH_VALUE] = key_val;
	    full_key.k[1].myV[CH_VALUE] = key_val;
	}
	else if(ch)
	    ch->getFullKey(end_time, full_key);

	fbx_time.SetSecondDouble(end_time+secs_per_sample);
	if(do_insert)
	    fbx_key_idx = fbx_curve->KeyInsert(fbx_time);
	else
	    fbx_key_idx = fbx_curve->KeyAdd(fbx_time);
	fbx_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);

	if(full_key.k[0].myVValid[CH_VALUE])
	    fbx_curve->KeySetValue(fbx_key_idx, full_key.k[0].myV[CH_VALUE]);
	else if(full_key.k[1].myVValid[CH_VALUE])
	    fbx_curve->KeySetValue(fbx_key_idx, full_key.k[1].myV[CH_VALUE]);
	else
	{
	    UT_ASSERT(0);
	}

    }

}
/********************************************************************************************************/
FbxVertexCacheDeformer* 
ROP_FBXAnimVisitor::addedVertexCacheDeformerToNode(FbxNode* fbx_node, const char* file_name)
{
    if(!fbx_node || !fbx_node->GetGeometry())
	return NULL;

    // By convention, all cache files are created in a .fpc folder located at the same
    // place as the .fbx file.     
    string cache_file_name(myFBXFileSourceFolder);
    if(myFBXFileSourceFolder.length() > 0)
	cache_file_name += "/";
    cache_file_name += myFBXShortFileName;
    cache_file_name += ".fpc";

    ::mkdir(cache_file_name.c_str(), 0777);

    string rel_pc_name("");;
    string absolute_pc_name(cache_file_name);
    absolute_pc_name += "/";
    absolute_pc_name += fbx_node->GetName();

    rel_pc_name += myFBXShortFileName;
    rel_pc_name += ".fpc/";
    rel_pc_name += fbx_node->GetName();
    if(myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
    {
	absolute_pc_name += ".xml";
	rel_pc_name += ".xml";
    }
    else
    {
	absolute_pc_name +=  ".pc2";
	rel_pc_name +=  ".pc2";
    }

    // Create the cache file
    FbxCache* v_cache = FbxCache::Create(mySDKManager, fbx_node->GetName());

    v_cache->SetCacheFileName(rel_pc_name.c_str(), absolute_pc_name.c_str());
    if(myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
	v_cache->SetCacheFileFormat(FbxCache::eMayaCache);
    else
	v_cache->SetCacheFileFormat(FbxCache::eMaxPointCacheV2);

    // Create the vertex deformer
    FbxVertexCacheDeformer* deformer = FbxVertexCacheDeformer::Create(mySDKManager, fbx_node->GetName());

    deformer->SetCache(v_cache);
    deformer->SetCacheChannel(fbx_node->GetName());
    deformer->SetActive(true);

    // Apply the deformer on the mesh
    fbx_node->GetGeometry()->AddDeformer(deformer);

    return deformer;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::outputVertexCache(FbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info)
{
//    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(geo_node);
//    if(!sop_node)
//	return false;

    FbxVertexCacheDeformer* vc_deformer = addedVertexCacheDeformerToNode(fbx_node, file_name);

    if(!vc_deformer)
    {
	myErrorManager->addError("Cannot create the vertex cache deformer. Node: ",geo_node->getName(), NULL,  false );
	return false;
    }

    CH_Manager *ch_manager = CHgetManager();
    fpreal start_frame, end_frame;
    fpreal start_time = myParentExporter->getStartTime();
    fpreal end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);
    fpreal curr_fps = ch_manager->getSamplesPerSec();

    // Now add data to the deformer
    FbxCache*               v_cache = vc_deformer->GetCache();
    bool res;

    FbxTime fbx_curr_time;
    fpreal hd_time;
    int curr_frame;

    unsigned int frame_count = end_frame - start_frame + 1;

    int num_vc_points = node_info_in->getMaxObjectPoints();

    if(node_info_in->getIsSurfacesOnly() && node_pair_info->getVertexCacheMethod() == ROP_FBXVertexCacheMethodGeometryConstant)
    {
	// We've got to have the exact number of vertices in the cache, or else FBX SDK will refuse to read invalid caches back in.
	int temp_pts = lookupExactPointCount(geo_node, ch_manager->getTime(start_frame), node_pair_info->getSourcePrimitive());
	if(temp_pts > 0)
	    num_vc_points = temp_pts;
    }

    // Open the file for writing
    FbxStatus status;
    if (myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
    {
	// NOTE: FbxCache::eMCC is the old cache file format (32-bit).
	//       FbxCache::eMCX is the new Maya 2014 cache file format (64-bit)
	res = v_cache->OpenFileForWrite(
		FbxCache::eMCOneFile,
		curr_fps, fbx_node->GetName(),
		FbxCache::eMCC,
		FbxCache::eDoubleVectorArray,
		"Points",
		&status);
    }
    else
    {
	res = v_cache->OpenFileForWrite(
		start_frame, curr_fps, frame_count, num_vc_points, &status);
    }

    if (!res)
    {
	myErrorManager->addError("Cannot open the vertex cache file. Error message: ", status.GetErrorString(), NULL,  false );
	return false;
    }

    int channel_index = v_cache->GetChannelIndex(fbx_node->GetName());

    // Allocate our buffer array
    double *vert_coords = new double[num_vc_points*3];
    bool bWriteRes;

    // Output the points. Remember that when outputting this mesh, the points were reversed.
    for(curr_frame = start_frame; curr_frame <= end_frame; curr_frame++)
    {
	hd_time = ch_manager->getTime(curr_frame);
	fbx_curr_time.SetTime(0,0,0, curr_frame);

	if(!fillVertexArray(geo_node, hd_time, node_info_in, vert_coords, num_vc_points, node_pair_info, curr_frame))
	{
	    myErrorManager->addError("Could not evaluate a frame of vertex cache array. Node: ", geo_node->getName(), NULL, false);
	    continue;
	}

	if (myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
	{
	    bWriteRes = v_cache->Write(channel_index, fbx_curr_time, vert_coords, num_vc_points);
	}
	else
	{
	    bWriteRes = v_cache->Write(curr_frame - start_frame, vert_coords);
	}
    }

    if (!v_cache->CloseFile(&status))
    {
	myErrorManager->addError("Cannot close the vertex cache file. Error message: ", status.GetErrorString(), NULL, false);
    }	

    delete[] vert_coords;
    return true;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::fillVertexArray(OP_Node* node, fpreal time, ROP_FBXBaseNodeVisitInfo* node_info_in, double* vert_array, 
				    int num_array_points, ROP_FBXNodeInfo* node_pair_info, fpreal frame_num)
{
    ROP_FBXVertexCacheMethodType vc_method = node_pair_info->getVertexCacheMethod();
    const GU_Detail *final_gdp = NULL;

    GU_Detail conv_gdp;

    // If the object does not change the number of points in an animation,
    // we need its GDP converted, but not triangulated and not broken up (which is 
    // what the stored, cached GDP is).
    if(vc_method == ROP_FBXVertexCacheMethodGeometryConstant || node_pair_info->getVertexCache()->getSaveMemory())
    {
	// Get at the gdp
	GU_DetailHandle gdh;
	SOP_Node* sop_node = dynamic_cast<SOP_Node*>(node);
	OBJ_Node* obj_node = dynamic_cast<OBJ_Node*>(node);
	OP_Context context(time);

	if(sop_node)
	    ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh);
	else
	    gdh = obj_node->getDisplayGeometryHandle(context);

	if(gdh.isNull())
	    return false;

	GU_DetailHandleAutoReadLock	 gdl(gdh);
	const GU_Detail *gdp;
	gdp = gdl.getGdp();
	if(!gdp)
	    return false;

	int dummy_int;

	if(vc_method == ROP_FBXVertexCacheMethodGeometryConstant)
	{
	    if(node_info_in->getIsSurfacesOnly())
		conv_gdp.duplicate(*gdp);
	    else
		ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(gdp, myParentExporter->getExportOptions()->getPolyConvertLOD(), false, conv_gdp, dummy_int);
	}
	else
	{
	    // Re-do the geometry
	    GA_PrimCompat::TypeMask prim_type = ROP_FBXUtil::getGdpPrimId(gdp);
	    if(prim_type == GEO_PrimTypeCompat::GEOPRIMPART)
		ROP_FBXUtil::convertParticleGDPtoPolyGDP(gdp, conv_gdp);
	    else
		ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(gdp, myParentExporter->getExportOptions()->getPolyConvertLOD(), true, conv_gdp, dummy_int);
	}
	final_gdp = &conv_gdp;
    }
    else
        final_gdp = node_pair_info->getVertexCache()->getFrameGeometry(frame_num);

    if(!final_gdp)
    {
	UT_ASSERT(0);
	return false;
    }


    int actual_gdp_points = final_gdp->getNumPoints();
    int curr_point;
    UT_Vector4 ut_vec;
    int arr_offset;

    if(node_info_in->getIsSurfacesOnly())
    {
	// The order of points is different for surfaces
	const GU_PrimNURBSurf* hd_nurb;
	const GU_PrimNURBCurve* hd_nurb_curve;
	GU_PrimRBezCurve* hd_bez_curve;
	GU_PrimRBezSurf* hd_bez;
	const GEO_Primitive* prim;
	int i_row, i_col, i_idx, i_curr_vert;

	memset(vert_array, 0, sizeof(double)*3*num_array_points);

	int curr_prim_cnt;
	int source_prim_cnt = node_pair_info->getSourcePrimitive();
	i_curr_vert = 0;
	curr_prim_cnt = -1;
	GA_FOR_MASK_PRIMITIVES(final_gdp, prim, GEO_PrimTypeCompat::GEOPRIMNURBSURF)
	{
	    curr_prim_cnt++;
	    if(source_prim_cnt >= 0 && source_prim_cnt != curr_prim_cnt)
		continue;

	    hd_nurb = dynamic_cast<const GU_PrimNURBSurf*>(prim);
	    if(!hd_nurb)
		continue;

	    int v_point_count = hd_nurb->getNumRows();
	    int u_point_count = hd_nurb->getNumCols();

	    for(i_row = 0; i_row < u_point_count; i_row++)
	    {
		for(i_col = 0; i_col < v_point_count; i_col++)
		{
		    i_idx = i_col*u_point_count + i_row;
		    if(i_idx < actual_gdp_points)	
			ut_vec = prim->getVertexElement(i_idx).getPos();
		    else
			ut_vec = 0;

		    arr_offset = i_curr_vert*3;
		    vert_array[arr_offset] = ut_vec.x();
		    vert_array[arr_offset+1] = ut_vec.y();
		    vert_array[arr_offset+2] = ut_vec.z();
		    i_curr_vert++;
		}
	    }
	} // end over NURBS surfaces

	GA_ElementWranglerCache	 wranglers(*const_cast<GU_Detail *>(final_gdp),
					   GA_PointWrangler::EXCLUDE_P);

	GA_FOR_MASK_PRIMITIVES(final_gdp, prim, GEO_PrimTypeCompat::GEOPRIMBEZSURF)
	{
	    curr_prim_cnt++;
	    if(source_prim_cnt >= 0 && source_prim_cnt != curr_prim_cnt)
		continue;

	    hd_bez = const_cast<GU_PrimRBezSurf*>(dynamic_cast<const GU_PrimRBezSurf*>(prim));
	    if(!hd_bez)
		continue;

	    hd_nurb = dynamic_cast<GU_PrimNURBSurf*>(hd_bez->convertToNURBNew(
								    wranglers));
	    if(!hd_nurb)
		continue;

	    int v_point_count = hd_nurb->getNumRows();
	    int u_point_count = hd_nurb->getNumCols();

	    for(i_row = 0; i_row < u_point_count; i_row++)
	    {
		for(i_col = 0; i_col < v_point_count; i_col++)
		{
		    i_idx = i_col*u_point_count + i_row;
		    if(i_idx < actual_gdp_points)	
			ut_vec = prim->getVertexElement(i_idx).getPos();
		    else
			ut_vec = 0;

		    arr_offset = i_curr_vert*3;
		    vert_array[arr_offset] = ut_vec.x();
		    vert_array[arr_offset+1] = ut_vec.y();
		    vert_array[arr_offset+2] = ut_vec.z();
		    i_curr_vert++;
		}
	    }
	} // end over Bezier surfaces

	GA_FOR_MASK_PRIMITIVES(final_gdp, prim, GEO_PrimTypeCompat::GEOPRIMBEZCURVE)
	{
	    curr_prim_cnt++;
	    if(source_prim_cnt >= 0 && source_prim_cnt != curr_prim_cnt)
		continue;

	    hd_bez_curve = const_cast<GU_PrimRBezCurve*>(dynamic_cast<const GU_PrimRBezCurve*>(prim));
	    if(!hd_bez_curve)
		continue;

	    hd_nurb_curve = dynamic_cast<GU_PrimNURBCurve*>(hd_bez_curve->convertToNURBNew(wranglers));

	    int u_point_count = hd_nurb_curve->getVertexCount();
	    for(i_idx = 0; i_idx < u_point_count; i_idx++)
	    {
		if(i_idx < actual_gdp_points)	
		    ut_vec = prim->getVertexElement(i_idx).getPos();
		else
		    ut_vec = 0;

		arr_offset = i_curr_vert*3;
		vert_array[arr_offset] = ut_vec.x();
		vert_array[arr_offset+1] = ut_vec.y();
		vert_array[arr_offset+2] = ut_vec.z();
		i_curr_vert++;
	    }
	} // end over Bezier curves

	GA_FOR_MASK_PRIMITIVES(final_gdp, prim, GEO_PrimTypeCompat::GEOPRIMNURBCURVE)
	{
	    curr_prim_cnt++;
	    if(source_prim_cnt >= 0 && source_prim_cnt != curr_prim_cnt)
		continue;

	    hd_nurb_curve = dynamic_cast<const GU_PrimNURBCurve*>(prim);
	    if(!hd_nurb_curve)
		continue;
    
	    int u_point_count = hd_nurb_curve->getVertexCount();
	    for(i_idx = 0; i_idx < u_point_count; i_idx++)
	    {
		if(i_idx < actual_gdp_points)	
		    ut_vec = prim->getVertexElement(i_idx).getPos();
		else
		    ut_vec = 0;

		arr_offset = i_curr_vert*3;
		vert_array[arr_offset] = ut_vec.x();
		vert_array[arr_offset+1] = ut_vec.y();
		vert_array[arr_offset+2] = ut_vec.z();
		i_curr_vert++;
	    }
	} // end over NURBS curves


    }
    else
    {
	for(curr_point = 0; curr_point < num_array_points; curr_point++)
	{
	    if(curr_point < actual_gdp_points)
		ut_vec = final_gdp->getPos4(final_gdp->pointOffset(curr_point));
	    else
		ut_vec = 0;

	    arr_offset = curr_point*3;
	    vert_array[arr_offset] = ut_vec.x();
	    vert_array[arr_offset+1] = ut_vec.y();
	    vert_array[arr_offset+2] = ut_vec.z();
	}
    }

    return true;
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportResampledAnimation(FbxAnimLayer* curr_fbx_anim_layer, OP_Node* source_node, 
					     FbxNode* fbx_node, ROP_FBXBaseNodeVisitInfo *node_info, 
					     bool force_obj_transfrom_from_world)
{
    // Get channels, range, and make sure we have any animation at all
    int curr_trs_channel;
    const int num_trs_channels = 3;
    const int num_channels = 10;
    int curr_channel_idx;
    const char* const channel_names[] = { "t", "r", "scale", "length", "pathobjpath", "roll", "pathorient", "up", "bank", "pos"};
    CH_Manager *ch_manager = CHgetManager();
    fpreal start_frame, end_frame;
    fpreal gb_start_time = myParentExporter->getStartTime();
    fpreal gb_end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(gb_start_time);
    end_frame = ch_manager->getSample(gb_end_time);

    bool uses_overrides = false;
    bool force_resample = false;
    fpreal start_time = SYS_FPREAL_MAX, end_time = -SYS_FPREAL_MAX;

    CH_Channel  *ch;
    PRM_Parm    *parm;

    // Get parameter.
    for(curr_channel_idx = 0; curr_channel_idx < num_channels; curr_channel_idx++)
    {
	parm = &source_node->getParm(channel_names[curr_channel_idx]);
	if(!parm)
	    continue;

	for(curr_trs_channel = 0; curr_trs_channel < num_trs_channels; curr_trs_channel++)
	{
	    if(parm->getIsOverrideActive(curr_trs_channel))
	    {
		uses_overrides = true;
		force_resample = true;
	    }

	    ch = parm->getChannel(curr_trs_channel);
	    if(!ch)
		continue;
	    if(ch->getStart() < start_time) start_time = ch->getStart();
	    if(ch->getEnd() > end_time) end_time = ch->getEnd();

	    if(SYSequalZero(ch->getLength()))
	    {
		// This might be an expression.
		force_resample |= ch->isTimeDependent();
	    }
	}
    }

    if(force_obj_transfrom_from_world)
	force_resample = true;

    // No animation.
    if((start_time == SYS_FPREAL_MAX || start_time >= end_time) && !force_resample)
	return;

    if(force_resample)
    {
	// Output the entire range
	start_time = ch_manager->getTime(start_frame);
	end_time = ch_manager->getTime(end_frame);
    }

    // Create the curves
//     fbx_node->LclRotation.GetKFCurveNode(true, curr_fbx_take->GetName());
//     fbx_node->LclTranslation.GetKFCurveNode(true, curr_fbx_take->GetName());
//     fbx_node->LclScaling.GetKFCurveNode(true, curr_fbx_take->GetName());

    // Get fbx curves
    FbxAnimCurve* fbx_t[num_trs_channels];
    FbxAnimCurve* fbx_r[num_trs_channels];
    FbxAnimCurve* fbx_s[num_trs_channels];
    fbx_t[0] = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    fbx_t[1] = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    fbx_t[2] = fbx_node->LclTranslation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);

    fbx_r[0] = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    fbx_r[1] = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    fbx_r[2] = fbx_node->LclRotation.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    
    fbx_s[0] = fbx_node->LclScaling.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
    fbx_s[1] = fbx_node->LclScaling.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    fbx_s[2] = fbx_node->LclScaling.GetCurve(curr_fbx_anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);

    int t_opt_idx[3];
    int r_opt_idx[3];
    int s_opt_idx[3];
    for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
    {
	fbx_t[curr_channel_idx]->KeyModifyBegin();
	fbx_r[curr_channel_idx]->KeyModifyBegin();
	fbx_s[curr_channel_idx]->KeyModifyBegin();

	t_opt_idx[curr_channel_idx] = 0;
	r_opt_idx[curr_channel_idx] = 0;
	s_opt_idx[curr_channel_idx] = 0;
    }

    double secs_per_sample = 1.0/(double)ch_manager->getSamplesPerSec();
    double time_step = secs_per_sample * (double)myExportOptions->getResampleIntervalInFrames();
    double curr_time;
    FbxTime fbx_time;
    int fbx_key_idx;
    fpreal bone_length;
    UT_Vector3D t_out, r_out, s_out;

    OP_Node* parent_node;
    int prev_fbx_frame_idx;
    UT_Vector3D prev_frame_rot, *prev_frame_rot_ptr = NULL;

    // Walk the time, compute the final transform matrix at each time, and break it.
    for(curr_time = start_time; curr_time < end_time; curr_time += time_step)
    {
	bone_length = 0;
	parent_node = source_node->getInput(source_node->getConnectedInputIndex(-1));
	if(parent_node)
	    bone_length = ROP_FBXUtil::getFloatOPParm(parent_node, "length", 0, curr_time);
	//bone_length = ROP_FBXUtil::getFloatOPParm(source_node, "length", 0, curr_time);
	ROP_FBXUtil::getFinalTransforms(source_node, node_info, false, bone_length, curr_time, NULL, t_out, r_out, s_out, NULL, prev_frame_rot_ptr, force_obj_transfrom_from_world);
	prev_frame_rot_ptr = &prev_frame_rot;
	prev_frame_rot = r_out;

	fbx_time.SetSecondDouble(curr_time+secs_per_sample);
	for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
	{
	    // Note that we can't use KeyInsert() here because sometimes (but not always) it returns
	    // the same key index for different key times, essentially causing us to overwrite frames.
	    // KeyAdd() does not do this.
	    fbx_key_idx = fbx_t[curr_channel_idx]->KeyAdd(fbx_time, &t_opt_idx[curr_channel_idx]);
	    fbx_t[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_t[curr_channel_idx]->KeySetValue(fbx_key_idx, t_out[curr_channel_idx]);
	    if(curr_channel_idx == 0)
	    {
		if(curr_time > start_time)
		{
		    UT_ASSERT_MSG(prev_fbx_frame_idx != fbx_key_idx, "Frame overwriting occurred.");
		}
		prev_fbx_frame_idx = fbx_key_idx;
	    }

	    fbx_key_idx = fbx_r[curr_channel_idx]->KeyAdd(fbx_time, &r_opt_idx[curr_channel_idx]);
	    fbx_r[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_r[curr_channel_idx]->KeySetValue(fbx_key_idx, r_out[curr_channel_idx]);

	    fbx_key_idx = fbx_s[curr_channel_idx]->KeyAdd(fbx_time, &s_opt_idx[curr_channel_idx]);
	    fbx_s[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_s[curr_channel_idx]->KeySetValue(fbx_key_idx, s_out[curr_channel_idx]);
	}
    }


    if(curr_time >= end_time)
    {
	bone_length = 0;
	parent_node = source_node->getInput(source_node->getConnectedInputIndex(-1));
	if(parent_node)
	    bone_length = ROP_FBXUtil::getFloatOPParm(parent_node, "length", 0, curr_time);
	//bone_length = ROP_FBXUtil::getFloatOPParm(source_node, "length", 0, end_time);
	ROP_FBXUtil::getFinalTransforms(source_node, node_info, false, bone_length, end_time, NULL, t_out, r_out, s_out, NULL, prev_frame_rot_ptr, force_obj_transfrom_from_world);

	fbx_time.SetSecondDouble(end_time+secs_per_sample);
	for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
	{
	    fbx_key_idx = fbx_t[curr_channel_idx]->KeyAdd(fbx_time, &t_opt_idx[curr_channel_idx]);
	    fbx_t[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_t[curr_channel_idx]->KeySetValue(fbx_key_idx, t_out[curr_channel_idx]);

	    fbx_key_idx = fbx_r[curr_channel_idx]->KeyAdd(fbx_time, &r_opt_idx[curr_channel_idx]);
	    fbx_r[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_r[curr_channel_idx]->KeySetValue(fbx_key_idx, r_out[curr_channel_idx]);

	    fbx_key_idx = fbx_s[curr_channel_idx]->KeyAdd(fbx_time, &s_opt_idx[curr_channel_idx]);
	    fbx_s[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
	    fbx_s[curr_channel_idx]->KeySetValue(fbx_key_idx, s_out[curr_channel_idx]);
	}
    }

    for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
    {
	fbx_t[curr_channel_idx]->KeyModifyEnd();
	fbx_r[curr_channel_idx]->KeyModifyEnd();
	fbx_s[curr_channel_idx]->KeyModifyEnd();
    }
}
/********************************************************************************************************/
int 
ROP_FBXAnimVisitor::lookupExactPointCount(OP_Node *node, fpreal time, int selected_prim_idx)
{
    // Get at the gdp
    GU_DetailHandle gdh;
    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(node);
    OP_Context context(time);

    if(!sop_node)
	return -1;
    ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh);

    if(gdh.isNull())
	return -1;

    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail *gdp;
    gdp = gdl.getGdp();
    if(!gdp)
	return -1;

    GU_Detail conv_gdp;
    conv_gdp.duplicate(*gdp);

    const GU_PrimNURBSurf* hd_nurb;
    const GU_PrimNURBCurve* hd_nurb_curve;
    GU_PrimRBezCurve* hd_bez_curve;
    const GEO_Primitive* prim;
    int curr_prim_cnt = -1;
    GA_FOR_MASK_PRIMITIVES(&conv_gdp, prim, GEO_PrimTypeCompat::GEOPRIMNURBSURF)
    {
	curr_prim_cnt++;
	if(selected_prim_idx >= 0 && selected_prim_idx != curr_prim_cnt)
	    continue;

	hd_nurb = dynamic_cast<const GU_PrimNURBSurf*>(prim);
	if(!hd_nurb)
	    return -1;

	int v_point_count = hd_nurb->getNumRows();
	int u_point_count = hd_nurb->getNumCols();
	return v_point_count * u_point_count;
    }

    GA_ElementWranglerCache	 wranglers(conv_gdp,
					   GA_PointWrangler::EXCLUDE_P);

    GA_FOR_MASK_PRIMITIVES(&conv_gdp, prim, GEO_PrimTypeCompat::GEOPRIMBEZSURF)
    {
	curr_prim_cnt++;
	if(selected_prim_idx >= 0 && selected_prim_idx != curr_prim_cnt)
	    continue;

	hd_bez_curve = const_cast<GU_PrimRBezCurve*>(dynamic_cast<const GU_PrimRBezCurve*>(prim));
	if(!hd_bez_curve)
	    continue;

	hd_nurb_curve = dynamic_cast<GU_PrimNURBCurve*>(hd_bez_curve->convertToNURBNew(wranglers));
	return hd_nurb_curve->getVertexCount();
    }

    GA_FOR_MASK_PRIMITIVES(&conv_gdp, prim, GEO_PrimTypeCompat::GEOPRIMBEZCURVE)
    {
	curr_prim_cnt++;
	if(selected_prim_idx >= 0 && selected_prim_idx != curr_prim_cnt)
	    continue;

	hd_bez_curve = const_cast<GU_PrimRBezCurve*>(dynamic_cast<const GU_PrimRBezCurve*>(prim));
	if(!hd_bez_curve)
	    continue;

	hd_nurb_curve = dynamic_cast<GU_PrimNURBCurve*>(hd_bez_curve->convertToNURBNew(wranglers));

	return hd_nurb_curve->getVertexCount();
    }

    GA_FOR_MASK_PRIMITIVES(&conv_gdp, prim, GEO_PrimTypeCompat::GEOPRIMNURBCURVE)
    {
	curr_prim_cnt++;
	if(selected_prim_idx >= 0 && selected_prim_idx != curr_prim_cnt)
	    continue;

	hd_nurb_curve = dynamic_cast<const GU_PrimNURBCurve*>(prim);
	if(!hd_nurb_curve)
	    continue;

	return hd_nurb_curve->getVertexCount();
    }

    return -1;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::hasPivotInfo(OP_Node* node)
{
    fpreal px = ROP_FBXUtil::getFloatOPParm(node, "p", 0, myParentExporter->getStartTime());
    fpreal py = ROP_FBXUtil::getFloatOPParm(node, "p", 1, myParentExporter->getStartTime());
    fpreal pz = ROP_FBXUtil::getFloatOPParm(node, "p", 2, myParentExporter->getStartTime());

    if(!SYSequalZero(px) || !SYSequalZero(py) || !SYSequalZero(pz))
	return true;

    // Check if we have any channels on pivots
    CH_Channel  *ch;
    PRM_Parm    *parm;
    int count;

    // Get parameter.
    parm = &node->getParm("p");
    if (!parm)
	return false;

    for(count = 0; count < 3; count++) 
    {
	ch = parm->getChannel(count);
	if(ch && ch->getLastSegment() != NULL && ch->getLastSegment()->getLength() > 0.0)
	    return true;
    }

    return false;
}
/********************************************************************************************************/
// ROP_FBXAnimNodeVisitInfo
/********************************************************************************************************/
ROP_FBXAnimNodeVisitInfo::ROP_FBXAnimNodeVisitInfo(OP_Node* hd_node) : ROP_FBXBaseNodeVisitInfo(hd_node)
{

}
/********************************************************************************************************/
ROP_FBXAnimNodeVisitInfo::~ROP_FBXAnimNodeVisitInfo()
{

}
/********************************************************************************************************/
