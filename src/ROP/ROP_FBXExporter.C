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
#include "ROP_FBXActionManager.h"
#include "ROP_FBXExporter.h"
#include <GU/GU_DetailHandle.h>
#include <OP/OP_Network.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <UT/UT_UndoManager.h>
#include <UT/UT_Interrupt.h>
#include <OBJ/OBJ_Node.h>
#include <TAKE/TAKE_Manager.h>
#include <TAKE/TAKE_Take.h>
#include <OP/OP_Take.h>
#include <SOP/SOP_Node.h>
#include <GEO/GEO_Vertex.h>

#include "ROP_FBXUtil.h"
#include "ROP_FBXAnimVisitor.h"
#include "ROP_FBXMainVisitor.h"
#include "ROP_FBXDerivedActions.h"

#ifdef UT_DEBUG
double ROP_FBXdb_maxVertsCountingTime;
double ROP_FBXdb_vcacheExportTime;
double ROP_FBXdb_cookingTime;
double ROP_FBXdb_convexTime;
double ROP_FBXdb_reorderTime;
double ROP_FBXdb_convertTime;
double ROP_FBXdb_duplicateTime;
#endif
/********************************************************************************************************/
ROP_FBXExporter::ROP_FBXExporter()
{
    mySDKManager = NULL;
    myScene = NULL;
    myNodeManager = NULL;
    myActionManager = NULL;
    myDummyRootNullNode = NULL;
    myBoss = NULL;
    myDidCancel = false;
    myErrorManager = new ROP_FBXErrorManager();
}
/********************************************************************************************************/
ROP_FBXExporter::~ROP_FBXExporter()
{
    if(myErrorManager)
	delete myErrorManager;
    myErrorManager = NULL;
}
/********************************************************************************************************/
bool 
ROP_FBXExporter::initializeExport(const char* output_name, float tstart, float tend, ROP_FBXExportOptions* options)
{
    if(!output_name)
	return false;

    deallocateQueuedStrings();

#ifdef UT_DEBUG
    ROP_FBXdb_vcacheExportTime = 0;
    ROP_FBXdb_maxVertsCountingTime = 0;
    ROP_FBXdb_cookingTime = 0;
    ROP_FBXdb_convexTime = 0;
    ROP_FBXdb_reorderTime = 0;
    ROP_FBXdb_convertTime = 0;
    ROP_FBXdb_duplicateTime = 0;
    myDBStartTime = clock();
#endif
    myErrorManager->reset();

    myStartTime = tstart;
    myEndTime = tend;

    if(options)
	myExportOptions = *options;
    else
	myExportOptions.reset();

    myNodeManager = new ROP_FBXNodeManager;
    myActionManager = new ROP_FBXActionManager(*myNodeManager, *myErrorManager, *this);

    // Initialize the fbx scene manager
    mySDKManager = KFbxSdkManager::CreateKFbxSdkManager();

    if (!mySDKManager)
    {
//	addError(ROP_COOK_ERROR, "Unable to create the FBX SDK manager");
	return false;
    }

    // Create the entity that will hold the scene.
    myScene = KFbxScene::Create(mySDKManager,"");
    myOutputFile = output_name;

    myDidCancel = false;
    myDummyRootNullNode = NULL;

    return true;
}
/********************************************************************************************************/
void 
ROP_FBXExporter::doExport(void)
{
    UT_UndoManager::disableUndoCreation();

    myBoss = UTgetInterrupt();

    if( !myBoss->opStart("Exporting FBX" ) )
	return;

    // Set the exported take
    OP_Take	*take_mgr = OPgetDirector()->getTakeManager();
    TAKE_Take *init_take = take_mgr->getCurrentTake();

    // Find and set the needed take
    if(strlen(myExportOptions.getExportTakeName()) == 0)
    {
	// Export current take.
	init_take = NULL;
    }
    else
	take_mgr->takeSet(myExportOptions.getExportTakeName());

    // Export geometry first
    ROP_FBXMainVisitor geom_visitor(this);

    KFbxGlobalTimeSettings& scene_time_setting = myScene->GetGlobalTimeSettings();

    bool exporting_single_frame = !getExportingAnimation();
    CH_Manager *ch_manager = CHgetManager();
    KTime fbx_start, fbx_stop;


    // If we're outputting a single frame, we don't allow exporting deforms as vertex caches
    if(exporting_single_frame)
    {
	// This is a copy of the originally passed-in options,
	// so it's ok to change it.
	myExportOptions.setExportDeformsAsVC(false);
    }

    if(!exporting_single_frame)
    {
	// NOTE: This is a hack. Maya's FBX importer/exporter assumes, for who knows what
	// reason, that the value set as seconds in this specific case (SetTimelineDefautTimeSpan)
	// is actually frames. Our importer, as a consequence, also assumes that.
	// So we export it like this, too. Hopefully they'll fix it in the next revision of FBX SDK.
	fbx_start.SetSecondDouble(CHgetFrameFromTime(myStartTime));
	fbx_stop.SetSecondDouble(CHgetFrameFromTime(myEndTime));
//	fbx_start.SetSecondDouble(myStartTime);
//	fbx_stop.SetSecondDouble(myEndTime);

	KTimeSpan time_span(fbx_start, fbx_stop);
	scene_time_setting.SetTimelineDefautTimeSpan(time_span);

	// FBX doesn't support arbitrary frame rates.
	// Try to match one if it's exact, otherwise default to 24fps and warn the user.
	float curr_fps = ch_manager->getSamplesPerSec();
	KTime::ETimeMode time_mode = KTime::eCINEMA;
	if(SYSisEqual(curr_fps, 24.0))
	    time_mode = KTime::eCINEMA;
	else if(SYSisEqual(curr_fps, 120.0))
	    time_mode = KTime::eFRAMES120;
	else if(SYSisEqual(curr_fps, 100.0))
	    time_mode = KTime::eFRAMES100;
	else if(SYSisEqual(curr_fps, 60.0))
	    time_mode = KTime::eFRAMES60;
	else if(SYSisEqual(curr_fps, 48.0))
	    time_mode = KTime::eFRAMES48;
	else if(SYSisEqual(curr_fps, 50.0))
	    time_mode = KTime::eFRAMES50;
	else if(SYSisEqual(curr_fps, 30.0))
	    time_mode = KTime::eFRAMES30;
	else if(SYSisEqual(curr_fps, 25.0))
	    time_mode = KTime::ePAL;
	else
	    myErrorManager->addError("Unsupported scene frame rate found. Defaulting to 24 frames per second.",NULL,NULL,false);
	scene_time_setting.SetTimeMode(time_mode);
	KTime::SetGlobalTimeMode(time_mode);
    }

    // Note: what about geom networks in other parts of the scene?
    OP_Node* geom_node;
    geom_node = OPgetDirector()->findNode(myExportOptions.getStartNodePath());

    if(!geom_node)
    {
	// Issue a warning and quit.
	myErrorManager->addError("Could not find the start node specified [ ",myExportOptions.getStartNodePath()," ]",true);
	return;
    }

    geom_visitor.addNonVisitableNetworkTypes(ROP_FBXnetworkTypesToIgnore);

    geom_visitor.visitScene(geom_node);
    myDidCancel = geom_visitor.getDidCancel();

    // Create any instances, if necessary
    if(geom_visitor.getCreateInstancesAction())
	geom_visitor.getCreateInstancesAction()->performAction();

    if(!myDidCancel)
    {
	// Global light settings - set the global ambient.
	KFbxColor fbx_col;
	float amb_col[3];
	UT_Color amb_color = geom_visitor.getAccumAmbientColor();
	amb_color.getRGB(&amb_col[0], &amb_col[1], &amb_col[2]);
	fbx_col.Set(amb_col[0],amb_col[1],amb_col[2]);
	KFbxGlobalLightSettings& global_light_settings = myScene->GetGlobalLightSettings();
	global_light_settings.SetAmbientColor(fbx_col);

	// Export animation if applicable
	if(!exporting_single_frame)
	{ 
	    ROP_FBXAnimVisitor anim_visitor(this);
	    anim_visitor.addNonVisitableNetworkTypes(ROP_FBXnetworkTypesToIgnore);

	    // Export the main world_root animation if applicable
	    if(myDummyRootNullNode)
	    {			
		KFbxTakeNode* curr_world_take_node = ROP_FBXAnimVisitor::addFBXTakeNode(myDummyRootNullNode);
		anim_visitor.exportTRSAnimation(geom_node, curr_world_take_node);
	    }	    

	    anim_visitor.reset();
	    anim_visitor.visitScene(geom_node);
	    myDidCancel = anim_visitor.getDidCancel();

	}
	// Perform post-actions
	if(!myDidCancel)
	    myActionManager->performPostActions();
    }

    // Restore original take
    if(init_take)
	take_mgr->takeSet(init_take->getName());

    myBoss->opEnd();
    myBoss = NULL;

    UT_UndoManager::enableUndoCreation();
}
/********************************************************************************************************/
bool
ROP_FBXExporter::finishExport(void)
{
#ifdef UT_DEBUG
    double write_time_start, write_time_end;
    write_time_start = clock();
#endif
    if(!myDidCancel)
    {
	// Save the built-up scene
	KFbxExporter* fbx_exporter = KFbxExporter::Create(mySDKManager, "");

	// Initialize the exporter by providing a filename.
	if(fbx_exporter->Initialize(myOutputFile.c_str()) == false)
	{
    //	addError(ROP_COOK_ERROR, (const char *)"Invalid output path");
    //	return ROP_ABORT_RENDER;
	    return false;
	}


	//Try to export in ASCII if possible
	int format_index, format_count = KFbxIOPluginRegistryAccessor::Get()->GetWriterFormatCount();
	int out_file_format = -1;

	for (format_index = 0; format_index < format_count; format_index++)
	{
	    if (KFbxIOPluginRegistryAccessor::Get()->WriterIsFBX(format_index))
	    {
		KString lDesc = KFbxIOPluginRegistryAccessor::Get()->GetWriterFormatDescription(format_index);
		if (lDesc.Find("ascii")>=0 && myExportOptions.getExportInAscii())
		{
		    out_file_format = format_index;
		    break;
		}
		else if (lDesc.Find("ascii")<0 && !myExportOptions.getExportInAscii())
		{
		    out_file_format = format_index;
		    break;
		}
	    }
	}

	fbx_exporter->SetFileFormat(out_file_format);

	KFbxStreamOptionsFbxWriter* export_options = KFbxStreamOptionsFbxWriter::Create(mySDKManager, "");
	if (KFbxIOPluginRegistryAccessor::Get()->WriterIsFBX(out_file_format))
	{
	    // Set the export states. By default, the export states are always set to 
	    // true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	    // shows how to change these states.
	    /*
	    export_options->SetOption(KFBXSTREAMOPT_FBX_MATERIAL, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_TEXTURE, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_EMBEDDED, pEmbedMedia);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_LINK, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_SHAPE, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_GOBO, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_ANIMATION, true);
	    export_options->SetOption(KFBXSTREAMOPT_FBX_GLOBAL_SETTINGS, true); */
	}

	// Export the scene.
	bool exp_status = fbx_exporter->Export(*myScene, export_options); 

	if(export_options)
	    export_options->Destroy();
	export_options=NULL;

	// Destroy the exporter.
	fbx_exporter->Destroy();
    }

    if(myScene)
	myScene->Destroy();
    myScene = NULL;

    if(mySDKManager)
	mySDKManager->DestroyKFbxSdkManager();
    mySDKManager = NULL;

    deallocateQueuedStrings();

#ifdef UT_DEBUG
    write_time_end = clock();
#endif

    if(myNodeManager)
	delete myNodeManager;
    myNodeManager = NULL;

    if(myActionManager)
	delete myActionManager;
    myActionManager = NULL;


#ifdef UT_DEBUG
    myDBEndTime = clock();

    // Print results
    double total_time = myDBEndTime - myDBStartTime;
    double temp_time;

    printf("Max Vertex Count Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_maxVertsCountingTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_maxVertsCountingTime / total_time * 100.0 );
    printf("\tPure Cooking Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_cookingTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_cookingTime / ROP_FBXdb_maxVertsCountingTime * 100.0 );        
    printf("\tDuplication Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_duplicateTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_duplicateTime / ROP_FBXdb_maxVertsCountingTime * 100.0 );
    printf("\tConversion Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_convertTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_convertTime / ROP_FBXdb_maxVertsCountingTime * 100.0 );
    printf("\tTri Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_convexTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_convexTime / ROP_FBXdb_maxVertsCountingTime * 100.0 );
    printf("\tReordering Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_reorderTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_reorderTime / ROP_FBXdb_maxVertsCountingTime * 100.0 );
    printf("Vertex Caching Time: %.2f secs ( %.2f%%) \n", ((double)ROP_FBXdb_vcacheExportTime) / ((double)CLOCKS_PER_SEC), ROP_FBXdb_vcacheExportTime / total_time * 100.0 );
    temp_time = write_time_end - write_time_start;
    printf("File Write Time: %.2f secs ( %.2f%%) \n", ((double)temp_time) / ((double)CLOCKS_PER_SEC), temp_time / total_time * 100.0 );
    
    printf("Total Export Time: %.2f secs \n\n", ((double)total_time) / ((double)CLOCKS_PER_SEC) );
#endif

    return true;
}
/********************************************************************************************************/
KFbxSdkManager* 
ROP_FBXExporter::getSDKManager(void)
{
    return mySDKManager;
}
/********************************************************************************************************/
KFbxScene* 
ROP_FBXExporter::getFBXScene(void)
{
    return myScene;
}
/********************************************************************************************************/
ROP_FBXErrorManager* 
ROP_FBXExporter::getErrorManager(void)
{
    return myErrorManager;
}
/********************************************************************************************************/
ROP_FBXNodeManager* 
ROP_FBXExporter::getNodeManager(void)
{
    return myNodeManager;
}
/********************************************************************************************************/
ROP_FBXActionManager* 
ROP_FBXExporter::getActionManager(void)
{
    return myActionManager;
}
/********************************************************************************************************/
ROP_FBXExportOptions* 
ROP_FBXExporter::getExportOptions(void)
{
    return &myExportOptions;
}
/********************************************************************************************************/
const char* 
ROP_FBXExporter::getOutputFileName(void)
{
    return myOutputFile.c_str();
}
/********************************************************************************************************/
float 
ROP_FBXExporter::getStartTime(void)
{
    return myStartTime;
}
/********************************************************************************************************/
float 
ROP_FBXExporter::getEndTime(void)
{
    return myEndTime;
}
/********************************************************************************************************/
bool 
ROP_FBXExporter::getExportingAnimation(void)
{
    return !SYSisEqual(myStartTime, myEndTime);
}
/********************************************************************************************************/
void 
ROP_FBXExporter::queueStringToDeallocate(char* string_ptr)
{
    myStringsToDeallocate.push_back(string_ptr);
}
/********************************************************************************************************/
void 
ROP_FBXExporter::deallocateQueuedStrings(void)
{
    int curr_str_idx, num_strings = myStringsToDeallocate.size();
    for(curr_str_idx = 0; curr_str_idx < num_strings; curr_str_idx++)
    {
	delete[] myStringsToDeallocate[curr_str_idx];
    }
    myStringsToDeallocate.clear();
}
/********************************************************************************************************/
KFbxNode* 
ROP_FBXExporter::GetFBXRootNode(OP_Node* asking_node)
{
    // If we are exporting one of the standard subnets, such as "/" or "/obj", etc., return the
    // fbx scene root. Otherwise, create a null node (if it's not created yet), and return that.

    KFbxNode* fbx_scene_root = myScene->GetRootNode();

    UT_String export_path(myExportOptions.getStartNodePath());

    if(export_path == "/")
	return fbx_scene_root;

    // Try to get the parent network
    OP_Node* export_node = OPgetDirector()->findNode(export_path);
    if(!export_node)
	return fbx_scene_root;

    // If our parent network is the same as us, just return the fbx scene root (this happens
    // when exporting a single GEO node, for example, or in general when exporting a network
    // that is not to be dived into).
    if(asking_node == export_node)
	return fbx_scene_root;

    OP_Network* parent_net = export_node->getParentNetwork();
    if(!parent_net)
	return fbx_scene_root;

    parent_net->getFullPath(export_path);
    if(export_path == "/")
	return fbx_scene_root;

    if(!myDummyRootNullNode)
    {
	myDummyRootNullNode = KFbxNode::Create(mySDKManager, (const char*)"world_root");
	KFbxNull *res_attr = KFbxNull::Create(mySDKManager, (const char*)"world_root");
	myDummyRootNullNode->SetNodeAttribute(res_attr);

	// Set world transform
	ROP_FBXUtil::setStandardTransforms(export_node, myDummyRootNullNode, false, 0.0, getStartTime(), NULL, true );
	fbx_scene_root->AddChild(myDummyRootNullNode);

	// Add nodes to the map
	ROP_FBXMainNodeVisitInfo dummy_info(export_node);
	dummy_info.setFbxNode(myDummyRootNullNode);
	ROP_FBXNodeInfo& stored_node_pair = myNodeManager->addNodePair(export_node, myDummyRootNullNode, dummy_info);
    }

    UT_ASSERT(myDummyRootNullNode);
    return myDummyRootNullNode;
}
/********************************************************************************************************/
UT_Interrupt* 
ROP_FBXExporter::GetBoss(void)
{
    return myBoss;
}
/********************************************************************************************************/