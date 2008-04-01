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

#include "ROP_FBXAnimVisitor.h"
#include "ROP_FBXExporter.h"

#include <UT/UT_Matrix4.h>
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
#include <GU/GU_DetailHandle.h>
#include <GU/GU_ConvertParms.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Vertex.h>
#include <GU/GU_Detail.h>
#include <OBJ/OBJ_Node.h>
#include <SOP/SOP_Node.h>

#include "ROP_FBXUtil.h"
#include "ROP_FBXActionManager.h"

#include "ROP_FBXUtil.h"
#ifdef UT_DEBUG
extern double ROP_FBXdb_vcacheExportTime;
#endif
/********************************************************************************************************/
ROP_FBXAnimVisitor::ROP_FBXAnimVisitor(ROP_FBXExporter* parent_exporter)
{
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
    myFBXFileSourceFolder = file_path;
    myFBXShortFileName = file_name.pathUpToExtension();
}
/********************************************************************************************************/
ROP_FBXAnimVisitor::~ROP_FBXAnimVisitor()
{

}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::reset(void)
{

}
/********************************************************************************************************/
ROP_FBXBaseNodeVisitInfo* 
ROP_FBXAnimVisitor::visitBegin(OP_Node* node)
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

    res_type = ROP_FBXVisitorResultSkipSubnet;

    // Find the related FBX node
    ROP_FBXNodeInfo* stored_node_info_ptr = myNodeManager->findNodeInfo(node);
    if(!stored_node_info_ptr || !stored_node_info_ptr->getFbxNode())
    {
	UT_ASSERT(0);
	return res_type;
    }

    if(stored_node_info_ptr)
	res_type = stored_node_info_ptr->getVisitResultType();

    KFbxNode *fbx_node = stored_node_info_ptr->getFbxNode();
    node_info_in->setMaxObjectPoints(stored_node_info_ptr->getMaxObjectPoints());
    node_info_in->setVertexCacheMethod(stored_node_info_ptr->getVertexCacheMethod());

    // Create take nodes
    KFbxTakeNode* curr_fbx_take;
    KFCurve* curr_fbx_curve;
    curr_fbx_take = addFBXTakeNode(fbx_node);

    if(!curr_fbx_take)
	return res_type;

    UT_String node_type = node->getOperator()->getName();

    if(node_type == "bone")
    {
	// Bones are special, since we have to force-resample them and
	// output all channels at the same time.

	// Also, we have to put the bone animation onto the parent of the fbx node,
	// since this FBX node corresponds to the end tip of the bone.
	//KFbxTakeNode* curr_fbx_bone_take = fbx_node->GetParent()->GetCurrentTakeNode();    
	//exportBonesAnimation(curr_fbx_bone_take, node);
	exportBonesAnimation(curr_fbx_take, node);
    }
    else
    {

	exportTRSAnimation(node, curr_fbx_take);

	if(node_type == "geo")
	{
	    // For geometry, check if we have a dopimport SOP in the chain.
	    OP_Network* geo_net = dynamic_cast<OP_Network*>(node);

	    //if(isVertexCacheable(geo_net))
	    if(node_info_in->getMaxObjectPoints() > 0)
	    {
#ifdef UT_DEBUG
		double vc_start_time, vc_end_time;
		vc_start_time = clock();
#endif
		outputVertexCache(fbx_node, geo_net->getRenderNodePtr(), myOutputFileName.c_str(), node_info_in, stored_node_info_ptr);
#ifdef UT_DEBUG
		vc_end_time = clock();
		ROP_FBXdb_vcacheExportTime += (vc_end_time - vc_start_time);
#endif
	    }
	}
	else if(node_type == "hlight")
	{
	    // Output its colour, intensity, and cone angle channels
	    curr_fbx_curve = curr_fbx_take->GetLightIntensity();
	    exportChannel(curr_fbx_curve, node, "light_intensity", 0, 100.0);

	    curr_fbx_curve = curr_fbx_take->GetLightConeAngle();
	    exportChannel(curr_fbx_curve, node, "coneangle", 0);

	    curr_fbx_curve = curr_fbx_take->GetColorR();
	    exportChannel(curr_fbx_curve, node, "light_color", 0);

	    curr_fbx_curve = curr_fbx_take->GetColorG();
	    exportChannel(curr_fbx_curve, node, "light_color", 1);

	    curr_fbx_curve = curr_fbx_take->GetColorB();
	    exportChannel(curr_fbx_curve, node, "light_color", 2);
	}
	else if(node_type == "cam")
	{
	    curr_fbx_curve = curr_fbx_take->GetCameraFocalLength();
	    exportChannel(curr_fbx_curve, node, "focal", 0);
	}
    }

    return res_type;
}
/********************************************************************************************************/
KFbxTakeNode* 
ROP_FBXAnimVisitor::addFBXTakeNode(KFbxNode *fbx_node)
{
    string hd_take_name;
    TAKE_Take *curr_hd_take = OPgetDirector()->getTakeManager()->getCurrentTake();
    hd_take_name = curr_hd_take->getName();
    KFbxTakeNode* curr_fbx_take;

    fbx_node->CreateTakeNode(const_cast<char*>(hd_take_name.c_str()));
    fbx_node->SetCurrentTakeNode(const_cast<char*>(hd_take_name.c_str()));
    curr_fbx_take = fbx_node->GetCurrentTakeNode();    

    return curr_fbx_take;
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportTRSAnimation(OP_Node* node, KFbxTakeNode* curr_fbx_take)
{
    KFCurve* curr_fbx_curve;

    if(!node || !curr_fbx_take)
	return;

    // Translations
    curr_fbx_curve = curr_fbx_take->GetTranslationX();
    exportChannel(curr_fbx_curve, node, "t", 0);

    curr_fbx_curve = curr_fbx_take->GetTranslationY();
    exportChannel(curr_fbx_curve, node, "t", 1);

    curr_fbx_curve = curr_fbx_take->GetTranslationZ();
    exportChannel(curr_fbx_curve, node, "t", 2);

    // Rotations
    curr_fbx_curve = curr_fbx_take->GetEulerRotationX();
    exportChannel(curr_fbx_curve, node, "r", 0);

    curr_fbx_curve = curr_fbx_take->GetEulerRotationY();
    exportChannel(curr_fbx_curve, node, "r", 1);

    curr_fbx_curve = curr_fbx_take->GetEulerRotationZ();
    exportChannel(curr_fbx_curve, node, "r", 2);

    // Scaling
    curr_fbx_curve = curr_fbx_take->GetScaleX();
    exportChannel(curr_fbx_curve, node, "s", 0);

    curr_fbx_curve = curr_fbx_take->GetScaleY();
    exportChannel(curr_fbx_curve, node, "s", 1);

    curr_fbx_curve = curr_fbx_take->GetScaleZ();
    exportChannel(curr_fbx_curve, node, "s", 2); 

}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::onEndHierarchyBranchVisiting(OP_Node* last_node, ROP_FBXBaseNodeVisitInfo* last_node_info)
{
    // Nothing to do for now.
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportChannel(KFCurve* fbx_curve, OP_Node* source_node, const char* parm_name, int parm_idx, double scale_factor)
{
    if(!fbx_curve || !source_node || !parm_name)
	return;

    CH_Channel  *ch;
    PRM_Parm    *parm;

    // Get parameter.
    parm = &source_node->getParm(parm_name);
    if (!parm)
	return;

    ch = parm->getChannel(parm_idx);
    if(!ch || ch->getLastSegment() == NULL || ch->getLastSegment()->getLength() <= 0.0)
	return;

    float	     start_frame;
    float	     end_frame;
    UT_SuperInterval range;
    UT_IntArray	     tmp_array;
    CH_Manager *ch_manager = CHgetManager();

    float start_time = myParentExporter->getStartTime();
    float end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);
    //start_frame = ch_manager->getGlobalStartFrame();
    //end_frame = ch_manager->getGlobalEndFrame();
    CHbuildRange( start_frame, end_frame, range );
    ch->getKeyFrames(range, tmp_array, false);

    fbx_curve->KeyModifyBegin();

    float secs_per_sample = 1.0/ch_manager->getSamplesPerSec();
    if(myExportOptions->getResampleAllAnimation())
    {
	outputResampled(fbx_curve, ch, 0, tmp_array.entries()-1, tmp_array, false);
    }
    else
    {
	int fbx_key_idx;
	float key_time;
	KTime fbx_time;
	CH_FullKey full_key;
	CH_Segment* next_seg;
	CH_Expression* hd_seg_expr;
	int curr_frame, num_frames = tmp_array.entries();
	string str_temp_expr;
	double key_val, db_val;

	for(curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    key_time = ch_manager->getTime(tmp_array[curr_frame]);
	    ch->getFullKey(key_time, full_key);

	    fbx_time.SetSecondDouble(key_time+secs_per_sample);
	    fbx_key_idx = fbx_curve->KeyAdd(fbx_time);
	}

	kFCurveIndex c_index = 0;
	for(curr_frame = 0; curr_frame < num_frames; curr_frame++)
	{
	    // Convert frame to time
	    key_time = ch_manager->getTime(tmp_array[curr_frame]);
	    ch->getFullKey(key_time, full_key);

	    fbx_time.SetSecondDouble(key_time+secs_per_sample);
	    fbx_key_idx = fbx_curve->KeyFind(fbx_time, &c_index);

	    if( (full_key.k[0].myVValid[CH_VALUE] && full_key.k[1].myVValid[CH_VALUE]) )
	    {
		if( SYSisEqual( full_key.k[0].myV[CH_VALUE], full_key.k[1].myV[CH_VALUE]) )
		{
		    myErrorManager->addError("Untied key values encountered. This is not supported. Node: ", source_node->getName(), NULL, false );
		    key_val = (full_key.k[0].myV[CH_VALUE] + full_key.k[1].myV[CH_VALUE])*0.5;
		}
	    }
	    else if(full_key.k[0].myVValid[CH_VALUE])
		key_val = full_key.k[0].myV[CH_VALUE];
	    else if(full_key.k[1].myVValid[CH_VALUE])
		key_val = full_key.k[1].myV[CH_VALUE];
	    else 
		key_val = 0.0;

	    key_val *= scale_factor;

	    // Doh! Segments can be expressions, as well.
	    // We'll support some basic types, and then resample the rest.
	    fbx_curve->KeySetValue(fbx_key_idx, key_val);

	    // Look at the next segment type
	    next_seg = ch->getSegmentAfterKey(key_time);
	    if(next_seg)
	    {
		hd_seg_expr = next_seg->getCHExpr();
		if( hd_seg_expr->usesSlopes())	    
		{
		    fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_CUBIC);
		    if(full_key.k[0].myVTied[CH_SLOPE] || full_key.k[1].myVTied[CH_SLOPE])
			fbx_curve->KeySetTangeantMode(fbx_key_idx, KFCURVE_TANGEANT_USER);
		    else
			fbx_curve->KeySetTangeantMode(fbx_key_idx, KFCURVE_GENERIC_BREAK);

		    KFCurveTangeantInfo r_info, l_info;

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

		    fbx_curve->KeySetLeftDerivativeInfo(fbx_key_idx, l_info);
		    fbx_curve->KeySetRightDerivativeInfo(fbx_key_idx, r_info);
		}
		else
		{
		    // Try to look for the word linear in it
		    if(hd_seg_expr->findString("linear()", true, false) || hd_seg_expr->findString("qlinear()", true, false))
		    {
			// Linear segment
			fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
		    }
		    else if(hd_seg_expr->findString("constant()", true, false))
		    {
			// Constant segment
			fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_CONSTANT);
			fbx_curve->KeySetConstantMode(fbx_key_idx, KFCURVE_CONSTANT_STANDARD);
		    }
		    else if(hd_seg_expr->findString("vmatchout()", true, false) || hd_seg_expr->findString("matchout()", true, false))
		    {
			// Constant segment
			fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_CONSTANT);
			fbx_curve->KeySetConstantMode(fbx_key_idx, KFCURVE_CONSTANT_NEXT);
		    }
		    else
		    {
			// Unsupported segment. Treat as linear. 
			// To-do - resample?
			string expr_string(", expression: ");
			expr_string += (const char *)hd_seg_expr;
			myErrorManager->addError("Unsupported segment type found. This segment will be resampled. Node: ", source_node->getName(), expr_string.c_str(), false );
			UT_ASSERT(0);

			fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);

			int next_frame_idx = curr_frame + 1;
			if(next_frame_idx >= num_frames)
			    next_frame_idx = curr_frame;
			outputResampled(fbx_curve, ch, curr_frame, next_frame_idx, tmp_array, true);
		    }
		}
    	
	    }

	}
    }
    fbx_curve->KeyModifyEnd();
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::outputResampled(KFCurve* fbx_curve, CH_Channel *ch, int start_array_idx, int end_array_idx, UT_IntArray& frame_array, bool do_insert)
{
    UT_ASSERT(start_array_idx <= end_array_idx);
    if(end_array_idx < start_array_idx)
	return;

    CH_Manager *ch_manager = CHgetManager();
    int curr_idx;
    float key_time;

    float secs_per_sample = 1.0/ch_manager->getSamplesPerSec();
    float time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
    float curr_time, end_time = 0.0;
    int end_idx;
    bool is_last;
    int fbx_key_idx;
    KTime fbx_time;
    CH_Segment *next_seg;
    kFCurveIndex opt_idx;
    float key_val;
    float s;
    curr_time = 0;
    for(curr_idx = start_array_idx; curr_idx < end_array_idx; curr_idx++)
    {
	// Insert keyframes, evaluate the values at the channel
	key_time = ch_manager->getTime(frame_array[curr_idx]);
	end_idx = curr_idx + 1;
	if(end_idx > end_array_idx)
	    end_idx = end_array_idx;
	end_time = ch_manager->getTime(frame_array[end_idx]);

	next_seg = ch->getSegmentAfterKey(key_time);

	if(next_seg)
	{
	    opt_idx = 0;
	    is_last = false;
	    for(curr_time = key_time; curr_time < end_time; curr_time += time_step)
	    {
		ch->sampleValueSlope(next_seg, curr_time, key_val, s);

		fbx_time.SetSecondDouble(curr_time+secs_per_sample);
		if(do_insert)
		    fbx_key_idx = fbx_curve->KeyInsert(fbx_time, &opt_idx);
		else
		    fbx_key_idx = fbx_curve->KeyAdd(fbx_time, &opt_idx);
		fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
		fbx_curve->KeySetValue(fbx_key_idx, key_val);
	    }
	}
    }

    if(start_array_idx < end_array_idx && curr_time >= end_time)
    {
	CH_FullKey full_key;

	// We skipped the last frame. Add it now.
	end_time = ch_manager->getTime(frame_array[end_array_idx]);
	ch->getFullKey(end_time, full_key);

	fbx_time.SetSecondDouble(end_time+secs_per_sample);
	if(do_insert)
	    fbx_key_idx = fbx_curve->KeyInsert(fbx_time);
	else
	    fbx_key_idx = fbx_curve->KeyAdd(fbx_time);
	fbx_curve->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);

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
KFbxVertexCacheDeformer* 
ROP_FBXAnimVisitor::addedVertexCacheDeformerToNode(KFbxNode* fbx_node, const char* file_name)
{
    // By convention, all cache files are created in a .fpc folder located at the same
    // place as the .fbx file.     
    string cache_file_name(myFBXFileSourceFolder);
    cache_file_name += "/";
    cache_file_name += myFBXShortFileName;
//    cache_file_name += fbx_node->GetName();
    cache_file_name += ".fpc";

    ::mkdir(cache_file_name.c_str(), 0777);

    string rel_pc_name("");;
    string absolute_pc_name(cache_file_name);
    absolute_pc_name += "/";
//    absolute_pc_name += myFBXShortFileName;
    absolute_pc_name += fbx_node->GetName();

    rel_pc_name += myFBXShortFileName;
    rel_pc_name += ".fpc/";
//    rel_pc_name += myFBXShortFileName;
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
    KFbxCache* v_cache = KFbxCache::Create(mySDKManager, fbx_node->GetName());

    v_cache->SetCacheFileName(rel_pc_name.c_str(), absolute_pc_name.c_str());
    if(myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
	v_cache->SetCacheFileFormat(KFbxCache::eMC);
    else
	v_cache->SetCacheFileFormat(KFbxCache::ePC2);

    // Create the vertex deformer
    KFbxVertexCacheDeformer* deformer = KFbxVertexCacheDeformer::Create(mySDKManager, fbx_node->GetName());

    deformer->SetCache(v_cache);
    deformer->SetCacheChannel(fbx_node->GetName());
    deformer->SetActive(true);

    // Apply the deformer on the mesh
    fbx_node->GetGeometry()->AddDeformer(deformer);

    return deformer;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::outputVertexCache(KFbxNode* fbx_node, OP_Node* geo_node, const char* file_name, ROP_FBXBaseNodeVisitInfo* node_info_in, ROP_FBXNodeInfo* node_pair_info)
{
    SOP_Node* sop_node = dynamic_cast<SOP_Node*>(geo_node);
    if(!sop_node)
	return false;

    KFbxVertexCacheDeformer* vc_deformer = addedVertexCacheDeformerToNode(fbx_node, file_name);

    CH_Manager *ch_manager = CHgetManager();
//    float start_frame = ch_manager->getGlobalStartFrame();
//    float end_frame = ch_manager->getGlobalEndFrame();
    float start_frame, end_frame;
    float start_time = myParentExporter->getStartTime();
    float end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(start_time);
    end_frame = ch_manager->getSample(end_time);
    float curr_fps = ch_manager->getSamplesPerSec();

    // Now add data to the deformer
    KFbxCache*               v_cache = vc_deformer->GetCache();
    bool res;

    // Write samples for 4 seconds
    KTime fbx_curr_time;
    float hd_time;
  //  KTime fbx_time_increment, fbx_curr_time, fbx_stop_time;
//    fbx_time_increment.SetTime(0, 0, 0, 1); // 1 frame @ current frame rate
//    fbx_stop_time.SetTime(0, 0, 4);         // 4 seconds
//    fbx_curr_time.SetTime(0,0,0,start_frame);
  //  fbx_stop_time.SetTime(0,0,0,end_frame);
    int curr_frame;

    //unsigned int frame_count = (unsigned int)(fbx_stop_time.Get()/fbx_time_increment.Get());
    unsigned int frame_count = end_frame - start_frame + 1;

    // Here we unfortunately have to go and find the maximum number of points over all frames.
    //int max_points = ROP_FBXUtil::getMaxPointsOverAnimation(sop_node);
    int max_gdp_points = node_info_in->getMaxObjectPoints();
    int num_vc_points;
    /*
    if(node_info_in->getVertexCacheMethod() == ROP_FBXVertexCacheMethodParticles)
	num_vc_points = max_particles*ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT;
    else */
	num_vc_points = max_gdp_points;
    //num_vc_points++;

    // Open the file for writing
    if (myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
	//res = v_cache->OpenFileForWrite(KFbxCache::eMC_ONE_FILE_PER_FRAME, curr_fps, fbx_node->GetName());
	res = v_cache->OpenFileForWrite(KFbxCache::eMC_ONE_FILE, curr_fps, fbx_node->GetName());
    else
	res = v_cache->OpenFileForWrite(0.0, curr_fps, frame_count, num_vc_points);  

    if (!res)
    {
	myErrorManager->addError("Cannot open the vertex cache file. Error message: ", v_cache->GetError().GetLastErrorString(), NULL,  false );
	return false;
    }

    int channel_index = v_cache->GetChannelIndex(fbx_node->GetName());
    //while (fbx_curr_time <= fbx_stop_time)

    // Allocate our buffer array
    double *vert_coords = new double[num_vc_points*3];

    // Output the points. Remember that when outputting this mesh, the points were reversed.
    for(curr_frame = start_frame; curr_frame <= end_frame; curr_frame++)
    {
	hd_time = ch_manager->getTime(curr_frame);
	fbx_curr_time.SetTime(0,0,0, curr_frame);
	//fbx_curr_time.SetSecondDouble(hd_time);

	if(!fillVertexArray(sop_node, hd_time, node_info_in, vert_coords, num_vc_points, node_pair_info, curr_frame))
	{
	    myErrorManager->addError("Could not evaluate a frame of vertex cache array. Node: ", geo_node->getName(), NULL, false);
	    continue;
	}
/*
	double lVertices[3][3];
	double lScaleFactor = 1.0-double(fbx_curr_time.GetSecondDouble()/fbx_stop_time.GetSecondDouble());

	lVertices[0][0] = -50.0 * lScaleFactor;  // X
	lVertices[0][1] = 0.0;                   // Y
	lVertices[0][2] = 50.0  * lScaleFactor;  // Z

	lVertices[1][0] = 50.0  * lScaleFactor;  // X
	lVertices[1][1] = 0.0;                   // Y
	lVertices[1][2] = 50.0  * lScaleFactor;  // Z

	lVertices[2][0] = 0.0   * lScaleFactor;  // X
	lVertices[2][1] = 50.0  * lScaleFactor;  // Y
	lVertices[2][2] = -50.0 * lScaleFactor;  // Z

	lVertices[0][0] = 0.0;  // X
	lVertices[0][1] = 0.0;                   // Y
	lVertices[0][2] = 0.0;  // Z

	lVertices[1][0] = 0.0;  // X
	lVertices[1][1] = 0.0;                   // Y
	lVertices[1][2] = 0.0;  // Z

	lVertices[2][0] = 0.0;  // X
	lVertices[2][1] = 0.0;  // Y
	lVertices[2][2] = 0.0;  // Z
*/
	if (myExportOptions->getVertexCacheFormat() == ROP_FBXVertexCacheExportFormatMaya)
	{
	    v_cache->Write(channel_index, fbx_curr_time, vert_coords, num_vc_points);
	}
	else
	{
	    v_cache->Write(curr_frame, vert_coords);
	}

//	fbx_curr_time += fbx_time_increment;
//	curr_frame++;
    }

    if (!v_cache->CloseFile())
    {
	myErrorManager->addError("Cannot close the vertex cache file. Error message: ", v_cache->GetError().GetLastErrorString(), NULL, false);
    }	

    delete[] vert_coords;
    return true;
}
/********************************************************************************************************/
bool 
ROP_FBXAnimVisitor::fillVertexArray(SOP_Node* node, float time, ROP_FBXBaseNodeVisitInfo* node_info_in, double* vert_array, 
				    int num_array_points, ROP_FBXNodeInfo* node_pair_info, float frame_num)
{
    ROP_FBXVertexCacheMethodType vc_method = node_pair_info->getVertexCacheMethod();
    const GU_Detail *final_gdp = NULL;

/*
    int points_per_prim = 0;

    UT_Vector4 part_stock_tri[ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT];
    if(vc_method == ROP_FBXVertexCacheMethodParticles)
    {
	// Get at the gdp
	GU_DetailHandle gdh;
	if(!ROP_FBXUtil::getGeometryHandle(node, time, gdh))
	    return false;

	GU_DetailHandleAutoReadLock	 gdl(gdh);
	const GU_Detail *gdp;
	gdp = gdl.getGdp();
	if(!gdp)
	    return false;

	GU_Detail conv_gdp;
	ROP_FBXUtil::convertParticleGDPtoPolyGDP(gdp, num_particles, conv_gdp);
	final_gdp = &conv_gdp;

	points_per_prim = ROP_FBX_DUMMY_PARTICLE_GEOM_VERTEX_COUNT;
	if(final_gdp->points().entries() > 3)
	{
	    part_stock_tri[0] = final_gdp->points()(0)->getPos();
	    part_stock_tri[1] = final_gdp->points()(1)->getPos();
	    part_stock_tri[2] = final_gdp->points()(2)->getPos();
	    part_stock_tri[3] = final_gdp->points()(3)->getPos();
	}
    }
    else
    {
	if(node_pair_info->getVertexCache())
	{
	    final_gdp = node_pair_info->getVertexCache()->getFrameGeometry(frame_num);
	}
	else
	{
	    UT_ASSERT(0);
//	    ROP_FBXUtil::convertGeoGDPtoVertexCacheableGDP(gdp, conv_gdp);
//	    final_gdp = &conv_gdp;
	}
	
	points_per_prim = 3;
	part_stock_tri[0] = 0;
	part_stock_tri[1] = 0;
	part_stock_tri[2] = 0;
	if(final_gdp->points().entries() > 2)
	{
//	    part_stock_tri[0] = final_gdp->points()(0)->getPos();
//	    part_stock_tri[1] = final_gdp->points()(1)->getPos();
//	    part_stock_tri[2] = final_gdp->points()(2)->getPos();
	}

    }
*/
    final_gdp = node_pair_info->getVertexCache()->getFrameGeometry(frame_num);
    if(!final_gdp)
	return false;


    int actual_gdp_points = final_gdp->points().entries();
    int curr_point, actual_max_points;
    UT_Vector4 ut_vec;
    int arr_offset;

    double add_offset = 1.0;
    for(curr_point = 0; curr_point < num_array_points; curr_point++)
    //for(curr_point = 0; curr_point < actual_gdp_points; curr_point++)
    {
	if(curr_point < actual_gdp_points)	
	    ut_vec = final_gdp->points()(curr_point%actual_gdp_points)->getPos();
	else
	    ut_vec = 0;
/*
	else
	{
	    arr_offset = (curr_point - actual_gdp_points)%points_per_prim;
	    //arr_offset = curr_point%points_per_prim;
	    ut_vec = part_stock_tri[arr_offset];
	} */

	arr_offset = (num_array_points - curr_point - 1)*3;
	//arr_offset = curr_point*3;
	vert_array[arr_offset] = ut_vec.x();
	vert_array[arr_offset+1] = ut_vec.y();
	vert_array[arr_offset+2] = ut_vec.z();
    }

    return true;
}
/********************************************************************************************************/
void 
ROP_FBXAnimVisitor::exportBonesAnimation(KFbxTakeNode* curr_fbx_take, OP_Node* source_node)
{
    // Get channels, range, and make sure we have any animation at all
    int curr_trs_channel;
    const int num_trs_channels = 3;
    const int num_channels = 4;
    int curr_channel_idx;
    const char* const channel_names[] = { "t", "r", "scale", "length"};
//    float	     start_frame;
//    float	     end_frame;
    CH_Manager *ch_manager = CHgetManager();
//    start_frame = ch_manager->getGlobalStartFrame();
//    end_frame = ch_manager->getGlobalEndFrame();
    float start_frame, end_frame;
    float gb_start_time = myParentExporter->getStartTime();
    float gb_end_time = myParentExporter->getEndTime();
    start_frame = ch_manager->getSample(gb_start_time);
    end_frame = ch_manager->getSample(gb_end_time);


    float start_time = FLT_MAX, end_time = -FLT_MAX;

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
	    ch = parm->getChannel(curr_trs_channel);
	    if(!ch)
		break;
	    if(ch->getStart() < start_time) start_time = ch->getStart();
	    if(ch->getEnd() > end_time) end_time = ch->getEnd();
	}
    }

    // No animation.
    if(start_time == FLT_MAX || start_time >= end_time)
	return;

    // Get fbx curves
    KFCurve* fbx_t[num_trs_channels];
    KFCurve* fbx_r[num_trs_channels];
    KFCurve* fbx_s[num_trs_channels];
    fbx_t[0] = curr_fbx_take->GetTranslationX();
    fbx_t[1] = curr_fbx_take->GetTranslationY();
    fbx_t[2] = curr_fbx_take->GetTranslationZ();

    fbx_r[0] = curr_fbx_take->GetEulerRotationX();
    fbx_r[1] = curr_fbx_take->GetEulerRotationY();
    fbx_r[2] = curr_fbx_take->GetEulerRotationZ();
    
    fbx_s[0] = curr_fbx_take->GetScaleX();
    fbx_s[1] = curr_fbx_take->GetScaleY();
    fbx_s[2] = curr_fbx_take->GetScaleZ();

    kFCurveIndex t_opt_idx[3];
    kFCurveIndex r_opt_idx[3];
    kFCurveIndex s_opt_idx[3];
    for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
    {
	fbx_t[curr_channel_idx]->KeyModifyBegin();
	fbx_r[curr_channel_idx]->KeyModifyBegin();
	fbx_s[curr_channel_idx]->KeyModifyBegin();

	t_opt_idx[curr_channel_idx] = 0;
	r_opt_idx[curr_channel_idx] = 0;
	s_opt_idx[curr_channel_idx] = 0;
    }

    float secs_per_sample = 1.0/ch_manager->getSamplesPerSec();
    float time_step = secs_per_sample * myExportOptions->getResampleIntervalInFrames();
    float curr_time;
    KTime fbx_time;
    int fbx_key_idx;
    float bone_length;
    UT_Vector3 t_out, r_out, s_out;

    OP_Node* parent_node;

    // Walk the time, compute the final transform matrix at each time, and break it.
    for(curr_time = start_time; curr_time < end_time; curr_time += time_step)
    {
	bone_length = 0;
	parent_node = source_node->getInput(source_node->getConnectedInputIndex(-1));
	if(parent_node)
	    bone_length = ROP_FBXUtil::getFloatOPParm(parent_node, "length", 0, curr_time);
	//bone_length = ROP_FBXUtil::getFloatOPParm(source_node, "length", 0, curr_time);
	ROP_FBXUtil::getFinalTransforms(source_node, false, bone_length, curr_time, t_out, r_out, s_out, NULL); // , true, parent_node);

	fbx_time.SetSecondDouble(curr_time+secs_per_sample);
	for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
	{
	    fbx_key_idx = fbx_t[curr_channel_idx]->KeyInsert(fbx_time, &t_opt_idx[curr_channel_idx]);
	    fbx_t[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
	    fbx_t[curr_channel_idx]->KeySetValue(fbx_key_idx, t_out[curr_channel_idx]);

	    fbx_key_idx = fbx_r[curr_channel_idx]->KeyInsert(fbx_time, &r_opt_idx[curr_channel_idx]);
	    fbx_r[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
	    fbx_r[curr_channel_idx]->KeySetValue(fbx_key_idx, r_out[curr_channel_idx]);

	    fbx_key_idx = fbx_s[curr_channel_idx]->KeyInsert(fbx_time, &s_opt_idx[curr_channel_idx]);
	    fbx_s[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
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
	ROP_FBXUtil::getFinalTransforms(source_node, false, bone_length, end_time, t_out, r_out, s_out, NULL); // , true, parent_node);

	fbx_time.SetSecondDouble(end_time+secs_per_sample);
	for(curr_channel_idx = 0; curr_channel_idx < num_trs_channels; curr_channel_idx++)
	{
	    fbx_key_idx = fbx_t[curr_channel_idx]->KeyInsert(fbx_time, &t_opt_idx[curr_channel_idx]);
	    fbx_t[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
	    fbx_t[curr_channel_idx]->KeySetValue(fbx_key_idx, t_out[curr_channel_idx]);

	    fbx_key_idx = fbx_r[curr_channel_idx]->KeyInsert(fbx_time, &r_opt_idx[curr_channel_idx]);
	    fbx_r[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
	    fbx_r[curr_channel_idx]->KeySetValue(fbx_key_idx, r_out[curr_channel_idx]);

	    fbx_key_idx = fbx_s[curr_channel_idx]->KeyInsert(fbx_time, &s_opt_idx[curr_channel_idx]);
	    fbx_s[curr_channel_idx]->KeySetInterpolation(fbx_key_idx, KFCURVE_INTERPOLATION_LINEAR);
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