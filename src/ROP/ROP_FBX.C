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

#include "ROP_FBX.h"
#include <UT/UT_InfoTree.h>
#include <CH/CH_LocalVariable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include <GEO/GEO_Vertex.h>
#include <SIM/SIM_Object.h>
#include <SIM/SIM_Geometry.h>
#include <DOP/DOP_Parent.h>
#include <DOP/DOP_FullPathData.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_DialogScript.h>
#include <PRM/PRM_SharedFunc.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_NodeInfoParms.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <UT/UT_DSOVersion.h>

using namespace std;

#if !defined(CUSTOM_FBX_TOKEN_PREFIX)
#define CUSTOM_FBX_TOKEN_PREFIX ""
#define CUSTOM_FBX_LABEL_PREFIX ""
#endif

static void
buildBundleMenu(
	void *, PRM_Name *menu, int max,
	const PRM_SpareData *spare, const PRM_Parm *)
{
    OPgetDirector()->getBundles()->buildBundleMenu(menu, max,
	spare ? spare->getValue("opfilter") : 0);
}

SOP_Node *
ROP_FBX::getSopNode() const
{
    SOP_Node *sop = CAST_SOPNODE(getInput(0));
    return sop;
}

PRM_SpareData		ROPoutFbxBundlesList(PRM_SpareArgs()
			    << PRM_SpareToken("opfilter",	"!!OBJ!!")
			    << PRM_SpareToken("oprelative",	"/")
			    << PRM_SpareToken("allownullbundles", "on")
			);

static PRM_ChoiceList	bundleMenu(PRM_CHOICELIST_REPLACE, ::buildBundleMenu);

static PRM_Name	vcType[] =
{
    PRM_Name("mayaformat",	"Maya Compatible (MC)"),
    PRM_Name("maxformat",	"3DS MAX Compatible (PC2)"),
    PRM_Name(0),
};

static PRM_Name	invisObj[] =
{
    PRM_Name("nullnodes",	"As Hidden Null Nodes"),
    PRM_Name("fullnodes",	"As Hidden Full Nodes"),
    PRM_Name("visiblenodes",	"As Visible Full Nodes"),
    PRM_Name("nonodes",	        "Don't Export"),
    PRM_Name(0),
};

CH_LocalVariable	ROP_FBX::myVariableList[] = { {0, 0, 0} };

static PRM_Name		sopOutput("sopoutput",	"Output File");
static PRM_Name		startNode("startnode", "Export");
static PRM_Name		createSubnetRoot("createsubnetroot", "Create Root for Subnet");
static PRM_Name		exportKind("exportkind", "Export in ASCII Format");
static PRM_Name		exportClips("exportclips", "Export Animation Clips (Takes)");
static PRM_Name		numclips("numclips", "Clips");
static PRM_Name		clipname("clipname#", "Clip #");
static PRM_Name		clipframerange("clipframerange#", "Frame Range");
static PRM_Name		detectConstPointObjs("detectconstpointobjs", "Detect Constant Point Count Dynamic Objects");
static PRM_Name		deformsAsVcs("deformsasvcs", "Export Deforms as Vertex Caches");
static PRM_Name		polyLOD("polylod", "Conversion Level of Detail");
static PRM_Name		vcTypeName("vcformat", "Vertex Cache Format");
static PRM_Name		invisObjTypeName("invisobj", "Export Invisible Objects");
static PRM_Name		convertSurfacesName("convertsurfaces", "Convert NURBS and Bezier Surfaces to Polygons");
static PRM_Name		sdkVersionName("sdkversion", "FBX SDK Version");
static PRM_Name		conserveMem("conservemem", "Conserve Memory at the Expense of Export Time");
static PRM_Name		forceBlendShape("forceblendshape", "Force Blend Shape Export");
static PRM_Name		forceSkinDeform("forceskindeform", "Force Skin Deform Export");
static PRM_Name		exportEndEffectors("exportendeffectors", "Export End Effectors");

static PRM_Range	polyLODRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 5);

static PRM_Default	exportKindDefault(1);
static PRM_Default	exportClipsDefault(0);
static PRM_Default	detectConstPointObjsDefault(1);
static PRM_Default	deformsAsVcsDefault(0);
static PRM_Default	convertSurfacesDefault(0);
static PRM_Default	conserveMemDefault(0);
static PRM_Default	forceBlendShapeDefault(0);
static PRM_Default	forceSkinDeformDefault(0);
static PRM_Default	polyLODDefault(1.0);
static PRM_Default	startNodeDefault(0, "/obj");
static PRM_Default	sopOutputDefault(0, "$HIP/out.fbx");
static PRM_Default	exportEndEffectorsDefault(1);
static PRM_ChoiceList	sopOutputMenu(PRM_CHOICELIST_REPLACE,
					 &ROP_FBX::buildGeoSaveMenu);

static PRM_ChoiceList	vcTypeMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE
				| PRM_CHOICELIST_REPLACE), vcType);
static PRM_ChoiceList	skdVersionsMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE
				   | PRM_CHOICELIST_REPLACE), &ROP_FBX::buildVersionsMenu);
static PRM_ChoiceList	invisObjMenu((PRM_ChoiceListType)(PRM_CHOICELIST_EXCLUSIVE
				   | PRM_CHOICELIST_REPLACE), invisObj);

static PRM_Name switcherName("switcher", "");

static PRM_Default switcherDefs[] =
{
    PRM_Default(0, "FBX"),
    PRM_Default(0, "Scripts"),
};

static PRM_Template theMultiClipsTemplate[] =
{

    PRM_Template(PRM_STRING | PRM_TYPE_JOIN_NEXT,  1, &clipname),
    PRM_Template(PRM_INT , 2, &clipframerange, PRMzeroDefaults, 0),

    PRM_Template()
};

static PRM_Template	 geoTemplates[] = {
    PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST, 1, &startNode,
                 &startNodeDefault, &bundleMenu, 0, 0, &ROPoutFbxBundlesList),
    PRM_Template(PRM_FILE, 1, &sopOutput, &sopOutputDefault, nullptr, 0, 0,
                 &PRM_SpareData::fileChooserModeWrite),
    PRM_Template(PRM_SWITCHER, 2, &switcherName, switcherDefs),
    PRM_Template(PRM_TOGGLE, 1, &createSubnetRoot, PRMoneDefaults, nullptr),
    PRM_Template(PRM_TOGGLE, 1, &exportKind, &exportKindDefault, nullptr),
    PRM_Template(PRM_STRING, PRM_Template::PRM_EXPORT_TBX, 1, &sdkVersionName,
                 0, &skdVersionsMenu),
    PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_TBX, 1, &vcTypeName, 0,
                 &vcTypeMenu),
    PRM_Template(PRM_ORD, PRM_Template::PRM_EXPORT_TBX, 1, &invisObjTypeName,
                 0, &invisObjMenu),
    PRM_Template(PRM_FLT, 1, &polyLOD, &polyLODDefault, nullptr, &polyLODRange),
    PRM_Template(PRM_TOGGLE, 1, &detectConstPointObjs,
                 &detectConstPointObjsDefault, nullptr),
    PRM_Template(PRM_TOGGLE, 1, &convertSurfacesName, &convertSurfacesDefault,
                 nullptr),
    PRM_Template(PRM_TOGGLE, 1, &conserveMem, &conserveMemDefault, nullptr),
    PRM_Template(PRM_TOGGLE, 1, &deformsAsVcs, &deformsAsVcsDefault, nullptr),
    PRM_Template(PRM_TOGGLE, 1, &forceBlendShape, &forceBlendShapeDefault,
                 nullptr),
    PRM_Template(PRM_TOGGLE, 1, &forceSkinDeform, &forceSkinDeformDefault,
                 nullptr),
    PRM_Template(PRM_TOGGLE, 1, &exportEndEffectors, &exportEndEffectorsDefault,
                 nullptr),
    PRM_Template(PRM_TOGGLE, 1, &exportClips, &exportClipsDefault, nullptr),
    PRM_Template(PRM_MULTITYPE_LIST, theMultiClipsTemplate, 2, &numclips),
};

static PRM_Template	geoObsolete[] = {
    PRM_Template()
};

static PRM_Name	ropDoRender	("execute",	"Save to Disk");

PRM_Template *
ROP_FBX::getTemplates()
{
    static PRM_Template	*theTemplate = 0;
    if (theTemplate)
	return theTemplate;

    theTemplate = new PRM_Template[ROP_FBX_MAXPARMS+1];
    theTemplate[ROP_FBX_RENDER] = PRM_Template(
		PRM_CALLBACK|PRM_TYPE_NOREFRESH|PRM_TYPE_JOIN_NEXT, 
		PRM_TYPE_NONE, 1, &ropDoRender,
		0, 0, 0, ROP_Node::doRenderCback, &theRopTakeAlways),
    theTemplate[ROP_FBX_RENDER_CTRL] = theRopTemplates[ROP_RENDERDIALOG_TPLATE];
    theTemplate[ROP_FBX_TRANGE] = theRopTemplates[ROP_TRANGE_TPLATE];
    theTemplate[ROP_FBX_FRANGE] = theRopTemplates[ROP_FRAMERANGE_TPLATE];
    theTemplate[ROP_FBX_TAKE] = theRopTemplates[ROP_TAKENAME_TPLATE];

    const PRM_Template *tplates = &geoTemplates[0];
    theTemplate[ROP_FBX_STARTNODE] = *tplates++;
    theTemplate[ROP_FBX_SOPOUTPUT] = *tplates++;
    theTemplate[ROP_FBX_MKPATH] = theRopTemplates[ROP_MKPATH_TPLATE];

    theTemplate[ROP_FBX_SWITCHER] = *tplates++;

    const PRM_Template *page_start = tplates;
    theTemplate[ROP_FBX_CREATESUBNETROOT] = *tplates++;
    theTemplate[ROP_FBX_EXPORTASCII] = *tplates++;
    theTemplate[ROP_FBX_SDKVERSION] = *tplates++;
    theTemplate[ROP_FBX_VCFORMAT] = *tplates++;
    theTemplate[ROP_FBX_INVISOBJ] = *tplates++;
    theTemplate[ROP_FBX_POLYLOD] = *tplates++;
    theTemplate[ROP_FBX_DETECTCONSTPOINTOBJS] = *tplates++;
    theTemplate[ROP_FBX_CONVERTSURFACES] = *tplates++;
    theTemplate[ROP_FBX_CONSERVEMEM] = *tplates++;
    theTemplate[ROP_FBX_DEFORMSASVCS] = *tplates++;
    theTemplate[ROP_FBX_FORCEBLENDSHAPE] = *tplates++;
    theTemplate[ROP_FBX_FORCESKINDEFORM] = *tplates++;
    theTemplate[ROP_FBX_EXPORTENDEFFECTORS] = *tplates++;
    theTemplate[ROP_FBX_EXPORTCLIPS] = *tplates++;
    theTemplate[ROP_FBX_NUMCLIPS] = *tplates++;
    switcherDefs[0].setOrdinal(tplates - page_start);

    theTemplate[ROP_FBX_TPRERENDER] = theRopTemplates[ROP_TPRERENDER_TPLATE];
    theTemplate[ROP_FBX_PRERENDER] = theRopTemplates[ROP_PRERENDER_TPLATE];
    theTemplate[ROP_FBX_LPRERENDER] = theRopTemplates[ROP_LPRERENDER_TPLATE];
    theTemplate[ROP_FBX_TPREFRAME] = theRopTemplates[ROP_TPREFRAME_TPLATE];
    theTemplate[ROP_FBX_PREFRAME] = theRopTemplates[ROP_PREFRAME_TPLATE];
    theTemplate[ROP_FBX_LPREFRAME] = theRopTemplates[ROP_LPREFRAME_TPLATE];
    theTemplate[ROP_FBX_TPOSTFRAME] = theRopTemplates[ROP_TPOSTFRAME_TPLATE];
    theTemplate[ROP_FBX_POSTFRAME] = theRopTemplates[ROP_POSTFRAME_TPLATE];
    theTemplate[ROP_FBX_LPOSTFRAME] = theRopTemplates[ROP_LPOSTFRAME_TPLATE];
    theTemplate[ROP_FBX_TPOSTRENDER] = theRopTemplates[ROP_TPOSTRENDER_TPLATE];
    theTemplate[ROP_FBX_POSTRENDER] = theRopTemplates[ROP_POSTRENDER_TPLATE];
    theTemplate[ROP_FBX_LPOSTRENDER] = theRopTemplates[ROP_LPOSTRENDER_TPLATE];
    switcherDefs[1].setOrdinal(ROP_FBX_LPOSTRENDER - ROP_FBX_TPRERENDER + 1);

    theTemplate[ROP_FBX_MAXPARMS] = PRM_Template();
    UT_ASSERT(PRM_Template::countTemplates(theTemplate) == ROP_FBX_MAXPARMS);

    return theTemplate;
}

PRM_Template *
ROP_FBX::getObsolete()
{
    return geoObsolete;
}

OP_Node *
ROP_FBX::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new ROP_FBX(net, name, op);
}

bool
ROP_FBX::updateParmsFlags()
{
    bool issop = CAST_SOPNODE(getInput(0)) != NULL;

    bool	changed = ROP_Node::updateParmsFlags();

    // Hide start_node if the node is a sop
    changed |= setVisibleState("startnode", !issop);
    changed |= enableParm("deformsasvcs", DORANGE());
    changed |= enableParm("exportclips", DORANGE());
    changed |= enableParm("numclips", EXPORTCLIPS() && DORANGE());
    changed |= setVisibleState("numclips", EXPORTCLIPS());
    

    return changed;
}

ROP_FBX::ROP_FBX(OP_Network *net, const char *name, OP_Operator *entry)
	: ROP_Node(net, name, entry)
{

}

void
ROP_FBX::resolveObsoleteParms(PRM_ParmList *obsolete_parms)
{
    UT_String	net, node, path, blank;
    ROP_Node::resolveObsoleteParms(obsolete_parms);
}

ROP_FBX::~ROP_FBX()
{
}

class ropFBX_AutoCookRender {
public:
    ropFBX_AutoCookRender(OP_Node *sop)
    {
	if ((myObj = sop->getParent()))
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

int
ROP_FBX::startRender(int /*nframes*/, fpreal tstart, fpreal tend)
{
    int			 rcode = 1;

    {
        initSimulationOPs();
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(1);
    }

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePreRenderScript(tstart) )
	    return 0;
    }

    UT_String str_start_node(UT_String::ALWAYS_DEEP);
    UT_String str_sdk_version(UT_String::ALWAYS_DEEP);

    OUTPUT(mySavePath, tstart);

    SOP_Node* sopNode = getSopNode();
    if ( sopNode )
	sopNode->getFullPath( str_start_node );
    else
        STARTNODE(str_start_node);

    bool create_subnet_root = CREATESUBNETROOT(tstart);
    SDKVERSION(str_sdk_version);

    if(str_start_node.length() <= 0)
	str_start_node = "/obj";

    // Set export options
    ROP_FBXExportOptions export_options;

    int vc_format = VCFORMAT();
    if(vc_format == 0)
	export_options.setVertexCacheFormat(ROP_FBXVertexCacheExportFormatMaya);
    else
	export_options.setVertexCacheFormat(ROP_FBXVertexCacheExportFormat3DStudio);

    UT_String str_take(UT_String::ALWAYS_DEEP);
    RENDER_TAKE(str_take);
    if (str_take.length() > 0)
	export_options.setExportTakeName(str_take);

    export_options.setInvisibleNodeExportMethod((ROP_FBXInvisibleNodeExportType)((int)INVISOBJ()));
    export_options.setVersion(str_sdk_version);
    
    export_options.setExportInAscii(EXPORTASCII());
    export_options.setPolyConvertLOD(POLYLOD());
    export_options.setDetectConstantPointCountObjects(DETECTCONSTOBJS());
    export_options.setExportDeformsAsVC(DEFORMSASVCS());
    export_options.setSaveMemory(CONSERVEMEM());    
    export_options.setForceBlendShapeExport(FORCEBLENDSHAPE());
    export_options.setForceSkinDeformExport(FORCESKINDEFORM());
    export_options.setStartNodePath((const char*)str_start_node, true);
    export_options.setCreateSubnetRoot(create_subnet_root);
    export_options.setConvertSurfaces(CONVERTSURFACES());
    export_options.setExportBonesEndEffectors(EXPORTENDEFFECTORS());

    int num_clips = NUM_CLIPS(tstart);
    for (int i = 1; i <= num_clips; ++i)
    {
	ROP_FBXExportClip clip;
	UT_String name;
	CLIP_NAME(name, i, tstart);
	clip.name = name;

	clip.start_frame = CLIP_START(i, tstart);
	clip.end_frame = CLIP_END(i, tstart);

	export_options.appendExportClip(clip);
    }

    if (sopNode)
	export_options.setSopExport(true);

    myFBXExporter.initializeExport((const char*)mySavePath, tstart, tend, &export_options);
    myDidCallExport = false;

    return rcode;
}

ROP_RENDER_CODE
ROP_FBX::renderFrame(fpreal time, UT_Interrupt *)
{
    if( !executePreFrameScript(time) )
	return ROP_ABORT_RENDER;

    // We only need to call this once, even if we're rendering a frame range.
    if(!myDidCallExport)
    {
	myFBXExporter.doExport();
	myDidCallExport = true;

	// Add any messages we might have had
	if(myFBXExporter.getErrorManager())
	{
	    ROP_FBXError* error_ptr;
	    int curr_error, num_errors = myFBXExporter.getErrorManager()->getNumItems();
	    for(curr_error = 0; curr_error < num_errors; curr_error++)
	    {
		error_ptr = myFBXExporter.getErrorManager()->getError(curr_error);
		if(error_ptr->getIsCritical())
		{
		    // Error		
		    addError(ROP_MESSAGE, error_ptr->getMessage());
		}
		else
		{
		    // Warning
		    addWarning(ROP_MESSAGE, error_ptr->getMessage());
		}
	    }
	}
    }

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePostFrameScript(time) )
	    return ROP_ABORT_RENDER;
    }

    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_FBX::endRender()
{
    myFBXExporter.finishExport();

    // Add any messages we might have had
    if (myFBXExporter.getErrorManager())
    {
        ROP_FBXError *error_ptr;
        int curr_error,
            num_errors = myFBXExporter.getErrorManager()->getNumItems();
        for (curr_error = 0; curr_error < num_errors; curr_error++)
        {
            error_ptr = myFBXExporter.getErrorManager()->getError(curr_error);
            if (error_ptr->getIsCritical())
            {
                // Error
                addError(ROP_MESSAGE, error_ptr->getMessage());
            }
            else
            {
                // Warning
                addWarning(ROP_MESSAGE, error_ptr->getMessage());
            }
        }
    }

    OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);

    if (error() < UT_ERROR_ABORT)
    {
	if( !executePostRenderScript(myEndTime) )
	    return ROP_ABORT_RENDER;
    }
    return ROP_CONTINUE_RENDER;
}


//------------------------------------------------------------------------------

static void
setMenu(PRM_Name &menu, const char *token, const char *label)
{
    menu.setToken(token);
    menu.setLabel(label);
}

void
ROP_FBX::buildGeoSaveMenu(
	void *, PRM_Name *menu, int,
	const PRM_SpareData *, const PRM_Parm *)
{
    int i = 0;
    setMenu(menu[i++], "$HIP/out.fbx",	"HIP: binary FBX file");
    setMenu(menu[i++], "$JOB/out.fbx",	"JOB: binary FBX file");

    setMenu(menu[i], 0, 0);
}

void			 
ROP_FBX::buildVersionsMenu(
	void *, PRM_Name *menu, int,
	const PRM_SpareData *, const PRM_Parm *)
{
    // Fill in the SDK versions
    int menu_item = 0;
    TStringVector fbx_formats;
    ROP_FBXExporterWrapper::getVersions(fbx_formats);
    int curr_format, num_formats = fbx_formats.size();
    for(curr_format = 0; curr_format < num_formats; curr_format++)
    {
	setMenu(menu[menu_item], fbx_formats[curr_format].c_str(), fbx_formats[curr_format].c_str());
	menu_item++;
    }

    if(num_formats <= 0)
    {
	// Shouldn't happen, but make sure we play nice anyway.
	setMenu(menu[menu_item], "(Current)", "(Current)");
	menu_item++;
    }

    setMenu(menu[menu_item], 0, 0);
}

void
ROP_FBX::inputConnectChanged(int which)
{
    ROP_Node::inputConnectChanged(which);
}
    
fpreal
ROP_FBX::getW() const
{
    return 4 * getFlagWidth() + getNodeButtonWidth();
}

fpreal
ROP_FBX::getH() const
{
    return getNodeHeight();
}

void
ROP_FBX::getNodeSpecificInfoText(OP_Context &context,
	OP_NodeInfoParms &iparms)
{
    SOP_Node		*sop;
    UT_String		 out;
    UT_String		 soppath;
    
    ROP_Node::getNodeSpecificInfoText(context, iparms);

    sop = CAST_SOPNODE(getInput(0));
    if( sop )
    {
	// If we have an input, get the full path to that SOP.
	sop->getFullPath(soppath);
    }

    if(soppath.isstring())
    {
	iparms.append("Render SOP        ");
	iparms.append(soppath);
	iparms.append("\n");
    }

    evalStringRaw(out, "sopoutput", 0, 0.0f);
    iparms.append("Write to          ");
    iparms.append(out);
}

void
ROP_FBX::fillInfoTreeNodeSpecific(UT_InfoTree &tree, 
	const OP_NodeInfoTreeParms &parms)
{
    ROP_Node::fillInfoTreeNodeSpecific(tree, parms);

    UT_InfoTree		*branch = tree.addChildMap("FBX ROP Info");
    SOP_Node		*sop;
    UT_String		 out;
    UT_String		 soppath;
    
    // If we have an input, get the full path to that SOP.
    sop = CAST_SOPNODE(getInput(0));
    if( sop )
	sop->getFullPath(soppath);

    if(soppath.isstring())
	branch->addProperties("Render SOP", soppath);

    evalStringRaw(out, "sopoutput", 0, 0.0f);
    branch->addProperties("Writes to", out);
}

void
newDriverOperator(OP_OperatorTable *table)
{
    // FBX ROP
    OP_Operator	*fbx_op = new OP_Operator(
	CUSTOM_FBX_TOKEN_PREFIX "filmboxfbx",		// Internal name
	CUSTOM_FBX_LABEL_PREFIX "Filmbox FBX",		// GUI name
	ROP_FBX::myConstructor,
	ROP_FBX::getTemplates(),
	ROP_FBX::theChildTableName,
	0, 9999,
	ROP_Node::myVariableList,
	OP_FLAG_UNORDERED | OP_FLAG_GENERATOR);
    fbx_op->setIconName("ROP_fbx");
    fbx_op->setObsoleteTemplates(ROP_FBX::getObsolete());
    table->addOperator(fbx_op);
}

void
newSopOperator(OP_OperatorTable *table)
{
    // FBX SOP ROP
    OP_Operator	*fbx_sop = new OP_Operator(
	CUSTOM_FBX_TOKEN_PREFIX "rop_fbx",
	CUSTOM_FBX_LABEL_PREFIX "ROP FBX Output",
	ROP_FBX::myConstructor,
	ROP_FBX::getTemplates(),
	ROP_FBX::theChildTableName,
	0, 1,
	ROP_Node::myVariableList,
	OP_FLAG_GENERATOR | OP_FLAG_MANAGER);
    fbx_sop->setIconName("ROP_fbx");
    fbx_sop->setObsoleteTemplates(ROP_FBX::getObsolete());
    table->addOperator( fbx_sop );
}
