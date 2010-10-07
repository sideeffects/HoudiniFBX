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
#include "ROP_FBXActionManager.h"
#include "ROP_FBXExporter.h"
#include <GU/GU_DetailHandle.h>
#include <OP/OP_Network.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Point.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Bundle.h>
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
    mySDKManager = KFbxSdkManager::Create();

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

    // See if we're exporting bundles
    if(myExportOptions.isExportingBundles())
    {
	// Parse bundle names
	//UT_String bundle_names(UT_String::ALWAYS_DEEP, myExportOptions.getBundlesString());

	UT_String bundle_names(UT_String::ALWAYS_DEEP, myExportOptions.getBundlesString());

	// Strip any '@' we might have
	bundle_names.strip("@");

	OP_Node* bundle_node;
	OP_Bundle	    *bundle;
	OP_BundleList   *bundles = OPgetDirector()->getBundles();
	int bundle_idx, node_idx;

	OP_Node* obj_net = OPgetDirector()->findNode("/obj");
	OP_Node* top_network = NULL;

	for (bundle_idx = 0; bundle_idx < bundles->entries(); bundle_idx++)
	{
	    bundle = bundles->getBundle(bundle_idx);
	    UT_ASSERT(bundle);
	    if (!bundle)
		continue;
	    UT_String	     bundle_name(bundle->getName());

	    if (!bundle_name.multiMatch(bundle_names))
		continue;

	    // Process the bundle
	    for (node_idx = 0; node_idx < bundle->entries(); node_idx++)
	    {
		bundle_node = bundle->getNode(node_idx);
		UT_ASSERT(bundle_node);
		if (!bundle_node)
		    continue;
		myNodeManager->addBundledNode(bundle_node);

		if(!top_network)
		{
		    top_network = bundle_node->getParent();
		    if(top_network->getIsContainedBy(obj_net) == false)
			top_network = NULL;
		}
		else
		{
		    // Find the parent network which contains all nodes so far.
		    while(top_network != obj_net && bundle_node->getIsContainedBy(top_network) == false)
			top_network = top_network->getParent();
		}
	    } // end over all nodes in a bundle
	} // end over all bundles

	if(!top_network)
	    top_network = obj_net;

	// Now set the top exported network
	UT_String start_path;
	top_network->getFullPath(start_path);
	myExportOptions.setStartNodePath(start_path, false);
    }

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
	KTime::SetGlobalTimeMode(time_mode, curr_fps);
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

	    TAKE_Take *curr_hd_take = OPgetDirector()->getTakeManager()->getCurrentTake();
	    myScene->SetCurrentTake(const_cast<char*>(curr_hd_take->getName()));

	    KFbxAnimStack* anim_stack = KFbxAnimStack::Create(myScene, curr_hd_take->getName());
	    KFbxAnimLayer* anim_layer = KFbxAnimLayer::Create(myScene, "Base Layer");
	    anim_stack->AddMember(anim_layer);
	    anim_visitor.reset(anim_layer);

	    // Create a single default animation stack.

	    // Export the main world_root animation if applicable
	    if(myDummyRootNullNode)
	    {			
		//KFbxTakeNode* curr_world_take_node = ROP_FBXAnimVisitor::addFBXTakeNode(myDummyRootNullNode);
		anim_visitor.exportTRSAnimation(geom_node, anim_layer, myDummyRootNullNode);
	    }	    

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

	//Try to export in ASCII if possible
	int format_index, format_count = mySDKManager->GetIOPluginRegistry()->GetWriterFormatCount();
	int out_file_format = -1;

	for (format_index = 0; format_index < format_count; format_index++)
	{
	    if (mySDKManager->GetIOPluginRegistry()->WriterIsFBX(format_index))
	    {
		KString format_desc = mySDKManager->GetIOPluginRegistry()->GetWriterFormatDescription(format_index);
		if (format_desc.Find("ascii")>=0 && myExportOptions.getExportInAscii())
		{
		    out_file_format = format_index;
		    break;
		}
		else if (format_desc.Find("ascii")<0 && !myExportOptions.getExportInAscii())
		{
		    out_file_format = format_index;
		    break;
		}
	    }
	}

	// Deprecated.
	///fbx_exporter->SetFileFormat(out_file_format);

	string sdk_version = myExportOptions.getVersion();
	if(sdk_version.length() > 0)
	    fbx_exporter->SetFileExportVersion(sdk_version.c_str(), KFbxSceneRenamer::eFBX_TO_FBX);
#if 0	
	// Options are now done differentyl. Luckily, we don't use them.
	KFbxStreamOptionsFbxWriter* export_options = KFbxStreamOptionsFbxWriter::Create(mySDKManager, "");
	if (mySDKManager->GetIOPluginRegistry()->WriterIsFBX(out_file_format))
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
#endif
	// Initialize the exporter by providing a filename.
	if(fbx_exporter->Initialize(myOutputFile.c_str(), out_file_format, mySDKManager->GetIOSettings()) == false)
	{
	    //	addError(ROP_COOK_ERROR, (const char *)"Invalid output path");
	    //	return ROP_ABORT_RENDER;
	    return false;
	}

	// Export the scene.
	bool exp_status = fbx_exporter->Export(myScene); 
/*
	if(export_options)
	    export_options->Destroy();
	export_options=NULL;
*/
	// Destroy the exporter.
	fbx_exporter->Destroy();
    }


    if(myScene)
	myScene->Destroy();
    myScene = NULL;

    if(mySDKManager)
	mySDKManager->Destroy();
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
	UT_String node_name(UT_String::ALWAYS_DEEP, "world_root");
	myNodeManager->makeNameUnique(node_name);

	myDummyRootNullNode = KFbxNode::Create(mySDKManager, (const char*)node_name);
	KFbxNull *res_attr = KFbxNull::Create(mySDKManager, (const char*)node_name);
	myDummyRootNullNode->SetNodeAttribute(res_attr);

	// Set world transform
	ROP_FBXUtil::setStandardTransforms(export_node, myDummyRootNullNode, NULL, false, 0.0, getStartTime(), NULL, true );
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
UT_String* 
ROP_FBXExporter::getVersions(void)
{
    KFbxSdkManager* tempSDKManager = KFbxSdkManager::Create();
    if(!tempSDKManager)
	return NULL;

    // Get versions for the first available format. This assumes that versions for the
    // ascii and binary exporters are the same. Which they are.
    int format_index, format_count = tempSDKManager->GetIOPluginRegistry()->GetWriterFormatCount();
    int out_file_format = -1;

    for (format_index = 0; format_index < format_count; format_index++)
    {
	if (tempSDKManager->GetIOPluginRegistry()->WriterIsFBX(format_index))
	{
	    KString format_desc = tempSDKManager->GetIOPluginRegistry()->GetWriterFormatDescription(format_index);
	    if (format_desc.Find("ascii")>=0)
	    {
		out_file_format = format_index;
		break;
	    }
	    else if (format_desc.Find("ascii")<0)
	    {
		out_file_format = format_index;
		break;
	    }
	}
    }

    if(out_file_format < 0)
	return NULL;

    char const* const* format_versions = tempSDKManager->GetIOPluginRegistry()->GetWritableVersions(out_file_format);
    if(!format_versions)
	return NULL;

    // Count the number
    int curr_ver, num_versions = 0;
    while(format_versions[num_versions])
	num_versions++;

    if(num_versions == 0)
	return NULL;

    UT_String* res_array = new UT_String[num_versions + 1];
    for(curr_ver = 0; curr_ver < num_versions; curr_ver++)
    {
	res_array[curr_ver].setAlwaysDeep(true);
	res_array[curr_ver] = format_versions[curr_ver];
    }
    res_array[curr_ver] = "";
    
    tempSDKManager->Destroy();

    return res_array;
}
/********************************************************************************************************/