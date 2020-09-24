/*
 * Copyright (c) 2020
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
#include "ROP_FBXAnimVisitor.h"

#include "ROP_FBXActionManager.h"
#include "ROP_FBXCommon.h"
#include "ROP_FBXExporter.h"
#include "ROP_FBXUtil.h"

#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include <GU/GU_ConvertParms.h>
#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimRBezCurve.h>
#include <GU/GU_PrimRBezSurf.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GA/GA_ElementWrangler.h>

#include <OP/OP_Director.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_Take.h>
#include <PRM/PRM_Parm.h>
#include <CH/CH_Channel.h>
#include <CH/CH_Expression.h>
#include <CH/CH_Manager.h>
#include <CH/CH_Segment.h>

#include <TAKE/TAKE_Take.h>
#include <UT/UT_ArrayStringMap.h>
#include <UT/UT_CrackMatrix.h>
#include <UT/UT_FloatArray.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_Thread.h>
#include <UT/UT_UniquePtr.h>
#include <UT/UT_WorkBuffer.h>
#include <SYS/SYS_Inline.h>
#include <SYS/SYS_Math.h>
#include <SYS/SYS_SequentialThreadIndex.h>
#include <SYS/SYS_StaticAssert.h>
#include <SYS/SYS_TypeTraits.h>


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
static inline fpreal
fbxHoudiniTime(const FbxTime& fbx_time)
{
    return fbx_time.GetSecondDouble() - (1.0 / CHgetManager()->getSamplesPerSec());
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

    OBJ_Node* obj_node = node->castToOBJNode();

    // Find the related FBX node
    ROP_FBXNodeInfo* stored_node_info_ptr;
    TFbxNodeInfoVector fbx_nodes;

    ROP_FBXExportOptions& options = *myParentExporter->getExportOptions();
    bool is_sop_export = options.isSopExport();
    if (is_sop_export || (obj_node && obj_node->getObjectType() == OBJ_GEOMETRY))
    {
        UT_StringRef path_attrib_name = options.getSopExportPathAttrib();
        if (path_attrib_name)
        {
            exportPackedPrimAnimation(node, path_attrib_name, myAnimLayer);

            // Reset obj_node so that we go through the SOP code path
            obj_node = nullptr;
        }
    }

    myNodeManager->findNodeInfos(node, fbx_nodes);
    int curr_fbx_node, num_fbx_nodes = fbx_nodes.size();
    for(curr_fbx_node = 0; curr_fbx_node < num_fbx_nodes; curr_fbx_node++)
    {
	stored_node_info_ptr = fbx_nodes[curr_fbx_node];
	if(!stored_node_info_ptr || !stored_node_info_ptr->getFbxNode())
	    continue;
	res_type = stored_node_info_ptr->getVisitResultType();

	const fpreal t = myParentExporter->getStartTime();

        OBJ_Camera *cam = nullptr;
	UT_StringRef node_type = node->getOperator()->getName();
	if (is_sop_export)
        {
	    node_type = "geo";
        }
        else
        {
            // Note that cam might be a switcher or representative object and
            // can be different from node/obj_node
            cam = ROPfbxCastToCamera(node, t);
        }

	FbxNode *fbx_node = stored_node_info_ptr->getFbxNode();
	node_info_in->setMaxObjectPoints(stored_node_info_ptr->getMaxObjectPoints());
	node_info_in->setVertexCacheMethod(stored_node_info_ptr->getVertexCacheMethod());
	node_info_in->setIsSurfacesOnly(stored_node_info_ptr->getIsSurfacesOnly());
	node_info_in->setSourcePrimitive(stored_node_info_ptr->getSourcePrimitive());
	for(int curr_blend_index = 0; curr_blend_index < stored_node_info_ptr->getBlendShapeNodeCount(); curr_blend_index++)
	    node_info_in->addBlendShapeNode(stored_node_info_ptr->getBlendShapeNodeAt(curr_blend_index));

	// We skip exporting the object-level transforms for SOPs
        if (obj_node)
        {
            if (ROP_FBXUtil::mapsToFBXTransform(t, obj_node))
                exportTRSAnimation(obj_node, myAnimLayer, fbx_node);
            else
                exportResampledAnimation(myAnimLayer, obj_node, fbx_node,
                                         node_info_in);
        }

	FbxAnimCurve* curr_anim_curve;

	if(node_type == "geo" || node_type == "instance")
	{
	    // For geometry, check if we have a dopimport SOP in the chain...
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
		    vc_node = is_sop_export ? node : geo_net->getRenderNodePtr();
		outputVertexCache(fbx_node, vc_node, myOutputFileName.c_str(), node_info_in, stored_node_info_ptr);
#ifdef UT_DEBUG
		vc_end_time = clock();
		ROP_FBXdb_vcacheExportTime += (vc_end_time - vc_start_time);
#endif
	    }

	    // ... or if we have blend shapes
	    for (int curr_blend_node_index = 0; curr_blend_node_index < node_info_in->getBlendShapeNodeCount(); curr_blend_node_index++)
	    {
		OP_Node* curr_blend_node = node_info_in->getBlendShapeNodeAt(curr_blend_node_index);
		if (!curr_blend_node)
		    continue;

		if (exportBlendShapeAnimation(curr_blend_node, fbx_node))
		    continue;

		TFbxNodeInfoVector blend_fbx_nodes;
		myNodeManager->findNodeInfos(curr_blend_node, blend_fbx_nodes);
		for (int info_index = 0; info_index < blend_fbx_nodes.size(); info_index++)
		    exportBlendShapeAnimation(curr_blend_node, blend_fbx_nodes[info_index]->getFbxNode());
	    }
	}
	else if(ROPfbxIsLightNodeType(node_type))
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
	else if(cam != nullptr)
	{
            // Note that node != cam in general when node is a switcher or a
            // subnet that has cam as its representative object.
            // Because we're not looping here for all time, animated camera
            // parameters are not supported with switcher cameras,
            // we'll only use the camera at the start time.

	    FbxCamera *cam_attrib = FbxCast<FbxCamera>(fbx_node->GetNodeAttribute());
	    if (cam_attrib)
	    {
///		fbx_attr_take_node = addFBXTakeNode(cam_attrib);
//		cam_attrib->FocalLength.GetKFCurveNode(true, fbx_attr_take_node->GetName());

		curr_anim_curve = cam_attrib->FocalLength.GetCurve(myAnimLayer, NULL, true);
		exportChannel(curr_anim_curve, cam, "focal", 0);
	    }
	}

	// Export visibility channel
	if (obj_node && obj_node->isDisplayTimeDependent())
	{
	    // The viewport doesn't support tdisplay being animated
	    curr_anim_curve = fbx_node->Visibility.GetCurve(myAnimLayer, NULL, true);
	    exportChannel(curr_anim_curve, node, "display", 0);
	    curr_anim_curve->KeyModifyBegin();
	    for (int k = 0, nk = curr_anim_curve->KeyGetCount(); k < nk; ++k)
	    {
		fpreal key_time = fbxHoudiniTime(curr_anim_curve->KeyGetTime(k));
		curr_anim_curve->KeySetValue(k, obj_node->getObjectDisplay(key_time) ? 1.0f : 0.0f);
	    }
	    curr_anim_curve->KeyModifyEnd();
	}

    } // end for over all fbx nodes

    return res_type;
}
/********************************************************************************************************/
static SYS_FORCE_INLINE void
ropSetFbxTime(FbxTime& fbx_time, fpreal hd_time_seconds)
{
    fbx_time.SetSecondDouble(hd_time_seconds);
    fbx_time += FbxTime::GetOneFrameValue();
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportTRSAnimation(
        OBJ_Node* node,
        FbxAnimLayer* fbx_anim_layer,
        FbxNode* fbx_node)
{
    if(!node || !fbx_anim_layer)
	return;

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
    SYS_STATIC_ASSERT(SYSarraySize(hd_names) == SYSarraySize(props));

    const int NUM_COMPONENTS = 3;
    const char* components[NUM_COMPONENTS] =
    {
	FBXSDK_CURVENODE_COMPONENT_X,
	FBXSDK_CURVENODE_COMPONENT_Y,
	FBXSDK_CURVENODE_COMPONENT_Z
    };

    UT_StringRef node_type = node->getOperator()->getName();
    const char *UNIFORM_SCALE = "scale";
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

    const PRM_ParmList *plist = node->getParmList();

    // We need to output resampled scales if we have at uniform scale value
    // other than 1.0 and we have either uniform scale or regular scales
    // animated.
    int skip_i = -1;
    const PRM_Parm *scale_parm = plist->getParmPtr(hd_names[ROP_FBX_S]);
    const PRM_Parm *uniform_scale_parm = plist->getParmPtr(UNIFORM_SCALE);
    if (uniform_scale_parm
	&& (uniform_scale_parm->isTimeDependent() || (scale_parm && scale_parm->isTimeDependent())))
    {
	skip_i = ROP_FBX_S;

	int curve_last[NUM_COMPONENTS] = { 0, 0, 0 };
	FbxAnimCurve* curves[NUM_COMPONENTS];
	for (int c = 0; c < NUM_COMPONENTS; ++c)
	{
	    curves[c] = fbx_node->LclScaling.GetCurve(fbx_anim_layer, components[c], true);
	    curves[c]->KeyModifyBegin();
	}

	const int thread = SYSgetSTID();
	const fpreal secs_per_sample = 1.0/CHgetManager()->getSamplesPerSec();
	const fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
	const fpreal beg_time = myParentExporter->getStartTime();
	const fpreal end_time = myParentExporter->getEndTime();
	for (fpreal key_time = beg_time; key_time < end_time; key_time += time_step)
	{
	    UT_Vector3 scale;
	    scale_parm->getValues(key_time, scale.data(), thread);
	    fpreal uniform_scale;
	    uniform_scale_parm->getValue(key_time, uniform_scale, 0, thread);
	    scale *= uniform_scale;

	    FbxTime fbx_time;
	    ropSetFbxTime(fbx_time, key_time);
	    for (int c = 0; c < NUM_COMPONENTS; ++c)
	    {
		FbxAnimCurve* curve = curves[c];
		int key_i = curve->KeyAdd(fbx_time, &curve_last[c]);
		curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);
		curve->KeySetValue(key_i, scale(c));
	    }
	}

	for (auto& curve : curves)
	    curve->KeyModifyEnd();
    }

    for (int i = 0; i < ROP_FBX_N; ++i)
    {
	if (i == skip_i)
	    continue;

	const char *parm_name = hd_names[i];
	const PRM_Parm *parm = plist->getParmPtr(parm_name);
	if (!parm)
	    continue;

	for (int c = 0; c < NUM_COMPONENTS; ++c)
	{
	    FbxAnimCurve* curve = props[i]->GetCurve(fbx_anim_layer, components[c], true);
	    if (!curve) // can fail if not animatible (eg. pre/post rotation)
		continue;
	    exportChannel(curve, node, parm_name, c);
	}
    }

    // For lights/cameras, they have a look at axis that is different from Houdini/Maya that use the -Z
    // axis. If there's an animated post rotation, we multiply in a adjustment so that we go from
    // Houdini's -Z to either the +X (for cameras) or -Y (for lights). This requires that we resample the
    // animation for correctness.
    //
    // NOTE: This code is all moot right now because FBX doesn't allow us to create animated post rotation
    // curves in the first place. So the caller has to go through the resampled output code path for now
    // in order to preserve animation.
#if 0
    FbxVector4 rotate_adjust;
    if (ROP_FBXUtil::getPostRotateAdjust(node_type, rotate_adjust))
    {
	const PRM_Parm *post_rotate_parm = plist->getParmPtr(hd_names[ROP_FBX_RPOST]);
	if (post_rotate_parm && post_rotate_parm->isTimeDependent())
	{
	    int curve_last[NUM_COMPONENTS] = { 0, 0, 0 };
	    FbxAnimCurve* curves[NUM_COMPONENTS];
	    for (int c = 0; c < NUM_COMPONENTS; ++c)
	    {
		curves[c] = fbx_node->PostRotation.GetCurve(fbx_anim_layer, components[c], true);
		curves[c]->KeyModifyBegin();
	    }

	    const int thread = SYSgetSTID();
	    const fpreal secs_per_sample = 1.0/CHgetManager()->getSamplesPerSec();
	    const fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
	    const fpreal beg_time = myParentExporter->getStartTime();
	    const fpreal end_time = myParentExporter->getEndTime();
	    for (fpreal key_time = beg_time; key_time < end_time; key_time += time_step)
	    {
		UT_Vector3 post_rot;
		post_rotate_parm->getValues(key_time, post_rot.data(), thread);
		FbxVector4 post_rotate(post_rot(0), post_rot(1), post_rot(2));
		ROP_FBXUtil::doPostRotateAdjust(post_rotate, rotate_adjust);

		FbxTime fbx_time;
	        ropSetFbxTime(fbx_time, key_time);
		for (int c = 0; c < NUM_COMPONENTS; ++c)
		{
		    FbxAnimCurve* curve = curves[c];
		    int key_i = curve->KeyAdd(fbx_time, &curve_last[c]);
		    curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);
		    curve->KeySetValue(key_i, post_rotate[c]);
		}
	    }

	    for (auto& curve : curves)
		curve->KeyModifyEnd();
	}
    }
#endif
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info)
{
    // Nothing to do for now.
}
/********************************************************************************************************/
static inline void
ropOutputLinearKeysGeneric(
        FbxAnimCurve* fbx_curve, int& key_hint, double scale_factor,
        CH_Channel& ch, fpreal& t, fpreal end_time, fpreal time_step, int thread)
{
    const fpreal tol = ch.getTolerance();
    FbxTime fbx_time;
    for ( ; SYSisLessOrEqual(t, end_time, tol); t += time_step)
    {
        ropSetFbxTime(fbx_time, t);
        int key_i = fbx_curve->KeyAdd(fbx_time, &key_hint);
        fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);

        fpreal key_val = ch.evaluate(t, /*no_disabling*/false, thread);
        fbx_curve->KeySetValue(key_i, key_val * scale_factor);
    }
}
/********************************************************************************************************/
static inline void
ropOutputBasicKeysOutside(
        FbxAnimCurve* fbx_curve, int& key_hint, double scale_factor,
        CH_Channel& ch, fpreal& t, fpreal end_time,
        fpreal time_step, int thread, bool detect_constant)
{
    CH_Segment* seg;
    const fpreal tol = ch.getTolerance();
    if (SYSisLess(t, ch.getStart(), tol))
    {
        if (ch.getChannelLeftType() != CH_CHANNEL_EXTEND)
        {
            ropOutputLinearKeysGeneric(fbx_curve, key_hint, scale_factor, ch, t, end_time,
                                       time_step, thread);
            return;
        }
        // else fall through to evaluating using seg
        seg = ch.getFirstSegment();
    }
    else if (SYSisGreaterOrEqual(t, ch.getEnd(), tol))
    {
        if (ch.getChannelRightType() != CH_CHANNEL_EXTEND)
        {
            ropOutputLinearKeysGeneric(fbx_curve, key_hint, scale_factor, ch, t, end_time,
                                       time_step, thread);
            return;
        }
        // else fall through to evaluating using seg
        seg = ch.getSegmentAfterKey(ch.getEnd());
        UT_ASSERT(seg);
    }
    else
    {
        UT_ASSERT(!"This function can only be used for evaluating outside the channel range");
        return;
    }

    // Fast path for evaluating the segment past the channel ends
    // NOTE: seg->isTimeDependent() might return true even when not strictly necessary but it doesn't
    //       harm because we just end up outputting more keys in that case.
    FbxTime fbx_time;
    if (detect_constant && !seg->isTimeDependent())
    {
        // Set key at time start time t
        {
            ropSetFbxTime(fbx_time, t);
            int key_i = fbx_curve->KeyAdd(fbx_time, &key_hint);
            fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationConstant);
            fbx_curve->KeySetConstantMode(key_i, FbxAnimCurveDef::eConstantStandard);

            fpreal key_val = ch.evaluateSegment(seg, ch.localTime(t), /*extend*/true, thread);
            fbx_curve->KeySetValue(key_i, key_val * scale_factor);
        }
        // Set key at end time
        t = end_time;
        {
            ropSetFbxTime(fbx_time, t);
            int key_i = fbx_curve->KeyAdd(fbx_time, &key_hint);
            fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);

            fpreal key_val = ch.evaluateSegment(seg, ch.localTime(t), /*extend*/true, thread);
            fbx_curve->KeySetValue(key_i, key_val * scale_factor);
        }
        t += time_step;
    }
    else
    {
        for ( ; SYSisLessOrEqual(t, end_time, tol); t += time_step)
        {
            ropSetFbxTime(fbx_time, t);
            int key_i = fbx_curve->KeyAdd(fbx_time, &key_hint);
            fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);

            fpreal key_val = ch.evaluateSegment(seg, ch.localTime(t), /*extend*/true, thread);
            fbx_curve->KeySetValue(key_i, key_val * scale_factor);
        }
    }
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportChannel(FbxAnimCurve* fbx_anim_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor, const int& param_inst)
{
    if(!fbx_anim_curve || !source_node || !parm_name)
	return;

    CH_Channel  *ch;
    PRM_Parm    *parm;    

    if (param_inst < 0)
    {
	// Get parameter.
	//PRM_Name* parm_name = new PRM_Name(parm_name);
	parm = source_node->getParmList()->getParmPtr(parm_name);
    }	
    else
    {
	// Get the instanced parameter
	parm = source_node->getParmList()->getParmPtrInst(parm_name, &param_inst, 1);
    }
	

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

    if (!force_resample && ch && !ch->isAllEnabled())
        force_resample = true;

    CH_Manager *ch_manager = CHgetManager();
    const fpreal start_time = myParentExporter->getStartTime();
    const fpreal end_time = myParentExporter->getEndTime();
    const fpreal start_frame = ch_manager->getSample(start_time);
    const fpreal end_frame = ch_manager->getSample(end_time);

    fbx_anim_curve->KeyModifyBegin();

    const bool resample_all = myExportOptions->getResampleAllAnimation();
    if (resample_all || force_resample)
    {
        if (use_override && parm && parm_idx >= 0)
        {
            // Do the entire time range
            UT_FprealArray tmp_array;
            tmp_array.setSizeNoInit(2);
            tmp_array(0) = start_time;
            tmp_array(1) = end_time;
            outputResampled(fbx_anim_curve, ch, 0, tmp_array.size()-1, tmp_array, false, parm, parm_idx,
                            scale_factor);
        }
        else if (ch)
        {
            outputBasicKeys(fbx_anim_curve, *ch, start_time, end_time, scale_factor, !resample_all);
        }
    }
    else
    {
        UT_ASSERT(ch);

        fpreal secs_per_sample = ch_manager->getSecsPerSample();
	int thread = SYSgetSTID();

        UT_SuperIntervalR range;
        UT_FprealArray tmp_array;
	CHbuildRange(start_frame, end_frame, range);
	ch->getKeyTimes(range, tmp_array, false);

	int c_index = 0;
	if(!ch->isAtHardKeyframe(start_frame))
	{
	    // We're not starting at a key frame. Resample from start_time to channel start
            fpreal t = start_time;
            fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
	    ropOutputBasicKeysOutside(fbx_anim_curve, c_index, scale_factor, *ch, t, ch->getStart(),
                                      time_step, thread, !resample_all);
	}
	FbxTime fbx_time;
	exint num_frames = tmp_array.size();
	for (exint curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    fpreal key_time = tmp_array(curr_frame);
            ropSetFbxTime(fbx_time, key_time);
	    (void) fbx_anim_curve->KeyAdd(fbx_time, &c_index);
	}
	if(!ch->isAtHardKeyframe(end_frame))
	{
	    // We're not ending at a key frame. Resample from channel end to end_time
            fpreal t = ch->getEnd();
            fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
	    ropOutputBasicKeysOutside(fbx_anim_curve, c_index, scale_factor, *ch, t, end_time,
                                      time_step, thread, !resample_all);
	}

        // Reset key index hint back to 0 just in case because we're starting from the beginning again
        c_index = 0;

	bool found_untied_keys = false;
	for(exint curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    // Convert frame to time
	    CH_FullKey full_key;
	    fpreal key_time = tmp_array(curr_frame);
	    ch->getFullKey(key_time, full_key, 
		/*reverse=*/false,
		/*accel_ratios=*/true, 
		CH_GETKEY_EXTEND_DEFAULT);

            ropSetFbxTime(fbx_time, key_time);
	    int fbx_key_idx = fbx_anim_curve->KeyFind(fbx_time, &c_index);

            fpreal key_val;
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
		parm->getValue(key_time, key_val, parm_idx, thread);
	    key_val *= scale_factor;

	    // Doh! Segments can be expressions, as well.
	    // We'll support some basic types, and then resample the rest.
	    fbx_anim_curve->KeySetValue(fbx_key_idx, key_val);

	    // Look at the next segment type
	    CH_Segment* next_seg = ch->getSegmentAfterKey(key_time);
	    if(next_seg)
	    { 
	        UT_String str_expression = next_seg->getCHExpr()->getExpression();
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
    			l_info.mDerivative = full_key.k[0].myV[CH_SLOPE]*scale_factor;
		    }
		    if(full_key.k[1].myVValid[CH_SLOPE])
		    {
			r_info.mDerivative = full_key.k[1].myV[CH_SLOPE]*scale_factor;
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
			outputResampled(fbx_anim_curve, ch, curr_frame, next_frame_idx, tmp_array, true, parm, parm_idx, scale_factor);
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
ROP_FBXAnimVisitor::outputResampled(FbxAnimCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_FprealArray& time_array, bool do_insert, PRM_Parm* direct_eval_parm, int parm_idx, double scale_factor)
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
	key_time = time_array(curr_idx);
	end_idx = curr_idx + 1;
	if(end_idx > end_array_idx)
	    end_idx = end_array_idx;
	end_time = time_array(end_idx);

	if(ch)
	    next_seg = ch->getSegmentAfterKey(key_time);
	else
	    next_seg = NULL;

	if((ch && next_seg) || (direct_eval_parm && parm_idx >= 0))
	{
	    opt_idx = 0;
	    for(curr_time = key_time; curr_time < end_time; curr_time += time_step)
	    {
		if(direct_eval_parm && parm_idx >= 0)
		    direct_eval_parm->getValue(curr_time, key_val, parm_idx,
					       thread);
		else if(ch && next_seg)
		    ch->sampleValueSlope(next_seg, curr_time, thread, key_val, s);		    

                ropSetFbxTime(fbx_time, curr_time);
		if(do_insert)
		    fbx_key_idx = fbx_curve->KeyInsert(fbx_time, &opt_idx);
		else
		    fbx_key_idx = fbx_curve->KeyAdd(fbx_time, &opt_idx);

		key_val *= scale_factor;

		fbx_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);
		fbx_curve->KeySetValue(fbx_key_idx, key_val);
	    }
	}
    }

    if(start_array_idx < end_array_idx && curr_time >= end_time)
    {
	bool valid = true;

	// We skipped the last frame. Add it now.
	end_time = time_array(end_array_idx);
	if(direct_eval_parm && parm_idx >= 0)
	{
	    direct_eval_parm->getValue(end_time, key_val, parm_idx, thread);
	    key_val *= scale_factor;
	}
	else if(ch)
	{
	    next_seg = ch->getSegmentAfterKey(end_time);
	    ch->sampleValueSlope(next_seg, end_time, thread, key_val, s);
	    key_val *= scale_factor;
	}
	else
	{
	    valid = false;
	    UT_ASSERT(0);
	}

        ropSetFbxTime(fbx_time, end_time);
	if(do_insert)
	    fbx_key_idx = fbx_curve->KeyInsert(fbx_time);
	else
	    fbx_key_idx = fbx_curve->KeyAdd(fbx_time);
	fbx_curve->KeySetInterpolation(fbx_key_idx, FbxAnimCurveDef::eInterpolationLinear);

	if(valid)
	    fbx_curve->KeySetValue(fbx_key_idx, key_val);

    }

}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::outputBasicKeys(
        FbxAnimCurve* fbx_curve, CH_Channel &ch, fpreal start_time, fpreal end_time,
        double scale_factor, bool detect_constant)
{
    CH_Manager &ch_mgr = *CHgetManager();

    const int thread = SYSgetSTID();
    const fpreal secs_per_sample = ch_mgr.getSecsPerSample();
    const fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();

    int last_key = 0;

    // Slow code path if channel has any disabled segments
    if (!ch.isAllEnabled())
    {
        fpreal t = start_time;
        ropOutputLinearKeysGeneric(fbx_curve, last_key, scale_factor, ch, t, end_time, time_step, thread);
        return;
    }

    //
    // Else, split evaluation into 3 portions to avoid binary searching
    //

    fpreal t = start_time;

    // Evaluate portion before start of channel, to handle end conditions
    const fpreal channel_start = ch.getStart();
    ropOutputBasicKeysOutside(fbx_curve, last_key, scale_factor, ch, t, channel_start, time_step, thread,
                              detect_constant);
    // Evaluate portion within the channel by directly traversing segments
    FbxTime fbx_time;
    const unsigned seg_n = ch.getNSegments() - 1; // -1 to exclude end segment
    for (unsigned seg_i = 0; t < end_time && seg_i < seg_n; ++seg_i)
    {
        CH_Segment& seg = *ch.getSegment(seg_i);
        // Avoid resampling if we can
        if (detect_constant && !seg.isTimeDependent())
        {
            ropSetFbxTime(fbx_time, t);
            int key_i = fbx_curve->KeyAdd(fbx_time, &last_key);
            fbx_curve->KeySetValue(key_i, seg.getInValue() * scale_factor);
            fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationConstant);
            fbx_curve->KeySetConstantMode(key_i, FbxAnimCurveDef::eConstantStandard);

            t = SYSmin(ch.globalTime(seg.getEnd()), end_time);
            continue;
        }
        // Else, resample entire segment
        for (fpreal seg_end = ch.globalTime(seg.getEnd()); t < end_time && t < seg_end; t += time_step)
        {
            ropSetFbxTime(fbx_time, t);
            int key_i = fbx_curve->KeyAdd(fbx_time, &last_key);
            fpreal key_val = ch.evaluateSegment(seg, ch.localTime(t), /*extend*/false, thread);
            fbx_curve->KeySetValue(key_i, key_val * scale_factor);
            fbx_curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);
        }
    }
    // Evaluate portion after the channel, to handle end conditions
    ropOutputBasicKeysOutside(fbx_curve, last_key, scale_factor, ch, t, end_time, time_step, thread,
                              detect_constant);
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

    string rel_pc_name("");
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
#if FBXSDK_VERSION_MAJOR >= 2015
    deformer->Channel.Set(fbx_node->GetName());
    deformer->Active.Set(true);
#else
    deformer->SetCacheChannel(fbx_node->GetName());
    deformer->SetActive(true);
#endif

    // Apply the deformer on the mesh
    fbx_node->GetGeometry()->AddDeformer(deformer);

    return deformer;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::outputVertexCache(FbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info)
{
    if ( !geo_node )
	return false;

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
	    (void) v_cache->Write(channel_index, fbx_curr_time, vert_coords, num_vc_points);
	}
	else
	{
	    (void) v_cache->Write(curr_frame - start_frame, vert_coords);
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
                ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(
                        gdp, myParentExporter->getExportOptions()->getPolyConvertLOD(), false, conv_gdp,
                        dummy_int);
	}
	else
	{
	    // Re-do the geometry
	    GA_PrimCompat::TypeMask prim_type = ROP_FBXUtil::getGdpPrimId(gdp);
	    if(prim_type == GEO_PrimTypeCompat::GEOPRIMPART)
		ROP_FBXUtil::convertParticleGDPtoPolyGDP(gdp, conv_gdp);
	    else
                ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(
                        gdp, myParentExporter->getExportOptions()->getPolyConvertLOD(), true, conv_gdp,
                        dummy_int);
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
    UT_Vector3 ut_vec;
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
			ut_vec = prim->getPos3(i_idx);
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
			ut_vec = prim->getPos3(i_idx);
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
		    ut_vec = prim->getPos3(i_idx);
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
		    ut_vec = prim->getPos3(i_idx);
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
		ut_vec = final_gdp->getPos3(final_gdp->pointOffset(curr_point));
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
ROP_FBXAnimVisitor::exportResampledAnimation(
        FbxAnimLayer* curr_fbx_anim_layer,
        OBJ_Node* source_node, 
        FbxNode* fbx_node,
        ROP_FBXBaseNodeVisitInfo *node_info)
{
    // Skip if it's not time dependent by cooking it. If there's errors, then just bail.
    // This check can cause more nodes to be resampled because don't we know if
    // the local transform changes due to different input transforms. So even
    // if none of the cooking parms on the node are time dependent, the output
    // transform might still vary across frames.
    OP_Context context(myParentExporter->getStartTime());
    if (!source_node
            || !source_node->cook(context)
            || !source_node->isTimeDependent(context))
	return;

    // Output the entire range
    fpreal start_time = myParentExporter->getStartTime();
    fpreal end_time = myParentExporter->getEndTime();

    // Maintain the rotation order of source_node. This assumes that
    // ROP_FBXUtil::setStandardTransforms() has already set the
    // transform order in an identical fashion.
    UT_XformOrder xform_order(UT_XformOrder::SRT, UT_XformOrder::XYZ);

    xform_order.rotOrder(OP_Node::getRotOrder(source_node->XYZ(start_time)));

    // Get fbx curves
    const int num_trs_channels = 3;
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
    for (int curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
    {
	fbx_t[curr_channel_idx]->KeyModifyBegin();
	fbx_r[curr_channel_idx]->KeyModifyBegin();
	fbx_s[curr_channel_idx]->KeyModifyBegin();

	t_opt_idx[curr_channel_idx] = 0;
	r_opt_idx[curr_channel_idx] = 0;
	s_opt_idx[curr_channel_idx] = 0;
    }

    double secs_per_sample = 1.0/(double)CHgetManager()->getSamplesPerSec();
    double time_step = secs_per_sample * (double)myExportOptions->getResampleIntervalInFrames();
    double curr_time;
    FbxTime fbx_time;
    int fbx_key_idx;
    UT_Vector3D t_out, r_out, s_out;

    UT_IF_ASSERT(int prev_fbx_frame_idx;)
    UT_Vector3D prev_frame_rot, *prev_frame_rot_ptr = NULL;

    // Walk the time, compute the final transform matrix at each time, and break it.
    for(curr_time = start_time; curr_time < end_time; curr_time += time_step)
    {
	ROP_FBXUtil::getFinalTransforms(source_node, node_info, 0.0, curr_time, xform_order, t_out, r_out, s_out, prev_frame_rot_ptr);
	prev_frame_rot_ptr = &prev_frame_rot;
	prev_frame_rot = r_out;

        ropSetFbxTime(fbx_time, curr_time);

	for(int curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
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
		UT_IF_ASSERT(prev_fbx_frame_idx = fbx_key_idx;)
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
	ROP_FBXUtil::getFinalTransforms(source_node, node_info, 0.0, end_time, xform_order, t_out, r_out, s_out, prev_frame_rot_ptr);

        ropSetFbxTime(fbx_time, end_time);
	for(int curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
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

    for(int curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
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
ROP_FBXAnimVisitor::exportBlendShapeAnimation(OP_Node* blend_shape_node, FbxNode* fbx_node)
{
    if ( !blend_shape_node || !fbx_node)
	return false;

    if ( !blend_shape_node->getOperator()->getName().contains("blendshapes") )
	return false;

    FbxGeometry* fbx_geo = fbx_node->GetGeometry();
    if ( !fbx_geo )
	return false;

    int num_deformer = fbx_geo->GetDeformerCount();
    for ( int current_deformer = 0; current_deformer < num_deformer; current_deformer++ )
    {
	// We only handle blend_shape deformer for now.
	FbxBlendShape* fbx_blend = FbxCast<FbxBlendShape>(fbx_geo->GetDeformer(current_deformer));
	if (!fbx_blend)
	    continue;

	// export all the blendshape channel's animation curves.
	int num_blendshape_channel = fbx_blend->GetBlendShapeChannelCount();
	for ( int n = 0; n < num_blendshape_channel; n++ )
	{
	    FbxAnimCurve* curr_anim_curve = fbx_blend->GetBlendShapeChannel(n)->DeformPercent.GetCurve(myAnimLayer, NULL, true);

	    // FBX uses percent for its BS values, so we need to use a scale factor here.
	    exportChannel(curr_anim_curve, blend_shape_node, "blend#", 0, 100.0, (n + 1));
	}
    }

    return true;
}
/********************************************************************************************************/
static inline bool
ropHasLocalTransform(const GA_Detail& geo, GA_PrimitiveTypeId type)
{
    const GA_PrimitiveDefinition* defn = geo.getPrimitiveFactory().lookupDefinition(type);
    UT_ASSERT(defn);
    return defn->hasLocalTransform();
}
/********************************************************************************************************/
bool
ROP_FBXAnimVisitor::exportPackedPrimAnimation(
        OP_Node* node, const UT_StringRef& path_attrib_name, FbxAnimLayer* fbx_anim_layer)
{
    OBJ_Node* obj_node = node->castToOBJNode();
    SOP_Node* sop_node = obj_node ? obj_node->getRenderSopPtr() : node->castToSOPNode();
    if (!sop_node)
        return false;

    constexpr const char* components[] =
    {
        FBXSDK_CURVENODE_COMPONENT_X,
        FBXSDK_CURVENODE_COMPONENT_Y,
        FBXSDK_CURVENODE_COMPONENT_Z
    };

    struct PathInfo
    {
        UT_Vector3D     myOrigin{0.0};
        UT_Vector3D     myPrevRot{0.0}; // in radians
        // These next two arrays are indexed in order of S, R, T
        FbxAnimCurve*   myCurves[9];
        int             myLastKeys[9] = { 0 };
        bool            myHasPrimTransform = false;

        void addTransformKey(const FbxTime& fbx_time, const UT_Matrix4D& xform)
        {
            UT_Vector3D vec[3]; // S, R, T order
            UT_XformOrder order(UT_XformOrder::SRT, UT_XformOrder::XYZ);
            // Note that FBX does not support shears as of writing
            xform.explode(order, /*r*/ vec[1], /*s*/ vec[0], /*t*/ vec[2]);

            UT_Vector3D& curr_rot = vec[1];
            UT_VERIFY(UTcrackMatrixSmooth(order, curr_rot(0), curr_rot(1), curr_rot(2),
                                          myPrevRot(0), myPrevRot(1), myPrevRot(2)));
            myPrevRot = curr_rot;

            curr_rot.radToDeg();

            int c = 0;
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j, ++c)
                {
                    FbxAnimCurve* curve = myCurves[c];
                    int key_i = curve->KeyAdd(fbx_time, &myLastKeys[c]);
                    curve->KeySetInterpolation(key_i, FbxAnimCurveDef::eInterpolationLinear);
                    curve->KeySetValue(key_i, vec[i](j));
                }
            }
        }
    };
    UT_ArrayStringMap<UT_UniquePtr<PathInfo>> info_from_path;
    TFbxNodeInfoVector node_infos;
    myNodeManager->findNodeInfos(node, node_infos);
    UT_Array<PathInfo*> obj_infos;
    for (auto&& info : node_infos)
    {
        UT_UniquePtr<PathInfo>& path_info = info_from_path[info->getPathValue()];
        path_info = UTmakeUnique<PathInfo>();

        FbxNode *fbx_node = info->getFbxNode();

        // Use default XYZ rotation order, with normal parent transform inheritance
        fbx_node->SetRotationActive(false);
        fbx_node->SetTransformationInheritType(FbxTransform::eInheritRSrs);

        // We had stashed the origin in the node's destination pivot in
        // ROP_FBXMainVisitor::outputShapePrimitives()
        path_info->myOrigin.assign(fbx_node->GetRotationPivot(FbxNode::eDestinationPivot).Buffer());

        using FbxDouble3Property = FbxPropertyT<FbxDouble3>;
        FbxDouble3Property* props[] =
        {
            &(fbx_node->LclScaling),
            &(fbx_node->LclRotation),
            &(fbx_node->LclTranslation)
        };
        int c = 0;
        for (FbxDouble3Property* prop : props)
        {
            for (const char* comp : components)
            {
                path_info->myCurves[c] = prop->GetCurve(fbx_anim_layer, comp, /*create*/true);
                path_info->myCurves[c]->KeyModifyBegin();
                ++c;
            }
        }

        path_info->myHasPrimTransform = info->getHasPrimTransform();
        if (obj_node && !path_info->myHasPrimTransform)
            obj_infos.append(path_info.get());
    }

    FbxTime fbx_time;
    fpreal start_time = myParentExporter->getStartTime();
    fpreal end_time = myParentExporter->getEndTime();
    fpreal secs_per_sample = CHgetManager()->getSecsPerSample();
    fpreal time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
    for (fpreal curr_time = start_time; SYSisLessOrEqual(curr_time, end_time); curr_time += time_step)
    {
        ropSetFbxTime(fbx_time, curr_time);

        OP_Context context(curr_time);
        UT_Matrix4D parent_xform(1.0); // identity
        if (obj_node)
        {
            (void) obj_node->getLocalToWorldTransform(context, parent_xform);

            for (PathInfo* path_info : obj_infos)
                path_info->addTransformKey(fbx_time, parent_xform);
        }

        GU_DetailHandle gdh;
        ROP_FBXUtil::getGeometryHandle(sop_node, context, gdh);
        GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();
        if (!gdp)
        {
            UT_WorkBuffer msg;
            msg.format("No geometry found on frame {}.", CHgetFrameFromTime(curr_time));
            myErrorManager->addError(msg.buffer());
            continue;
        }
        GA_ROHandleS path_attrib(gdp, GA_ATTRIB_PRIMITIVE, path_attrib_name);
        if (path_attrib.isInvalid())
        {
            UT_WorkBuffer msg;
            msg.format("Missing path attrib '{}' on frame {}.",
                       path_attrib_name, CHgetFrameFromTime(curr_time));
            myErrorManager->addError(msg.buffer());
            continue;
        }

        // Export prim transforms
        for (GA_Offset primoff : gdp->getPrimitiveRange())
        {
            auto item = info_from_path.find(path_attrib.get(primoff));
            if (item == info_from_path.end())
            {
                UT_WorkBuffer msg;
                msg.format("Found invalid path attrib value '{}' on frame {} primitive {}.",
                           path_attrib.get(primoff), CHgetFrameFromTime(curr_time),
                           gdp->primitiveIndex(primoff));
	        myErrorManager->addError(msg.buffer());
                continue;
            }

            PathInfo& path_info = *(item->second);
            if (!path_info.myHasPrimTransform)
                continue;

            UT_Matrix4D xform;
            int type_id = gdp->getPrimitiveTypeId(primoff);
            if (GU_PrimPacked::isPackedPrimitive(type_id))
            {
                const GA_Primitive *prim = gdp->getPrimitive(primoff);
                const GU_PrimPacked *packed_prim = UTverify_cast<const GU_PrimPacked *>(prim);
                packed_prim->getFullTransform4(xform);

                xform.pretranslate(path_info.myOrigin);
            }
            else if (ropHasLocalTransform(*gdp, type_id))
            {
                const GA_Primitive *prim = gdp->getPrimitive(primoff);
                prim->getLocalTransform4(xform);
            }
            else // no transform available, vertex cache output
            {
                continue;
            }

            xform *= parent_xform;

            path_info.addTransformKey(fbx_time, xform);
        }
    }

    for (auto&& item : info_from_path)
    {
        PathInfo& path_info = *(item.second);
        for (int c = 0; c < 9; ++c)
            path_info.myCurves[c]->KeyModifyEnd();
    }

    return true;
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
